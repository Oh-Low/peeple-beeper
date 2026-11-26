#include "beeper.h"
#include "display.h"
#include "emu_hal.h"
#include "steps.h"

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

static accel_sample_t sample_at(int z, bool motor) {
  accel_sample_t s;
  memset(&s, 0, sizeof s);
  s.z = (int16_t)z;
  s.motor = motor;
  return s;
}

static void test_step_detect(void) {
  step_det_t d;
  accel_sample_t s;
  memset(&d, 0, sizeof d);

  s = sample_at(1000, false);
  CHECK(!step_detect(&d, &s, 0), "rest is not a step");
  s = sample_at(1100, false);
  CHECK(!step_detect(&d, &s, 40), "a bump under 1.2 g is not a step");

  s = sample_at(1500, false);
  CHECK(step_detect(&d, &s, 80), "a rise through 1.2 g is a step");
  CHECK(!step_detect(&d, &s, 120), "staying above 1.2 g is not another step");

  s = sample_at(900, false);
  CHECK(!step_detect(&d, &s, 160), "falling arms the counter");
  s = sample_at(1500, false);
  CHECK(!step_detect(&d, &s, 200), "a second peak inside 300 ms is one step");

  s = sample_at(900, false);
  CHECK(!step_detect(&d, &s, 400), "the counter arms again under 1.05 g");
  s = sample_at(1500, true);
  CHECK(!step_detect(&d, &s, 440), "the motor is not a step");
  s = sample_at(900, false);
  CHECK(!step_detect(&d, &s, 800), "after the motor it waits for a quiet sample");
  s = sample_at(1500, false);
  CHECK(step_detect(&d, &s, 840), "the next rise after the motor counts");
  pass("step detect");
}

static void test_steps_follow_beeple(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  beeper_init(&b);
  emu_hal_init(&ram, &hal);
  ram.now_ms = 1000;
  emu_hal_set_accel(&ram, 0, 0, 1000);
  beeper_tick(&b, &hal);
  CHECK_EQ(b.screen.steps[0], 0, "rest does not count");

  emu_hal_set_accel(&ram, 0, 0, 1500);
  ram.now_ms = 1040;
  beeper_tick(&b, &hal);
  CHECK_EQ(b.screen.steps[0], 1, "a step counts for the selected beeple");
  CHECK_EQ(b.screen.steps[4], 0, "another beeple does not take that step");

  emu_hal_set_accel(&ram, 0, 0, 900);
  ram.now_ms = 1080;
  beeper_tick(&b, &hal);
  screen_set_beeple(&b.screen, 4);
  emu_hal_set_accel(&ram, 0, 0, 1500);
  ram.now_ms = 1400;
  beeper_tick(&b, &hal);
  CHECK_EQ(b.screen.steps[0], 1, "the previous beeple keeps its count");
  CHECK_EQ(b.screen.steps[4], 1, "the new beeple gets the next step");

  emu_hal_set_accel(&ram, 0, 0, 1000);
  ram.now_ms = 5000;
  beeper_tick(&b, &hal);
  CHECK_EQ(b.screen.steps_saved[0], 0, "a step is not saved while still walking");
  CHECK_EQ(b.screen.steps_saved[4], 0, "the second beeple is not saved yet");

  ram.now_ms = 1400 + 30000;
  beeper_tick(&b, &hal);
  CHECK_EQ(b.screen.steps_saved[0], 1, "a pause saves the first beeple");
  CHECK_EQ(b.screen.steps_saved[4], 1, "a pause saves the beeple that was selected");
  pass("steps follow the selected beeple");
}

static bool ink_matches(const uint8_t *got, const uint8_t *want, const uint8_t *other) {
  for (int y = 29; y <= 35; y++) {
    for (int x = 14; x <= 32; x++) {
      bool w = display_get_pixel(want, x, y);
      bool o = display_get_pixel(other, x, y);
      bool g = display_get_pixel(got, x, y);
      if (w && !g) return false;
      if (!w && o && g) return false;
    }
  }
  return true;
}

static void paint(uint8_t *buf, const char *text) {
  display_clear(buf);
  display_draw_text_small(buf, 14, 29, text);
}

static void test_stats_show_steps(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  uint8_t twelve[DISPLAY_BUF_LEN];
  uint8_t seven[DISPLAY_BUF_LEN];
  uint8_t placeholder[DISPLAY_BUF_LEN];
  uint8_t tenk[DISPLAY_BUF_LEN];
  uint8_t raw[DISPLAY_BUF_LEN];
  beeper_init(&b);
  emu_hal_init(&ram, &hal);
  ram.now_ms = 1000;
  b.screen.mode = SCREEN_BEEPLE;
  b.screen.beeple_page = 1;
  b.screen.steps[0] = 12;
  b.screen.steps[3] = 7;
  paint(twelve, "12");
  paint(seven, "7");
  paint(placeholder, "6.5k");
  paint(tenk, "10.0k");
  paint(raw, "10000");

  screen_render(&b.screen, &hal);
  CHECK(ink_matches(ram.display, twelve, placeholder), "the top-left number is the step count");

  screen_set_beeple(&b.screen, 3);
  screen_render(&b.screen, &hal);
  CHECK(ink_matches(ram.display, seven, twelve), "the number is the selected beeple");

  b.screen.steps[3] = 10000;
  screen_render(&b.screen, &hal);
  CHECK(ink_matches(ram.display, tenk, raw), "10000 steps reads 10.0k");
  pass("stats show steps");
}

int main(void) {
  test_step_detect();
  test_steps_follow_beeple();
  test_stats_show_steps();
  printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
