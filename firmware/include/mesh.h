#pragma once

#include "emu_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RADIO_RANGE_FT 100
#define MESH_MAX 16

typedef struct mesh mesh_t;

typedef struct {
  mesh_t *mesh;
  int index;
} mesh_tx_ctx_t;

struct mesh {
  emu_hal_t *ram[MESH_MAX];
  const int *east[MESH_MAX];
  const int *north[MESH_MAX]; /* NULL means 0, so a 1-D bench can pass only east. */
  int n;
  mesh_tx_ctx_t ctx[MESH_MAX];
};

int mesh_distance_ft(int pos_a, int pos_b);
/* false = out of range. */
bool mesh_rssi(int distance_ft, int8_t *out_rssi);

void mesh_clear(mesh_t *m);
void mesh_add(mesh_t *m, emu_hal_t *ram, const int *east, const int *north);
void mesh_link(mesh_t *m);
