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
  pager_on_page(&c->b->pager, c->hal, page);
  c->got_page = true;
}

void beeper_init(beeper_t *b) {
  memset(b, 0, sizeof *b);
  identity_init(&b->identity);
  contacts_init(&b->contacts);
  pair_init(&b->pair);
  page_radio_init(&b->radio);
  locate_init(&b->locate);
  pager_init(&b->pager, &b->identity, &b->contacts, &b->pair, &b->radio, &b->locate);
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
    if (!pair_active(&b->pair) && !pair_error(&b->pair)) pager_finish_pair(&b->pager);
  } else {
    page_tick(&b->radio, &b->identity, &b->contacts, hal, on_page, &ctx, &b->locate);
  }

  pager_poll_locate(&b->pager);
  pager_poll_pose(&b->pager, hal);

  pager_poll_send(&b->pager, hal);
  bool track_grab = pager_poll_track(&b->pager, hal);

  if (!ctx.got_page && !track_grab) pager_input(&b->pager, hal);

  pager_render(&b->pager, hal);
  store_sync(b, hal);
}

int beeper_debug_key(beeper_t *b, const char *key) {
  return pager_handle_text(&b->pager, key) ? 1 : 0;
}

void beeper_reset(beeper_t *b, const hal_t *hal) {
  identity_reset(&b->identity);
  contacts_reset(&b->contacts);
  pair_reset(&b->pair, hal);
  page_radio_reset(&b->radio);
  locate_reset(&b->locate);
  b->locate.track_open = false;
  pager_reset(&b->pager);
  b->nvm_ready = true;
  b->nvm_hash = 0;
  store_save(b, hal);
  hal->vibe(hal->ctx, 0);
}
