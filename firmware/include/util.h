#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline bool bytes_eq(const uint8_t *a, size_t na, const uint8_t *b, size_t nb) {
  if (na != nb) return false;
  for (size_t i = 0; i < na; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

static inline bool bytes_eq_n(const uint8_t *a, const uint8_t *b, size_t n) {
  return bytes_eq(a, n, b, n);
}

static inline int memcmp_u8(const uint8_t *a, size_t na, const uint8_t *b, size_t nb) {
  size_t n = na < nb ? na : nb;
  for (size_t i = 0; i < n; i++) {
    int d = (int)a[i] - (int)b[i];
    if (d != 0) return d;
  }
  if (na < nb) return -1;
  if (na > nb) return 1;
  return 0;
}

static inline uint32_t elapsed_u32(uint32_t now, uint32_t start) {
  return now - start;
}

static inline bool time_reached(uint32_t now, uint32_t deadline) {
  return elapsed_u32(now, deadline) < 0x80000000u;
}

void str_clip_ascii(char *dst, size_t dst_cap, const char *src, size_t max_chars);
size_t str_ascii_len(const char *s);
void str_copy_max(char *dst, size_t dst_cap, const char *src, size_t max_chars);
