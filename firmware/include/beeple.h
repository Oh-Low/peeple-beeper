#pragma once

#include <stdint.h>

/* Shared mascot index. Both units have the same pictures, so only this byte
   is stored on a contact and sealed into a page. 255 means the sender has
   not told us yet. */
#define BEEPLE_N 12
#define BEEPLE_NONE 255

typedef struct {
  const char *name;
  const uint8_t *bits;
  int w;
  int h;
} beeple_def_t;

const beeple_def_t *beeple_at(int id);
