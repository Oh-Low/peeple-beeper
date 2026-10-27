#include "contacts.h"
#include "frames.h"
#include "emu_hal.h"
#include "identity.h"
#include "beeper.h"
#include "message.h"
#include "relay.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
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
  identity_store_t identity;
  contact_book_t contacts;
  tx_log_t tx;
  page_t pages[16];
  int page_n;
} unit_t;

static __attribute__((unused)) void on_tx(void *user, const uint8_t *bytes, size_t len) {
  tx_log_t *l = user;
  if (l->n >= TX_MAX || !bytes || len == 0) return;
  if (len > RAW_FRAME_MAX) len = RAW_FRAME_MAX;
  memcpy(l->frames[l->n], bytes, len);
  l->lens[l->n] = (uint8_t)len;
  l->n++;
}

static __attribute__((unused)) void on_page(void *user, const page_t *page) {
  unit_t *u = user;
  if (u->page_n < 16) u->pages[u->page_n++] = *page;
}

static __attribute__((unused)) void id_byte(uint8_t *out, uint8_t n) {
  memset(out, n, NODE_ID_LEN);
}

static __attribute__((unused)) void hex_of(const uint8_t *b, size_t n, char *out, size_t cap) {
  static const char H[] = "0123456789abcdef";
  size_t o = 0;
  for (size_t i = 0; i < n && o + 2 < cap; i++) {
    out[o++] = H[b[i] >> 4];
    out[o++] = H[b[i] & 15];
  }
  out[o] = 0;
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
}

static __attribute__((unused)) void tick(unit_t *u) {
  beeper_sessions_tick(&u->radio, &u->identity, &u->contacts, &u->hal, on_page, u, NULL);
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

static __attribute__((unused)) void dummy_page(uint8_t *out, size_t *len, const uint8_t *dest, const uint8_t *src,
                       uint8_t hop, const uint8_t *nonce) {
  page_payload_t p;
  memset(&p, 0, sizeof p);
  memcpy(p.dest_id, dest, NODE_ID_LEN);
  memcpy(p.src_id, src, NODE_ID_LEN);
  if (nonce) memcpy(p.nonce, nonce, PAGE_NONCE_LEN);
  p.hop_ttl = hop;
  p.body_len = 0;
  *len = frame_encode_page(&p, out, RAW_FRAME_MAX);
}

static __attribute__((unused)) void dummy_ack(uint8_t *out, size_t *len, const uint8_t *dest, const uint8_t *src,
                      uint8_t hop, const uint8_t *nonce) {
  page_ack_payload_t p;
  memset(&p, 0, sizeof p);
  memcpy(p.dest_id, dest, NODE_ID_LEN);
  memcpy(p.src_id, src, NODE_ID_LEN);
  if (nonce) memcpy(p.nonce, nonce, PAGE_NONCE_LEN);
  p.hop_ttl = hop;
  *len = frame_encode_page_ack(&p, out, RAW_FRAME_MAX);
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

static __attribute__((unused)) uint8_t g_saved_page[RAW_FRAME_MAX];
static __attribute__((unused)) uint8_t g_saved_page_len;
