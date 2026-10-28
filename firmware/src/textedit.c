#include "textedit.h"

#include "display.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define TEXT_LINES 2
#define SEP_Y (CHAR_H * TEXT_LINES)
#define MODE_Y (SEP_Y + 2)
#define MODE_H 9
#define KEYS_Y (MODE_Y + MODE_H + 1)
#define KEY_H 9
#define NARROW_W 12
#define CELL_MAX 64

typedef enum {
  CELL_CASE,
  CELL_SYM,
  CELL_NUM,
  CELL_CHAR,
  CELL_LEFT,
  CELL_RIGHT,
  CELL_SPC,
  CELL_DEL,
  CELL_SAVE,
  CELL_OK,
} cell_kind_t;

typedef struct {
  cell_kind_t kind;
  const char *label;
  char ch;
  int row;
} cell_t;

typedef struct {
  int x, y, w, h;
} rect_t;

static void snap_to_e(text_edit_t *edit);

void text_edit_init(text_edit_t *edit) {
  memset(edit, 0, sizeof *edit);
  edit->max_len = 1;
}

void text_edit_begin(text_edit_t *edit, const char *initial, int max_len, bool can_save) {
  str_copy_max(edit->draft, sizeof edit->draft, initial ? initial : "", (size_t)max_len);
  edit->max_len = max_len;
  edit->page = TEXT_PAGE_LETTER;
  edit->caps = false;
  edit->caret = (int)str_ascii_len(edit->draft);
  edit->can_save = can_save;
  snap_to_e(edit);
}

void text_edit_reset(text_edit_t *edit) {
  text_edit_init(edit);
}

void text_edit_draft(const text_edit_t *edit, char *out, size_t cap) {
  str_copy_max(out, cap, edit->draft, PAGE_BODY_MAX);
  size_t n = str_ascii_len(out);
  while (n > 0 && isspace((unsigned char)out[n - 1])) {
    out[--n] = 0;
  }
}

/* Turn toward E from the left walks SPACE, SAVE, DEL, OK. The column sits
 * just before E, bottom to top, then the keys to the right of the rule. */
static int flatten(const text_edit_t *edit, cell_t *out) {
  static const char *right[4] = {
      "ETAOINSHRDL",
      "CUMWFGYPBVK",
      "1234567890",
      ".,?!'-@#$&*()+=/",
  };
  static const cell_kind_t left_kind[4] = {CELL_SPC, CELL_SAVE, CELL_DEL, CELL_OK};
  static const char *left_label[4] = {"SPACE", "SAVE", "DEL", "OK"};
  int n = 0;
  for (int r = 3; r >= 0; r--) {
    if (left_kind[r] == CELL_SAVE && !edit->can_save) continue;
    if (n >= CELL_MAX) break;
    out[n].kind = left_kind[r];
    out[n].label = left_label[r];
    out[n].ch = 0;
    out[n].row = r;
    n++;
  }
  for (int r = 0; r < 4; r++) {
    for (const char *s = right[r]; *s && n < CELL_MAX; s++) {
      out[n].kind = CELL_CHAR;
      out[n].label = NULL;
      out[n].ch = *s;
      out[n].row = r;
      n++;
    }
  }
  return n;
}

static void clamp_sel(text_edit_t *edit) {
  cell_t cells[CELL_MAX];
  int n = flatten(edit, cells);
  if (n <= 0) {
    edit->sel = 0;
    return;
  }
  if (edit->sel < 0) edit->sel = n - 1;
  if (edit->sel >= n) edit->sel = 0;
}

static void clamp_caret(text_edit_t *edit) {
  int len = (int)str_ascii_len(edit->draft);
  if (edit->caret < 0) edit->caret = 0;
  if (edit->caret > len) edit->caret = len;
}

static void insert_ch(text_edit_t *edit, char ch) {
  int len = (int)str_ascii_len(edit->draft);
  if (len >= edit->max_len) return;
  clamp_caret(edit);
  memmove(edit->draft + edit->caret + 1, edit->draft + edit->caret, (size_t)(len - edit->caret) + 1);
  edit->draft[edit->caret] = ch;
  edit->caret += 1;
}

static void del_before(text_edit_t *edit) {
  clamp_caret(edit);
  if (edit->caret <= 0) return;
  int len = (int)str_ascii_len(edit->draft);
  memmove(edit->draft + edit->caret - 1, edit->draft + edit->caret, (size_t)(len - edit->caret) + 1);
  edit->caret -= 1;
}

static void snap_to_e(text_edit_t *edit) {
  cell_t cells[CELL_MAX];
  int n = flatten(edit, cells);
  for (int i = 0; i < n; i++) {
    if (cells[i].kind == CELL_CHAR && cells[i].ch == 'E') {
      edit->sel = i;
      return;
    }
  }
  edit->sel = 0;
}

void text_edit_turn(text_edit_t *edit, int dir) {
  cell_t cells[CELL_MAX];
  int n = flatten(edit, cells);
  if (n <= 0) return;
  edit->sel = (edit->sel + dir % n + n) % n;
}

text_edit_press_t text_edit_press(text_edit_t *edit) {
  clamp_sel(edit);
  cell_t cells[CELL_MAX];
  int n = flatten(edit, cells);
  if (edit->sel < 0 || edit->sel >= n) return TEXT_PRESS_NONE;
  cell_t cell = cells[edit->sel];

  if (cell.kind == CELL_SPC) {
    insert_ch(edit, ' ');
    return TEXT_PRESS_NONE;
  }
  if (cell.kind == CELL_DEL) {
    del_before(edit);
    return TEXT_PRESS_NONE;
  }
  if (cell.kind == CELL_SAVE) return TEXT_PRESS_SAVE;
  if (cell.kind == CELL_OK) return TEXT_PRESS_OK;
  if (cell.kind == CELL_CHAR && cell.ch) {
    insert_ch(edit, cell.ch);
    if (isalpha((unsigned char)cell.ch)) snap_to_e(edit);
  }
  return TEXT_PRESS_NONE;
}

bool text_edit_handle_text(text_edit_t *edit, const char *key) {
  if (!key) return false;
  if (strcmp(key, "Backspace") == 0) {
    del_before(edit);
    return true;
  }
  if (key[0] == 0 || key[1] != 0) return false;
  unsigned char code = (unsigned char)key[0];
  if (code < 32 || code > 126) return false;
  insert_ch(edit, key[0]);
  return true;
}

static void invert_slant_bar(uint8_t *buf, int x, int y, int w, int h, int cut_left, int cut_right) {
  for (int yy = 0; yy < h; yy++) {
    int from_bottom = h - 1 - yy;
    int left = cut_left - yy;
    if (left < 1) left = 1;
    int right = cut_right - from_bottom;
    if (right < 0) right = 0;
    int span = w - left - right;
    if (span > 0) display_invert_rect(buf, x + left, y + yy, span, 1);
  }
}

static void draw_edit_header(uint8_t *buf, uint8_t hh, uint8_t mm) {
  invert_slant_bar(buf, 0, 1, 64, 9, 6, 8);
  for (int k = 0; k < 3; k++) {
    for (int yy = 0; yy <= 7 - k; yy++) {
      int sx = 66 - yy + k * 5;
      for (int dx = 0; dx < 3; dx++) display_set_pixel(buf, sx + dx, 1 + yy, true);
    }
  }
  char clock[8];
  if (hh > 23 || mm > 59) str_copy_max(clock, sizeof clock, "--:--", 5);
  else snprintf(clock, sizeof clock, "%02u:%02u", (unsigned)hh, (unsigned)mm);
  display_draw_text_small(buf, 107, 3, clock);
}

static void draw_draft_row(uint8_t *buf, const char *draft, int caret) {
  const int x0 = 4;
  const int end_ink = 124;
  int room = end_ink - x0 + 1;
  int maxc = room < 3 ? 1 : (room - 3) / FONT_SMALL_W + 1;
  int len = draft ? (int)str_ascii_len(draft) : 0;
  int start = 0;
  if (caret > maxc - 1) start = caret - (maxc - 1);
  if (start < 0) start = 0;
  if (start > len) start = len;
  char shown[40];
  int n = len - start;
  if (n > maxc) n = maxc;
  if (n < 0) n = 0;
  if (n > (int)sizeof shown - 1) n = (int)sizeof shown - 1;
  if (n > 0 && draft) memcpy(shown, draft + start, (size_t)n);
  shown[n] = 0;
  display_draw_text_small(buf, x0, 17, shown);
  int cx = x0 + (caret - start) * FONT_SMALL_W;
  if (cx <= end_ink - 2) display_invert_rect(buf, cx, 17, 3, FONT_SMALL_H);
}

void text_edit_draw(text_edit_t *edit, uint8_t *buf, uint8_t hh, uint8_t mm) {
  clamp_caret(edit);
  draw_edit_header(buf, hh, mm);
  for (int x = 1; x <= 90; x++) display_set_pixel(buf, x, 13, true);
  draw_draft_row(buf, edit->draft, edit->caret);
  for (int x = 1; x <= 90; x++) display_set_pixel(buf, x, 25, true);

  static const int row_y[4] = {33, 40, 47, 54};
  static const int row_pitch[4] = {7, 7, 6, 5};
  cell_t cells[CELL_MAX];
  int n = flatten(edit, cells);
  for (int r = 0; r < 4; r++) {
    int y = row_y[r];
    int x = 34;
    int pitch = row_pitch[r];
    for (int i = 0; i < n; i++) {
      if (cells[i].row != r) continue;
      if (cells[i].kind != CELL_CHAR) {
        const char *label = cells[i].label ? cells[i].label : "";
        display_draw_text_small(buf, 4, y, label);
        if (i == edit->sel) {
          int ink = display_text_width_small(label);
          if (ink < 3) ink = 3;
          display_invert_rect(buf, 3, y - 1, ink + 2, FONT_SMALL_H + 1);
        }
        continue;
      }
      char lab[2] = {cells[i].ch, 0};
      display_draw_text_small(buf, x, y, lab);
      if (i == edit->sel) {
        int ink = 3;
        int pad = pitch > ink + 2 ? 1 : 0;
        display_invert_rect(buf, x - pad, y - 1, ink + pad * 2, FONT_SMALL_H + 1);
      }
      x += pitch;
    }
  }
  for (int y = 32; y <= 63; y++) display_set_pixel(buf, 30, y, true);
}
