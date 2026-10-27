#pragma once

#include "attempt.h"
#include "contacts.h"
#include "frames.h"
#include "hal.h"
#include "identity.h"
#include "relay.h"

#include <stdbool.h>
#include <stdint.h>

#define PAGE_SEEN_MAX 24

typedef enum {
  MESSAGE_IDLE = 0,
  MESSAGE_SENDING,
  MESSAGE_DELIVERED,
  MESSAGE_FAILED,
} message_status_t;

typedef struct {
  uint8_t dest_id[NODE_ID_LEN];
  uint8_t msg_id[PAGE_MSG_ID_LEN];
} message_outbox_t;

typedef struct {
  uint8_t src_id[NODE_ID_LEN];
  uint8_t msg_id[PAGE_MSG_ID_LEN];
} message_delivered_t;

typedef struct message_session {
  message_status_t status;
  message_outbox_t outbox;
  attempt_t attempt;
  char body[PAGE_BODY_MAX + 1];
  /* Retries seal the index message_tx stored. */
  uint8_t send_beeple;
  relay_cache_t relay;
  message_delivered_t delivered[PAGE_SEEN_MAX];
  int delivered_n;
} message_session_t;

typedef void (*message_on_page_fn)(void *user, const page_t *page);

void message_init(message_session_t *session);
void message_reset(message_session_t *session);
bool message_busy(const message_session_t *session);
message_status_t message_status(const message_session_t *session);
void message_clear_status(message_session_t *session);
int message_outbox_attempts(const message_session_t *session);
int message_outbox_budget(const message_session_t *session);
bool message_first_wait_done(const message_session_t *session, uint32_t now);

bool message_tx(message_session_t *session, const hal_t *hal, const uint8_t *dest_id,
                const char *body, contact_book_t *contacts, uint8_t beeple);
void message_on_raw(message_session_t *session, identity_store_t *identity,
                    contact_book_t *contacts, const hal_t *hal, const raw_frame_t *raw,
                    message_on_page_fn on_page, void *user);
void message_outbox_tick(message_session_t *session, identity_store_t *identity,
                         contact_book_t *contacts, const hal_t *hal);
