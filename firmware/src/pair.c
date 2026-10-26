#include "pair.h"

#include "beeple.h"
#include "crypto.h"
#include "util.h"

#include <string.h>

void pair_init(pair_session_t *session) {
  memset(session, 0, sizeof *session);
}

static bool pair_bytes_eq(const uint8_t *a, size_t na, const uint8_t *b, size_t nb) {
  if (na != nb) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < na; i++) d = (uint8_t)(d | (a[i] ^ b[i]));
  return d == 0;
}

static bool confirm_for(const pair_session_t *session, uint8_t out[CONFIRM_LEN]) {
  if (!session->our_hello_len || !session->has_pending) return false;
  pair_confirm_hash(session->our_hello_bytes, session->our_hello_len,
                    session->pending.hello_bytes, session->pending.hello_len, out);
  return true;
}

static void clear_session(pair_session_t *session) {
  session->active = false;
  session->start_ms = 0;
  session->last_hello_at = 0;
  session->hello_sent = false;
  session->has_eph = false;
  session->our_hello_len = 0;
  session->has_pending = false;
  session->they_have_us = false;
  session->has_rssi = false;
}

static void pair_send_hello(pair_session_t *session, const hal_t *hal) {
  if (!session->our_hello_len) return;
  hal->radio_send_raw(hal->ctx, session->our_hello_bytes, session->our_hello_len);
}

static void pair_send_ack(pair_session_t *session, identity_store_t *identity, const hal_t *hal) {
  uint8_t confirm[CONFIRM_LEN];
  if (!confirm_for(session, confirm) || !session->has_pending) return;
  hello_ack_payload_t ack;
  memset(&ack, 0, sizeof ack);
  identity_get_node_id(identity, ack.src_id);
  memcpy(ack.dest_id, session->pending.hello.node_id, NODE_ID_LEN);
  memcpy(ack.confirm, confirm, CONFIRM_LEN);
  uint8_t buf[HELLO_ACK_LEN];
  size_t n = frame_encode_hello_ack(&ack, buf, sizeof buf);
  if (n) hal->radio_send_raw(hal->ctx, buf, n);
}

static void pair_send_leave(pair_session_t *session, identity_store_t *identity, const hal_t *hal) {
  uint8_t confirm[CONFIRM_LEN];
  if (!confirm_for(session, confirm) || !session->has_pending) return;
  pair_leave_payload_t leave;
  memset(&leave, 0, sizeof leave);
  identity_get_node_id(identity, leave.src_id);
  memcpy(leave.dest_id, session->pending.hello.node_id, NODE_ID_LEN);
  memcpy(leave.confirm, confirm, CONFIRM_LEN);
  uint8_t buf[HELLO_ACK_LEN];
  size_t n = frame_encode_pair_leave(&leave, buf, sizeof buf);
  if (n) hal->radio_send_raw(hal->ctx, buf, n);
}

static void pair_broadcast(pair_session_t *session, identity_store_t *identity, const hal_t *hal) {
  pair_send_hello(session, hal);
  if (session->has_pending) pair_send_ack(session, identity, hal);
  if (session->has_pending && session->they_have_us) pair_send_leave(session, identity, hal);
  session->last_hello_at = hal->now_ms(hal->ctx);
  session->hello_sent = true;
}

static void pair_on_hello(pair_session_t *session, identity_store_t *identity, const hal_t *hal,
                          const uint8_t *raw, size_t raw_len, const hello_payload_t *hello,
                          int8_t rssi) {
  if (!session->active || !session->our_hello_len) return;
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  if (node_id_equal(hello->node_id, us)) return;

  if (session->has_pending &&
      (!node_id_equal(hello->node_id, session->pending.hello.node_id) ||
       !pair_bytes_eq(hello->eph, EPH_LEN, session->pending.hello.eph, EPH_LEN))) {
    session->they_have_us = false;
  }

  session->has_pending = true;
  size_t n = raw_len > RAW_FRAME_MAX ? RAW_FRAME_MAX : raw_len;
  memcpy(session->pending.hello_bytes, raw, n);
  session->pending.hello_len = (uint8_t)n;
  session->pending.hello = *hello;
  session->pending.rssi = rssi;
  session->last_rssi = rssi;
  session->has_rssi = true;
  pair_send_ack(session, identity, hal);
}

static void pair_complete(pair_session_t *session, const hal_t *hal) {
  pair_abort(session, hal);
  session->last_error = PAIR_ERR_NONE;
  hal->vibe(hal->ctx, 200);
}

static bool pair_commit(pair_session_t *session, identity_store_t *identity, contact_book_t *contacts,
                        const hal_t *hal) {
  if (!session->has_pending || !session->has_eph) return false;
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  contact_t c;
  memset(&c, 0, sizeof c);
  memcpy(c.node_id, session->pending.hello.node_id, NODE_ID_LEN);
  str_copy_max(c.name, sizeof c.name, session->pending.hello.name, PAGE_FROM_MAX);
  c.beeple = session->pending.hello.beeple;
  pair_derive_session_key(us, session->pending.hello.node_id, session->our_eph,
                          session->pending.hello.eph, c.session_key);
  c.paired_ms = hal->now_ms(hal->ctx);
  if (!contacts_upsert(contacts, &c)) {
    pair_abort(session, hal);
    session->last_error = PAIR_ERR_BOOK_FULL;
    return false;
  }
  return true;
}

static bool ack_matches(pair_session_t *session, identity_store_t *identity,
                        const hello_ack_payload_t *ack) {
  if (!session->has_pending) return false;
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  if (!node_id_equal(ack->dest_id, us)) return false;
  if (!node_id_equal(ack->src_id, session->pending.hello.node_id)) return false;
  uint8_t confirm[CONFIRM_LEN];
  if (!confirm_for(session, confirm)) return false;
  return pair_bytes_eq(confirm, CONFIRM_LEN, ack->confirm, CONFIRM_LEN);
}

static void pair_on_ack(pair_session_t *session, identity_store_t *identity,
                        const hello_ack_payload_t *ack) {
  if (!session->active || !session->our_hello_len || !session->has_pending) return;
  if (!ack_matches(session, identity, ack)) return;
  session->they_have_us = true;
}

static void pair_on_leave(pair_session_t *session, identity_store_t *identity,
                         contact_book_t *contacts, const hal_t *hal,
                         const pair_leave_payload_t *leave) {
  if (!session->active) return;
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  if (!node_id_equal(leave->dest_id, us)) return;

  if (!session->they_have_us || !session->has_pending) {
    pair_abort(session, hal);
    session->last_error = PAIR_ERR_TRY_AGAIN;
    return;
  }
  if (!ack_matches(session, identity, leave)) return;

  pair_send_leave(session, identity, hal);
  if (!pair_commit(session, identity, contacts, hal)) return;
  pair_complete(session, hal);
}

void pair_start(pair_session_t *session, identity_store_t *identity, const hal_t *hal,
                uint8_t beeple) {
  identity_load_or_create(identity, hal);
  session->last_error = PAIR_ERR_NONE;
  clear_session(session);

  hal->radio_set_whisper(hal->ctx, true);

  uint8_t eph[EPH_LEN];
  uint8_t nonce[HELLO_NONCE_LEN];
  hal->random_fill(hal->ctx, eph, EPH_LEN);
  hal->random_fill(hal->ctx, nonce, HELLO_NONCE_LEN);

  hello_payload_t hello;
  memset(&hello, 0, sizeof hello);
  identity_get_node_id(identity, hello.node_id);
  memcpy(hello.eph, eph, EPH_LEN);
  memcpy(hello.nonce, nonce, HELLO_NONCE_LEN);
  identity_get_display_name(identity, hello.name, sizeof hello.name);
  hello.beeple = beeple < BEEPLE_N ? beeple : BEEPLE_NONE;

  uint8_t encoded[RAW_FRAME_MAX];
  size_t n = frame_encode_hello(&hello, encoded, sizeof encoded);
  if (!n) {
    hal->radio_set_whisper(hal->ctx, false);
    return;
  }

  memcpy(session->our_eph, eph, EPH_LEN);
  session->has_eph = true;
  memcpy(session->our_hello_bytes, encoded, n);
  session->our_hello_len = (uint8_t)n;
  session->start_ms = hal->now_ms(hal->ctx);
  session->last_hello_at = 0;
  session->hello_sent = false;
  session->active = true;
  hal->vibe(hal->ctx, 80);
}

void pair_tick(pair_session_t *session, identity_store_t *identity, contact_book_t *contacts,
               const hal_t *hal) {
  if (!session->active) return;

  uint32_t now = hal->now_ms(hal->ctx);
  if (elapsed_u32(now, session->start_ms) >= PAIR_WINDOW_MS) {
    pair_abort(session, hal);
    session->last_error = PAIR_ERR_TIMEOUT;
    return;
  }

  /* A queued HELLO can retire they_have_us. Read it before this burst sends LEAVE. */
  raw_frame_t incoming;
  memset(&incoming, 0, sizeof incoming);
  while (hal->radio_poll_raw(hal->ctx, &incoming)) {
    if (!session->active) continue;
    if (incoming.rssi < PAIR_RSSI_MIN) continue;
    session->last_rssi = incoming.rssi;
    session->has_rssi = true;

    int kind = frame_type(incoming.bytes, incoming.len);
    if (kind == FRAME_HELLO) {
      hello_payload_t hello;
      if (frame_decode_hello(incoming.bytes, incoming.len, &hello)) {
        pair_on_hello(session, identity, hal, incoming.bytes, incoming.len, &hello, incoming.rssi);
      }
    } else if (kind == FRAME_HELLO_ACK) {
      hello_ack_payload_t ack;
      if (frame_decode_hello_ack(incoming.bytes, incoming.len, &ack)) {
        pair_on_ack(session, identity, &ack);
      }
    } else if (kind == FRAME_PAIR_LEAVE) {
      pair_leave_payload_t leave;
      if (frame_decode_pair_leave(incoming.bytes, incoming.len, &leave)) {
        pair_on_leave(session, identity, contacts, hal, &leave);
      }
    }
  }

  if (!session->active) return;
  if (!session->hello_sent || elapsed_u32(now, session->last_hello_at) >= HELLO_PERIOD_MS) {
    pair_broadcast(session, identity, hal);
  }
}

void pair_abort(pair_session_t *session, const hal_t *hal) {
  hal->radio_set_whisper(hal->ctx, false);
  clear_session(session);
}

bool pair_active(const pair_session_t *session) {
  return session->active;
}

uint32_t pair_deadline_ms(const pair_session_t *session) {
  if (!session->active) return 0;
  return session->start_ms + PAIR_WINDOW_MS;
}

bool pair_last_rssi(const pair_session_t *session, int8_t *out) {
  if (!session->has_rssi) return false;
  if (out) *out = session->last_rssi;
  return true;
}

const char *pair_error(const pair_session_t *session) {
  switch (session->last_error) {
    case PAIR_ERR_TIMEOUT:
      return "timeout";
    case PAIR_ERR_BOOK_FULL:
      return "book full";
    case PAIR_ERR_TRY_AGAIN:
      return "try again";
    default:
      return NULL;
  }
}

void pair_reset(pair_session_t *session, const hal_t *hal) {
  if (hal) hal->radio_set_whisper(hal->ctx, false);
  session->last_error = PAIR_ERR_NONE;
  clear_session(session);
}
