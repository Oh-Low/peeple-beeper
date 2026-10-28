#include "sx1262_setup.h"

/* 915 MHz, SF7, 125 kHz, 4/5. Each command is a length byte, then the SPI bytes. */
static const uint8_t sx1262_setup_cmds[] = {
    2, 0x80, 0x00,
    /* 32 MHz TCXO is powered from DIO3. 1.8 V, 10 ms to start. */
    5, 0x97, 0x02, 0x00, 0x02, 0x80,
    2, 0x96, 0x00,
    2, 0x9D, 0x01,
    2, 0x89, 0x7F,
    2, 0x8A, 0x01,
    5, 0x86, 0x39, 0x30, 0x00, 0x00,
    3, 0x98, 0xE1, 0xE9,
    5, 0x95, 0x04, 0x07, 0x00, 0x01,
    3, 0x8E, 0xF7, 0x04,
    3, 0x8F, 0x00, 0x00,
    5, 0x8B, 0x07, 0x04, 0x01, 0x00,
    7, 0x8C, 0x00, 0x08, 0x00, 0xFF, 0x01, 0x00,
    9, 0x08, 0x02, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0
};

int sx1262_setup(sx1262_write_fn write) {
  const uint8_t *p = sx1262_setup_cmds;
  while (*p) {
    if (!write(p + 1, *p)) return 0;
    p += 1 + *p;
  }
  return 1;
}

int sx1262_packet_len(sx1262_write_fn write, uint8_t len) {
  const uint8_t *p = sx1262_setup_cmds;
  uint8_t pkt[7];
  int i;
  while (*p) {
    if (*p == 7 && p[1] == 0x8C) {
      for (i = 0; i < 7; i++) pkt[i] = p[1 + i];
      pkt[4] = len;
      return write(pkt, 7);
    }
    p += 1 + *p;
  }
  return 0;
}
