#include "canned.h"

#include "util.h"

#include <ctype.h>
#include <string.h>

const char *const CANNED_DEFAULTS[CANNED_DEFAULT_N] = {
    "On my way",
    "Running late",
    "Yes",
    "No",
    "Where are you?",
    "Call me",
};

void canned_init(canned_book_t *book) {
  memset(book, 0, sizeof *book);
}

void canned_reset(canned_book_t *book) {
  canned_init(book);
}

static void rtrim_copy(char *dst, size_t cap, const char *src, size_t max_chars) {
  str_copy_max(dst, cap, src ? src : "", max_chars);
  size_t n = str_ascii_len(dst);
  while (n > 0 && isspace((unsigned char)dst[n - 1])) {
    dst[--n] = 0;
  }
}

int canned_list(const canned_book_t *book, char out[][PAGE_BODY_MAX + 1], int max) {
  int n = book->n + CANNED_DEFAULT_N;
  int w = 0;
  for (int i = 0; i < book->n && w < max; i++) {
    str_copy_max(out[w], PAGE_BODY_MAX + 1, book->saved[i], PAGE_BODY_MAX);
    w++;
  }
  for (int i = 0; i < CANNED_DEFAULT_N && w < max; i++) {
    str_copy_max(out[w], PAGE_BODY_MAX + 1, CANNED_DEFAULTS[i], PAGE_BODY_MAX);
    w++;
  }
  (void)n;
  return book->n + CANNED_DEFAULT_N;
}

bool canned_add(canned_book_t *book, const char *body) {
  char text[PAGE_BODY_MAX + 1];
  rtrim_copy(text, sizeof text, body, PAGE_BODY_MAX);
  if (text[0] == 0) return false;

  for (int i = 0; i < CANNED_DEFAULT_N; i++) {
    if (strcmp(CANNED_DEFAULTS[i], text) == 0) return true;
  }

  int found = -1;
  for (int i = 0; i < book->n; i++) {
    if (strcmp(book->saved[i], text) == 0) {
      found = i;
      break;
    }
  }
  if (found >= 0) {
    if (found + 1 < book->n) {
      memmove(&book->saved[found], &book->saved[found + 1],
              (size_t)(book->n - found - 1) * sizeof book->saved[0]);
    }
    book->n--;
  } else if (book->n >= CANNED_SAVED_MAX) {
    book->n--;
  }
  if (book->n > 0) {
    memmove(&book->saved[1], &book->saved[0], (size_t)book->n * sizeof book->saved[0]);
  }
  str_copy_max(book->saved[0], sizeof book->saved[0], text, PAGE_BODY_MAX);
  book->n++;
  return true;
}
