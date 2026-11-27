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
  b.screen.sel = 5;
  b.screen.tz_edit = true;
  b.screen.light_edit = true;
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_SETTINGS, "settings opens");
  CHECK(b.screen.sel == 1, "name is selected");
  CHECK(!b.screen.tz_edit && !b.screen.light_edit, "opening settings drops a grabbed value");
  pass("settings release");
}

static void test_beep_test(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  beeper_init(&b);
  emu_hal_init(&ram, &hal);

  b.screen.mode = SCREEN_SETTINGS;
  b.screen.sel = 8;
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.beep_test, "press arms the beep");
  screen_render(&b.screen, &hal);
  CHECK(ram.beep_ms == 65535, "settings plays the tone");
  ram.beep_ms = 0;
  screen_render(&b.screen, &hal);
  CHECK(ram.beep_ms == 65535, "settings keeps the tone armed");

  emu_hal_push_back(&ram);
  screen_input(&b.screen, &hal);
  CHECK(!b.screen.beep_test, "back clears the beep");
  CHECK(ram.beep_ms == 0, "back stops the tone");
  pass("beep test");
}

static void test_alert_keeps_chirp(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  page_t page;
  beeper_init(&b);
  emu_hal_init(&ram, &hal);

  b.screen.mode = SCREEN_SETTINGS;
  b.screen.vibe_test = true;
  b.screen.beep_test = true;
  fill_page(&page);
  screen_on_page(&b.screen, &hal, &page);
  screen_render(&b.screen, &hal);
  CHECK(!b.screen.vibe_test && !b.screen.beep_test, "a page drops both tests");
  CHECK(ram.vibe_ms == 250 && ram.beep_ms == 80, "a page keeps its chirp");

  b.screen.mode = SCREEN_SETTINGS;
  b.screen.vibe_test = true;
  b.screen.beep_test = true;
  ram.vibe_ms = 0;
  ram.beep_ms = 0;
  b.locate.ask_n = 1;
  b.locate.ask_alert_ready = true;
  screen_poll_track(&b.screen, &hal);
  screen_render(&b.screen, &hal);
  CHECK(!b.screen.vibe_test && !b.screen.beep_test, "a locate prompt drops both tests");
  CHECK(ram.vibe_ms == 250 && ram.beep_ms == 80, "a locate prompt keeps its chirp");

  b.screen.mode = SCREEN_SETTINGS;
  b.screen.vibe_test = true;
  b.screen.beep_test = true;
  ram.vibe_ms = 250;
  ram.beep_ms = 250;
  b.message.status = MESSAGE_DELIVERED;
  screen_poll_send(&b.screen, &hal);
  screen_render(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_SENT, "a delivery leaves settings");
  CHECK(!b.screen.vibe_test && !b.screen.beep_test, "a delivery drops both tests");
  CHECK(ram.vibe_ms == 0 && ram.beep_ms == 0, "a delivery stops the tests");
  pass("alert keeps chirp");
}

static void test_vibe_pulse(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  beeper_init(&b);
  emu_hal_init(&ram, &hal);

  b.screen.mode = SCREEN_SETTINGS;
  b.screen.sel = 7;
  ram.now_ms = 1000;
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  screen_render(&b.screen, &hal);
  CHECK(b.screen.vibe_test, "press arms the vibe");
  CHECK(ram.vibe_ms == 250, "the pulse starts on");

  ram.now_ms = 1249;
  screen_render(&b.screen, &hal);
  CHECK(ram.vibe_ms == 1, "still on at the end of the quarter");

  ram.now_ms = 1250;
  screen_render(&b.screen, &hal);
  CHECK(ram.vibe_ms == 0, "off for the next quarter");

  ram.now_ms = 1500;
  screen_render(&b.screen, &hal);
  CHECK(ram.vibe_ms == 250, "on again after a half second");
  pass("vibe pulse");
}

static bool cols_eq(const uint8_t *a, int ax, const uint8_t *b, int bx, int n) {
  for (int x = 0; x < n; x++) {
    for (int page = 0; page < DISPLAY_PAGES; page++) {
      if (a[page * DISPLAY_WIDTH + ax + x] != b[page * DISPLAY_WIDTH + bx + x]) return false;
    }
  }
  return true;
}

static void test_slide(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  uint8_t menu[DISPLAY_BUF_LEN];
  uint8_t settings[DISPLAY_BUF_LEN];
  uint8_t mid[DISPLAY_BUF_LEN];
  uint8_t settled[DISPLAY_BUF_LEN];
  beeper_init(&b);
  emu_hal_init(&ram, &hal);

  ram.now_ms = 1000;
  screen_render(&b.screen, &hal);
  memcpy(menu, ram.display, DISPLAY_BUF_LEN);

  b.screen.sel = 5;
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_SETTINGS, "press opens settings");
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_SETTINGS && b.screen.sel == 1, "a press during the slide is dropped");

  screen_render(&b.screen, &hal);
  CHECK(memcmp(ram.display, menu, DISPLAY_BUF_LEN) == 0, "a slide starts on the old page");

  ram.now_ms = 1000 + SCREEN_SLIDE_MS / 2;
  screen_render(&b.screen, &hal);
  memcpy(mid, ram.display, DISPLAY_BUF_LEN);
  ram.now_ms = 1000 + SCREEN_SLIDE_MS;
  screen_render(&b.screen, &hal);
  memcpy(settings, ram.display, DISPLAY_BUF_LEN);
  CHECK(memcmp(settings, menu, DISPLAY_BUF_LEN) != 0, "settings is a different page");
  CHECK(cols_eq(mid, 0, menu, DISPLAY_WIDTH / 2, DISPLAY_WIDTH / 2), "the old page leaves to the left");
  CHECK(cols_eq(mid, DISPLAY_WIDTH / 2, settings, 0, DISPLAY_WIDTH / 2), "the new page comes in from the right");

  emu_hal_push_back(&ram);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_MENU, "back returns to the menu");
  screen_render(&b.screen, &hal);
  CHECK(memcmp(ram.display, settings, DISPLAY_BUF_LEN) == 0, "back starts on settings");
  ram.now_ms = 1000 + SCREEN_SLIDE_MS + SCREEN_SLIDE_MS / 2;
  screen_render(&b.screen, &hal);
  memcpy(mid, ram.display, DISPLAY_BUF_LEN);
  ram.now_ms = 1000 + SCREEN_SLIDE_MS + SCREEN_SLIDE_MS;
  screen_render(&b.screen, &hal);
  memcpy(settled, ram.display, DISPLAY_BUF_LEN);
  CHECK(cols_eq(mid, 0, settled, DISPLAY_WIDTH / 2, DISPLAY_WIDTH / 2), "back brings the menu in from the left");
  CHECK(cols_eq(mid, DISPLAY_WIDTH / 2, settings, 0, DISPLAY_WIDTH / 2), "back takes settings off to the right");

  ram.now_ms = 5000;
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  {
    page_t page;
    fill_page(&page);
    screen_on_page(&b.screen, &hal, &page);
  }
  screen_render(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_ALERT, "a page takes the screen");
  CHECK(b.screen.slide_dir == 0, "an alert cancels the slide");
  memcpy(settled, ram.display, DISPLAY_BUF_LEN);
  CHECK(memcmp(settled, menu, DISPLAY_BUF_LEN) != 0, "an alert is not the page it interrupted");
  screen_render(&b.screen, &hal);
  CHECK(memcmp(ram.display, settled, DISPLAY_BUF_LEN) == 0, "an alert does not slide");

  b.screen.mode = SCREEN_MENU;
  b.screen.sel = 0;
  b.screen.dark_mode = true;
  ram.now_ms = 8000;
  screen_render(&b.screen, &hal);
  memcpy(menu, ram.display, DISPLAY_BUF_LEN);
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  ram.now_ms = 8000 + SCREEN_SLIDE_MS / 2;
  screen_render(&b.screen, &hal);
  memcpy(mid, ram.display, DISPLAY_BUF_LEN);
  ram.now_ms = 8000 + SCREEN_SLIDE_MS;
  screen_render(&b.screen, &hal);
  memcpy(settled, ram.display, DISPLAY_BUF_LEN);
  CHECK(cols_eq(mid, 0, menu, DISPLAY_WIDTH / 2, DISPLAY_WIDTH / 2), "a dark slide keeps the old half");
  CHECK(cols_eq(mid, DISPLAY_WIDTH / 2, settled, 0, DISPLAY_WIDTH / 2), "a dark slide keeps the new half");

  b.screen.mode = SCREEN_MENU;
  b.screen.sel = 5;
  b.screen.dark_mode = false;
  b.screen.slide_dir = 0;
  ram.now_ms = 9000;
  screen_render(&b.screen, &hal);
  emu_hal_push_encoder(&ram, ENC_PRESS);
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_SETTINGS, "the first press opens settings");
  ram.now_ms = 9000 + SCREEN_SLIDE_MS;
  screen_render(&b.screen, &hal);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_SETTINGS, "a second press in the same turn is not left queued");

  b.screen.mode = SCREEN_PAIR;
  b.screen.slide_dir = 1;
  b.screen.slide_mode = SCREEN_PAIR;
  b.screen.slide_start = ram.now_ms;
  emu_hal_push_back(&ram);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_PEEPLE, "back during a slide aborts pairing");

  b.screen.mode = SCREEN_LOCATING;
  b.screen.slide_dir = 1;
  b.screen.slide_mode = SCREEN_LOCATING;
  b.screen.slide_start = ram.now_ms;
  emu_hal_push_back(&ram);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_LOCATE, "back during a slide aborts a locate");
  pass("slide");
}

static void test_stay_press(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  contact_t ada, bea;
  beeper_init(&b);
  emu_hal_init(&ram, &hal);
  memset(&ada, 0, sizeof ada);
  memset(&bea, 0, sizeof bea);
  ada.node_id[0] = 1;
  bea.node_id[0] = 2;
  str_copy_max(ada.name, sizeof ada.name, "Ada", PAGE_FROM_MAX);
  str_copy_max(bea.name, sizeof bea.name, "Bea", PAGE_FROM_MAX);
  contacts_upsert(&b.contacts, &ada);
  contacts_upsert(&b.contacts, &bea);

  b.screen.mode = SCREEN_COMPOSE;
  b.screen.sel = 0;
  ram.now_ms = 1000;
  emu_hal_push_encoder(&ram, ENC_PRESS);
  emu_hal_push_encoder(&ram, ENC_CW);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_CANNED && b.screen.sel == 0 && b.screen.slide_dir == 0,
        "a contact press opens canned without sliding or turning");

  emu_hal_push_back(&ram);
  emu_hal_push_encoder(&ram, ENC_CW);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.mode == SCREEN_COMPOSE && b.screen.slide_dir == 0 && b.screen.sel == 0,
        "back from canned does not slide or turn");

  b.screen.mode = SCREEN_BEEPLE;
  b.screen.beeple_page = 0;
  b.screen.beeple_focus = 0;
  b.screen.beeple_slide_dir = 0;
  screen_set_beeple(&b.screen, 0);
  emu_hal_push_encoder(&ram, ENC_PRESS);
  emu_hal_push_encoder(&ram, ENC_CW);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.beeple_focus == 1 && b.screen.beeple == 0,
        "a beeple press does not also change the mascot");
  pass("stay press");
}

static bool pixel(const uint8_t *buf, int x, int y) {
  return display_get_pixel(buf, x, y);
}

static void test_beeple_windows(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  uint8_t spooder[DISPLAY_BUF_LEN];
  uint8_t teef[DISPLAY_BUF_LEN];
  beeper_init(&b);
  emu_hal_init(&ram, &hal);
  ram.now_ms = 1000;

  b.screen.mode = SCREEN_BEEPLE;
  b.screen.beeple_page = 0;
  b.screen.beeple_focus = 1;
  screen_set_beeple(&b.screen, 10);
  screen_render(&b.screen, &hal);
  memcpy(spooder, ram.display, DISPLAY_BUF_LEN);

  screen_set_beeple(&b.screen, 11);
  screen_render(&b.screen, &hal);
  memcpy(teef, ram.display, DISPLAY_BUF_LEN);

  bool art_differs = false;
  for (int y = 16; y < DISPLAY_HEIGHT && !art_differs; y++) {
    for (int x = 64; x < DISPLAY_WIDTH; x++) {
      if (pixel(spooder, x, y) != pixel(teef, x, y)) art_differs = true;
    }
  }
  CHECK(art_differs, "spooder and teef are different pictures");

  screen_set_beeple(&b.screen, 10);
  emu_hal_push_encoder(&ram, ENC_CW);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.beeple == 11 && b.screen.beeple_slide_from == 10 && b.screen.beeple_slide_dir == 1,
        "a name turn slides from the previous beeple");

  ram.now_ms = 1000 + SCREEN_SLIDE_MS / 2;
  screen_render(&b.screen, &hal);
  const int half = 64;
  const int shift = half / 2;
  bool art_ok = true;
  bool flavor_ok = true;
  bool name_ok = true;
  for (int y = 16; y < DISPLAY_HEIGHT; y++) {
    for (int x = 0; x < half; x++) {
      int sx = x < half - shift ? 64 + x + shift : 64 + x - (half - shift);
      const uint8_t *src = x < half - shift ? spooder : teef;
      if (pixel(ram.display, 64 + x, y) != pixel(src, sx, y)) art_ok = false;
    }
  }
  for (int y = 32; y < DISPLAY_HEIGHT; y++) {
    for (int x = 0; x < half; x++) {
      int sx = x < half - shift ? x + shift : x - (half - shift);
      if (pixel(ram.display, x, y) != pixel(teef, sx, y)) flavor_ok = false;
    }
  }
  for (int y = 22; y < 29; y++) {
    for (int x = 0; x < half; x++) {
      if (pixel(ram.display, x, y) != pixel(teef, x, y)) name_ok = false;
    }
  }
  CHECK(art_ok, "the mascot slides in the right 64");
  CHECK(flavor_ok, "the flavor slides in the left 64");
  CHECK(name_ok, "the name stays with the beeple just chosen");
  pass("beeple windows");
}


static char g_px[64];

static bool left_row(const uint8_t *buf, int y, const char *row) {
  for (int x = 0; x < 64; x++) {
    if (pixel(buf, x, y) != (row[x] == '#')) {
      snprintf(g_px, sizeof g_px, "scene pixel %d,%d", x, y);
      return false;
    }
  }
  return true;
}

static bool left_rows(const uint8_t *buf, int y0, const char *const *rows, int n) {
  for (int i = 0; i < n; i++) {
    if (!left_row(buf, y0 + i, rows[i])) return false;
  }
  return true;
}

static void test_beeple_games(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  uint8_t mascot[DISPLAY_BUF_LEN];
  beeper_init(&b);
  emu_hal_init(&ram, &hal);
  ram.now_ms = 1000;

  b.screen.mode = SCREEN_BEEPLE;
  b.screen.beeple_page = 0;
  b.screen.beeple_focus = 0;
  b.screen.beeple_game = 0;
  screen_set_beeple(&b.screen, 0);

  emu_hal_push_encoder(&ram, ENC_CW);
  screen_input(&b.screen, &hal);
  ram.now_ms += SCREEN_SLIDE_MS;
  emu_hal_push_encoder(&ram, ENC_CW);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.beeple_page == 2 && b.screen.beeple_focus == 0 && b.screen.beeple_game == 0,
        "two turns from Select land on Games");

  ram.now_ms += SCREEN_SLIDE_MS;
  screen_render(&b.screen, &hal);
  static const char *fishing[] = {
      "..........................#.........#...........................",
      ".....................####.......#...............................",
      ".....................#....#..##.##..#.##...##...................",
      ".....................###..#.##..#.#.#.#.#.#.#...................",
      ".....................#....#...#.#.#.#.#.#.#.#...................",
      ".....................#....#.##..#.#.#.#.#..##...................",
      "..........................................##....................",
      "................................................................",
      "................................................................",
      "................................................................",
      "................................................................",
      "................................................................",
      "........................................#.......................",
      "........................................#.......................",
      "........................................#.......................",
      "........................................#.......................",
      "........................................#.......................",
      "........................................#.......................",
      "........................................#.......................",
      "........................................#.......................",
      ".......................................###......................",
      "......................................#.#.#.....................",
      ".......................................###......................",
      "........................................#.......................",
      "......................##########........#.......................",
      ".............##.....######.....#........#.......................",
      "............#..######..........###...#..#.......................",
      "............#....##.....###....#.#....##........................",
      "...........#..#.##.....#####..#..#..............................",
      "..........#....##.....####..#....#..............................",
      "..........#..#.#......####..#....#..............................",
      "..........#...##......#######...##..............................",
      "..........#..###.......#####....#...............................",
      "..........#...#.........###.....#...............................",
      ".........#..###.......#........##...............................",
      ".........#...#........##......##................................",
      ".........#..##..........#.....#.................................",
      "..........#..#...........#...##.................................",
      "..........#.##............####..................................",
      "..........####....####......#...................................",
      "...........###...#....#.....#...................................",
      "............#...#.....#....#....................................",
      "............#...#.....#....#....................................",
  };
  CHECK(left_rows(ram.display, 21, fishing, 43), g_px);

  memcpy(mascot, ram.display, DISPLAY_BUF_LEN);
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.beeple_focus == 1 && b.screen.beeple_game == 0, "a Games press highlights Fishing");

  emu_hal_push_encoder(&ram, ENC_CW);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.beeple_game == 1 && b.screen.beeple_slide_from == 0 && b.screen.beeple_slide_dir == 1,
        "a game turn slides from Fishing");
  ram.now_ms += SCREEN_SLIDE_MS / 2;
  screen_render(&b.screen, &hal);
  bool mascot_ok = true;
  for (int y = 0; y < DISPLAY_HEIGHT && mascot_ok; y++) {
    for (int x = 64; x < DISPLAY_WIDTH; x++) {
      if (pixel(ram.display, x, y) != pixel(mascot, x, y)) mascot_ok = false;
    }
  }
  CHECK(mascot_ok, "the mascot stays while the game scene slides");

  ram.now_ms += SCREEN_SLIDE_MS;
  screen_render(&b.screen, &hal);
  static const char *farming[] = {
      ".....................................#..........................",
      "...................####.........................................",
      "...................#.....##.##.####..#.##...##..................",
      "...................###..#.#.#..#.#.#.#.#.#.#.#..................",
      "...................#....#.#.#..#.#.#.#.#.#.#.#..................",
      "...................#.....##.#..#.#.#.#.#.#..##..................",
      "...........................................##...................",
      "................................................................",
      "................................................................",
      "................................................................",
      "................................................................",
      "................................................................",
      "................................................................",
      "................................................................",
      "....###.........................................................",
      "....#.##..........########......................................",
      "....#..##.......##........#......#..............................",
      "....#...##...#####.........#.....#..............................",
      "....#....####....#.........####..#..............................",
      "....#.............#.......#....#.#..............................",
      "....##.............#######.....####.............................",
      ".....####.........#.......#....#.#..............................",
      "........#########..##...##..###..#..............................",
      "................##############...#..............................",
      ".................#........#......#..............................",
      ".................#........#.....................................",
      ".........######################.................................",
      "........##.....................#................................",
      "........####...................#................................",
      "............######...........##.................................",
      "..................###########...................................",
      "................................................................",
      "................................................................",
      "..............................................#..####...........",
      ".............................................#.#.#..#...........",
      "..........................#.................#...##..#...........",
      "..#.......#.......#......#.#...............#.....#..#...........",
      "..##.....###.....##.....#...#.............#.......#.#...........",
      "..#.#...#.#.#...#.#......#.#.............#.........##...........",
      "..#..###..#..###..#.....#...#...........#...........#...........",
      "..#.......#.......#....#.....#.........###############..........",
      "..#.......#.......#.....#...#............#.........#............",
      "..#.......#.......#....#.....#...........#.........#............",
  };
  CHECK(left_rows(ram.display, 33, farming + 12, 31), g_px);

  emu_hal_push_back(&ram);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.beeple_focus == 0 && b.screen.beeple_slide_dir == 0 && b.screen.beeple_game == 1 &&
            b.screen.beeple_page == 2,
        "back returns to the Games title and keeps Farming");
  screen_render(&b.screen, &hal);
  CHECK(left_rows(ram.display, 21, farming, 43), g_px);

  emu_hal_push_encoder(&ram, ENC_CW);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.beeple_page == 0 && b.screen.beeple_game == 1, "Games wraps to Select and keeps the scene");
  pass("beeple games");
}

static void test_fishing(void) {
  beeper_t b;
  emu_hal_t ram;
  hal_t hal;
  uint8_t calm_buf[DISPLAY_BUF_LEN];
  uint8_t bite_buf[DISPLAY_BUF_LEN];
  uint32_t wait;
  int bar, letters, y, x;
  bool differ;
  beeper_init(&b);
  emu_hal_init(&ram, &hal);

  b.screen.mode = SCREEN_BEEPLE;
  b.screen.beeple_page = 2;
  b.screen.beeple_focus = 1;
  b.screen.beeple_game = 1;
  ram.now_ms = 10000;
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 0, "farming does not start a game");

  b.screen.beeple_game = 0;
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 1, "clicking Fishing starts the calm water");
  wait = b.screen.fish.next - ram.now_ms;
  CHECK(wait >= 3000 && wait <= 15000, "the bite waits between 3 and 15 seconds");

  ram.now_ms = b.screen.fish.next - 1;
  screen_poll_fish(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 1 && ram.vibe_ms == 0, "the bite has not started yet");

  ram.now_ms = b.screen.fish.next;
  screen_poll_fish(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 2 && ram.vibe_ms == 350, "the bite vibrates for 0.35 seconds");

  screen_render(&b.screen, &hal);
  memcpy(bite_buf, ram.display, DISPLAY_BUF_LEN);
  b.screen.fish.phase = 1;
  screen_render(&b.screen, &hal);
  memcpy(calm_buf, ram.display, DISPLAY_BUF_LEN);
  b.screen.fish.phase = 2;
  differ = false;
  for (y = 40; y < DISPLAY_HEIGHT && !differ; y++) {
    for (x = 0; x < DISPLAY_WIDTH; x++) {
      if (pixel(calm_buf, x, y) != pixel(bite_buf, x, y)) differ = true;
    }
  }
  CHECK(differ, "the bite waves are noisier than the calm rings");

  emu_hal_push_encoder(&ram, ENC_CW);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 2 && b.screen.beeple_page == 2, "the wheel does nothing during a bite");

  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 3 && ram.vibe_ms == 0, "a click during the bite catches");
  screen_render(&b.screen, &hal);
  bar = 0;
  letters = 0;
  for (x = 0; x < DISPLAY_WIDTH; x++) {
    if (pixel(ram.display, x, 26)) bar++;
    if (pixel(ram.display, x, 28)) letters++;
  }
  snprintf(g_px, sizeof g_px, "caught bar %d letters %d", bar, letters);
  CHECK(bar > 96 && letters < bar, g_px);

  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 1, "another click returns to calm");
  emu_hal_push_encoder(&ram, ENC_PRESS);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 1, "a click in calm water does not catch");
  wait = b.screen.fish.next - ram.now_ms;
  CHECK(wait >= 3000 && wait <= 15000, "the next bite waits between 3 and 15 seconds");

  ram.now_ms = b.screen.fish.next;
  screen_poll_fish(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 2, "the next bite starts");
  ram.now_ms = b.screen.fish.since + 350;
  screen_poll_fish(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 1, "a missed bite returns to calm");

  emu_hal_push_back(&ram);
  screen_input(&b.screen, &hal);
  CHECK(b.screen.fish.phase == 0 && b.screen.mode == SCREEN_BEEPLE && b.screen.beeple_page == 2 &&
            b.screen.beeple_focus == 1,
        "back returns to the games menu");
  pass("fishing");
}

int main(void) {
  test_backlight_timer();
  test_settings_release();
  test_beep_test();
  test_alert_keeps_chirp();
  test_vibe_pulse();
  test_slide();
  test_stay_press();
  test_beeple_windows();
  test_beeple_games();
  test_fishing();
  printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
