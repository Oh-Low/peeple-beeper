#include "beeper.h"
#include "emu_hal.h"
#include "util.h"

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

static void pass(const char *name) {
  g_pass++;
  printf("ok  %s\n", name);
}

static void fill_page(page_t *page) {
  memset(page, 0, sizeof *page);
  str_copy_max(page->from, sizeof page->from, "Bob", sizeof page->from - 1);
  str_copy_max(page->body, sizeof page->body, "hi", sizeof page->body - 1);
}

static void test_backlight_timer(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  page_t page;
  beeper_init(&b);
  emu_hal_init(&ram, &hal);

  b.screen.light_mode = LIGHT_30S;
  screen_light_apply(&b.screen, &hal);
  CHECK(ram.backlight_on, "30s starts lit");
  ram.now_ms = 29999;
  screen_render(&b.screen, &hal);
  CHECK(ram.backlight_on, "still lit inside 30s");
  ram.now_ms = 30000;
  screen_render(&b.screen, &hal);
  CHECK(!ram.backlight_on, "30s turns the light off");

  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(ram.backlight_on, "a press turns it back on");
  ram.now_ms += 30000;
  screen_render(&b.screen, &hal);
  CHECK(!ram.backlight_on, "the new window also ends");

  fill_page(&page);
  screen_on_page(&b.screen, &hal, &page);
  CHECK(ram.backlight_on, "a page turns it back on");

  b.screen.light_mode = LIGHT_OFF;
  screen_light_apply(&b.screen, &hal);
  CHECK(!ram.backlight_on, "off stays dark");
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(!ram.backlight_on, "a press leaves off dark");

  ram.backlight_on = true;
  b.screen.light_mode = LIGHT_OFF;
  screen_light_apply(&b.screen, &hal);
  CHECK(!ram.backlight_on, "saved off drives a lit lamp down");
  screen_render(&b.screen, &hal);
  CHECK(!ram.backlight_on, "a later frame leaves off dark");

  b.screen.light_mode = LIGHT_ON;
  screen_light_apply(&b.screen, &hal);
  ram.now_ms += 400000;
  screen_render(&b.screen, &hal);
  CHECK(ram.backlight_on, "on stays lit");
  pass("backlight timer");
}

static void grab(screen_t *screen) {
  screen->mode = SCREEN_SETTINGS;
  screen->tz_edit = true;
  screen->light_edit = true;
}

static void test_settings_release(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  page_t page;
  beeper_init(&b);
  emu_hal_init(&ram, &hal);

  grab(&b.screen);
  fill_page(&page);
  screen_on_page(&b.screen, &hal, &page);
  CHECK(!b.screen.tz_edit && !b.screen.light_edit, "a page drops a grabbed value");

  grab(&b.screen);
  b.locate.ask_n = 1;
  b.locate.ask_alert_ready = true;
  screen_poll_track(&b.screen, &hal);
  CHECK(!b.screen.tz_edit && !b.screen.light_edit, "a locate prompt drops a grabbed value");

  grab(&b.screen);
  b.locate.ask_n = 0;
  b.locate.ask_alert_ready = false;
  b.locate.tracked_note = true;
  screen_poll_track(&b.screen, &hal);
  CHECK(!b.screen.tz_edit && !b.screen.light_edit, "a tracked warning drops a grabbed value");

  b.screen.mode = SCREEN_MENU;
  b.screen.sel = 4;
  b.screen.tz_edit = true;
  b.screen.light_edit = true;
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_SETTINGS, "settings opens");
  CHECK(b.screen.sel == 1, "name is selected");
  CHECK(!b.screen.tz_edit && !b.screen.light_edit, "opening settings drops a grabbed value");
  pass("settings release");
}

int main(void) {
  test_backlight_timer();
  test_settings_release();
  printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
