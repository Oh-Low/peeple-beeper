#include "display.h"

#include <string.h>

static const uint8_t FONT_ASCII[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
    {0x00, 0x00, 0x2f, 0x00, 0x00}, /* ! */
    {0x00, 0x03, 0x00, 0x03, 0x00}, /* " */
    {0x0a, 0x1f, 0x0a, 0x1f, 0x0a}, /* # */
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, /* $ */
    {0x63, 0x13, 0x08, 0x64, 0x63}, /* % */
    {0x32, 0x4d, 0x59, 0x26, 0x50}, /* & */
    {0x00, 0x00, 0x03, 0x00, 0x00}, /* ' */
    {0x00, 0x00, 0x3e, 0x41, 0x00}, /* ( */
    {0x00, 0x41, 0x3e, 0x00, 0x00}, /* ) */
    {0x08, 0x2a, 0x1c, 0x2a, 0x08}, /* * */
    {0x08, 0x08, 0x3e, 0x08, 0x08}, /* + */
    {0x00, 0x40, 0x30, 0x00, 0x00}, /* , */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* - */
    {0x00, 0x00, 0x20, 0x00, 0x00}, /* . */
    {0x60, 0x10, 0x08, 0x04, 0x03}, /* / */
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, /* 0 */
    {0x00, 0x42, 0x7f, 0x40, 0x00}, /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 */
    {0x22, 0x49, 0x49, 0x49, 0x36}, /* 3 */
    {0x18, 0x14, 0x12, 0x7f, 0x10}, /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 */
    {0x3e, 0x49, 0x49, 0x49, 0x30}, /* 6 */
    {0x01, 0x01, 0x79, 0x05, 0x03}, /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 */
    {0x06, 0x49, 0x49, 0x49, 0x3e}, /* 9 */
    {0x00, 0x00, 0x12, 0x00, 0x00}, /* : */
    {0x00, 0x40, 0x32, 0x00, 0x00}, /* ; */
    {0x08, 0x14, 0x22, 0x41, 0x00}, /* < */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /* = */
    {0x00, 0x41, 0x22, 0x14, 0x08}, /* > */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* ? */
    {0x3e, 0x41, 0x5d, 0x55, 0x5e}, /* @ */
    {0x7e, 0x09, 0x09, 0x09, 0x7e}, /* A */
    {0x7f, 0x49, 0x49, 0x49, 0x36}, /* B */
    {0x3e, 0x41, 0x41, 0x41, 0x22}, /* C */
    {0x7f, 0x41, 0x41, 0x41, 0x3e}, /* D */
    {0x7f, 0x49, 0x49, 0x49, 0x41}, /* E */
    {0x7f, 0x09, 0x09, 0x09, 0x01}, /* F */
    {0x3e, 0x41, 0x49, 0x49, 0x3a}, /* G */
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, /* H */
    {0x00, 0x41, 0x7f, 0x41, 0x00}, /* I */
    {0x20, 0x40, 0x41, 0x3f, 0x01}, /* J */
    {0x7f, 0x08, 0x14, 0x22, 0x41}, /* K */
    {0x7f, 0x40, 0x40, 0x40, 0x40}, /* L */
    {0x7f, 0x02, 0x04, 0x02, 0x7f}, /* M */
    {0x7f, 0x02, 0x04, 0x08, 0x7f}, /* N */
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, /* O */
    {0x7f, 0x09, 0x09, 0x09, 0x06}, /* P */
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, /* Q */
    {0x7f, 0x09, 0x19, 0x29, 0x46}, /* R */
    {0x26, 0x49, 0x49, 0x49, 0x32}, /* S */
    {0x01, 0x01, 0x7f, 0x01, 0x01}, /* T */
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, /* U */
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, /* V */
    {0x7f, 0x20, 0x18, 0x20, 0x7f}, /* W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* X */
    {0x03, 0x04, 0x78, 0x04, 0x03}, /* Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* Z */
    {0x00, 0x7f, 0x41, 0x41, 0x00}, /* [ */
    {0x03, 0x04, 0x08, 0x10, 0x60}, /* \ */
    {0x00, 0x41, 0x41, 0x7f, 0x00}, /* ] */
    {0x04, 0x02, 0x01, 0x02, 0x04}, /* ^ */
    {0x40, 0x40, 0x40, 0x40, 0x40}, /* _ */
    {0x00, 0x01, 0x02, 0x00, 0x00}, /* ` */
    {0x20, 0x54, 0x54, 0x54, 0x78}, /* a */
    {0x7f, 0x44, 0x44, 0x44, 0x38}, /* b */
    {0x38, 0x44, 0x44, 0x44, 0x00}, /* c */
    {0x38, 0x44, 0x44, 0x44, 0x7f}, /* d */
    {0x38, 0x54, 0x54, 0x54, 0x18}, /* e */
    {0x08, 0x7e, 0x09, 0x01, 0x02}, /* f */
    {0x08, 0x54, 0x54, 0x54, 0x3c}, /* g */
    {0x7f, 0x04, 0x04, 0x04, 0x78}, /* h */
    {0x00, 0x44, 0x7d, 0x40, 0x00}, /* i */
    {0x20, 0x40, 0x44, 0x3d, 0x00}, /* j */
    {0x7f, 0x10, 0x28, 0x44, 0x00}, /* k */
    {0x00, 0x41, 0x7f, 0x40, 0x00}, /* l */
    {0x7c, 0x04, 0x78, 0x04, 0x78}, /* m */
    {0x7c, 0x04, 0x04, 0x04, 0x78}, /* n */
    {0x38, 0x44, 0x44, 0x44, 0x38}, /* o */
    {0x7c, 0x14, 0x14, 0x14, 0x08}, /* p */
    {0x08, 0x14, 0x14, 0x14, 0x7c}, /* q */
    {0x7c, 0x08, 0x04, 0x04, 0x08}, /* r */
    {0x48, 0x54, 0x54, 0x54, 0x20}, /* s */
    {0x04, 0x3f, 0x44, 0x40, 0x20}, /* t */
    {0x3c, 0x40, 0x40, 0x40, 0x7c}, /* u */
    {0x1c, 0x20, 0x40, 0x20, 0x1c}, /* v */
    {0x3c, 0x40, 0x38, 0x40, 0x3c}, /* w */
    {0x44, 0x28, 0x10, 0x28, 0x44}, /* x */
    {0x0c, 0x50, 0x50, 0x50, 0x3c}, /* y */
    {0x44, 0x64, 0x54, 0x4c, 0x44}, /* z */
    {0x08, 0x3e, 0x41, 0x41, 0x00}, /* { */
    {0x00, 0x00, 0x7f, 0x00, 0x00}, /* | */
    {0x00, 0x41, 0x41, 0x3e, 0x08}, /* } */
    {0x02, 0x02, 0x04, 0x04, 0x02}, /* ~ */
};

/* 3×6, three columns, bit 0 is the top row. Advance is FONT_SMALL_W. */
static const uint8_t FONT_SMALL[95][3] = {
    {0x00, 0x00, 0x00}, /*   */
    {0x00, 0x2f, 0x00}, /* ! */
    {0x03, 0x00, 0x03}, /* " */
    {0x1f, 0x0a, 0x1f}, /* # */
    {0x26, 0x2b, 0x1a}, /* $ */
    {0x31, 0x0c, 0x23}, /* % */
    {0x1a, 0x25, 0x1a}, /* & */
    {0x00, 0x03, 0x00}, /* \' */
    {0x00, 0x1e, 0x21}, /* ( */
    {0x21, 0x1e, 0x00}, /* ) */
    {0x2a, 0x1c, 0x2a}, /* * */
    {0x08, 0x3e, 0x08}, /* + */
    {0x20, 0x18, 0x00}, /* , */
    {0x08, 0x08, 0x08}, /* - */
    {0x00, 0x30, 0x00}, /* . */
    {0x30, 0x0c, 0x03}, /* / */
    {0x1e, 0x21, 0x1e}, /* 0 */
    {0x22, 0x3f, 0x20}, /* 1 */
    {0x31, 0x29, 0x26}, /* 2 */
    {0x21, 0x25, 0x1a}, /* 3 */
    {0x07, 0x04, 0x3f}, /* 4 */
    {0x27, 0x25, 0x19}, /* 5 */
    {0x1e, 0x25, 0x19}, /* 6 */
    {0x01, 0x39, 0x07}, /* 7 */
    {0x1a, 0x25, 0x1a}, /* 8 */
    {0x26, 0x29, 0x1e}, /* 9 */
    {0x00, 0x12, 0x00}, /* : */
    {0x20, 0x1a, 0x00}, /* ; */
    {0x0c, 0x12, 0x21}, /* < */
    {0x14, 0x14, 0x14}, /* = */
    {0x21, 0x12, 0x0c}, /* > */
    {0x01, 0x15, 0x02}, /* ? */
    {0x1e, 0x29, 0x2e}, /* @ */
    {0x3e, 0x09, 0x3e}, /* A */
    {0x3f, 0x25, 0x1a}, /* B */
    {0x1e, 0x21, 0x21}, /* C */
    {0x3f, 0x21, 0x1e}, /* D */
    {0x3f, 0x25, 0x21}, /* E */
    {0x3f, 0x05, 0x01}, /* F */
    {0x1e, 0x21, 0x39}, /* G */
    {0x3f, 0x04, 0x3f}, /* H */
    {0x21, 0x3f, 0x21}, /* I */
    {0x11, 0x21, 0x1f}, /* J */
    {0x3f, 0x0c, 0x33}, /* K */
    {0x3f, 0x20, 0x20}, /* L */
    {0x3f, 0x02, 0x3f}, /* M */
    {0x3f, 0x01, 0x3e}, /* N */
    {0x1e, 0x21, 0x1e}, /* O */
    {0x3f, 0x09, 0x06}, /* P */
    {0x0e, 0x11, 0x2e}, /* Q */
    {0x3f, 0x09, 0x36}, /* R */
    {0x22, 0x2d, 0x11}, /* S */
    {0x01, 0x3f, 0x01}, /* T */
    {0x1f, 0x20, 0x1f}, /* U */
    {0x0f, 0x30, 0x0f}, /* V */
    {0x3f, 0x10, 0x3f}, /* W */
    {0x33, 0x0c, 0x33}, /* X */
    {0x03, 0x3c, 0x03}, /* Y */
    {0x31, 0x2d, 0x23}, /* Z */
    {0x00, 0x3f, 0x21}, /* [ */
    {0x03, 0x0c, 0x30}, /* \\ */
    {0x21, 0x3f, 0x00}, /* ] */
    {0x02, 0x01, 0x02}, /* ^ */
    {0x20, 0x20, 0x20}, /* _ */
    {0x00, 0x01, 0x02}, /* ` */
    {0x32, 0x2a, 0x3c}, /* a */
    {0x3f, 0x24, 0x18}, /* b */
    {0x18, 0x24, 0x24}, /* c */
    {0x18, 0x24, 0x3f}, /* d */
    {0x1c, 0x2a, 0x26}, /* e */
    {0x04, 0x3f, 0x01}, /* f */
    {0x24, 0x2a, 0x1e}, /* g */
    {0x3f, 0x04, 0x38}, /* h */
    {0x04, 0x3d, 0x00}, /* i */
    {0x20, 0x20, 0x1d}, /* j */
    {0x3f, 0x14, 0x22}, /* k */
    {0x01, 0x3f, 0x20}, /* l */
    {0x3e, 0x02, 0x3c}, /* m */
    {0x3c, 0x04, 0x38}, /* n */
    {0x18, 0x24, 0x18}, /* o */
    {0x3e, 0x0a, 0x04}, /* p */
    {0x04, 0x0a, 0x3e}, /* q */
    {0x3c, 0x08, 0x04}, /* r */
    {0x28, 0x2c, 0x14}, /* s */
    {0x04, 0x3f, 0x24}, /* t */
    {0x1c, 0x20, 0x3c}, /* u */
    {0x1c, 0x20, 0x1c}, /* v */
    {0x3c, 0x10, 0x3c}, /* w */
    {0x24, 0x18, 0x24}, /* x */
    {0x26, 0x28, 0x1e}, /* y */
    {0x34, 0x2c, 0x24}, /* z */
    {0x04, 0x1e, 0x21}, /* { */
    {0x00, 0x3f, 0x00}, /* | */
    {0x21, 0x1e, 0x04}, /* } */
    {0x08, 0x04, 0x18}, /* ~ */
};

static const uint8_t GLYPH_SMALL_BOX[3] = {0x3f, 0x21, 0x3f};

static const uint8_t GLYPH_HEART[5] = {0x06, 0x1f, 0x3e, 0x1f, 0x06};
static const uint8_t GLYPH_GEAR[5] = {0x36, 0x6b, 0x3e, 0x6b, 0x36};
static const uint8_t GLYPH_FALLBACK[5] = {0x7f, 0x41, 0x41, 0x41, 0x7f};

#define CP_HEART 0x2665u
#define CP_GEAR 0x2699u

static int display_utf8_next(const unsigned char *s, uint32_t *cp) {
  if (!s || !s[0]) {
    if (cp) *cp = 0;
    return 0;
  }
  if (s[0] < 0x80) {
    if (cp) *cp = s[0];
    return 1;
  }
  if ((s[0] & 0xe0) == 0xc0 && (s[1] & 0xc0) == 0x80) {
    if (cp) *cp = ((uint32_t)(s[0] & 0x1f) << 6) | (uint32_t)(s[1] & 0x3f);
    return 2;
  }
  if ((s[0] & 0xf0) == 0xe0 && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80) {
    if (cp) {
      *cp = ((uint32_t)(s[0] & 0x0f) << 12) | ((uint32_t)(s[1] & 0x3f) << 6) |
            (uint32_t)(s[2] & 0x3f);
    }
    return 3;
  }
  if ((s[0] & 0xf8) == 0xf0 && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80 &&
      (s[3] & 0xc0) == 0x80) {
    if (cp) {
      *cp = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3f) << 12) |
            ((uint32_t)(s[2] & 0x3f) << 6) | (uint32_t)(s[3] & 0x3f);
    }
    return 4;
  }
  if (cp) *cp = s[0];
  return 1;
}

int display_utf8_count(const char *s) {
  int n = 0;
  const unsigned char *p = (const unsigned char *)s;
  if (!p) return 0;
  while (*p) {
    uint32_t cp;
    int k = display_utf8_next(p, &cp);
    if (k <= 0) break;
    p += k;
    n++;
  }
  return n;
}

static const uint8_t *glyph5(uint32_t cp) {
  if (cp == CP_HEART) return GLYPH_HEART;
  if (cp == CP_GEAR) return GLYPH_GEAR;
  if (cp >= 32 && cp <= 126) return FONT_ASCII[cp - 32];
  return GLYPH_FALLBACK;
}

static const uint8_t *glyph3(uint32_t cp) {
  if (cp >= 32 && cp <= 126) return FONT_SMALL[cp - 32];
  return GLYPH_SMALL_BOX;
}

size_t display_buf_index(int x, int y) {
  return (size_t)(y >> 3) * DISPLAY_WIDTH + (size_t)x;
}

void display_set_pixel(uint8_t *buf, int x, int y, bool on) {
  if (x < 0 || y < 0 || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
  size_t i = display_buf_index(x, y);
  uint8_t bit = (uint8_t)(1u << (y & 7));
  if (on) buf[i] |= bit;
  else buf[i] &= (uint8_t)~bit;
}

bool display_get_pixel(const uint8_t *buf, int x, int y) {
  if (x < 0 || y < 0 || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return false;
  size_t i = display_buf_index(x, y);
  uint8_t bit = (uint8_t)(1u << (y & 7));
  return (buf[i] & bit) != 0;
}

void display_clear(uint8_t *buf) {
  memset(buf, 0, DISPLAY_BUF_LEN);
}

void display_invert_rect(uint8_t *buf, int x, int y, int w, int h) {
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      if (xx < 0 || yy < 0 || xx >= DISPLAY_WIDTH || yy >= DISPLAY_HEIGHT) continue;
      size_t i = display_buf_index(xx, yy);
      buf[i] ^= (uint8_t)(1u << (yy & 7));
    }
  }
}

void display_hline(uint8_t *buf, int x, int y, int w) {
  for (int i = 0; i < w; i++) display_set_pixel(buf, x + i, y, true);
}

static void draw_line(uint8_t *buf, int x0, int y0, int x1, int y1) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  int sx = dx < 0 ? -1 : 1;
  int sy = dy < 0 ? -1 : 1;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  int err = dx - dy;
  for (;;) {
    display_set_pixel(buf, x0, y0, true);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err * 2;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

static int isqrt_int(int n) {
  if (n <= 0) return 0;
  int x = n;
  int y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return x;
}

void display_draw_arrow(uint8_t *buf, int x0, int y0, int x1, int y1) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  int mag = isqrt_int(dx * dx + dy * dy);
  if (mag < 2) {
    display_set_pixel(buf, x1, y1, true);
    return;
  }
  draw_line(buf, x0, y0, x1, y1);
  int head = mag / 3;
  if (head < 4) head = 4;
  if (head > 9) head = 9;
  int wing = head / 2;
  if (wing < 3) wing = 3;
  int bx = x1 - dx * head / mag;
  int by = y1 - dy * head / mag;
  int px = -dy * wing / mag;
  int py = dx * wing / mag;
  if (px == 0 && py == 0) {
    if (dx == 0) px = wing;
    else py = wing;
  }
  draw_line(buf, x1, y1, bx + px, by + py);
  draw_line(buf, x1, y1, bx - px, by - py);
}

void display_draw_char(uint8_t *buf, int x, int y, uint32_t cp) {
  const uint8_t *cols = glyph5(cp);
  for (int cx = 0; cx < 5; cx++) {
    uint8_t col = cols[cx];
    for (int cy = 0; cy < 7; cy++) {
      if (col & (1u << cy)) display_set_pixel(buf, x + cx, y + cy, true);
    }
  }
}

void display_draw_text(uint8_t *buf, int x, int y, const char *s) {
  int cx = x;
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  while (*p) {
    if (cx >= DISPLAY_WIDTH) break;
    uint32_t cp;
    int k = display_utf8_next(p, &cp);
    if (k <= 0) break;
    display_draw_char(buf, cx, y, cp);
    cx += CHAR_W;
    p += k;
  }
}

int display_text_width(const char *s) {
  return display_utf8_count(s ? s : "") * CHAR_W;
}

static void draw_char_small(uint8_t *buf, int x, int y, uint32_t cp) {
  const uint8_t *cols = glyph3(cp);
  for (int cx = 0; cx < 3; cx++) {
    uint8_t col = cols[cx];
    for (int cy = 0; cy < FONT_SMALL_H; cy++) {
      if (col & (1u << cy)) display_set_pixel(buf, x + cx, y + cy, true);
    }
  }
}

void display_draw_text_small(uint8_t *buf, int x, int y, const char *s) {
  int cx = x;
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  while (*p) {
    if (cx >= DISPLAY_WIDTH) break;
    uint32_t cp;
    int k = display_utf8_next(p, &cp);
    if (k <= 0) break;
    draw_char_small(buf, cx, y, cp);
    cx += FONT_SMALL_W;
    p += k;
  }
}

int display_text_width_small(const char *s) {
  int n = display_utf8_count(s ? s : "");
  if (n <= 0) return 0;
  return (n - 1) * FONT_SMALL_W + 3;
}

void display_draw_header(uint8_t *buf, const char *title) {
  char shown[48];
  int n = 0;
  shown[0] = 0;
  const unsigned char *p = (const unsigned char *)(title ? title : "");
  int x = 4;
  while (*p && n < (int)sizeof shown - 1) {
    uint32_t cp;
    int k = display_utf8_next(p, &cp);
    if (k <= 0) break;
    if (x + 2 > DISPLAY_WIDTH - 2) break;
    if (n + k >= (int)sizeof shown) break;
    for (int i = 0; i < k; i++) shown[n++] = (char)p[i];
    shown[n] = 0;
    x += FONT_SMALL_W;
    p += k;
  }
  display_draw_text_small(buf, 4, 1, shown);
  display_invert_rect(buf, 0, 0, DISPLAY_WIDTH, DISPLAY_HEADER_H);
}

int display_cols(void) {
  return DISPLAY_WIDTH / CHAR_W;
}

void display_clip(char *dst, size_t cap, const char *s, int max_chars) {
  if (!dst || cap == 0) return;
  dst[0] = 0;
  if (!s || max_chars < 0) return;
  int n = display_utf8_count(s);
  int keep = n;
  bool ellip = false;
  if (n > max_chars) {
    if (max_chars <= 1) keep = max_chars;
    else {
      keep = max_chars - 1;
      ellip = true;
    }
  }
  const unsigned char *p = (const unsigned char *)s;
  size_t o = 0;
  for (int i = 0; i < keep; i++) {
    uint32_t cp;
    int k = display_utf8_next(p, &cp);
    if (k <= 0) break;
    if (o + (size_t)k + 1 + (ellip ? 1 : 0) >= cap) break;
    memcpy(dst + o, p, (size_t)k);
    o += (size_t)k;
    p += k;
  }
  if (ellip && o + 2 <= cap) {
    dst[o++] = '.';
  }
  dst[o] = 0;
}

static int last_space_at_most(const char *s, int from) {
  int found = -1;
  int i = 0;
  while (s[i] && i <= from) {
    if (s[i] == ' ') found = i;
    i++;
  }
  return found;
}

int display_wrap(const char *s, int width, char out[][DISPLAY_WIDTH + 1], int max_out) {
  if (!s || width <= 0 || max_out <= 0) return 0;
  if (s[0] == 0) return 0;
  int nlines = 0;
  const char *p = s;
  while (*p && nlines < max_out) {
    const char *nl = p;
    while (*nl && *nl != '\n') nl++;
    int part_len = (int)(nl - p);
    if (part_len == 0) {
      out[nlines][0] = 0;
      nlines++;
    } else {
      const char *rest = p;
      int rest_len = part_len;
      while (rest_len > 0 && nlines < max_out) {
        if (rest_len <= width) {
          memcpy(out[nlines], rest, (size_t)rest_len);
          out[nlines][rest_len] = 0;
          nlines++;
          break;
        }
        char tmp[DISPLAY_WIDTH + 1];
        int copy = rest_len < DISPLAY_WIDTH ? rest_len : DISPLAY_WIDTH;
        memcpy(tmp, rest, (size_t)copy);
        tmp[copy] = 0;
        int cut = last_space_at_most(tmp, width);
        if (cut < 1) cut = width;
        memcpy(out[nlines], rest, (size_t)cut);
        out[nlines][cut] = 0;
        nlines++;
        rest += cut;
        rest_len -= cut;
        if (rest_len > 0 && *rest == ' ') {
          rest++;
          rest_len--;
        }
      }
    }
    if (*nl == '\n') p = nl + 1;
    else break;
  }
  return nlines;
}

void display_blit_sprite(uint8_t *dst, const uint8_t *sprite, size_t sprite_len, int sw, int sh,
                         int dx, int dy) {
  for (int y = 0; y < sh; y++) {
    int page = y >> 3;
    uint8_t bit = (uint8_t)(1u << (y & 7));
    for (int x = 0; x < sw; x++) {
      int i = page * sw + x;
      if (i >= 0 && (size_t)i < sprite_len && (sprite[i] & bit)) {
        display_set_pixel(dst, dx + x, dy + y, true);
      }
    }
  }
}
