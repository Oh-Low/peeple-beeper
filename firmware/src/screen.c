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
#define MENU_COMPOSE 0
#define MENU_INBOX 1
#define MENU_OUTBOX 2
#define MENU_PEEPLE 3
#define MENU_BEEPLE 4
#define MENU_SETTINGS 5

#define PEEP_N 2
#define PEEP_LOCATE 0
#define PEEP_CONTACT 1

#define STEP_POLL_MS 40
#define STEP_QUIET_MS 30000u
/* A save erases flash. Unsaved steps wait until the unit has been still for
   30 s, and two saves are at least 15 min apart. A walk with no pause is
   saved every 15 min. */
#define STEP_NVM_MS (15u * 60u * 1000u)

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
  SET_TZ,
  SET_DARK,
  SET_LIGHT,
  SET_VIBE,
  SET_BEEP,
  SET_N
};

enum { NAV_STAY = 0, NAV_PUSH = 1, NAV_POP = -1 };

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
  screen->light_mode = LIGHT_ON;
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

static uint32_t light_window(uint8_t mode) {
  if (mode == LIGHT_30S) return 30000u;
  if (mode == LIGHT_1M) return 60000u;
  if (mode == LIGHT_5M) return 300000u;
  return 0;
}

static void light_drive(const hal_t *hal, bool on) {
  if (hal && hal->backlight) hal->backlight(hal->ctx, on);
}

static void light_wake(screen_t *screen, const hal_t *hal) {
  if (!screen || !hal) return;
  uint32_t window = light_window(screen->light_mode);
  if (window == 0) {
    screen->light_lit = false;
    light_drive(hal, screen->light_mode == LIGHT_ON);
    return;
  }
  screen->light_since = hal->now_ms(hal->ctx);
  screen->light_lit = true;
  light_drive(hal, true);
}

static void light_service(screen_t *screen, const hal_t *hal) {
  if (!screen || !hal || !screen->light_lit) return;
  if (!deadline_live(hal->now_ms(hal->ctx), screen->light_since, light_window(screen->light_mode))) {
    screen->light_lit = false;
    light_drive(hal, false);
  }
}

static void settings_release(screen_t *screen) {
  screen->tz_edit = false;
  screen->light_edit = false;
}

static void light_step(screen_t *screen, int dir) {
  int n = (int)screen->light_mode + dir;
  if (n < LIGHT_OFF) n = LIGHT_ON;
  if (n > LIGHT_ON) n = LIGHT_OFF;
  screen->light_mode = (uint8_t)n;
}

static const char *light_label(uint8_t mode) {
  if (mode == LIGHT_30S) return "30s";
  if (mode == LIGHT_1M) return "1m";
  if (mode == LIGHT_5M) return "5m";
  if (mode == LIGHT_ON) return "on";
  return "off";
}

void screen_light_apply(screen_t *screen, const hal_t *hal) { light_wake(screen, hal); }

static void pad2(char *out, int n) {
  if (n < 10) snprintf(out, 8, "0%d", n);
  else snprintf(out, 8, "%d", n);
}

static int wrap_hour(int hour) {
  hour %= 24;
  if (hour < 0) hour += 24;
  return hour;
}

static void local_hm(const screen_t *screen, const hal_t *hal, uint8_t *hh, uint8_t *mm) {
  uint8_t h = 0, m = 0;
  hal->clock(hal->ctx, &h, &m);
  if (h > 23 || m > 59) {
    *hh = CLOCK_NONE;
    *mm = CLOCK_NONE;
    return;
  }
  *hh = (uint8_t)wrap_hour((int)h + (int)screen->tz_hours);
  *mm = m;
}

static void stamp_clock(const screen_t *screen, const hal_t *hal, uint8_t *hh, uint8_t *mm) {
  local_hm(screen, hal, hh, mm);
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
  stamp_clock(screen, hal, &row->sent_hh, &row->sent_mm);
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
        stamp_clock(screen, hal, &row->acked_hh, &row->acked_mm);
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
  stamp_clock(screen, hal, &row->received_hh, &row->received_mm);
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

#define BEEPLE_PAGE_N 2
#define BEEPLE_BODY_Y 22
#define BEEPLE_FLAVOR_Y 32

static bool beeple_sliding(const screen_t *screen, const hal_t *hal) {
  if (screen->beeple_slide_dir == 0) return false;
  return elapsed_u32(hal->now_ms(hal->ctx), screen->beeple_slide_start) < SCREEN_SLIDE_MS;
}

static void beeple_turn(screen_t *screen, const hal_t *hal, int dir) {
  if (beeple_sliding(screen, hal)) return;
  if (screen->beeple_focus == 0) {
    int page = screen->beeple_page + (dir > 0 ? 1 : -1);
    if (page < 0) page = BEEPLE_PAGE_N - 1;
    if (page >= BEEPLE_PAGE_N) page = 0;
    screen->beeple_slide_from = screen->beeple_page;
    screen->beeple_slide_dir = dir > 0 ? 1 : -1;
    screen->beeple_slide_start = hal->now_ms(hal->ctx);
    screen->beeple_page = (int8_t)page;
    return;
  }
  int from = screen->beeple;
  int id = from + dir;
  if (id < 0) id = BEEPLE_N - 1;
  if (id >= BEEPLE_N) id = 0;
  screen->beeple_slide_from = (int8_t)from;
  screen->beeple_slide_dir = dir > 0 ? 1 : -1;
  screen->beeple_slide_start = hal->now_ms(hal->ctx);
  screen_set_beeple(screen, id);
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
  if (screen->mode == SCREEN_SETTINGS && screen->tz_edit) {
    int h = (int)screen->tz_hours + dir;
    if (h < -12) h = -12;
    if (h > 14) h = 14;
    screen->tz_hours = (int8_t)h;
    return;
  }
  if (screen->mode == SCREEN_SETTINGS && screen->light_edit) {
    light_step(screen, dir);
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
  else if (screen->mode == SCREEN_LOCATE) clamp_sel(screen, screen->contacts->n + 1);
  else if (screen->mode == SCREEN_TRACKASK) clamp_sel(screen, 2);
}

static void stop_output_tests(screen_t *screen, const hal_t *hal) {
  screen->vibe_test = false;
  screen->beep_test = false;
  hal->vibe(hal->ctx, 0);
  hal->beep(hal->ctx, 0);
}

static void service_vibe_test(screen_t *screen, const hal_t *hal) {
  uint32_t phase;
  uint16_t left;
  if (!screen->vibe_test || screen->mode != SCREEN_SETTINGS) return;
  phase = (hal->now_ms(hal->ctx) - screen->vibe_since) % 500u;
  if (phase < 250u) {
    left = (uint16_t)(250u - phase);
    hal->vibe(hal->ctx, left);
  } else {
    hal->vibe(hal->ctx, 0);
  }
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
    if (screen->tz_edit) {
      screen->tz_edit = false;
      return;
    }
    if (screen->light_edit) {
      screen->light_edit = false;
      return;
    }
    stop_output_tests(screen, hal);
    screen->mode = SCREEN_MENU;
    screen->sel = MENU_SETTINGS;
    return;
  }
  if (screen->mode == SCREEN_BEEPLE) {
    if (screen->beeple_focus) {
      screen->beeple_focus = 0;
      screen->beeple_slide_dir = 0;
      return;
    }
    screen->beeple_slide_dir = 0;
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
    screen->sel = MENU_OUTBOX;
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
  if (screen->mode == SCREEN_COMPOSE) {
    screen->mode = SCREEN_MENU;
    screen->sel = MENU_COMPOSE;
    screen->has_compose_dest = false;
    return;
  }
  if (screen->mode == SCREEN_READ || screen->mode == SCREEN_INBOX) {
    screen->mode = SCREEN_MENU;
    screen->sel = MENU_INBOX;
    screen->has_compose_dest = false;
  }
}

static int on_press(screen_t *screen, const hal_t *hal) {
  if (screen->mode == SCREEN_PAIR || screen->mode == SCREEN_SENDING ||
      screen->mode == SCREEN_LOCATING) {
    return NAV_STAY;
  }
  if (screen->mode == SCREEN_LOCREAD) {
    if (locate_busy(screen->locate)) locate_abort(screen->locate);
    screen->loc_busy = false;
    screen->loc_need_refresh = false;
    screen->loc_capture_wait = false;
    screen->mode = SCREEN_LOCATE;
    screen->sel = screen->contacts->n ? 1 : 0;
    return NAV_POP;
  }

  if (screen->mode == SCREEN_SENT || screen->mode == SCREEN_FAILED) {
    dismiss_sent(screen, hal);
    return screen->mode == SCREEN_OUTBOX ? NAV_POP : NAV_STAY;
  }
  if (screen->mode == SCREEN_ALERT && screen->has_alert) {
    screen->reading = 0;
    screen->mode = SCREEN_READ;
    screen->has_alert = false;
    return NAV_PUSH;
  }
  if (screen->mode == SCREEN_READ) {
    screen->mode = SCREEN_INBOX;
    screen->sel = screen->reading;
    if (screen->inbox_n <= 0) screen->sel = 0;
    else if (screen->sel >= screen->inbox_n) screen->sel = screen->inbox_n - 1;
    return NAV_POP;
  }
  if (screen->mode == SCREEN_MENU) {
    if (screen->sel == MENU_COMPOSE) {
      screen->mode = SCREEN_COMPOSE;
      screen->has_compose_dest = false;
      screen->sel = 0;
    } else if (screen->sel == MENU_INBOX) {
      screen->mode = SCREEN_INBOX;
      screen->sel = 0;
    } else if (screen->sel == MENU_OUTBOX) {
      screen->mode = SCREEN_OUTBOX;
      screen->sel = 0;
    } else if (screen->sel == MENU_PEEPLE) {
      screen->mode = SCREEN_PEEPLE;
      screen->sel = PEEP_LOCATE;
    } else if (screen->sel == MENU_SETTINGS) {
      screen->mode = SCREEN_SETTINGS;
      screen->sel = SET_NAME;
      screen->settings_scroll = 0;
      settings_release(screen);
    } else if (screen->sel == MENU_BEEPLE) {
      screen->mode = SCREEN_BEEPLE;
      screen->beeple_page = 0;
      screen->beeple_focus = 0;
      screen->beeple_slide_dir = 0;
    }
    return screen->mode == SCREEN_MENU ? NAV_STAY : NAV_PUSH;
  }
  if (screen->mode == SCREEN_BEEPLE) {
    if (!beeple_sliding(screen, hal) && screen->beeple_focus == 0 && screen->beeple_page == 0) {
      screen->beeple_focus = 1;
    }
    return NAV_STAY;
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
    return screen->mode == SCREEN_PEEPLE ? NAV_STAY : NAV_PUSH;
  }
  if (screen->mode == SCREEN_SETTINGS) {
    if (screen->sel == SET_BACK) {
      settings_release(screen);
      stop_output_tests(screen, hal);
      screen->mode = SCREEN_MENU;
      screen->sel = MENU_SETTINGS;
      return NAV_POP;
    }
    if (screen->sel == SET_NAME) {
      stop_output_tests(screen, hal);
      begin_name_edit(screen);
      return NAV_PUSH;
    }
    if (screen->sel == SET_TRACK) {
      locate_set_open(screen->locate, !locate_is_open(screen->locate));
      return NAV_STAY;
    }
    if (screen->sel == SET_CLOCK) {
      screen->show_clock_bat = !screen->show_clock_bat;
      return NAV_STAY;
    }
    if (screen->sel == SET_TZ) {
      screen->tz_edit = !screen->tz_edit;
      return NAV_STAY;
    }
    if (screen->sel == SET_DARK) {
      screen->dark_mode = !screen->dark_mode;
      return NAV_STAY;
    }
    if (screen->sel == SET_LIGHT) {
      screen->light_edit = !screen->light_edit;
      return NAV_STAY;
    }
    if (screen->sel == SET_VIBE) {
      screen->vibe_test = true;
      screen->vibe_since = hal->now_ms(hal->ctx);
    }
    if (screen->sel == SET_BEEP) screen->beep_test = true;
    return NAV_STAY;
  }
  if (screen->mode == SCREEN_TRACKASK) {
    if (screen->sel == 0) {
      locate_allow(screen->locate, hal, screen->identity, screen->contacts);
    } else {
      locate_deny(screen->locate, hal, screen->identity, screen->contacts);
    }
    screen->mode = SCREEN_MENU;
    screen->sel = 0;
    return NAV_POP;
  }
  if (screen->mode == SCREEN_TRACKED) {
    screen->mode = SCREEN_MENU;
    screen->sel = 0;
    return NAV_POP;
  }
  if (screen->mode == SCREEN_NAMEEDIT) {
    if (text_edit_press(&screen->edit) == TEXT_PRESS_OK) save_name_edit(screen);
    return screen->mode == SCREEN_NAMEEDIT ? NAV_STAY : NAV_POP;
  }
  if (screen->mode == SCREEN_BODYEDIT) {
    text_edit_press_t result = text_edit_press(&screen->edit);
    if (result == TEXT_PRESS_OK) send_body_edit(screen, hal);
    else if (result == TEXT_PRESS_SAVE) save_canned_draft(screen, hal);
    return screen->mode == SCREEN_CANNED ? NAV_POP : NAV_STAY;
  }
  if (screen->mode == SCREEN_INBOX) {
    if (screen->sel >= 0 && screen->sel < screen->inbox_n) {
      screen->reading = screen->sel;
      screen->mode = SCREEN_READ;
    }
    return screen->mode == SCREEN_INBOX ? NAV_STAY : NAV_PUSH;
  }
  if (screen->mode == SCREEN_OUTBOX) {
    if (screen->sel >= 0 && screen->sel < screen->outbox_n) {
      if (screen->outbox[screen->sel].status == OUTBOX_FAILED) {
        retry_outbox(screen, hal, screen->sel);
        return NAV_STAY;
      }
      screen->reading = screen->sel;
      screen->mode = SCREEN_OUTREAD;
    }
    return screen->mode == SCREEN_OUTBOX ? NAV_STAY : NAV_PUSH;
  }
  if (screen->mode == SCREEN_OUTREAD) {
    if (screen->reading >= 0 && screen->reading < screen->outbox_n &&
        screen->outbox[screen->reading].status == OUTBOX_FAILED) {
      retry_outbox(screen, hal, screen->reading);
      return NAV_STAY;
    }
    screen->mode = SCREEN_OUTBOX;
    screen->sel = screen->reading;
    if (screen->outbox_n <= 0) screen->sel = 0;
    else if (screen->sel >= screen->outbox_n) screen->sel = screen->outbox_n - 1;
    return NAV_POP;
  }
  if (screen->mode == SCREEN_COMPOSE) {
    if (!screen->contacts || screen->sel < 0 || screen->sel >= screen->contacts->n) return NAV_STAY;
    memcpy(screen->compose_dest, screen->contacts->rows[screen->sel].node_id, NODE_ID_LEN);
    screen->has_compose_dest = true;
    screen->mode = SCREEN_CANNED;
    screen->sel = 0;
    return NAV_STAY;
  }
  if (screen->mode == SCREEN_CANNED) {
    int n = canned_msg_n(screen);
    if (screen->sel == n) {
      begin_body_edit(screen);
      return NAV_PUSH;
    }
    if (!screen->has_compose_dest) return NAV_STAY;
    char list[CANNED_SAVED_MAX + CANNED_DEFAULT_N][PAGE_BODY_MAX + 1];
    int listed = canned_list(&screen->canned, list, CANNED_SAVED_MAX + CANNED_DEFAULT_N);
    if (screen->sel >= 0 && screen->sel < listed) send_page(screen, hal, screen->compose_dest, list[screen->sel], -1);
    return NAV_STAY;
  }
  if (screen->mode == SCREEN_LOCATE) {
    if (screen->sel == 0) {
      screen->mode = SCREEN_PEEPLE;
      screen->sel = PEEP_LOCATE;
      return NAV_POP;
    }
    if (screen->contacts->n == 0) return NAV_STAY;
    int idx = screen->sel - 1;
    if (idx < 0 || idx >= screen->contacts->n) return NAV_STAY;
    const contact_t *c = &screen->contacts->rows[idx];
    str_copy_max(screen->loc_name, sizeof screen->loc_name, c->name, PAGE_FROM_MAX);
    if (!locate_tx(screen->locate, hal, c->node_id, screen->contacts)) {
      screen->loc_busy = true;
      screen->mode = SCREEN_LOCREAD;
      return NAV_STAY;
    }
    screen->loc_busy = false;
    screen->mode = SCREEN_LOCATING;
    return NAV_PUSH;
  }
  return NAV_STAY;
}

void screen_on_page(screen_t *screen, const hal_t *hal, const page_t *page) {
  push_inbox(screen, page, hal);
  memset(&screen->alert, 0, sizeof screen->alert);
  copy_page(page, &screen->alert);
  screen->has_alert = true;
  screen->flash_start = hal->now_ms(hal->ctx);
  screen->flash_ms = FLASH_MS;
  settings_release(screen);
  stop_output_tests(screen, hal);
  hal->vibe(hal->ctx, 250);
  hal->beep(hal->ctx, 80);
  light_wake(screen, hal);
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

static void draw_slant_header(screen_t *screen, uint8_t *buf, const hal_t *hal, const char *title) {
  draw_slant_bar(buf, title);
  uint8_t hh = 0, mm = 0;
  char clock[8];
  local_hm(screen, hal, &hh, &mm);
  format_hm(clock, sizeof clock, hh, mm);
  display_draw_text_small(buf, 107, 3, clock);
}

static void format_battery(char *out, size_t cap, const battery_t *b) {
  if (b->charging && b->minutes >= 60) {
    snprintf(out, cap, "%u%% %uh", (unsigned)b->pct, (unsigned)((b->minutes + 30) / 60));
  } else if (b->charging && b->minutes > 0) {
    snprintf(out, cap, "%u%% %um", (unsigned)b->pct, (unsigned)b->minutes);
  } else {
    snprintf(out, cap, "%u%%", (unsigned)b->pct);
  }
}

static void draw_side_menu(screen_t *screen, const hal_t *hal, uint8_t *buf, char labels[][24], int n) {
  int y0 = 4;
  int pitch = 10;
  if (screen->show_clock_bat) {
    uint8_t hh = 0, mm = 0;
    char clock[8];
    local_hm(screen, hal, &hh, &mm);
    format_hm(clock, sizeof clock, hh, mm);
    display_draw_text(buf, 4, 1, clock);
    battery_t sample;
    char bat[16];
    int timed = 0;
    if (!hal->battery(hal->ctx, &sample)) str_copy_max(bat, sizeof bat, "--", 2);
    else {
      timed = sample.charging && sample.minutes > 0;
      format_battery(bat, sizeof bat, &sample);
    }
    if (timed) {
      int w = display_text_width_small(bat);
      display_draw_text_small(buf, MENU_HALF - 2 - w, 2, bat);
    } else {
      display_draw_text(buf, MENU_HALF - 2 - display_text_width(bat), 1, bat);
    }
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
  str_copy_max(labels[MENU_COMPOSE], sizeof labels[MENU_COMPOSE], "Compose", 16);
  if (count) snprintf(labels[MENU_INBOX], sizeof labels[MENU_INBOX], "Inbox (%d)", count);
  else str_copy_max(labels[MENU_INBOX], sizeof labels[MENU_INBOX], "Inbox", 16);
  str_copy_max(labels[MENU_OUTBOX], sizeof labels[MENU_OUTBOX], pending ? "Outbox (*)" : "Outbox", 16);
  str_copy_max(labels[MENU_PEEPLE], sizeof labels[MENU_PEEPLE], "Peeple", 16);
  str_copy_max(labels[MENU_BEEPLE], sizeof labels[MENU_BEEPLE], "Beeple", 16);
  str_copy_max(labels[MENU_SETTINGS], sizeof labels[MENU_SETTINGS], "Settings", 16);
  draw_side_menu(screen, hal, buf, labels, MENU_N);
}

static void draw_peeple(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  char labels[PEEP_N][24];
  str_copy_max(labels[0], sizeof labels[0], "Locate", 16);
  str_copy_max(labels[1], sizeof labels[1], "Add", 16);
  draw_side_menu(screen, hal, buf, labels, PEEP_N);
}

static void draw_chevron(uint8_t *buf, int x, int y, int dir) {
  for (int row = 0; row < 5; row++) {
    int wide = row != 0 && row != 4;
    int origin = (!wide && dir > 0) ? x + 1 : x;
    int n = wide ? 2 : 1;
    for (int col = 0; col < n; col++) {
      int px = origin + col;
      if (px >= 0 && px < MENU_HALF) display_set_pixel(buf, px, y + row, true);
    }
  }
}

static void draw_marked(uint8_t *buf, int y, const char *label, bool marked) {
  int w = display_text_width_small(label);
  int x = (MENU_HALF - w) / 2;
  if (x < 6) x = 6;
  display_draw_text_small(buf, x, y, label);
  if (!marked) return;
  display_invert_rect(buf, x, y, w, FONT_SMALL_H);
  draw_chevron(buf, x - 5, y + 1, -1);
  draw_chevron(buf, x + w + 2, y + 1, 1);
}

static void draw_icon(uint8_t *buf, int x, int y, const char *const *rows, int n) {
  for (int row = 0; row < n; row++) {
    for (int col = 0; rows[row][col]; col++) {
      if (rows[row][col] == '#') display_set_pixel(buf, x + col, y + row, true);
    }
  }
}

static void draw_beeple_name(uint8_t *buf, const screen_t *screen, bool marked) {
  const beeple_def_t *b = beeple_at(screen->beeple);
  const char *name = b ? b->name : "";
  draw_marked(buf, BEEPLE_BODY_Y, name, marked);
}

static void draw_beeple_flavor(uint8_t *buf) {
  char lines[4][DISPLAY_WIDTH + 1];
  int n = display_wrap("Lorem ipsum dolor sit amet, consectetur adipiscing elit.", 58, lines, 4);
  for (int i = 0; i < n; i++) display_draw_text_small(buf, 3, BEEPLE_FLAVOR_Y + i * 8, lines[i]);
}

static void draw_beeple_select(uint8_t *buf, const screen_t *screen) {
  draw_beeple_name(buf, screen, screen->beeple_focus == 1);
  draw_beeple_flavor(buf);
}

static void format_steps(char *out, size_t cap, uint32_t n) {
  if (n < 10000u) {
    snprintf(out, cap, "%u", n);
    return;
  }
  if (n < 100000u) {
    snprintf(out, cap, "%u.%uk", n / 1000u, (n % 1000u) / 100u);
    return;
  }
  if (n < 1000000u) {
    snprintf(out, cap, "%uk", n / 1000u);
    return;
  }
  if (n < 10000000u) {
    snprintf(out, cap, "%u.%um", n / 1000000u, (n % 1000000u) / 100000u);
    return;
  }
  if (n < 1000000000u) {
    snprintf(out, cap, "%um", n / 1000000u);
    return;
  }
  snprintf(out, cap, "%u.%ub", n / 1000000000u, (n % 1000000000u) / 100000000u);
}

static void draw_beeple_stats(uint8_t *buf, const screen_t *screen) {
  static const char *battery[] = {"###", "###", "###", "######", "#.....#", "#######"};
  static const char *spark[] = {"#....#", "....###", "#..#.#.#", "..###.###", "#..#.#.#", "....###", "#....#"};
  static const char *heart[] = {".##...##", "#..#.#..#", "#...#...#", ".#.....#", "..#...#", "...#.#", "....#"};
  static const char *bar[] = {
      "#################", "#######.........#", "#######.........#", "#######.........#",
      "#######.........#", "#######.........#", "#################",
  };
  static const char *bolt[] = {"#", "....###", "#..#.#.#", "..#..##.#", "#.#..#..#", "..#.....#", "#..#...#", "....###", "#"};
  static const char *mail[] = {"#########", "##.....##", "#.#...#.#", "#..###..#", "#.......#", "#.......#", "#########"};
  static const char *figure[] = {"#", "", "#..###...#", "..#.###.##", "#.########", "..#####.##", "#..###...#"};
  static const char *clock[] = {".....###", ".##.#...#", "#..#....#", "#..#.....#", ".#....#..#", "..####.##"};
  int step_id = screen->beeple;
  char steps[16];
  if (step_id < 0 || step_id >= BEEPLE_N) step_id = 0;
  format_steps(steps, sizeof steps, screen->steps[step_id]);
  draw_beeple_name(buf, screen, false);
  draw_icon(buf, 3, 30, battery, 6);
  display_draw_text_small(buf, 14, 29, steps);
  draw_icon(buf, 34, 29, spark, 7);
  display_draw_text_small(buf, 46, 29, "87");
  draw_icon(buf, 2, 38, heart, 7);
  draw_icon(buf, 14, 38, bar, 7);
  draw_icon(buf, 34, 37, bolt, 9);
  display_draw_text_small(buf, 46, 38, "45 s");
  draw_icon(buf, 2, 47, mail, 7);
  display_draw_text_small(buf, 14, 47, "781");
  draw_icon(buf, 34, 47, figure, 7);
  display_draw_text_small(buf, 46, 48, "14 kg");
  draw_icon(buf, 2, 56, clock, 6);
  display_draw_text_small(buf, 14, 56, "42 hr");
  for (int y = 29; y <= 61; y += 2) display_set_pixel(buf, 34, y, true);
}

static void draw_beeple_body(uint8_t *buf, const screen_t *screen, int page) {
  if (page == 0) draw_beeple_select(buf, screen);
  else draw_beeple_stats(buf, screen);
}

static void copy_col(uint8_t *dst, const uint8_t *src, int dx, int sx, int y0, int y1) {
  if (dx < 0 || sx < 0 || dx >= DISPLAY_WIDTH || sx >= DISPLAY_WIDTH) return;
  for (int y = y0; y < y1; y++) display_set_pixel(dst, dx, y, display_get_pixel(src, sx, y));
}

static void paint_slide(uint8_t *dst, const uint8_t *src, int x0, int y0, int w, int h, int shift,
                        int dir, bool incoming) {
  int y1 = y0 + h;
  if (dir > 0) {
    if (!incoming) {
      for (int x = 0; x < w - shift; x++) copy_col(dst, src, x0 + x, x0 + x + shift, y0, y1);
    } else {
      for (int x = w - shift; x < w; x++)
        copy_col(dst, src, x0 + x, x0 + x - (w - shift), y0, y1);
    }
  } else if (!incoming) {
    for (int x = shift; x < w; x++) copy_col(dst, src, x0 + x, x0 + x - shift, y0, y1);
  } else {
    for (int x = 0; x < shift; x++) copy_col(dst, src, x0 + x, x0 + x + (w - shift), y0, y1);
  }
}

static void draw_art_id(uint8_t *buf, int id) {
  const beeple_def_t *b = beeple_at(id);
  if (!b) return;
  blit_slot(buf, b->bits, b->w, b->h, MENU_HALF, 0, MENU_HALF, DISPLAY_HEIGHT);
}

static bool beeple_slide_shift(const screen_t *screen, const hal_t *hal, int *shift) {
  if (screen->beeple_slide_dir == 0) return false;
  uint32_t elapsed = elapsed_u32(hal->now_ms(hal->ctx), screen->beeple_slide_start);
  if (elapsed >= SCREEN_SLIDE_MS) return false;
  int s = (int)((elapsed * (uint32_t)MENU_HALF) / SCREEN_SLIDE_MS);
  if (s > MENU_HALF) s = MENU_HALF;
  *shift = s;
  return true;
}

static void draw_beeple_column(screen_t *screen, uint8_t *buf, bool sliding, int shift) {
  static const char *tabs[BEEPLE_PAGE_N] = {"Select", "Stats"};
  int page = screen->beeple_page;
  draw_marked(buf, 13, tabs[page], screen->beeple_focus == 0);
  if (!sliding) {
    draw_beeple_body(buf, screen, page);
    return;
  }
  uint8_t tmp[DISPLAY_BUF_LEN];
  if (screen->beeple_focus) {
    draw_beeple_name(buf, screen, true);
    display_clear(tmp);
    draw_beeple_flavor(tmp);
    paint_slide(buf, tmp, 0, BEEPLE_FLAVOR_Y, MENU_HALF, DISPLAY_HEIGHT - BEEPLE_FLAVOR_Y, shift,
                screen->beeple_slide_dir, false);
    display_clear(tmp);
    draw_beeple_flavor(tmp);
    paint_slide(buf, tmp, 0, BEEPLE_FLAVOR_Y, MENU_HALF, DISPLAY_HEIGHT - BEEPLE_FLAVOR_Y, shift,
                screen->beeple_slide_dir, true);
    return;
  }
  int from = screen->beeple_slide_from;
  display_clear(tmp);
  draw_beeple_body(tmp, screen, from);
  paint_slide(buf, tmp, 0, BEEPLE_BODY_Y, MENU_HALF, DISPLAY_HEIGHT - BEEPLE_BODY_Y, shift,
              screen->beeple_slide_dir, false);
  display_clear(tmp);
  draw_beeple_body(tmp, screen, page);
  paint_slide(buf, tmp, 0, BEEPLE_BODY_Y, MENU_HALF, DISPLAY_HEIGHT - BEEPLE_BODY_Y, shift,
              screen->beeple_slide_dir, true);
}

static void draw_beeple(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  int shift = 0;
  bool sliding = beeple_slide_shift(screen, hal, &shift);
  if (!sliding) screen->beeple_slide_dir = 0;
  if (sliding && screen->beeple_focus) {
    uint8_t tmp[DISPLAY_BUF_LEN];
    display_clear(tmp);
    draw_art_id(tmp, screen->beeple_slide_from);
    paint_slide(buf, tmp, MENU_HALF, 0, MENU_HALF, DISPLAY_HEIGHT, shift, screen->beeple_slide_dir, false);
    display_clear(tmp);
    draw_art_id(tmp, screen->beeple);
    paint_slide(buf, tmp, MENU_HALF, 0, MENU_HALF, DISPLAY_HEIGHT, shift, screen->beeple_slide_dir, true);
  } else {
    draw_menu_art(screen, buf);
  }
  draw_slant_header(screen, buf, hal, "BEEPLE");
  draw_beeple_column(screen, buf, sliding, shift);
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
  draw_slant_header(screen, buf, hal, "INBOX");
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
    display_clip_px(name, sizeof name, screen->inbox[idx].from, time_x - 2 - name_x);
    char body[PAGE_BODY_MAX + 1];
    display_clip_px(body, sizeof body, screen->inbox[idx].body, msg_end - msg_x + 1);
    display_draw_text_small(buf, name_x, y, name);
    display_draw_text_small(buf, time_x, y, hm);
    display_draw_text_small(buf, msg_x, y, body);
    if (idx == screen->sel) display_invert_rect(buf, 0, y - 1, DISPLAY_WIDTH, pitch);
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
  draw_slant_header(screen, buf, hal, "OUTBOX");
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
    display_clip_px(name, sizeof name, item->to, status_x - 2 - name_x);
    char body[PAGE_BODY_MAX + 1];
    display_clip_px(body, sizeof body, item->body, msg_end - msg_x + 1);
    display_draw_text_small(buf, name_x, y, name);
    display_draw_text_small(buf, status_x, y, status);
    display_draw_text_small(buf, msg_x, y, body);
    if (idx == screen->sel) display_invert_rect(buf, 0, y - 1, DISPLAY_WIDTH, pitch);
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
  display_clip_px(tclip, sizeof tclip, title, DISPLAY_WIDTH - 2);
  display_draw_text(buf, 2, 2, tclip);
  display_invert_rect(buf, 0, 0, DISPLAY_WIDTH, CHAR_H + 4);
  char to[40];
  snprintf(to, sizeof to, "To : %s", p->to);
  display_clip_px(tclip, sizeof tclip, to, DISPLAY_WIDTH - 2);
  display_draw_text(buf, 2, 14, tclip);
  char sent[8], ack[8], times[48];
  format_hm(sent, sizeof sent, p->sent_hh, p->sent_mm);
  format_hm(ack, sizeof ack, p->acked_hh, p->acked_mm);
  snprintf(times, sizeof times, "sent %s  ack %s", sent, ack);
  display_clip_px(tclip, sizeof tclip, times, DISPLAY_WIDTH - 2);
  display_draw_text(buf, 2, 24, tclip);
  char lines[DISPLAY_WRAP_MAX][DISPLAY_WIDTH + 1];
  int n = display_wrap(p->body, DISPLAY_WIDTH - 2, lines, DISPLAY_WRAP_MAX);
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
  header(buf, p->from, hal);
  char hm[8];
  format_hm(hm, sizeof hm, p->received_hh, p->received_mm);
  display_draw_text(buf, 2, 14, hm);
  char lines[DISPLAY_WRAP_MAX][DISPLAY_WIDTH + 1];
  int n = display_wrap(p->body, DISPLAY_WIDTH - 2, lines, DISPLAY_WRAP_MAX);
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
  for (int i = 0; i < screen->contacts->n; i++) {
    display_clip_px(rows[i + 1], sizeof rows[i + 1], screen->contacts->rows[i].name, DISPLAY_WIDTH - 4);
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
  display_clip_px(clip, sizeof clip, text ? text : "", width);
  display_draw_text_small(buf, x, y, clip);
}

static void draw_track_looking(screen_t *screen, uint8_t *buf) {
  char me[PAGE_FROM_MAX + 1];
  char line[PAGE_FROM_MAX + 8];
  identity_get_display_name(screen->identity, me, sizeof me);
  draw_track_chrome(buf);
  draw_track_line(buf, 6, 19, me[0] ? me : "BEEPLE", 58);
  draw_track_line(buf, 6, 29, "LOOKING", 58);
  snprintf(line, sizeof line, "FOR %s", screen->loc_name[0] ? screen->loc_name : "PEEPLE");
  draw_track_line(buf, 6, 38, line, 58);
}

static void draw_track_found(screen_t *screen, uint8_t *buf) {
  char line[PAGE_FROM_MAX + 12];
  draw_track_chrome(buf);
  snprintf(line, sizeof line, "%s FOUND!", screen->loc_name[0] ? screen->loc_name : "BEEPLE");
  draw_track_line(buf, 6, 19, line, 58);
  draw_track_line(buf, 6, 29, "ASKING NICELY", 58);
}

static void draw_track_denied(uint8_t *buf) {
  draw_track_chrome(buf);
  draw_track_line(buf, 6, 19, "DENIED.", 58);
  draw_track_line(buf, 6, 29, "THIS IS", 58);
  draw_track_line(buf, 6, 39, "devastating.", 58);
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
  display_clip_px(title, sizeof title, screen->loc_name[0] ? screen->loc_name : "LOCATE", DISPLAY_WIDTH - 8);
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
    char when[8];
    str_copy_max(when, sizeof when, view.when, 5);
    if (when[2] == ':' && when[0] >= '0' && when[0] <= '9' && when[1] >= '0' && when[1] <= '9') {
      int hour = wrap_hour((when[0] - '0') * 10 + (when[1] - '0') + (int)screen->tz_hours);
      when[0] = (char)('0' + hour / 10);
      when[1] = (char)('0' + hour % 10);
    }
    snprintf(line, sizeof line, "Last Loc : %s", when);
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
  draw_slant_header(screen, buf, hal, "COMPOSE");
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
      display_clip_px(name, sizeof name, screen->contacts->rows[idx].name, name_end - name_x + 1);
      display_draw_text_small(buf, name_x, y, name);
      if (idx == name_focus) display_invert_rect(buf, 0, y - 1, name_end + 2, pitch);
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
    display_clip_px(body, sizeof body, list[idx], msg_end - msg_x + 1);
    display_draw_text_small(buf, msg_x, y, body);
    if (messages && idx == screen->sel) {
      display_invert_rect(buf, 49, y - 1, DISPLAY_WIDTH - 49, pitch);
    }
  }
  int custom_y = y0 + msg_scroll * pitch;
  display_draw_text_small(buf, msg_x, custom_y, "Custom");
  if (messages && screen->sel == n) {
    display_invert_rect(buf, 49, custom_y - 1, DISPLAY_WIDTH - 49, pitch);
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
  display_clip_px(clip, sizeof clip, text ? text : "", right - left);
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
    display_clip_px(clip, sizeof clip, err, DISPLAY_WIDTH - 2);
    display_draw_text(buf, 2, 52, clip);
  }
}

static void name_for(screen_t *screen, const uint8_t *id, char *out, size_t n) {
  const contact_t *c = id ? contacts_find_by_id(screen->contacts, id) : NULL;
  if (c && c->name[0]) str_copy_max(out, n, c->name, PAGE_FROM_MAX);
  else str_copy_max(out, n, "contact", 16);
}

static const char *SET_LABEL[SET_N] = {
    "BACK", "NAME", "TRACKING", "CLOCK BAT", "TIME ZONE", "DARK MODE", "BACKLIGHT", "VIBE TEST",
    "BEEP TEST",
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
  else if (idx == SET_TZ) {
    if (screen->tz_hours > 0) snprintf(out, cap, "GMT+%d", (int)screen->tz_hours);
    else if (screen->tz_hours < 0) snprintf(out, cap, "GMT%d", (int)screen->tz_hours);
    else str_copy_max(out, cap, "GMT", 3);
  }
  else if (idx == SET_DARK) str_copy_max(out, cap, screen->dark_mode ? "on" : "off", 4);
  else if (idx == SET_LIGHT) str_copy_max(out, cap, light_label(screen->light_mode), 4);
  else if (idx == SET_VIBE) str_copy_max(out, cap, screen->vibe_test ? "on" : "off", 4);
  else if (idx == SET_BEEP) str_copy_max(out, cap, screen->beep_test ? "on" : "off", 4);
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
    if (idx == screen->sel && !((idx == SET_TZ && screen->tz_edit) || (idx == SET_LIGHT && screen->light_edit)))
      invert_cut_corner(buf, SET_BAR_X, y - 1, SET_BAR_W, SET_BAR_H);
    if (((idx == SET_TZ && screen->tz_edit) || (idx == SET_LIGHT && screen->light_edit)) && value[0]) {
      int span = display_text_width_small(value);
      display_invert_rect(buf, SET_VALUE_END - span, y - 1, span + 2, SET_BAR_H);
    }
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
  display_clip_px(clip, sizeof clip, name, DISPLAY_WIDTH - 2);
  display_draw_text(buf, 2, 16, clip);
  display_draw_text(buf, 2, 32, "Allow");
  display_draw_text(buf, 2, 44, "Deny");
  int y = screen->sel == 0 ? 31 : 43;
  display_invert_rect(buf, 0, y, DISPLAY_WIDTH, 10);
}

static void draw_tracked(screen_t *screen, const hal_t *hal, uint8_t *buf) {
  header(buf, "TRACKED", hal);
  char clip[32];
  display_clip_px(clip, sizeof clip, screen->track_name, DISPLAY_WIDTH - 2);
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
    display_clip_px(name, sizeof name, screen->alert.from, 26);
    display_draw_text_small(buf, 3, 17, name);
    char hm[8];
    if (screen->inbox_n > 0) {
      format_hm(hm, sizeof hm, screen->inbox[0].received_hh, screen->inbox[0].received_mm);
    } else {
      format_hm(hm, sizeof hm, CLOCK_NONE, CLOCK_NONE);
    }
    display_draw_text_small(buf, 32, 17, hm);
    char lines[4][DISPLAY_WIDTH + 1];
    int n = display_wrap(screen->alert.body, 60, lines, 4);
    for (int i = 0; i < n; i++) display_draw_text_small(buf, 3, 29 + i * 8, lines[i]);
  }
  if (deadline_live(hal->now_ms(hal->ctx), screen->flash_start, screen->flash_ms)) {
    display_invert_rect(buf, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  }
}

static bool slide_cut(screen_mode_t mode) {
  return mode == SCREEN_SENDING || mode == SCREEN_ALERT || mode == SCREEN_LOCREAD;
}

static bool slide_playing(const screen_t *screen, const hal_t *hal) {
  if (screen->slide_dir == 0 || screen->mode != screen->slide_mode) return false;
  return elapsed_u32(hal->now_ms(hal->ctx), screen->slide_start) < SCREEN_SLIDE_MS;
}

static void drop_encoder(const hal_t *hal) {
  while (hal->encoder_poll(hal->ctx) != ENC_NONE) {
  }
}

static void arm_slide(screen_t *screen, const hal_t *hal, int dir) {
  if (slide_cut(screen->mode)) {
    screen->slide_dir = 0;
    return;
  }
  memcpy(screen->slide_from, hal->display_buffer(hal->ctx), DISPLAY_BUF_LEN);
  screen->slide_start = hal->now_ms(hal->ctx);
  screen->slide_mode = screen->mode;
  screen->slide_dir = (int8_t)dir;
}

void screen_input(screen_t *screen, const hal_t *hal) {
  bool woke = false;
  if (screen->slide_dir != 0 && screen->mode != screen->slide_mode) screen->slide_dir = 0;
  if (hal->back_poll(hal->ctx)) {
    screen_mode_t before = screen->mode;
    on_back(screen, hal);
    woke = true;
    if (before == SCREEN_CANNED && screen->mode == SCREEN_COMPOSE) {
      drop_encoder(hal);
      light_wake(screen, hal);
      return;
    }
    if (screen->mode != before) arm_slide(screen, hal, NAV_POP);
  }
  if (slide_playing(screen, hal)) {
    while (hal->encoder_poll(hal->ctx) != ENC_NONE) woke = true;
    if (woke) light_wake(screen, hal);
    return;
  }
  for (;;) {
    encoder_event_t ev = hal->encoder_poll(hal->ctx);
    if (ev == ENC_NONE) break;
    woke = true;
    if (screen->mode == SCREEN_PAIR || screen->mode == SCREEN_SENDING ||
        screen->mode == SCREEN_LOCATING) {
      continue;
    }
    if (ev == ENC_CW || ev == ENC_CCW) {
      int dir = ev == ENC_CW ? 1 : -1;
      if (screen->mode == SCREEN_BEEPLE) beeple_turn(screen, hal, dir);
      else on_turn(screen, dir);
    }
    else if (ev == ENC_PRESS) {
      screen_mode_t before = screen->mode;
      int dir = on_press(screen, hal);
      if (dir) {
        arm_slide(screen, hal, dir);
        drop_encoder(hal);
        break;
      }
      if (screen->mode == SCREEN_BEEPLE ||
          (before == SCREEN_COMPOSE && screen->mode == SCREEN_CANNED)) {
        drop_encoder(hal);
        break;
      }
    }
  }
  if (woke) light_wake(screen, hal);
}

void screen_render(screen_t *screen, const hal_t *hal) {
  light_service(screen, hal);
  service_vibe_test(screen, hal);
  /* A transmit waits in board_sample, which ends a short beep. */
  if (screen->beep_test && screen->mode == SCREEN_SETTINGS) hal->beep(hal->ctx, 65535);
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
  else if (screen->mode == SCREEN_BEEPLE) draw_beeple(screen, hal, buf);
  else if (screen->mode == SCREEN_TRACKASK) draw_track_ask(screen, hal, buf);
  else if (screen->mode == SCREEN_TRACKED) draw_tracked(screen, hal, buf);
  else if (screen->mode == SCREEN_NAMEEDIT || screen->mode == SCREEN_BODYEDIT) {
    uint8_t hh = 0, mm = 0;
    local_hm(screen, hal, &hh, &mm);
    text_edit_draw(&screen->edit, buf, hh, mm);
  } else {
    draw_alert(screen, hal, buf);
  }
  if (screen->dark_mode) display_invert_rect(buf, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

  if (screen->slide_dir != 0 && screen->mode != screen->slide_mode) screen->slide_dir = 0;

  uint8_t *dst = hal->display_buffer(hal->ctx);
  if (screen->slide_dir != 0) {
    uint32_t elapsed = elapsed_u32(hal->now_ms(hal->ctx), screen->slide_start);
    if (elapsed >= SCREEN_SLIDE_MS) {
      screen->slide_dir = 0;
    } else {
      int shift = (int)((elapsed * (uint32_t)DISPLAY_WIDTH) / SCREEN_SLIDE_MS);
      if (screen->slide_dir < 0) shift = -shift;
      display_slide(dst, screen->slide_from, buf, shift);
      hal->display_flush(hal->ctx);
      return;
    }
  }
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

static void steps_commit(screen_t *screen, uint32_t now) {
  bool quiet;
  bool held;
  bool cooled;
  if (!screen->steps_holding) return;
  if (memcmp(screen->steps, screen->steps_saved, sizeof screen->steps) == 0) {
    screen->steps_holding = false;
    return;
  }
  quiet = elapsed_u32(now, screen->steps_last_ms) >= STEP_QUIET_MS;
  held = elapsed_u32(now, screen->steps_hold_ms) >= STEP_NVM_MS;
  cooled = !screen->steps_saved_ms || elapsed_u32(now, screen->steps_saved_ms) >= STEP_NVM_MS;
  if (!cooled || (!quiet && !held)) return;
  memcpy(screen->steps_saved, screen->steps, sizeof screen->steps);
  screen->steps_saved_ms = now ? now : 1;
  screen->steps_holding = false;
}

void screen_poll_steps(screen_t *screen, const hal_t *hal) {
  accel_sample_t sample;
  uint32_t now;
  int id;
  if (!screen || !hal) return;
  now = hal->now_ms(hal->ctx);
  if (hal->accel && (!screen->step_sampled || elapsed_u32(now, screen->step_sample_ms) >= STEP_POLL_MS)) {
    screen->step_sampled = true;
    screen->step_sample_ms = now;
    if (hal->accel(hal->ctx, &sample) && step_detect(&screen->step_det, &sample, now)) {
      id = screen->beeple;
      if (id >= 0 && id < BEEPLE_N && screen->steps[id] != UINT32_MAX) {
        screen->steps[id]++;
        screen->steps_last_ms = now;
        if (!screen->steps_holding) {
          screen->steps_holding = true;
          screen->steps_hold_ms = now;
        }
      }
    }
  }
  steps_commit(screen, now);
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
    settings_release(screen);
    stop_output_tests(screen, hal);
    screen->mode = SCREEN_TRACKASK;
    screen->sel = 1;
    hal->vibe(hal->ctx, 250);
    hal->beep(hal->ctx, 80);
    light_wake(screen, hal);
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
  settings_release(screen);
  light_wake(screen, hal);
  if (screen->mode == SCREEN_TRACKED) return false;
  stop_output_tests(screen, hal);
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
  if (screen->mode == SCREEN_SETTINGS) stop_output_tests(screen, hal);
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
  screen->beep_test = false;
  screen->slide_dir = 0;
  screen->beeple_slide_dir = 0;
  memset(screen->steps, 0, sizeof screen->steps);
  memset(screen->steps_saved, 0, sizeof screen->steps_saved);
  screen->steps_saved_ms = 0;
  screen->steps_last_ms = 0;
  screen->steps_hold_ms = 0;
  screen->steps_holding = false;
  screen->step_sampled = false;
  memset(&screen->step_det, 0, sizeof screen->step_det);
  display_clear(screen->buf);
}
