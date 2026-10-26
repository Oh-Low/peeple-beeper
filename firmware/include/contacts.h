#pragma once

#include "beeple.h"
#include "crypto.h"
#include "hal.h"
#include "identity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONTACTS_MAX 256

typedef struct {
  uint8_t node_id[NODE_ID_LEN];
  char name[PAGE_FROM_MAX + 1];
  uint8_t session_key[SESSION_KEY_LEN];
  uint32_t paired_ms;
  /* Their mascot. BEEPLE_NONE until HELLO or a page says otherwise. */
  uint8_t beeple;
} contact_t;

typedef struct {
  contact_t rows[CONTACTS_MAX];
  int n;
} contact_book_t;

void contacts_init(contact_book_t *book);
void contacts_reset(contact_book_t *book);
const contact_t *contacts_find_by_id(const contact_book_t *book, const uint8_t *node_id);
bool contacts_upsert(contact_book_t *book, const contact_t *c);
/* Write a known mascot onto an existing row. Unknown indexes are ignored. */
bool contacts_set_beeple(contact_book_t *book, const uint8_t *node_id, uint8_t beeple);
int contacts_list(const contact_book_t *book, contact_t *out, int max);
bool contacts_delete(contact_book_t *book, const uint8_t *node_id);
