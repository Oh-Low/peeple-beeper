#include "crypto.h"
#include "frames.h"
#include "gps.h"
#include "hal_ram.h"
#include "identity.h"
#include "heading.h"
#include "locate.h"
#include "nmea.h"
#include "nrf_hal.h"
#include "page.h"
#include "pager.h"
#include "util.h"

#include <stdio.h>
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
  locate_radio_t locate;
  identity_store_t identity;
  contact_book_t contacts;
  tx_log_t tx;
} unit_t;

static void on_tx(void *user, const uint8_t *bytes, size_t len) {
  tx_log_t *l = user;
  if (l->n >= TX_MAX || !bytes || len == 0) return;
  if (len > RAW_FRAME_MAX) len = RAW_FRAME_MAX;
  memcpy(l->frames[l->n], bytes, len);
  l->lens[l->n] = (uint8_t)len;
  l->n++;
}

static void id_byte(uint8_t *out, uint8_t n) {
  memset(out, n, NODE_ID_LEN);
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
  locate_init(&u->locate);
}

static void tick(unit_t *u) {
  page_tick(&u->radio, &u->identity, &u->contacts, &u->hal, NULL, NULL, &u->locate);
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

static void set_grid(unit_t *u, int east, int north) {
  gps_fix_t fix;
  gps_from_feet(east, north, &fix);
  ram_hal_set_gps(&u->ram, &fix);
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

static int32_t get_i32le(const uint8_t *p) {
  uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  return (int32_t)u;
}

static bool loc_flags(const uint8_t *frame, size_t len, uint8_t *flags) {
  uint8_t key[SESSION_KEY_LEN];
  memset(key, 9, sizeof key);
  loc_payload_t p;
  if (!frame_decode_loc(frame, len, &p)) return false;
  uint8_t plain[LOC_PLAIN_LEN];
  if (!page_open(key, p.nonce, p.body, p.body_len, p.mac, plain)) return false;
  if (flags) *flags = plain[0];
  return true;
}

static bool open_loc(const uint8_t *frame, size_t len, gps_fix_t *out) {
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

static void finish_nmea(char *s, size_t cap) {
  uint8_t cs = 0;
  for (char *p = s + 1; *p; p++) cs = (uint8_t)(cs ^ (uint8_t)*p);
  size_t n = strlen(s);
  snprintf(s + n, cap - n, "*%02X", cs);
}

static void test_grid_quantization(void) {
  gps_fix_t one, corner;
  gps_from_feet(1, 0, &one);
  gps_from_feet(-150, 150, &corner);
  CHECK(one.valid, "1 ft is a fix");
  CHECK_EQ(one.lat_min_e4, 0, "1 ft east has no latitude");
  CHECK_EQ(one.lon_min_e4, 2, "1 ft east is 2 arcminute quanta");
  CHECK_EQ(corner.lat_min_e4, 248, "150 ft north");
  CHECK_EQ(corner.lon_min_e4, -246, "150 ft west");
  char lat[24], lon[24];
  gps_format_lat(lat, sizeof lat, corner.lat_min_e4);
  gps_format_lon(lon, sizeof lon, corner.lon_min_e4);
  CHECK(strcmp(lat, "0000.0248 N") == 0, "latitude posts as RMC");
  CHECK(strcmp(lon, "00000.0246 W") == 0, "longitude posts as RMC");
  pass("foot grid quantizes to PA1616S arcminutes");
}

static void test_nmea(void) {
  char line[128];
  gps_fix_t fix;
  snprintf(line, sizeof line, "$GPRMC,120000.000,A,0000.0248,N,00000.0246,W,0.00,0.00,091026,,,A");
  finish_nmea(line, sizeof line);
  CHECK(nmea_parse_rmc(line, &fix), "parse corner RMC");
  CHECK(fix.valid, "corner fix valid");
  CHECK_EQ(fix.lat_min_e4, 248, "RMC lat");
  CHECK_EQ(fix.lon_min_e4, -246, "RMC lon");

  snprintf(line, sizeof line, "$GPRMC,123519.000,A,4807.0380,N,01131.0000,E,022.4,084.4,230394,,,A");
  finish_nmea(line, sizeof line);
  CHECK(nmea_parse_rmc(line, &fix), "parse classic RMC");
  CHECK_EQ(fix.lat_min_e4, 28870380, "4807.0380 N");
  CHECK_EQ(fix.lon_min_e4, 6910000, "01131.0000 E");

  snprintf(line, sizeof line, "$GNRMC,120000.000,V,,,,,,,091026,,,N");
  finish_nmea(line, sizeof line);
  CHECK(nmea_parse_rmc(line, &fix), "parse void RMC");
  CHECK(!fix.valid, "void fix");

  hal_t hal;
  nrf_hal_init(&hal);
  snprintf(line, sizeof line, "$GPRMC,120000.000,A,0000.0248,N,00000.0246,W,0.00,0.00,091026,,,A");
  finish_nmea(line, sizeof line);
  for (char *p = line; *p; p++) nrf_gps_push_byte((uint8_t)*p);
  nrf_gps_push_byte('\n');
  CHECK(hal.gps_fix(hal.ctx, &fix), "nRF line buffer yields a fix");
  CHECK_EQ(fix.lat_min_e4, 248, "pushed lat");
  CHECK_EQ(fix.lon_min_e4, -246, "pushed lon");
  pass("RMC sentences match the posted arcminutes");
}

static void exchange(unit_t *from, unit_t *to) {
  uint8_t frames[TX_MAX][RAW_FRAME_MAX];
  uint8_t lens[TX_MAX];
  int n = take_sent(from, frames, lens, TX_MAX);
  for (int i = 0; i < n; i++) ram_hal_push_raw(&to->ram, frames[i], lens[i], -40);
  tick(to);
}

static void test_direct(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "A", "B");
  set_grid(&a, 0, 0);
  set_grid(&b, 75, 0);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate");
  exchange(&a, &b);
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
  ram_hal_push_raw(&a.ram, frames[0], lens[0], -40);
  tick(&a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "found");
  CHECK_EQ(a.locate.their_lon, expect.lon_min_e4, "stored lon");
  locate_view_t view;
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "75 ft") == 0, "75 ft");
  CHECK(view.arrow && view.east_ft > 0 && view.north_ft == 0, "arrow points east");
  ram_hal_set_gps(&a.ram, NULL);
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
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate");
  exchange(&a, &b);
  uint8_t frames[4][RAW_FRAME_MAX];
  uint8_t lens[4];
  int n = take_sent(&b, frames, lens, 4);
  CHECK_EQ(n, 1, "reply");
  gps_fix_t posted;
  CHECK(open_loc(frames[0], lens[0], &posted), "open");
  CHECK_EQ(posted.lat_min_e4, 248, "carried lat");
  CHECK_EQ(posted.lon_min_e4, -246, "carried lon");
  ram_hal_push_raw(&a.ram, frames[0], lens[0], -40);
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
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate");
  exchange(&a, &b);
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
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate");
  uint8_t frames[4][RAW_FRAME_MAX];
  uint8_t lens[4];
  uint8_t good[RAW_FRAME_MAX];
  int n = take_sent(&a, frames, lens, 4);
  CHECK_EQ(n, 1, "req");
  memcpy(good, frames[0], lens[0]);
  uint8_t good_len = lens[0];
  frames[0][lens[0] - 1] ^= 0x01;
  ram_hal_push_raw(&b.ram, frames[0], lens[0], -40);
  tick(&b);
  CHECK_EQ(take_sent(&b, frames, lens, 4), 0, "bad MAC dropped");

  unit_t c;
  make_unit(&c, 3);
  ram_hal_push_raw(&c.ram, good, good_len, -40);
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
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate");
  uint8_t frames[8][RAW_FRAME_MAX];
  uint8_t lens[8];
  CHECK_EQ(take_sent(&a, frames, lens, 8), 1, "first TX");
  for (int i = 1; i < LOCATE_ATTEMPTS; i++) {
    a.ram.now_ms = (uint32_t)(i * LOCATE_RETRY_MS);
    tick(&a);
  }
  CHECK_EQ(take_sent(&a, frames, lens, 8), LOCATE_ATTEMPTS - 1, "retries");
  CHECK_EQ(locate_status(&a.locate), LOCATE_SENDING, "still waiting");
  a.ram.now_ms = LOCATE_ATTEMPTS * LOCATE_RETRY_MS;
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
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate");
  exchange(&a, &b);
  CHECK_EQ(b.tx.n, 0, "no TX before jitter");
  relay_once(&b);
  exchange(&b, &c);
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
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate starts");
  CHECK(!page_tx(&a.radio, &a.hal, dest, "hi", &a.identity, &a.contacts, &a.locate, 0),
        "page waits for locate");
  page_radio_reset(&a.radio);
  locate_abort(&a.locate);
  CHECK(page_tx(&a.radio, &a.hal, dest, "hi", &a.identity, &a.contacts, &a.locate, 0), "page");
  CHECK(!locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate waits for page");
  page_radio_reset(&a.radio);
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate again");
  memcpy(a.radio.outbox.dest_id, dest, NODE_ID_LEN);
  str_copy_max(a.radio.body, sizeof a.radio.body, "hi", 2);
  a.radio.has_outbox = true;
  a.radio.status = PAGE_SENDING;
  a.radio.outbox.attempts = 1;
  a.radio.outbox.next_at = 0;
  a.tx.n = 0;
  tick(&a);
  int pages = 0;
  for (int i = 0; i < a.tx.n; i++) {
    if (frame_type(a.tx.frames[i], a.tx.lens[i]) == FRAME_PAGE) pages++;
  }
  CHECK_EQ(pages, 0, "page retry waits while locate is in flight");
  locate_abort(&a.locate);
  tick(&a);
  pages = 0;
  for (int i = 0; i < a.tx.n; i++) {
    if (frame_type(a.tx.frames[i], a.tx.lens[i]) == FRAME_PAGE) pages++;
  }
  CHECK(pages >= 1, "page retry runs after locate");
  pass("locate and page do not transmit together");
}

static heading_view_t pose_at(const heading_sample_t *sample, int north, int east, uint32_t now,
                             bool *holding, uint32_t *since) {
  heading_view_t view;
  heading_view(sample, north, east, now, holding, since, &view);
  return view;
}

static void test_heading_pose(void) {
  heading_sample_t s = {.tilt_deg = 25, .still = true, .motor = false, .heading_deg = 0};
  bool holding = false;
  uint32_t since = 0;
  heading_view_t v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.hold_flat && !v.arrow && !v.dont_move, "tipped asks to hold flat");

  s.tilt_deg = 0;
  s.still = false;
  v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.hold_still && !v.hold_flat && !v.arrow, "moving asks to hold still");

  s.still = true;
  s.motor = true;
  v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.hold_still && !v.arrow, "motor hides the heading");

  s.motor = false;
  s.tilt_deg = 25;
  s.still = false;
  v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.hold_flat && v.hold_still && !v.arrow, "tipped and moving shows both");

  s.tilt_deg = 0;
  s.still = true;
  holding = false;
  v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.dont_move && !v.arrow && holding, "flat and still waits for a clean reading");
  v = pose_at(&s, 0, 75, 1000 + HEADING_SETTLE_MS, &holding, &since);
  CHECK(v.arrow && v.octant == 2, "east is octant 2 when the top points north");

  s.heading_deg = 90;
  holding = false;
  v = pose_at(&s, 0, 75, 5000, &holding, &since);
  v = pose_at(&s, 0, 75, 5000 + HEADING_SETTLE_MS, &holding, &since);
  CHECK(v.arrow && v.octant == 0, "facing east, an eastern contact is straight ahead");
  pass("heading waits for flat and still, then an 8-way arrow");
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
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate");
  exchange(&a, &b);
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
  CHECK(locate_refresh(&a.locate, &a.radio, &a.hal, &a.identity, &a.contacts), "refresh");
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "50 ft") == 0, "old target until the reply");
  CHECK(strcmp(view.when, "09:05") == 0, "time stays until a new fix");
  exchange(&a, &b);
  exchange(&b, &a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "refreshed");
  CHECK(a.locate.their_lon != kept, "new target longitude");
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "125 ft") == 0, "arrow uses both new fixes");
  CHECK(strcmp(view.when, "10:15") == 0, "time is the new reply");

  kept = a.locate.their_lon;
  CHECK(locate_refresh(&a.locate, &a.radio, &a.hal, &a.identity, &a.contacts), "refresh again");
  uint8_t frames[8][RAW_FRAME_MAX];
  uint8_t lens[8];
  take_sent(&a, frames, lens, 8);
  for (int i = 1; i < LOCATE_ATTEMPTS; i++) {
    a.ram.now_ms = (uint32_t)(i * LOCATE_RETRY_MS);
    tick(&a);
  }
  a.ram.now_ms = LOCATE_ATTEMPTS * LOCATE_RETRY_MS;
  tick(&a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "failed refresh keeps the fix");
  CHECK_EQ(a.locate.their_lon, kept, "old longitude");
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.when, "10:15") == 0, "failed refresh keeps the time");

  ram_hal_set_gps(&b.ram, NULL);
  b.ram.hh = 11;
  b.ram.mm = 0;
  CHECK(locate_refresh(&a.locate, &a.radio, &a.hal, &a.identity, &a.contacts), "refresh no sky");
  exchange(&a, &b);
  exchange(&b, &a);
  CHECK_EQ(locate_status(&a.locate), LOCATE_FOUND, "no sky keeps the fix");
  CHECK_EQ(a.locate.their_lon, kept, "no sky keeps the longitude");
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.when, "10:15") == 0, "no sky keeps the time");
  pass("a later settle refreshes, and a miss keeps the last target fix");
}

static void test_gps_sample(void) {
  unit_t a, b;
  make_unit(&a, 1);
  make_unit(&b, 2);
  pair_books(&a, &b, "A", "B");
  set_grid(&a, 0, 0);
  set_grid(&b, 75, 0);
  b.ram.hh = 8;
  b.ram.mm = 30;
  locate_gps_tick(&b.locate, &b.hal);
  locate_gps_tick(&b.locate, &b.hal);
  CHECK(b.locate.saved_fix, "sample stored");
  CHECK(!b.ram.gps_on, "receiver sleeps after a fix");
  gps_fix_t expect;
  gps_from_feet(75, 0, &expect);
  CHECK_EQ(b.locate.saved_lon, expect.lon_min_e4, "stored lon");
  CHECK_EQ(b.locate.saved_hh, 8, "stored hour");

  ram_hal_set_gps(&b.ram, NULL);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "locate");
  exchange(&a, &b);
  exchange(&b, &a);
  locate_view_t view;
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "75 ft") == 0, "no sky still sends the stored fix");
  CHECK(strcmp(view.when, "08:30") == 0, "timestamp is when the fix was taken");

  set_grid(&b, 40, 0);
  b.ram.hh = 9;
  b.ram.mm = 0;
  b.ram.now_ms = GPS_SAMPLE_PERIOD_MS;
  locate_gps_tick(&b.locate, &b.hal);
  locate_gps_tick(&b.locate, &b.hal);
  CHECK_EQ(b.locate.saved_hh, 9, "later sample updates the time");
  gps_from_feet(40, 0, &expect);
  CHECK_EQ(b.locate.saved_lon, expect.lon_min_e4, "later sample updates the fix");
  CHECK(!b.ram.gps_on, "receiver sleeps again");
  pass("a stored fix is sent with its timestamp when the sky is gone");
}

static void test_capture_after_move(void) {
  unit_t a;
  make_unit(&a, 1);
  set_grid(&a, 0, 0);
  locate_gps_tick(&a.locate, &a.hal);
  locate_gps_tick(&a.locate, &a.hal);
  CHECK(!a.ram.gps_on, "asleep at the origin");
  a.locate.has_fix = true;
  a.locate.status = LOCATE_FOUND;
  gps_fix_t theirs;
  gps_from_feet(75, 0, &theirs);
  a.locate.their_lat = theirs.lat_min_e4;
  a.locate.their_lon = theirs.lon_min_e4;
  locate_view_t view;
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "75 ft") == 0, "origin to 75 ft");

  set_grid(&a, 25, 0);
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "75 ft") == 0, "asleep receiver ignores the new grid");
  locate_gps_capture(&a.locate, &a.hal);
  CHECK(!a.ram.gps_on, "capture leaves the receiver off");
  locate_format(&a.locate, &a.hal, &view);
  CHECK(strcmp(view.dist, "50 ft") == 0, "capture stores the moved fix");
  pass("returning to level rereads this unit before drawing the arrow");
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

  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "ask locate");
  exchange(&a, &b);
  CHECK_EQ(b.tx.n, 0, "ask holds the reply");
  CHECK(locate_ask_pending(&b.locate), "prompt waiting");
  a.ram.now_ms = LOCATE_RETRY_MS;
  tick(&a);
  exchange(&a, &b);
  CHECK_EQ(b.tx.n, 0, "retry still waits");
  CHECK(locate_allow(&b.locate, &b.hal, &b.identity, &b.contacts), "allow");
  exchange(&b, &a);
  CHECK_EQ(a.locate.status, LOCATE_FOUND, "allowed fix");

  b.ram.now_ms = TRACK_ALLOW_MS - 1;
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "refresh in window");
  exchange(&a, &b);
  CHECK(!locate_ask_pending(&b.locate), "no prompt inside 30 min");
  CHECK_EQ(b.tx.n, 1, "refresh answered");
  exchange(&b, &a);
  CHECK_EQ(a.locate.status, LOCATE_FOUND, "refresh fix");

  b.ram.now_ms = TRACK_ALLOW_MS;
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "deny locate");
  exchange(&a, &b);
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
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "open locate");
  exchange(&a, &b);
  CHECK(locate_tracked(&b.locate), "open track warns");
  uint8_t id[NODE_ID_LEN];
  CHECK(locate_tracked_id(&b.locate, id), "tracked id");
  CHECK(node_id_equal(id, a.identity.node_id), "warned about Ada");
  exchange(&b, &a);
  locate_clear_tracked(&b.locate);

  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "quiet locate");
  exchange(&a, &b);
  CHECK(!locate_tracked(&b.locate), "quiet window");
  exchange(&b, &a);

  b.ram.now_ms = TRACK_ALLOW_MS + TRACK_WARN_MS;
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "later locate");
  exchange(&a, &b);
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

  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "Ada asks");
  exchange(&a, &b);
  CHECK(locate_tx(&c.locate, &c.radio, &c.hal, dest, &c.identity, &c.contacts), "Cy asks");
  exchange(&c, &b);
  uint8_t id[NODE_ID_LEN];
  CHECK(locate_ask_id(&b.locate, id), "prompt waiting");
  CHECK(node_id_equal(id, a.identity.node_id), "prompt is Ada");

  a.ram.now_ms = LOCATE_RETRY_MS;
  tick(&a);
  exchange(&a, &b);
  c.ram.now_ms = LOCATE_RETRY_MS;
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
  CHECK(bytes_eq_n(sent.nonce, a.locate.nonces[a.locate.nonce_n - 1], PAGE_NONCE_LEN),
        "reply uses Ada's latest nonce");
  exchange(&b, &a);
  CHECK_EQ(a.locate.status, LOCATE_FOUND, "Ada got the fix");
  CHECK_EQ(c.locate.status, LOCATE_SENDING, "Cy still waiting on a fix");

  CHECK(locate_ask_pending(&b.locate), "Cy is next");
  CHECK(locate_ask_id(&b.locate, id), "second prompt");
  CHECK(node_id_equal(id, c.identity.node_id), "prompt is Cy");
  CHECK(locate_deny(&b.locate, &b.hal, &b.identity, &b.contacts), "deny Cy");
  CHECK_EQ(b.tx.n, 1, "one deny");
  CHECK(frame_decode_loc(b.tx.frames[0], b.tx.lens[0], &sent), "decode Cy's reply");
  CHECK(node_id_equal(sent.dest_id, c.identity.node_id), "deny addressed to Cy");
  CHECK(bytes_eq_n(sent.nonce, c.locate.nonces[c.locate.nonce_n - 1], PAGE_NONCE_LEN),
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
  CHECK(locate_tx(&a.locate, &a.radio, &a.hal, dest, &a.identity, &a.contacts), "Ada asks");
  exchange(&a, &b);
  CHECK(locate_ask_pending(&b.locate), "pending before the screen");

  pair_session_t pair;
  pair_init(&pair);
  pager_t pager;
  pager_init(&pager, &b.identity, &b.contacts, &pair, &b.radio, &b.locate);
  ram_hal_push_encoder(&b.ram, ENC_PRESS);
  ram_hal_push_back(&b.ram);
  CHECK(pager_poll_track(&pager, &b.hal), "prompt takes the screen");
  CHECK_EQ(pager.screen, SCREEN_TRACKASK, "track ask screen");
  CHECK_EQ(b.ram.enc_n, 0, "encoder drained");
  CHECK_EQ(b.ram.back_n, 0, "back drained");
  pager_input(&pager, &b.hal);
  CHECK(locate_ask_pending(&b.locate), "queued press is not the answer");
  CHECK_EQ(pager.screen, SCREEN_TRACKASK, "prompt stays up");

  ram_hal_push_encoder(&b.ram, ENC_PRESS);
  CHECK(!pager_poll_track(&pager, &b.hal), "prompt already up");
  CHECK_EQ(b.ram.enc_n, 1, "later press stays queued");
  pager_input(&pager, &b.hal);
  CHECK(!locate_ask_pending(&b.locate), "press on the prompt answers");
  CHECK_EQ(pager.screen, SCREEN_MENU, "answer leaves the prompt");
  uint8_t flags = 0;
  CHECK_EQ(b.tx.n, 1, "one reply");
  CHECK(loc_flags(b.tx.frames[0], b.tx.lens[0], &flags), "open the reply");
  CHECK((flags & LOC_FLAG_DENY) != 0, "default press denies");
  pass("input queued before the tracking prompt is not the answer");
}

int main(void) {
  test_grid_quantization();
  test_nmea();
  test_direct();
  test_corner_wire();
  test_no_sky();
  test_unknown_and_mac();
  test_no_reply();
  test_hop();
  test_radio_busy();
  test_heading_pose();
  test_refresh();
  test_gps_sample();
  test_capture_after_move();
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
