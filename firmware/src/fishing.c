#include "fishing.h"

#include "display.h"
#include "util.h"

#define FISH_BITE_MS 350
#define FISH_WAIT_MIN 3000u
#define FISH_WAIT_SPAN 12001u

bool fishing_playing(const fishing_t *fish) {
  return fish && fish->phase != FISH_OFF;
}

static uint32_t fish_wait_ms(const hal_t *hal) {
  uint8_t b[2];
  unsigned n;
  hal->random_fill(hal->ctx, b, 2);
  n = (unsigned)b[0] | ((unsigned)b[1] << 8);
  return FISH_WAIT_MIN + (n % FISH_WAIT_SPAN);
}

void fishing_start(fishing_t *fish, const hal_t *hal) {
  uint32_t now = hal->now_ms(hal->ctx);
  fish->phase = FISH_CALM;
  fish->since = now;
  fish->next = now + fish_wait_ms(hal);
}

void fishing_stop(fishing_t *fish, const hal_t *hal) {
  if (!fish) return;
  fish->phase = FISH_OFF;
  if (hal) hal->vibe(hal->ctx, 0);
}

void fishing_dismiss(fishing_t *fish) {
  if (!fish) return;
  fish->phase = FISH_OFF;
}

void fishing_press(fishing_t *fish, const hal_t *hal) {
  if (!fish || fish->phase == FISH_OFF) return;
  if (fish->phase == FISH_BITE) {
    fish->phase = FISH_CAUGHT;
    hal->vibe(hal->ctx, 0);
    return;
  }
  if (fish->phase == FISH_CAUGHT) fishing_start(fish, hal);
}

void fishing_poll(fishing_t *fish, const hal_t *hal) {
  uint32_t now;
  uint32_t late;
  if (!fish || !hal || fish->phase == FISH_OFF) return;
  now = hal->now_ms(hal->ctx);
  if (fish->phase == FISH_CALM) {
    if (!time_reached(now, fish->next)) return;
    late = elapsed_u32(now, fish->next);
    if (late >= FISH_BITE_MS) {
      fishing_start(fish, hal);
      return;
    }
    fish->phase = FISH_BITE;
    fish->since = fish->next;
    hal->vibe(hal->ctx, (uint16_t)(FISH_BITE_MS - late));
  }
}

void fishing_expire(fishing_t *fish, const hal_t *hal) {
  if (!fish || !hal || fish->phase != FISH_BITE) return;
  if (elapsed_u32(hal->now_ms(hal->ctx), fish->since) < FISH_BITE_MS) return;
  hal->vibe(hal->ctx, 0);
  fishing_start(fish, hal);
}

static int fish_bob(uint32_t now) {
  static const int8_t wave[16] = {0, 1, 2, 2, 2, 1, 0, -1, -2, -2, -2, -1, 0, 1, 1, 0};
  return wave[(now / 180u) & 15u];
}

static int fish_jag(int chunk, int salt) {
  uint32_t h = (uint32_t)chunk * 1103515245u + (uint32_t)salt * 12345u;
  return (int)((h >> 16) % 5u) - 2;
}

static bool fish_gap(int chunk, int salt) {
  uint32_t h = (uint32_t)chunk * 2246822519u + (uint32_t)salt * 12345u;
  return ((h >> 16) & 3u) == 0;
}

static bool fish_thick(int i, int salt) {
  int chunk = (i + salt * 3) / 6;
  uint32_t h = (uint32_t)chunk * 1664525u + (uint32_t)salt * 1013904223u;
  return ((h >> 16) & 1u) == 0;
}

/* Unit superellipse, n = 3. */
static const int8_t FISH_WAVE[] = {
    120, -70, 121, -66, 122, -62, 123, -58, 124, -54, 124, -49, 125, -45, 126, -40,
    126, -34, 127, -28, 127, -22, 127, -14, 127, 0, 127, 14, 127, 22, 127, 28,
    126, 34, 126, 40, 125, 45, 124, 49, 124, 54, 123, 58, 122, 62, 121, 66,
    120, 70, 118, 73, 117, 77, 115, 80, 114, 83, 112, 86, 110, 89, 108, 92,
    106, 95, 104, 97, 102, 100, 100, 102, 97, 104, 95, 106, 92, 108, 89, 110,
    86, 112, 83, 114, 80, 115, 77, 117, 73, 118, 70, 120, 66, 121, 62, 122,
    58, 123, 54, 124, 49, 124, 45, 125, 40, 126, 34, 126, 28, 127, 22, 127,
    14, 127, 0, 127, -14, 127, -22, 127, -28, 127, -34, 126, -40, 126, -45, 125,
    -49, 124, -54, 124, -58, 123, -62, 122, -66, 121, -70, 120, -73, 118, -77, 117,
    -80, 115, -83, 114, -86, 112, -89, 110, -92, 108, -95, 106, -97, 104, -100, 102,
    -102, 100, -104, 97, -106, 95, -108, 92, -110, 89, -112, 86, -114, 83, -115, 80,
    -117, 77, -118, 73, -120, 70, -121, 66, -122, 62, -123, 58, -124, 54, -124, 49,
    -125, 45, -126, 40, -126, 34, -127, 28, -127, 22, -127, 14, -127, 0, -127, -14,
    -127, -22, -127, -28, -126, -34, -126, -40, -125, -45, -124, -49, -124, -54, -123, -58,
    -122, -62, -121, -66, -120, -70,
};
#define FISH_WAVE_N 115

static bool fish_open(int i, int salt) {
  int a = -24 + 2 * i - (salt & 3) * 26;
  if (a >= 22 && a <= 58) return true;
  if (a >= 84 && a <= 96) return true;
  if (a >= 122 && a <= 158) return true;
  return false;
}

static void stroke(uint8_t *buf, int *prev, int *px, int *py, int x, int y) {
  if (*prev) display_draw_line(buf, *px, *py, x, y);
  else display_set_pixel(buf, x, y, true);
  *px = x;
  *py = y;
  *prev = 1;
}

static void draw_fish_ring(uint8_t *buf, int cx, int cy, int rx, int ry, bool noisy, int salt) {
  int prev = 0;
  int px = 0;
  int py = 0;
  int iprev = 0;
  int ix = 0;
  int iy = 0;
  for (int i = 0; i < FISH_WAVE_N; i++) {
    int jx = 0;
    int jy = 0;
    int chunk = i / 5;
    if (fish_open(i, salt) || (noisy && fish_gap(chunk, salt))) {
      prev = 0;
      iprev = 0;
      continue;
    }
    if (noisy) {
      jx = fish_jag(chunk, salt);
      jy = fish_jag(chunk, salt + 1);
    }
    int x = cx + (int)((int32_t)FISH_WAVE[i * 2] * rx / 127) + jx;
    int y = cy + (int)((int32_t)FISH_WAVE[i * 2 + 1] * ry / 127) + jy;
    stroke(buf, &prev, &px, &py, x, y);
    if (!fish_thick(i, salt)) {
      iprev = 0;
      continue;
    }
    x = cx + (int)((int32_t)FISH_WAVE[i * 2] * (rx - 1) / 127) + jx;
    y = cy + (int)((int32_t)FISH_WAVE[i * 2 + 1] * (ry - 1) / 127) + jy;
    stroke(buf, &iprev, &ix, &iy, x, y);
  }
}

static void draw_fish_waves(uint8_t *buf, int cx, int cy, uint32_t now, bool noisy) {
  int age = (int)((now / 90u) % 48u);
  for (int k = 0; k < 4; k++) {
    int rx = 20 + (age + k * 12) % 48;
    int ry = rx * 5 / 12;
    draw_fish_ring(buf, cx, cy, rx, ry, noisy, k);
  }
}

static void draw_fish_bobber(uint8_t *buf, int x, int y) {
  static const char *rows[] = {
      "..........##########..................",
      ".................####.................",
      "................#.##.#................",
      "...............#......#...............",
      "...............#......#...............",
      ".............############.............",
      "..........##################..........",
      ".........####################.........",
      ".......########################.......",
      "......#######.##################......",
      ".....######...###################.....",
      "....######...#####################....",
      "...######...#######################...",
      "...#####...########################...",
      "..######.###########################..",
      ".####################################.",
      "######################################",
      "######################################",
      "######################################",
      "######################################",
      "######################################",
      "######################################",
      "##################################.###",
      ".###############################..###.",
      "#.###########################....###.#",
      "#.#######################.....######.#",
      "#..###############################...#",
      "#.....#########################......#",
      "#..........###############...........#",
  };
  display_draw_rows(buf, x, y, rows, 29);
}

static void draw_fish_line(uint8_t *buf, int bob) {
  int end_y = 8 + bob;
  for (int t = 0; t <= 24; t++) {
    int o = 24 - t;
    int den = 24 * 24;
    int x = (o * o * 42 + 2 * o * t * 48 + t * t * 58) / den;
    int y = (2 * o * t * 5 + t * t * end_y) / den;
    display_set_pixel(buf, x, y, true);
    if (t > 10) display_set_pixel(buf, x + 1, y, true);
  }
}

static void draw_fish_caught(uint8_t *buf) {
  const char *msg = "FISH CAUGHT";
  int w = display_text_width(msg);
  int x = (DISPLAY_WIDTH - w) / 2;
  int y = 28;
  int y0 = y - 2;
  int h = CHAR_H + 4;
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < DISPLAY_WIDTH; col++) display_set_pixel(buf, col, y0 + row, false);
  }
  display_draw_text(buf, x, y, msg);
  display_invert_rect(buf, 0, y0, DISPLAY_WIDTH, h);
}

void fishing_draw(const fishing_t *fish, const hal_t *hal, uint8_t *buf) {
  uint32_t now = hal->now_ms(hal->ctx);
  int bob = fish_bob(now);
  bool noisy = fish->phase == FISH_BITE;
  draw_fish_waves(buf, 64, 38, now, noisy);
  draw_fish_bobber(buf, 45, 8 + bob);
  draw_fish_line(buf, bob);
  if (fish->phase == FISH_CAUGHT) draw_fish_caught(buf);
}
