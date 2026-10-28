#ifndef BEEPER_SX1262_SETUP_H
#define BEEPER_SX1262_SETUP_H

#include <stdint.h>

typedef int (*sx1262_write_fn)(const uint8_t *bytes, int n);

int sx1262_setup(sx1262_write_fn write);
int sx1262_packet_len(sx1262_write_fn write, uint8_t len);

#endif
