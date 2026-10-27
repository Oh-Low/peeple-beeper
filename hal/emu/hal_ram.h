#pragma once

#include "hal.h"

#define RAM_ENC_Q 32
#define RAM_RAW_Q 16

typedef struct {
  uint8_t display[DISPLAY_BUF_LEN];
  encoder_event_t enc[RAM_ENC_Q];
  int enc_r, enc_w, enc_n;
  int back_n;
  raw_frame_t raw[RAM_RAW_Q];
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
  int tilt_deg;
  bool still;
  int heading_deg;
  uint32_t vibe_until;
} ram_hal_t;

void ram_hal_init(ram_hal_t *r, hal_t *out);
void ram_hal_push_encoder(ram_hal_t *r, encoder_event_t ev);
void ram_hal_push_back(ram_hal_t *r);
void ram_hal_push_raw(ram_hal_t *r, const uint8_t *bytes, size_t len, int8_t rssi);
void ram_hal_set_gps(ram_hal_t *r, const gps_fix_t *fix);
void ram_hal_set_heading(ram_hal_t *r, int tilt_deg, bool still, int heading_deg);
