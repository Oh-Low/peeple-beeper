#pragma once

#include "contacts.h"
#include "hal.h"
#include "identity.h"
#include "page.h"
#include "pager.h"
#include "pair.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  identity_store_t identity;
  contact_book_t contacts;
  pair_session_t pair;
  page_radio_t radio;
  locate_radio_t locate;
  pager_t pager;
  bool nvm_ready;
  uint32_t nvm_hash;
} beeper_t;

void beeper_init(beeper_t *b);
void beeper_tick(beeper_t *b, const hal_t *hal);
void beeper_reset(beeper_t *b, const hal_t *hal);
int beeper_debug_key(beeper_t *b, const char *key);
