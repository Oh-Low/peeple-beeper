#pragma once

#include "hal.h"

#include <stdint.h>

void nrf_hal_init(hal_t *out);

/* Feed one byte from the PA1616S UART (Feather RX). A finished RMC line updates gps_fix. */
void nrf_gps_push_byte(uint8_t byte);
