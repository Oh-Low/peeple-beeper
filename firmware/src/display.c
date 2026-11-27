#include "display.h"

#include <string.h>

static const uint8_t FONT_ADV[95] = {
    2,2,4,6,4,5,5,2,3,3,4,4,2,4,2,3,
    4,4,4,4,4,4,4,4,4,4,2,2,4,4,4,4,
    5,5,5,5,5,5,5,5,5,2,5,5,5,6,5,5,
    5,5,5,5,4,5,6,6,6,6,5,3,3,3,4,4,
    3,4,4,4,4,4,3,4,4,2,3,4,2,6,4,4,
    4,4,3,4,3,4,4,6,4,4,4,4,2,4,5,
};

/* Tiny5 Regular. Bit 0 is the top row, bit 6 is the descender. Advance is FONT_ADV. */
static const uint8_t FONT_NORMAL[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
    {0x2e, 0x00, 0x00, 0x00, 0x00}, /* ! */
    {0x06, 0x00, 0x06, 0x00, 0x00}, /* " */
    {0x14, 0x3e, 0x14, 0x3e, 0x14}, /* # */
    {0x28, 0x6e, 0x14, 0x00, 0x00}, /* $ */
    {0x24, 0x10, 0x08, 0x24, 0x00}, /* % */
    {0x14, 0x2a, 0x14, 0x20, 0x00}, /* & */
    {0x06, 0x00, 0x00, 0x00, 0x00}, /* ' */
    {0x3e, 0x41, 0x00, 0x00, 0x00}, /* ( */
    {0x41, 0x3e, 0x00, 0x00, 0x00}, /* ) */
    {0x14, 0x0e, 0x14, 0x00, 0x00}, /* * */
    {0x08, 0x1c, 0x08, 0x00, 0x00}, /* + */
    {0x60, 0x00, 0x00, 0x00, 0x00}, /* , */
    {0x08, 0x08, 0x08, 0x00, 0x00}, /* - */
    {0x20, 0x00, 0x00, 0x00, 0x00}, /* . */
    {0x38, 0x06, 0x00, 0x00, 0x00}, /* / */
    {0x1c, 0x22, 0x1c, 0x00, 0x00}, /* 0 */
    {0x04, 0x3e, 0x00, 0x00, 0x00}, /* 1 */
    {0x32, 0x2a, 0x24, 0x00, 0x00}, /* 2 */
    {0x22, 0x2a, 0x16, 0x00, 0x00}, /* 3 */
    {0x18, 0x14, 0x3e, 0x00, 0x00}, /* 4 */
    {0x2e, 0x2a, 0x12, 0x00, 0x00}, /* 5 */
    {0x1c, 0x2a, 0x12, 0x00, 0x00}, /* 6 */
    {0x02, 0x3a, 0x06, 0x00, 0x00}, /* 7 */
    {0x14, 0x2a, 0x14, 0x00, 0x00}, /* 8 */
    {0x24, 0x2a, 0x1c, 0x00, 0x00}, /* 9 */
    {0x24, 0x00, 0x00, 0x00, 0x00}, /* : */
    {0x64, 0x00, 0x00, 0x00, 0x00}, /* ; */
    {0x08, 0x14, 0x22, 0x00, 0x00}, /* < */
    {0x14, 0x14, 0x14, 0x00, 0x00}, /* = */
    {0x22, 0x14, 0x08, 0x00, 0x00}, /* > */
    {0x02, 0x2a, 0x04, 0x00, 0x00}, /* ? */
    {0x3c, 0x42, 0x5a, 0x1c, 0x00}, /* @ */
    {0x3c, 0x0a, 0x0a, 0x3c, 0x00}, /* A */
    {0x3e, 0x2a, 0x2a, 0x14, 0x00}, /* B */
    {0x1c, 0x22, 0x22, 0x22, 0x00}, /* C */
    {0x3e, 0x22, 0x22, 0x1c, 0x00}, /* D */
    {0x3e, 0x2a, 0x2a, 0x22, 0x00}, /* E */
    {0x3e, 0x0a, 0x0a, 0x02, 0x00}, /* F */
    {0x1c, 0x22, 0x2a, 0x3a, 0x00}, /* G */
    {0x3e, 0x08, 0x08, 0x3e, 0x00}, /* H */
    {0x3e, 0x00, 0x00, 0x00, 0x00}, /* I */
    {0x10, 0x20, 0x20, 0x1e, 0x00}, /* J */
    {0x3e, 0x08, 0x14, 0x22, 0x00}, /* K */
    {0x3e, 0x20, 0x20, 0x20, 0x00}, /* L */
    {0x3e, 0x04, 0x08, 0x04, 0x3e}, /* M */
    {0x3e, 0x04, 0x08, 0x3e, 0x00}, /* N */
    {0x1c, 0x22, 0x22, 0x1c, 0x00}, /* O */
    {0x3e, 0x0a, 0x0a, 0x04, 0x00}, /* P */
    {0x1c, 0x22, 0x62, 0x5c, 0x00}, /* Q */
    {0x3e, 0x0a, 0x0a, 0x34, 0x00}, /* R */
    {0x24, 0x2a, 0x2a, 0x12, 0x00}, /* S */
    {0x02, 0x3e, 0x02, 0x00, 0x00}, /* T */
    {0x1e, 0x20, 0x20, 0x1e, 0x00}, /* U */
    {0x06, 0x18, 0x20, 0x18, 0x06}, /* V */
    {0x0e, 0x30, 0x0e, 0x30, 0x0e}, /* W */
    {0x22, 0x14, 0x08, 0x14, 0x22}, /* X */
    {0x02, 0x04, 0x38, 0x04, 0x02}, /* Y */
    {0x32, 0x2a, 0x26, 0x22, 0x00}, /* Z */
    {0x7f, 0x41, 0x00, 0x00, 0x00}, /* [ */
    {0x06, 0x38, 0x00, 0x00, 0x00}, /* \\ */
    {0x41, 0x7f, 0x00, 0x00, 0x00}, /* ] */
    {0x04, 0x02, 0x04, 0x00, 0x00}, /* ^ */
    {0x40, 0x40, 0x40, 0x00, 0x00}, /* _ */
    {0x01, 0x02, 0x00, 0x00, 0x00}, /* ` */
    {0x18, 0x24, 0x3c, 0x00, 0x00}, /* a */
    {0x3e, 0x24, 0x18, 0x00, 0x00}, /* b */
    {0x18, 0x24, 0x24, 0x00, 0x00}, /* c */
    {0x18, 0x24, 0x3e, 0x00, 0x00}, /* d */
    {0x18, 0x2c, 0x28, 0x00, 0x00}, /* e */
    {0x3c, 0x0a, 0x00, 0x00, 0x00}, /* f */
    {0x58, 0x64, 0x3c, 0x00, 0x00}, /* g */
    {0x3e, 0x04, 0x38, 0x00, 0x00}, /* h */
    {0x3d, 0x00, 0x00, 0x00, 0x00}, /* i */
    {0x40, 0x3d, 0x00, 0x00, 0x00}, /* j */
    {0x3e, 0x08, 0x34, 0x00, 0x00}, /* k */
    {0x3e, 0x00, 0x00, 0x00, 0x00}, /* l */
    {0x3c, 0x04, 0x3c, 0x04, 0x38}, /* m */
    {0x3c, 0x04, 0x38, 0x00, 0x00}, /* n */
    {0x18, 0x24, 0x18, 0x00, 0x00}, /* o */
    {0x7c, 0x24, 0x18, 0x00, 0x00}, /* p */
    {0x18, 0x24, 0x7c, 0x00, 0x00}, /* q */
    {0x3c, 0x04, 0x00, 0x00, 0x00}, /* r */
    {0x28, 0x2c, 0x14, 0x00, 0x00}, /* s */
    {0x1e, 0x24, 0x00, 0x00, 0x00}, /* t */
    {0x1c, 0x20, 0x3c, 0x00, 0x00}, /* u */
    {0x0c, 0x30, 0x0c, 0x00, 0x00}, /* v */
    {0x0c, 0x30, 0x0c, 0x30, 0x0c}, /* w */
    {0x34, 0x08, 0x34, 0x00, 0x00}, /* x */
    {0x5c, 0x20, 0x1c, 0x00, 0x00}, /* y */
    {0x34, 0x2c, 0x24, 0x00, 0x00}, /* z */
    {0x08, 0x77, 0x41, 0x00, 0x00}, /* { */
    {0x7f, 0x00, 0x00, 0x00, 0x00}, /* | */
    {0x41, 0x77, 0x08, 0x00, 0x00}, /* } */
    {0x08, 0x04, 0x08, 0x04, 0x00}, /* ~ */
};

/* Tall face, same advances as Tiny5. Bit 0 is the top row, bit 7 is a descender. */
static const uint8_t FONT_TALL[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
    {0x4f, 0x00, 0x00, 0x00, 0x00}, /* ! */
    {0x07, 0x00, 0x07, 0x00, 0x00}, /* " */
    {0x36, 0x7f, 0x36, 0x7f, 0x36}, /* # */
    {0x48, 0xcf, 0x36, 0x00, 0x00}, /* $ */
    {0x46, 0x30, 0x08, 0x46, 0x00}, /* % */
    {0x36, 0x49, 0x36, 0x40, 0x00}, /* & */
    {0x07, 0x00, 0x00, 0x00, 0x00}, /* ' */
    {0x3e, 0x41, 0x00, 0x00, 0x00}, /* ( */
    {0x41, 0x3e, 0x00, 0x00, 0x00}, /* ) */
    {0x36, 0x0f, 0x36, 0x00, 0x00}, /* * */
    {0x08, 0x3e, 0x08, 0x00, 0x00}, /* + */
    {0xc0, 0x00, 0x00, 0x00, 0x00}, /* , */
    {0x08, 0x08, 0x08, 0x00, 0x00}, /* - */
    {0x40, 0x00, 0x00, 0x00, 0x00}, /* . */
    {0x78, 0x07, 0x00, 0x00, 0x00}, /* / */
    {0x3e, 0x41, 0x3e, 0x00, 0x00}, /* 0 */
    {0x06, 0x7f, 0x00, 0x00, 0x00}, /* 1 */
    {0x71, 0x49, 0x46, 0x00, 0x00}, /* 2 */
    {0x41, 0x49, 0x37, 0x00, 0x00}, /* 3 */
    {0x38, 0x36, 0x7f, 0x00, 0x00}, /* 4 */
    {0x4f, 0x49, 0x31, 0x00, 0x00}, /* 5 */
    {0x3e, 0x49, 0x31, 0x00, 0x00}, /* 6 */
    {0x01, 0x79, 0x07, 0x00, 0x00}, /* 7 */
    {0x36, 0x49, 0x36, 0x00, 0x00}, /* 8 */
    {0x46, 0x49, 0x3e, 0x00, 0x00}, /* 9 */
    {0x46, 0x00, 0x00, 0x00, 0x00}, /* : */
    {0xc6, 0x00, 0x00, 0x00, 0x00}, /* ; */
    {0x08, 0x36, 0x41, 0x00, 0x00}, /* < */
    {0x36, 0x36, 0x36, 0x00, 0x00}, /* = */
    {0x41, 0x36, 0x08, 0x00, 0x00}, /* > */
    {0x01, 0x49, 0x06, 0x00, 0x00}, /* ? */
    {0x7e, 0x81, 0xb9, 0x3e, 0x00}, /* @ */
    {0x7e, 0x09, 0x09, 0x7e, 0x00}, /* A */
    {0x7f, 0x49, 0x49, 0x36, 0x00}, /* B */
    {0x3e, 0x41, 0x41, 0x22, 0x00}, /* C */
    {0x7f, 0x41, 0x41, 0x3e, 0x00}, /* D */
    {0x7f, 0x49, 0x49, 0x41, 0x00}, /* E */
    {0x7f, 0x09, 0x09, 0x01, 0x00}, /* F */
    {0x3e, 0x41, 0x49, 0x79, 0x00}, /* G */
    {0x7f, 0x08, 0x08, 0x7f, 0x00}, /* H */
    {0x7f, 0x00, 0x00, 0x00, 0x00}, /* I */
    {0x20, 0x40, 0x3f, 0x00, 0x00}, /* J */
    {0x7f, 0x08, 0x14, 0x62, 0x00}, /* K */
    {0x7f, 0x40, 0x40, 0x00, 0x00}, /* L */
    {0x7f, 0x02, 0x04, 0x02, 0x7f}, /* M */
    {0x7f, 0x04, 0x08, 0x7f, 0x00}, /* N */
    {0x3e, 0x41, 0x41, 0x3e, 0x00}, /* O */
    {0x7f, 0x09, 0x09, 0x06, 0x00}, /* P */
    {0x3e, 0x41, 0x21, 0x5e, 0x00}, /* Q */
    {0x7f, 0x09, 0x09, 0x76, 0x00}, /* R */
    {0x46, 0x49, 0x49, 0x31, 0x00}, /* S */
    {0x01, 0x7f, 0x01, 0x00, 0x00}, /* T */
    {0x3f, 0x40, 0x40, 0x3f, 0x00}, /* U */
    {0x1f, 0x60, 0x1f, 0x00, 0x00}, /* V */
    {0x1f, 0x60, 0x1f, 0x60, 0x1f}, /* W */
    {0x77, 0x08, 0x77, 0x00, 0x00}, /* X */
    {0x0f, 0x70, 0x0f, 0x00, 0x00}, /* Y */
    {0x71, 0x49, 0x47, 0x00, 0x00}, /* Z */
    {0x7f, 0x41, 0x00, 0x00, 0x00}, /* [ */
    {0x07, 0x78, 0x00, 0x00, 0x00}, /* \\ */
    {0x41, 0x7f, 0x00, 0x00, 0x00}, /* ] */
    {0x06, 0x01, 0x06, 0x00, 0x00}, /* ^ */
    {0x80, 0x80, 0x80, 0x00, 0x00}, /* _ */
    {0x01, 0x02, 0x00, 0x00, 0x00}, /* ` */
    {0x38, 0x44, 0x7c, 0x00, 0x00}, /* a */
    {0x7f, 0x44, 0x38, 0x00, 0x00}, /* b */
    {0x38, 0x44, 0x44, 0x00, 0x00}, /* c */
    {0x38, 0x44, 0x7f, 0x00, 0x00}, /* d */
    {0x38, 0x54, 0x48, 0x00, 0x00}, /* e */
    {0x7e, 0x09, 0x00, 0x00, 0x00}, /* f */
    {0x98, 0xa4, 0x7c, 0x00, 0x00}, /* g */
    {0x7f, 0x04, 0x78, 0x00, 0x00}, /* h */
    {0x7d, 0x00, 0x00, 0x00, 0x00}, /* i */
    {0xfd, 0x00, 0x00, 0x00, 0x00}, /* j */
    {0x7f, 0x08, 0x76, 0x00, 0x00}, /* k */
    {0x7f, 0x00, 0x00, 0x00, 0x00}, /* l */
    {0x7c, 0x04, 0x7c, 0x04, 0x78}, /* m */
    {0x7c, 0x04, 0x78, 0x00, 0x00}, /* n */
    {0x38, 0x44, 0x38, 0x00, 0x00}, /* o */
    {0xfc, 0x44, 0x38, 0x00, 0x00}, /* p */
    {0x38, 0x44, 0xfc, 0x00, 0x00}, /* q */
    {0x7c, 0x04, 0x00, 0x00, 0x00}, /* r */
    {0x48, 0x54, 0x24, 0x00, 0x00}, /* s */
    {0x3f, 0x44, 0x00, 0x00, 0x00}, /* t */
    {0x3c, 0x40, 0x7c, 0x00, 0x00}, /* u */
    {0x1c, 0x60, 0x1c, 0x00, 0x00}, /* v */
    {0x1c, 0x60, 0x1c, 0x60, 0x1c}, /* w */
    {0x6c, 0x10, 0x6c, 0x00, 0x00}, /* x */
    {0xbc, 0x40, 0x3c, 0x00, 0x00}, /* y */
    {0x64, 0x54, 0x4c, 0x00, 0x00}, /* z */
    {0x08, 0x77, 0x41, 0x00, 0x00}, /* { */
    {0x7f, 0x00, 0x00, 0x00, 0x00}, /* | */
    {0x41, 0x77, 0x08, 0x00, 0x00}, /* } */
    {0x08, 0x06, 0x08, 0x06, 0x00}, /* ~ */
};


static const uint8_t GLYPH_HEART_NORMAL[5] = {0x0c, 0x1e, 0x3c, 0x1e, 0x0c};
static const uint8_t GLYPH_HEART_TALL[5] = {0x0e, 0x3f, 0x7e, 0x3f, 0x0e};
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

static int glyph_advance(uint32_t cp) {
  if (cp == CP_HEART || cp == CP_GEAR) return 6;
  if (cp >= 32 && cp <= 126) return FONT_ADV[(unsigned)(cp - 32)];
  return 6;
}

static const uint8_t *glyph_cols(uint32_t cp, bool tall) {
  if (cp == CP_HEART) return tall ? GLYPH_HEART_TALL : GLYPH_HEART_NORMAL;
  if (cp == CP_GEAR) return GLYPH_GEAR;
  if (cp >= 32 && cp <= 126) {
    unsigned i = (unsigned)(cp - 32);
    return tall ? FONT_TALL[i] : FONT_NORMAL[i];
  }
  return GLYPH_FALLBACK;
}

int display_glyph_ink(uint32_t cp) {
  const uint8_t *cols = glyph_cols(cp, false);
  int n = 0;
  for (int x = 0; x < 5; x++) {
    if (cols[x]) n = x + 1;
  }
  return n > 0 ? n : 1;
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

void display_draw_rows(uint8_t *buf, int x, int y, const char *const *rows, int n) {
  for (int row = 0; row < n; row++) {
    for (int col = 0; rows[row][col]; col++) {
      if (rows[row][col] == '#') display_set_pixel(buf, x + col, y + row, true);
    }
  }
}

void display_draw_line(uint8_t *buf, int x0, int y0, int x1, int y1) {
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
  display_draw_line(buf, x0, y0, x1, y1);
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
  display_draw_line(buf, x1, y1, bx + px, by + py);
  display_draw_line(buf, x1, y1, bx - px, by - py);
}

static void draw_glyph(uint8_t *buf, int x, int y, const uint8_t *cols, int rows) {
  for (int cx = 0; cx < 5; cx++) {
    uint8_t col = cols[cx];
    for (int cy = 0; cy < rows; cy++) {
      if (col & (1u << cy)) display_set_pixel(buf, x + cx, y + cy, true);
    }
  }
}

void display_draw_char(uint8_t *buf, int x, int y, uint32_t cp) {
  draw_glyph(buf, x, y, glyph_cols(cp, true), 8);
}

static void draw_text(uint8_t *buf, int x, int y, const char *s, bool tall) {
  int cx = x;
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  while (*p) {
    if (cx >= DISPLAY_WIDTH) break;
    uint32_t cp;
    int k = display_utf8_next(p, &cp);
    if (k <= 0) break;
    draw_glyph(buf, cx, y, glyph_cols(cp, tall), tall ? 8 : 7);
    cx += glyph_advance(cp);
    p += k;
  }
}

void display_draw_text(uint8_t *buf, int x, int y, const char *s) {
  draw_text(buf, x, y, s, true);
}

int display_text_width(const char *s) {
  int w = 0;
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  while (*p) {
    uint32_t cp;
    int k = display_utf8_next(p, &cp);
    if (k <= 0) break;
    w += glyph_advance(cp);
    p += k;
  }
  return w;
}

void display_draw_text_small(uint8_t *buf, int x, int y, const char *s) {
  draw_text(buf, x, y, s, false);
}

int display_text_width_small(const char *s) {
  return display_text_width(s);
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
    int adv = glyph_advance(cp);
    if (x + adv > DISPLAY_WIDTH - 2) break;
    if (n + k >= (int)sizeof shown) break;
    for (int i = 0; i < k; i++) shown[n++] = (char)p[i];
    shown[n] = 0;
    x += adv;
    p += k;
  }
  display_draw_text(buf, 4, 1, shown);
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

void display_clip_px(char *dst, size_t cap, const char *s, int max_px) {
  if (!dst || cap == 0) return;
  dst[0] = 0;
  if (!s || max_px <= 0) return;
  if (display_text_width(s) <= max_px) {
    const unsigned char *p = (const unsigned char *)s;
    size_t o = 0;
    while (*p) {
      uint32_t cp;
      int k = display_utf8_next(p, &cp);
      if (k <= 0) break;
      if (o + (size_t)k + 1 >= cap) break;
      memcpy(dst + o, p, (size_t)k);
      o += (size_t)k;
      p += k;
    }
    dst[o] = 0;
    return;
  }
  int dot = glyph_advance('.');
  int budget = max_px - dot;
  if (budget < 0) budget = 0;
  const unsigned char *p = (const unsigned char *)s;
  size_t o = 0;
  int used = 0;
  while (*p) {
    uint32_t cp;
    int k = display_utf8_next(p, &cp);
    if (k <= 0) break;
    int adv = glyph_advance(cp);
    if (used + adv > budget) break;
    if (o + (size_t)k + 2 >= cap) break;
    memcpy(dst + o, p, (size_t)k);
    o += (size_t)k;
    used += adv;
    p += k;
  }
  if (o + 2 <= cap) dst[o++] = '.';
  dst[o] = 0;
}

int display_wrap(const char *s, int max_px, char out[][DISPLAY_WIDTH + 1], int max_out) {
  if (!s || max_px <= 0 || max_out <= 0) return 0;
  if (s[0] == 0) return 0;
  int nlines = 0;
  const unsigned char *p = (const unsigned char *)s;
  while (*p && nlines < max_out) {
    const unsigned char *nl = p;
    while (*nl && *nl != '\n') nl++;
    int part_len = (int)(nl - p);
    if (part_len == 0) {
      out[nlines][0] = 0;
      nlines++;
    } else {
      const unsigned char *rest = p;
      int rest_len = part_len;
      while (rest_len > 0 && nlines < max_out) {
        int px = 0;
        int i = 0;
        int space_at = -1;
        int fit = 0;
        int stopped_on_space = 0;
        while (i < rest_len) {
          uint32_t cp;
          int k = display_utf8_next(rest + i, &cp);
          if (k <= 0 || i + k > rest_len) break;
          int adv = glyph_advance(cp);
          if (fit > 0 && px + adv > max_px) {
            stopped_on_space = cp == ' ';
            break;
          }
          px += adv;
          if (cp == ' ') space_at = i;
          i += k;
          fit = i;
          if (px > max_px) break;
        }
        if (fit >= rest_len && px <= max_px) {
          int copy = rest_len > DISPLAY_WIDTH ? DISPLAY_WIDTH : rest_len;
          memcpy(out[nlines], rest, (size_t)copy);
          out[nlines][copy] = 0;
          nlines++;
          break;
        }
        int cut;
        if (stopped_on_space) cut = fit;
        else if (space_at > 0) cut = space_at;
        else if (fit > 0 && px <= max_px) cut = fit;
        else {
          uint32_t cp;
          int k = display_utf8_next(rest, &cp);
          cut = k > 0 ? k : 1;
        }
        if (cut > rest_len) cut = rest_len;
        if (cut > DISPLAY_WIDTH) cut = DISPLAY_WIDTH;
        if (cut < 1) cut = 1;
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

void display_slide(uint8_t *dst, const uint8_t *from, const uint8_t *to, int shift) {
  if (shift > DISPLAY_WIDTH) shift = DISPLAY_WIDTH;
  if (shift < -DISPLAY_WIDTH) shift = -DISPLAY_WIDTH;
  for (int page = 0; page < DISPLAY_PAGES; page++) {
    const uint8_t *f = from + page * DISPLAY_WIDTH;
    const uint8_t *t = to + page * DISPLAY_WIDTH;
    uint8_t *d = dst + page * DISPLAY_WIDTH;
    if (shift > 0) {
      if (shift < DISPLAY_WIDTH) memcpy(d, f + shift, (size_t)(DISPLAY_WIDTH - shift));
      memcpy(d + (DISPLAY_WIDTH - shift), t, (size_t)shift);
    } else if (shift < 0) {
      int s = -shift;
      memcpy(d, t + (DISPLAY_WIDTH - s), (size_t)s);
      if (s < DISPLAY_WIDTH) memcpy(d + s, f, (size_t)(DISPLAY_WIDTH - s));
    } else {
      memcpy(d, f, DISPLAY_WIDTH);
    }
  }
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
