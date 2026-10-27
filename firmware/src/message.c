#include "message.h"

#include "beeple.h"
#include "crypto.h"
#include "util.h"

#include <string.h>

void message_init(message_session_t *session) {
  memset(session, 0, sizeof *session);
  session->status = MESSAGE_IDLE;
}

void message_reset(message_session_t *session) { message_init(session); }

bool message_busy(const message_session_t *session) {
  return session && session->status == MESSAGE_SENDING;
}

message_status_t message_status(const message_session_t *session) { return session->status; }

void message_clear_status(message_session_t *session) {
  if (session->status == MESSAGE_DELIVERED || session->status == MESSAGE_FAILED) {
    session->status = MESSAGE_IDLE;
  }
}

int message_outbox_attempts(const message_session_t *session) {
  return session->attempt.in_flight ? session->attempt.attempts : 0;
}

int message_outbox_budget(const message_session_t *session) {
  return attempt_budget(&session->attempt);
}

bool message_first_wait_done(const message_session_t *session, uint32_t now) {
  return session && attempt_first_wait_done(&session->attempt, now);
}

static bool delivered_find(const message_session_t *session, const uint8_t *src_id,
                           const uint8_t *msg_id) {
  for (int i = 0; i < session->delivered_n; i++) {
    if (node_id_equal(session->delivered[i].src_id, src_id) &&
        bytes_eq_n(session->delivered[i].msg_id, msg_id, PAGE_MSG_ID_LEN)) {
      return true;
    }
  }
  return false;
}

static void delivered_insert(message_session_t *session, const uint8_t *src_id, const uint8_t *msg_id) {
  if (session->delivered_n >= PAGE_SEEN_MAX) {
    memmove(&session->delivered[0], &session->delivered[1],
            (size_t)(PAGE_SEEN_MAX - 1) * sizeof(message_delivered_t));
    session->delivered_n = PAGE_SEEN_MAX - 1;
  }
  memcpy(session->delivered[session->delivered_n].src_id, src_id, NODE_ID_LEN);
  memcpy(session->delivered[session->delivered_n].msg_id, msg_id, PAGE_MSG_ID_LEN);
  session->delivered_n++;
}

static void send_page_ack(message_session_t *session, const hal_t *hal, identity_store_t *identity,
                          const uint8_t *dest_id, const uint8_t *nonce, const uint8_t *session_key) {
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  page_ack_payload_t ack;
  memset(&ack, 0, sizeof ack);
  memcpy(ack.dest_id, dest_id, NODE_ID_LEN);
  memcpy(ack.src_id, us, NODE_ID_LEN);
  memcpy(ack.nonce, nonce, PAGE_NONCE_LEN);
  ack.hop_ttl = PAGE_HOP_TTL;
  page_ack_mac(session_key, nonce, ack.mac);
  uint8_t frame[PAGE_ACK_LEN];
  size_t n = frame_encode_page_ack(&ack, frame, sizeof frame);
  if (!n) return;
  relay_mark_seen(&session->relay, FRAME_PAGE_ACK, us, nonce);
  hal->radio_send_raw(hal->ctx, frame, n);
}

static void deliver_page(message_session_t *session, identity_store_t *identity,
                         contact_book_t *contacts, const hal_t *hal, const raw_frame_t *raw,
                         const page_payload_t *p, message_on_page_fn on_page, void *user) {
  const contact_t *contact = contacts_find_by_id(contacts, p->src_id);
  if (!contact) return;

  uint8_t plain[PAGE_WIRE_BODY_MAX];
  if (!page_open(contact->session_key, p->nonce, p->body, p->body_len, p->mac, plain)) return;
  if (p->body_len < PAGE_MSG_ID_LEN + 1) return;

  const uint8_t *msg_id = plain;
  uint8_t beeple = plain[PAGE_MSG_ID_LEN];
  char text[PAGE_BODY_MAX + 1];
  size_t tlen = (size_t)p->body_len - PAGE_MSG_ID_LEN - 1;
  if (tlen > PAGE_BODY_MAX) tlen = PAGE_BODY_MAX;
  memset(text, 0, sizeof text);
  if (tlen) memcpy(text, plain + PAGE_MSG_ID_LEN + 1, tlen);
  if (beeple < BEEPLE_N) contacts_set_beeple(contacts, p->src_id, beeple);

  if (!delivered_find(session, p->src_id, msg_id)) {
    delivered_insert(session, p->src_id, msg_id);
    page_t page;
    memset(&page, 0, sizeof page);
    str_copy_max(page.from, sizeof page.from, contact->name, PAGE_FROM_MAX);
    str_copy_max(page.body, sizeof page.body, text, PAGE_BODY_MAX);
    page.rssi = raw->rssi;
    page.received_ms = raw->received_ms;
    page.beeple = beeple < BEEPLE_N ? beeple : BEEPLE_NONE;
    if (on_page) on_page(user, &page);
  }

  send_page_ack(session, hal, identity, p->src_id, p->nonce, contact->session_key);
}

static void try_outbox_ack(message_session_t *session, identity_store_t *identity,
                           contact_book_t *contacts, const page_ack_payload_t *ack) {
  if (session->status != MESSAGE_SENDING) return;
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  if (!attempt_from_peer(&session->attempt, us, ack->dest_id, session->outbox.dest_id, ack->src_id,
                         ack->nonce)) {
    return;
  }

  const contact_t *contact = contacts_find_by_id(contacts, session->outbox.dest_id);
  if (!contact) return;
  if (!page_ack_check(contact->session_key, ack->nonce, ack->mac)) return;

  attempt_stop(&session->attempt);
  session->status = MESSAGE_DELIVERED;
}

void message_on_raw(message_session_t *session, identity_store_t *identity, contact_book_t *contacts,
                    const hal_t *hal, const raw_frame_t *raw, message_on_page_fn on_page, void *user) {
  int kind = frame_type(raw->bytes, raw->len);
  if (kind != FRAME_PAGE && kind != FRAME_PAGE_ACK) return;

  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);

  if (kind == FRAME_PAGE) {
    page_payload_t p;
    if (!frame_decode_page(raw->bytes, raw->len, &p)) return;
    if (node_id_equal(p.dest_id, us)) {
      deliver_page(session, identity, contacts, hal, raw, &p, on_page, user);
      return;
    }
    if (node_id_equal(p.src_id, us)) return;
    relay_on_request(&session->relay, hal, FRAME_PAGE, FRAME_PAGE_ACK, p.src_id, p.dest_id, p.nonce,
                     p.hop_ttl, raw->bytes, raw->len);
    return;
  }

  page_ack_payload_t ack;
  if (!frame_decode_page_ack(raw->bytes, raw->len, &ack)) return;
  if (node_id_equal(ack.dest_id, us)) {
    try_outbox_ack(session, identity, contacts, &ack);
    return;
  }
  relay_on_reply(&session->relay, hal, FRAME_PAGE, FRAME_PAGE_ACK, ack.src_id, ack.dest_id, ack.nonce,
                 ack.hop_ttl, raw->bytes, raw->len);
}

static bool tx_attempt(message_session_t *session, const hal_t *hal, identity_store_t *identity,
                       contact_book_t *contacts) {
  const contact_t *contact = contacts_find_by_id(contacts, session->outbox.dest_id);
  if (!contact) return false;

  uint8_t nonce[PAGE_NONCE_LEN];
  hal->random_fill(hal->ctx, nonce, PAGE_NONCE_LEN);

  size_t text_len = str_ascii_len(session->body);
  if (text_len > PAGE_BODY_MAX) text_len = PAGE_BODY_MAX;
  uint8_t plaintext[PAGE_MSG_ID_LEN + 1 + PAGE_BODY_MAX];
  memcpy(plaintext, session->outbox.msg_id, PAGE_MSG_ID_LEN);
  plaintext[PAGE_MSG_ID_LEN] = session->send_beeple;
  if (text_len) memcpy(plaintext + PAGE_MSG_ID_LEN + 1, session->body, text_len);

  uint8_t cipher[PAGE_MSG_ID_LEN + 1 + PAGE_BODY_MAX];
  uint8_t mac[MAC_LEN];
  size_t plain_len = PAGE_MSG_ID_LEN + 1 + text_len;
  page_seal(contact->session_key, nonce, plaintext, plain_len, cipher, mac);

  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  page_payload_t p;
  memset(&p, 0, sizeof p);
  memcpy(p.dest_id, session->outbox.dest_id, NODE_ID_LEN);
  memcpy(p.src_id, us, NODE_ID_LEN);
  memcpy(p.nonce, nonce, PAGE_NONCE_LEN);
  p.hop_ttl = PAGE_HOP_TTL;
  memcpy(p.body, cipher, plain_len);
  p.body_len = (uint8_t)plain_len;
  memcpy(p.mac, mac, MAC_LEN);

  uint8_t frame[RAW_FRAME_MAX];
  size_t n = frame_encode_page(&p, frame, sizeof frame);
  if (!n) return false;

  if (!attempt_remember(&session->attempt, nonce)) return false;
  relay_mark_seen(&session->relay, FRAME_PAGE, us, nonce);
  hal->radio_send_raw(hal->ctx, frame, n);
  return true;
}

typedef struct {
  message_session_t *session;
  const hal_t *hal;
  identity_store_t *identity;
  contact_book_t *contacts;
} message_send_t;

static bool message_send(void *user) {
  message_send_t *send = user;
  return tx_attempt(send->session, send->hal, send->identity, send->contacts);
}

void message_outbox_tick(message_session_t *session, identity_store_t *identity,
                         contact_book_t *contacts, const hal_t *hal) {
  if (session->status != MESSAGE_SENDING) return;
  message_send_t send = {session, hal, identity, contacts};
  if (attempt_step(&session->attempt, hal->now_ms(hal->ctx), message_send, &send) == ATTEMPT_DONE) {
    session->status = MESSAGE_FAILED;
  }
}

bool message_tx(message_session_t *session, const hal_t *hal, const uint8_t *dest_id, const char *body,
                contact_book_t *contacts, uint8_t beeple) {
  if (message_busy(session)) return false;
  const contact_t *contact = contacts_find_by_id(contacts, dest_id);
  if (!contact) return false;

  uint8_t msg_id[PAGE_MSG_ID_LEN];
  hal->random_fill(hal->ctx, msg_id, PAGE_MSG_ID_LEN);

  memset(&session->outbox, 0, sizeof session->outbox);
  memcpy(session->outbox.dest_id, dest_id, NODE_ID_LEN);
  memcpy(session->outbox.msg_id, msg_id, PAGE_MSG_ID_LEN);
  session->status = MESSAGE_SENDING;
  str_copy_max(session->body, sizeof session->body, body ? body : "", PAGE_BODY_MAX);
  session->send_beeple = beeple < BEEPLE_N ? beeple : BEEPLE_NONE;
  attempt_arm(&session->attempt, ATTEMPT_PAGE);
  return true;
}
