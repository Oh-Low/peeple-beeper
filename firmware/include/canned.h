#pragma once

#include "hal.h"

#include <stdbool.h>
#include <stddef.h>

#define CANNED_SAVED_MAX 16
#define CANNED_DEFAULT_N 6

typedef struct {
  char saved[CANNED_SAVED_MAX][PAGE_BODY_MAX + 1];
  int n;
} canned_book_t;

extern const char *const CANNED_DEFAULTS[CANNED_DEFAULT_N];

void canned_init(canned_book_t *book);
void canned_reset(canned_book_t *book);
int canned_list(const canned_book_t *book, char out[][PAGE_BODY_MAX + 1], int max);
bool canned_add(canned_book_t *book, const char *body);
