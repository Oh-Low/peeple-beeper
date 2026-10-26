#include "gps.h"

#include <stdio.h>

/* lat_min_e4 = round(north_ft * 0.3048 * 60 * 10000 / M_PER_DEG). */
#define GPS_FT_NUM 1828800000LL
#define GPS_LAT_DEN 1105742725LL
#define GPS_LON_DEN 1113194908LL

static int32_t div_round(int64_t num, int64_t den) {
  if (den < 0) {
    num = -num;
    den = -den;
  }
  if (num >= 0) return (int32_t)((num + den / 2) / den);
  return (int32_t)(-((-num + den / 2) / den));
}

static int32_t feet_to_min_e4(int feet, int64_t den) {
  return div_round((int64_t)feet * GPS_FT_NUM, den);
}

static int min_e4_to_feet(int32_t min_e4, int64_t den) {
  return (int)div_round((int64_t)min_e4 * den, GPS_FT_NUM);
}

void gps_from_feet(int east_ft, int north_ft, gps_fix_t *out) {
  if (!out) return;
  out->valid = true;
  out->lat_min_e4 = feet_to_min_e4(north_ft, GPS_LAT_DEN);
  out->lon_min_e4 = feet_to_min_e4(east_ft, GPS_LON_DEN);
}

void gps_delta_ft(int32_t lat_a, int32_t lon_a, int32_t lat_b, int32_t lon_b, int *north_ft,
                  int *east_ft) {
  if (north_ft) *north_ft = min_e4_to_feet(lat_b - lat_a, GPS_LAT_DEN);
  if (east_ft) *east_ft = min_e4_to_feet(lon_b - lon_a, GPS_LON_DEN);
}

static int isqrt64(int64_t n) {
  if (n <= 0) return 0;
  int64_t x = n;
  int64_t y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return (int)x;
}

int gps_hypot_ft(int north_ft, int east_ft) {
  int64_t n = (int64_t)north_ft * (int64_t)north_ft + (int64_t)east_ft * (int64_t)east_ft;
  return isqrt64(n);
}

const char *gps_compass(int north_ft, int east_ft) {
  if (north_ft == 0 && east_ft == 0) return "";
  int an = north_ft < 0 ? -north_ft : north_ft;
  int ae = east_ft < 0 ? -east_ft : east_ft;
  bool ns = (int64_t)ae * 10000 < (int64_t)an * 4142;
  bool ew = (int64_t)an * 10000 < (int64_t)ae * 4142;
  if (ns && !ew) return north_ft >= 0 ? "N" : "S";
  if (ew && !ns) return east_ft >= 0 ? "E" : "W";
  if (north_ft >= 0 && east_ft >= 0) return "NE";
  if (north_ft < 0 && east_ft >= 0) return "SE";
  if (north_ft < 0 && east_ft < 0) return "SW";
  return "NW";
}

static void format_arcmin(char *dst, size_t cap, int32_t min_e4, int deg_width, char pos, char neg) {
  char hemi = min_e4 < 0 ? neg : pos;
  int32_t v = min_e4 < 0 ? -min_e4 : min_e4;
  int32_t deg = v / 600000;
  int32_t rem = v % 600000;
  int32_t minutes = rem / 10000;
  int32_t frac = rem % 10000;
  if (deg_width >= 3) snprintf(dst, cap, "%03d%02d.%04d %c", deg, minutes, frac, hemi);
  else snprintf(dst, cap, "%02d%02d.%04d %c", deg, minutes, frac, hemi);
}

void gps_format_lat(char *dst, size_t cap, int32_t lat_min_e4) {
  if (!dst || cap == 0) return;
  format_arcmin(dst, cap, lat_min_e4, 2, 'N', 'S');
}

void gps_format_lon(char *dst, size_t cap, int32_t lon_min_e4) {
  if (!dst || cap == 0) return;
  format_arcmin(dst, cap, lon_min_e4, 3, 'E', 'W');
}
