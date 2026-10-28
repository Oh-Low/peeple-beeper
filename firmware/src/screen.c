#include "screen.h"

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
  SET_LIGHT,
  SET_VIBE,
  SET_N
};

void screen_init(screen_t *screen, identity_store_t *identity, contact_book_t *contacts,
                pair_session_t *pair, message_session_t *radio, locate_radio_t *locate) {
  memset(screen, 0, sizeof *screen);
  screen->identity = identity;
  screen->contacts = contacts;
  screen->pair = pair;
  screen->radio = radio;
  screen->locate = locate;
  screen->mode = SCREEN_MENU;
  screen->show_clock_bat = true;
  screen->backlight_on = true;
  screen_set_beeple(screen, 0);
  text_edit_init(&screen->edit);
  canned_init(&screen->canned);
}

void screen_set_beeple(screen_t *screen, int id) {
  if (!screen) return;
  if (id < 0 || id >= BEEPLE_N) id = 0;
  screen->beeple = id;
  const beeple_def_t *b = beeple_at(id);
  if (!b) return;
  screen->menu_art = b->bits;
  screen->menu_art_w = b->w;
  screen->menu_art_h = b->h;
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

static void push_outbox(screen_t *screen, const uint8_t *dest_id, const char *to, const char *body,
                        const hal_t *hal) {
  if (screen->outbox_n >= PAGE_KEEP_MAX) screen->outbox_n = PAGE_KEEP_MAX;
  if (screen->outbox_n == PAGE_KEEP_MAX) screen->outbox_n--;
  if (screen->outbox_n > 0) {
    memmove(&screen->outbox[1], &screen->outbox[0], (size_t)screen->outbox_n * sizeof(outbox_item_t));
  }
  outbox_item_t *row = &screen->outbox[0];
  memset(row, 0, sizeof *row);
  memcpy(row->dest_id, dest_id, NODE_ID_LEN);
  str_copy_max(row->to, sizeof row->to, to, PAGE_FROM_MAX);
  str_copy_max(row->body, sizeof row->body, body, PAGE_BODY_MAX);
  row->status = OUTBOX_SENDING;
  stamp_clock(hal, &row->sent_hh, &row->sent_mm);
  row->acked_hh = CLOCK_NONE;
  row->acked_mm = CLOCK_NONE;
  screen->outbox_n++;
}

static void finish_outbox(screen_t *screen, outbox_status_t next, const hal_t *hal) {
  for (int i = 0; i < screen->outbox_n; i++) {
    outbox_item_t *row = &screen->outbox[i];
    if (row->status == OUTBOX_SENDING) {
      row->status = next;
      if (next == OUTBOX_SENT) {
        stamp_clock(hal, &row->acked_hh, &row->acked_mm);
      }
      return;
    }
  }
}

static void promote_outbox(screen_t *screen, int idx) {
  if (idx <= 0 || idx >= screen->outbox_n) {
    if (idx >= 0 && idx < screen->outbox_n) {
      screen->outbox[idx].status = OUTBOX_SENDING;
      screen->outbox[idx].acked_hh = CLOCK_NONE;
      screen->outbox[idx].acked_mm = CLOCK_NONE;
    }
    return;
  }
  outbox_item_t item = screen->outbox[idx];
  memmove(&screen->outbox[1], &screen->outbox[0], (size_t)idx * sizeof(outbox_item_t));
  screen->outbox[0] = item;
  screen->outbox[0].status = OUTBOX_SENDING;
  screen->outbox[0].acked_hh = CLOCK_NONE;
  screen->outbox[0].acked_mm = CLOCK_NONE;
}

static void push_inbox(screen_t *screen, const page_t *p, const hal_t *hal) {
  if (screen->inbox_n == PAGE_KEEP_MAX) screen->inbox_n--;
  if (screen->inbox_n > 0) {
    memmove(&screen->inbox[1], &screen->inbox[0], (size_t)screen->inbox_n * sizeof(inbox_row_t));
  }
  inbox_row_t *row = &screen->inbox[0];
  memset(row, 0, sizeof *row);
  page_t tmp;
  memset(&tmp, 0, sizeof tmp);
  copy_page(p, &tmp);
  str_copy_max(row->from, sizeof row->from, tmp.from, PAGE_FROM_MAX);
  str_copy_max(row->body, sizeof row->body, tmp.body, PAGE_BODY_MAX);
  row->rssi = tmp.rssi;
  row->received_ms = tmp.received_ms;
  stamp_clock(hal, &row->received_hh, &row->received_mm);
  screen->inbox_n++;
}

static void clamp_sel(screen_t *screen, int max) {
  if (max <= 0) {
    screen->sel = 0;
    return;
  }
  if (screen->sel < 0) screen->sel = max - 1;
  if (screen->sel >= max) screen->sel = 0;
}

static void begin_name_edit(screen_t *screen) {
  screen->mode = SCREEN_NAMEEDIT;
  char name[PAGE_FROM_MAX + 1];
  identity_get_display_name(screen->identity, name, sizeof name);
  text_edit_begin(&screen->edit, name, PAGE_FROM_MAX, false);
}

static void begin_body_edit(screen_t *screen) {
  screen->mode = SCREEN_BODYEDIT;
  text_edit_begin(&screen->edit, "", PAGE_BODY_MAX, true);
}

static void save_name_edit(screen_t *screen) {
  char draft[PAGE_FROM_MAX + 1];
  text_edit_draft(&screen->edit, draft, sizeof draft);
  identity_set_display_name(screen->identity, draft);
  screen->mode = SCREEN_SETTINGS;
  screen->sel = 1;
}

static void cancel_name_edit(screen_t *screen) {
  screen->mode = SCREEN_SETTINGS;
  screen->sel = 1;
}

static void send_page(screen_t *screen, const hal_t *hal, const uint8_t *dest, const char *body,
                      int reuse_idx) {
  if (!body || !body[0]) return;
  if (message_busy(screen->radio)) return;
  if (!message_tx(screen->radio, hal, dest, body, screen->contacts, (uint8_t)screen->beeple)) {
    return;
  }
  const contact_t *contact = contacts_find_by_id(screen->contacts, dest);
  str_copy_max(screen->sent_to, sizeof screen->sent_to, contact ? contact->name : "", PAGE_FROM_MAX);
  str_copy_max(screen->sent_body, sizeof screen->sent_body, body, PAGE_BODY_MAX);
  if (reuse_idx >= 0) promote_outbox(screen, reuse_idx);
  else push_outbox(screen, dest, screen->sent_to, screen->sent_body, hal);
  screen->mode = SCREEN_SENDING;
  screen->sel = 0;
}

static void retry_outbox(screen_t *screen, const hal_t *hal, int idx) {
  if (idx < 0 || idx >= screen->outbox_n) return;
  outbox_item_t *item = &screen->outbox[idx];
  if (item->status != OUTBOX_FAILED) return;
  send_page(screen, hal, item->dest_id, item->body, idx);
}

static int canned_msg_n(const screen_t *screen) {
  return screen->canned.n + CANNED_DEFAULT_N;
}

static int canned_rows(screen_t *screen) {
  return canned_msg_n(screen) + 1;
}

static int compose_contact_sel(const screen_t *screen) {
  if (!screen->has_compose_dest || !screen->contacts) return 0;
  for (int i = 0; i < screen->contacts->n; i++) {
    if (memcmp(screen->contacts->rows[i].node_id, screen->compose_dest, NODE_ID_LEN) == 0) return i;
  }
  return 0;
}

static void send_body_edit(screen_t *screen, const hal_t *hal) {
  char body[PAGE_BODY_MAX + 1];
  text_edit_draft(&screen->edit, body, sizeof body);
  if (!screen->has_compose_dest || !body[0]) {
    screen->mode = SCREEN_CANNED;
    screen->sel = canned_msg_n(screen);
    return;
  }
  send_page(screen, hal, screen->compose_dest, body, -1);
}

static void cancel_body_edit(screen_t *screen) {
  screen->mode = SCREEN_CANNED;
  screen->sel = canned_msg_n(screen);
}

static void save_canned_draft(screen_t *screen, const hal_t *hal) {
  char draft[PAGE_BODY_MAX + 1];
  text_edit_draft(&screen->edit, draft, sizeof draft);
  if (!canned_add(&screen->canned, draft)) return;
  hal->vibe(hal->ctx, 80);
}

static void dismiss_sent(screen_t *screen, const hal_t *hal) {
  if (screen->has_alert) {
    screen->mode = SCREEN_ALERT;
    screen->flash_start = hal->now_ms(hal->ctx);
    screen->flash_ms = FLASH_MS;
    return;
  }
  screen->mode = SCREEN_OUTBOX;
  screen->sel = 0;
}

static void on_turn(screen_t *screen, int dir) {
  if (screen->mode == SCREEN_ALERT || screen->mode == SCREEN_READ ||
      screen->mode == SCREEN_PAIR || screen->mode == SCREEN_SENDING ||
      screen->mode == SCREEN_SENT || screen->mode == SCREEN_FAILED ||
      screen->mode == SCREEN_OUTREAD || screen->mode == SCREEN_LOCATING ||
      screen->mode == SCREEN_LOCREAD || screen->mode == SCREEN_TRACKED) {
    return;
  }
  if (screen->mode == SCREEN_NAMEEDIT || screen->mode == SCREEN_BODYEDIT) {
    text_edit_turn(&screen->edit, dir);
    return;
  }
  screen->sel += dir;
  if (screen->mode == SCREEN_MENU) clamp_sel(screen, MENU_N);
  else if (screen->mode == SCREEN_PEEPLE) clamp_sel(screen, PEEP_N);
  else if (screen->mode == SCREEN_INBOX) {
    if (screen->inbox_n <= 0) screen->sel = 0;
    else clamp_sel(screen, screen->inbox_n);
  }
  else if (screen->mode == SCREEN_OUTBOX) {
    if (screen->outbox_n <= 0) screen->sel = 0;
    else clamp_sel(screen, screen->outbox_n);
  }
  else if (screen->mode == SCREEN_COMPOSE) {
    if (!screen->contacts || screen->contacts->n <= 0) screen->sel = 0;
    else clamp_sel(screen, screen->contacts->n);
  }
  else if (screen->mode == SCREEN_CANNED) clamp_sel(screen, canned_rows(screen));
  else if (screen->mode == SCREEN_SETTINGS) clamp_sel(screen, SET_N);
  else if (screen->mode == SCREEN_BEEPLE) clamp_sel(screen, BEEPLE_N);
  else if (screen->mode == SCREEN_LOCATE) clamp_sel(screen, screen->contacts->n + 1);
  else if (screen->mode == SCREEN_TRACKASK) clamp_sel(screen, 2);
}

static void stop_vibe_test(screen_t *screen, const hal_t *hal) {
  screen->vibe_test = false;
  hal->vibe(hal->ctx, 0);
}

static void on_back(screen_t *screen, const hal_t *hal) {
  if (screen->mode == SCREEN_SENDING) {
    screen->mode = SCREEN_OUTBOX;
    screen->sel = 0;
    return;
  }
  if (screen->mode == SCREEN_PAIR) {
    pair_abort(screen->pair, hal);
    screen->mode = SCREEN_PEEPLE;
    screen->sel = PEEP_CONTACT;
    screen->has_compose_dest = false;
    return;
  }
  if (screen->mode == SCREEN_NAMEEDIT) {
    cancel_name_edit(screen);
    return;
  }
  if (screen->mode == SCREEN_BODYEDIT) {
    cancel_body_edit(screen);
    return;
  }
  if (screen->mode == SCREEN_SETTINGS) {
    stop_vibe_test(screen, hal);
    screen->mode = SCREEN_MENU;
    screen->sel = MENU_SETTINGS;
    return;
  }
  if (screen->mode == SCREEN_BEEPLE) {
    screen->mode = SCREEN_MENU;
    screen->sel = MENU_BEEPLE;
    return;
  }
  if (screen->mode == SCREEN_PEEPLE) {
    screen->mode = SCREEN_MENU;
    screen->sel = MENU_PEEPLE;
    return;
  }
  if (screen->mode == SCREEN_TRACKASK) {
    locate_deny(screen->locate, hal, screen->identity, screen->contacts);
    screen->mode = SCREEN_MENU;
    screen->sel = 0;
    return;
  }
  if (screen->mode == SCREEN_TRACKED) {
    screen->mode = SCREEN_MENU;
    screen->sel = 0;
    return;
  }
  if (screen->mode == SCREEN_ALERT) {
    screen->mode = SCREEN_READ;
    screen->reading = 0;
    screen->has_alert = false;
    return;
  }
  if (screen->mode == SCREEN_SENT || screen->mode == SCREEN_FAILED) {
    dismiss_sent(screen, hal);
    return;
  }
  if (screen->mode == SCREEN_CANNED) {
    int sel = compose_contact_sel(screen);
    screen->mode = SCREEN_COMPOSE;
    screen->has_compose_dest = false;
    screen->sel = sel;
    return;
  }
  if (screen->mode == SCREEN_OUTREAD) {
    screen->mode = SCREEN_OUTBOX;
    screen->sel = screen->reading;
    if (screen->outbox_n <= 0) screen->sel = 0;
    else if (screen->sel >= screen->outbox_n) screen->sel = screen->outbox_n - 1;
    return;
  }
  if (screen->mode == SCREEN_OUTBOX) {
    screen->mode = SCREEN_MENU;
    screen->sel = 2;
    return;
  }
  if (screen->mode == SCREEN_LOCATING) {
    locate_abort(screen->locate);
    screen->loc_busy = false;
    screen->mode = SCREEN_LOCATE;
    screen->sel = screen->contacts->n ? 1 : 0;
    return;
  }
  if (screen->mode == SCREEN_LOCREAD) {
    if (locate_busy(screen->locate)) locate_abort(screen->locate);
    screen->loc_busy = false;
    screen->loc_need_refresh = false;
    screen->loc_capture_wait = false;
    screen->mode = SCREEN_LOCATE;
    screen->sel = screen->contacts->n ? 1 : 0;
    return;
  }
  if (screen->mode == SCREEN_LOCATE) {
    screen->mode = SCREEN_PEEPLE;
    screen->sel = PEEP_LOCATE;
    return;
  }
  if (screen->mode == SCREEN_READ || screen->mode == SCREEN_INBOX ||
      screen->mode == SCREEN_COMPOSE) {
    screen->mode = SCREEN_MENU;
    screen->sel = 0;
    screen->has_compose_dest = false;
  }
}

static void on_press(screen_t *screen, const hal_t *hal) {
  if (screen->mode == SCREEN_PAIR || screen->mode == SCREEN_SENDING ||
      screen->mode == SCREEN_LOCATING) {
    return;
  }
  if (screen->mode == SCREEN_LOCREAD) {
    if (locate_busy(screen->locate)) locate_abort(screen->locate);
    screen->loc_busy = false;
    screen->loc_need_refresh = false;
    screen->loc_capture_wait = false;
    screen->mode = SCREEN_LOCATE;
    screen->sel = screen->contacts->n ? 1 : 0;
    return;
  }

  if (screen->mode == SCREEN_SENT || screen->mode == SCREEN_FAILED) {
    dismiss_sent(screen, hal);
    return;
  }
  if (screen->mode == SCREEN_ALERT && screen->has_alert) {
    screen->reading = 0;
    screen->mode = SCREEN_READ;
    screen->has_alert = false;
    return;
  }
  if (screen->mode == SCREEN_READ) {
    screen->mode = SCREEN_INBOX;
    screen->sel = screen->reading;
    if (screen->inbox_n <= 0) screen->sel = 0;
    else if (screen->sel >= screen->inbox_n) screen->sel = screen->inbox_n - 1;
    return;
  }
  if (screen->mode == SCREEN_MENU) {
    if (screen->sel == 0) {
      screen->mode = SCREEN_INBOX;
      screen->sel = 0;
    } else if (screen->sel == 1) {
      screen->mode = SCREEN_COMPOSE;
      screen->has_compose_dest = false;
      screen->sel = 0;
    } else if (screen->sel == 2) {
      screen->mode = SCREEN_OUTBOX;
      screen->sel = 0;
    } else if (screen->sel == MENU_PEEPLE) {
      screen->mode = SCREEN_PEEPLE;
      screen->sel = PEEP_LOCATE;
    } else if (screen->sel == MENU_SETTINGS) {
      screen->mode = SCREEN_SETTINGS;
      screen->sel = SET_NAME;
      screen->settings_scroll = 0;
    } else if (screen->sel == MENU_BEEPLE) {
      screen->mode = SCREEN_BEEPLE;
      screen->sel = screen->beeple;
    }
    return;
  }
  if (screen->mode == SCREEN_BEEPLE) {
    screen_set_beeple(screen, screen->sel);
    screen->mode = SCREEN_MENU;
    screen->sel = MENU_BEEPLE;
    return;
  }
  if (screen->mode == SCREEN_PEEPLE) {
    if (screen->sel == PEEP_LOCATE) {
      screen->loc_busy = false;
      screen->mode = SCREEN_LOCATE;
      screen->sel = screen->contacts->n ? 1 : 0;
    } else if (screen->sel == PEEP_CONTACT) {
      pair_start(screen->pair, screen->identity, hal, (uint8_t)screen->beeple);
      if (pair_active(screen->pair)) {
        screen->mode = SCREEN_PAIR;
        screen->sel = 0;
      }
    }
    return;
  }
  if (screen->mode == SCREEN_SETTINGS) {
    if (screen->sel == SET_BACK) {
      stop_vibe_test(screen, hal);
      screen->mode = SCREEN_MENU;
      screen->sel = MENU_SETTINGS;
      return;
    }
    if (screen->sel == SET_NAME) {
      stop_vibe_test(screen, hal);
      begin_name_edit(screen);
      return;
    }
    if (screen->sel == SET_TRACK) {
      locate_set_open(screen->locate, !locate_is_open(screen->locate));
      return;
    }
    if (screen->sel == SET_CLOCK) {
      screen->show_clock_bat = !screen->show_clock_bat;
      return;
    }
    if (screen->sel == SET_DARK) {
      screen->dark_mode = !screen->dark_mode;
      return;
    }
    if (screen->sel == SET_LIGHT) {
      screen->backlight_on = !screen->backlight_on;
      hal->backlight(hal->ctx, screen->backlight_on);
      return;
    }
    if (screen->sel == SET_VIBE) {
      screen->vibe_test = true;
      hal->vibe(hal->ctx, 250);
    }
    return;
  }
  if (screen->mode == SCREEN_TRACKASK) {
    if (screen->sel == 0) {
      locate_allow(screen->locate, hal, screen->identity, screen->contacts);
    } else {
      locate_deny(screen->locate, hal, screen->identity, screen->contacts);
    }
    screen->mode = SCREEN_MENU;
    screen->sel = 0;
    return;
  }
  if (screen->mode == SCREEN_TRACKED) {
    screen->mode = SCREEN_MENU;
    screen->sel = 0;
    return;
  }
  if (screen->mode == SCREEN_NAMEEDIT) {
    if (text_edit_press(&screen->edit) == TEXT_PRESS_OK) save_name_edit(screen);
    return;
  }
  if (screen->mode == SCREEN_BODYEDIT) {
    text_edit_press_t result = text_edit_press(&screen->edit);
    if (result == TEXT_PRESS_OK) send_body_edit(screen, hal);
    else if (result == TEXT_PRESS_SAVE) save_canned_draft(screen, hal);
    return;
  }
  if (screen->mode == SCREEN_INBOX) {
    if (screen->sel >= 0 && screen->sel < screen->inbox_n) {
      screen->reading = screen->sel;
      screen->mode = SCREEN_READ;
    }
    return;
  }
  if (screen->mode == SCREEN_OUTBOX) {
    if (screen->sel >= 0 && screen->sel < screen->outbox_n) {
      if (screen->outbox[screen->sel].status == OUTBOX_FAILED) {
        retry_outbox(screen, hal, screen->sel);
        return;
      }
      screen->reading = screen->sel;
      screen->mode = SCREEN_OUTREAD;
    }
    return;
  }
  if (screen->mode == SCREEN_OUTREAD) {
    if (screen->reading >= 0 && screen->reading < screen->outbox_n &&
        screen->outbox[screen->reading].status == OUTBOX_FAILED) {
      retry_outbox(screen, hal, screen->reading);
      return;
    }
    screen->mode = SCREEN_OUTBOX;
    screen->sel = screen->reading;
    if (screen->outbox_n <= 0) screen->sel = 0;
    else if (screen->sel >= screen->outbox_n) screen->sel = screen->outbox_n - 1;
    return;
  }
  if (screen->mode == SCREEN_COMPOSE) {
    if (!screen->contacts || screen->sel < 0 || screen->sel >= screen->contacts->n) return;
    memcpy(screen->compose_dest, screen->contacts->rows[screen->sel].node_id, NODE_ID_LEN);
    screen->has_compose_dest = true;
    screen->mode = SCREEN_CANNED;
    screen->sel = 0;
    return;
  }
  if (screen->mode == SCREEN_CANNED) {
    int n = canned_msg_n(screen);
    if (screen->sel == n) {
      begin_body_edit(screen);
      return;
    }
    if (!screen->has_compose_dest) return;
    char list[CANNED_SAVED_MAX + CANNED_DEFAULT_N][PAGE_BODY_MAX + 1];
    int listed = canned_list(&screen->canned, list, CANNED_SAVED_MAX + CANNED_DEFAULT_N);
    if (screen->sel >= 0 && screen->sel < listed) send_page(screen, hal, screen->compose_dest, list[screen->sel], -1);
    return;
  }
  if (screen->mode == SCREEN_LOCATE) {
    if (screen->sel == 0) {
      screen->mode = SCREEN_PEEPLE;
      screen->sel = PEEP_LOCATE;
      return;
    }
    if (screen->contacts->n == 0) return;
    int idx = screen->sel - 1;
    if (idx < 0 || idx >= screen->contacts->n) return;
    const contact_t *c = &screen->contacts->rows[idx];
    str_copy_max(screen->loc_name, sizeof screen->loc_name, c->name, PAGE_FROM_MAX);
    if (!locate_tx(screen->locate, hal, c->node_id, screen->contacts)) {
      screen->loc_busy = true;
      screen->mode = SCREEN_LOCREAD;
      return;
    }
    screen->loc_busy = false;
    screen->mode = SCREEN_LOCATING;
  }
}

void screen_on_page(screen_t *screen, const hal_t *hal, const page_t *page) {
  push_inbox(screen, page, hal);
  memset(&screen->alert, 0, sizeof screen->alert);
  copy_page(page, &screen->alert);
  screen->has_alert = true;
  screen->flash_start = hal->now_ms(hal->ctx);
  screen->flash_ms = FLASH_MS;
  hal->vibe(hal->ctx, 250);
  hal->beep(hal->ctx, 80);
  if (screen->mode == SCREEN_SENDING) return;
  screen->mode = SCREEN_ALERT;
}

void screen_go_menu(screen_t *screen) {
  screen->mode = SCREEN_MENU;
  screen->sel = 0;
  screen->has_compose_dest = false;
  screen->has_alert = false;
}

void screen_finish_pair(screen_t *screen) {
  screen_go_menu(screen);
  screen->mode = SCREEN_PEEPLE;
  screen->sel = PEEP_CONTACT;
}

static void header(uint8_t *buf, const char *title, const hal_t *hal) {
  (void)hal;
  display_draw_header(buf, title);
}

static void draw_list(screen_t *screen, uint8_t *buf, const char rows[][DISPLAY_WIDTH + 1], int nrows) {
  const int top = 12;
  const int vis = 4;
  int start = 0;
  if (screen->sel >= vis) start = screen->sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= nrows) break;
    int y = top + i * 12;
    display_draw_text(buf, 2, y, rows[idx]);
    if (idx == screen->sel) display_invert_rect(buf, 0, y - 1, DISPLAY_WIDTH, 10);
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

static void blit_slot(uint8_t *buf, const uint8_t *bits, int sw, int sh, int dx, int dy, int bw,
                      int bh) {
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
      display_set_pixel(buf, ox + x, oy + y, (bits[i] & bit) != 0);
    }
  }
}

static void draw_art_box(const screen_t *screen, uint8_t *buf, int dx, int dy, int bw, int bh) {
  blit_slot(buf, screen->menu_art, screen->menu_art_w, screen->menu_art_h, dx, dy, bw, bh);
}

static void draw_menu_art(const screen_t *screen, uint8_t *buf) {
  draw_art_box(screen, buf, MENU_HALF, 0, MENU_HALF, MENU_HALF);
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

static void draw_slant_header(uint8_t *buf, const hal_t *hal, const char *title) {
  draw_slant_bar(buf, title);
  uint8_t hh = 0, mm = 0;
  hal->clock(hal->ctx, &hh, &mm);
  char clock[8];
  format_hm(clock, sizeof clock, hh, mm);
  display_draw_text_small(buf, 107, 3, clock);
}

static void draw_side_menu(screen_t *screen, const hal_t *hal, uint8_t *buf, char labels[][24], int n) {
  int y0 = 4;
  int pitch = 10;
  if (screen->show_clock_bat) {
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
  if (screen->sel >= vis) start = screen->sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= n) break;
    int y = y0 + i * pitch;
    display_draw_text(buf, 4, y, labels[idx]);
    if (idx == screen->sel) invert_cut_corner(buf, 1, y - 1, MENU_HALF - 2, pitch);
  }
  draw_menu_art(screen, buf);
}

static void draw_menu(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  int count = screen->inbox_n;
  bool pending = message_busy(screen->radio);
  char labels[MENU_N][24];
  if (count) snprintf(labels[0], sizeof labels[0], "Inbox (%d)", count);
  else str_copy_max(labels[0], sizeof labels[0], "Inbox", 16);
  str_copy_max(labels[1], sizeof labels[1], "Compose", 16);
  str_copy_max(labels[2], sizeof labels[2], pending ? "Outbox (*)" : "Outbox", 16);
  str_copy_max(labels[3], sizeof labels[3], "Peeple", 16);
  str_copy_max(labels[4], sizeof labels[4], "Settings", 16);
  str_copy_max(labels[5], sizeof labels[5], "Beeple", 16);
  draw_side_menu(screen, hal, buf, labels, MENU_N);
}

static void draw_peeple(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  char labels[PEEP_N][24];
  str_copy_max(labels[0], sizeof labels[0], "Locate", 16);
  str_copy_max(labels[1], sizeof labels[1], "Add", 16);
  draw_side_menu(screen, hal, buf, labels, PEEP_N);
}

static void draw_beeple(screen_t *screen, uint8_t *buf) {
  const int y0 = 8;
  const int pitch = 10;
  int sel = screen->sel;
  if (sel < 0 || sel >= BEEPLE_N) sel = 0;
  int vis = 0;
  while (vis < BEEPLE_N && y0 + vis * pitch + CHAR_H <= DISPLAY_HEIGHT) vis++;
  int start = 0;
  if (sel >= vis) start = sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= BEEPLE_N) break;
    int y = y0 + i * pitch;
    display_draw_text(buf, 4, y, beeple_at(idx)->name);
    if (idx == sel) invert_cut_corner(buf, 1, y - 1, MENU_HALF - 2, pitch);
  }
  const beeple_def_t *b = beeple_at(sel);
  blit_slot(buf, b->bits, b->w, b->h, MENU_HALF, 0, MENU_HALF, MENU_HALF);
}

static int small_chars_fit(int width) {
  if (width < 3) return 0;
  return (width - 3) / FONT_SMALL_W + 1;
}

static void draw_inbox(screen_t *screen, const hal_t *hal, uint8_t *buf) {
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
  if (screen->inbox_n <= 0) {
    display_draw_text_small(buf, name_x, y0, "no pages");
    return;
  }
  int start = 0;
  if (screen->sel >= vis) start = screen->sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= screen->inbox_n) break;
    int y = y0 + i * pitch;
    char hm[8];
    format_hm(hm, sizeof hm, screen->inbox[idx].received_hh, screen->inbox[idx].received_mm);
    char name[PAGE_FROM_MAX + 1];
    display_clip(name, sizeof name, screen->inbox[idx].from, small_chars_fit(time_x - 2 - name_x));
    char body[PAGE_BODY_MAX + 1];
    display_clip(body, sizeof body, screen->inbox[idx].body, small_chars_fit(msg_end - msg_x + 1));
    display_draw_text_small(buf, name_x, y, name);
    display_draw_text_small(buf, time_x, y, hm);
    display_draw_text_small(buf, msg_x, y, body);
    if (idx == screen->sel) display_invert_rect(buf, 0, y - 1, DISPLAY_WIDTH, pitch - 1);
  }
}

static void draw_outbox(screen_t *screen, const hal_t *hal, uint8_t *buf) {
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
  if (screen->outbox_n <= 0) {
    display_draw_text_small(buf, name_x, y0, "no pages");
    return;
  }
  int start = 0;
  if (screen->sel >= vis) start = screen->sel - vis + 1;
  for (int i = 0; i < vis; i++) {
    int idx = start + i;
    if (idx >= screen->outbox_n) break;
    int y = y0 + i * pitch;
    const outbox_item_t *item = &screen->outbox[idx];
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
    if (idx == screen->sel) display_invert_rect(buf, 0, y - 1, DISPLAY_WIDTH, pitch - 1);
  }
}

static void draw_outread(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  (void)hal;
  if (screen->reading < 0 || screen->reading >= screen->outbox_n) {
    draw_outbox(screen, hal, buf);
    return;
  }
  const outbox_item_t *p = &screen->outbox[screen->reading];
  char title[40];
  if (p->status == OUTBOX_SENDING) {
    snprintf(title, sizeof title, "retry %d/%d", message_outbox_attempts(screen->radio),
             message_outbox_budget(screen->radio));
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

static void draw_read(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  if (screen->reading < 0 || screen->reading >= screen->inbox_n) {
    draw_inbox(screen, hal, buf);
    return;
  }
  const inbox_row_t *p = &screen->inbox[screen->reading];
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

static void draw_locate_list(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  header(buf, "LOCATE", hal);
  char rows[CONTACTS_MAX + 1][DISPLAY_WIDTH + 1];
  str_copy_max(rows[0], sizeof rows[0], "< back", 16);
  if (screen->contacts->n == 0) {
    draw_list(screen, buf, rows, 1);
    display_draw_text(buf, 14, 36, "pair first");
    return;
  }
  int ncols = display_cols();
  for (int i = 0; i < screen->contacts->n; i++) {
    display_clip(rows[i + 1], sizeof rows[i + 1], screen->contacts->rows[i].name, ncols - 1);
  }
  draw_list(screen, buf, rows, screen->contacts->n + 1);
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

static void draw_track_looking(screen_t *screen, uint8_t *buf) {
  char me[PAGE_FROM_MAX + 1];
  char line[PAGE_FROM_MAX + 8];
  identity_get_display_name(screen->identity, me, sizeof me);
  draw_track_chrome(buf);
  draw_track_line(buf, 6, 19, me[0] ? me : "BEEPLE", 56);
  draw_track_line(buf, 6, 29, "LOOKING", 56);
  snprintf(line, sizeof line, "FOR %s", screen->loc_name[0] ? screen->loc_name : "PEEPLE");
  draw_track_line(buf, 6, 38, line, 56);
}

static void draw_track_found(screen_t *screen, uint8_t *buf) {
  char line[PAGE_FROM_MAX + 12];
  draw_track_chrome(buf);
  snprintf(line, sizeof line, "%s FOUND!", screen->loc_name[0] ? screen->loc_name : "BEEPLE");
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

static void draw_locating(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  if (locate_first_wait_done(screen->locate, hal->now_ms(hal->ctx))) draw_track_found(screen, buf);
  else draw_track_looking(screen, buf);
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

static void draw_locread(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  char title[32];
  display_clip(title, sizeof title, screen->loc_name[0] ? screen->loc_name : "LOCATE", 12);
  if (screen->loc_busy) {
    header(buf, title, hal);
    display_draw_text(buf, 2, 20, "radio busy");
    return;
  }
  locate_view_t view;
  locate_format(screen->locate, hal, &view);
  if (locate_status(screen->locate) == LOCATE_DENIED) {
    draw_track_denied(buf);
    return;
  }
  if (view.arrow && hal->heading) {
    heading_sample_t sample;
    heading_view_t pose;
    if (hal->heading(hal->ctx, &sample)) {
      heading_view(&sample, view.north_ft, view.east_ft, hal->now_ms(hal->ctx), &screen->loc_holding,
                   &screen->loc_hold_since, &pose);
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

static void draw_compose_split(screen_t *screen, const hal_t *hal, uint8_t *buf, bool messages) {
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

  int name_focus = messages ? compose_contact_sel(screen) : screen->sel;
  int name_n = screen->contacts ? screen->contacts->n : 0;
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
      display_clip(name, sizeof name, screen->contacts->rows[idx].name, small_chars_fit(name_end - name_x + 1));
      display_draw_text_small(buf, name_x, y, name);
      if (idx == name_focus) display_invert_rect(buf, 0, y - 1, name_end + 2, pitch - 1);
    }
  }

  char list[CANNED_SAVED_MAX + CANNED_DEFAULT_N][PAGE_BODY_MAX + 1];
  int n = canned_list(&screen->canned, list, CANNED_SAVED_MAX + CANNED_DEFAULT_N);
  int msg_scroll = msg_vis - 1;
  int start = 0;
  if (messages && screen->sel < n && screen->sel >= msg_scroll) start = screen->sel - msg_scroll + 1;
  if (start + msg_scroll > n) start = n - msg_scroll;
  if (start < 0) start = 0;
  for (int i = 0; i < msg_scroll; i++) {
    int idx = start + i;
    if (idx >= n) break;
    int y = y0 + i * pitch;
    char body[PAGE_BODY_MAX + 1];
    display_clip(body, sizeof body, list[idx], small_chars_fit(msg_end - msg_x + 1));
    display_draw_text_small(buf, msg_x, y, body);
    if (messages && idx == screen->sel) {
      display_invert_rect(buf, 49, y - 1, DISPLAY_WIDTH - 49, pitch - 1);
    }
  }
  int custom_y = y0 + msg_scroll * pitch;
  display_draw_text_small(buf, msg_x, custom_y, "Custom");
  if (messages && screen->sel == n) {
    display_invert_rect(buf, 49, custom_y - 1, DISPLAY_WIDTH - 49, pitch - 1);
  }
  for (int y = y0; y <= 57; y++) {
    display_set_pixel(buf, 46, y, true);
    display_set_pixel(buf, 47, y, true);
  }
}

static void draw_compose(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  draw_compose_split(screen, hal, buf, false);
}

static void draw_canned(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  draw_compose_split(screen, hal, buf, true);
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

static void draw_send_status(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  (void)hal;
  int id = screen->beeple;
  if (id < 0 || id >= BEEPLE_N) id = 0;
  const beeple_def_t *b = beeple_at(id);
  blit_slot(buf, b->bits, b->w, b->h, MENU_HALF, 0, MENU_HALF, MENU_HALF);
  draw_slant_bar(buf, "SENDING");
  if (screen->mode == SCREEN_SENT) {
    draw_status_line(buf, 30, "BEEP");
    draw_status_line(buf, 40, "SUCCESS!");
  } else if (screen->mode == SCREEN_FAILED) {
    draw_status_line(buf, 30, "CAN'T FIND");
    draw_status_line(buf, 40, "WILL TRY");
    draw_status_line(buf, 49, "AGAIN!");
  } else {
    draw_status_line(buf, 30, screen->sent_to);
    draw_status_line(buf, 40, "SEARCHING");
  }
}

static void draw_pair(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  header(buf, "ADD", hal);
  uint32_t now = hal->now_ms(hal->ctx);
  uint32_t rem = pair_active(screen->pair) ? elapsed_u32(pair_deadline_ms(screen->pair), now) : 0;
  unsigned secs = (unsigned)((rem + 999) / 1000);
  char t[16];
  snprintf(t, sizeof t, "%us", secs);
  display_draw_text(buf, 2, 16, t);
  display_draw_text(buf, 2, 28, "hold together");
  int8_t rssi;
  if (pair_last_rssi(screen->pair, &rssi)) {
    snprintf(t, sizeof t, "%d dBm", (int)rssi);
    display_draw_text(buf, 2, 40, t);
  }
  const char *err = pair_error(screen->pair);
  if (err && strcmp(err, "try again") == 0) display_draw_text(buf, 2, 52, "ERROR: Try again");
  else if (err) {
    char clip[32];
    display_clip(clip, sizeof clip, err, display_cols());
    display_draw_text(buf, 2, 52, clip);
  }
}

static void name_for(screen_t *screen, const uint8_t *id, char *out, size_t n) {
  const contact_t *c = id ? contacts_find_by_id(screen->contacts, id) : NULL;
  if (c && c->name[0]) str_copy_max(out, n, c->name, PAGE_FROM_MAX);
  else str_copy_max(out, n, "contact", 16);
}

static const char *SET_LABEL[SET_N] = {
    "BACK", "NAME", "TRACKING", "CLOCK BAT", "DARK MODE", "BACKLIGHT", "VIBE TEST",
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

static void settings_value(const screen_t *screen, int idx, char *out, size_t cap) {
  out[0] = 0;
  if (idx == SET_NAME) identity_get_display_name(screen->identity, out, cap);
  else if (idx == SET_TRACK)
    str_copy_max(out, cap, locate_is_open(screen->locate) ? "open" : "ask", 8);
  else if (idx == SET_CLOCK) str_copy_max(out, cap, screen->show_clock_bat ? "on" : "off", 4);
  else if (idx == SET_DARK) str_copy_max(out, cap, screen->dark_mode ? "on" : "off", 4);
  else if (idx == SET_LIGHT) str_copy_max(out, cap, screen->backlight_on ? "on" : "off", 4);
  else if (idx == SET_VIBE) str_copy_max(out, cap, screen->vibe_test ? "on" : "off", 4);
}

static void settings_reveal(screen_t *screen) {
  if (screen->settings_scroll > screen->sel) screen->settings_scroll = screen->sel;
  if (screen->sel >= screen->settings_scroll + SET_VIS)
    screen->settings_scroll = screen->sel - SET_VIS + 1;
  int max_top = SET_N > SET_VIS ? SET_N - SET_VIS : 0;
  if (screen->settings_scroll > max_top) screen->settings_scroll = max_top;
  if (screen->settings_scroll < 0) screen->settings_scroll = 0;
}

static void draw_settings(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  header(buf, "SETTINGS", hal);
  settings_reveal(screen);
  for (int row = 0; row < SET_VIS; row++) {
    int idx = screen->settings_scroll + row;
    if (idx >= SET_N) break;
    int y = SET_Y0 + row * SET_PITCH;
    char label[24];
    small_fit_left(label, sizeof label, SET_LABEL[idx], SET_LABEL_X, SET_LABEL_END);
    display_draw_text_small(buf, SET_LABEL_X, y, label);
    char value[PAGE_FROM_MAX + 1];
    settings_value(screen, idx, value, sizeof value);
    small_fit_right(value, sizeof value, value, SET_VALUE_MIN, SET_VALUE_END);
    if (value[0]) {
      int span = display_text_width_small(value);
      display_draw_text_small(buf, SET_VALUE_END - (span - 1), y, value);
    }
    if (idx == screen->sel) invert_cut_corner(buf, SET_BAR_X, y - 1, SET_BAR_W, SET_BAR_H);
  }
  for (int y = DISPLAY_HEADER_H; y < DISPLAY_HEIGHT; y++) display_set_pixel(buf, SET_RULE_X, y, true);
}

static void draw_track_ask(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  header(buf, "TRACK?", hal);
  uint8_t id[NODE_ID_LEN];
  char name[PAGE_FROM_MAX + 1];
  char clip[32];
  if (locate_ask_id(screen->locate, id)) name_for(screen, id, name, sizeof name);
  else str_copy_max(name, sizeof name, "contact", 16);
  display_clip(clip, sizeof clip, name, display_cols());
  display_draw_text(buf, 2, 16, clip);
  display_draw_text(buf, 2, 32, "Allow");
  display_draw_text(buf, 2, 44, "Deny");
  int y = screen->sel == 0 ? 31 : 43;
  display_invert_rect(buf, 0, y, DISPLAY_WIDTH, 10);
}

static void draw_tracked(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  header(buf, "TRACKED", hal);
  char clip[32];
  display_clip(clip, sizeof clip, screen->track_name, display_cols());
  display_draw_text(buf, 2, 20, clip);
  display_draw_text(buf, 2, 36, "is locating you");
}

static void draw_alert(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  int beeple = screen->has_alert ? screen->alert.beeple : BEEPLE_NONE;
  if (beeple >= 0 && beeple < BEEPLE_N) {
    const beeple_def_t *b = beeple_at(beeple);
    blit_slot(buf, b->bits, b->w, b->h, MENU_HALF, 0, MENU_HALF, MENU_HALF);
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
  if (screen->has_alert) {
    char name[PAGE_FROM_MAX + 1];
    display_clip(name, sizeof name, screen->alert.from, small_chars_fit(26));
    display_draw_text_small(buf, 3, 17, name);
    char hm[8];
    if (screen->inbox_n > 0) {
      format_hm(hm, sizeof hm, screen->inbox[0].received_hh, screen->inbox[0].received_mm);
    } else {
      format_hm(hm, sizeof hm, CLOCK_NONE, CLOCK_NONE);
    }
    display_draw_text_small(buf, 32, 17, hm);
    char lines[4][DISPLAY_WIDTH + 1];
    int n = display_wrap(screen->alert.body, 14, lines, 4);
    for (int i = 0; i < n; i++) display_draw_text_small(buf, 3, 29 + i * 7, lines[i]);
  }
  if (deadline_live(hal->now_ms(hal->ctx), screen->flash_start, screen->flash_ms)) {
    display_invert_rect(buf, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  }
}

void screen_input(screen_t *screen, const hal_t *hal) {
  if (hal->back_poll(hal->ctx)) on_back(screen, hal);
  for (;;) {
    encoder_event_t ev = hal->encoder_poll(hal->ctx);
    if (ev == ENC_NONE) break;
    if (screen->mode == SCREEN_PAIR || screen->mode == SCREEN_SENDING ||
        screen->mode == SCREEN_LOCATING) {
      continue;
    }
    if (ev == ENC_CW) on_turn(screen, 1);
    else if (ev == ENC_CCW) on_turn(screen, -1);
    else if (ev == ENC_PRESS) on_press(screen, hal);
  }
}

void screen_render(screen_t *screen, const hal_t *hal) {
  if (screen->vibe_test) {
    if (screen->mode == SCREEN_SETTINGS) hal->vibe(hal->ctx, 250);
    else stop_vibe_test(screen, hal);
  }
  uint8_t *buf = screen->buf;
  display_clear(buf);
  if (screen->mode == SCREEN_MENU) draw_menu(screen, hal, buf);
  else if (screen->mode == SCREEN_PEEPLE) draw_peeple(screen, hal, buf);
  else if (screen->mode == SCREEN_INBOX) draw_inbox(screen, hal, buf);
  else if (screen->mode == SCREEN_OUTBOX) draw_outbox(screen, hal, buf);
  else if (screen->mode == SCREEN_OUTREAD) draw_outread(screen, hal, buf);
  else if (screen->mode == SCREEN_READ) draw_read(screen, hal, buf);
  else if (screen->mode == SCREEN_COMPOSE) draw_compose(screen, hal, buf);
  else if (screen->mode == SCREEN_LOCATE) draw_locate_list(screen, hal, buf);
  else if (screen->mode == SCREEN_LOCATING) draw_locating(screen, hal, buf);
  else if (screen->mode == SCREEN_LOCREAD) draw_locread(screen, hal, buf);
  else if (screen->mode == SCREEN_CANNED) draw_canned(screen, hal, buf);
  else if (screen->mode == SCREEN_SENDING || screen->mode == SCREEN_SENT ||
           screen->mode == SCREEN_FAILED) {
    draw_send_status(screen, hal, buf);
  }
  else if (screen->mode == SCREEN_PAIR) draw_pair(screen, hal, buf);
  else if (screen->mode == SCREEN_SETTINGS) draw_settings(screen, hal, buf);
  else if (screen->mode == SCREEN_BEEPLE) draw_beeple(screen, buf);
  else if (screen->mode == SCREEN_TRACKASK) draw_track_ask(screen, hal, buf);
  else if (screen->mode == SCREEN_TRACKED) draw_tracked(screen, hal, buf);
  else if (screen->mode == SCREEN_NAMEEDIT || screen->mode == SCREEN_BODYEDIT) {
    text_edit_draw(&screen->edit, hal, buf);
  } else {
    draw_alert(screen, hal, buf);
  }
  if (screen->dark_mode) display_invert_rect(buf, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

  uint8_t *dst = hal->display_buffer(hal->ctx);
  if (memcmp(dst, buf, DISPLAY_BUF_LEN) == 0) return;
  memcpy(dst, buf, DISPLAY_BUF_LEN);
  hal->display_flush(hal->ctx);
}

bool screen_handle_text(screen_t *screen, const char *key) {
  if (screen->mode != SCREEN_NAMEEDIT && screen->mode != SCREEN_BODYEDIT) return false;
  return text_edit_handle_text(&screen->edit, key);
}

void screen_poll_locate(screen_t *screen) {
  if (screen->mode != SCREEN_LOCATING) return;
  locate_status_t status = locate_status(screen->locate);
  if (status == LOCATE_SENDING) return;
  screen->mode = SCREEN_LOCREAD;
  screen->loc_holding = false;
  screen->loc_need_refresh = false;
  screen->loc_capture_wait = false;
}

void screen_poll_pose(screen_t *screen, const hal_t *hal) {
  if (!screen || screen->mode != SCREEN_LOCREAD || screen->loc_busy) return;
  if (!hal->heading) return;
  heading_sample_t sample;
  if (!hal->heading(hal->ctx, &sample)) return;
  locate_view_t view;
  locate_format(screen->locate, hal, &view);
  heading_view_t pose;
  heading_view(&sample, view.north_ft, view.east_ft, hal->now_ms(hal->ctx), &screen->loc_holding,
               &screen->loc_hold_since, &pose);
  if (pose.hold_flat || pose.hold_still) screen->loc_need_refresh = true;
  if (screen->locate->gps_listening) return;
  if (screen->loc_capture_wait) {
    screen->loc_capture_wait = false;
    if (locate_busy(screen->locate)) return;
    if (locate_refresh(screen->locate, hal, screen->contacts)) screen->loc_need_refresh = false;
    return;
  }
  if (!pose.ready || !screen->loc_need_refresh) return;
  locate_gps_capture(screen->locate, hal);
  if (screen->locate->gps_listening) {
    screen->loc_capture_wait = true;
    return;
  }
  if (locate_busy(screen->locate)) return;
  if (locate_refresh(screen->locate, hal, screen->contacts)) {
    screen->loc_need_refresh = false;
  }
}

bool screen_poll_track(screen_t *screen, const hal_t *hal) {
  if (!screen || !screen->locate || !hal) return false;
  if (locate_ask_pending(screen->locate)) {
    if (screen->mode == SCREEN_TRACKASK) return false;
    screen->mode = SCREEN_TRACKASK;
    screen->sel = 1;
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
  if (!locate_tracked(screen->locate)) return false;
  uint8_t id[NODE_ID_LEN];
  if (locate_tracked_id(screen->locate, id)) {
    name_for(screen, id, screen->track_name, sizeof screen->track_name);
  }
  locate_clear_tracked(screen->locate);
  if (screen->mode == SCREEN_TRACKED) return false;
  screen->mode = SCREEN_TRACKED;
  screen->sel = 0;
  hal->vibe(hal->ctx, 250);
  hal->beep(hal->ctx, 80);
  return true;
}

void screen_poll_send(screen_t *screen, const hal_t *hal) {
  if (screen->mode == SCREEN_SENDING &&
      message_first_wait_done(screen->radio, hal->now_ms(hal->ctx))) {
    screen->mode = SCREEN_FAILED;
    screen->sel = 0;
  }
  message_status_t status = message_status(screen->radio);
  if (status != MESSAGE_DELIVERED && status != MESSAGE_FAILED) return;
  finish_outbox(screen, status == MESSAGE_DELIVERED ? OUTBOX_SENT : OUTBOX_FAILED, hal);
  message_clear_status(screen->radio);
  if (status == MESSAGE_FAILED) {
    if (screen->mode == SCREEN_SENDING) {
      screen->mode = SCREEN_FAILED;
      screen->sel = 0;
    }
    return;
  }
  if (screen->mode == SCREEN_PAIR || screen->mode == SCREEN_ALERT ||
      screen->mode == SCREEN_NAMEEDIT || screen->mode == SCREEN_BODYEDIT) {
    return;
  }
  screen->mode = SCREEN_SENT;
}

void screen_reset(screen_t *screen) {
  screen->mode = SCREEN_MENU;
  screen->sel = 0;
  screen->inbox_n = 0;
  screen->outbox_n = 0;
  screen->reading = 0;
  screen->has_alert = false;
  screen->flash_start = 0;
  screen->flash_ms = 0;
  screen->has_compose_dest = false;
  screen->sent_to[0] = 0;
  screen->sent_body[0] = 0;
  screen->loc_name[0] = 0;
  screen->track_name[0] = 0;
  screen->loc_busy = false;
  screen->loc_holding = false;
  screen->loc_need_refresh = false;
  screen->loc_capture_wait = false;
  text_edit_reset(&screen->edit);
  canned_reset(&screen->canned);
  screen->vibe_test = false;
  display_clear(screen->buf);
}
