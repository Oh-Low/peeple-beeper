#pragma once

#include "beeper.h"
#include "hal.h"

#define EMU_NAME_MAX 16

typedef struct {
  beeper_t *beeper;
  char name[EMU_NAME_MAX];
  const hal_t *hal;
} emu_unit_t;

void seed_emulator_directory(emu_unit_t *units, int n);
void introduce_emulator_unit(emu_unit_t *existing, int n, emu_unit_t *newbie);
void forget_emulator_unit(emu_unit_t *remaining, int n, emu_unit_t *gone);
