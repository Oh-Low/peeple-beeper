#pragma once

#include "contacts.h"
#include "frames.h"
#include "hal.h"
#include "identity.h"
#include "relay.h"

#include <stdbool.h>
#include <stdint.h>

#define LOCATE_ATTEMPTS 4
#define LOCATE_RETRY_MS 2000
/* Wake the receiver on this period and listen this long for one fix. */
#define GPS_SAMPLE_PERIOD_MS (30u * 60u * 1000u)
#define GPS_LISTEN_MS (60u * 1000u)
/* One warning per requester until this quiet time passes. */
#define TRACK_WARN_MS 12000u
/* After Allow, that contact is answered without another prompt. */
#define TRACK_ALLOW_MS (30u * 60u * 1000u)
#define TRACK_GRANT_MAX 8
#define TRACK_ASK_MAX 8

struct page_radio;

typedef struct locate_radio locate_radio_t;

typedef enum {
  LOCATE_IDLE = 0,
  LOCATE_SENDING,
  LOCATE_FOUND,
  LOCATE_NO_SKY,
  LOCATE_NO_REPLY,
  LOCATE_DENIED,
} locate_status_t;

struct locate_radio {
  locate_status_t status;
  uint8_t dest_id[NODE_ID_LEN];
  uint8_t nonces[LOCATE_ATTEMPTS][PAGE_NONCE_LEN];
  int nonce_n;
  int attempts;
  uint32_t next_at;
  bool has_outbox;
  bool has_fix;
  bool has_loc_time;
  uint8_t loc_hh;
  uint8_t loc_mm;
  int32_t their_lat;
  int32_t their_lon;
  bool saved_fix;
  int32_t saved_lat;
  int32_t saved_lon;
  uint8_t saved_hh;
  uint8_t saved_mm;
  bool gps_listening;
  uint32_t gps_listen_at;
  uint32_t gps_next_on;
  relay_cache_t relay;
  /* Tests start open. A device with no saved setting asks before it answers. */
  bool track_open;
  bool ask_pending;
  uint8_t ask_id[NODE_ID_LEN];
  uint8_t ask_nonce[PAGE_NONCE_LEN];
  struct {
    uint8_t id[NODE_ID_LEN];
    uint8_t nonce[PAGE_NONCE_LEN];
  } ask_wait[TRACK_ASK_MAX];
  int ask_wait_n;
  bool tracked_note;
  uint8_t tracked_id[NODE_ID_LEN];
  uint32_t tracked_quiet;
  /* Allow remembers a contact until this monotonic deadline. Not saved. */
  struct {
    bool on;
    uint8_t id[NODE_ID_LEN];
    uint32_t until;
  } grant[TRACK_GRANT_MAX];
};

void locate_init(locate_radio_t *loc);
void locate_reset(locate_radio_t *loc);
bool locate_busy(const locate_radio_t *loc);
locate_status_t locate_status(const locate_radio_t *loc);
void locate_abort(locate_radio_t *loc);

bool locate_tx(locate_radio_t *loc, struct page_radio *page, const hal_t *hal,
               const uint8_t *dest_id, identity_store_t *identity, contact_book_t *contacts);
/* Ask again. A failed refresh keeps the last target fix and its time. */
bool locate_refresh(locate_radio_t *loc, struct page_radio *page, const hal_t *hal,
                    identity_store_t *identity, contact_book_t *contacts);

void locate_on_raw(locate_radio_t *loc, identity_store_t *identity, contact_book_t *contacts,
                   const hal_t *hal, const raw_frame_t *raw);
void locate_tick(locate_radio_t *loc, identity_store_t *identity, contact_book_t *contacts,
                 const hal_t *hal);
/* Power the receiver, keep one fix, then sleep until the next period. */
void locate_gps_tick(locate_radio_t *loc, const hal_t *hal);
/* Run a sample on the next tick instead of waiting out the period. */
void locate_gps_soon(locate_radio_t *loc);
/* Read one fix now. Used when the unit returns to flat and still. */
void locate_gps_capture(locate_radio_t *loc, const hal_t *hal);

/* Open tracking answers a paired contact immediately. Otherwise the unit asks,
   unless Allow for that contact is still inside TRACK_ALLOW_MS. */
void locate_set_open(locate_radio_t *loc, bool open);
bool locate_is_open(const locate_radio_t *loc);
bool locate_ask_pending(const locate_radio_t *loc);
bool locate_ask_id(const locate_radio_t *loc, uint8_t id[NODE_ID_LEN]);
bool locate_allow(locate_radio_t *loc, const hal_t *hal, identity_store_t *identity,
                  contact_book_t *contacts);
bool locate_deny(locate_radio_t *loc, const hal_t *hal, identity_store_t *identity,
                 contact_book_t *contacts);
bool locate_tracked(const locate_radio_t *loc);
bool locate_tracked_id(const locate_radio_t *loc, uint8_t id[NODE_ID_LEN]);
void locate_clear_tracked(locate_radio_t *loc);

typedef struct {
  char dist[24];
  char when[8];
  bool arrow;
  int north_ft;
  int east_ft;
} locate_view_t;

/* Distance text, and the foot offset used to point an arrow. Coordinates are not shown. */
void locate_format(const locate_radio_t *loc, const hal_t *hal, locate_view_t *view);
