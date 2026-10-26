#include "pager.h"

#include "art.h"
#include "beeple.h"
#include "heading.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#define FLASH_MS 400
#define MENU_HALF 64
#define MENU_N 6
#define MENU_PEEPLE 3
#define MENU_SETTINGS 4
#define MENU_BEEPLE 5

#define PEEP_N 2
#define PEEP_LOCATE 0
#define PEEP_CONTACT 1

/* Mascots for the home-screen slot. A filled picture is inverted so the
   character stays ink on paper. The slot centers and clips to 64×64. */
typedef struct {
  const char *name;
  const uint8_t *bits;
  int w;
  int h;
  bool invert;
} beeple_def_t;

static const beeple_def_t BEEPLES[] = {
    {"Voidfarer", art_voidfarers, ART_VOIDFARERS_W, ART_VOIDFARERS_H, false},
    {"Octohat", art_octohat, ART_OCTOHAT_W, ART_OCTOHAT_H, false},
    {"Camnon", art_camnon, ART_CAMNON_W, ART_CAMNON_H, false},
    {"Vibey Eye", art_vibey_eye, ART_VIBEY_EYE_W, ART_VIBEY_EYE_H, false},
    {"Alleycat", art_alleycat, ART_ALLEYCAT_W, ART_ALLEYCAT_H, false},
    {"Beep Bop", art_beepbop, ART_BEEPBOP_W, ART_BEEPBOP_H, false},
    {"Dino", art_dino, ART_DINO_W, ART_DINO_H, false},
    {"Midge", art_midge, ART_MIDGE_W, ART_MIDGE_H, false},
    {"Oscar", art_oscar, ART_OSCAR_W, ART_OSCAR_H, false},
    {"Robort v2", art_robot, ART_ROBOT_W, ART_ROBOT_H, false},
    {"Spooder", art_spooder, ART_SPOODER_W, ART_SPOODER_H, false},
    {"Teef", art_teef, ART_TEEF_W, ART_TEEF_H, false},
};
_Static_assert(sizeof BEEPLES / sizeof BEEPLES[0] == BEEPLE_N, "beeple table");

/* Settings list. Names stay left of a 1 px rule; values sit on the right.
   Five rows fit under the header. A longer list scrolls. */
#define SET_LABEL_X 4
#define SET_LABEL_END 60
#define SET_RULE_X 63
#define SET_VALUE_MIN 65
#define SET_VALUE_END 124
#define SET_Y0 12
#define SET_PITCH 10
#define SET_VIS 5
#define SET_BAR_X 1
#define SET_BAR_W 61
#define SET_BAR_H 8

enum {
  SET_BACK = 0,
  SET_NAME,
  SET_TRACK,
  SET_CLOCK,
  SET_DARK,
  SET_VIBE,
  SET_N
};

void pager_init(pager_t *pager, identity_store_t *identity, contact_book_t *contacts,
                pair_session_t *pair, page_radio_t *radio, locate_radio_t *locate) {
  memset(pager, 0, sizeof *pager);
  pager->identity = identity;
  pager->contacts = contacts;
  pager->pair = pair;
  pager->radio = radio;
  pager->locate = locate;
  pager->screen = SCREEN_MENU;
  pager->show_clock_bat = true;
  pager_set_beeple(pager, 0);
  text_edit_init(&pager->edit);
  canned_init(&pager->canned);
}

void pager_set_menu_art(pager_t *pager, const uint8_t *bits, int w, int h, bool invert) {
  if (!pager) return;
  if (!bits || w <= 0 || h <= 0) {
    pager->menu_art = NULL;
    pager->menu_art_w = 0;
    pager->menu_art_h = 0;
    pager->menu_art_invert = false;
    return;
  }
  pager->menu_art = bits;
  pager->menu_art_w = w;
  pager->menu_art_h = h;
  pager->menu_art_invert = invert;
}

void pager_set_beeple(pager_t *pager, int id) {
  if (!pager) return;
  if (id < 0 || id >= BEEPLE_N) id = 0;
  pager->beeple = id;
  const beeple_def_t *b = &BEEPLES[id];
  pager_set_menu_art(pager, b->bits, b->w, b->h, b->invert);
}

static bool deadline_live(uint32_t now, uint32_t start, uint32_t ms) {
  return ms > 0 && elapsed_u32(now, start) < ms;
}

static void pad2(char *out, int n) {
  if (n < 10) snprintf(out, 8, "0%d", n);
  else snprintf(out, 8, "%d", n);
}

static int clamp_hour(int n) {
  if (n < 0) return 0;
  if (n > 23) return 23;
  return n;
}

static int clamp_minute(int n) {
  if (n < 0) return 0;
  if (n > 59) return 59;
  return n;
}

static void stamp_clock(const hal_t *hal, uint8_t *hh, uint8_t *mm) {
  uint8_t h = 0, m = 0;
  hal->clock(hal->ctx, &h, &m);
  *hh = (uint8_t)clamp_hour(h);
  *mm = (uint8_t)clamp_minute(m);
}

static void format_hm(char *out, size_t cap, int hh, int mm) {
  if (hh > 23 || mm > 59) {
    str_copy_max(out, cap, "--:--", 5);
    return;
  }
  char a[8], b[8];
  pad2(a, hh);
  pad2(b, mm);
  snprintf(out, cap, "%s:%s", a, b);
}

static void copy_page(const page_t *src, page_t *dst) {
  str_copy_max(dst->from, sizeof dst->from, src->from, PAGE_FROM_MAX);
  str_copy_max(dst->body, sizeof dst->body, src->body, PAGE_BODY_MAX);
  dst->rssi = src->rssi;
  dst->received_ms = src->received_ms;
  dst->beeple = src->beeple;
}

static void push_outbox(pager_t *pager, const uint8_t *dest_id, const char *to, const char *body,
                        const hal_t *hal) {
  if (pager->outbox_n >= PAGE_KEEP_MAX) pager->outbox_n = PAGE_KEEP_MAX;
  if (pager->outbox_n == PAGE_KEEP_MAX) pager->outbox_n--;
  if (pager->outbox_n > 0) {
    memmove(&pager->outbox[1], &pager->outbox[0], (size_t)pager->outbox_n * sizeof(outbox_item_t));
  }
  outbox_item_t *row = &pager->outbox[0];
  memset(row, 0, sizeof *row);
  memcpy(row->dest_id, dest_id, NODE_ID_LEN);
  str_copy_max(row->to, sizeof row->to, to, PAGE_FROM_MAX);
  str_copy_max(row->body, sizeof row->body, body, PAGE_BODY_MAX);
  row->status = OUTBOX_SENDING;
  stamp_clock(hal, &row->sent_hh, &row->sent_mm);
  row->acked_hh = CLOCK_NONE;
  row->acked_mm = CLOCK_NONE;
  pager->outbox_n++;
}

static void finish_outbox(pager_t *pager, outbox_status_t next, const hal_t *hal) {
  for (int i = 0; i < pager->outbox_n; i++) {
    outbox_item_t *row = &pager->outbox[i];
    if (row->status == OUTBOX_SENDING) {
      row->status = next;
      if (next == OUTBOX_SENT) {
        stamp_clock(hal, &row->acked_hh, &row->acked_mm);
      }
      return;
    }
  }
}

static void promote_outbox(pager_t *pager, int idx) {
  if (idx <= 0 || idx >= pager->outbox_n) {
    if (idx >= 0 && idx < pager->outbox_n) {
      pager->outbox[idx].status = OUTBOX_SENDING;
      pager->outbox[idx].acked_hh = CLOCK_NONE;
      pager->outbox[idx].acked_mm = CLOCK_NONE;
    }
    return;
  }
  outbox_item_t item = pager->outbox[idx];
  memmove(&pager->outbox[1], &pager->outbox[0], (size_t)idx * sizeof(outbox_item_t));
  pager->outbox[0] = item;
  pager->outbox[0].status = OUTBOX_SENDING;
  pager->outbox[0].acked_hh = CLOCK_NONE;
  pager->outbox[0].acked_mm = CLOCK_NONE;
}

static void push_inbox(pager_t *pager, const page_t *p, const hal_t *hal) {
  if (pager->inbox_n == PAGE_KEEP_MAX) pager->inbox_n--;
  if (pager->inbox_n > 0) {
    memmove(&pager->inbox[1], &pager->inbox[0], (size_t)pager->inbox_n * sizeof(inbox_row_t));
  }
  inbox_row_t *row = &pager->inbox[0];
  memset(row, 0, sizeof *row);
  page_t tmp;
  memset(&tmp, 0, sizeof tmp);
  copy_page(p, &tmp);
  str_copy_max(row->from, sizeof row->from, tmp.from, PAGE_FROM_MAX);
  str_copy_max(row->body, sizeof row->body, tmp.body, PAGE_BODY_MAX);
  row->rssi = tmp.rssi;
  row->received_ms = tmp.received_ms;
  stamp_clock(hal, &row->received_hh, &row->received_mm);
  pager->inbox_n++;
}

static void clamp_sel(pager_t *pager, int max) {
  if (max <= 0) {
    pager->sel = 0;
    return;
  }
  if (pager->sel < 0) pager->sel = max - 1;
  if (pager->sel >= max) pager->sel = 0;
}

static void begin_name_edit(pager_t *pager) {
  pager->screen = SCREEN_NAMEEDIT;
  char name[PAGE_FROM_MAX + 1];
  identity_get_display_name(pager->identity, name, sizeof name);
  text_edit_begin(&pager->edit, name, PAGE_FROM_MAX, false);
}

static void begin_body_edit(pager_t *pager) {
  pager->screen = SCREEN_BODYEDIT;
  text_edit_begin(&pager->edit, "", PAGE_BODY_MAX, true);
}

static void save_name_edit(pager_t *pager) {
  char draft[PAGE_FROM_MAX + 1];
  text_edit_draft(&pager->edit, draft, sizeof draft);
  identity_set_display_name(pager->identity, draft);
  pager->screen = SCREEN_SETTINGS;
  pager->sel = 1;
}

static void cancel_name_edit(pager_t *pager) {
  pager->screen = SCREEN_SETTINGS;
  pager->sel = 1;
}

static void send_page(pager_t *pager, const hal_t *hal, const uint8_t *dest, const char *body,
                      int reuse_idx) {
  if (!body || !body[0]) return;
  if (page_busy(pager->radio)) return;
  if (!page_tx(pager->radio, hal, dest, body, pager->identity, pager->contacts, pager->locate,
               (uint8_t)pager->beeple)) {
    return;
  }
  const contact_t *contact = contacts_find_by_id(pager->contacts, dest);
  str_copy_max(pager->sent_to, sizeof pager->sent_to, contact ? contact->name : "", PAGE_FROM_MAX);
  str_copy_max(pager->sent_body, sizeof pager->sent_body, body, PAGE_BODY_MAX);
  if (reuse_idx >= 0) promote_outbox(pager, reuse_idx);
  else push_outbox(pager, dest, pager->sent_to, pager->sent_body, hal);
  pager->screen = SCREEN_SENDING;
  pager->sel = 0;
  pager->send_shown_ms = hal->now_ms(hal->ctx);
}

static void retry_outbox(pager_t *pager, const hal_t *hal, int idx) {
  if (idx < 0 || idx >= pager->outbox_n) return;
  outbox_item_t *item = &pager->outbox[idx];
  if (item->status != OUTBOX_FAILED) return;
  send_page(pager, hal, item->dest_id, item->body, idx);
}

static int canned_msg_n(const pager_t *pager) {
  return pager->canned.n + CANNED_DEFAULT_N;
}

static int canned_rows(pager_t *pager) {
  return canned_msg_n(pager) + 1;
}

static int compose_contact_sel(const pager_t *pager) {
  if (!pager->has_compose_dest || !pager->contacts) return 0;
  for (int i = 0; i < pager->contacts->n; i++) {
    if (memcmp(pager->contacts->rows[i].node_id, pager->compose_dest, NODE_ID_LEN) == 0) return i;
  }
  return 0;
}

static void send_body_edit(pager_t *pager, const hal_t *hal) {
  char body[PAGE_BODY_MAX + 1];
  text_edit_draft(&pager->edit, body, sizeof body);
  if (!pager->has_compose_dest || !body[0]) {
    pager->screen = SCREEN_CANNED;
    pager->sel = canned_msg_n(pager);
    return;
  }
  send_page(pager, hal, pager->compose_dest, body, -1);
}

static void cancel_body_edit(pager_t *pager) {
  pager->screen = SCREEN_CANNED;
  pager->sel = canned_msg_n(pager);
}

static void save_canned_draft(pager_t *pager, const hal_t *hal) {
  char draft[PAGE_BODY_MAX + 1];
  text_edit_draft(&pager->edit, draft, sizeof draft);
  if (!canned_add(&pager->canned, draft)) return;
  hal->vibe(hal->ctx, 80);
}

static void dismiss_sent(pager_t *pager, const hal_t *hal) {
  if (pager->has_alert) {
    pager->screen = SCREEN_ALERT;
    pager->flash_start = hal->now_ms(hal->ctx);
    pager->flash_ms = FLASH_MS;
    return;
  }
  pager->screen = SCREEN_OUTBOX;
  pager->sel = 0;
}

static void on_turn(pager_t *pager, int dir) {
  if (pager->screen == SCREEN_ALERT || pager->screen == SCREEN_READ ||
      pager->screen == SCREEN_PAIR || pager->screen == SCREEN_SENDING ||
      pager->screen == SCREEN_SENT || pager->screen == SCREEN_FAILED ||
      pager->screen == SCREEN_OUTREAD || pager->screen == SCREEN_LOCATING ||
      pager->screen == SCREEN_LOCREAD || pager->screen == SCREEN_TRACKED) {
    return;
  }
  if (pager->screen == SCREEN_NAMEEDIT || pager->screen == SCREEN_BODYEDIT) {
    text_edit_turn(&pager->edit, dir);
    return;
  }
  pager->sel += dir;
  if (pager->screen == SCREEN_MENU) clamp_sel(pager, MENU_N);
  else if (pager->screen == SCREEN_PEEPLE) clamp_sel(pager, PEEP_N);
  else if (pager->screen == SCREEN_INBOX) {
    if (pager->inbox_n <= 0) pager->sel = 0;
    else clamp_sel(pager, pager->inbox_n);
  }
  else if (pager->screen == SCREEN_OUTBOX) {
    if (pager->outbox_n <= 0) pager->sel = 0;
    else clamp_sel(pager, pager->outbox_n);
  }
  else if (pager->screen == SCREEN_COMPOSE) {
    if (!pager->contacts || pager->contacts->n <= 0) pager->sel = 0;
    else clamp_sel(pager, pager->contacts->n);
  }
  else if (pager->screen == SCREEN_CANNED) clamp_sel(pager, canned_rows(pager));
  else if (pager->screen == SCREEN_SETTINGS) clamp_sel(pager, SET_N);
  else if (pager->screen == SCREEN_BEEPLE) clamp_sel(pager, BEEPLE_N);
  else if (pager->screen == SCREEN_LOCATE) clamp_sel(pager, pager->contacts->n + 1);
  else if (pager->screen == SCREEN_TRACKASK) clamp_sel(pager, 2);
}

static void stop_vibe_test(pager_t *pager, const hal_t *hal) {
  pager->vibe_test = false;
  hal->vibe(hal->ctx, 0);
}

static void on_back(pager_t *pager, const hal_t *hal) {
  if (pager->screen == SCREEN_SENDING) {
    pager->screen = SCREEN_OUTBOX;
    pager->sel = 0;
    return;
  }
  if (pager->screen == SCREEN_PAIR) {
    pair_abort(pager->pair, hal);
    pager->screen = SCREEN_PEEPLE;
    pager->sel = PEEP_CONTACT;
    pager->has_compose_dest = false;
    return;
  }
  if (pager->screen == SCREEN_NAMEEDIT) {
    cancel_name_edit(pager);
    return;
  }
  if (pager->screen == SCREEN_BODYEDIT) {
    cancel_body_edit(pager);
    return;
  }
  if (pager->screen == SCREEN_SETTINGS) {
    stop_vibe_test(pager, hal);
    pager->screen = SCREEN_MENU;
    pager->sel = MENU_SETTINGS;
    return;
  }
  if (pager->screen == SCREEN_BEEPLE) {
    pager->screen = SCREEN_MENU;
    pager->sel = MENU_BEEPLE;
    return;
  }
  if (pager->screen == SCREEN_PEEPLE) {
    pager->screen = SCREEN_MENU;
    pager->sel = MENU_PEEPLE;
    return;
  }
  if (pager->screen == SCREEN_TRACKASK) {
    locate_deny(pager->locate, hal, pager->identity, pager->contacts);
    pager->screen = SCREEN_MENU;
    pager->sel = 0;
    return;
  }
  if (pager->screen == SCREEN_TRACKED) {
    pager->screen = SCREEN_MENU;
    pager->sel = 0;
    return;
  }
  if (pager->screen == SCREEN_ALERT) {
    pager->screen = SCREEN_READ;
    pager->reading = 0;
    pager->has_alert = false;
    return;
  }
  if (pager->screen == SCREEN_SENT || pager->screen == SCREEN_FAILED) {
    dismiss_sent(pager, hal);
    return;
  }
  if (pager->screen == SCREEN_CANNED) {
    int sel = compose_contact_sel(pager);
    pager->screen = SCREEN_COMPOSE;
    pager->has_compose_dest = false;
    pager->sel = sel;
    return;
  }
  if (pager->screen == SCREEN_OUTREAD) {
    pager->screen = SCREEN_OUTBOX;
    pager->sel = pager->reading;
    if (pager->outbox_n <= 0) pager->sel = 0;
    else if (pager->sel >= pager->outbox_n) pager->sel = pager->outbox_n - 1;
    return;
  }
  if (pager->screen == SCREEN_OUTBOX) {
    pager->screen = SCREEN_MENU;
    pager->sel = 2;
    return;
  }
  if (pager->screen == SCREEN_LOCATING) {
    locate_abort(pager->locate);
    pager->loc_busy = false;
    pager->screen = SCREEN_LOCATE;
    pager->sel = pager->contacts->n ? 1 : 0;
    return;
  }
  if (pager->screen == SCREEN_LOCREAD) {
    if (locate_busy(pager->locate)) locate_abort(pager->locate);
    pager->loc_busy = false;
    pager->loc_need_refresh = false;
    pager->screen = SCREEN_LOCATE;
    pager->sel = pager->contacts->n ? 1 : 0;
    return;
  }
  if (pager->screen == SCREEN_LOCATE) {
    pager->screen = SCREEN_PEEPLE;
    pager->sel = PEEP_LOCATE;
    return;
  }
  if (pager->screen == SCREEN_READ || pager->screen == SCREEN_INBOX ||
      pager->screen == SCREEN_COMPOSE) {
    pager->screen = SCREEN_MENU;
    pager->sel = 0;
    pager->has_compose_dest = false;
  }
}

static void on_press(pager_t *pager, const hal_t *hal) {
  if (pager->screen == SCREEN_PAIR || pager->screen == SCREEN_SENDING ||
      pager->screen == SCREEN_LOCATING) {
    return;
  }
  if (pager->screen == SCREEN_LOCREAD) {
    if (locate_busy(pager->locate)) locate_abort(pager->locate);
    pager->loc_busy = false;
    pager->loc_need_refresh = false;
    pager->screen = SCREEN_LOCATE;
    pager->sel = pager->contacts->n ? 1 : 0;
    return;
  }

  if (pager->screen == SCREEN_SENT || pager->screen == SCREEN_FAILED) {
    dismiss_sent(pager, hal);
    return;
  }
  if (pager->screen == SCREEN_ALERT && pager->has_alert) {
    pager->reading = 0;
    pager->screen = SCREEN_READ;
    pager->has_alert = false;
    return;
  }
  if (pager->screen == SCREEN_READ) {
    pager->screen = SCREEN_INBOX;
    pager->sel = pager->reading;
    if (pager->inbox_n <= 0) pager->sel = 0;
    else if (pager->sel >= pager->inbox_n) pager->sel = pager->inbox_n - 1;
    return;
  }
  if (pager->screen == SCREEN_MENU) {
    if (pager->sel == 0) {
      pager->screen = SCREEN_INBOX;
      pager->sel = 0;
    } else if (pager->sel == 1) {
      pager->screen = SCREEN_COMPOSE;
      pager->has_compose_dest = false;
      pager->sel = 0;
    } else if (pager->sel == 2) {
      pager->screen = SCREEN_OUTBOX;
      pager->sel = 0;
    } else if (pager->sel == MENU_PEEPLE) {
      pager->screen = SCREEN_PEEPLE;
      pager->sel = PEEP_LOCATE;
    } else if (pager->sel == MENU_SETTINGS) {
      pager->screen = SCREEN_SETTINGS;
      pager->sel = SET_NAME;
      pager->settings_scroll = 0;
    } else if (pager->sel == MENU_BEEPLE) {
      pager->screen = SCREEN_BEEPLE;
      pager->sel = pager->beeple;
    }
    return;
  }
  if (pager->screen == SCREEN_BEEPLE) {
    pager_set_beeple(pager, pager->sel);
    pager->screen = SCREEN_MENU;
    pager->sel = MENU_BEEPLE;
    return;
  }
  if (pager->screen == SCREEN_PEEPLE) {
    if (pager->sel == PEEP_LOCATE) {
      pager->loc_busy = false;
      pager->screen = SCREEN_LOCATE;
      pager->sel = pager->contacts->n ? 1 : 0;
    } else if (pager->sel == PEEP_CONTACT) {
      pair_start(pager->pair, pager->identity, hal, (uint8_t)pager->beeple);
      if (pair_active(pager->pair)) {
        pager->screen = SCREEN_PAIR;
        pager->sel = 0;
      }
    }
    return;
  }
  if (pager->screen == SCREEN_SETTINGS) {
    if (pager->sel == SET_BACK) {
      stop_vibe_test(pager, hal);
      pager->screen = SCREEN_MENU;
      pager->sel = MENU_SETTINGS;
      return;
    }
    if (pager->sel == SET_NAME) {
      stop_vibe_test(pager, hal);
      begin_name_edit(pager);
      return;
    }
    if (pager->sel == SET_TRACK) {
      locate_set_open(pager->locate, !locate_is_open(pager->locate));
      return;
    }
    if (pager->sel == SET_CLOCK) {
      pager->show_clock_bat = !pager->show_clock_bat;
      return;
    }
    if (pager->sel == SET_DARK) {
      pager->dark_mode = !pager->dark_mode;
      return;
    }
    if (pager->sel == SET_VIBE) {
      pager->vibe_test = true;
      hal->vibe(hal->ctx, 250);
    }
    return;
  }
  if (pager->screen == SCREEN_TRACKASK) {
    if (pager->sel == 0) {
      locate_allow(pager->locate, hal, pager->identity, pager->contacts);
    } else {
      locate_deny(pager->locate, hal, pager->identity, pager->contacts);
    }
    pager->screen = SCREEN_MENU;
    pager->sel = 0;
    return;
  }
  if (pager->screen == SCREEN_TRACKED) {
    pager->screen = SCREEN_MENU;
    pager->sel = 0;
    return;
  }
  if (pager->screen == SCREEN_NAMEEDIT) {
    if (text_edit_press(&pager->edit) == TEXT_PRESS_OK) save_name_edit(pager);
    return;
  }
  if (pager->screen == SCREEN_BODYEDIT) {
    text_edit_press_t result = text_edit_press(&pager->edit);
    if (result == TEXT_PRESS_OK) send_body_edit(pager, hal);
    else if (result == TEXT_PRESS_SAVE) save_canned_draft(pager, hal);
    return;
  }
  if (pager->screen == SCREEN_INBOX) {
    if (pager->sel >= 0 && pager->sel < pager->inbox_n) {
      pager->reading = pager->sel;
      pager->screen = SCREEN_READ;
    }
    return;
  }
  if (pager->screen == SCREEN_OUTBOX) {
    if (pager->sel >= 0 && pager->sel < pager->outbox_n) {
      if (pager->outbox[pager->sel].status == OUTBOX_FAILED) {
        retry_outbox(pager, hal, pager->sel);
        return;
      }
      pager->reading = pager->sel;
      pager->screen = SCREEN_OUTREAD;
    }
    return;
  }
  if (pager->screen == SCREEN_OUTREAD) {
    if (pager->reading >= 0 && pager->reading < pager->outbox_n &&
        pager->outbox[pager->reading].status == OUTBOX_FAILED) {
      retry_outbox(pager, hal, pager->reading);
      return;
    }
    pager->screen = SCREEN_OUTBOX;
    pager->sel = pager->reading;
    if (pager->outbox_n <= 0) pager->sel = 0;
    else if (pager->sel >= pager->outbox_n) pager->sel = pager->outbox_n - 1;
    return;
  }
  if (pager->screen == SCREEN_COMPOSE) {
    if (!pager->contacts || pager->sel < 0 || pager->sel >= pager->contacts->n) return;
    memcpy(pager->compose_dest, pager->contacts->rows[pager->sel].node_id, NODE_ID_LEN);
    pager->has_compose_dest = true;
    pager->screen = SCREEN_CANNED;
    pager->sel = 0;
    return;
  }
  if (pager->screen == SCREEN_CANNED) {
    int n = canned_msg_n(pager);
    if (pager->sel == n) {
      begin_body_edit(pager);
      return;
    }
    if (!pager->has_compose_dest) return;
    char list[CANNED_SAVED_MAX + CANNED_DEFAULT_N][PAGE_BODY_MAX + 1];
    int listed = canned_list(&pager->canned, list, CANNED_SAVED_MAX + CANNED_DEFAULT_N);
    if (pager->sel >= 0 && pager->sel < listed) send_page(pager, hal, pager->compose_dest, list[pager->sel], -1);
    return;
  }
  if (pager->screen == SCREEN_LOCATE) {
    if (pager->sel == 0) {
      pager->screen = SCREEN_PEEPLE;
      pager->sel = PEEP_LOCATE;
      return;
    }
    if (pager->contacts->n == 0) return;
    int idx = pager->sel - 1;
    if (idx < 0 || idx >= pager->contacts->n) return;
    const contact_t *c = &pager->contacts->rows[idx];
    str_copy_max(pager->loc_name, sizeof pager->loc_name, c->name, PAGE_FROM_MAX);
    if (page_busy(pager->radio) ||
        !locate_tx(pager->locate, pager->radio, hal, c->node_id, pager->identity, pager->contacts)) {
      pager->loc_busy = true;
      pager->screen = SCREEN_LOCREAD;
      return;
    }
    pager->loc_busy = false;
    pager->screen = SCREEN_LOCATING;
    pager->loc_ui_ms = hal->now_ms(hal->ctx);
  }
}

void pager_on_page(pager_t *pager, const hal_t *hal, const page_t *page) {
  push_inbox(pager, page, hal);
  memset(&pager->alert, 0, sizeof pager->alert);
  copy_page(page, &pager->alert);
  pager->has_alert = true;
  pager->flash_start = hal->now_ms(hal->ctx);
  pager->flash_ms = FLASH_MS;
  hal->vibe(hal->ctx, 250);
  hal->beep(hal->ctx, 80);
  if (pager->screen == SCREEN_SENDING) return;
  pager->screen = SCREEN_ALERT;
}

void pager_go_menu(pager_t *pager) {
  pager->screen = SCREEN_MENU;
  pager->sel = 0;
  pager->has_compose_dest = false;
  pager->has_alert = false;
}

void pager_finish_pair(pager_t *pager) {
  pager_go_menu(pager);
  pager->screen = SCREEN_PEEPLE;
  pager->sel = PEEP_CONTACT;
}

static void header(uint8_t *buf, const char *title, const hal_t *hal) {
  (void)hal;
  display_draw_header(buf, title);
}

static void draw_list(pager_t *pager, uint8_t *buf, const char rows[][DISPLAY_WIDTH + 1], int nrows) {
  const int top = 12;
  const int vis = 4;
  int start = 0;
  if (pager->sel >= vis) start = pager->sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= nrows) break;
    int y = top + i * 12;
    display_draw_text(buf, 2, y, rows[idx]);
    if (idx == pager->sel) display_invert_rect(buf, 0, y - 1, DISPLAY_WIDTH, 10);
  }
}

static void invert_cut_corner(uint8_t *buf, int x, int y, int w, int h) {
  for (int yy = 0; yy < h; yy++) {
    int from_bottom = h - 1 - yy;
    int trim = h - from_bottom;
    if (trim > w) trim = w;
    display_invert_rect(buf, x, y + yy, w - trim, 1);
  }
}

static void blit_slot(uint8_t *buf, const uint8_t *bits, int sw, int sh, bool invert, int dx, int dy,
                      int bw, int bh) {
  if (!bits || sw <= 0 || sh <= 0 || bw <= 0 || bh <= 0) return;
  int vis_w = sw > bw ? bw : sw;
  int vis_h = sh > bh ? bh : sh;
  int src_x = (sw - vis_w) / 2;
  int src_y = (sh - vis_h) / 2;
  int ox = dx + (bw - vis_w) / 2;
  int oy = dy + (bh - vis_h) / 2;
  for (int y = 0; y < vis_h; y++) {
    int sy = src_y + y;
    int page = sy >> 3;
    uint8_t bit = (uint8_t)(1u << (sy & 7));
    for (int x = 0; x < vis_w; x++) {
      int i = page * sw + src_x + x;
      bool on = (bits[i] & bit) != 0;
      if (invert) on = !on;
      display_set_pixel(buf, ox + x, oy + y, on);
    }
  }
}

static void draw_art_box(const pager_t *pager, uint8_t *buf, int dx, int dy, int bw, int bh) {
  blit_slot(buf, pager->menu_art, pager->menu_art_w, pager->menu_art_h, pager->menu_art_invert, dx, dy,
            bw, bh);
}

static void draw_menu_art(const pager_t *pager, uint8_t *buf) {
  draw_art_box(pager, buf, MENU_HALF, 0, MENU_HALF, MENU_HALF);
}

/* Slanted bar: the top-left and bottom-right corners are cut. */
static void invert_slant_bar(uint8_t *buf, int x, int y, int w, int h, int cut_left, int cut_right) {
  for (int yy = 0; yy < h; yy++) {
    int from_bottom = h - 1 - yy;
    int left = cut_left - yy;
    if (left < 1) left = 1;
    int right = cut_right - from_bottom;
    if (right < 0) right = 0;
    int span = w - left - right;
    if (span > 0) display_invert_rect(buf, x + left, y + yy, span, 1);
  }
}

/* Title sits in the slanted bar. Stripes run off the right edge. */
static void draw_slant_bar(uint8_t *buf, const char *title) {
  display_draw_text_small(buf, 11, 3, title);
  invert_slant_bar(buf, 0, 1, 64, 9, 6, 8);
  for (int k = 0; k < 3; k++) {
    for (int yy = 0; yy <= 7 - k; yy++) {
      int x0 = 66 - yy + k * 5;
      for (int dx = 0; dx < 3; dx++) display_set_pixel(buf, x0 + dx, 1 + yy, true);
    }
  }
}

/* Title sits in the slanted bar. The clock sits on the open right side. */
static void draw_slant_header(uint8_t *buf, const hal_t *hal, const char *title) {
  draw_slant_bar(buf, title);
  uint8_t hh = 0, mm = 0;
  hal->clock(hal->ctx, &hh, &mm);
  char clock[8];
  format_hm(clock, sizeof clock, hh, mm);
  display_draw_text_small(buf, 107, 3, clock);
}

static void draw_side_menu(pager_t *pager, const hal_t *hal, uint8_t *buf, char labels[][24], int n) {
  int y0 = 4;
  int pitch = 10;
  if (pager->show_clock_bat) {
    uint8_t hh = 0, mm = 0;
    hal->clock(hal->ctx, &hh, &mm);
    char clock[8];
    format_hm(clock, sizeof clock, hh, mm);
    display_draw_text(buf, 4, 1, clock);
    char bat[8];
    snprintf(bat, sizeof bat, "%u%%", (unsigned)hal->battery_pct(hal->ctx));
    display_draw_text(buf, MENU_HALF - 2 - display_text_width(bat), 1, bat);
    y0 = 12;
    pitch = 8;
  }
  int vis = 0;
  while (vis < n && y0 + vis * pitch + CHAR_H <= DISPLAY_HEIGHT) vis++;
  int start = 0;
  if (pager->sel >= vis) start = pager->sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= n) break;
    int y = y0 + i * pitch;
    display_draw_text(buf, 4, y, labels[idx]);
    if (idx == pager->sel) invert_cut_corner(buf, 1, y - 1, MENU_HALF - 2, pitch);
  }
  draw_menu_art(pager, buf);
}

static void draw_menu(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  int count = pager->inbox_n;
  bool pending = page_busy(pager->radio);
  char labels[MENU_N][24];
  if (count) snprintf(labels[0], sizeof labels[0], "Inbox (%d)", count);
  else str_copy_max(labels[0], sizeof labels[0], "Inbox", 16);
  str_copy_max(labels[1], sizeof labels[1], "Compose", 16);
  str_copy_max(labels[2], sizeof labels[2], pending ? "Outbox (*)" : "Outbox", 16);
  str_copy_max(labels[3], sizeof labels[3], "Peeple", 16);
  str_copy_max(labels[4], sizeof labels[4], "Settings", 16);
  str_copy_max(labels[5], sizeof labels[5], "Beeple", 16);
  draw_side_menu(pager, hal, buf, labels, MENU_N);
}

static void draw_peeple(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  char labels[PEEP_N][24];
  str_copy_max(labels[0], sizeof labels[0], "Locate", 16);
  str_copy_max(labels[1], sizeof labels[1], "Add", 16);
  draw_side_menu(pager, hal, buf, labels, PEEP_N);
}

static void draw_beeple(pager_t *pager, uint8_t *buf) {
  const int y0 = 8;
  const int pitch = 10;
  int sel = pager->sel;
  if (sel < 0 || sel >= BEEPLE_N) sel = 0;
  int vis = 0;
  while (vis < BEEPLE_N && y0 + vis * pitch + CHAR_H <= DISPLAY_HEIGHT) vis++;
  int start = 0;
  if (sel >= vis) start = sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= BEEPLE_N) break;
    int y = y0 + i * pitch;
    display_draw_text(buf, 4, y, BEEPLES[idx].name);
    if (idx == sel) invert_cut_corner(buf, 1, y - 1, MENU_HALF - 2, pitch);
  }
  const beeple_def_t *b = &BEEPLES[sel];
  blit_slot(buf, b->bits, b->w, b->h, b->invert, MENU_HALF, 0, MENU_HALF, MENU_HALF);
}

/* How many 3×6 glyphs fit in a pixel width. */
static int small_chars_fit(int width) {
  if (width < 3) return 0;
  return (width - 3) / FONT_SMALL_W + 1;
}

static void draw_inbox(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  const int y0 = 16;
  const int pitch = 8;
  int vis = 0;
  while (y0 + vis * pitch + FONT_SMALL_H <= DISPLAY_HEIGHT) vis++;
  const int name_x = 2;
  const int time_x = 43;
  const int msg_x = 64;
  const int msg_end = 124;
  draw_slant_header(buf, hal, "INBOX");
  for (int x = 1; x <= 90; x++) display_set_pixel(buf, x, 13, true);
  if (pager->inbox_n <= 0) {
    display_draw_text_small(buf, name_x, y0, "no pages");
    return;
  }
  int start = 0;
  if (pager->sel >= vis) start = pager->sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= pager->inbox_n) break;
    int y = y0 + i * pitch;
    char hm[8];
    format_hm(hm, sizeof hm, pager->inbox[idx].received_hh, pager->inbox[idx].received_mm);
    char name[PAGE_FROM_MAX + 1];
    display_clip(name, sizeof name, pager->inbox[idx].from, small_chars_fit(time_x - 2 - name_x));
    char body[PAGE_BODY_MAX + 1];
    display_clip(body, sizeof body, pager->inbox[idx].body, small_chars_fit(msg_end - msg_x + 1));
    display_draw_text_small(buf, name_x, y, name);
    display_draw_text_small(buf, time_x, y, hm);
    display_draw_text_small(buf, msg_x, y, body);
    if (idx == pager->sel) display_invert_rect(buf, 0, y - 1, DISPLAY_WIDTH, pitch - 1);
  }
}

static void draw_outbox(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  const int y0 = 16;
  const int pitch = 8;
  int vis = 0;
  while (y0 + vis * pitch + FONT_SMALL_H <= DISPLAY_HEIGHT) vis++;
  const int name_x = 2;
  const int status_x = 44;
  const int msg_x = 66;
  const int msg_end = 124;
  draw_slant_header(buf, hal, "OUTBOX");
  for (int x = 1; x <= 90; x++) display_set_pixel(buf, x, 13, true);
  if (pager->outbox_n <= 0) {
    display_draw_text_small(buf, name_x, y0, "no pages");
    return;
  }
  int start = 0;
  if (pager->sel >= vis) start = pager->sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= pager->outbox_n) break;
    int y = y0 + i * pitch;
    const outbox_item_t *item = &pager->outbox[idx];
    char status[8];
    if (item->status == OUTBOX_FAILED) str_copy_max(status, sizeof status, "Fail", 8);
    else if (item->status == OUTBOX_SENDING) str_copy_max(status, sizeof status, "Retry", 8);
    else format_hm(status, sizeof status, item->sent_hh, item->sent_mm);
    char name[PAGE_FROM_MAX + 1];
    display_clip(name, sizeof name, item->to, small_chars_fit(status_x - 2 - name_x));
    char body[PAGE_BODY_MAX + 1];
    display_clip(body, sizeof body, item->body, small_chars_fit(msg_end - msg_x + 1));
    display_draw_text_small(buf, name_x, y, name);
    display_draw_text_small(buf, status_x, y, status);
    display_draw_text_small(buf, msg_x, y, body);
    if (idx == pager->sel) display_invert_rect(buf, 0, y - 1, DISPLAY_WIDTH, pitch - 1);
  }
}

static void draw_outread(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  (void)hal;
  if (pager->reading < 0 || pager->reading >= pager->outbox_n) {
    draw_outbox(pager, hal, buf);
    return;
  }
  const outbox_item_t *p = &pager->outbox[pager->reading];
  char title[40];
  if (p->status == OUTBOX_SENDING) {
    snprintf(title, sizeof title, "retry %d/%d", page_outbox_attempts(pager->radio), PAGE_ATTEMPTS);
  } else if (p->status == OUTBOX_FAILED) {
    str_copy_max(title, sizeof title, "X  press to retry", 24);
  } else {
    str_copy_max(title, sizeof title, "delivered", 16);
  }
  char tclip[32];
  display_clip(tclip, sizeof tclip, title, display_cols());
  display_draw_text(buf, 2, 2, tclip);
  display_invert_rect(buf, 0, 0, DISPLAY_WIDTH, CHAR_H + 4);
  char to[40];
  snprintf(to, sizeof to, "To : %s", p->to);
  display_clip(tclip, sizeof tclip, to, display_cols());
  display_draw_text(buf, 2, 14, tclip);
  char sent[8], ack[8], times[48];
  format_hm(sent, sizeof sent, p->sent_hh, p->sent_mm);
  format_hm(ack, sizeof ack, p->acked_hh, p->acked_mm);
  snprintf(times, sizeof times, "sent %s  ack %s", sent, ack);
  display_clip(tclip, sizeof tclip, times, display_cols());
  display_draw_text(buf, 2, 24, tclip);
  char lines[DISPLAY_WRAP_MAX][DISPLAY_WIDTH + 1];
  int n = display_wrap(p->body, display_cols(), lines, DISPLAY_WRAP_MAX);
  int y = 36;
  int show = n < 3 ? n : 3;
  for (int i = 0; i < show; i++) {
    display_draw_text(buf, 2, y, lines[i]);
    y += CHAR_H + 1;
  }
}

static void draw_read(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  if (pager->reading < 0 || pager->reading >= pager->inbox_n) {
    draw_inbox(pager, hal, buf);
    return;
  }
  const inbox_row_t *p = &pager->inbox[pager->reading];
  char from[32];
  display_clip(from, sizeof from, p->from, 10);
  header(buf, from, hal);
  char hm[8];
  format_hm(hm, sizeof hm, p->received_hh, p->received_mm);
  display_draw_text(buf, 2, 14, hm);
  char lines[DISPLAY_WRAP_MAX][DISPLAY_WIDTH + 1];
  int n = display_wrap(p->body, display_cols(), lines, DISPLAY_WRAP_MAX);
  int y = 24;
  int show = n < 3 ? n : 3;
  for (int i = 0; i < show; i++) {
    display_draw_text(buf, 2, y, lines[i]);
    y += CHAR_H + 1;
  }
  char rssi[16];
  snprintf(rssi, sizeof rssi, "%d dBm", (int)p->rssi);
  display_draw_text(buf, DISPLAY_WIDTH - display_text_width(rssi) - 1, 54, rssi);
  display_hline(buf, 0, 62, DISPLAY_WIDTH);
}

static void draw_locate_list(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  header(buf, "LOCATE", hal);
  char rows[CONTACTS_MAX + 1][DISPLAY_WIDTH + 1];
  str_copy_max(rows[0], sizeof rows[0], "< back", 16);
  if (pager->contacts->n == 0) {
    draw_list(pager, buf, rows, 1);
    display_draw_text(buf, 14, 36, "pair first");
    return;
  }
  int ncols = display_cols();
  for (int i = 0; i < pager->contacts->n; i++) {
    display_clip(rows[i + 1], sizeof rows[i + 1], pager->contacts->rows[i].name, ncols - 1);
  }
  draw_list(pager, buf, rows, pager->contacts->n + 1);
}

static void blit_flip(uint8_t *buf, const uint8_t *sprite, size_t sprite_len, int sw, int sh, int dx,
                      int dy, bool flip_x, bool flip_y) {
  if (!sprite || sw <= 0 || sh <= 0) return;
  for (int y = 0; y < sh; y++) {
    int sy = flip_y ? sh - 1 - y : y;
    int page = sy >> 3;
    uint8_t bit = (uint8_t)(1u << (sy & 7));
    for (int x = 0; x < sw; x++) {
      int sx = flip_x ? sw - 1 - x : x;
      int i = page * sw + sx;
      if (i >= 0 && (size_t)i < sprite_len && (sprite[i] & bit)) display_set_pixel(buf, dx + x, dy + y, true);
    }
  }
}

static void draw_track_chrome(uint8_t *buf) {
  draw_slant_bar(buf, "TRACKING");
  display_blit_sprite(buf, art_spider, ART_SPIDER_LEN, ART_SPIDER_W, ART_SPIDER_H, 65, 10);
}

static void draw_track_line(uint8_t *buf, int x, int y, const char *text, int width) {
  char clip[40];
  int fit = small_chars_fit(width);
  if (fit > 36) fit = 36;
  display_clip(clip, sizeof clip, text ? text : "", fit);
  display_draw_text_small(buf, x, y, clip);
}

static void draw_track_looking(pager_t *pager, uint8_t *buf) {
  char me[PAGE_FROM_MAX + 1];
  char line[PAGE_FROM_MAX + 8];
  identity_get_display_name(pager->identity, me, sizeof me);
  draw_track_chrome(buf);
  draw_track_line(buf, 6, 19, me[0] ? me : "BEEPLE", 56);
  draw_track_line(buf, 6, 29, "LOOKING", 56);
  snprintf(line, sizeof line, "FOR %s", pager->loc_name[0] ? pager->loc_name : "PEEPLE");
  draw_track_line(buf, 6, 38, line, 56);
}

static void draw_track_found(pager_t *pager, uint8_t *buf) {
  char line[PAGE_FROM_MAX + 12];
  draw_track_chrome(buf);
  snprintf(line, sizeof line, "%s FOUND!", pager->loc_name[0] ? pager->loc_name : "BEEPLE");
  draw_track_line(buf, 6, 19, line, 56);
  draw_track_line(buf, 6, 29, "ASKING NICELY", 56);
}

static void draw_track_denied(uint8_t *buf) {
  draw_track_chrome(buf);
  draw_track_line(buf, 6, 19, "DENIED.", 56);
  draw_track_line(buf, 6, 29, "THIS IS", 56);
  draw_track_line(buf, 6, 39, "devastating.", 56);
}

static void draw_track_arrow(uint8_t *buf, const char *dist, int octant, int heading_deg) {
  static const char *facing[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  static const struct {
    const uint8_t *bits;
    int len, w, h, x, y;
    bool flip_x, flip_y;
  } arrows[8] = {
      {art_arrow_up, ART_ARROW_UP_LEN, ART_ARROW_UP_W, ART_ARROW_UP_H, 21, 30, false, false},
      {art_arrow_up_right, ART_ARROW_UP_RIGHT_LEN, ART_ARROW_UP_RIGHT_W, ART_ARROW_UP_RIGHT_H, 21, 31, false,
       false},
      {art_arrow_right, ART_ARROW_RIGHT_LEN, ART_ARROW_RIGHT_W, ART_ARROW_RIGHT_H, 20, 33, false, false},
      {art_arrow_down_right, ART_ARROW_DOWN_RIGHT_LEN, ART_ARROW_DOWN_RIGHT_W, ART_ARROW_DOWN_RIGHT_H, 21, 32,
       false, false},
      {art_arrow_up, ART_ARROW_UP_LEN, ART_ARROW_UP_W, ART_ARROW_UP_H, 21, 30, false, true},
      {art_arrow_down_right, ART_ARROW_DOWN_RIGHT_LEN, ART_ARROW_DOWN_RIGHT_W, ART_ARROW_DOWN_RIGHT_H, 21, 32,
       true, false},
      {art_arrow_right, ART_ARROW_RIGHT_LEN, ART_ARROW_RIGHT_W, ART_ARROW_RIGHT_H, 20, 33, true, false},
      {art_arrow_up_right, ART_ARROW_UP_RIGHT_LEN, ART_ARROW_UP_RIGHT_W, ART_ARROW_UP_RIGHT_H, 21, 31, true,
       false},
  };
  int i = octant & 7;
  char line[32];
  char shown[24];
  draw_track_chrome(buf);
  str_copy_max(shown, sizeof shown, dist ? dist : "", sizeof shown - 1);
  size_t n = 0;
  while (shown[n]) n++;
  if (n >= 3 && shown[n - 3] == ' ' && shown[n - 2] == 'f' && shown[n - 1] == 't') {
    shown[n - 2] = 'F';
    shown[n - 1] = 'T';
  }
  draw_track_line(buf, 16, 20, shown, 48);
  blit_flip(buf, arrows[i].bits, (size_t)arrows[i].len, arrows[i].w, arrows[i].h, arrows[i].x, arrows[i].y,
            arrows[i].flip_x, arrows[i].flip_y);
  snprintf(line, sizeof line, "You're looking: %s", facing[heading_device_octant(heading_deg) & 7]);
  draw_track_line(buf, 2, 57, line, 124);
}

static void draw_locating(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  uint32_t age = elapsed_u32(hal->now_ms(hal->ctx), pager->loc_ui_ms);
  if (age >= LOCATE_RETRY_MS) draw_track_found(pager, buf);
  else draw_track_looking(pager, buf);
}

static void draw_center(uint8_t *buf, int y, const char *s) {
  int x = (DISPLAY_WIDTH - display_text_width(s)) / 2;
  if (x < 0) x = 0;
  display_draw_text(buf, x, y, s);
}

/* Octant 0 points at the top of the screen. Clockwise after that. */
static void draw_octant_arrow(uint8_t *buf, int octant) {
  static const int oxs[8] = {0, 11, 16, 11, 0, -11, -16, -11};
  static const int oys[8] = {-16, -11, 0, 11, 16, 11, 0, -11};
  int i = octant & 7;
  int cx = DISPLAY_WIDTH / 2;
  int cy = 26;
  display_draw_arrow(buf, cx - oxs[i], cy - oys[i], cx + oxs[i], cy + oys[i]);
}

static void draw_locread(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  char title[32];
  display_clip(title, sizeof title, pager->loc_name[0] ? pager->loc_name : "LOCATE", 12);
  if (pager->loc_busy) {
    header(buf, title, hal);
    display_draw_text(buf, 2, 20, "radio busy");
    return;
  }
  locate_view_t view;
  locate_format(pager->locate, hal, &view);
  if (locate_status(pager->locate) == LOCATE_DENIED) {
    draw_track_denied(buf);
    return;
  }
  if (view.arrow && hal->heading) {
    heading_sample_t sample;
    heading_view_t pose;
    if (hal->heading(hal->ctx, &sample)) {
      heading_view(&sample, view.north_ft, view.east_ft, hal->now_ms(hal->ctx), &pager->loc_holding,
                   &pager->loc_hold_since, &pose);
      if (pose.arrow && !pose.hold_flat && !pose.hold_still && !pose.dont_move) {
        draw_track_arrow(buf, view.dist, pose.octant, sample.heading_deg);
        return;
      }
      header(buf, title, hal);
      if (pose.arrow) draw_octant_arrow(buf, pose.octant);
      int y = pose.hold_flat && pose.hold_still ? 20 : 28;
      if (pose.hold_flat) {
        draw_center(buf, y, "hold flat");
        y += 12;
      }
      if (pose.hold_still) draw_center(buf, y, "hold still");
      if (pose.dont_move) draw_center(buf, 28, "do not move");
    } else {
      header(buf, title, hal);
    }
  } else {
    header(buf, title, hal);
  }
  if (view.dist[0]) {
    int x = (DISPLAY_WIDTH - display_text_width(view.dist)) / 2;
    if (x < 0) x = 0;
    display_draw_text(buf, x, 44, view.dist);
  }
  if (view.when[0]) {
    char line[24];
    snprintf(line, sizeof line, "Last Loc : %s", view.when);
    draw_center(buf, 54, line);
  }
}

static void draw_compose_split(pager_t *pager, const hal_t *hal, uint8_t *buf, bool messages) {
  const int y0 = 16;
  const int pitch = 8;
  const int name_vis = 3;
  const int msg_vis = 4;
  const int name_x = 2;
  const int name_end = 42;
  const int msg_x = 56;
  const int msg_end = 124;
  draw_slant_header(buf, hal, "COMPOSE");
  for (int x = 1; x <= 90; x++) display_set_pixel(buf, x, 13, true);

  int name_focus = messages ? compose_contact_sel(pager) : pager->sel;
  int name_n = pager->contacts ? pager->contacts->n : 0;
  if (name_n <= 0) {
    display_draw_text_small(buf, name_x, y0, "pair first");
  } else {
    int start = 0;
    if (name_focus >= name_vis) start = name_focus - name_vis + 1;
    for (int i = 0; i < name_vis; i++) {
      int idx = start + i;
      if (idx >= name_n) break;
      int y = y0 + i * pitch;
      char name[PAGE_FROM_MAX + 1];
      display_clip(name, sizeof name, pager->contacts->rows[idx].name, small_chars_fit(name_end - name_x + 1));
      display_draw_text_small(buf, name_x, y, name);
      if (idx == name_focus) display_invert_rect(buf, 0, y - 1, name_end + 2, pitch - 1);
    }
  }

  char list[CANNED_SAVED_MAX + CANNED_DEFAULT_N][PAGE_BODY_MAX + 1];
  int n = canned_list(&pager->canned, list, CANNED_SAVED_MAX + CANNED_DEFAULT_N);
  int msg_scroll = msg_vis - 1;
  int start = 0;
  if (messages && pager->sel < n && pager->sel >= msg_scroll) start = pager->sel - msg_scroll + 1;
  if (start + msg_scroll > n) start = n - msg_scroll;
  if (start < 0) start = 0;
  for (int i = 0; i < msg_scroll; i++) {
    int idx = start + i;
    if (idx >= n) break;
    int y = y0 + i * pitch;
    char body[PAGE_BODY_MAX + 1];
    display_clip(body, sizeof body, list[idx], small_chars_fit(msg_end - msg_x + 1));
    display_draw_text_small(buf, msg_x, y, body);
    if (messages && idx == pager->sel) {
      display_invert_rect(buf, 49, y - 1, DISPLAY_WIDTH - 49, pitch - 1);
    }
  }
  int custom_y = y0 + msg_scroll * pitch;
  display_draw_text_small(buf, msg_x, custom_y, "Custom");
  if (messages && pager->sel == n) {
    display_invert_rect(buf, 49, custom_y - 1, DISPLAY_WIDTH - 49, pitch - 1);
  }
  for (int y = y0; y <= 57; y++) {
    display_set_pixel(buf, 46, y, true);
    display_set_pixel(buf, 47, y, true);
  }
}

static void draw_compose(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  draw_compose_split(pager, hal, buf, false);
}

static void draw_canned(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  draw_compose_split(pager, hal, buf, true);
}

/* Center a small-font line in the open space left of the beep picture. */
static void draw_status_line(uint8_t *buf, int y, const char *text) {
  const int left = 4;
  const int right = 62;
  char clip[24];
  int fit = small_chars_fit(right - left);
  display_clip(clip, sizeof clip, text ? text : "", fit);
  int width = display_text_width_small(clip);
  int x = left;
  if (width < right - left) x = left + (right - left - width) / 2;
  display_draw_text_small(buf, x, y, clip);
}

static void draw_send_status(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  (void)hal;
  int id = pager->beeple;
  if (id < 0 || id >= BEEPLE_N) id = 0;
  const beeple_def_t *b = &BEEPLES[id];
  blit_slot(buf, b->bits, b->w, b->h, b->invert, MENU_HALF, 0, MENU_HALF, MENU_HALF);
  draw_slant_bar(buf, "SENDING");
  if (pager->screen == SCREEN_SENT) {
    draw_status_line(buf, 30, "BEEP");
    draw_status_line(buf, 40, "SUCCESS!");
  } else if (pager->screen == SCREEN_FAILED) {
    draw_status_line(buf, 30, "CAN'T FIND");
    draw_status_line(buf, 40, "WILL TRY");
    draw_status_line(buf, 49, "AGAIN!");
  } else {
    draw_status_line(buf, 30, pager->sent_to);
    draw_status_line(buf, 40, "SEARCHING");
  }
}

static void draw_pair(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  header(buf, "ADD", hal);
  uint32_t now = hal->now_ms(hal->ctx);
  uint32_t rem = pair_active(pager->pair) ? elapsed_u32(pair_deadline_ms(pager->pair), now) : 0;
  unsigned secs = (unsigned)((rem + 999) / 1000);
  char t[16];
  snprintf(t, sizeof t, "%us", secs);
  display_draw_text(buf, 2, 16, t);
  display_draw_text(buf, 2, 28, "hold together");
  int8_t rssi;
  if (pair_last_rssi(pager->pair, &rssi)) {
    snprintf(t, sizeof t, "%d dBm", (int)rssi);
    display_draw_text(buf, 2, 40, t);
  }
  const char *err = pair_error(pager->pair);
  if (err && strcmp(err, "try again") == 0) display_draw_text(buf, 2, 52, "ERROR: Try again");
  else if (err) {
    char clip[32];
    display_clip(clip, sizeof clip, err, display_cols());
    display_draw_text(buf, 2, 52, clip);
  }
}

static void name_for(pager_t *pager, const uint8_t *id, char *out, size_t n) {
  const contact_t *c = id ? contacts_find_by_id(pager->contacts, id) : NULL;
  if (c && c->name[0]) str_copy_max(out, n, c->name, PAGE_FROM_MAX);
  else str_copy_max(out, n, "contact", 16);
}

static const char *SET_LABEL[SET_N] = {
    "BACK", "NAME", "TRACKING", "CLOCK BAT", "DARK MODE", "VIBE TEST",
};

static void small_fit_left(char *dst, size_t cap, const char *src, int x0, int end_ink) {
  if (!dst || cap == 0) return;
  dst[0] = 0;
  if (!src) return;
  size_t n = 0;
  while (src[n] && n + 1 < cap) {
    dst[n] = src[n];
    dst[n + 1] = 0;
    int span = display_text_width_small(dst);
    if (span > 0 && x0 + span - 1 > end_ink) {
      dst[n] = 0;
      break;
    }
    n++;
  }
}

static void small_fit_right(char *dst, size_t cap, const char *src, int min_x, int end_ink) {
  str_copy_max(dst, cap, src ? src : "", cap > 0 ? cap - 1 : 0);
  while (dst[0]) {
    int span = display_text_width_small(dst);
    if (span > 0 && end_ink - (span - 1) >= min_x) return;
    size_t n = 0;
    while (dst[n]) n++;
    dst[n - 1] = 0;
  }
}

static void settings_value(const pager_t *pager, int idx, char *out, size_t cap) {
  out[0] = 0;
  if (idx == SET_NAME) identity_get_display_name(pager->identity, out, cap);
  else if (idx == SET_TRACK)
    str_copy_max(out, cap, locate_is_open(pager->locate) ? "open" : "ask", 8);
  else if (idx == SET_CLOCK) str_copy_max(out, cap, pager->show_clock_bat ? "on" : "off", 4);
  else if (idx == SET_DARK) str_copy_max(out, cap, pager->dark_mode ? "on" : "off", 4);
  else if (idx == SET_VIBE) str_copy_max(out, cap, pager->vibe_test ? "on" : "off", 4);
}

static void settings_reveal(pager_t *pager) {
  if (pager->settings_scroll > pager->sel) pager->settings_scroll = pager->sel;
  if (pager->sel >= pager->settings_scroll + SET_VIS)
    pager->settings_scroll = pager->sel - SET_VIS + 1;
  int max_top = SET_N > SET_VIS ? SET_N - SET_VIS : 0;
  if (pager->settings_scroll > max_top) pager->settings_scroll = max_top;
  if (pager->settings_scroll < 0) pager->settings_scroll = 0;
}

static void draw_settings(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  header(buf, "SETTINGS", hal);
  settings_reveal(pager);
  for (int row = 0; row < SET_VIS; row++) {
    int idx = pager->settings_scroll + row;
    if (idx >= SET_N) break;
    int y = SET_Y0 + row * SET_PITCH;
    char label[24];
    small_fit_left(label, sizeof label, SET_LABEL[idx], SET_LABEL_X, SET_LABEL_END);
    display_draw_text_small(buf, SET_LABEL_X, y, label);
    char value[PAGE_FROM_MAX + 1];
    settings_value(pager, idx, value, sizeof value);
    small_fit_right(value, sizeof value, value, SET_VALUE_MIN, SET_VALUE_END);
    if (value[0]) {
      int span = display_text_width_small(value);
      display_draw_text_small(buf, SET_VALUE_END - (span - 1), y, value);
    }
    if (idx == pager->sel) invert_cut_corner(buf, SET_BAR_X, y - 1, SET_BAR_W, SET_BAR_H);
  }
  for (int y = DISPLAY_HEADER_H; y < DISPLAY_HEIGHT; y++) display_set_pixel(buf, SET_RULE_X, y, true);
}

static void draw_track_ask(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  header(buf, "TRACK?", hal);
  uint8_t id[NODE_ID_LEN];
  char name[PAGE_FROM_MAX + 1];
  char clip[32];
  if (locate_ask_id(pager->locate, id)) name_for(pager, id, name, sizeof name);
  else str_copy_max(name, sizeof name, "contact", 16);
  display_clip(clip, sizeof clip, name, display_cols());
  display_draw_text(buf, 2, 16, clip);
  display_draw_text(buf, 2, 32, "Allow");
  display_draw_text(buf, 2, 44, "Deny");
  int y = pager->sel == 0 ? 31 : 43;
  display_invert_rect(buf, 0, y, DISPLAY_WIDTH, 10);
}

static void draw_tracked(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  header(buf, "TRACKED", hal);
  char clip[32];
  display_clip(clip, sizeof clip, pager->track_name, display_cols());
  display_draw_text(buf, 2, 20, clip);
  display_draw_text(buf, 2, 36, "is locating you");
}

static void draw_alert(pager_t *pager, const hal_t *hal, uint8_t *buf) {
  int beeple = pager->has_alert ? pager->alert.beeple : BEEPLE_NONE;
  if (beeple >= 0 && beeple < BEEPLE_N) {
    const beeple_def_t *b = &BEEPLES[beeple];
    blit_slot(buf, b->bits, b->w, b->h, b->invert, MENU_HALF, 0, MENU_HALF, MENU_HALF);
    draw_slant_bar(buf, "NEW BEEP");
  } else {
    draw_slant_bar(buf, "NEW BEEP");
    display_blit_sprite(buf, art_spider, ART_SPIDER_LEN, ART_SPIDER_W, ART_SPIDER_H, 65, 10);
    for (int y = 0; y < 10; y++) {
      display_set_pixel(buf, 95, y, true);
      display_set_pixel(buf, 96, y, true);
    }
  }
  for (int x = 0; x <= 43; x++) display_set_pixel(buf, x, 14, true);
  for (int x = 0; x <= 54; x++) display_set_pixel(buf, x, 24, true);
  if (pager->has_alert) {
    char name[PAGE_FROM_MAX + 1];
    display_clip(name, sizeof name, pager->alert.from, small_chars_fit(26));
    display_draw_text_small(buf, 3, 17, name);
    char hm[8];
    if (pager->inbox_n > 0) {
      format_hm(hm, sizeof hm, pager->inbox[0].received_hh, pager->inbox[0].received_mm);
    } else {
      format_hm(hm, sizeof hm, CLOCK_NONE, CLOCK_NONE);
    }
    display_draw_text_small(buf, 32, 17, hm);
    char lines[4][DISPLAY_WIDTH + 1];
    int n = display_wrap(pager->alert.body, 14, lines, 4);
    for (int i = 0; i < n; i++) display_draw_text_small(buf, 3, 29 + i * 7, lines[i]);
  }
  if (deadline_live(hal->now_ms(hal->ctx), pager->flash_start, pager->flash_ms)) {
    display_invert_rect(buf, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  }
}

void pager_input(pager_t *pager, const hal_t *hal) {
  if (hal->back_poll(hal->ctx)) on_back(pager, hal);
  for (;;) {
    encoder_event_t ev = hal->encoder_poll(hal->ctx);
    if (ev == ENC_NONE) break;
    if (pager->screen == SCREEN_PAIR || pager->screen == SCREEN_SENDING ||
        pager->screen == SCREEN_LOCATING) {
      continue;
    }
    if (ev == ENC_CW) on_turn(pager, 1);
    else if (ev == ENC_CCW) on_turn(pager, -1);
    else if (ev == ENC_PRESS) on_press(pager, hal);
  }
}

void pager_render(pager_t *pager, const hal_t *hal) {
  if (pager->vibe_test) {
    if (pager->screen == SCREEN_SETTINGS) hal->vibe(hal->ctx, 250);
    else stop_vibe_test(pager, hal);
  }
  uint8_t *buf = pager->buf;
  display_clear(buf);
  if (pager->screen == SCREEN_MENU) draw_menu(pager, hal, buf);
  else if (pager->screen == SCREEN_PEEPLE) draw_peeple(pager, hal, buf);
  else if (pager->screen == SCREEN_INBOX) draw_inbox(pager, hal, buf);
  else if (pager->screen == SCREEN_OUTBOX) draw_outbox(pager, hal, buf);
  else if (pager->screen == SCREEN_OUTREAD) draw_outread(pager, hal, buf);
  else if (pager->screen == SCREEN_READ) draw_read(pager, hal, buf);
  else if (pager->screen == SCREEN_COMPOSE) draw_compose(pager, hal, buf);
  else if (pager->screen == SCREEN_LOCATE) draw_locate_list(pager, hal, buf);
  else if (pager->screen == SCREEN_LOCATING) draw_locating(pager, hal, buf);
  else if (pager->screen == SCREEN_LOCREAD) draw_locread(pager, hal, buf);
  else if (pager->screen == SCREEN_CANNED) draw_canned(pager, hal, buf);
  else if (pager->screen == SCREEN_SENDING || pager->screen == SCREEN_SENT ||
           pager->screen == SCREEN_FAILED) {
    draw_send_status(pager, hal, buf);
  }
  else if (pager->screen == SCREEN_PAIR) draw_pair(pager, hal, buf);
  else if (pager->screen == SCREEN_SETTINGS) draw_settings(pager, hal, buf);
  else if (pager->screen == SCREEN_BEEPLE) draw_beeple(pager, buf);
  else if (pager->screen == SCREEN_TRACKASK) draw_track_ask(pager, hal, buf);
  else if (pager->screen == SCREEN_TRACKED) draw_tracked(pager, hal, buf);
  else if (pager->screen == SCREEN_NAMEEDIT || pager->screen == SCREEN_BODYEDIT) {
    text_edit_draw(&pager->edit, hal, buf);
  } else {
    draw_alert(pager, hal, buf);
  }
  if (pager->dark_mode) display_invert_rect(buf, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

  uint8_t *dst = hal->display_buffer(hal->ctx);
  if (memcmp(dst, buf, DISPLAY_BUF_LEN) == 0) return;
  memcpy(dst, buf, DISPLAY_BUF_LEN);
  hal->display_flush(hal->ctx);
}

bool pager_handle_text(pager_t *pager, const char *key) {
  if (pager->screen != SCREEN_NAMEEDIT && pager->screen != SCREEN_BODYEDIT) return false;
  return text_edit_handle_text(&pager->edit, key);
}

void pager_poll_locate(pager_t *pager) {
  if (pager->screen != SCREEN_LOCATING) return;
  locate_status_t status = locate_status(pager->locate);
  if (status == LOCATE_SENDING) return;
  pager->screen = SCREEN_LOCREAD;
  pager->loc_holding = false;
  pager->loc_need_refresh = false;
}

void pager_poll_pose(pager_t *pager, const hal_t *hal) {
  if (!pager || pager->screen != SCREEN_LOCREAD || pager->loc_busy) return;
  if (!hal->heading) return;
  heading_sample_t sample;
  if (!hal->heading(hal->ctx, &sample)) return;
  locate_view_t view;
  locate_format(pager->locate, hal, &view);
  heading_view_t pose;
  heading_view(&sample, view.north_ft, view.east_ft, hal->now_ms(hal->ctx), &pager->loc_holding,
               &pager->loc_hold_since, &pose);
  if (pose.hold_flat || pose.hold_still) pager->loc_need_refresh = true;
  if (!pose.ready || !pager->loc_need_refresh) return;
  locate_gps_capture(pager->locate, hal);
  if (locate_busy(pager->locate)) return;
  if (locate_refresh(pager->locate, pager->radio, hal, pager->identity, pager->contacts)) {
    pager->loc_need_refresh = false;
  }
}

bool pager_poll_track(pager_t *pager, const hal_t *hal) {
  if (!pager || !pager->locate || !hal) return false;
  if (locate_ask_pending(pager->locate)) {
    if (pager->screen == SCREEN_TRACKASK) return false;
    pager->screen = SCREEN_TRACKASK;
    pager->sel = 1;
    hal->vibe(hal->ctx, 250);
    hal->beep(hal->ctx, 80);
    for (;;) {
      if (hal->encoder_poll(hal->ctx) == ENC_NONE) break;
    }
    for (;;) {
      if (!hal->back_poll(hal->ctx)) break;
    }
    return true;
  }
  if (!locate_tracked(pager->locate)) return false;
  uint8_t id[NODE_ID_LEN];
  if (locate_tracked_id(pager->locate, id)) {
    name_for(pager, id, pager->track_name, sizeof pager->track_name);
  }
  locate_clear_tracked(pager->locate);
  if (pager->screen == SCREEN_TRACKED) return false;
  pager->screen = SCREEN_TRACKED;
  pager->sel = 0;
  hal->vibe(hal->ctx, 250);
  hal->beep(hal->ctx, 80);
  return true;
}

void pager_poll_send(pager_t *pager, const hal_t *hal) {
  if (pager->screen == SCREEN_SENDING &&
      elapsed_u32(hal->now_ms(hal->ctx), pager->send_shown_ms) >= PAGE_RETRY_FAST_MS) {
    pager->screen = SCREEN_FAILED;
    pager->sel = 0;
  }
  page_send_status_t status = page_send_status(pager->radio);
  if (status != PAGE_DELIVERED && status != PAGE_FAILED) return;
  finish_outbox(pager, status == PAGE_DELIVERED ? OUTBOX_SENT : OUTBOX_FAILED, hal);
  page_clear_status(pager->radio);
  if (status == PAGE_FAILED) {
    if (pager->screen == SCREEN_SENDING) {
      pager->screen = SCREEN_FAILED;
      pager->sel = 0;
    }
    return;
  }
  if (pager->screen == SCREEN_PAIR || pager->screen == SCREEN_ALERT ||
      pager->screen == SCREEN_NAMEEDIT || pager->screen == SCREEN_BODYEDIT) {
    return;
  }
  pager->screen = SCREEN_SENT;
}

void pager_reset(pager_t *pager) {
  pager->screen = SCREEN_MENU;
  pager->sel = 0;
  pager->inbox_n = 0;
  pager->outbox_n = 0;
  pager->reading = 0;
  pager->has_alert = false;
  pager->flash_start = 0;
  pager->flash_ms = 0;
  pager->has_compose_dest = false;
  pager->sent_to[0] = 0;
  pager->sent_body[0] = 0;
  pager->loc_name[0] = 0;
  pager->track_name[0] = 0;
  pager->loc_busy = false;
  pager->loc_holding = false;
  pager->loc_need_refresh = false;
  text_edit_reset(&pager->edit);
  canned_reset(&pager->canned);
  pager->vibe_test = false;
  display_clear(pager->buf);
}
