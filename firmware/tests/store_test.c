#include "beeper.h"
#include "canned.h"
#include "contacts.h"
#include "emu_hal.h"
#include "identity.h"
#include "store.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;

#define CHECK(cond, msg)           \
  do {                             \
    if (!(cond)) {                 \
      printf("not ok  %s\n", msg); \
      g_fail++;                    \
      return;                      \
    }                              \
  } while (0)

#define CHECK_EQ(a, b, msg)                                                    \
  do {                                                                         \
    int _a = (int)(a), _b = (int)(b);                                          \
    if (_a != _b) {                                                            \
      printf("not ok  %s: %d !== %d\n", msg, _a, _b);                          \
      g_fail++;                                                                \
      return;                                                                  \
    }                                                                          \
  } while (0)

static void pass(const char *name) {
  g_pass++;
  printf("ok  %s\n", name);
}

static void hex_of(const uint8_t *b, size_t n, char *out, size_t cap) {
  static const char H[] = "0123456789abcdef";
  size_t o = 0;
  for (size_t i = 0; i < n && o + 2 < cap; i++) {
    out[o++] = H[b[i] >> 4];
    out[o++] = H[b[i] & 15];
  }
  out[o] = 0;
}

static void test_nvm_cap(void) {
  emu_hal_t ram;
  hal_t hal;
  emu_hal_init(&ram, &hal);
  uint8_t too_big[NVM_BYTES + 1];
  memset(too_big, 1, sizeof too_big);
  CHECK(!hal.nvm_save(hal.ctx, too_big, NVM_BYTES + 1), "too big");
  CHECK(hal.nvm_save(hal.ctx, too_big, 0), "empty ok");
  CHECK(hal.nvm_save(hal.ctx, too_big, NVM_BYTES), "exact cap ok");
  pass("HAL nvm_save rejects over NVM_BYTES");
}

static void test_roundtrip(void) {
  emu_hal_t ram;
  hal_t hal;
  emu_hal_init(&ram, &hal);

  beeper_t a;
  beeper_init(&a);
  memset(a.identity.node_id, 0x11, NODE_ID_LEN);
  a.identity.has_id = true;
  str_copy_max(a.identity.display_name, sizeof a.identity.display_name, "Ada", PAGE_FROM_MAX);

  uint8_t peer[NODE_ID_LEN];
  memset(peer, 0x22, NODE_ID_LEN);
  uint8_t key[SESSION_KEY_LEN];
  memset(key, 0x33, SESSION_KEY_LEN);
  contact_t c;
  memset(&c, 0, sizeof c);
  memcpy(c.node_id, peer, NODE_ID_LEN);
  str_copy_max(c.name, sizeof c.name, "Bob", PAGE_FROM_MAX);
  memcpy(c.session_key, key, SESSION_KEY_LEN);
  c.paired_ms = 9;
  c.beeple = 2;
  CHECK(contacts_upsert(&a.contacts, &c), "upsert");
  CHECK(canned_add(&a.screen.canned, "See you"), "canned");

  inbox_row_t *in = &a.screen.inbox[a.screen.inbox_n++];
  memset(in, 0, sizeof *in);
  str_copy_max(in->from, sizeof in->from, "Bob", PAGE_FROM_MAX);
  str_copy_max(in->body, sizeof in->body, "hi", PAGE_BODY_MAX);
  in->rssi = -40;
  in->received_ms = 1000;
  in->received_hh = 14;
  in->received_mm = 32;

  outbox_item_t *o0 = &a.screen.outbox[a.screen.outbox_n++];
  memset(o0, 0, sizeof *o0);
  memcpy(o0->dest_id, peer, NODE_ID_LEN);
  str_copy_max(o0->to, sizeof o0->to, "Bob", PAGE_FROM_MAX);
  str_copy_max(o0->body, sizeof o0->body, "yo", PAGE_BODY_MAX);
  o0->status = OUTBOX_SENT;
  o0->sent_hh = 9;
  o0->sent_mm = 1;
  o0->acked_hh = 9;
  o0->acked_mm = 2;

  outbox_item_t *o1 = &a.screen.outbox[a.screen.outbox_n++];
  memset(o1, 0, sizeof *o1);
  memcpy(o1->dest_id, peer, NODE_ID_LEN);
  str_copy_max(o1->to, sizeof o1->to, "Bob", PAGE_FROM_MAX);
  str_copy_max(o1->body, sizeof o1->body, "later", PAGE_BODY_MAX);
  o1->status = OUTBOX_SENDING;
  o1->sent_hh = 10;
  o1->sent_mm = 5;
  o1->acked_hh = 255;
  o1->acked_mm = 255;

  a.locate.saved_fix = true;
  a.locate.saved_lat = 248;
  a.locate.saved_lon = -246;
  a.locate.saved_hh = 8;
  a.locate.saved_mm = 30;
  a.locate.track_open = false;
  a.screen.show_clock_bat = false;
  a.screen.dark_mode = true;
  a.screen.light_mode = LIGHT_5M;
  a.screen.tz_hours = -4;
  a.screen.beeple = 3;
  CHECK(store_save(&a, &hal), "save");

  beeper_t b;
  beeper_init(&b);
  CHECK(store_load(&b, &hal), "load");
  CHECK(b.identity.has_id, "restored id");
  char ha[32], hb[32];
  hex_of(b.identity.node_id, NODE_ID_LEN, hb, sizeof hb);
  hex_of(a.identity.node_id, NODE_ID_LEN, ha, sizeof ha);
  CHECK(strcmp(ha, hb) == 0, "node_id");
  CHECK(strcmp(b.identity.display_name, "Ada") == 0, "display name");
  CHECK_EQ(b.contacts.n, 1, "one contact");
  CHECK(strcmp(b.contacts.rows[0].name, "Bob") == 0, "contact name");
  CHECK_EQ(b.contacts.rows[0].beeple, 2, "contact beeple");
  char k0[40], k1[40];
  hex_of(b.contacts.rows[0].session_key, SESSION_KEY_LEN, k0, sizeof k0);
  hex_of(key, SESSION_KEY_LEN, k1, sizeof k1);
  CHECK(strcmp(k0, k1) == 0, "session key");
  CHECK(strcmp(b.screen.canned.saved[0], "See you") == 0, "canned");
  CHECK_EQ(b.screen.inbox_n, 1, "inbox");
  CHECK(strcmp(b.screen.inbox[0].body, "hi") == 0, "inbox body");
  CHECK_EQ(b.screen.inbox[0].received_hh, 14, "inbox hour");
  CHECK_EQ(b.screen.inbox[0].received_mm, 32, "inbox minute");
  CHECK_EQ(b.screen.outbox_n, 2, "outbox");
  CHECK_EQ(b.screen.outbox[0].status, OUTBOX_SENT, "sent kept");
  CHECK_EQ(b.screen.outbox[0].sent_hh, 9, "sent hour");
  CHECK_EQ(b.screen.outbox[0].acked_mm, 2, "ack minute");
  CHECK_EQ(b.screen.outbox[1].status, OUTBOX_FAILED, "sending becomes failed");
  CHECK_EQ(b.screen.outbox[1].acked_hh, 255, "no ack clock");
  CHECK(b.locate.saved_fix, "saved fix");
  CHECK_EQ(b.locate.saved_lat, 248, "saved lat");
  CHECK_EQ(b.locate.saved_lon, -246, "saved lon");
  CHECK_EQ(b.locate.saved_hh, 8, "saved hour");
  CHECK_EQ(b.locate.saved_mm, 30, "saved minute");
  CHECK(!b.locate.track_open, "tracking ask is saved");
  CHECK(!b.screen.show_clock_bat, "clock hidden is saved");
  CHECK(b.screen.dark_mode, "dark mode is saved");
  CHECK_EQ(b.screen.light_mode, LIGHT_5M, "backlight 5m is saved");
  CHECK_EQ(b.screen.tz_hours, -4, "time zone is saved");
  CHECK_EQ(b.screen.beeple, 3, "beeple is saved");
  CHECK(b.screen.menu_art_w == 64, "beeple art applied");
  pass("store roundtrip: node_id, name, contacts, canned, inbox, outbox");
}

int main(void) {
  test_nvm_cap();
  test_roundtrip();
  printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
