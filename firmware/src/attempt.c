#include "attempt.h"

#include "util.h"

#include <string.h>

static int budget_of(attempt_kind_t kind) {
  return kind == ATTEMPT_LOCATE ? ATTEMPT_LOCATE_BUDGET : ATTEMPT_PAGE_BUDGET;
}

static uint32_t wait_after(const attempt_t *a) {
  if (a->kind == ATTEMPT_LOCATE) {
    if (a->attempts + 1 >= ATTEMPT_LOCATE_BUDGET) return ATTEMPT_LOCATE_MS + ATTEMPT_LOCATE_TAIL_MS;
    return ATTEMPT_LOCATE_MS;
  }
  if (a->attempts + 1 < ATTEMPT_PAGE_FAST) return ATTEMPT_PAGE_FAST_MS;
  return ATTEMPT_PAGE_SLOW_MS;
}

static void note_sent(attempt_t *a, uint32_t now) {
  uint32_t wait = wait_after(a);
  a->attempts += 1;
  a->next_at = now + wait;
}

typedef enum { DUE_WAIT = 0, DUE_SEND, DUE_STOP } due_t;

static due_t when_due(const attempt_t *a, uint32_t now) {
  if (!a->in_flight || !time_reached(now, a->next_at)) return DUE_WAIT;
  if (a->attempts >= budget_of(a->kind)) return DUE_STOP;
  return DUE_SEND;
}

void attempt_arm(attempt_t *a, attempt_kind_t kind) {
  memset(a, 0, sizeof *a);
  a->kind = kind;
  a->in_flight = true;
}

attempt_step_t attempt_step(attempt_t *a, uint32_t now, attempt_send_fn send, void *user) {
  due_t step = when_due(a, now);
  if (step == DUE_WAIT) return ATTEMPT_WAIT;
  if (step == DUE_STOP) {
    a->in_flight = false;
    return ATTEMPT_DONE;
  }
  if (!send(user)) {
    a->in_flight = false;
    return ATTEMPT_DONE;
  }
  note_sent(a, now);
  return ATTEMPT_SENT;
}

bool attempt_remember(attempt_t *a, const uint8_t *nonce) {
  if (!nonce || a->nonce_n >= budget_of(a->kind) || a->nonce_n >= ATTEMPT_NONCE_MAX) return false;
  memcpy(a->nonces[a->nonce_n], nonce, PAGE_NONCE_LEN);
  a->nonce_n++;
  return true;
}

static bool attempt_has(const attempt_t *a, const uint8_t *nonce) {
  if (!nonce) return false;
  for (int i = 0; i < a->nonce_n; i++) {
    if (bytes_eq_n(a->nonces[i], nonce, PAGE_NONCE_LEN)) return true;
  }
  return false;
}

void attempt_stop(attempt_t *a) { a->in_flight = false; }

int attempt_budget(const attempt_t *a) { return budget_of(a->kind); }

bool attempt_first_wait_done(const attempt_t *a, uint32_t now) {
  if (!a->in_flight || a->attempts < 1) return false;
  if (a->attempts > 1) return true;
  return time_reached(now, a->next_at);
}

bool attempt_from_peer(const attempt_t *a, const uint8_t *our_id, const uint8_t *frame_dest,
                       const uint8_t *peer_id, const uint8_t *frame_src, const uint8_t *nonce) {
  if (!a->in_flight) return false;
  if (!node_id_equal(frame_dest, our_id)) return false;
  if (!node_id_equal(frame_src, peer_id)) return false;
  return attempt_has(a, nonce);
}
