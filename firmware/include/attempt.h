#pragma once

#include "frames.h"
#include "identity.h"

#include <stdbool.h>
#include <stdint.h>

#define ATTEMPT_PAGE_FAST 4
#define ATTEMPT_PAGE_SLOW 5
#define ATTEMPT_PAGE_BUDGET (ATTEMPT_PAGE_FAST + ATTEMPT_PAGE_SLOW)
#define ATTEMPT_PAGE_FAST_MS 10000u
#define ATTEMPT_PAGE_SLOW_MS 60000u
#define ATTEMPT_LOCATE_BUDGET 4
#define ATTEMPT_LOCATE_MS 2000u
#define ATTEMPT_NONCE_MAX ATTEMPT_PAGE_BUDGET

typedef enum {
  ATTEMPT_PAGE = 0,
  ATTEMPT_LOCATE,
} attempt_kind_t;

typedef struct {
  uint8_t nonces[ATTEMPT_NONCE_MAX][PAGE_NONCE_LEN];
  int nonce_n;
  int attempts;
  uint32_t next_at;
  bool in_flight;
  attempt_kind_t kind;
} attempt_t;

typedef enum {
  ATTEMPT_WAIT = 0,
  ATTEMPT_SENT,
  ATTEMPT_DONE,
} attempt_step_t;

typedef bool (*attempt_send_fn)(void *user);

void attempt_arm(attempt_t *a, attempt_kind_t kind);
attempt_step_t attempt_step(attempt_t *a, uint32_t now, attempt_send_fn send, void *user);
bool attempt_remember(attempt_t *a, const uint8_t *nonce);
void attempt_stop(attempt_t *a);
int attempt_budget(const attempt_t *a);
bool attempt_first_wait_done(const attempt_t *a, uint32_t now);
bool attempt_from_peer(const attempt_t *a, const uint8_t *our_id, const uint8_t *frame_dest,
                       const uint8_t *peer_id, const uint8_t *frame_src, const uint8_t *nonce);
