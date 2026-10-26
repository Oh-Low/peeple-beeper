#pragma once

#include "beeper.h"
#include "hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t store_encode(beeper_t *b, uint8_t *out, size_t max);
bool store_apply(beeper_t *b, const uint8_t *bytes, size_t len);
bool store_load(beeper_t *b, const hal_t *hal);
bool store_save(beeper_t *b, const hal_t *hal);
void store_sync(beeper_t *b, const hal_t *hal);
void store_remember_hash(beeper_t *b, const uint8_t *bytes, size_t len);
uint32_t store_hash_blob(const uint8_t *bytes, size_t len);
