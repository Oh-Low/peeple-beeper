#pragma once

#include <SDL.h>

#include <stdbool.h>
#include <stdint.h>

/* Rasterize Roboto into an atlas. dpi is renderer pixels per window point.
   Pass a path the first time; later calls may pass NULL to rebuild for a new dpi. */
bool text_open(SDL_Renderer *r, const char *ttf_path, float dpi);
void text_close(void);

/* y is the top of the line, in window points. */
void text_draw(SDL_Renderer *r, int x, int y, const char *utf8, uint8_t cr, uint8_t cg, uint8_t cb);
int text_width(const char *utf8);
