#include "identity.h"

#include "util.h"

#include <string.h>

static const char HEX[] = "0123456789abcdef";

void identity_init(identity_store_t *store) {
  memset(store, 0, sizeof *store);
}

void identity_load_or_create(identity_store_t *store, const hal_t *hal) {
  if (store->has_id) return;
  hal->random_fill(hal->ctx, store->node_id, NODE_ID_LEN);
  store->has_id = true;
}

void identity_get_node_id(const identity_store_t *store, uint8_t out[NODE_ID_LEN]) {
  memcpy(out, store->node_id, NODE_ID_LEN);
}

void identity_reset(identity_store_t *store) {
  memset(store, 0, sizeof *store);
}

void node_id_to_hex(const uint8_t *id, size_t len, char *out, size_t cap) {
  if (!out || cap == 0) return;
  size_t o = 0;
  for (size_t i = 0; i < len && o + 2 < cap; i++) {
    out[o++] = HEX[id[i] >> 4];
    out[o++] = HEX[id[i] & 15];
  }
  out[o] = 0;
}

void identity_blank_name(const uint8_t *node_id, char *out, size_t cap) {
  if (!out || cap == 0) return;
  char hex[NODE_ID_LEN * 2 + 1];
  node_id_to_hex(node_id, NODE_ID_LEN, hex, sizeof hex);
  size_t n = str_ascii_len(hex);
  const char *tail = n >= 4 ? hex + n - 4 : hex;
  str_copy_max(out, cap, tail, 4);
}

void identity_get_display_name(const identity_store_t *store, char *out, size_t cap) {
  char clipped[PAGE_FROM_MAX + 1];
  str_copy_max(clipped, sizeof clipped, store->display_name, PAGE_FROM_MAX);
  if (clipped[0]) {
    str_copy_max(out, cap, clipped, PAGE_FROM_MAX);
    return;
  }
  if (!store->has_id) {
    str_copy_max(out, cap, "", 0);
    return;
  }
  identity_blank_name(store->node_id, out, cap);
}

void identity_set_display_name(identity_store_t *store, const char *name) {
  size_t o = 0;
  if (name) {
    for (size_t i = 0; name[i] && o < PAGE_FROM_MAX; i++) {
      unsigned char code = (unsigned char)name[i];
      if (code >= 32 && code <= 126) store->display_name[o++] = (char)code;
    }
  }
  store->display_name[o] = 0;
}

bool node_id_equal(const uint8_t *a, const uint8_t *b) {
  return bytes_eq_n(a, b, NODE_ID_LEN);
}
