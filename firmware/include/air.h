#pragma once

#include "hal_ram.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RADIO_RANGE_FT 100
#define AIR_MAX 16

typedef struct air_mesh air_mesh_t;

typedef struct {
  air_mesh_t *mesh;
  int index;
} air_tx_ctx_t;

struct air_mesh {
  ram_hal_t *ram[AIR_MAX];
  const int *east[AIR_MAX];
  const int *north[AIR_MAX]; /* NULL means 0, so a 1-D bench can pass only east. */
  int n;
  air_tx_ctx_t ctx[AIR_MAX];
};

int air_distance_ft(int pos_a, int pos_b);
/* false = out of range (do not push_raw). */
bool air_rssi(int distance_ft, int8_t *out_rssi);

void air_mesh_clear(air_mesh_t *m);
void air_mesh_add(air_mesh_t *m, ram_hal_t *ram, const int *east, const int *north);
void air_mesh_link(air_mesh_t *m);
