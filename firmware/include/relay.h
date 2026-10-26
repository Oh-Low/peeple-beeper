#pragma once

#include "frames.h"
#include "hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RELAY_SEEN_MAX 24

typedef struct {
  int kind;
  uint8_t src_id[NODE_ID_LEN];
  uint8_t nonce[PAGE_NONCE_LEN];
  uint8_t pending[RAW_FRAME_MAX];
  uint8_t pending_len;
  bool has_pending;
  uint32_t pending_at;
} relay_seen_t;

typedef struct {
  relay_seen_t seen[RELAY_SEEN_MAX];
  int n;
} relay_cache_t;

void relay_mark_seen(relay_cache_t *cache, int kind, const uint8_t *src_id, const uint8_t *nonce);

void relay_on_request(relay_cache_t *cache, const hal_t *hal, int req_kind, int reply_kind,
                      const uint8_t *src_id, const uint8_t *dest_id, const uint8_t *nonce,
                      uint8_t hop_ttl, const uint8_t *bytes, size_t len);

void relay_on_reply(relay_cache_t *cache, const hal_t *hal, int req_kind, int reply_kind,
                    const uint8_t *src_id, const uint8_t *dest_id, const uint8_t *nonce,
                    uint8_t hop_ttl, const uint8_t *bytes, size_t len);

void relay_tick(relay_cache_t *cache, const hal_t *hal);
