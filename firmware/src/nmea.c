#include "nmea.h"

#include <string.h>

bool nmea_push_byte(nmea_line_t *line, uint8_t byte, gps_fix_t *out, uint8_t *hh, uint8_t *mm) {
  if (!line) return false;
  if (byte == '\r') return false;
  if (byte == '\n') {
    line->bytes[line->len] = 0;
    bool skip = line->skip;
    line->len = 0;
    line->skip = false;
    if (skip) return false;
    return nmea_parse_rmc(line->bytes, out, hh, mm);
  }
  if (line->skip || line->len >= NMEA_LINE_MAX - 1) {
    line->skip = true;
    return false;
  }
  line->bytes[line->len++] = (char)byte;
  return false;
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static bool checksum_ok(const char *line) {
  const char *star = strchr(line, '*');
  if (!star) return true;
  if (star[1] == 0 || star[2] == 0) return false;
  int hi = hex_nibble(star[1]);
  int lo = hex_nibble(star[2]);
  if (hi < 0 || lo < 0) return false;
  uint8_t cs = 0;
  const char *p = line;
  if (*p == '$') p++;
  for (; p < star; p++) cs = (uint8_t)(cs ^ (uint8_t)*p);
  return cs == (uint8_t)((hi << 4) | lo);
}

static bool is_rmc(const char *line) {
  const char *p = line;
  if (*p == '$') p++;
  if (p[0] == 0 || p[1] == 0) return false;
  return p[2] == 'R' && p[3] == 'M' && p[4] == 'C';
}

/* ddmm.mmmm or dddmm.mmmm → signed arcminutes * 10000, before hemisphere. */
static bool parse_coord(const char *s, int32_t *min_e4) {
  if (!s || !*s) return false;
  int64_t ip = 0;
  int frac = 0;
  int frac_digits = 0;
  bool saw = false;
  const char *p = s;
  while (*p >= '0' && *p <= '9') {
    ip = ip * 10 + (*p - '0');
    saw = true;
    p++;
  }
  if (*p == '.') {
    p++;
    while (*p >= '0' && *p <= '9' && frac_digits < 4) {
      frac = frac * 10 + (*p - '0');
      frac_digits++;
      p++;
    }
  }
  if (!saw) return false;
  while (frac_digits < 4) {
    frac *= 10;
    frac_digits++;
  }
  int64_t degrees = ip / 100;
  int64_t minutes_e4 = (ip % 100) * 10000 + frac;
  *min_e4 = (int32_t)(degrees * 60 * 10000 + minutes_e4);
  return true;
}

static const char *field_at(const char *line, int index, char *buf, size_t cap) {
  const char *p = line;
  if (*p == '$') p++;
  int n = 0;
  while (*p && *p != '*') {
    const char *start = p;
    while (*p && *p != ',' && *p != '*') p++;
    if (n == index) {
      size_t len = (size_t)(p - start);
      if (len >= cap) len = cap - 1;
      memcpy(buf, start, len);
      buf[len] = 0;
      return buf;
    }
    n++;
    if (*p == ',') p++;
  }
  if (n == index && cap) {
    buf[0] = 0;
    return buf;
  }
  return NULL;
}

static void note_clock(const char *time, uint8_t *hh, uint8_t *mm) {
  if (!hh || !mm) return;
  if (!time || time[0] < '0' || time[1] < '0' || time[2] < '0' || time[3] < '0') return;
  int hour = (time[0] - '0') * 10 + (time[1] - '0');
  int minute = (time[2] - '0') * 10 + (time[3] - '0');
  if (hour > 23 || minute > 59) return;
  *hh = (uint8_t)hour;
  *mm = (uint8_t)minute;
}

bool nmea_parse_rmc(const char *line, gps_fix_t *out, uint8_t *hh, uint8_t *mm) {
  if (!line || !out) return false;
  if (!is_rmc(line) || !checksum_ok(line)) return false;
  char status[8];
  char tim[16];
  char lat[24];
  char ns[4];
  char lon[24];
  char ew[4];
  if (!field_at(line, 2, status, sizeof status)) return false;
  memset(out, 0, sizeof *out);
  if (status[0] == 'A' && field_at(line, 1, tim, sizeof tim)) note_clock(tim, hh, mm);
  if (status[0] != 'A') return true;
  if (!field_at(line, 3, lat, sizeof lat) || !field_at(line, 4, ns, sizeof ns) ||
      !field_at(line, 5, lon, sizeof lon) || !field_at(line, 6, ew, sizeof ew)) {
    return true;
  }
  int32_t lat_e4 = 0;
  int32_t lon_e4 = 0;
  if (!parse_coord(lat, &lat_e4) || !parse_coord(lon, &lon_e4)) return true;
  if (ns[0] == 'S') lat_e4 = -lat_e4;
  else if (ns[0] != 'N') return true;
  if (ew[0] == 'W') lon_e4 = -lon_e4;
  else if (ew[0] != 'E') return true;
  out->valid = true;
  out->lat_min_e4 = lat_e4;
  out->lon_min_e4 = lon_e4;
  return true;
}
