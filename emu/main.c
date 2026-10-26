/* Desktop bench for the Peeple firmware. One SDL2 window, the RAM HAL, and fake air.
   Saved contacts go in the per-user app support directory, not the working directory. */

#include "air.h"
#include "beeper.h"
#include "font.h"
#include "gps.h"
#include "hal_ram.h"
#include "locate.h"
#include "seed.h"
#include "util.h"

#include <SDL.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_RADIOS 16
#define SCALE 3
#define COL_W (DISPLAY_WIDTH * SCALE + 24)
#define LCD_H (DISPLAY_HEIGHT * SCALE)
#define TOP_BAR 36
#define GRID_STEP 5
#define GRID_STEP_FAST 25
#define HEAD_STEP 15
#define HEAD_STEP_FAST 45

/* Compact panel: a compass, one status line, then two button rows. */
#define PAD 6
#define DIR 28
#define DIR_GAP 4
#define LINE 16
#define BTN_H 22
#define STEP_W 28
#define HEAD_VALUE_W 72
#define COMPASS_H (DIR * 3 + DIR_GAP * 2)
#define CHROME_H (PAD + COMPASS_H + 6 + LINE + LINE + 6 + BTN_H + 4 + BTN_H + PAD)

enum { DIR_N = 0, DIR_E, DIR_S, DIR_W };

typedef struct {
  int x, y, w, h;
} rect_i;

typedef struct {
  ram_hal_t ram;
  hal_t hal;
  beeper_t beeper;
  int east;
  int north;
  int tilt_deg;
  bool still;
  bool sky;
  int heading_deg;
  int id;
  char name[EMU_NAME_MAX];
  uint32_t nvm_hash;
  size_t nvm_saved_len;
  bool nvm_valid;
} radio_t;

static radio_t g_radios[MAX_RADIOS];
static int g_n;
static int g_focus;
static air_mesh_t g_mesh;
static uint32_t g_t0;
static int g_next_id = 1;
static char g_nvm_dir[512];
static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static SDL_Texture *g_lcd;
static float g_dpi;
static bool g_dragging;
static bool g_lcd_click;
static float g_drag_acc;

static uint32_t emu_now_ms(void) { return SDL_GetTicks() - g_t0; }

static uint32_t hash_bytes(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h ? h : 1u;
}

static void nvm_path(int id, char *out, size_t cap) {
  snprintf(out, cap, "%semu-nvm-%d.bin", g_nvm_dir, id);
}

static void init_nvm_dir(void) {
  char *pref = SDL_GetPrefPath("Peeple", "Beeper");
  if (pref) {
    snprintf(g_nvm_dir, sizeof g_nvm_dir, "%s", pref);
    SDL_free(pref);
    return;
  }
  snprintf(g_nvm_dir, sizeof g_nvm_dir, "./");
}

static void nvm_load_file(radio_t *r) {
  char path[768];
  nvm_path(r->id, path, sizeof path);
  bool legacy = false;
  FILE *f = fopen(path, "rb");
  if (!f) {
    char old[64];
    snprintf(old, sizeof old, "emu-nvm-%d.bin", r->id);
    f = fopen(old, "rb");
    legacy = f != NULL;
  }
  if (!f) return;
  r->ram.nvm_len = fread(r->ram.nvm, 1, NVM_BYTES, f);
  fclose(f);
  if (legacy) return;
  r->nvm_hash = hash_bytes(r->ram.nvm, r->ram.nvm_len);
  r->nvm_saved_len = r->ram.nvm_len;
  r->nvm_valid = r->ram.nvm_len > 0;
}

static void nvm_save_file(radio_t *r) {
  char path[768];
  nvm_path(r->id, path, sizeof path);
  if (r->ram.nvm_len == 0) {
    if (r->nvm_valid) remove(path);
    r->nvm_valid = false;
    r->nvm_saved_len = 0;
    return;
  }
  uint32_t h = hash_bytes(r->ram.nvm, r->ram.nvm_len);
  if (r->nvm_valid && h == r->nvm_hash && r->nvm_saved_len == r->ram.nvm_len) return;
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fwrite(r->ram.nvm, 1, r->ram.nvm_len, f);
  fclose(f);
  r->nvm_hash = h;
  r->nvm_saved_len = r->ram.nvm_len;
  r->nvm_valid = true;
}

static void relink(void) {
  air_mesh_clear(&g_mesh);
  for (int i = 0; i < g_n; i++)
    air_mesh_add(&g_mesh, &g_radios[i].ram, &g_radios[i].east, &g_radios[i].north);
  air_mesh_link(&g_mesh);
}

static void as_units(emu_unit_t *out) {
  for (int i = 0; i < g_n; i++) {
    out[i].beeper = &g_radios[i].beeper;
    str_copy_max(out[i].name, sizeof out[i].name, g_radios[i].name, EMU_NAME_MAX - 1);
    out[i].hal = &g_radios[i].hal;
  }
}

static void apply_gps(radio_t *r) {
  if (!r->sky) {
    ram_hal_set_gps(&r->ram, NULL);
    return;
  }
  gps_fix_t fix;
  gps_from_feet(r->east, r->north, &fix);
  ram_hal_set_gps(&r->ram, &fix);
}

static void apply_heading(radio_t *r) {
  ram_hal_set_heading(&r->ram, r->tilt_deg, r->still, r->heading_deg);
}

static void add_radio(const char *name, int east, int north) {
  if (g_n >= MAX_RADIOS) return;
  radio_t *r = &g_radios[g_n];
  memset(r, 0, sizeof *r);
  ram_hal_init(&r->ram, &r->hal);
  r->id = g_next_id++;
  r->ram.rng = (uint32_t)r->id * 2654435761u + 1;
  r->east = east;
  r->north = north;
  r->still = true;
  r->sky = true;
  apply_gps(r);
  apply_heading(r);
  str_copy_max(r->name, sizeof r->name, name, EMU_NAME_MAX - 1);
  nvm_load_file(r);
  beeper_init(&r->beeper);
  g_n++;
  relink();
  emu_unit_t units[MAX_RADIOS];
  as_units(units);
  if (g_n == 1) {
    seed_emulator_directory(units, 1);
  } else {
    introduce_emulator_unit(units, g_n - 1, &units[g_n - 1]);
  }
}

static void remove_last(void) {
  if (g_n <= 1) return;
  emu_unit_t units[MAX_RADIOS];
  as_units(units);
  forget_emulator_unit(units, g_n - 1, &units[g_n - 1]);
  g_n--;
  if (g_focus >= g_n) g_focus = g_n - 1;
  relink();
}

static void local_hm(uint8_t *hh, uint8_t *mm) {
  time_t t = time(NULL);
  struct tm tm_buf;
#if defined(_WIN32)
  localtime_s(&tm_buf, &t);
#else
  localtime_r(&t, &tm_buf);
#endif
  *hh = (uint8_t)tm_buf.tm_hour;
  *mm = (uint8_t)tm_buf.tm_min;
}

static void tick_all(void) {
  uint32_t now = emu_now_ms();
  uint8_t hh = 0, mm = 0;
  local_hm(&hh, &mm);
  for (int i = 0; i < g_n; i++) {
    g_radios[i].ram.now_ms = now;
    g_radios[i].ram.hh = hh;
    g_radios[i].ram.mm = mm;
    beeper_tick(&g_radios[i].beeper, &g_radios[i].hal);
    nvm_save_file(&g_radios[i]);
  }
}

static rect_i lcd_rect(int i) {
  rect_i r = {12 + i * COL_W, TOP_BAR, DISPLAY_WIDTH * SCALE, LCD_H};
  return r;
}

static int chrome_y(rect_i lcd) { return lcd.y + LCD_H + PAD; }

static int compass_x(rect_i lcd) { return lcd.x + (lcd.w - (DIR * 2 + DIR_GAP)) / 2; }

static rect_i dir_btn(int i, int dir) {
  rect_i lcd = lcd_rect(i);
  int x0 = compass_x(lcd);
  int y0 = chrome_y(lcd);
  int mid = y0 + DIR + DIR_GAP;
  int south = mid + DIR + DIR_GAP;
  int nx = x0 + (DIR + DIR_GAP) / 2;
  int ex = x0 + DIR + DIR_GAP;
  rect_i r = {nx, y0, DIR, DIR};
  if (dir == DIR_E) {
    r.x = ex;
    r.y = mid;
  } else if (dir == DIR_S) {
    r.y = south;
  } else if (dir == DIR_W) {
    r.x = x0;
    r.y = mid;
  }
  return r;
}

static int status_y(rect_i lcd) { return chrome_y(lcd) + COMPASS_H + 6; }

static int toggle_y(rect_i lcd) { return status_y(lcd) + LINE + LINE + 6; }

static rect_i toggle_btn(int i, int which) {
  rect_i lcd = lcd_rect(i);
  int gap = 6;
  int w = (lcd.w - 3 * gap) / 4;
  rect_i r = {lcd.x + which * (w + gap), toggle_y(lcd), w, BTN_H};
  return r;
}

static rect_i head_btn(int i, int side) {
  rect_i lcd = lcd_rect(i);
  int cluster = STEP_W + HEAD_VALUE_W + STEP_W;
  int x0 = lcd.x + (lcd.w - cluster) / 2;
  rect_i r = {x0 + (side ? STEP_W + HEAD_VALUE_W : 0), toggle_y(lcd) + BTN_H + 4, STEP_W, BTN_H};
  return r;
}

static rect_i column_meta_rect(int i) {
  rect_i lcd = lcd_rect(i);
  rect_i r = {lcd.x, lcd.y + LCD_H, lcd.w, CHROME_H};
  return r;
}

static rect_i add_rect(void) {
  rect_i r = {12, 6, 72, 24};
  return r;
}

static rect_i remove_rect(void) {
  rect_i r = {90, 6, 88, 24};
  return r;
}

static bool point_in(int x, int y, rect_i r) {
  return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static void fit_bench_window(void) {
  if (!g_window) return;
  int cols = g_n < 3 ? 3 : g_n;
  SDL_SetWindowSize(g_window, COL_W * cols + 24, TOP_BAR + LCD_H + CHROME_H + 8);
}

static int grid_step(void) { return (SDL_GetModState() & KMOD_SHIFT) ? GRID_STEP_FAST : GRID_STEP; }

static void set_radio_grid(int i, int east, int north) {
  if (i < 0 || i >= g_n) return;
  if (east < -9999) east = -9999;
  if (east > 9999) east = 9999;
  if (north < -9999) north = -9999;
  if (north > 9999) north = 9999;
  g_radios[i].east = east;
  g_radios[i].north = north;
  apply_gps(&g_radios[i]);
}

static int heading_step(void) { return (SDL_GetModState() & KMOD_SHIFT) ? HEAD_STEP_FAST : HEAD_STEP; }

static void set_heading(int i, int deg) {
  deg %= 360;
  if (deg < 0) deg += 360;
  g_radios[i].heading_deg = deg;
  apply_heading(&g_radios[i]);
}

static const char *heading_name(int deg) {
  static const char *name[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int d = deg % 360;
  if (d < 0) d += 360;
  return name[((d + 22) / 45) % 8];
}

static int hit_radio(int x, int y) {
  for (int i = 0; i < g_n; i++) {
    if (point_in(x, y, lcd_rect(i))) return i;
  }
  return -1;
}

static int hit_meta(int x, int y) {
  for (int i = 0; i < g_n; i++) {
    if (point_in(x, y, column_meta_rect(i))) return i;
  }
  return -1;
}

static void fill_rect(rect_i r, uint8_t g) {
  SDL_SetRenderDrawColor(g_renderer, g, g, g, 255);
  SDL_Rect s = {r.x, r.y, r.w, r.h};
  SDL_RenderFillRect(g_renderer, &s);
}

static void draw_btn(rect_i r, const char *label, bool enabled) {
  fill_rect(r, enabled ? 58 : 40);
  int tw = text_width(label);
  int tx = r.x + (r.w - tw) / 2;
  if (tx < r.x + 2) tx = r.x + 2;
  uint8_t c = enabled ? 255 : 120;
  text_draw(g_renderer, tx, r.y + 3, label, c, c, c);
}

static void blit_lcd(int i, rect_i lcd) {
  void *pixels = NULL;
  int pitch = 0;
  if (SDL_LockTexture(g_lcd, NULL, &pixels, &pitch) != 0) return;
  SDL_PixelFormat *fmt = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
  Uint32 paper = SDL_MapRGBA(fmt, 207, 212, 192, 255);
  Uint32 ink = SDL_MapRGBA(fmt, 28, 34, 24, 255);
  const uint8_t *buf = g_radios[i].ram.display;
  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    Uint32 *row = (Uint32 *)((uint8_t *)pixels + (size_t)y * (size_t)pitch);
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
      size_t idx = (size_t)(y >> 3) * DISPLAY_WIDTH + (size_t)x;
      bool on = (buf[idx] & (uint8_t)(1u << (y & 7))) != 0;
      row[x] = on ? ink : paper;
    }
  }
  SDL_FreeFormat(fmt);
  SDL_UnlockTexture(g_lcd);
  SDL_Rect dst = {lcd.x, lcd.y, lcd.w, lcd.h};
  SDL_RenderCopy(g_renderer, g_lcd, NULL, &dst);
}

static void draw_all(void) {
  SDL_SetRenderDrawColor(g_renderer, 31, 31, 31, 255);
  SDL_RenderClear(g_renderer);

  draw_btn(add_rect(), "+ add", g_n < MAX_RADIOS);
  draw_btn(remove_rect(), "− remove", g_n > 1);
  text_draw(g_renderer, 190, 10,
            "click LCD to focus    scroll LCD = knob    N W E S move    Shift = 25 ft    Esc = back", 255,
            255, 255);

  for (int i = 0; i < g_n; i++) {
    rect_i lcd = lcd_rect(i);
    if (i == g_focus) {
      SDL_SetRenderDrawColor(g_renderer, 230, 191, 51, 255);
      SDL_Rect fr = {lcd.x - 3, lcd.y - 3, lcd.w + 6, lcd.h + 6};
      SDL_RenderDrawRect(g_renderer, &fr);
    }
    blit_lcd(i, lcd);

    char hears[256];
    hears[0] = 0;
    int hp = 0;
    for (int j = 0; j < g_n; j++) {
      if (j == i) continue;
      int8_t rssi;
      int de = g_radios[i].east - g_radios[j].east;
      int dn = g_radios[i].north - g_radios[j].north;
      if (!air_rssi(gps_hypot_ft(dn, de), &rssi)) continue;
      if ((size_t)hp + 1 >= sizeof hears) break;
      hp += snprintf(hears + hp, sizeof hears - (size_t)hp, "%s%s", hp ? "," : "", g_radios[j].name);
    }

    const char *dir_lbl[] = {"N", "E", "S", "W"};
    for (int d = 0; d < 4; d++) draw_btn(dir_btn(i, d), dir_lbl[d], true);
    rect_i west = dir_btn(i, DIR_W);
    char place[40];
    snprintf(place, sizeof place, "%d E, %d N", g_radios[i].east, g_radios[i].north);
    text_draw(g_renderer, lcd.x, west.y + 3, place, 255, 255, 255);

    char hear_line[280];
    snprintf(hear_line, sizeof hear_line, "hears %s", hears[0] ? hears : "nobody");
    text_draw(g_renderer, lcd.x, status_y(lcd), hear_line, 210, 210, 210);

    char posted[64];
    if (!g_radios[i].sky) {
      snprintf(posted, sizeof posted, "no sky");
    } else {
      gps_fix_t fix;
      gps_from_feet(g_radios[i].east, g_radios[i].north, &fix);
      char lat[24], lon[24];
      gps_format_lat(lat, sizeof lat, fix.lat_min_e4);
      gps_format_lon(lon, sizeof lon, fix.lon_min_e4);
      snprintf(posted, sizeof posted, "%s    %s", lat, lon);
    }
    text_draw(g_renderer, lcd.x, status_y(lcd) + LINE, posted, 170, 170, 170);

    draw_btn(toggle_btn(i, 0), g_radios[i].tilt_deg > 20 ? "tipped" : "flat", true);
    draw_btn(toggle_btn(i, 1), g_radios[i].still ? "still" : "moving", true);
    draw_btn(toggle_btn(i, 2), g_radios[i].sky ? "sky" : "no sky", true);
    draw_btn(toggle_btn(i, 3), "sample", true);

    draw_btn(head_btn(i, 0), "−", true);
    draw_btn(head_btn(i, 1), "+", true);
    char hdg[32];
    int deg = g_radios[i].heading_deg;
    snprintf(hdg, sizeof hdg, "%d° %s", deg, heading_name(deg));
    rect_i hminus = head_btn(i, 0);
    rect_i hplus = head_btn(i, 1);
    int hw = text_width(hdg);
    int hx = hminus.x + hminus.w;
    text_draw(g_renderer, hx + (hplus.x - hx - hw) / 2, hminus.y + 3, hdg, 255, 255, 255);
  }
}

static void on_click(int x, int y) {
  g_dragging = false;
  g_lcd_click = false;
  g_drag_acc = 0;
  if (point_in(x, y, add_rect())) {
    if (g_n >= MAX_RADIOS) return;
    int maxe = 0;
    for (int i = 0; i < g_n; i++) {
      if (g_radios[i].east > maxe) maxe = g_radios[i].east;
    }
    char name[8];
    snprintf(name, sizeof name, "%c", 'A' + g_n);
    add_radio(name, maxe + 75, 0);
    fit_bench_window();
    return;
  }
  if (point_in(x, y, remove_rect())) {
    remove_last();
    fit_bench_window();
    return;
  }
  for (int i = 0; i < g_n; i++) {
    for (int d = 0; d < 4; d++) {
      if (!point_in(x, y, dir_btn(i, d))) continue;
      g_focus = i;
      int step = grid_step();
      int east = g_radios[i].east;
      int north = g_radios[i].north;
      if (d == DIR_N) north += step;
      else if (d == DIR_S) north -= step;
      else if (d == DIR_E) east += step;
      else east -= step;
      set_radio_grid(i, east, north);
      return;
    }
    if (point_in(x, y, toggle_btn(i, 3))) {
      g_focus = i;
      locate_gps_soon(&g_radios[i].beeper.locate);
      return;
    }
    if (point_in(x, y, toggle_btn(i, 2))) {
      g_focus = i;
      g_radios[i].sky = !g_radios[i].sky;
      apply_gps(&g_radios[i]);
      return;
    }
    if (point_in(x, y, toggle_btn(i, 0))) {
      g_focus = i;
      g_radios[i].tilt_deg = g_radios[i].tilt_deg > 20 ? 0 : 25;
      apply_heading(&g_radios[i]);
      return;
    }
    if (point_in(x, y, toggle_btn(i, 1))) {
      g_focus = i;
      g_radios[i].still = !g_radios[i].still;
      apply_heading(&g_radios[i]);
      return;
    }
    if (point_in(x, y, head_btn(i, 0)) || point_in(x, y, head_btn(i, 1))) {
      g_focus = i;
      int step = heading_step();
      set_heading(i, g_radios[i].heading_deg + (point_in(x, y, head_btn(i, 1)) ? step : -step));
      return;
    }
  }
  int i = hit_radio(x, y);
  if (i >= 0) {
    g_focus = i;
    g_lcd_click = true;
  }
}

static void on_wheel(int x, int y, int dy) {
  if (dy == 0) return;
  int meta = hit_meta(x, y);
  if (meta >= 0 && hit_radio(x, y) < 0) {
    g_focus = meta;
    int delta = dy > 0 ? grid_step() : -grid_step();
    set_radio_grid(meta, g_radios[meta].east, g_radios[meta].north + delta);
    return;
  }
  int i = hit_radio(x, y);
  if (i >= 0) g_focus = i;
  if (g_focus < 0 || g_focus >= g_n) return;
  ram_hal_push_encoder(&g_radios[g_focus].ram, dy < 0 ? ENC_CW : ENC_CCW);
}

static void on_key(SDL_Keycode key) {
  if (g_focus < 0 || g_focus >= g_n) return;
  if (key == SDLK_ESCAPE) {
    ram_hal_push_back(&g_radios[g_focus].ram);
    return;
  }
  if (key == SDLK_DOWN || key == SDLK_j) {
    ram_hal_push_encoder(&g_radios[g_focus].ram, ENC_CW);
    return;
  }
  if (key == SDLK_UP || key == SDLK_k) {
    ram_hal_push_encoder(&g_radios[g_focus].ram, ENC_CCW);
    return;
  }
  if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
    ram_hal_push_encoder(&g_radios[g_focus].ram, ENC_PRESS);
    ram_hal_push_encoder(&g_radios[g_focus].ram, ENC_RELEASE);
    return;
  }
  if (key == SDLK_BACKSPACE) beeper_debug_key(&g_radios[g_focus].beeper, "Backspace");
}

static void on_text(const char *s) {
  if (!s || !s[0] || g_focus < 0 || g_focus >= g_n) return;
  if (s[0] == 'j' || s[0] == 'k' || s[0] == ' ') return;
  unsigned char c = (unsigned char)s[0];
  if (c >= 32 && c <= 126 && s[1] == 0) beeper_debug_key(&g_radios[g_focus].beeper, s);
}

static void handle_event(const SDL_Event *e) {
  if (e->type == SDL_QUIT) return;
  if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
    on_click(e->button.x, e->button.y);
    return;
  }
  if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
    if (g_lcd_click && !g_dragging && g_focus >= 0 && g_focus < g_n) {
      ram_hal_push_encoder(&g_radios[g_focus].ram, ENC_PRESS);
      ram_hal_push_encoder(&g_radios[g_focus].ram, ENC_RELEASE);
    }
    g_dragging = false;
    g_lcd_click = false;
    return;
  }
  if (e->type == SDL_MOUSEMOTION && (e->motion.state & SDL_BUTTON_LMASK) && g_lcd_click) {
    if (e->motion.yrel != 0) g_dragging = true;
    g_drag_acc += (float)e->motion.yrel;
    while (g_drag_acc >= 8.f) {
      ram_hal_push_encoder(&g_radios[g_focus].ram, ENC_CW);
      g_drag_acc -= 8.f;
    }
    while (g_drag_acc <= -8.f) {
      ram_hal_push_encoder(&g_radios[g_focus].ram, ENC_CCW);
      g_drag_acc += 8.f;
    }
    return;
  }
  if (e->type == SDL_MOUSEWHEEL) {
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    int dy = e->wheel.y;
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (dy == 0 && e->wheel.preciseY != 0.f) dy = e->wheel.preciseY > 0.f ? 1 : -1;
#endif
    on_wheel(mx, my, dy);
    return;
  }
  if (e->type == SDL_KEYDOWN) on_key(e->key.keysym.sym);
  if (e->type == SDL_TEXTINPUT) on_text(e->text.text);
}

static bool find_font(char *out, size_t cap) {
  char *base = SDL_GetBasePath();
  if (base) {
    snprintf(out, cap, "%sRoboto-Regular.ttf", base);
    SDL_free(base);
    FILE *f = fopen(out, "rb");
    if (f) {
      fclose(f);
      return true;
    }
  }
  const char *rel = "emu/third_party/roboto/Roboto-Regular.ttf";
  FILE *f = fopen(rel, "rb");
  if (!f) return false;
  fclose(f);
  snprintf(out, cap, "%s", rel);
  return true;
}

static bool ensure_text(void) {
  int ww = 1, wh = 1, pw = 1, ph = 1;
  SDL_GetWindowSize(g_window, &ww, &wh);
  SDL_GetRendererOutputSize(g_renderer, &pw, &ph);
  float dpi = (ww > 0) ? (float)pw / (float)ww : 1.f;
  if (dpi < 1.f) dpi = 1.f;
  SDL_RenderSetScale(g_renderer, dpi, dpi);
  if (g_dpi > 0.f && dpi < g_dpi + 0.05f && dpi > g_dpi - 0.05f) return true;
  char path[1024];
  if (!find_font(path, sizeof path) || !text_open(g_renderer, path, dpi)) return false;
  g_dpi = dpi;
  return true;
}

static bool create_lcd_texture(void) {
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  g_lcd = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                            DISPLAY_WIDTH, DISPLAY_HEIGHT);
  return g_lcd != NULL;
}

static int fail(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Peeple Beeper", msg, g_window);
  return 1;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  init_nvm_dir();
  g_t0 = SDL_GetTicks();
  add_radio("A", 0, 0);
  add_radio("B", 75, 0);
  add_radio("C", 150, 0);
  emu_unit_t units[MAX_RADIOS];
  as_units(units);
  seed_emulator_directory(units, g_n);
  g_focus = 0;

  int win_w = COL_W * 3 + 24;
  int win_h = TOP_BAR + LCD_H + CHROME_H + 8;
  g_window = SDL_CreateWindow("Peeple Beeper", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w,
                              win_h, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
  if (!g_window) return fail(SDL_GetError());
  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
  if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
  if (!g_renderer) return fail(SDL_GetError());
  if (!create_lcd_texture()) return fail("Could not create the LCD texture");

  if (!ensure_text()) {
    return fail("Roboto-Regular.ttf is missing. Rebuild so it sits next to the executable.");
  }
  SDL_StartTextInput();
  SDL_RaiseWindow(g_window);

  tick_all();
  uint32_t next = SDL_GetTicks();
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;
      else handle_event(&e);
    }
    uint32_t now = SDL_GetTicks();
    if ((int32_t)(now - next) < 0) {
      SDL_Delay(next - now);
      now = SDL_GetTicks();
    }
    next = now + 16;
    tick_all();
    ensure_text();
    draw_all();
    SDL_RenderPresent(g_renderer);
  }

  text_close();
  SDL_DestroyTexture(g_lcd);
  SDL_DestroyRenderer(g_renderer);
  SDL_DestroyWindow(g_window);
  SDL_Quit();
  return 0;
}
