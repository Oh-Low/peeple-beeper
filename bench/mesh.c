#include "mesh.h"
#include "gps.h"

#include <string.h>

#define RSSI_AT_ZERO (-30)
#define RSSI_DROP_OVER_RANGE 60

int mesh_distance_ft(int pos_a, int pos_b) {
  int d = pos_a - pos_b;
  return d < 0 ? -d : d;
}

bool mesh_rssi(int distance_ft, int8_t *out_rssi) {
  if (distance_ft < 0 || distance_ft >= RADIO_RANGE_FT) return false;
  /* Round half away from zero. */
  double x = (double)RSSI_AT_ZERO -
             ((double)distance_ft / (double)RADIO_RANGE_FT) * (double)RSSI_DROP_OVER_RANGE;
  int r = (int)(x >= 0 ? x + 0.5 : x - 0.5);
  if (out_rssi) *out_rssi = (int8_t)r;
  return true;
}

static int coord(const int *p) { return p ? *p : 0; }

static int span_ft(const mesh_t *m, int a, int b) {
  return gps_hypot_ft(coord(m->north[a]) - coord(m->north[b]), coord(m->east[a]) - coord(m->east[b]));
}

static void on_tx(void *user, const uint8_t *bytes, size_t len) {
  mesh_tx_ctx_t *ctx = user;
  mesh_t *m = ctx->mesh;
  int from = ctx->index;
  for (int i = 0; i < m->n; i++) {
    if (i == from) continue;
    int8_t rssi;
    if (!mesh_rssi(span_ft(m, from, i), &rssi)) continue;
    emu_hal_push_raw(m->ram[i], bytes, len, rssi);
  }
}

void mesh_clear(mesh_t *m) {
  if (!m) return;
  for (int i = 0; i < m->n; i++) {
    if (m->ram[i]) {
      m->ram[i]->on_tx_raw = NULL;
      m->ram[i]->on_tx_user = NULL;
    }
  }
  memset(m, 0, sizeof *m);
}

void mesh_add(mesh_t *m, emu_hal_t *ram, const int *east, const int *north) {
  if (!m || m->n >= MESH_MAX) return;
  m->ram[m->n] = ram;
  m->east[m->n] = east;
  m->north[m->n] = north;
  m->n++;
}

void mesh_link(mesh_t *m) {
  for (int i = 0; i < m->n; i++) {
    if (!m->ram[i]) continue;
    m->ctx[i].mesh = m;
    m->ctx[i].index = i;
    m->ram[i]->on_tx_raw = on_tx;
    m->ram[i]->on_tx_user = &m->ctx[i];
  }
}
