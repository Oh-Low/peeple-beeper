#pragma once

#include "hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  FISH_OFF = 0,
  FISH_CALM = 1,
  FISH_BITE = 2,
  FISH_CAUGHT = 3,
} fish_phase_t;

typedef struct {
  fish_phase_t phase;
  uint32_t since;
  uint32_t next;
} fishing_t;

bool fishing_playing(const fishing_t *fish);
void fishing_start(fishing_t *fish, const hal_t *hal);
void fishing_stop(fishing_t *fish, const hal_t *hal);
void fishing_dismiss(fishing_t *fish);
void fishing_press(fishing_t *fish, const hal_t *hal);
void fishing_poll(fishing_t *fish, const hal_t *hal);
void fishing_expire(fishing_t *fish, const hal_t *hal);
void fishing_draw(const fishing_t *fish, const hal_t *hal, uint8_t *buf);
