#include "page_harness.h"

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
  emu_hal_push_raw(&c.ram, frame, flen, -30);
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
  emu_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  c.ram.now_ms = second_hear + RELAY_JITTER_MAX_MS;
  tick(&c);
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "already-sent hear does not TX again");

  unit_t c3;
  make_unit(&c3, 3);
  c3.ram.now_ms = 0;
  dummy_page(frame, &flen, dest, src, 1, nonce);
  emu_hal_push_raw(&c3.ram, frame, flen, -30);
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
  emu_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no TX after first hear");

  c.ram.now_ms = 50;
  emu_hal_push_raw(&c.ram, frame, flen, -30);
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
  emu_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no TX before ACK");

  dummy_ack(frame, &flen, src, dest, 3, nonce);
  emu_hal_push_raw(&c.ram, frame, flen, -30);
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
  emu_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  dummy_page(frame, &flen, dest, src, 3, nonce);
  emu_hal_push_raw(&c.ram, frame, flen, -30);
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
  emu_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 0, "no ACK TX after first hear");

  c.ram.now_ms = 50;
  emu_hal_push_raw(&c.ram, frame, flen, -30);
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
  emu_hal_push_raw(&c.ram, frame, flen, -30);
  tick(&c);
  c.ram.now_ms = RELAY_JITTER_MAX_MS;
  tick(&c);
  uint8_t sent[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  CHECK_EQ(take_sent(&c, sent, lens, TX_MAX), 1, "relay first nonce");

  dummy_page(frame, &flen, dest, src, 3, n2);
  emu_hal_push_raw(&c.ram, frame, flen, -30);
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
    emu_hal_push_raw(&c.ram, frame, flen, -30);
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
  emu_hal_push_raw(&c.ram, frame, flen, -30);
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
