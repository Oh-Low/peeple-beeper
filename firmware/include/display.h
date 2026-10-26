#pragma once

#include "hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHAR_W 6
#define CHAR_H 8
/* 3×6 glyphs. FONT_SMALL_W is the advance, including a 1 px gap. */
#define FONT_SMALL_W 4
#define FONT_SMALL_H 6
#define DISPLAY_HEADER_H 9
#define DISPLAY_WRAP_MAX 16

size_t display_buf_index(int x, int y);
void display_set_pixel(uint8_t *buf, int x, int y, bool on);
bool display_get_pixel(const uint8_t *buf, int x, int y);
void display_clear(uint8_t *buf);
void display_invert_rect(uint8_t *buf, int x, int y, int w, int h);
void display_hline(uint8_t *buf, int x, int y, int w);
/* Shaft from (x0,y0) to (x1,y1), head at the second point. */
void display_draw_arrow(uint8_t *buf, int x0, int y0, int x1, int y1);
void display_draw_char(uint8_t *buf, int x, int y, uint32_t cp);
void display_draw_text(uint8_t *buf, int x, int y, const char *s);
int display_text_width(const char *s);
void display_draw_text_small(uint8_t *buf, int x, int y, const char *s);
/* Ink span of the small font, with no trailing gap. */
int display_text_width_small(const char *s);
/* Solid inverted bar, DISPLAY_HEADER_H tall. Title in the small font at (4, 1). */
void display_draw_header(uint8_t *buf, const char *title);
int display_cols(void);
void display_clip(char *dst, size_t cap, const char *s, int max_chars);
int display_wrap(const char *s, int width, char out[][DISPLAY_WIDTH + 1], int max_out);
void display_blit_sprite(uint8_t *dst, const uint8_t *sprite, size_t sprite_len, int sw, int sh,
                         int dx, int dy);
int display_utf8_count(const char *s);
