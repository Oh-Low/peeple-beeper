#include "nrf_hal.h"

#include "nmea.h"

#include <string.h>

static uint8_t display[DISPLAY_BUF_LEN];
static uint8_t rng_ctr;
static bool whisper;
static char nmea_line[96];
static int nmea_len;
static gps_fix_t nrf_fix;
static bool gps_on;

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
}

static encoder_event_t encoder_poll(void *ctx) {
  (void)ctx;
  return ENC_NONE;
}

static bool back_poll(void *ctx) {
  (void)ctx;
  return false;
}

static void random_fill(void *ctx, uint8_t *buf, size_t len) {
  (void)ctx;
  for (size_t i = 0; i < len; i++) buf[i] = rng_ctr++;
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
  (void)out;
  return false;
}

static size_t nvm_load(void *ctx, uint8_t *out, size_t max) {
  (void)ctx;
  (void)out;
  (void)max;
  return 0;
}

static bool nvm_save(void *ctx, const uint8_t *bytes, size_t len) {
  (void)ctx;
  (void)bytes;
  return len <= NVM_BYTES;
}

static uint32_t now_ms(void *ctx) {
  (void)ctx;
  return 0;
}

static void clock_fn(void *ctx, uint8_t *hh, uint8_t *mm) {
  (void)ctx;
  if (hh) *hh = 0;
  if (mm) *mm = 0;
}

static uint8_t battery_pct(void *ctx) {
  (void)ctx;
  return 0;
}

static void vibe(void *ctx, uint16_t ms) {
  (void)ctx;
  (void)ms;
}

static void beep(void *ctx, uint16_t ms) {
  (void)ctx;
  (void)ms;
}

static bool heading_fn(void *ctx, heading_sample_t *out) {
  (void)ctx;
  if (out) memset(out, 0, sizeof *out);
  return false;
}

static void gps_power_fn(void *ctx, bool on) {
  (void)ctx;
  gps_on = on;
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

void nrf_gps_push_byte(uint8_t byte) {
  if (byte == '\r') return;
  if (byte == '\n') {
    nmea_line[nmea_len] = 0;
    gps_fix_t fix;
    if (nmea_parse_rmc(nmea_line, &fix)) nrf_fix = fix;
    nmea_len = 0;
    return;
  }
  if (nmea_len >= (int)sizeof nmea_line - 1) {
    nmea_len = 0;
    return;
  }
  nmea_line[nmea_len++] = (char)byte;
}

void nrf_hal_init(hal_t *out) {
  memset(display, 0, sizeof display);
  rng_ctr = 0;
  whisper = false;
  nmea_len = 0;
  nmea_line[0] = 0;
  memset(&nrf_fix, 0, sizeof nrf_fix);
  gps_on = true;
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
