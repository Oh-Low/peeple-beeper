#pragma once

#include "canned.h"
#include "contacts.h"
#include "display.h"
#include "hal.h"
#include "identity.h"
#include "page.h"
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
} pager_screen_t;

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
  page_radio_t *radio;
  locate_radio_t *locate;
  pager_screen_t screen;
  int sel;
  /* First visible settings row. The window moves when the selection hits an edge. */
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
  /* Main menu. On shows the clock and battery above the words. Saved in the store. */
  bool show_clock_bat;
  /* Invert the finished frame on every screen. Saved in the store. Off by default. */
  bool dark_mode;
  /* Which mascot fills the home-screen slot. Saved in the store. 0 is Voidfarer. */
  int beeple;
  /* Right 64×64 of the main menu. Null leaves that half blank. */
  const uint8_t *menu_art;
  int menu_art_w;
  int menu_art_h;
  bool menu_art_invert;
  char sent_to[PAGE_FROM_MAX + 1];
  char sent_body[PAGE_BODY_MAX + 1];
  /* When the Sending screen opened. Retry replaces it after PAGE_RETRY_FAST_MS. */
  uint32_t send_shown_ms;
  char loc_name[PAGE_FROM_MAX + 1];
  bool loc_busy;
  bool loc_holding;
  uint32_t loc_hold_since;
  /* When the locate wait screen opened. Looking, then Found. */
  uint32_t loc_ui_ms;
  bool loc_need_refresh;
  char track_name[PAGE_FROM_MAX + 1];
} pager_t;

void pager_init(pager_t *pager, identity_store_t *identity, contact_book_t *contacts,
                pair_session_t *pair, page_radio_t *radio, locate_radio_t *locate);
/* Picture for the right half of the main menu. bits is a 1-bit sprite in the
   display_blit_sprite layout (page * w + x, bit 0 at the top of the page).
   The sprite is centered in the 64×64 slot and clipped to it. invert swaps
   ink and paper inside that sprite. Pass NULL to clear the slot. */
void pager_set_menu_art(pager_t *pager, const uint8_t *bits, int w, int h, bool invert);
/* Choose a Beeple by index and put it in the home-screen slot. Out of range
   selects Voidfarer. */
void pager_set_beeple(pager_t *pager, int id);
void pager_poll_locate(pager_t *pager);
void pager_poll_pose(pager_t *pager, const hal_t *hal);
/* Returns true when a tracking prompt or warning just took the screen. */
bool pager_poll_track(pager_t *pager, const hal_t *hal);
void pager_on_page(pager_t *pager, const hal_t *hal, const page_t *page);
void pager_go_menu(pager_t *pager);
/* After a finished pair, open the Peeple menu on Add. */
void pager_finish_pair(pager_t *pager);
void pager_input(pager_t *pager, const hal_t *hal);
void pager_render(pager_t *pager, const hal_t *hal);
bool pager_handle_text(pager_t *pager, const char *key);
void pager_poll_send(pager_t *pager, const hal_t *hal);
void pager_reset(pager_t *pager);
