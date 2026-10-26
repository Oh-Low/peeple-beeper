#pragma once

#include "contacts.h"
#include "frames.h"
#include "hal.h"
#include "identity.h"
#include "locate.h"

#include <stdbool.h>
#include <stdint.h>

#define PAGE_HOP_TTL 3
#define PAGE_RETRY_FAST_MS 10000
#define PAGE_RETRY_SLOW_MS 60000
#define PAGE_ATTEMPTS_FAST 4
#define PAGE_ATTEMPTS_SLOW 5
#define PAGE_ATTEMPTS (PAGE_ATTEMPTS_FAST + PAGE_ATTEMPTS_SLOW)
#define PAGE_SEEN_MAX 24

typedef enum {
  PAGE_IDLE = 0,
  PAGE_SENDING,
  PAGE_DELIVERED,
  PAGE_FAILED,
} page_send_status_t;

typedef struct {
  uint8_t dest_id[NODE_ID_LEN];
  uint8_t msg_id[PAGE_MSG_ID_LEN];
  uint8_t nonces[PAGE_ATTEMPTS][PAGE_NONCE_LEN];
  int nonce_n;
  int attempts;
  uint32_t next_at;
} page_outbox_t;

typedef struct {
  int kind;
  uint8_t src_id[NODE_ID_LEN];
  uint8_t nonce[PAGE_NONCE_LEN];
  uint8_t pending[RAW_FRAME_MAX];
  uint8_t pending_len;
  bool has_pending;
  uint32_t pending_at;
} page_seen_t;

typedef struct {
  uint8_t src_id[NODE_ID_LEN];
  uint8_t msg_id[PAGE_MSG_ID_LEN];
} page_delivered_t;

typedef struct page_radio {
  page_send_status_t status;
  page_outbox_t outbox;
  bool has_outbox;
  uint8_t dest_id[NODE_ID_LEN];
  bool has_dest;
  char body[PAGE_BODY_MAX + 1];
  /* Mascot sealed into this send. Retries keep the index from page_tx. */
  uint8_t send_beeple;
  page_seen_t seen[PAGE_SEEN_MAX];
  int seen_n;
  page_delivered_t delivered[PAGE_SEEN_MAX];
  int delivered_n;
} page_radio_t;

typedef void (*page_on_page_fn)(void *user, const page_t *page);

void page_radio_init(page_radio_t *radio);
void page_radio_reset(page_radio_t *radio);
bool page_busy(const page_radio_t *radio);
page_send_status_t page_send_status(const page_radio_t *radio);
void page_clear_status(page_radio_t *radio);
bool page_outbox_to(const page_radio_t *radio, uint8_t dest[NODE_ID_LEN]);
const char *page_outbox_body(const page_radio_t *radio);
int page_outbox_attempts(const page_radio_t *radio);

bool page_tx(page_radio_t *radio, const hal_t *hal, const uint8_t *dest_id, const char *body,
             identity_store_t *identity, contact_book_t *contacts, locate_radio_t *locate,
             uint8_t beeple);
void page_tick(page_radio_t *radio, identity_store_t *identity, contact_book_t *contacts,
               const hal_t *hal, page_on_page_fn on_page, void *user, locate_radio_t *locate);
void page_rx_tick(page_radio_t *radio, identity_store_t *identity, contact_book_t *contacts,
                  const hal_t *hal, page_on_page_fn on_page, void *user, locate_radio_t *locate);
