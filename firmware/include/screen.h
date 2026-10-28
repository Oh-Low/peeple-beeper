#pragma once

#include "canned.h"
#include "contacts.h"
#include "display.h"
#include "hal.h"
#include "identity.h"
#include "locate.h"
#include "message.h"
#include "pair.h"
#include "textedit.h"

#include <stdbool.h>
#include <stdint.h>

#define PAGE_KEEP_MAX 256
#define CLOCK_NONE 255

typedef enum {
  SCREEN_MENU = 0,
  SCREEN_INBOX,
  SCREEN_READ,
  SCREEN_COMPOSE,
  SCREEN_CANNED,
  SCREEN_ALERT,
  SCREEN_PAIR,
  SCREEN_SETTINGS,
  SCREEN_BEEPLE,
  SCREEN_PEEPLE,
  SCREEN_NAMEEDIT,
  SCREEN_BODYEDIT,
  SCREEN_SENDING,
  SCREEN_SENT,
  SCREEN_FAILED,
  SCREEN_OUTBOX,
  SCREEN_OUTREAD,
  SCREEN_LOCATE,
  SCREEN_LOCATING,
  SCREEN_LOCREAD,
  SCREEN_TRACKASK,
  SCREEN_TRACKED,
} screen_mode_t;

typedef enum {
  OUTBOX_SENDING = 0,
  OUTBOX_SENT,
  OUTBOX_FAILED,
} outbox_status_t;

typedef struct {
  char from[PAGE_FROM_MAX + 1];
  char body[PAGE_BODY_MAX + 1];
  int8_t rssi;
  uint32_t received_ms;
  uint8_t received_hh;
  uint8_t received_mm;
} inbox_row_t;

typedef struct {
  uint8_t dest_id[NODE_ID_LEN];
  char to[PAGE_FROM_MAX + 1];
  char body[PAGE_BODY_MAX + 1];
  outbox_status_t status;
  uint8_t sent_hh;
  uint8_t sent_mm;
  uint8_t acked_hh;
  uint8_t acked_mm;
} outbox_item_t;

typedef struct {
  identity_store_t *identity;
  contact_book_t *contacts;
  pair_session_t *pair;
  message_session_t *radio;
  locate_radio_t *locate;
  screen_mode_t mode;
  int sel;
  /* First visible settings row. */
  int settings_scroll;
  inbox_row_t inbox[PAGE_KEEP_MAX];
  int inbox_n;
  outbox_item_t outbox[PAGE_KEEP_MAX];
  int outbox_n;
  int reading;
  page_t alert;
  bool has_alert;
  uint32_t flash_start;
  uint32_t flash_ms;
  uint8_t buf[DISPLAY_BUF_LEN];
  uint8_t compose_dest[NODE_ID_LEN];
  bool has_compose_dest;
  text_edit_t edit;
  canned_book_t canned;
  bool vibe_test;
  /* Saved. On draws the clock and battery above side-menu labels. */
  bool show_clock_bat;
  /* Saved. Inverts the finished frame. */
  bool dark_mode;
  /* Saved. Starts on. */
  bool backlight_on;
  /* Saved. Index 0 is Voidfarer. */
  int beeple;
  /* Right 64×64 of a side menu. NULL leaves it blank. */
  const uint8_t *menu_art;
  int menu_art_w;
  int menu_art_h;
  char sent_to[PAGE_FROM_MAX + 1];
  char sent_body[PAGE_BODY_MAX + 1];
  char loc_name[PAGE_FROM_MAX + 1];
  bool loc_busy;
  bool loc_holding;
  uint32_t loc_hold_since;
  bool loc_need_refresh;
  bool loc_capture_wait;
  char track_name[PAGE_FROM_MAX + 1];
} screen_t;

void screen_init(screen_t *screen, identity_store_t *identity, contact_book_t *contacts,
                pair_session_t *pair, message_session_t *radio, locate_radio_t *locate);
/* Out of range selects Voidfarer. */
void screen_set_beeple(screen_t *screen, int id);
void screen_poll_locate(screen_t *screen);
void screen_poll_pose(screen_t *screen, const hal_t *hal);
/* Returns true when a tracking prompt or warning just took the screen. */
bool screen_poll_track(screen_t *screen, const hal_t *hal);
void screen_on_page(screen_t *screen, const hal_t *hal, const page_t *page);
void screen_go_menu(screen_t *screen);
/* Opens the Peeple menu on Add. */
void screen_finish_pair(screen_t *screen);
void screen_input(screen_t *screen, const hal_t *hal);
void screen_render(screen_t *screen, const hal_t *hal);
bool screen_handle_text(screen_t *screen, const char *key);
void screen_poll_send(screen_t *screen, const hal_t *hal);
void screen_reset(screen_t *screen);
