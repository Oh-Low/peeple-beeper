#include "emu_hal.h"

#include <string.h>

static void q_push_enc(emu_hal_t *r, encoder_event_t ev) {
  if (r->enc_n >= EMU_ENC_Q) return;
  r->enc[r->enc_w] = ev;
  r->enc_w = (r->enc_w + 1) % EMU_ENC_Q;
  r->enc_n++;
}

static void display_clear(void *ctx) {
  emu_hal_t *r = ctx;
  memset(r->display, 0, sizeof r->display);
}

static void display_set_pixel(void *ctx, uint8_t x, uint8_t y, bool on) {
  emu_hal_t *r = ctx;
  if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
  size_t i = (size_t)(y >> 3) * DISPLAY_WIDTH + x;
  uint8_t bit = (uint8_t)(1u << (y & 7));
  if (on) r->display[i] |= bit;
  else r->display[i] &= (uint8_t)~bit;
}

static uint8_t *display_buffer(void *ctx) {
  emu_hal_t *r = ctx;
  return r->display;
}

static void display_flush(void *ctx) {
  (void)ctx;
}

static encoder_event_t encoder_poll(void *ctx) {
  emu_hal_t *r = ctx;
  if (r->enc_n == 0) return ENC_NONE;
  encoder_event_t ev = r->enc[r->enc_r];
  r->enc_r = (r->enc_r + 1) % EMU_ENC_Q;
  r->enc_n--;
  return ev;
}

static bool back_poll(void *ctx) {
  emu_hal_t *r = ctx;
  if (r->back_n <= 0) return false;
  r->back_n--;
  return true;
}

static void random_fill(void *ctx, uint8_t *buf, size_t len) {
  emu_hal_t *r = ctx;
  for (size_t i = 0; i < len; i++) {
    r->rng = r->rng * 1664525u + 1013904223u;
    buf[i] = (uint8_t)(r->rng >> 24);
  }
}

static void radio_set_whisper(void *ctx, bool on) {
  emu_hal_t *r = ctx;
  r->whisper = on;
}

static bool radio_whisper(void *ctx) {
  emu_hal_t *r = ctx;
  return r->whisper;
}

static void radio_send_raw(void *ctx, const uint8_t *bytes, size_t len) {
  emu_hal_t *r = ctx;
  if (!bytes || len == 0) return;
  if (len > RAW_FRAME_MAX) len = RAW_FRAME_MAX;
  if (r->on_tx_raw) r->on_tx_raw(r->on_tx_user, bytes, len);
}

static bool radio_poll_raw(void *ctx, raw_frame_t *out) {
  emu_hal_t *r = ctx;
  if (r->raw_n == 0) return false;
  *out = r->raw[r->raw_r];
  r->raw_r = (r->raw_r + 1) % EMU_RAW_Q;
  r->raw_n--;
  return true;
}

static size_t nvm_load(void *ctx, uint8_t *out, size_t max) {
  emu_hal_t *r = ctx;
  size_t n = r->nvm_len;
  if (n > max) n = max;
  if (n) memcpy(out, r->nvm, n);
  return n;
}

static bool nvm_save(void *ctx, const uint8_t *bytes, size_t len) {
  emu_hal_t *r = ctx;
  if (len > NVM_BYTES) return false;
  if (len) memcpy(r->nvm, bytes, len);
  r->nvm_len = len;
  return true;
}

static uint32_t now_ms(void *ctx) {
  emu_hal_t *r = ctx;
  return r->now_ms;
}

static void clock_fn(void *ctx, uint8_t *hh, uint8_t *mm) {
  emu_hal_t *r = ctx;
  if (hh) *hh = r->hh;
  if (mm) *mm = r->mm;
}

static uint8_t battery_pct(void *ctx) {
  emu_hal_t *r = ctx;
  return r->battery;
}

static void vibe(void *ctx, uint16_t ms) {
  emu_hal_t *r = ctx;
  r->vibe_ms = ms;
  r->vibe_until = ms == 0 ? r->now_ms : r->now_ms + ms;
}

static void beep(void *ctx, uint16_t ms) {
  emu_hal_t *r = ctx;
  r->beep_ms = ms;
}

static void backlight_fn(void *ctx, bool on) {
  (void)ctx;
  (void)on;
}

static bool motor_running(const emu_hal_t *r) {
  return r->vibe_ms != 0 && (int32_t)(r->vibe_until - r->now_ms) > 0;
}

static bool heading_fn(void *ctx, heading_sample_t *out) {
  emu_hal_t *r = ctx;
  if (!out) return false;
  out->tilt_deg = r->tilt_deg;
  out->still = r->still;
  out->motor = motor_running(r);
  int d = r->heading_deg % 360;
  if (d < 0) d += 360;
  out->heading_deg = d;
  return true;
}

static void gps_power_fn(void *ctx, bool on) {
  emu_hal_t *r = ctx;
  r->gps_on = on;
}

static bool gps_fix_fn(void *ctx, gps_fix_t *out) {
  emu_hal_t *r = ctx;
  if (!out) return false;
  if (!r->gps_on || !r->gps_set || !r->gps.valid) {
    memset(out, 0, sizeof *out);
    return false;
  }
  *out = r->gps;
  return true;
}

void emu_hal_init(emu_hal_t *r, hal_t *out) {
  memset(r, 0, sizeof *r);
  r->battery = 87;
  r->rng = 1;
  r->still = true;
  r->gps_on = true;
  out->ctx = r;
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
  out->backlight = backlight_fn;
}

void emu_hal_push_encoder(emu_hal_t *r, encoder_event_t ev) {
  q_push_enc(r, ev);
}

void emu_hal_push_back(emu_hal_t *r) {
  if (r->back_n < 8) r->back_n++;
}

void emu_hal_set_heading(emu_hal_t *r, int tilt_deg, bool still, int heading_deg) {
  if (!r) return;
  if (tilt_deg < 0) tilt_deg = 0;
  if (tilt_deg > 90) tilt_deg = 90;
  r->tilt_deg = tilt_deg;
  r->still = still;
  int d = heading_deg % 360;
  if (d < 0) d += 360;
  r->heading_deg = d;
}

void emu_hal_set_gps(emu_hal_t *r, const gps_fix_t *fix) {
  if (!r) return;
  if (!fix) {
    r->gps_set = false;
    memset(&r->gps, 0, sizeof r->gps);
    return;
  }
  r->gps = *fix;
  r->gps_set = true;
}

void emu_hal_push_raw(emu_hal_t *r, const uint8_t *bytes, size_t len, int8_t rssi) {
  if (!bytes || len == 0 || r->raw_n >= EMU_RAW_Q) return;
  if (len > RAW_FRAME_MAX) len = RAW_FRAME_MAX;
  raw_frame_t *f = &r->raw[r->raw_w];
  memcpy(f->bytes, bytes, len);
  f->len = (uint8_t)len;
  f->rssi = rssi;
  f->received_ms = r->now_ms;
  r->raw_w = (r->raw_w + 1) % EMU_RAW_Q;
  r->raw_n++;
}
