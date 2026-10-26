#include "page.h"

#include "beeple.h"
#include "crypto.h"
#include "util.h"

#include <string.h>

void page_radio_init(page_radio_t *radio) {
  memset(radio, 0, sizeof *radio);
  radio->status = PAGE_IDLE;
}

void page_radio_reset(page_radio_t *radio) {
  page_radio_init(radio);
}

bool page_busy(const page_radio_t *radio) {
  return radio->status == PAGE_SENDING;
}

page_send_status_t page_send_status(const page_radio_t *radio) {
  return radio->status;
}

void page_clear_status(page_radio_t *radio) {
  if (radio->status == PAGE_DELIVERED || radio->status == PAGE_FAILED) {
    radio->status = PAGE_IDLE;
  }
}

bool page_outbox_to(const page_radio_t *radio, uint8_t dest[NODE_ID_LEN]) {
  if (!radio->has_dest) return false;
  if (dest) memcpy(dest, radio->dest_id, NODE_ID_LEN);
  return true;
}

const char *page_outbox_body(const page_radio_t *radio) {
  return radio->body;
}

int page_outbox_attempts(const page_radio_t *radio) {
  return radio->has_outbox ? radio->outbox.attempts : 0;
}

static bool nonce_used(const page_outbox_t *box, const uint8_t *nonce) {
  for (int i = 0; i < box->nonce_n; i++) {
    if (bytes_eq_n(box->nonces[i], nonce, PAGE_NONCE_LEN)) return true;
  }
  return false;
}

static bool delivered_find(const page_radio_t *radio, const uint8_t *src_id, const uint8_t *msg_id) {
  for (int i = 0; i < radio->delivered_n; i++) {
    if (node_id_equal(radio->delivered[i].src_id, src_id) &&
        bytes_eq_n(radio->delivered[i].msg_id, msg_id, PAGE_MSG_ID_LEN)) {
      return true;
    }
  }
  return false;
}

static void delivered_insert(page_radio_t *radio, const uint8_t *src_id, const uint8_t *msg_id) {
  if (radio->delivered_n >= PAGE_SEEN_MAX) {
    memmove(&radio->delivered[0], &radio->delivered[1],
            (size_t)(PAGE_SEEN_MAX - 1) * sizeof(page_delivered_t));
    radio->delivered_n = PAGE_SEEN_MAX - 1;
  }
  memcpy(radio->delivered[radio->delivered_n].src_id, src_id, NODE_ID_LEN);
  memcpy(radio->delivered[radio->delivered_n].msg_id, msg_id, PAGE_MSG_ID_LEN);
  radio->delivered_n++;
}

static uint32_t wait_after_attempt(int attempts) {
  if (attempts < PAGE_ATTEMPTS_FAST) return PAGE_RETRY_FAST_MS;
  return PAGE_RETRY_SLOW_MS;
}

static void send_page_ack(page_radio_t *radio, const hal_t *hal, identity_store_t *identity,
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
  relay_mark_seen(&radio->relay, FRAME_PAGE_ACK, us, nonce);
  hal->radio_send_raw(hal->ctx, frame, n);
}

static void deliver_page(page_radio_t *radio, identity_store_t *identity, contact_book_t *contacts,
                         const hal_t *hal, const raw_frame_t *raw, const page_payload_t *p,
                         page_on_page_fn on_page, void *user) {
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

  if (!delivered_find(radio, p->src_id, msg_id)) {
    delivered_insert(radio, p->src_id, msg_id);
    page_t page;
    memset(&page, 0, sizeof page);
    str_copy_max(page.from, sizeof page.from, contact->name, PAGE_FROM_MAX);
    str_copy_max(page.body, sizeof page.body, text, PAGE_BODY_MAX);
    page.rssi = raw->rssi;
    page.received_ms = raw->received_ms;
    page.beeple = beeple < BEEPLE_N ? beeple : BEEPLE_NONE;
    if (on_page) on_page(user, &page);
  }

  send_page_ack(radio, hal, identity, p->src_id, p->nonce, contact->session_key);
}

static void try_outbox_ack(page_radio_t *radio, identity_store_t *identity, contact_book_t *contacts,
                           const page_ack_payload_t *ack) {
  if (!radio->has_outbox || radio->status != PAGE_SENDING) return;
  page_outbox_t *box = &radio->outbox;
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  if (!node_id_equal(ack->dest_id, us)) return;
  if (!node_id_equal(ack->src_id, box->dest_id)) return;
  if (!nonce_used(box, ack->nonce)) return;

  const contact_t *contact = contacts_find_by_id(contacts, box->dest_id);
  if (!contact) return;
  if (!page_ack_check(contact->session_key, ack->nonce, ack->mac)) return;

  radio->has_outbox = false;
  radio->status = PAGE_DELIVERED;
}

static void on_raw_frame(page_radio_t *radio, identity_store_t *identity, contact_book_t *contacts,
                         const hal_t *hal, const raw_frame_t *raw, page_on_page_fn on_page,
                         void *user, locate_radio_t *locate) {
  int kind = frame_type(raw->bytes, raw->len);
  if (kind == FRAME_LOC_REQ || kind == FRAME_LOC) {
    locate_on_raw(locate, identity, contacts, hal, raw);
    return;
  }
  if (kind == FRAME_HELLO || kind == FRAME_HELLO_ACK || kind == FRAME_PAIR_LEAVE) return;

  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);

  if (kind == FRAME_PAGE) {
    page_payload_t p;
    if (!frame_decode_page(raw->bytes, raw->len, &p)) return;
    if (node_id_equal(p.dest_id, us)) {
      deliver_page(radio, identity, contacts, hal, raw, &p, on_page, user);
      return;
    }
    if (node_id_equal(p.src_id, us)) return;
    relay_on_request(&radio->relay, hal, FRAME_PAGE, FRAME_PAGE_ACK, p.src_id, p.dest_id, p.nonce,
                     p.hop_ttl, raw->bytes, raw->len);
    return;
  }

  if (kind == FRAME_PAGE_ACK) {
    page_ack_payload_t ack;
    if (!frame_decode_page_ack(raw->bytes, raw->len, &ack)) return;
    if (node_id_equal(ack.dest_id, us)) {
      try_outbox_ack(radio, identity, contacts, &ack);
      return;
    }
    relay_on_reply(&radio->relay, hal, FRAME_PAGE, FRAME_PAGE_ACK, ack.src_id, ack.dest_id,
                   ack.nonce, ack.hop_ttl, raw->bytes, raw->len);
  }
}

static bool tx_attempt(page_radio_t *radio, const hal_t *hal, identity_store_t *identity,
                       contact_book_t *contacts, page_outbox_t *box) {
  const contact_t *contact = contacts_find_by_id(contacts, box->dest_id);
  if (!contact) return false;

  uint8_t nonce[PAGE_NONCE_LEN];
  hal->random_fill(hal->ctx, nonce, PAGE_NONCE_LEN);

  size_t text_len = str_ascii_len(radio->body);
  if (text_len > PAGE_BODY_MAX) text_len = PAGE_BODY_MAX;
  uint8_t plaintext[PAGE_MSG_ID_LEN + 1 + PAGE_BODY_MAX];
  memcpy(plaintext, box->msg_id, PAGE_MSG_ID_LEN);
  plaintext[PAGE_MSG_ID_LEN] = radio->send_beeple;
  if (text_len) memcpy(plaintext + PAGE_MSG_ID_LEN + 1, radio->body, text_len);

  uint8_t cipher[PAGE_MSG_ID_LEN + 1 + PAGE_BODY_MAX];
  uint8_t mac[MAC_LEN];
  size_t plain_len = PAGE_MSG_ID_LEN + 1 + text_len;
  page_seal(contact->session_key, nonce, plaintext, plain_len, cipher, mac);

  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  page_payload_t p;
  memset(&p, 0, sizeof p);
  memcpy(p.dest_id, box->dest_id, NODE_ID_LEN);
  memcpy(p.src_id, us, NODE_ID_LEN);
  memcpy(p.nonce, nonce, PAGE_NONCE_LEN);
  p.hop_ttl = PAGE_HOP_TTL;
  memcpy(p.body, cipher, plain_len);
  p.body_len = (uint8_t)plain_len;
  memcpy(p.mac, mac, MAC_LEN);

  uint8_t frame[RAW_FRAME_MAX];
  size_t n = frame_encode_page(&p, frame, sizeof frame);
  if (!n) return false;

  if (box->nonce_n < PAGE_ATTEMPTS) {
    memcpy(box->nonces[box->nonce_n], nonce, PAGE_NONCE_LEN);
    box->nonce_n++;
  }
  relay_mark_seen(&radio->relay, FRAME_PAGE, us, nonce);
  hal->radio_send_raw(hal->ctx, frame, n);
  return true;
}

static void outbox_tick(page_radio_t *radio, identity_store_t *identity, contact_book_t *contacts,
                        const hal_t *hal, locate_radio_t *locate) {
  if (locate_busy(locate)) return;
  if (!radio->has_outbox || radio->status != PAGE_SENDING) return;
  page_outbox_t *box = &radio->outbox;
  uint32_t now = hal->now_ms(hal->ctx);
  if (!time_reached(now, box->next_at)) return;

  if (box->attempts >= PAGE_ATTEMPTS) {
    radio->has_outbox = false;
    radio->status = PAGE_FAILED;
    return;
  }

  if (!tx_attempt(radio, hal, identity, contacts, box)) {
    radio->has_outbox = false;
    radio->status = PAGE_FAILED;
    return;
  }
  box->attempts += 1;
  box->next_at = now + wait_after_attempt(box->attempts);
}

bool page_tx(page_radio_t *radio, const hal_t *hal, const uint8_t *dest_id, const char *body,
             identity_store_t *identity, contact_book_t *contacts, locate_radio_t *locate,
             uint8_t beeple) {
  if (page_busy(radio) || locate_busy(locate)) return false;
  const contact_t *contact = contacts_find_by_id(contacts, dest_id);
  if (!contact) return false;

  uint8_t msg_id[PAGE_MSG_ID_LEN];
  hal->random_fill(hal->ctx, msg_id, PAGE_MSG_ID_LEN);

  memset(&radio->outbox, 0, sizeof radio->outbox);
  memcpy(radio->outbox.dest_id, dest_id, NODE_ID_LEN);
  memcpy(radio->outbox.msg_id, msg_id, PAGE_MSG_ID_LEN);
  radio->outbox.attempts = 0;
  radio->outbox.next_at = 0;
  radio->status = PAGE_SENDING;
  memcpy(radio->dest_id, dest_id, NODE_ID_LEN);
  radio->has_dest = true;
  str_copy_max(radio->body, sizeof radio->body, body ? body : "", PAGE_BODY_MAX);
  radio->send_beeple = beeple < BEEPLE_N ? beeple : BEEPLE_NONE;
  radio->has_outbox = true;

  if (!tx_attempt(radio, hal, identity, contacts, &radio->outbox)) {
    radio->status = PAGE_IDLE;
    radio->has_outbox = false;
    radio->has_dest = false;
    radio->body[0] = 0;
    return false;
  }
  radio->outbox.attempts = 1;
  radio->outbox.next_at = hal->now_ms(hal->ctx) + wait_after_attempt(1);
  return true;
}

void page_tick(page_radio_t *radio, identity_store_t *identity, contact_book_t *contacts,
               const hal_t *hal, page_on_page_fn on_page, void *user, locate_radio_t *locate) {
  raw_frame_t raw;
  memset(&raw, 0, sizeof raw);
  while (hal->radio_poll_raw(hal->ctx, &raw)) {
    on_raw_frame(radio, identity, contacts, hal, &raw, on_page, user, locate);
  }
  outbox_tick(radio, identity, contacts, hal, locate);
  relay_tick(&radio->relay, hal);
  locate_tick(locate, identity, contacts, hal);
}

void page_rx_tick(page_radio_t *radio, identity_store_t *identity, contact_book_t *contacts,
                 const hal_t *hal, page_on_page_fn on_page, void *user, locate_radio_t *locate) {
  page_tick(radio, identity, contacts, hal, on_page, user, locate);
}
