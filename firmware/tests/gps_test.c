#include "gps.h"
#include "nmea.h"
#include "beeper_hal.h"

#include <stdio.h>
#include <string.h>

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
  CHECK(nmea_parse_rmc(line, &fix, NULL, NULL), "parse corner RMC");
  CHECK(fix.valid, "corner fix valid");
  CHECK_EQ(fix.lat_min_e4, 248, "RMC lat");
  CHECK_EQ(fix.lon_min_e4, -246, "RMC lon");

  snprintf(line, sizeof line, "$GPRMC,123519.000,A,4807.0380,N,01131.0000,E,022.4,084.4,230394,,,A");
  finish_nmea(line, sizeof line);
  CHECK(nmea_parse_rmc(line, &fix, NULL, NULL), "parse classic RMC");
  CHECK_EQ(fix.lat_min_e4, 28870380, "4807.0380 N");
  CHECK_EQ(fix.lon_min_e4, 6910000, "01131.0000 E");

  snprintf(line, sizeof line, "$GNRMC,120000.000,V,,,,,,,091026,,,N");
  finish_nmea(line, sizeof line);
  CHECK(nmea_parse_rmc(line, &fix, NULL, NULL), "parse void RMC");
  CHECK(!fix.valid, "void fix");

  hal_t hal;
  beeper_hal_init(&hal);
  snprintf(line, sizeof line, "$GPRMC,120000.000,A,0000.0248,N,00000.0246,W,0.00,0.00,091026,,,A");
  finish_nmea(line, sizeof line);
  for (char *p = line; *p; p++) beeper_gps_push_byte((uint8_t)*p);
  beeper_gps_push_byte('\n');
  CHECK(hal.gps_fix(hal.ctx, &fix), "nRF line buffer yields a fix");
  CHECK_EQ(fix.lat_min_e4, 248, "pushed lat");
  CHECK_EQ(fix.lon_min_e4, -246, "pushed lon");

  char classic[128];
  snprintf(classic, sizeof classic,
           "$GPRMC,123519.000,A,4807.0380,N,01131.0000,E,022.4,084.4,230394,,,A");
  finish_nmea(classic, sizeof classic);
  for (int i = 0; i < 95; i++) beeper_gps_push_byte('X');
  for (char *p = classic; *p; p++) beeper_gps_push_byte((uint8_t)*p);
  beeper_gps_push_byte('\n');
  CHECK(hal.gps_fix(hal.ctx, &fix), "previous fix still stands");
  CHECK_EQ(fix.lat_min_e4, 248, "an overlong line is not stored");

  for (char *p = classic; *p; p++) beeper_gps_push_byte((uint8_t)*p);
  beeper_gps_push_byte('\n');
  CHECK(hal.gps_fix(hal.ctx, &fix), "the next line parses");
  CHECK_EQ(fix.lat_min_e4, 28870380, "next line lat");
  uint8_t hh = 0;
  uint8_t mm = 0;
  hal.clock(hal.ctx, &hh, &mm);
  CHECK_EQ(hh, 12, "clock hour");
  CHECK_EQ(mm, 35, "clock minute");
  hal.gps_power(hal.ctx, false);
  hal.gps_power(hal.ctx, true);
  CHECK(!hal.gps_fix(hal.ctx, &fix), "a new listen waits for a new sentence");
  hal.clock(hal.ctx, &hh, &mm);
  CHECK_EQ(hh, 12, "power cycle keeps the clock");
  CHECK_EQ(mm, 35, "power cycle keeps the minute");
  pass("RMC sentences match the posted arcminutes");
}

int main(void) {
  test_grid_quantization();
  test_nmea();
  if (g_fail) {
    printf("%d failed, %d passed\n", g_fail, g_pass);
    return 1;
  }
  printf("%d passed\n", g_pass);
  return 0;
}
