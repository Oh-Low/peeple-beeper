#include "steps.h"

#include "util.h"

/* Milli-g. A step is a rise through 1.2 g. It can fire again after a fall under
   1.05 g, and not sooner than 300 ms. */
#define STEP_HIGH 1200
#define STEP_LOW 1050
#define STEP_GAP_MS 300

static int64_t mag2(const accel_sample_t *s) {
  int64_t x = s->x;
  int64_t y = s->y;
  int64_t z = s->z;
  return x * x + y * y + z * z;
}

bool step_detect(step_det_t *d, const accel_sample_t *sample, uint32_t now_ms) {
  int64_t m2;
  if (!d || !sample) return false;
  if (!d->started) {
    d->started = true;
    d->armed = true;
  }
  if (sample->motor) {
    d->armed = false;
    return false;
  }
  m2 = mag2(sample);
  if (!d->armed) {
    if (m2 < (int64_t)STEP_LOW * STEP_LOW) d->armed = true;
    return false;
  }
  if (m2 < (int64_t)STEP_HIGH * STEP_HIGH) return false;
  d->armed = false;
  if (d->counted && elapsed_u32(now_ms, d->last_ms) < STEP_GAP_MS) return false;
  d->counted = true;
  d->last_ms = now_ms;
  return true;
}
