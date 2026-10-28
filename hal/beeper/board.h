#pragma once

#include "hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void board_init(void);
void board_pump(void);
void board_flush(const uint8_t *fb);
encoder_event_t board_encoder_poll(void);
bool board_back_poll(void);
void board_random_fill(uint8_t *buf, size_t len);
uint32_t board_now_ms(void);
void board_gps_power(bool on);
bool board_heading(heading_sample_t *out);
bool board_radio_poll(raw_frame_t *out);
void board_radio_send(const uint8_t *bytes, size_t len, bool whisper);
size_t board_nvm_load(uint8_t *out, size_t max);
bool board_nvm_save(const uint8_t *bytes, size_t len);
void board_vibe(uint16_t ms);
void fault_blink(void);
