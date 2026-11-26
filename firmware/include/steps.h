#pragma once

#include "hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool started;
  bool armed;
  bool counted;
  uint32_t last_ms;
} step_det_t;

/* True when this sample is a new footstep. Peaks while motor is set are ignored. */
bool step_detect(step_det_t *d, const accel_sample_t *sample, uint32_t now_ms);
