#pragma once

#include "hal.h"

#include <stdint.h>

void beeper_hal_init(hal_t *out);

/* One byte from the PA1616S UART on Feather RX. */
void beeper_gps_push_byte(uint8_t byte);
