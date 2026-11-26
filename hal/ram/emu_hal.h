#pragma once

#include "hal.h"

#define EMU_ENC_Q 32
#define EMU_RAW_Q 16

typedef struct {
  uint8_t display[DISPLAY_BUF_LEN];
  encoder_event_t enc[EMU_ENC_Q];
  int enc_r, enc_w, enc_n;
  int back_n;
  raw_frame_t raw[EMU_RAW_Q];
  int raw_r, raw_w, raw_n;
  uint8_t nvm[NVM_BYTES];
  size_t nvm_len;
  uint32_t now_ms;
  uint8_t hh, mm;
  uint8_t battery;
  bool whisper;
  uint16_t vibe_ms;
  uint16_t beep_ms;
  void (*on_tx_raw)(void *user, const uint8_t *bytes, size_t len);
  void *on_tx_user;
  uint32_t rng;
  gps_fix_t gps;
  bool gps_set;
  bool gps_on;
  bool backlight_on;
  int tilt_deg;
  bool still;
  int heading_deg;
  uint32_t vibe_until;
} emu_hal_t;

void emu_hal_init(emu_hal_t *r, hal_t *out);
void emu_hal_push_encoder(emu_hal_t *r, encoder_event_t ev);
void emu_hal_push_back(emu_hal_t *r);
void emu_hal_push_raw(emu_hal_t *r, const uint8_t *bytes, size_t len, int8_t rssi);
void emu_hal_set_gps(emu_hal_t *r, const gps_fix_t *fix);
void emu_hal_set_heading(emu_hal_t *r, int tilt_deg, bool still, int heading_deg);
