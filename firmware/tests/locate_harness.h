#include "crypto.h"
#include "frames.h"
#include "gps.h"
#include "emu_hal.h"
#include "identity.h"
#include "locate.h"
#include "beeper.h"
#include "message.h"
#include "relay.h"
#include "screen.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#define TX_MAX 32

static __attribute__((unused)) int g_pass, g_fail;

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

static __attribute__((unused)) void pass(const char *name) {
  g_pass++;
  printf("ok  %s\n", name);
}

typedef struct {
  uint8_t frames[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  int n;
} tx_log_t;

typedef struct {
  emu_hal_t ram;
  hal_t hal;
  message_session_t radio;
  locate_radio_t locate;
  identity_store_t identity;
  contact_book_t contacts;
  tx_log_t tx;
} unit_t;

static __attribute__((unused)) void on_tx(void *user, const uint8_t *bytes, size_t len) {
  tx_log_t *l = user;
  if (l->n >= TX_MAX || !bytes || len == 0) return;
  if (len > RAW_FRAME_MAX) len = RAW_FRAME_MAX;
  memcpy(l->frames[l->n], bytes, len);
  l->lens[l->n] = (uint8_t)len;
  l->n++;
}

static __attribute__((unused)) void id_byte(uint8_t *out, uint8_t n) {
  memset(out, n, NODE_ID_LEN);
}

static __attribute__((unused)) void make_unit(unit_t *u, uint8_t id) {
  memset(u, 0, sizeof *u);
  emu_hal_init(&u->ram, &u->hal);
  u->ram.on_tx_raw = on_tx;
  u->ram.on_tx_user = &u->tx;
  identity_init(&u->identity);
  id_byte(u->identity.node_id, id);
  u->identity.has_id = true;
  contacts_init(&u->contacts);
  message_init(&u->radio);
  locate_init(&u->locate);
}

static __attribute__((unused)) void tick(unit_t *u) {
  beeper_sessions_tick(&u->radio, &u->identity, &u->contacts, &u->hal, NULL, NULL, &u->locate);
}

static __attribute__((unused)) void pair_books(unit_t *a, unit_t *b, const char *name_a, const char *name_b) {
  uint8_t key[SESSION_KEY_LEN];
  memset(key, 9, sizeof key);
  contact_t ca, cb;
  memset(&ca, 0, sizeof ca);
  memset(&cb, 0, sizeof cb);
  memcpy(ca.node_id, b->identity.node_id, NODE_ID_LEN);
  str_copy_max(ca.name, sizeof ca.name, name_b, PAGE_FROM_MAX);
  memcpy(ca.session_key, key, SESSION_KEY_LEN);
  memcpy(cb.node_id, a->identity.node_id, NODE_ID_LEN);
  str_copy_max(cb.name, sizeof cb.name, name_a, PAGE_FROM_MAX);
  memcpy(cb.session_key, key, SESSION_KEY_LEN);
  contacts_upsert(&a->contacts, &ca);
  contacts_upsert(&b->contacts, &cb);
}

static __attribute__((unused)) void set_grid(unit_t *u, int east, int north) {
  gps_fix_t fix;
  gps_from_feet(east, north, &fix);
  emu_hal_set_gps(&u->ram, &fix);
}

static __attribute__((unused)) int take_sent(unit_t *u, uint8_t frames[][RAW_FRAME_MAX], uint8_t *lens, int max) {
  int n = u->tx.n;
  if (n > max) n = max;
  for (int i = 0; i < n; i++) {
    memcpy(frames[i], u->tx.frames[i], u->tx.lens[i]);
    lens[i] = u->tx.lens[i];
  }
  u->tx.n = 0;
  return n;
}

static __attribute__((unused)) int32_t get_i32le(const uint8_t *p) {
  uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  return (int32_t)u;
}

static __attribute__((unused)) bool loc_flags(const uint8_t *frame, size_t len, uint8_t *flags) {
  uint8_t key[SESSION_KEY_LEN];
  memset(key, 9, sizeof key);
  loc_payload_t p;
  if (!frame_decode_loc(frame, len, &p)) return false;
  uint8_t plain[LOC_PLAIN_LEN];
  if (!page_open(key, p.nonce, p.body, p.body_len, p.mac, plain)) return false;
  if (flags) *flags = plain[0];
  return true;
}

static __attribute__((unused)) bool open_loc(const uint8_t *frame, size_t len, gps_fix_t *out) {
  uint8_t key[SESSION_KEY_LEN];
  memset(key, 9, sizeof key);
  loc_payload_t p;
  if (!frame_decode_loc(frame, len, &p)) return false;
  uint8_t plain[LOC_PLAIN_LEN];
  if (!page_open(key, p.nonce, p.body, p.body_len, p.mac, plain)) return false;
  memset(out, 0, sizeof *out);
  out->valid = (plain[0] & LOC_FLAG_FIX) != 0;
  if (out->valid) {
    out->lat_min_e4 = get_i32le(plain + 1);
    out->lon_min_e4 = get_i32le(plain + 5);
  }
  return true;
}

static __attribute__((unused)) void elapse(unit_t *u, uint32_t ms) {
  u->ram.now_ms += ms;
  tick(u);
}

static __attribute__((unused)) void exchange(unit_t *from, unit_t *to) {
  tick(from);
  uint8_t frames[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  int n = take_sent(from, frames, lens, TX_MAX);
  for (int i = 0; i < n; i++) emu_hal_push_raw(&to->ram, frames[i], lens[i], -40);
  tick(to);
}
