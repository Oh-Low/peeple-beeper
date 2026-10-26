#include "contacts.h"

#include "beeple.h"
#include "util.h"

#include <string.h>

void contacts_init(contact_book_t *book) {
  memset(book, 0, sizeof *book);
}

void contacts_reset(contact_book_t *book) {
  contacts_init(book);
}

static int index_of(const contact_book_t *book, const uint8_t *node_id) {
  for (int i = 0; i < book->n; i++) {
    if (node_id_equal(book->rows[i].node_id, node_id)) return i;
  }
  return -1;
}

static void clamp_name(char *dst, const char *name, const uint8_t *node_id) {
  str_copy_max(dst, PAGE_FROM_MAX + 1, name ? name : "", PAGE_FROM_MAX);
  if (dst[0]) return;
  char hex[NODE_ID_LEN * 2 + 1];
  node_id_to_hex(node_id, NODE_ID_LEN, hex, sizeof hex);
  size_t n = str_ascii_len(hex);
  const char *tail = n >= 4 ? hex + n - 4 : hex;
  str_copy_max(dst, PAGE_FROM_MAX + 1, tail, 4);
}

const contact_t *contacts_find_by_id(const contact_book_t *book, const uint8_t *node_id) {
  int i = index_of(book, node_id);
  if (i < 0) return NULL;
  return &book->rows[i];
}

bool contacts_upsert(contact_book_t *book, const contact_t *c) {
  if (!book || !c) return false;
  contact_t row;
  memset(&row, 0, sizeof row);
  memcpy(row.node_id, c->node_id, NODE_ID_LEN);
  clamp_name(row.name, c->name, c->node_id);
  memcpy(row.session_key, c->session_key, SESSION_KEY_LEN);
  row.paired_ms = c->paired_ms;
  row.beeple = c->beeple < BEEPLE_N ? c->beeple : BEEPLE_NONE;

  int i = index_of(book, c->node_id);
  if (i >= 0) {
    book->rows[i] = row;
    return true;
  }
  if (book->n >= CONTACTS_MAX) return false;
  if (book->n > 0) {
    memmove(&book->rows[1], &book->rows[0], (size_t)book->n * sizeof(contact_t));
  }
  book->rows[0] = row;
  book->n++;
  return true;
}

bool contacts_set_beeple(contact_book_t *book, const uint8_t *node_id, uint8_t beeple) {
  if (!book || !node_id || beeple >= BEEPLE_N) return false;
  int i = index_of(book, node_id);
  if (i < 0) return false;
  book->rows[i].beeple = beeple;
  return true;
}

int contacts_list(const contact_book_t *book, contact_t *out, int max) {
  int n = book->n;
  if (n > max) n = max;
  if (out && n > 0) memcpy(out, book->rows, (size_t)n * sizeof(contact_t));
  return book->n;
}

bool contacts_delete(contact_book_t *book, const uint8_t *node_id) {
  int i = index_of(book, node_id);
  if (i < 0) return false;
  if (i + 1 < book->n) {
    memmove(&book->rows[i], &book->rows[i + 1], (size_t)(book->n - i - 1) * sizeof(contact_t));
  }
  book->n--;
  memset(&book->rows[book->n], 0, sizeof(contact_t));
  return true;
}
