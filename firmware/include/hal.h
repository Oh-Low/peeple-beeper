#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_PAGES 8
#define DISPLAY_BUF_LEN (DISPLAY_WIDTH * DISPLAY_PAGES)
#define RAW_FRAME_MAX 128
#define NVM_BYTES (100 * 1024)
#define PAGE_FROM_MAX 15
#define PAGE_BODY_MAX 79

typedef enum {
  ENC_NONE = 0,
  ENC_CW,
  ENC_CCW,
  ENC_PRESS,
  ENC_RELEASE,
} encoder_event_t;

/* from/body are NUL-terminated. Max payload is PAGE_FROM_MAX / PAGE_BODY_MAX. */
typedef struct {
  char from[16];
  char body[80];
  int8_t rssi;
  uint32_t received_ms;
  /* Sender's mascot from the sealed page. BEEPLE_NONE keeps the spider. */
  uint8_t beeple;
} page_t;

typedef struct {
  uint8_t bytes[RAW_FRAME_MAX];
  uint8_t len;
  int8_t rssi;
  uint32_t received_ms;
} raw_frame_t;

/* Signed arcminutes times 10,000. Positive is north and east.
   This is the PA1616S RMC field (ddmm.mmmm / dddmm.mmmm), not decimal degrees. */
typedef struct {
  bool valid;
  int32_t lat_min_e4;
  int32_t lon_min_e4;
} gps_fix_t;

/* LSM303AGR sample. heading_deg is clockwise from north: 0 means the top of
   the device points north. The UI withholds it unless the unit is level and still. */
typedef struct {
  int tilt_deg;
  bool still;
  bool motor;
  int heading_deg;
} heading_sample_t;

typedef struct hal {
  void *ctx;
  void (*display_clear)(void *ctx);
  void (*display_set_pixel)(void *ctx, uint8_t x, uint8_t y, bool on);
  uint8_t *(*display_buffer)(void *ctx);
  void (*display_flush)(void *ctx);
  encoder_event_t (*encoder_poll)(void *ctx);
  bool (*back_poll)(void *ctx);
  void (*random_fill)(void *ctx, uint8_t *buf, size_t len);
  void (*radio_set_whisper)(void *ctx, bool on);
  bool (*radio_whisper)(void *ctx);
  void (*radio_send_raw)(void *ctx, const uint8_t *bytes, size_t len);
  bool (*radio_poll_raw)(void *ctx, raw_frame_t *out);
  size_t (*nvm_load)(void *ctx, uint8_t *out, size_t max);
  bool (*nvm_save)(void *ctx, const uint8_t *bytes, size_t len);
  uint32_t (*now_ms)(void *ctx);
  void (*clock)(void *ctx, uint8_t *hh, uint8_t *mm);
  uint8_t (*battery_pct)(void *ctx);
  void (*vibe)(void *ctx, uint16_t ms);
  void (*beep)(void *ctx, uint16_t ms);
  bool (*gps_fix)(void *ctx, gps_fix_t *out);
  void (*gps_power)(void *ctx, bool on);
  bool (*heading)(void *ctx, heading_sample_t *out);
} hal_t;
