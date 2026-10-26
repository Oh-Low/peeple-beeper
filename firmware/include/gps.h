#pragma once

#include "hal.h"

#include <stddef.h>
#include <stdint.h>

/* WGS84 lengths of one degree at the equator. 1 ft = 0.3048 m.
   Feet convert to decimal degrees, then to the module's 0.0001 arcminute. */
#define GPS_M_PER_DEG_LAT 110574.2725
#define GPS_M_PER_DEG_LON 111319.4908

void gps_from_feet(int east_ft, int north_ft, gps_fix_t *out);
void gps_delta_ft(int32_t lat_a, int32_t lon_a, int32_t lat_b, int32_t lon_b, int *north_ft,
                  int *east_ft);
int gps_hypot_ft(int north_ft, int east_ft);
/* Empty string when both axes are zero. */
const char *gps_compass(int north_ft, int east_ft);
void gps_format_lat(char *dst, size_t cap, int32_t lat_min_e4);
void gps_format_lon(char *dst, size_t cap, int32_t lon_min_e4);
