#include "util.h"

size_t str_ascii_len(const char *s) {
  size_t n = 0;
  if (!s) return 0;
  while (s[n]) n++;
  return n;
}

void str_copy_max(char *dst, size_t dst_cap, const char *src, size_t max_chars) {
  if (!dst || dst_cap == 0) return;
  size_t o = 0;
  if (src) {
    while (src[o] && o + 1 < dst_cap && o < max_chars) {
      dst[o] = src[o];
      o++;
    }
  }
  dst[o] = 0;
}

void str_clip_ascii(char *dst, size_t dst_cap, const char *src, size_t max_chars) {
  if (!dst || dst_cap == 0) return;
  size_t n = str_ascii_len(src);
  if (n <= max_chars) {
    str_copy_max(dst, dst_cap, src, max_chars);
    return;
  }
  if (max_chars <= 1) {
    str_copy_max(dst, dst_cap, src, max_chars);
    return;
  }
  str_copy_max(dst, dst_cap, src, max_chars - 1);
  size_t L = str_ascii_len(dst);
  if (L + 1 < dst_cap) {
    dst[L] = '.';
    dst[L + 1] = 0;
  }
}
