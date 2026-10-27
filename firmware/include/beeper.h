#pragma once

#include "contacts.h"
#include "hal.h"
#include "identity.h"
#include "message.h"
#include "screen.h"
#include "pair.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  identity_store_t identity;
  contact_book_t contacts;
  pair_session_t pair;
  message_session_t radio;
  locate_radio_t locate;
  screen_t screen;
  bool nvm_ready;
  uint32_t nvm_hash;
} beeper_t;

void beeper_init(beeper_t *b);
void beeper_tick(beeper_t *b, const hal_t *hal);
void beeper_sessions_tick(message_session_t *session, identity_store_t *identity,
                          contact_book_t *contacts, const hal_t *hal, message_on_page_fn on_page,
                          void *user, locate_radio_t *locate);
void beeper_reset(beeper_t *b, const hal_t *hal);
int beeper_debug_key(beeper_t *b, const char *key);
