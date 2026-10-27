#include "heading.h"

#include <stdio.h>

#define TX_MAX 32

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

static heading_view_t pose_at(const heading_sample_t *sample, int north, int east, uint32_t now,
                             bool *holding, uint32_t *since) {
  heading_view_t view;
  heading_view(sample, north, east, now, holding, since, &view);
  return view;
}

static void test_heading_pose(void) {
  heading_sample_t s = {.tilt_deg = 25, .still = true, .motor = false, .heading_deg = 0};
  bool holding = false;
  uint32_t since = 0;
  heading_view_t v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.hold_flat && !v.arrow && !v.dont_move, "tipped asks to hold flat");

  s.tilt_deg = 0;
  s.still = false;
  v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.hold_still && !v.hold_flat && !v.arrow, "moving asks to hold still");

  s.still = true;
  s.motor = true;
  v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.hold_still && !v.arrow, "motor hides the heading");

  s.motor = false;
  s.tilt_deg = 25;
  s.still = false;
  v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.hold_flat && v.hold_still && !v.arrow, "tipped and moving shows both");

  s.tilt_deg = 0;
  s.still = true;
  holding = false;
  v = pose_at(&s, 0, 75, 1000, &holding, &since);
  CHECK(v.dont_move && !v.arrow && holding, "flat and still waits for a clean reading");
  v = pose_at(&s, 0, 75, 1000 + HEADING_SETTLE_MS, &holding, &since);
  CHECK(v.arrow && v.octant == 2, "east is octant 2 when the top points north");

  s.heading_deg = 90;
  holding = false;
  v = pose_at(&s, 0, 75, 5000, &holding, &since);
  v = pose_at(&s, 0, 75, 5000 + HEADING_SETTLE_MS, &holding, &since);
  CHECK(v.arrow && v.octant == 0, "facing east, an eastern contact is straight ahead");
  pass("heading waits for flat and still, then an 8-way arrow");
}

int main(void) {
  test_heading_pose();
  if (g_fail) {
    printf("%d failed, %d passed\n", g_fail, g_pass);
    return 1;
  }
  printf("%d passed\n", g_pass);
  return 0;
}
