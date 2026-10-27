#include "beeper_hal.h"

#include "board.h"
#include "nmea.h"

#include <string.h>

static uint8_t display[DISPLAY_BUF_LEN];
static uint8_t rng_ctr;
static bool whisper;
static gps_fix_t nrf_fix;
static nmea_line_t nmea;
static bool gps_on;
static uint8_t clock_hh;
static uint8_t clock_mm;

static void display_clear(void *ctx) {
  (void)ctx;
  memset(display, 0, sizeof display);
}

static void display_set_pixel(void *ctx, uint8_t x, uint8_t y, bool on) {
  (void)ctx;
  if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
  size_t i = (size_t)(y >> 3) * DISPLAY_WIDTH + x;
  uint8_t bit = (uint8_t)(1u << (y & 7));
  if (on) display[i] |= bit;
  else display[i] &= (uint8_t)~bit;
}

static uint8_t *display_buffer(void *ctx) {
  (void)ctx;
  return display;
}

static void display_flush(void *ctx) {
  (void)ctx;
  board_flush(display);
}

static encoder_event_t encoder_poll(void *ctx) {
  (void)ctx;
  return board_encoder_poll();
}

static bool back_poll(void *ctx) {
  (void)ctx;
  return board_back_poll();
}

static void random_fill(void *ctx, uint8_t *buf, size_t len) {
  (void)ctx;
  board_random_fill(buf, len);
}

static void radio_set_whisper(void *ctx, bool on) {
  (void)ctx;
  whisper = on;
}

static bool radio_whisper(void *ctx) {
  (void)ctx;
  return whisper;
}

static void radio_send_raw(void *ctx, const uint8_t *bytes, size_t len) {
  (void)ctx;
  (void)bytes;
  (void)len;
}

static bool radio_poll_raw(void *ctx, raw_frame_t *out) {
  (void)ctx;
  return board_radio_poll(out);
}

static size_t nvm_load(void *ctx, uint8_t *out, size_t max) {
  (void)ctx;
  return board_nvm_load(out, max);
}

static bool nvm_save(void *ctx, const uint8_t *bytes, size_t len) {
  (void)ctx;
  return board_nvm_save(bytes, len);
}

static uint32_t now_ms(void *ctx) {
  (void)ctx;
  return board_now_ms();
}

static void clock_fn(void *ctx, uint8_t *hh, uint8_t *mm) {
  (void)ctx;
  if (hh) *hh = clock_hh;
  if (mm) *mm = clock_mm;
}

static uint8_t battery_pct(void *ctx) {
  (void)ctx;
  return 0;
}

static void vibe(void *ctx, uint16_t ms) {
  (void)ctx;
  board_vibe(ms);
}

static void beep(void *ctx, uint16_t ms) {
  (void)ctx;
  (void)ms;
}

static bool heading_fn(void *ctx, heading_sample_t *out) {
  (void)ctx;
  return board_heading(out);
}

static void gps_power_fn(void *ctx, bool on) {
  (void)ctx;
  if (on && !gps_on) memset(&nrf_fix, 0, sizeof nrf_fix);
  gps_on = on;
  board_gps_power(on);
}

static bool gps_fix_fn(void *ctx, gps_fix_t *out) {
  (void)ctx;
  if (!out) return false;
  if (!gps_on) {
    memset(out, 0, sizeof *out);
    return false;
  }
  *out = nrf_fix;
  return nrf_fix.valid;
}

void beeper_gps_push_byte(uint8_t byte) {
  gps_fix_t fix;
  if (nmea_push_byte(&nmea, byte, &fix, &clock_hh, &clock_mm)) nrf_fix = fix;
}

void beeper_hal_init(hal_t *out) {
  memset(display, 0, sizeof display);
  rng_ctr = 0;
  whisper = false;
  memset(&nmea, 0, sizeof nmea);
  memset(&nrf_fix, 0, sizeof nrf_fix);
  clock_hh = 0;
  clock_mm = 0;
  gps_on = true;
  board_init();
  out->ctx = NULL;
  out->display_clear = display_clear;
  out->display_set_pixel = display_set_pixel;
  out->display_buffer = display_buffer;
  out->display_flush = display_flush;
  out->encoder_poll = encoder_poll;
  out->back_poll = back_poll;
  out->random_fill = random_fill;
  out->radio_set_whisper = radio_set_whisper;
  out->radio_whisper = radio_whisper;
  out->radio_send_raw = radio_send_raw;
  out->radio_poll_raw = radio_poll_raw;
  out->nvm_load = nvm_load;
  out->nvm_save = nvm_save;
  out->now_ms = now_ms;
  out->clock = clock_fn;
  out->battery_pct = battery_pct;
  out->vibe = vibe;
  out->beep = beep;
  out->gps_fix = gps_fix_fn;
  out->gps_power = gps_power_fn;
  out->heading = heading_fn;
}

__attribute__((weak)) void board_init(void) {}
__attribute__((weak)) void board_pump(void) {}
__attribute__((weak)) void board_flush(const uint8_t *fb) { (void)fb; }
__attribute__((weak)) encoder_event_t board_encoder_poll(void) { return ENC_NONE; }
__attribute__((weak)) bool board_back_poll(void) { return false; }
__attribute__((weak)) void board_random_fill(uint8_t *buf, size_t len) {
  for (size_t i = 0; i < len; i++) buf[i] = rng_ctr++;
}
__attribute__((weak)) uint32_t board_now_ms(void) { return 0; }
__attribute__((weak)) void board_gps_power(bool on) { (void)on; }
__attribute__((weak)) bool board_heading(heading_sample_t *out) {
  if (out) memset(out, 0, sizeof *out);
  return false;
}
__attribute__((weak)) bool board_radio_poll(raw_frame_t *out) {
  (void)out;
  return false;
}
__attribute__((weak)) size_t board_nvm_load(uint8_t *out, size_t max) {
  (void)out;
  (void)max;
  return 0;
}
__attribute__((weak)) bool board_nvm_save(const uint8_t *bytes, size_t len) {
  (void)bytes;
  return len <= NVM_BYTES;
}
__attribute__((weak)) void board_vibe(uint16_t ms) { (void)ms; }
