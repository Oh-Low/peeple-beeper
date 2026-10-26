#include "contacts.h"
#include "frames.h"
#include "hal_ram.h"
#include "identity.h"
#include "page.h"
#include "relay.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RELAY_JITTER_MAX_MS 400
#define TX_MAX 32

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

typedef struct {
  uint8_t frames[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  int n;
} tx_log_t;

typedef struct {
  ram_hal_t ram;
  hal_t hal;
  page_radio_t radio;
  identity_store_t identity;
  contact_book_t contacts;
  tx_log_t tx;
  page_t pages[16];
  int page_n;
} unit_t;

static void on_tx(void *user, const uint8_t *bytes, size_t len) {
  tx_log_t *l = user;
  if (l->n >= TX_MAX || !bytes || len == 0) return;
  if (len > RAW_FRAME_MAX) len = RAW_FRAME_MAX;
  memcpy(l->frames[l->n], bytes, len);
  l->lens[l->n] = (uint8_t)len;
  l->n++;
}

static void on_page(void *user, const page_t *page) {
  unit_t *u = user;
  if (u->page_n < 16) u->pages[u->page_n++] = *page;
}

static void id_byte(uint8_t *out, uint8_t n) {
  memset(out, n, NODE_ID_LEN);
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

static void make_unit(unit_t *u, uint8_t id) {
  memset(u, 0, sizeof *u);
  ram_hal_init(&u->ram, &u->hal);
  u->ram.on_tx_raw = on_tx;
  u->ram.on_tx_user = &u->tx;
  identity_init(&u->identity);
  id_byte(u->identity.node_id, id);
  u->identity.has_id = true;
  contacts_init(&u->contacts);
  page_radio_init(&u->radio);
}

static void tick(unit_t *u) {
  page_tick(&u->radio, &u->identity, &u->contacts, &u->hal, on_page, u, NULL);
}

static void pair_books(unit_t *a, unit_t *b, const char *name_a, const char *name_b) {
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

static void dummy_page(uint8_t *out, size_t *len, const uint8_t *dest, const uint8_t *src,
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

static void dummy_ack(uint8_t *out, size_t *len, const uint8_t *dest, const uint8_t *src,
                      uint8_t hop, const uint8_t *nonce) {
  page_ack_payload_t p;
  memset(&p, 0, sizeof p);
  memcpy(p.dest_id, dest, NODE_ID_LEN);
  memcpy(p.src_id, src, NODE_ID_LEN);
  if (nonce) memcpy(p.nonce, nonce, PAGE_NONCE_LEN);
  p.hop_ttl = hop;
  *len = frame_encode_page_ack(&p, out, RAW_FRAME_MAX);
}

static int take_sent(unit_t *u, uint8_t frames[][RAW_FRAME_MAX], uint8_t *lens, int max) {
  int n = u->tx.n;
  if (n > max) n = max;
  for (int i = 0; i < n; i++) {
    memcpy(frames[i], u->tx.frames[i], u->tx.lens[i]);
    lens[i] = u->tx.lens[i];
  }
  u->tx.n = 0;
  return n;
}

static uint8_t g_saved_page[RAW_FRAME_MAX];
static uint8_t g_saved_page_len;

static void test_dest_decrypts_full(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "Alice", "Bob");
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(page_tx(&a.radio, &a.hal, dest, "hi", &a.identity, &a.contacts, NULL, 3), "pageTx");
  CHECK_EQ(page_send_status(&a.radio), PAGE_SENDING, "status after tx");
  uint8_t frames[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  int n = take_sent(&a, frames, lens, TX_MAX);
  CHECK_EQ(n, 1, "one PAGE on first tx");
  CHECK_EQ(frame_type(frames[0], lens[0]), FRAME_PAGE, "first frame is PAGE");
  memcpy(g_saved_page, frames[0], lens[0]);
  g_saved_page_len = lens[0];

  ram_hal_push_raw(&b.ram, frames[0], lens[0], -30);
  tick(&b);
  CHECK_EQ(b.page_n, 1, "deliver once");
  CHECK(strcmp(b.pages[0].from, "Alice") == 0, "from contact name");
  CHECK(strcmp(b.pages[0].body, "hi") == 0, "body");
  CHECK_EQ(b.pages[0].beeple, 3, "sender beeple");
  const contact_t *alice = contacts_find_by_id(&b.contacts, a.identity.node_id);
  CHECK(alice && alice->beeple == 3, "contact beeple updated");
  n = take_sent(&b, frames, lens, TX_MAX);
  CHECK_EQ(n, 1, "one PAGE_ACK");
  CHECK_EQ(frame_type(frames[0], lens[0]), FRAME_PAGE_ACK, "ACK type");

  ram_hal_push_raw(&a.ram, frames[0], lens[0], -30);
  tick(&a);
  CHECK_EQ(page_send_status(&a.radio), PAGE_DELIVERED, "sender delivered");

  ram_hal_push_raw(&b.ram, g_saved_page, g_saved_page_len, -30);
  tick(&b);
  CHECK_EQ(b.page_n, 1, "repeat PAGE does not deliver again");
  n = take_sent(&b, frames, lens, TX_MAX);
  CHECK_EQ(n, 1, "still ACK on repeat");
  pass("dest decrypts PAGE, ACKs, delivered; deliver once if repeated");
}

static void test_retries(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "Alice", "Bob");
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(page_tx(&a.radio, &a.hal, dest, "ping", &a.identity, &a.contacts, NULL, 0), "pageTx");
  tick(&a);

  int tx_at[PAGE_ATTEMPTS];
  tx_at[0] = 0;
  int t = 0;
  for (int i = 1; i < PAGE_ATTEMPTS; i++) {
    t += i < 4 ? PAGE_RETRY_FAST_MS : PAGE_RETRY_SLOW_MS;
    tx_at[i] = t;
    a.ram.now_ms = (uint32_t)t;
    tick(&a);
  }

  uint8_t frames[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  int n = take_sent(&a, frames, lens, TX_MAX);
  CHECK_EQ(n, PAGE_ATTEMPTS, "9 TX");
  char hexes[PAGE_ATTEMPTS][17];
  for (int i = 0; i < n; i++) {
    page_payload_t p;
    CHECK(frame_decode_page(frames[i], lens[i], &p), "decode PAGE");
    hex_of(p.nonce, PAGE_NONCE_LEN, hexes[i], sizeof hexes[i]);
  }
  for (int i = 1; i < n; i++) {
    CHECK(strcmp(hexes[i], hexes[0]) != 0, "nonce differs from tx1");
    for (int j = 0; j < i; j++) CHECK(strcmp(hexes[i], hexes[j]) != 0, "nonce unique");
  }
  CHECK_EQ(page_send_status(&a.radio), PAGE_SENDING, "still sending after last TX");
  CHECK_EQ(tx_at[3], 30000, "4th TX at 30s");
  CHECK_EQ(tx_at[4], 90000, "5th TX at 90s");
  CHECK_EQ(tx_at[8], 330000, "9th TX at 330s");

  a.ram.now_ms = (uint32_t)(t + PAGE_RETRY_SLOW_MS);
  tick(&a);
  CHECK_EQ(page_send_status(&a.radio), PAGE_FAILED, "failed after last ACK wait");
  n = take_sent(&a, frames, lens, TX_MAX);
  CHECK_EQ(n, 0, "no 10th TX");
  pass("no ACK: 10s then 60s retries, unique nonces, fail after last wait");
}

static void test_retry_nonce_ack(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "Alice", "Bob");
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(page_tx(&a.radio, &a.hal, dest, "hi", &a.identity, &a.contacts, NULL, 0), "pageTx");
  uint8_t first[TX_MAX][RAW_FRAME_MAX];
  uint8_t flen[TX_MAX];
  int n = take_sent(&a, first, flen, TX_MAX);
  CHECK_EQ(n, 1, "first TX");

  ram_hal_push_raw(&b.ram, first[0], flen[0], -30);
  tick(&b);
  CHECK_EQ(b.page_n, 1, "deliver first copy");
  CHECK(strcmp(b.pages[0].body, "hi") == 0, "body without msg_id");
  uint8_t ack1[TX_MAX][RAW_FRAME_MAX];
  uint8_t alen[TX_MAX];
  n = take_sent(&b, ack1, alen, TX_MAX);
  CHECK_EQ(n, 1, "ACK nonce 1");

  a.ram.now_ms = PAGE_RETRY_FAST_MS;
  tick(&a);
  uint8_t second[TX_MAX][RAW_FRAME_MAX];
  uint8_t slen[TX_MAX];
  n = take_sent(&a, second, slen, TX_MAX);
  CHECK_EQ(n, 1, "retry TX");
  page_payload_t p0, p1;
  CHECK(frame_decode_page(first[0], flen[0], &p0) && frame_decode_page(second[0], slen[0], &p1),
        "decode");
  char h0[17], h1[17];
  hex_of(p0.nonce, PAGE_NONCE_LEN, h0, sizeof h0);
  hex_of(p1.nonce, PAGE_NONCE_LEN, h1, sizeof h1);
  CHECK(strcmp(h0, h1) != 0, "retry nonce differs");

  ram_hal_push_raw(&b.ram, second[0], slen[0], -30);
  tick(&b);
  CHECK_EQ(b.page_n, 1, "retry does not deliver again");
  n = take_sent(&b, second, slen, TX_MAX);
  CHECK_EQ(n, 1, "ACK nonce 2");

  ram_hal_push_raw(&a.ram, ack1[0], alen[0], -30);
  tick(&a);
  CHECK_EQ(page_send_status(&a.radio), PAGE_DELIVERED, "ACK of first nonce counts");
  pass("retry new nonce: dest delivers once; ACK of first nonce still delivers");
}

static void test_relay_ttl(void) {
  unit_t c;
  make_unit(&c, 3);
  uint8_t dest[NODE_ID_LEN], src[NODE_ID_LEN], nonce[PAGE_NONCE_LEN];
  id_byte(dest, 2);
  id_byte(src, 1);
  memset(nonce, 0xab, PAGE_NONCE_LEN);
  uint8_t frame[RAW_FRAME_MAX];
  size_t flen = 0;
  dummy_page(frame, &flen, dest, src, 3, nonce);
  CHECK(flen > 0, "encode PAGE");
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no TX before jitter");
  c.ram.now_ms = RELAY_JITTER_MAX_MS;
  tick(&c);
  int n = take_sent(&c, sent, lens, TX_MAX);
  CHECK_EQ(n, 1, "one relay TX");
  page_payload_t p;
  CHECK(frame_decode_page(sent[0], lens[0], &p), "relayed PAGE");
  CHECK_EQ(p.hop_ttl, 2, "ttl decremented");
  char hx[17], hn[17];
  hex_of(p.nonce, PAGE_NONCE_LEN, hx, sizeof hx);
  hex_of(nonce, PAGE_NONCE_LEN, hn, sizeof hn);
  CHECK(strcmp(hx, hn) == 0, "nonce unchanged");

  uint32_t second_hear = c.ram.now_ms;
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  c.ram.now_ms = second_hear + RELAY_JITTER_MAX_MS;
  tick(&c);
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "already-sent hear does not TX again");

  unit_t c3;
  make_unit(&c3, 3);
  c3.ram.now_ms = 0;
  dummy_page(frame, &flen, dest, src, 1, nonce);
  ram_hal_push_raw(&c3.ram, frame, flen, -30);
  tick(&c3);
  c3.ram.now_ms = RELAY_JITTER_MAX_MS;
  tick(&c3);
  CHECK_EQ(take_sent(&c3, sent, lens, TX_MAX), 0, "ttl=1 dropped");
  pass("relay PAGE: ttl--, one TX; already-sent hear does not TX again; ttl=1 dropped");
}

static void test_relay_reroll(void) {
  unit_t c;
  make_unit(&c, 3);
  uint8_t dest[NODE_ID_LEN], src[NODE_ID_LEN], nonce[PAGE_NONCE_LEN];
  id_byte(dest, 2);
  id_byte(src, 1);
  memset(nonce, 0xab, PAGE_NONCE_LEN);
  uint8_t frame[RAW_FRAME_MAX];
  size_t flen = 0;
  dummy_page(frame, &flen, dest, src, 3, nonce);
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no TX after first hear");

  c.ram.now_ms = 50;
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no TX after second hear");

  c.ram.now_ms = 50 + RELAY_JITTER_MAX_MS;
  tick(&c);
  int n = take_sent(&c, sent, lens, TX_MAX);
  CHECK_EQ(n, 1, "one PAGE relay after re-roll");
  CHECK_EQ(frame_type(sent[0], lens[0]), FRAME_PAGE, "relayed PAGE");
  page_payload_t p;
  CHECK(frame_decode_page(sent[0], lens[0], &p), "decode relayed PAGE");
  CHECK_EQ(p.hop_ttl, 2, "ttl decremented");
  char hx[17], hn[17];
  hex_of(p.nonce, PAGE_NONCE_LEN, hx, sizeof hx);
  hex_of(nonce, PAGE_NONCE_LEN, hn, sizeof hn);
  CHECK(strcmp(hx, hn) == 0, "nonce unchanged");
  pass("relay PAGE: second hear before TX re-rolls, still sends once");
}

static void test_ack_cancels_page(void) {
  unit_t c;
  make_unit(&c, 3);
  uint8_t dest[NODE_ID_LEN], src[NODE_ID_LEN], nonce[PAGE_NONCE_LEN];
  id_byte(dest, 2);
  id_byte(src, 1);
  memset(nonce, 0xab, PAGE_NONCE_LEN);
  uint8_t frame[RAW_FRAME_MAX];
  size_t flen = 0;
  dummy_page(frame, &flen, dest, src, 3, nonce);
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no TX before ACK");

  dummy_ack(frame, &flen, src, dest, 3, nonce);
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);

  c.ram.now_ms = RELAY_JITTER_MAX_MS;
  tick(&c);
  int n = take_sent(&c, sent, lens, TX_MAX);
  int pages = 0;
  for (int i = 0; i < n; i++) {
    if (frame_type(sent[i], lens[i]) == FRAME_PAGE) pages++;
  }
  CHECK_EQ(pages, 0, "PAGE_ACK cancels pending PAGE");
  pass("relay PAGE: PAGE_ACK cancels pending PAGE");
}

static void test_ack_first(void) {
  unit_t c;
  make_unit(&c, 3);
  uint8_t dest[NODE_ID_LEN], src[NODE_ID_LEN], nonce[PAGE_NONCE_LEN];
  id_byte(dest, 2);
  id_byte(src, 1);
  memset(nonce, 0xab, PAGE_NONCE_LEN);
  uint8_t frame[RAW_FRAME_MAX];
  size_t flen = 0;
  dummy_ack(frame, &flen, src, dest, 3, nonce);
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  dummy_page(frame, &flen, dest, src, 3, nonce);
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  c.ram.now_ms = RELAY_JITTER_MAX_MS;
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  int n = take_sent(&c, sent, lens, TX_MAX);
  int pages = 0;
  for (int i = 0; i < n; i++) {
    if (frame_type(sent[i], lens[i]) == FRAME_PAGE) pages++;
  }
  CHECK_EQ(pages, 0, "no PAGE relay after ACK-first");
  pass("relay PAGE: ACK first then PAGE does not schedule PAGE relay");
}

static void test_ack_reroll(void) {
  unit_t c;
  make_unit(&c, 3);
  uint8_t dest[NODE_ID_LEN], src[NODE_ID_LEN];
  id_byte(dest, 1);
  id_byte(src, 2);
  uint8_t frame[RAW_FRAME_MAX];
  size_t flen = 0;
  dummy_ack(frame, &flen, dest, src, 3, NULL);
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no ACK TX after first hear");

  c.ram.now_ms = 50;
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no ACK TX after second hear");

  c.ram.now_ms = 50 + RELAY_JITTER_MAX_MS;
  tick(&c);
  int n = take_sent(&c, sent, lens, TX_MAX);
  CHECK_EQ(n, 1, "one ACK relay after re-roll");
  CHECK_EQ(frame_type(sent[0], lens[0]), FRAME_PAGE_ACK, "relayed PAGE_ACK");
  page_ack_payload_t ack;
  CHECK(frame_decode_page_ack(sent[0], lens[0], &ack), "decode relayed ACK");
  CHECK_EQ(ack.hop_ttl, 2, "ttl decremented");
  pass("relay ACK: duplicate ACK before TX re-rolls, still sends once");
}

static void test_diff_nonce(void) {
  unit_t c;
  make_unit(&c, 3);
  uint8_t dest[NODE_ID_LEN], src[NODE_ID_LEN], n1[PAGE_NONCE_LEN], n2[PAGE_NONCE_LEN];
  id_byte(dest, 2);
  id_byte(src, 1);
  memset(n1, 0xab, PAGE_NONCE_LEN);
  memset(n2, 0xcd, PAGE_NONCE_LEN);
  uint8_t frame[RAW_FRAME_MAX];
  size_t flen = 0;
  dummy_page(frame, &flen, dest, src, 3, n1);
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  c.ram.now_ms = RELAY_JITTER_MAX_MS;
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 1, "relay first nonce");

  dummy_page(frame, &flen, dest, src, 3, n2);
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  c.ram.now_ms = RELAY_JITTER_MAX_MS * 2;
  tick(&c);
  int n = take_sent(&c, sent, lens, TX_MAX);
  CHECK_EQ(n, 1, "relay second nonce");
  page_payload_t p;
  CHECK(frame_decode_page(sent[0], lens[0], &p), "decode");
  char hx[17], hn[17];
  hex_of(p.nonce, PAGE_NONCE_LEN, hx, sizeof hx);
  hex_of(n2, PAGE_NONCE_LEN, hn, sizeof hn);
  CHECK(strcmp(hx, hn) == 0, "second nonce");
  CHECK_EQ(p.hop_ttl, 2, "ttl decremented");
  pass("relay: different nonce is forwarded again");
}

static void test_relay_evict(void) {
  unit_t c;
  make_unit(&c, 3);
  uint8_t dest[NODE_ID_LEN], src[NODE_ID_LEN], nonce[PAGE_NONCE_LEN];
  id_byte(dest, 2);
  id_byte(src, 1);
  uint8_t frame[RAW_FRAME_MAX];
  size_t flen = 0;
  for (int i = 0; i < RELAY_SEEN_MAX + 1; i++) {
    memset(nonce, 0, sizeof nonce);
    nonce[0] = (uint8_t)(i + 1);
    dummy_page(frame, &flen, dest, src, 3, nonce);
    ram_hal_push_raw(&c.ram, frame, flen, -30);
    tick(&c);
  }
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no TX before jitter");
  c.ram.now_ms = RELAY_JITTER_MAX_MS;
  tick(&c);
  int n = take_sent(&c, sent, lens, TX_MAX);
  CHECK_EQ(n, RELAY_SEEN_MAX, "full cache relays the kept slots");
  int saw_first = 0;
  int saw_last = 0;
  for (int i = 0; i < n; i++) {
    page_payload_t p;
    CHECK(frame_decode_page(sent[i], lens[i], &p), "decode relayed PAGE");
    if (p.nonce[0] == 1) saw_first = 1;
    if (p.nonce[0] == (uint8_t)(RELAY_SEEN_MAX + 1)) saw_last = 1;
    CHECK_EQ(p.hop_ttl, 2, "ttl decremented");
  }
  CHECK_EQ(saw_first, 0, "oldest nonce evicted");
  CHECK_EQ(saw_last, 1, "newest nonce kept");

  memset(nonce, 0, sizeof nonce);
  nonce[0] = 1;
  dummy_page(frame, &flen, dest, src, 3, nonce);
  ram_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  c.ram.now_ms = RELAY_JITTER_MAX_MS * 2;
  tick(&c);
  n = take_sent(&c, sent, lens, TX_MAX);
  CHECK_EQ(n, 1, "evicted nonce relays again");
  page_payload_t again;
  CHECK(frame_decode_page(sent[0], lens[0], &again), "decode evicted PAGE");
  CHECK_EQ(again.nonce[0], 1, "oldest nonce");
  CHECK_EQ(again.hop_ttl, 2, "ttl decremented");
  pass("relay PAGE: full cache evicts the oldest slot");
}

static void test_relay_oversize(void) {
  unit_t c;
  make_unit(&c, 3);
  uint8_t src[NODE_ID_LEN], dest[NODE_ID_LEN], nonce[PAGE_NONCE_LEN];
  id_byte(src, 1);
  id_byte(dest, 2);
  memset(nonce, 0x11, PAGE_NONCE_LEN);
  uint8_t big[RAW_FRAME_MAX + 16];
  memset(big, 0x5a, sizeof big);
  relay_cache_t cache;
  memset(&cache, 0, sizeof cache);
  relay_on_request(&cache, &c.hal, FRAME_PAGE, FRAME_PAGE_ACK, src, dest, nonce, 3, big,
                   sizeof big);
  CHECK_EQ(cache.n, 1, "oversize remembered");
  CHECK(!cache.seen[0].has_pending, "oversize not armed");
  CHECK_EQ(cache.seen[0].pending_len, 0, "pending cleared");
  CHECK_EQ(cache.seen[0].pending[RAW_FRAME_MAX - 1], 0x5a, "copy clamped to the slot");
  c.ram.now_ms = RELAY_JITTER_MAX_MS;
  relay_tick(&cache, &c.hal);
  CHECK_EQ(c.tx.n, 0, "oversize not sent");
  pass("relay: frame longer than the slot is not sent");
}

int main(void) {
  test_dest_decrypts_full();
  test_retries();
  test_retry_nonce_ack();
  test_relay_ttl();
  test_relay_reroll();
  test_ack_cancels_page();
  test_ack_first();
  test_ack_reroll();
  test_diff_nonce();
  test_relay_evict();
  test_relay_oversize();
  printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
