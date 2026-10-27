#pragma once

#include "hal.h"

#include <stdbool.h>

#define NMEA_LINE_MAX 96

typedef struct {
  char bytes[NMEA_LINE_MAX];
  int len;
  bool skip;
} nmea_line_t;

/* A void fix sets valid to false. A legal HHMM on status A is written to hh and mm. */
bool nmea_parse_rmc(const char *line, gps_fix_t *out, uint8_t *hh, uint8_t *mm);

bool nmea_push_byte(nmea_line_t *line, uint8_t byte, gps_fix_t *out, uint8_t *hh, uint8_t *mm);
