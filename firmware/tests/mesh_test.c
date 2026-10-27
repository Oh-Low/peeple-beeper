#include "mesh.h"
#include "emu_hal.h"

#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;

#define CHECK(cond, msg)           \
  do {                             \
    if (!(cond)) {                 \
      printf("not ok  %s\n", msg); \
      g_fail++;                    \
      return;                      \
    }                              \
  } while (0)

#define CHECK_EQ(a, b, msg)                                                    \
  do {                                                                         \
    int _a = (int)(a), _b = (int)(b);                                          \
    if (_a != _b) {                                                            \
      printf("not ok  %s: %d !== %d\n", msg, _a, _b);                          \
      g_fail++;                                                                \
      return;                                                                  \
    }                                                                          \
  } while (0)

static void pass(const char *name) {
  g_pass++;
  printf("ok  %s\n", name);
}

static bool poll_raw_hal(hal_t *hal, uint8_t *out, uint8_t *len) {
  raw_frame_t f;
  memset(&f, 0, sizeof f);
  if (!hal->radio_poll_raw(hal->ctx, &f)) return false;
  if (out && f.len) memcpy(out, f.bytes, f.len);
  if (len) *len = f.len;
  return true;
}

static void test_falloff(void) {
  int8_t rssi = 0;
  CHECK(mesh_rssi(0, &rssi) && rssi == -30, "at origin");
  CHECK(mesh_rssi(75, &rssi), "75ft still on air");
  CHECK(!mesh_rssi(RADIO_RANGE_FT, &rssi), "100ft silent");
  CHECK(!mesh_rssi(150, &rssi), "150ft silent");
  CHECK_EQ(mesh_distance_ft(0, 150), 150, "1 and 3 span");
  CHECK_EQ(mesh_distance_ft(0, 75), 75, "1 and 2");
  CHECK_EQ(mesh_distance_ft(75, 150), 75, "2 and 3");
  pass("falloff: linked under 100ft, silent at 100+");
}

static void test_mesh_hear(void) {
  emu_hal_t ra, rb, rc;
  hal_t ha, hb, hc;
  emu_hal_init(&ra, &ha);
  emu_hal_init(&rb, &hb);
  emu_hal_init(&rc, &hc);
  int fa = 0, fb = 75, fc = 150;
  mesh_t mesh;
  memset(&mesh, 0, sizeof mesh);
  mesh_add(&mesh, &ra, &fa, NULL);
  mesh_add(&mesh, &rb, &fb, NULL);
  mesh_add(&mesh, &rc, &fc, NULL);
  mesh_link(&mesh);

  uint8_t ping[3] = {1, 2, 3};
  ha.radio_send_raw(ha.ctx, ping, sizeof ping);
  uint8_t buf[16], len = 0;
  CHECK(poll_raw_hal(&hb, buf, &len), "B hears A");
  CHECK(!poll_raw_hal(&hc, buf, &len), "C does not hear A");

  hc.radio_send_raw(hc.ctx, ping, sizeof ping);
  CHECK(poll_raw_hal(&hb, buf, &len), "B hears C");
  CHECK(!poll_raw_hal(&ha, buf, &len), "A does not hear C");
  pass("mesh: 0 and 150 do not hear; 75 does");
}

int main(void) {
  test_falloff();
  test_mesh_hear();
  printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
