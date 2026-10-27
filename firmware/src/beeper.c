#include "beeper.h"

#include "store.h"

#include <string.h>

typedef struct {
  beeper_t *b;
  const hal_t *hal;
  bool got_page;
} tick_ctx_t;

static void on_page(void *user, const page_t *page) {
  tick_ctx_t *c = user;
  screen_on_page(&c->b->screen, c->hal, page);
  c->got_page = true;
}

void beeper_sessions_tick(message_session_t *session, identity_store_t *identity,
                          contact_book_t *contacts, const hal_t *hal, message_on_page_fn on_page,
                          void *user, locate_radio_t *locate) {
  if (locate) locate_clear_reply(locate);
  raw_frame_t raw;
  memset(&raw, 0, sizeof raw);
  while (hal->radio_poll_raw(hal->ctx, &raw)) {
    int kind = frame_type(raw.bytes, raw.len);
    if (kind == FRAME_LOC_REQ || kind == FRAME_LOC) {
      if (locate) locate_on_raw(locate, identity, contacts, hal, &raw);
      continue;
    }
    if (kind == FRAME_HELLO || kind == FRAME_HELLO_ACK || kind == FRAME_PAIR_LEAVE) continue;
    if (session) message_on_raw(session, identity, contacts, hal, &raw, on_page, user);
  }
  if (session && !(locate && (locate_busy(locate) || locate_sent_reply(locate)))) {
    message_outbox_tick(session, identity, contacts, hal);
  }
  if (session) relay_tick(&session->relay, hal);
  if (locate) locate_tick(locate, identity, contacts, hal);
}

void beeper_init(beeper_t *b) {
  memset(b, 0, sizeof *b);
  identity_init(&b->identity);
  contacts_init(&b->contacts);
  pair_init(&b->pair);
  message_init(&b->radio);
  locate_init(&b->locate);
  screen_init(&b->screen, &b->identity, &b->contacts, &b->pair, &b->radio, &b->locate);
  b->nvm_ready = false;
  b->nvm_hash = 0;
}

void beeper_tick(beeper_t *b, const hal_t *hal) {
  if (!b->nvm_ready) {
    if (!store_load(b, hal)) b->locate.track_open = false;
    b->nvm_ready = true;
  }
  identity_load_or_create(&b->identity, hal);
  locate_gps_tick(&b->locate, hal);

  tick_ctx_t ctx = {.b = b, .hal = hal, .got_page = false};
  if (pair_active(&b->pair)) {
    pair_tick(&b->pair, &b->identity, &b->contacts, hal);
    /* A repeat pair replaces the contact, so the count does not grow. */
    if (!pair_active(&b->pair) && !pair_error(&b->pair)) screen_finish_pair(&b->screen);
  } else {
    beeper_sessions_tick(&b->radio, &b->identity, &b->contacts, hal, on_page, &ctx, &b->locate);
  }

  screen_poll_locate(&b->screen);
  screen_poll_pose(&b->screen, hal);

  screen_poll_send(&b->screen, hal);
  bool track_grab = screen_poll_track(&b->screen, hal);

  if (!ctx.got_page && !track_grab) screen_input(&b->screen, hal);

  screen_render(&b->screen, hal);
  store_sync(b, hal);
}

int beeper_debug_key(beeper_t *b, const char *key) {
  return screen_handle_text(&b->screen, key) ? 1 : 0;
}

void beeper_reset(beeper_t *b, const hal_t *hal) {
  identity_reset(&b->identity);
  contacts_reset(&b->contacts);
  pair_reset(&b->pair, hal);
  message_reset(&b->radio);
  locate_reset(&b->locate);
  b->locate.track_open = false;
  screen_reset(&b->screen);
  b->nvm_ready = true;
  b->nvm_hash = 0;
  store_save(b, hal);
  hal->vibe(hal->ctx, 0);
}
