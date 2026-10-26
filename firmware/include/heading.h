#pragma once

#include "hal.h"

#include <stdbool.h>
#include <stdint.h>

/* Past this, the magnetometer is outside the 40° budget. */
#define HEADING_TILT_MAX_DEG 20
/* Flat and still for this long before the 8-way arrow is shown. */
#define HEADING_SETTLE_MS 400

typedef struct {
  bool hold_flat;
  bool hold_still;
  bool dont_move;
  bool ready;
  bool arrow;
  int octant;
} heading_view_t;

/* octant 0 is the top of the screen. Clockwise from there. arrow is false
   until the unit has been flat, still, and quiet for HEADING_SETTLE_MS.
   *holding / *since remember that wait across calls. */
void heading_view(const heading_sample_t *sample, int north_ft, int east_ft, uint32_t now_ms,
                  bool *holding, uint32_t *since, heading_view_t *view);
/* 0 is north. Clockwise in 45° steps: N, NE, E, SE, S, SW, W, NW. */
int heading_device_octant(int heading_deg);
