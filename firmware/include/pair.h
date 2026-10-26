#pragma once

#include "contacts.h"
#include "frames.h"
#include "hal.h"
#include "identity.h"

#include <stdbool.h>
#include <stdint.h>

#define PAIR_WINDOW_MS 20000
#define HELLO_PERIOD_MS 400
#define PAIR_RSSI_MIN (-45)

typedef enum {
  PAIR_ERR_NONE = 0,
  PAIR_ERR_TIMEOUT,
  PAIR_ERR_BOOK_FULL,
  PAIR_ERR_TRY_AGAIN,
} pair_error_t;

typedef struct {
  uint8_t hello_bytes[RAW_FRAME_MAX];
  uint8_t hello_len;
  hello_payload_t hello;
  int8_t rssi;
} pair_pending_t;

typedef struct {
  bool active;
  uint32_t start_ms;
  uint32_t last_hello_at;
  bool hello_sent;
  uint8_t our_eph[EPH_LEN];
  bool has_eph;
  uint8_t our_hello_bytes[RAW_FRAME_MAX];
  uint8_t our_hello_len;
  pair_pending_t pending;
  bool has_pending;
  bool they_have_us;
  int8_t last_rssi;
  bool has_rssi;
  pair_error_t last_error;
} pair_session_t;

void pair_init(pair_session_t *session);
void pair_start(pair_session_t *session, identity_store_t *identity, const hal_t *hal,
                uint8_t beeple);
void pair_tick(pair_session_t *session, identity_store_t *identity, contact_book_t *contacts,
               const hal_t *hal);
void pair_abort(pair_session_t *session, const hal_t *hal);
bool pair_active(const pair_session_t *session);
uint32_t pair_deadline_ms(const pair_session_t *session);
bool pair_last_rssi(const pair_session_t *session, int8_t *out);
const char *pair_error(const pair_session_t *session);
void pair_reset(pair_session_t *session, const hal_t *hal);
