#include "relay.h"

#include "util.h"

#include <string.h>

static uint32_t relay_jitter_ms(const hal_t *hal) {
  uint8_t r[2];
  hal->random_fill(hal->ctx, r, 2);
  uint32_t u = ((uint32_t)r[0] << 8) | r[1];
  uint32_t span = RELAY_JITTER_MAX_MS - RELAY_JITTER_MIN_MS;
  return RELAY_JITTER_MIN_MS + (u % (span + 1));
}

static uint32_t relay_deadline(const hal_t *hal) {
  return hal->now_ms(hal->ctx) + relay_jitter_ms(hal);
}

static relay_seen_t *seen_find(relay_cache_t *cache, int kind, const uint8_t *src_id,
                               const uint8_t *nonce) {
  for (int i = 0; i < cache->n; i++) {
    relay_seen_t *e = &cache->seen[i];
    if (e->kind == kind && node_id_equal(e->src_id, src_id) &&
        bytes_eq_n(e->nonce, nonce, PAGE_NONCE_LEN)) {
      return e;
    }
  }
  return NULL;
}

static relay_seen_t *seen_insert(relay_cache_t *cache, int kind, const uint8_t *src_id,
                                 const uint8_t *nonce) {
  if (cache->n >= RELAY_SEEN_MAX) {
    memmove(&cache->seen[0], &cache->seen[1], (size_t)(RELAY_SEEN_MAX - 1) * sizeof(relay_seen_t));
    cache->n = RELAY_SEEN_MAX - 1;
  }
  relay_seen_t *e = &cache->seen[cache->n++];
  memset(e, 0, sizeof *e);
  e->kind = kind;
  memcpy(e->src_id, src_id, NODE_ID_LEN);
  memcpy(e->nonce, nonce, PAGE_NONCE_LEN);
  return e;
}

void relay_mark_seen(relay_cache_t *cache, int kind, const uint8_t *src_id, const uint8_t *nonce) {
  if (seen_find(cache, kind, src_id, nonce)) return;
  seen_insert(cache, kind, src_id, nonce);
}

static void arm_relay(relay_seen_t *entry, const hal_t *hal, uint8_t hop_ttl, const uint8_t *bytes,
                      size_t len) {
  if (hop_ttl <= 1) return;
  if (len > RAW_FRAME_MAX) len = RAW_FRAME_MAX;
  memcpy(entry->pending, bytes, len);
  entry->pending_len = (uint8_t)len;
  if (!frame_set_hop_ttl(entry->pending, len, (uint8_t)(hop_ttl - 1))) {
    entry->has_pending = false;
    entry->pending_len = 0;
    return;
  }
  entry->has_pending = true;
  entry->pending_at = relay_deadline(hal);
}

static void reroll_pending(relay_seen_t *entry, const hal_t *hal) {
  if (!entry->has_pending) return;
  entry->pending_at = relay_deadline(hal);
}

static void arm_or_reroll(relay_cache_t *cache, const hal_t *hal, int kind, const uint8_t *src_id,
                          const uint8_t *nonce, uint8_t hop_ttl, const uint8_t *bytes, size_t len) {
  relay_seen_t *existing = seen_find(cache, kind, src_id, nonce);
  if (existing) {
    reroll_pending(existing, hal);
    return;
  }
  relay_seen_t *entry = seen_insert(cache, kind, src_id, nonce);
  arm_relay(entry, hal, hop_ttl, bytes, len);
}

void relay_on_request(relay_cache_t *cache, const hal_t *hal, int req_kind, int reply_kind,
                      const uint8_t *src_id, const uint8_t *dest_id, const uint8_t *nonce,
                      uint8_t hop_ttl, const uint8_t *bytes, size_t len) {
  if (seen_find(cache, reply_kind, dest_id, nonce)) {
    relay_mark_seen(cache, req_kind, src_id, nonce);
    return;
  }
  arm_or_reroll(cache, hal, req_kind, src_id, nonce, hop_ttl, bytes, len);
}

void relay_on_reply(relay_cache_t *cache, const hal_t *hal, int req_kind, int reply_kind,
                    const uint8_t *src_id, const uint8_t *dest_id, const uint8_t *nonce,
                    uint8_t hop_ttl, const uint8_t *bytes, size_t len) {
  relay_seen_t *pending = seen_find(cache, req_kind, dest_id, nonce);
  if (pending) {
    pending->has_pending = false;
    pending->pending_len = 0;
  }
  arm_or_reroll(cache, hal, reply_kind, src_id, nonce, hop_ttl, bytes, len);
}

void relay_tick(relay_cache_t *cache, const hal_t *hal) {
  uint32_t now = hal->now_ms(hal->ctx);
  for (int i = 0; i < cache->n; i++) {
    relay_seen_t *e = &cache->seen[i];
    if (!e->has_pending) continue;
    if (!time_reached(now, e->pending_at)) continue;
    hal->radio_send_raw(hal->ctx, e->pending, e->pending_len);
    e->has_pending = false;
    e->pending_len = 0;
  }
}
