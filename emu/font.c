#include "font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define ATLAS_W 1024
#define ATLAS_H 1024
#define ASCII_FIRST 32
#define ASCII_COUNT 95

static unsigned char *g_ttf;
static SDL_Texture *g_tex;
static stbtt_packedchar g_ascii[ASCII_COUNT];
static stbtt_packedchar g_extra[2];
static float g_dpi = 1.f;
static float g_ascent_px;

static int iround(float v) {
  return (int)(v >= 0.f ? v + 0.5f : v - 0.5f);
}

static uint32_t utf8_next(const char **s) {
  const unsigned char *p = (const unsigned char *)*s;
  if (*p == 0) return 0;
  if (*p < 0x80) {
    *s += 1;
    return *p;
  }
  if ((*p & 0xE0) == 0xC0 && p[1]) {
    uint32_t cp = ((uint32_t)(*p & 0x1F) << 6) | (p[1] & 0x3F);
    *s += 2;
    return cp;
  }
  if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
    uint32_t cp = ((uint32_t)(*p & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    *s += 3;
    return cp;
  }
  *s += 1;
  return '?';
}

static bool load_ttf(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long n = ftell(f);
  if (n <= 0) {
    fclose(f);
    return false;
  }
  rewind(f);
  unsigned char *buf = (unsigned char *)malloc((size_t)n);
  if (!buf) {
    fclose(f);
    return false;
  }
  size_t got = fread(buf, 1, (size_t)n, f);
  fclose(f);
  if (got != (size_t)n) {
    free(buf);
    return false;
  }
  free(g_ttf);
  g_ttf = buf;
  return true;
}

bool text_open(SDL_Renderer *r, const char *ttf_path, float dpi) {
  if (!g_ttf) {
    if (!ttf_path || !load_ttf(ttf_path)) return false;
  }
  if (dpi < 1.f) dpi = 1.f;

  unsigned char *alpha = (unsigned char *)calloc((size_t)ATLAS_W * ATLAS_H, 1);
  if (!alpha) return false;

  stbtt_pack_context pack;
  if (!stbtt_PackBegin(&pack, alpha, ATLAS_W, ATLAS_H, 0, 1, NULL)) {
    free(alpha);
    return false;
  }
  stbtt_PackSetOversampling(&pack, 1, 1);
  stbtt_PackSetSkipMissingCodepoints(&pack, 1);

  float px = 15.f * dpi;
  int extra_cp[2] = {0x00B0, 0x2212};
  stbtt_pack_range range[2];
  memset(range, 0, sizeof range);
  range[0].font_size = px;
  range[0].first_unicode_codepoint_in_range = ASCII_FIRST;
  range[0].num_chars = ASCII_COUNT;
  range[0].chardata_for_range = g_ascii;
  range[1].font_size = px;
  range[1].array_of_unicode_codepoints = extra_cp;
  range[1].num_chars = 2;
  range[1].chardata_for_range = g_extra;

  int ok = stbtt_PackFontRanges(&pack, g_ttf, 0, range, 2);
  stbtt_PackEnd(&pack);
  if (!ok) {
    free(alpha);
    return false;
  }

  float descent = 0.f, gap = 0.f;
  stbtt_GetScaledFontVMetrics(g_ttf, 0, px, &g_ascent_px, &descent, &gap);

  SDL_PixelFormat *fmt = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
  Uint32 *rgba = (Uint32 *)malloc((size_t)ATLAS_W * ATLAS_H * sizeof(Uint32));
  if (!fmt || !rgba) {
    SDL_FreeFormat(fmt);
    free(rgba);
    free(alpha);
    return false;
  }
  for (int i = 0; i < ATLAS_W * ATLAS_H; i++) rgba[i] = SDL_MapRGBA(fmt, 255, 255, 255, alpha[i]);
  SDL_FreeFormat(fmt);
  free(alpha);

  if (g_tex) SDL_DestroyTexture(g_tex);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  g_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, ATLAS_W, ATLAS_H);
  if (!g_tex) {
    free(rgba);
    return false;
  }
  SDL_UpdateTexture(g_tex, NULL, rgba, ATLAS_W * (int)sizeof(Uint32));
  free(rgba);
  SDL_SetTextureBlendMode(g_tex, SDL_BLENDMODE_BLEND);
  g_dpi = dpi;
  return true;
}

void text_close(void) {
  if (g_tex) SDL_DestroyTexture(g_tex);
  g_tex = NULL;
  free(g_ttf);
  g_ttf = NULL;
}

static void quad_for(uint32_t cp, float *xpos, float *ypos, stbtt_aligned_quad *q,
                     const stbtt_packedchar **out) {
  const stbtt_packedchar *table;
  int index;
  if (cp == 0x00B0) {
    table = g_extra;
    index = 0;
  } else if (cp == 0x2212) {
    table = g_extra;
    index = 1;
  } else {
    if (cp < ASCII_FIRST || cp >= ASCII_FIRST + ASCII_COUNT) cp = '?';
    table = g_ascii;
    index = (int)cp - ASCII_FIRST;
  }
  *out = &table[index];
  stbtt_GetPackedQuad(table, ATLAS_W, ATLAS_H, index, xpos, ypos, q, 1);
}

int text_width(const char *utf8) {
  if (!g_tex || !utf8 || !utf8[0] || g_dpi <= 0.f) return 0;
  float xpos = 0.f;
  float ypos = 0.f;
  const char *s = utf8;
  while (*s) {
    uint32_t cp = utf8_next(&s);
    if (cp == 0) break;
    stbtt_aligned_quad q;
    const stbtt_packedchar *b = NULL;
    quad_for(cp, &xpos, &ypos, &q, &b);
  }
  return iround(xpos / g_dpi);
}

void text_draw(SDL_Renderer *r, int x, int y, const char *utf8, uint8_t cr, uint8_t cg, uint8_t cb) {
  if (!g_tex || !utf8 || !utf8[0] || g_dpi <= 0.f) return;
  SDL_SetTextureColorMod(g_tex, cr, cg, cb);
  float xpos = (float)x * g_dpi;
  float ypos = (float)y * g_dpi + g_ascent_px;
  const char *s = utf8;
  while (*s) {
    uint32_t cp = utf8_next(&s);
    if (cp == 0) break;
    stbtt_aligned_quad q;
    const stbtt_packedchar *b = NULL;
    quad_for(cp, &xpos, &ypos, &q, &b);
    int sw = (int)b->x1 - (int)b->x0;
    int sh = (int)b->y1 - (int)b->y0;
    if (sw <= 0 || sh <= 0) continue;
    SDL_Rect src = {(int)b->x0, (int)b->y0, sw, sh};
    SDL_Rect dst = {iround(q.x0 / g_dpi), iround(q.y0 / g_dpi), iround((q.x1 - q.x0) / g_dpi),
                    iround((q.y1 - q.y0) / g_dpi)};
    if (dst.w <= 0 || dst.h <= 0) continue;
    SDL_RenderCopy(r, g_tex, &src, &dst);
  }
}
