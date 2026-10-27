#pragma once

#include "hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NODE_ID_LEN 8

typedef struct {
  uint8_t node_id[NODE_ID_LEN];
  bool has_id;
  char display_name[PAGE_FROM_MAX + 1];
} identity_store_t;

void identity_init(identity_store_t *store);
void identity_load_or_create(identity_store_t *store, const hal_t *hal);
void identity_get_node_id(const identity_store_t *store, uint8_t out[NODE_ID_LEN]);
void identity_reset(identity_store_t *store);
void identity_get_display_name(const identity_store_t *store, char *out, size_t cap);
void identity_blank_name(const uint8_t *node_id, char *out, size_t cap);
void identity_set_display_name(identity_store_t *store, const char *name);
bool node_id_equal(const uint8_t *a, const uint8_t *b);
void node_id_to_hex(const uint8_t *id, size_t len, char *out, size_t cap);
