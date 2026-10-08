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

typedef enum {
  ENC_NONE = 0,
  ENC_CW,
  ENC_CCW,
  ENC_PRESS,
  ENC_RELEASE,
} encoder_event_t;

/* from/body are NUL-terminated. Max payload is 15 / 79 chars
   (the last byte of each array is the terminator). */
typedef struct {
  char from[16];
  char body[80];
  int8_t rssi;
  uint32_t received_ms;
} page_t;

typedef struct {
  uint8_t bytes[RAW_FRAME_MAX];
  uint8_t len;
  int8_t rssi;
  uint32_t received_ms;
} raw_frame_t;

void hal_display_clear(void);
void hal_display_set_pixel(uint8_t x, uint8_t y, bool on);
/* HAL-owned DISPLAY_BUF_LEN-byte ST7567 buffer: index = page*128+x, bit y&7. */
uint8_t *hal_display_buffer(void);
void hal_display_flush(void);

/* Dequeue one encoder event, or ENC_NONE if the queue is empty. */
encoder_event_t hal_encoder_poll(void);
/* Consumed edge (latched press), not a GPIO level. */
bool hal_back_poll(void);

/* Dequeue one RX packet into out, or false if the queue is empty. */
bool hal_radio_poll_rx(page_t *out);
void hal_radio_send(const char *to, const char *body);

/* Fill buf with CSPRNG bytes. Host: crypto.getRandomValues. */
void hal_random_fill(uint8_t *buf, size_t len);
/* Whisper = min TX power on real nRF. Host only stores a bool. */
void hal_radio_set_whisper(bool on);
/* Current whisper mode (firmware/tests). */
bool hal_radio_whisper(void);
/* Copy and transmit raw bytes. Empty send is a no-op. */
void hal_radio_send_raw(const uint8_t *bytes, size_t len);
/* Dequeue one raw frame into out, or false if empty. Copy on poll. */
bool hal_radio_poll_raw(raw_frame_t *out);

/* Persistent store partition (node_id, contacts, inbox, outbox, …). */
size_t hal_nvm_load(uint8_t *out, size_t max);
bool hal_nvm_save(const uint8_t *bytes, size_t len);

/* Monotonic milliseconds from boot. uint32_t wraps. */
uint32_t hal_now_ms(void);
/* Wall-clock time (hours, minutes). */
void hal_clock(uint8_t *hh, uint8_t *mm);
uint8_t hal_battery_pct(void);
void hal_vibe(uint16_t ms);
void hal_beep(uint16_t ms);
