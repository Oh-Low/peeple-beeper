#include "heading.h"

#include "gps.h"
#include "util.h"

static int device_octant(int heading_deg) {
  int d = heading_deg % 360;
  if (d < 0) d += 360;
  return ((d + 22) / 45) % 8;
}

int heading_device_octant(int heading_deg) { return device_octant(heading_deg); }

void heading_view(const heading_sample_t *sample, int north_ft, int east_ft, uint32_t now_ms,
                  bool *holding, uint32_t *since, heading_view_t *view) {
  if (!view) return;
  view->hold_flat = false;
  view->hold_still = false;
  view->dont_move = false;
  view->ready = false;
  view->arrow = false;
  view->octant = 0;
  if (!sample || !holding || !since) return;

  bool flat = sample->tilt_deg <= HEADING_TILT_MAX_DEG;
  bool quiet = sample->still && !sample->motor;
  if (!flat) view->hold_flat = true;
  if (!quiet) view->hold_still = true;
  if (!flat || !quiet) {
    *holding = false;
    return;
  }
  if (!*holding) {
    *holding = true;
    *since = now_ms;
  }
  if (elapsed_u32(now_ms, *since) < HEADING_SETTLE_MS) {
    view->dont_move = true;
    return;
  }
  view->ready = true;
  int target = gps_octant(north_ft, east_ft);
  if (target < 0) return;
  view->arrow = true;
  view->octant = (target - device_octant(sample->heading_deg) + 8) % 8;
}
