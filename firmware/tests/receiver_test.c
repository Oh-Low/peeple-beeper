#include "locate_harness.h"

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

  emu_hal_set_gps(&b.ram, NULL);
  uint8_t dest[NODE_ID_LEN];
  id_byte(dest, 2);
  CHECK(locate_tx(&a.locate, &a.hal, dest, &a.contacts), "locate");
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

static void test_cold_start_stays_on(void) {
  unit_t a;
  make_unit(&a, 1);
  emu_hal_set_gps(&a.ram, NULL);
  locate_gps_tick(&a.locate, &a.hal);
  CHECK(a.ram.gps_on, "cold start powers the receiver");
  a.ram.now_ms = GPS_LISTEN_MS;
  locate_gps_tick(&a.locate, &a.hal);
  CHECK(!a.locate.saved_fix, "no sentence yet");
  CHECK(a.ram.gps_on, "EN stays high without a fix");
  set_grid(&a, 10, 0);
  locate_gps_tick(&a.locate, &a.hal);
  locate_gps_tick(&a.locate, &a.hal);
  CHECK(a.locate.saved_fix, "first fix is stored");
  CHECK(!a.ram.gps_on, "a stored fix sleeps the receiver");
  pass("a cold start leaves the receiver on until a fix");
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

int main(void) {
  test_gps_sample();
  test_cold_start_stays_on();
  test_capture_after_move();
  if (g_fail) {
    printf("%d failed, %d passed\n", g_fail, g_pass);
    return 1;
  }
  printf("%d passed\n", g_pass);
  return 0;
}
