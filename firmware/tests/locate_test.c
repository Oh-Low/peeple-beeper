#include "locate_harness.h"

static void test_direct(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "A", "B");
  set_grid(&a, 0, 0);
  set_grid(&b, 75, 0);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate");
  exchange(&a, &b);
  elapse(&b, GPS_OPEN_REPLY_MS);
  uint8_t frames[4][RAW_FRAME_MAX];
  uint8_t lens[4];
  int n = take_sent(&b, frames, lens, 4);
  CHECK_EQ(n, 1, "one LOC");
  gps_fix_t posted;
  CHECK(open_loc(frames[0], lens[0], &posted), "open LOC");
  CHECK(posted.valid, "body has a fix");
  gps_fix_t expect;
  gps_from_feet(75, 0, &expect);
  CHECK_EQ(posted.lat_min_e4, expect.lat_min_e4, "wire lat");
  CHECK_EQ(posted.lon_min_e4, expect.lon_min_e4, "wire lon");
  emu_hal_push_raw(&a.ram, frames[0], lens[0], -40);
  tick(&a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "found");
  CHECK_EQ(a.locate.their_lon, expect.lon_min_e4, "stored lon");
  locate_view_t view;
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "75 ft") == 0, "75 ft");
  CHECK(view.arrow && view.east_ft > 0 && view.north_ft == 0, "arrow points east");
  emu_hal_set_gps(&a.ram, NULL);
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "no own fix") == 0, "no own fix");
  CHECK(!view.arrow, "no arrow without own fix");
  pass("direct locate: 75 ft east, posted arcminutes");
}

static void test_corner_wire(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "A", "B");
  set_grid(&a, 0, 0);
  set_grid(&b, -150, 150);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate");
  exchange(&a, &b);
  elapse(&b, GPS_OPEN_REPLY_MS);
  uint8_t frames[4][RAW_FRAME_MAX];
  uint8_t lens[4];
  int n = take_sent(&b, frames, lens, 4);
  CHECK_EQ(n, 1, "reply");
  gps_fix_t posted;
  CHECK(open_loc(frames[0], lens[0], &posted), "open");
  CHECK_EQ(posted.lat_min_e4, 248, "carried lat");
  CHECK_EQ(posted.lon_min_e4, -246, "carried lon");
  emu_hal_push_raw(&a.ram, frames[0], lens[0], -40);
  tick(&a);
  locate_view_t view;
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "212 ft") == 0, "diagonal distance");
  CHECK(view.arrow && view.north_ft > 0 && view.east_ft < 0, "arrow points northwest");
  pass("(-150, 150) is carried as 0000.0248 N, 00000.0246 W");
}

static void test_no_sky(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "A", "B");
  set_grid(&a, 0, 0);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate");
  exchange(&a, &b);
  elapse(&b, GPS_OPEN_REPLY_MS);
  exchange(&b, &a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_NO_SKY, "no sky");
  locate_view_t view;
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "no sky") == 0, "no sky text");
  CHECK(!view.arrow, "no arrow without a fix");
  pass("reply with no fix is no sky");
}

static void test_unknown_and_mac(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "A", "B");
  set_grid(&b, 10, 0);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate");
  tick(&a);
  uint8_t frames[4][RAW_FRAME_MAX];
  uint8_t lens[4];
  uint8_t good[RAW_FRAME_MAX];
  int n = take_sent(&a, frames, lens, 4);
  CHECK_EQ(n, 1, "req");
  memcpy(good, frames[0], lens[0]);
  uint8_t good_len = lens[0];
  frames[0][lens[0] - 1] ^= 0x01;
  emu_hal_push_raw(&b.ram, frames[0], lens[0], -40);
  tick(&b);
  CHECK_EQ(take_sent(&b, frames, lens, 4), 0, "bad MAC dropped");

  unit_t c;
  make_unit(&c, 3);
  emu_hal_push_raw(&c.ram, good, good_len, -40);
  tick(&c);
  CHECK_EQ(take_sent(&c, frames, lens, 4), 0, "unknown contact dropped");
  pass("bad MAC and unknown contact get no reply");
}

static void test_no_reply(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "A", "B");
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate");
  tick(&a);
  uint8_t frames[8][RAW_FRAME_MAX];
  uint8_t lens[8];
  CHECK_EQ(take_sent(&a, frames, lens, 8), 1, "first TX");
  for (int i = 1; i < ATTEMPT_LOCATE_BUDGET; i++) {
    a.ram.now_ms = (uint32_t)(i * ATTEMPT_LOCATE_MS);
    tick(&a);
  }
  CHECK_EQ(take_sent(&a, frames, lens, 8), ATTEMPT_LOCATE_BUDGET - 1, "retries");
  CHECK_EQ(locate_status(&a.locate), LOCATE_SENDING, "still waiting");
  a.ram.now_ms = ATTEMPT_LOCATE_BUDGET * ATTEMPT_LOCATE_MS + ATTEMPT_LOCATE_TAIL_MS;
  tick(&a);
  CHECK_EQ(take_sent(&a, frames, lens, 8), 0, "no fifth TX");
  CHECK_EQ(locate_status(&a.locate), LOCATE_NO_REPLY, "no reply");
  pass("four attempts then no reply");
}

static void relay_once(unit_t *relay) {
  tick(relay);
  relay->ram.now_ms += RELAY_JITTER_MAX_MS;
  tick(relay);
}

static void test_hop(void) {
  unit_t a, b, c;
  make_unit(&a, 1);
  make_unit(&b, 2);
  make_unit(&c, 3);
  pair_books(&a, &c, "A", "C");
  set_grid(&a, 0, 0);
  set_grid(&c, 150, 0);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 3);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate");
  exchange(&a, &b);
  relay_once(&b);
  exchange(&b, &c);
  elapse(&c, GPS_OPEN_REPLY_MS);
  CHECK_EQ(c.tx.n, 1, "C replies");
  CHECK_EQ(frame_type(c.tx.frames[0], c.tx.lens[0]), FRAME_LOC, "reply is LOC");
  exchange(&c, &b);
  b.tx.n = 0;
  relay_once(&b);
  exchange(&b, &a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "hopped locate");
  locate_view_t view;
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "150 ft") == 0, "150 ft through relay");
  CHECK(view.arrow && view.east_ft > 0 && view.north_ft == 0, "arrow points east");
  pass("A-B-C hop delivers a locate");
}

static void test_radio_busy(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "A", "B");
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate arms");
  CHECK(message_tx(&a.radio, &a.hal, dest, "hi", &a.contacts, 0), "page arms during locate");
  tick(&a);
  int pages = 0;
  int reqs = 0;
  for (int i = 0; i < a.tx.n; i++) {
    int kind = frame_type(a.tx.frames[i], a.tx.lens[i]);
    if (kind == FRAME_PAGE) pages++;
    if (kind == FRAME_LOC_REQ) reqs++;
  }
  CHECK_EQ(pages, 0, "page waits while locate is sending");
  CHECK_EQ(reqs, 1, "locate takes the tick");
  locate_abort(&a.locate);
  tick(&a);
  pages = 0;
  for (int i = 0; i < a.tx.n; i++) {
    if (frame_type(a.tx.frames[i], a.tx.lens[i]) == FRAME_PAGE) pages++;
  }
  CHECK(pages >= 1, "page runs after locate");

  message_reset(&a.radio);
  locate_abort(&a.locate);
  a.tx.n = 0;
  CHECK(message_tx(&a.radio, &a.hal, dest, "hi", &a.contacts, 0), "page");
  tick(&a);
  a.tx.n = 0;
  a.radio.attempt.next_at = 0;
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate arms during page");
  tick(&a);
  pages = 0;
  reqs = 0;
  for (int i = 0; i < a.tx.n; i++) {
    int kind = frame_type(a.tx.frames[i], a.tx.lens[i]);
    if (kind == FRAME_PAGE) pages++;
    if (kind == FRAME_LOC_REQ) reqs++;
  }
  CHECK_EQ(pages, 0, "due page yields the tick");
  CHECK_EQ(reqs, 1, "locate sends while a page is armed");
  pass("locate and page do not transmit together");
}

static void test_refresh(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "A", "B");
  set_grid(&a, 0, 0);
  set_grid(&b, 75, 0);
  b.ram.hh = 9;
  b.ram.mm = 5;
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate");
  exchange(&a, &b);
  elapse(&b, GPS_OPEN_REPLY_MS);
  exchange(&b, &a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "found");
  locate_view_t view;
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.when, "09:05") == 0, "first fix time");
  int32_t kept = a.locate.their_lon;

  set_grid(&a, 25, 0);
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "50 ft") == 0, "own fix updates the distance");

  set_grid(&b, 150, 0);
  b.ram.hh = 10;
  b.ram.mm = 15;
  CHECK(locate_refresh(&a.locate, &a.hal, &a.contacts), "refresh");
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "50 ft") == 0, "old target until the reply");
  CHECK(strcmp(view.when, "09:05") == 0, "time stays until a new fix");
  exchange(&a, &b);
  elapse(&b, GPS_OPEN_REPLY_MS);
  exchange(&b, &a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "refreshed");
  CHECK(a.locate.their_lon != kept, "new target longitude");
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "125 ft") == 0, "arrow uses both new fixes");
  CHECK(strcmp(view.when, "10:15") == 0, "time is the new reply");

  kept = a.locate.their_lon;
  CHECK(locate_refresh(&a.locate, &a.hal, &a.contacts), "refresh again");
  tick(&a);
  uint8_t frames[8][RAW_FRAME_MAX];
  uint8_t lens[8];
  take_sent(&a, frames, lens, 8);
  for (int i = 1; i < ATTEMPT_LOCATE_BUDGET; i++) {
    a.ram.now_ms = (uint32_t)(i * ATTEMPT_LOCATE_MS);
    tick(&a);
  }
  a.ram.now_ms = ATTEMPT_LOCATE_BUDGET * ATTEMPT_LOCATE_MS + ATTEMPT_LOCATE_TAIL_MS;
  tick(&a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "failed refresh keeps the fix");
  CHECK_EQ(a.locate.their_lon, kept, "old longitude");
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.when, "10:15") == 0, "failed refresh keeps the time");

  emu_hal_set_gps(&b.ram, NULL);
  b.ram.hh = 11;
  b.ram.mm = 0;
  CHECK(locate_refresh(&a.locate, &a.hal, &a.contacts), "refresh no sky");
  exchange(&a, &b);
  elapse(&b, GPS_OPEN_REPLY_MS);
  exchange(&b, &a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "no sky keeps the fix");
  CHECK_EQ(a.locate.their_lon, kept, "no sky keeps the longitude");
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.when, "10:15") == 0, "no sky keeps the time");
  pass("a later settle refreshes, and a miss keeps the last target fix");
}

static void test_track_consent(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "Ada", "Bea");
  set_grid(&b, 75, 0);
  locate_set_open(&b.locate, false);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);

  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "ask locate");
  exchange(&a, &b);
  CHECK_EQ(b.tx.n, 0, "ask holds the reply");
  CHECK(!locate_ask_pending(&b.locate), "alert waits");
  a.ram.now_ms = ATTEMPT_LOCATE_MS;
  tick(&a);
  exchange(&a, &b);
  CHECK_EQ(b.tx.n, 0, "retry still waits");
  elapse(&b, GPS_ALERT_MS);
  CHECK(locate_ask_pending(&b.locate), "prompt waiting");
  CHECK(locate_allow(&b.locate, &b.hal, &b.identity, &b.contacts), "allow");
  exchange(&b, &a);
  CHECK_EQ(a.locate.status, LOCATE_FOUND, "allowed fix");

  uint32_t granted = b.ram.now_ms;
  b.ram.now_ms = granted + TRACK_ALLOW_MS - 1;
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "refresh in window");
  exchange(&a, &b);
  CHECK(!locate_ask_pending(&b.locate), "no prompt inside 30 min");
  CHECK_EQ(b.tx.n, 1, "refresh answered");
  exchange(&b, &a);
  CHECK_EQ(a.locate.status, LOCATE_FOUND, "refresh fix");

  b.ram.now_ms = granted + TRACK_ALLOW_MS;
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "deny locate");
  exchange(&a, &b);
  elapse(&b, GPS_ALERT_MS);
  CHECK(locate_ask_pending(&b.locate), "asks again after 30 min");
  CHECK(locate_deny(&b.locate, &b.hal, &b.identity, &b.contacts), "deny");
  uint8_t flags = 0;
  CHECK(loc_flags(b.tx.frames[0], b.tx.lens[0], &flags), "open deny");
  CHECK((flags & LOC_FLAG_DENY) != 0, "deny flag");
  CHECK((flags & LOC_FLAG_FIX) == 0, "deny has no fix");
  exchange(&b, &a);
  CHECK_EQ(a.locate.status, LOCATE_DENIED, "denied status");
  locate_view_t view;
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "denied") == 0, "denied text");
  CHECK(!view.arrow, "denied has no arrow");

  locate_set_open(&b.locate, true);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "open locate");
  exchange(&a, &b);
  CHECK(!locate_tracked(&b.locate), "warning waits");
  elapse(&b, GPS_OPEN_REPLY_MS);
  CHECK(locate_tracked(&b.locate), "open track warns");
  uint8_t id[NODE_ID_LEN];
  CHECK(locate_tracked_id(&b.locate, id), "tracked id");
  CHECK(node_id_equal(id, a.identity.node_id), "warned about Ada");
  exchange(&b, &a);
  locate_clear_tracked(&b.locate);

  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "quiet locate");
  exchange(&a, &b);
  elapse(&b, GPS_OPEN_REPLY_MS);
  CHECK(!locate_tracked(&b.locate), "quiet window");
  exchange(&b, &a);

  b.ram.now_ms += TRACK_WARN_MS;
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "later locate");
  exchange(&a, &b);
  elapse(&b, GPS_ALERT_MS);
  CHECK(locate_tracked(&b.locate), "warns again after the window");
  pass("tracking asks, denies, and warns when open");
}

static void test_track_ask_queue(void) {
  unit_t a, b, c;
  make_unit(&a, 1);
  make_unit(&b, 2);
  make_unit(&c, 3);
  pair_books(&a, &b, "Ada", "Bea");
  pair_books(&c, &b, "Cy", "Bea");
  set_grid(&b, 75, 0);
  locate_set_open(&b.locate, false);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);

  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "Ada asks");
  exchange(&a, &b);
  CHECK(locate_tx(&c.locate, &c.hal, dest, &c.contacts), "Cy asks");
  exchange(&c, &b);
  uint8_t id[NODE_ID_LEN];
  CHECK(locate_ask_id(&b.locate, id), "prompt waiting");
  CHECK(node_id_equal(id, a.identity.node_id), "prompt is Ada");

  a.ram.now_ms = ATTEMPT_LOCATE_MS;
  tick(&a);
  exchange(&a, &b);
  c.ram.now_ms = ATTEMPT_LOCATE_MS;
  tick(&c);
  exchange(&c, &b);
  CHECK_EQ(b.tx.n, 0, "retries stay held");
  CHECK(locate_ask_id(&b.locate, id), "prompt still waiting");
  CHECK(node_id_equal(id, a.identity.node_id), "retry leaves Ada on screen");

  CHECK(locate_allow(&b.locate, &b.hal, &b.identity, &b.contacts), "allow Ada");
  CHECK_EQ(b.tx.n, 1, "one reply");
  loc_payload_t sent;
  CHECK(frame_decode_loc(b.tx.frames[0], b.tx.lens[0], &sent), "decode Ada's reply");
  CHECK(node_id_equal(sent.dest_id, a.identity.node_id), "reply addressed to Ada");
  CHECK(bytes_eq_n(sent.nonce, a.locate.attempt.nonces[a.locate.attempt.nonce_n - 1], PAGE_NONCE_LEN),
        "reply uses Ada's latest nonce");
  exchange(&b, &a);
  CHECK_EQ(a.locate.status, LOCATE_FOUND, "Ada got the fix");
  CHECK_EQ(c.locate.status, LOCATE_SENDING, "Cy still waiting on a fix");

  elapse(&b, GPS_ALERT_MS);
  CHECK(locate_ask_pending(&b.locate), "Cy is next");
  CHECK(locate_ask_id(&b.locate, id), "second prompt");
  CHECK(node_id_equal(id, c.identity.node_id), "prompt is Cy");
  CHECK(locate_deny(&b.locate, &b.hal, &b.identity, &b.contacts), "deny Cy");
  CHECK_EQ(b.tx.n, 1, "one deny");
  CHECK(frame_decode_loc(b.tx.frames[0], b.tx.lens[0], &sent), "decode Cy's reply");
  CHECK(node_id_equal(sent.dest_id, c.identity.node_id), "deny addressed to Cy");
  CHECK(bytes_eq_n(sent.nonce, c.locate.attempt.nonces[c.locate.attempt.nonce_n - 1], PAGE_NONCE_LEN),
        "deny uses Cy's latest nonce");
  exchange(&b, &c);
  CHECK_EQ(c.locate.status, LOCATE_DENIED, "Cy was denied");
  CHECK(!locate_ask_pending(&b.locate), "no one left waiting");
  pass("a second tracking request waits until the first is answered");
}

static void test_track_prompt_drops_queued_input(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "Ada", "Bea");
  locate_set_open(&b.locate, false);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "Ada asks");
  exchange(&a, &b);
  elapse(&b, GPS_ALERT_MS);
  CHECK(locate_ask_pending(&b.locate), "pending before the screen");

  pair_session_t pair;
  pair_init(&pair);
  screen_t screen;
  screen_init(&screen, &b.identity, &b.contacts, &pair, &b.radio, &b.locate);
  emu_hal_push_encoder(&b.ram, ENC_PRESS);
  emu_hal_push_back(&b.ram);
  CHECK(screen_poll_track(&screen, &b.hal), "prompt takes the screen");
  CHECK_EQ(screen.mode, SCREEN_TRACKASK, "track ask screen");
  CHECK_EQ(b.ram.enc_n, 0, "encoder drained");
  CHECK_EQ(b.ram.back_n, 0, "back drained");
  screen_input(&screen, &b.hal);
  CHECK(locate_ask_pending(&b.locate), "queued press is not the answer");
  CHECK_EQ(screen.mode, SCREEN_TRACKASK, "prompt stays up");

  emu_hal_push_encoder(&b.ram, ENC_PRESS);
  CHECK(!screen_poll_track(&screen, &b.hal), "prompt already up");
  CHECK_EQ(b.ram.enc_n, 1, "later press stays queued");
  screen_input(&screen, &b.hal);
  CHECK(!locate_ask_pending(&b.locate), "press on the prompt answers");
  CHECK_EQ(screen.mode, SCREEN_MENU, "answer leaves the prompt");
  uint8_t flags = 0;
  CHECK_EQ(b.tx.n, 1, "one reply");
  CHECK(loc_flags(b.tx.frames[0], b.tx.lens[0], &flags), "open the reply");
  CHECK((flags & LOC_FLAG_DENY) != 0, "default press denies");
  pass("input queued before the tracking prompt is not the answer");
}

int main(void) {
  test_direct();
  test_corner_wire();
  test_no_sky();
  test_unknown_and_mac();
  test_no_reply();
  test_hop();
  test_radio_busy();
  test_refresh();
  test_track_consent();
  test_track_ask_queue();
  test_track_prompt_drops_queued_input();
  if (g_fail) {
    printf("%d failed, %d passed\n", g_fail, g_pass);
    return 1;
  }
  printf("%d passed\n", g_pass);
  return 0;
}
