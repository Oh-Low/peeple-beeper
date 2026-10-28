#pragma once

#include "hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  TEXT_PAGE_LETTER = 0,
  TEXT_PAGE_SYM,
  TEXT_PAGE_NUM,
} text_edit_page_t;

typedef enum {
  TEXT_PRESS_NONE = 0,
  TEXT_PRESS_OK,
  TEXT_PRESS_SAVE,
} text_edit_press_t;

typedef struct {
  char draft[PAGE_BODY_MAX + 1];
  int max_len;
  text_edit_page_t page;
  bool caps;
  int sel;
  int caret;
  bool can_save;
} text_edit_t;

void text_edit_init(text_edit_t *edit);
void text_edit_begin(text_edit_t *edit, const char *initial, int max_len, bool can_save);
void text_edit_reset(text_edit_t *edit);
void text_edit_draft(const text_edit_t *edit, char *out, size_t cap);
void text_edit_turn(text_edit_t *edit, int dir);
text_edit_press_t text_edit_press(text_edit_t *edit);
bool text_edit_handle_text(text_edit_t *edit, const char *key);
void text_edit_draw(text_edit_t *edit, uint8_t *buf, uint8_t hh, uint8_t mm);
