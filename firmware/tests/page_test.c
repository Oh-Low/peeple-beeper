#include "page_harness.h"

static void test_dest_decrypts_full(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "Alice", "Bob");
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(message_tx(&a.radio, &a.hal, dest, "hi", &a.contacts, 3), "pageTx");
  CHECK_EQ(message_status(&a.radio), MESSAGE_SENDING, "status after tx");
  tick(&a);
  uint8_t frames[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  int n = take_sent(&a, frames, lens, TX_MAX);
  CHECK_EQ(n, 1, "one PAGE on first tx");
  CHECK_EQ(frame_type(frames[0], lens[0]), FRAME_PAGE, "first frame is PAGE");
  memcpy(g_saved_page, frames[0], lens[0]);
  g_saved_page_len = lens[0];

  emu_hal_push_raw(&b.ram, frames[0], lens[0], -30);
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

  emu_hal_push_raw(&a.ram, frames[0], lens[0], -30);
  tick(&a);
  CHECK_EQ(message_status(&a.radio), MESSAGE_DELIVERED, "sender delivered");

  emu_hal_push_raw(&b.ram, g_saved_page, g_saved_page_len, -30);
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
  CHECK(message_tx(&a.radio, &a.hal, dest, "ping", &a.contacts, 0), "pageTx");
  tick(&a);

  int tx_at[ATTEMPT_PAGE_BUDGET];
  tx_at[0] = 0;
  int t = 0;
  for (int i = 1; i < ATTEMPT_PAGE_BUDGET; i++) {
    t += i < ATTEMPT_PAGE_FAST ? ATTEMPT_PAGE_FAST_MS : ATTEMPT_PAGE_SLOW_MS;
    tx_at[i] = t;
    a.ram.now_ms = (uint32_t)t;
    tick(&a);
  }

  uint8_t frames[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  int n = take_sent(&a, frames, lens, TX_MAX);
  CHECK_EQ(n, ATTEMPT_PAGE_BUDGET, "9 TX");
  char hexes[ATTEMPT_PAGE_BUDGET][17];
  for (int i = 0; i < n; i++) {
    page_payload_t p;
    CHECK(frame_decode_page(frames[i], lens[i], &p), "decode PAGE");
    hex_of(p.nonce, PAGE_NONCE_LEN, hexes[i], sizeof hexes[i]);
  }
  for (int i = 1; i < n; i++) {
    CHECK(strcmp(hexes[i], hexes[0]) != 0, "nonce differs from tx1");
    for (int j = 0; j < i; j++) CHECK(strcmp(hexes[i], hexes[j]) != 0, "nonce unique");
  }
  CHECK_EQ(message_status(&a.radio), MESSAGE_SENDING, "still sending after last TX");
  CHECK_EQ(tx_at[3], 30000, "4th TX at 30s");
  CHECK_EQ(tx_at[4], 90000, "5th TX at 90s");
  CHECK_EQ(tx_at[8], 330000, "9th TX at 330s");

  a.ram.now_ms = (uint32_t)(t + ATTEMPT_PAGE_SLOW_MS);
  tick(&a);
  CHECK_EQ(message_status(&a.radio), MESSAGE_FAILED, "failed after last ACK wait");
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
  CHECK(message_tx(&a.radio, &a.hal, dest, "hi", &a.contacts, 0), "pageTx");
  tick(&a);
  uint8_t first[TX_MAX][RAW_FRAME_MAX];
  uint8_t flen[TX_MAX];
  int n = take_sent(&a, first, flen, TX_MAX);
  CHECK_EQ(n, 1, "first TX");

  emu_hal_push_raw(&b.ram, first[0], flen[0], -30);
  tick(&b);
  CHECK_EQ(b.page_n, 1, "deliver first copy");
  CHECK(strcmp(b.pages[0].body, "hi") == 0, "body without msg_id");
  uint8_t ack1[TX_MAX][RAW_FRAME_MAX];
  uint8_t alen[TX_MAX];
  n = take_sent(&b, ack1, alen, TX_MAX);
  CHECK_EQ(n, 1, "ACK nonce 1");

  a.ram.now_ms = ATTEMPT_PAGE_FAST_MS;
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

  emu_hal_push_raw(&b.ram, second[0], slen[0], -30);
  tick(&b);
  CHECK_EQ(b.page_n, 1, "retry does not deliver again");
  n = take_sent(&b, second, slen, TX_MAX);
  CHECK_EQ(n, 1, "ACK nonce 2");

  emu_hal_push_raw(&a.ram, ack1[0], alen[0], -30);
  tick(&a);
  CHECK_EQ(message_status(&a.radio), MESSAGE_DELIVERED, "ACK of first nonce counts");
  pass("retry new nonce: dest delivers once; ACK of first nonce still delivers");
}

int main(void) {
  test_dest_decrypts_full();
  test_retries();
  test_retry_nonce_ack();
  printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
