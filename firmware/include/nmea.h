#pragma once

#include "hal.h"

#include <stdbool.h>

/* Parse one PA1616S RMC sentence ($GPRMC or $GNRMC).
   Returns true when the line is an RMC sentence. A void fix sets valid to false. */
bool nmea_parse_rmc(const char *line, gps_fix_t *out);
