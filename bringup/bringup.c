#include "sx1262_setup.h"

#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

enum {
  ACC_ADDR = 0x19,
  ACC_WHO = 0x0F,
  ACC_ID = 0x33,
  MAG_ADDR = 0x1E,
  MAG_WHO = 0x4F,
  MAG_ID = 0x40,
  SYNC_REG = 0x0740,
  SYNC_ID = 0x1424,
  CYCLES_PER_US = 64
};

typedef struct {
  uint8_t port;
  uint8_t pin;
} Pin;

static const Pin SCK = {0, 14};
static const Pin MO = {0, 13};
static const Pin MI = {0, 15};
static const Pin D10 = {0, 27};
static const Pin D11 = {0, 6};
static const Pin D12 = {0, 8};
static const Pin D13 = {1, 9};
static const Pin A0 = {0, 4};
static const Pin A1 = {0, 5};
static const Pin A2 = {0, 30};
static const Pin D6 = {0, 7};
static const Pin D9 = {0, 26};
static const Pin A3 = {0, 28};
static const Pin A4 = {0, 2};
static const Pin SDA = {0, 12};
static const Pin SCL = {0, 11};
static const Pin RX = {0, 24};
static const Pin D5 = {1, 8};
static const Pin LED_RED = {1, 15};

enum {
  CNF_OUT = 0x1u,
  CNF_IN_UP = 0xCu,
  CNF_OD = 6u << 8
};

static uint8_t fb[1024];
static uint8_t gps_buf[64] __attribute__((aligned(4)));
static uint32_t gps_bytes;
static uint32_t gps_dollars;
static int gps_run;
static int radio_released;
static int radio_ready;
static uint32_t radio_wait_us = 100000;
static uint32_t tx_count;
static uint32_t tx_done;
static uint32_t rx_count;
static uint32_t crc_count;
static uint32_t next_beacon;
static uint32_t last_radio;
static int listen_only;
static int chip_mode = -1;
static int cmd_status = -1;
static int packet_type = -1;
static int write_ok;
static int inst_rssi;
static int inst_seen;
static int peak_rssi;
static int peak_seen;
static int gps_page;
static int gps_status;
static int gps_sats = -1;
static char gps_time[12];
static char gps_lat[16];
static char gps_lon[16];
static char gps_ns;
static char gps_ew;
static char gps_hdop[8];
static int compass_on;
static int compass_seen;
static int acc_x, acc_y, acc_z;
static int mag_x, mag_y, mag_z;
static int acc_id = -1;
static int mag_id = -1;
static int radio_id = -1;
static unsigned closed_bits;
static uint8_t contrast = 0x34;
static uint8_t ratio = 0x22;
static uint8_t enc_prev;
static int enc_acc;
static int enc_on;
static int contrast_dirty;

static void gps_pump(void);

void *memset(void *dst, int c, unsigned long n) {
  unsigned char *p = dst;
  while (n--) *p++ = (unsigned char)c;
  return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  while (n--) *d++ = *s++;
  return dst;
}

void fault_blink(void) {
  REG(0x50000A00u + 15u * 4u) = 1u;
  for (;;) {
    REG(0x50000808u) = 1u << 15;
    for (volatile uint32_t i = 0; i < 200000u; i++) {
    }
    REG(0x5000080Cu) = 1u << 15;
    for (volatile uint32_t i = 0; i < 200000u; i++) {
    }
  }
}

static int pin_get(Pin p);

static void nudge_contrast(int dir) {
  int ev = (int)contrast + dir;
  if (ev > 0x3F) {
    if (ratio < 0x27) {
      ev = 0;
      ratio++;
    } else {
      ev = 0x3F;
    }
  } else if (ev < 0) {
    if (ratio > 0x20) {
      ev = 0x3F;
      ratio--;
    } else {
      ev = 0;
    }
  }
  contrast = (uint8_t)ev;
  contrast_dirty = 1;
}

static void note_encoder(void) {
  static const int8_t step_for[16] = {
      0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t now;
  int step;
  if (!enc_on) return;
  now = (uint8_t)((pin_get(D6) ? 2u : 0u) | (pin_get(D9) ? 1u : 0u));
  if (now == enc_prev) return;
  step = step_for[(enc_prev << 2) | now];
  enc_prev = now;
  if (!step) return;
  enc_acc += step;
  if (enc_acc >= 4) {
    enc_acc = 0;
    nudge_contrast(1);
  } else if (enc_acc <= -4) {
    enc_acc = 0;
    nudge_contrast(-1);
  }
}

static void note_page(void) {
  static int down;
  static int stable;
  static uint32_t since;
  int now_down = !pin_get(A3);
  uint32_t now = REG(0xE0001004u);
  if (now_down != down) {
    down = now_down;
    since = now;
    return;
  }
  if ((now - since) < 20u * 1000u * CYCLES_PER_US || stable == down) return;
  stable = down;
  if (down) gps_page = !gps_page;
}

static void note_back(void) {
  static int down;
  static int stable;
  static uint32_t since;
  int now_down = !pin_get(A4);
  uint32_t now = REG(0xE0001004u);
  if (now_down != down) {
    down = now_down;
    since = now;
    return;
  }
  if ((now - since) < 20u * 1000u * CYCLES_PER_US || stable == down) return;
  stable = down;
  if (down) listen_only = !listen_only;
}

static void note_buttons(void) {
  if (!pin_get(D6)) closed_bits |= 1u;
  if (!pin_get(D9)) closed_bits |= 2u;
  if (!pin_get(A3)) closed_bits |= 4u;
  if (!pin_get(A4)) closed_bits |= 8u;
  note_back();
  note_page();
  note_encoder();
}

static int button_closed(Pin p, unsigned bit) {
  return !pin_get(p) || (closed_bits & bit);
}

static void gps_take_byte(uint8_t byte);

static void gps_pump(void) {
  uint32_t n;
  uint32_t i;
  note_buttons();
  if (!gps_run) return;
  if (REG(0x40002124u)) {
    REG(0x40002124u) = 0;
    (void)REG(0x40002124u);
    REG(0x40002480u) = 0xFu;
  }
  if (!REG(0x40002110u)) return;
  n = REG(0x4000253Cu);
  REG(0x40002110u) = 0;
  (void)REG(0x40002110u);
  if (n > sizeof gps_buf) n = sizeof gps_buf;
  for (i = 0; i < n; i++) gps_take_byte(gps_buf[i]);
  REG(0x40002534u) = (uint32_t)(uintptr_t)gps_buf;
  REG(0x40002538u) = sizeof gps_buf;
  REG(0x40002000u) = 1u;
}

static void delay_us(uint32_t us) {
  uint32_t start = REG(0xE0001004u);
  uint32_t limit = us * CYCLES_PER_US;
  while ((REG(0xE0001004u) - start) < limit) gps_pump();
}

static void delay_ms(uint32_t ms) {
  while (ms--) delay_us(1000);
}

static volatile uint32_t *gpio(Pin p, uint32_t offset) {
  uint32_t base = p.port ? 0x50000300u : 0x50000000u;
  return (volatile uint32_t *)(uintptr_t)(base + offset);
}

static void pin_set(Pin p) { *gpio(p, 0x508) = 1u << p.pin; }
static void pin_clr(Pin p) { *gpio(p, 0x50C) = 1u << p.pin; }

static int pin_get(Pin p) { return (int)((*gpio(p, 0x510) >> p.pin) & 1u); }

static void pin_cnf(Pin p, uint32_t value) {
  *gpio(p, 0x700u + 4u * p.pin) = value;
}

static void drive(Pin p, int high) {
  if (high) pin_set(p);
  else pin_clr(p);
  pin_cnf(p, CNF_OUT);
}

static void pullup(Pin p) { pin_cnf(p, CNF_IN_UP); }

static void od_low(Pin p) {
  pin_clr(p);
  pin_cnf(p, CNF_OUT | CNF_OD);
}

static void od_release(Pin p) { pin_cnf(p, CNF_OD); }

static void clocks(void) {
  uint32_t i;
  REG(0xE000EDFCu) |= 1u << 24;
  REG(0xE0001004u) = 0;
  REG(0xE0001000u) |= 1u;
  REG(0x40000100u) = 0;
  REG(0x40000000u) = 1u;
  for (i = 0; i < 5000u && !REG(0x40000100u); i++) delay_us(1);
}

static void gpio_init(void) {
  drive(SCK, 0);
  drive(MO, 0);
  pullup(MI);
  drive(D10, 1);
  drive(A2, 1);
  drive(D12, 1);
  drive(A1, 1);
  drive(A0, 0);
  pullup(D11);
  /* No external resistor on these four. */
  pullup(D6);
  pullup(D9);
  pullup(A3);
  pullup(A4);
  pullup(D13); /* Pull-up holds the backlight on. */
  od_release(SDA);
  od_release(SCL);
  drive(D5, 1); /* High runs the receiver. */
  drive(LED_RED, 0);
  closed_bits = 0;
  enc_prev = (uint8_t)((pin_get(D6) ? 2u : 0u) | (pin_get(D9) ? 1u : 0u));
  enc_on = 1;
}

static void blink(void) {
  int i;
  for (i = 0; i < 3; i++) {
    drive(LED_RED, 1);
    delay_ms(80);
    drive(LED_RED, 0);
    delay_ms(80);
  }
}

static uint8_t spi_byte(uint8_t value) {
  uint8_t got = 0;
  int i;
  for (i = 0; i < 8; i++) {
    if (value & 0x80u) pin_set(MO);
    else pin_clr(MO);
    value = (uint8_t)(value << 1);
    delay_us(1);
    pin_set(SCK);
    got = (uint8_t)((got << 1) | (pin_get(MI) ? 1u : 0u));
    delay_us(1);
    pin_clr(SCK);
  }
  return got;
}

static void lcd_cmd(uint8_t command) {
  pin_set(D10);
  pin_clr(A0);
  pin_clr(A2);
  spi_byte(command);
  pin_set(A2);
}

static void lcd_init(void) {
  pin_clr(A1);
  delay_ms(10);
  pin_set(A1);
  delay_ms(10);
  lcd_cmd(0xE2);
  delay_ms(5);
  lcd_cmd(0xA2);
  lcd_cmd(0xA0);
  lcd_cmd(0xC8);
  lcd_cmd(0xA4);
  lcd_cmd(0xA6);
  lcd_cmd(0x40);
  /* 0x25 / 0x20 stayed black except at a steep angle. */
  lcd_cmd(ratio);
  lcd_cmd(0x81);
  lcd_cmd(contrast);
  lcd_cmd(0x2F);
  delay_ms(50);
  lcd_cmd(0xAF);
}

static void lcd_contrast(void) {
  uint8_t ev = contrast;
  uint8_t rr = ratio;
  contrast_dirty = 0;
  lcd_cmd(rr);
  lcd_cmd(0x81);
  lcd_cmd(ev);
}

static void px(int x, int y) {
  if ((unsigned)x >= 128u || (unsigned)y >= 64u) return;
  fb[(y >> 3) * 128 + x] |= (uint8_t)(1u << (y & 7));
}

/* Same glyphs as display.c FONT_SMALL for space through Z. */
static const uint8_t FONT[][3] = {
    {0x00, 0x00, 0x00}, {0x00, 0x2f, 0x00}, {0x03, 0x00, 0x03}, {0x1f, 0x0a, 0x1f},
    {0x26, 0x2b, 0x1a}, {0x31, 0x0c, 0x23}, {0x1a, 0x25, 0x1a}, {0x00, 0x03, 0x00},
    {0x00, 0x1e, 0x21}, {0x21, 0x1e, 0x00}, {0x2a, 0x1c, 0x2a}, {0x08, 0x3e, 0x08},
    {0x20, 0x18, 0x00}, {0x08, 0x08, 0x08}, {0x00, 0x30, 0x00}, {0x30, 0x0c, 0x03},
    {0x1e, 0x21, 0x1e}, {0x22, 0x3f, 0x20}, {0x31, 0x29, 0x26}, {0x21, 0x25, 0x1a},
    {0x07, 0x04, 0x3f}, {0x27, 0x25, 0x19}, {0x1e, 0x25, 0x19}, {0x01, 0x39, 0x07},
    {0x1a, 0x25, 0x1a}, {0x26, 0x29, 0x1e}, {0x00, 0x12, 0x00}, {0x20, 0x1a, 0x00},
    {0x0c, 0x12, 0x21}, {0x14, 0x14, 0x14}, {0x21, 0x12, 0x0c}, {0x01, 0x15, 0x02},
    {0x1e, 0x29, 0x2e}, {0x3e, 0x09, 0x3e}, {0x3f, 0x25, 0x1a}, {0x1e, 0x21, 0x21},
    {0x3f, 0x21, 0x1e}, {0x3f, 0x25, 0x21}, {0x3f, 0x05, 0x01}, {0x1e, 0x21, 0x39},
    {0x3f, 0x04, 0x3f}, {0x21, 0x3f, 0x21}, {0x11, 0x21, 0x1f}, {0x3f, 0x0c, 0x33},
    {0x3f, 0x20, 0x20}, {0x3f, 0x02, 0x3f}, {0x3f, 0x01, 0x3e}, {0x1e, 0x21, 0x1e},
    {0x3f, 0x09, 0x06}, {0x0e, 0x11, 0x2e}, {0x3f, 0x09, 0x36}, {0x22, 0x2d, 0x11},
    {0x01, 0x3f, 0x01}, {0x1f, 0x20, 0x1f}, {0x0f, 0x30, 0x0f}, {0x3f, 0x10, 0x3f},
    {0x33, 0x0c, 0x33}, {0x03, 0x3c, 0x03}, {0x31, 0x2d, 0x23},
};

static void draw_char(int x, int y, char c) {
  const uint8_t *glyph;
  int col;
  int row;
  if (c < ' ' || c > 'Z') return;
  glyph = FONT[c - ' '];
  for (col = 0; col < 3; col++) {
    for (row = 0; row < 6; row++) {
      if (glyph[col] & (1u << row)) px(x + col, y + row);
    }
  }
}

static void text(int x, int y, const char *s) {
  while (*s) {
    draw_char(x, y, *s);
    x += 4;
    s++;
  }
}

static void lcd_flush(void) {
  int page;
  int col;
  for (page = 0; page < 8; page++) {
    lcd_cmd((uint8_t)(0xB0 | page));
    lcd_cmd(0x10);
    lcd_cmd(0x00);
    pin_set(D10);
    pin_set(A0);
    pin_clr(A2);
    for (col = 0; col < 128; col++) spi_byte(fb[page * 128 + col]);
    pin_set(A2);
  }
}

static int scl_high(void) {
  int i;
  od_release(SCL);
  for (i = 0; i < 1000; i++) {
    if (pin_get(SCL)) return 1;
    delay_us(1);
  }
  return 0;
}

static void i2c_start(void) {
  od_release(SDA);
  scl_high();
  delay_us(5);
  od_low(SDA);
  delay_us(5);
  od_low(SCL);
}

static void i2c_stop(void) {
  od_low(SDA);
  delay_us(5);
  scl_high();
  delay_us(5);
  od_release(SDA);
  delay_us(5);
}

static int i2c_write(uint8_t value) {
  int i;
  int ack;
  for (i = 0; i < 8; i++) {
    if (value & 0x80u) od_release(SDA);
    else od_low(SDA);
    value = (uint8_t)(value << 1);
    delay_us(5);
    if (!scl_high()) return 0;
    delay_us(5);
    od_low(SCL);
  }
  od_release(SDA);
  delay_us(5);
  if (!scl_high()) return 0;
  ack = !pin_get(SDA);
  delay_us(5);
  od_low(SCL);
  return ack;
}

static int i2c_read(int ack) {
  int value = 0;
  int i;
  od_release(SDA);
  for (i = 0; i < 8; i++) {
    value <<= 1;
    if (!scl_high()) return -1;
    if (pin_get(SDA)) value |= 1;
    delay_us(5);
    od_low(SCL);
    delay_us(5);
  }
  if (ack) od_low(SDA);
  else od_release(SDA);
  delay_us(5);
  if (!scl_high()) return -1;
  delay_us(5);
  od_low(SCL);
  od_release(SDA);
  return value;
}

static int whoami(uint8_t addr, uint8_t reg) {
  int value;
  i2c_start();
  if (!i2c_write((uint8_t)(addr << 1))) {
    i2c_stop();
    return -1;
  }
  if (!i2c_write(reg)) {
    i2c_stop();
    return -1;
  }
  i2c_start();
  if (!i2c_write((uint8_t)((addr << 1) | 1u))) {
    i2c_stop();
    return -1;
  }
  value = i2c_read(0);
  i2c_stop();
  return value;
}

static int i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
  int ok;
  i2c_start();
  ok = i2c_write((uint8_t)(addr << 1)) && i2c_write(reg) && i2c_write(value);
  i2c_stop();
  return ok;
}

static int i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *dst, int n) {
  int i;
  i2c_start();
  if (!i2c_write((uint8_t)(addr << 1)) || !i2c_write(reg)) {
    i2c_stop();
    return 0;
  }
  i2c_start();
  if (!i2c_write((uint8_t)((addr << 1) | 1u))) {
    i2c_stop();
    return 0;
  }
  for (i = 0; i < n; i++) {
    int got = i2c_read(i + 1 < n);
    if (got < 0) {
      i2c_stop();
      return 0;
    }
    dst[i] = (uint8_t)got;
  }
  i2c_stop();
  return 1;
}

/* ±2 g, high resolution. About 16384 counts is 1 g. */
static void compass_start(void) {
  if (compass_on || acc_id != ACC_ID || mag_id != MAG_ID) return;
  if (!i2c_write_reg(ACC_ADDR, 0x20, 0x57)) return;
  if (!i2c_write_reg(ACC_ADDR, 0x23, 0x88)) return;
  if (!i2c_write_reg(MAG_ADDR, 0x60, 0x00)) return;
  if (!i2c_write_reg(MAG_ADDR, 0x62, 0x10)) return;
  compass_on = 1;
}

static int16_t le16(const uint8_t *p) {
  return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void compass_sample(void) {
  uint8_t acc[6];
  uint8_t mag[6];
  if (!compass_on) return;
  if (!i2c_read_regs(ACC_ADDR, 0x28 | 0x80, acc, 6)) return;
  if (!i2c_read_regs(MAG_ADDR, 0x68 | 0x80, mag, 6)) return;
  acc_x = le16(acc);
  acc_y = le16(acc + 2);
  acc_z = le16(acc + 4);
  mag_x = le16(mag);
  mag_y = le16(mag + 2);
  mag_z = le16(mag + 4);
  compass_seen = 1;
}

static int wait_busy(void) {
  uint32_t start = REG(0xE0001004u);
  uint32_t limit = radio_wait_us * CYCLES_PER_US;
  while (pin_get(D11)) {
    gps_pump();
    if ((REG(0xE0001004u) - start) > limit) {
      radio_wait_us = 2000;
      return 0;
    }
  }
  radio_wait_us = 100000;
  return 1;
}

/* D12 is the SX1262 NRESET. */
static int radio_sync(void) {
  uint8_t hi;
  uint8_t lo;
  if (!radio_released) {
    pin_clr(D12);
    delay_us(200);
    pin_set(D12);
    radio_released = 1;
  }
  if (!wait_busy()) return -1;
  pin_set(A2);
  pin_clr(D10);
  spi_byte(0x1D);
  spi_byte((uint8_t)(SYNC_REG >> 8));
  spi_byte((uint8_t)SYNC_REG);
  spi_byte(0x00);
  hi = spi_byte(0x00);
  lo = spi_byte(0x00);
  pin_set(D10);
  return (hi << 8) | lo;
}

static int wait_busy_us(uint32_t us) {
  uint32_t start = REG(0xE0001004u);
  while (pin_get(D11)) {
    gps_pump();
    if ((REG(0xE0001004u) - start) > us * CYCLES_PER_US) return 0;
  }
  return 1;
}

/* BUSY rises as late as 600 ns after NSS. A sooner read looks idle. */
static int radio_write(const uint8_t *bytes, int n) {
  int i;
  if (!wait_busy_us(20000)) return 0;
  pin_set(A2);
  pin_clr(D10);
  for (i = 0; i < n; i++) spi_byte(bytes[i]);
  pin_set(D10);
  delay_us(2);
  return wait_busy_us(100000);
}

static int radio_read(uint8_t cmd, uint8_t *dst, int n) {
  int i;
  if (!wait_busy_us(20000)) return 0;
  pin_set(A2);
  pin_clr(D10);
  spi_byte(cmd);
  spi_byte(0x00);
  for (i = 0; i < n; i++) dst[i] = spi_byte(0x00);
  pin_set(D10);
  return 1;
}

static void radio_listen(void) {
  uint8_t rx[4];
  rx[0] = 0x82;
  rx[1] = 0xFF;
  rx[2] = 0xFF;
  rx[3] = 0xFF;
  radio_write(rx, 4);
}

static void radio_prove(void);

static int radio_up(void) {
  if (!sx1262_setup(radio_write)) return 0;
  radio_prove();
  radio_listen();
  next_beacon = REG(0xE0001004u) + (REG(0x10000060u) % 50u) * 20000u * CYCLES_PER_US;
  return 1;
}

static void radio_ping(void) {
  uint8_t standby[2];
  uint8_t clear[3];
  uint8_t tx[4];
  uint8_t payload[4];
  uint8_t irq[2];
  uint16_t flags = 0;
  uint32_t start;
  int i;
  standby[0] = 0x80;
  standby[1] = 0x00;
  if (!radio_write(standby, 2)) {
    radio_listen();
    return;
  }
  if (!sx1262_packet_len(radio_write, 4)) {
    radio_listen();
    return;
  }
  payload[0] = 0x50;
  payload[1] = 0x42;
  payload[2] = 0x01;
  payload[3] = (uint8_t)tx_count;
  pin_set(A2);
  pin_clr(D10);
  spi_byte(0x0E);
  spi_byte(0x00);
  for (i = 0; i < 4; i++) spi_byte(payload[i]);
  pin_set(D10);
  delay_us(2);
  if (!wait_busy_us(100000)) {
    sx1262_packet_len(radio_write, 0xFF);
    radio_listen();
    return;
  }
  clear[0] = 0x02;
  clear[1] = 0xFF;
  clear[2] = 0xFF;
  radio_write(clear, 3);
  tx[0] = 0x83;
  tx[1] = 0x00;
  tx[2] = 0x00;
  tx[3] = 0x00;
  if (!radio_write(tx, 4)) {
    sx1262_packet_len(radio_write, 0xFF);
    radio_listen();
    return;
  }
  start = REG(0xE0001004u);
  for (;;) {
    gps_pump();
    if ((REG(0xE0001004u) - start) > 1000u * 1000u * CYCLES_PER_US) break;
    if (!radio_read(0x12, irq, 2)) continue;
    flags = (uint16_t)(((uint16_t)irq[0] << 8) | irq[1]);
    if (flags & 0x0001u) break;
    if (flags & 0x0200u) break;
  }
  radio_write(clear, 3);
  sx1262_packet_len(radio_write, 0xFF);
  radio_listen();
  tx_count++;
  if (flags & 0x0001u) tx_done++;
}

static void radio_catch(void) {
  uint8_t irq[2];
  uint8_t stat[2];
  uint8_t pkt[3];
  uint8_t clear[3];
  uint8_t buf[8];
  uint16_t flags;
  int n;
  int i;
  if (!radio_read(0x12, irq, 2)) return;
  flags = (uint16_t)(((uint16_t)irq[0] << 8) | irq[1]);
  if ((flags & 0x0242u) == 0) return;
  clear[0] = 0x02;
  clear[1] = 0xFF;
  clear[2] = 0xFF;
  if (flags & 0x0040u) {
    crc_count++;
    radio_write(clear, 3);
    radio_listen();
    return;
  }
  if ((flags & 0x0002u) == 0) {
    radio_write(clear, 3);
    radio_listen();
    return;
  }
  if (!radio_read(0x13, stat, 2)) return;
  n = stat[0];
  if (n < 4 || n > 8) {
    radio_write(clear, 3);
    radio_listen();
    return;
  }
  pin_set(A2);
  pin_clr(D10);
  spi_byte(0x1E);
  spi_byte(stat[1]);
  spi_byte(0x00);
  for (i = 0; i < n; i++) buf[i] = spi_byte(0x00);
  pin_set(D10);
  if (!radio_read(0x14, pkt, 3)) {
    radio_write(clear, 3);
    radio_listen();
    return;
  }
  radio_write(clear, 3);
  radio_listen();
  if (buf[0] != 0x50 || buf[1] != 0x42) return;
  rx_count++;
}

static int reg_write(uint16_t addr, uint8_t val) {
  uint8_t b[4];
  b[0] = 0x0D;
  b[1] = (uint8_t)(addr >> 8);
  b[2] = (uint8_t)addr;
  b[3] = val;
  return radio_write(b, 4);
}

static int reg_read(uint16_t addr, uint8_t *val) {
  if (!wait_busy_us(20000)) return 0;
  pin_set(A2);
  pin_clr(D10);
  spi_byte(0x1D);
  spi_byte((uint8_t)(addr >> 8));
  spi_byte((uint8_t)addr);
  spi_byte(0x00);
  *val = spi_byte(0x00);
  pin_set(D10);
  return 1;
}

/* A value other than the reset sync word, so a readback proves the write landed. */
static void radio_prove(void) {
  uint8_t got = 0;
  uint8_t kind = 0;
  write_ok = 0;
  if (reg_write(SYNC_REG, 0xAB) && reg_read(SYNC_REG, &got)) write_ok = got == 0xAB;
  reg_write(SYNC_REG, 0x14);
  if (!reg_read(SYNC_REG, &got) || got != 0x14) reg_write(SYNC_REG, 0x14);
  if (radio_read(0x11, &kind, 1)) packet_type = kind;
}

static int radio_status_byte(void) {
  uint8_t st;
  if (!wait_busy_us(20000)) return -1;
  pin_set(A2);
  pin_clr(D10);
  spi_byte(0xC0);
  st = spi_byte(0x00);
  pin_set(D10);
  return st;
}

static void radio_sample(void) {
  uint8_t b;
  int st;
  int rssi;
  st = radio_status_byte();
  if (st >= 0) {
    chip_mode = (st >> 4) & 7;
    cmd_status = (st >> 1) & 7;
  }
  if (radio_read(0x11, &b, 1)) packet_type = b;
  if (chip_mode != 5 || !radio_read(0x15, &b, 1)) return;
  rssi = -((int)b) / 2;
  inst_rssi = rssi;
  inst_seen = 1;
  if (!peak_seen || rssi > peak_rssi) {
    peak_rssi = rssi;
    peak_seen = 1;
  }
}

static void radio_service(void) {
  uint32_t now;
  if (!radio_ready) return;
  radio_catch();
  radio_sample();
  if (listen_only) return;
  now = REG(0xE0001004u);
  if ((int32_t)(now - next_beacon) < 0) return;
  next_beacon = now + 1000u * 1000u * CYCLES_PER_US;
  radio_ping();
}

static void probe(void) {
  acc_id = whoami(ACC_ADDR, ACC_WHO);
  mag_id = whoami(MAG_ADDR, MAG_WHO);
  compass_start();
  compass_sample();
  if (radio_id < 0) radio_id = radio_sync();
  if (radio_id == SYNC_ID && !radio_ready) radio_ready = radio_up();
}

static void gps_start(void) {
  pin_cnf(RX, 0);
  REG(0x40002500u) = 0;
  REG(0x40002514u) = RX.pin;
  REG(0x4000250Cu) = 0xFFFFFFFFu;
  REG(0x40002510u) = 0xFFFFFFFFu;
  REG(0x40002508u) = 0xFFFFFFFFu;
  REG(0x40002524u) = 0x00275000u;
  REG(0x4000256Cu) = 0;
  REG(0x40002534u) = (uint32_t)(uintptr_t)gps_buf;
  REG(0x40002538u) = sizeof gps_buf;
  REG(0x40002110u) = 0;
  REG(0x40002124u) = 0;
  REG(0x40002480u) = 0xFu;
  REG(0x40002500u) = 8u;
  REG(0x40002000u) = 1u;
  gps_run = 1;
}

static char *copy(char *dst, const char *src) {
  while (*src) *dst++ = *src++;
  return dst;
}

static char *hex_byte(char *dst, unsigned value) {
  static const char digits[] = "0123456789ABCDEF";
  *dst++ = digits[(value >> 4) & 15u];
  *dst++ = digits[value & 15u];
  return dst;
}

static char *hex_word(char *dst, unsigned value) {
  dst = hex_byte(dst, value >> 8);
  return hex_byte(dst, value);
}

static char *dec_pad(char *dst, unsigned value, int width) {
  char tmp[10];
  int n = 0;
  do {
    unsigned q = 0;
    unsigned r = value;
    while (r >= 10u) {
      r -= 10u;
      q++;
    }
    tmp[n++] = (char)('0' + r);
    value = q;
  }   while (value && n < 10);
  if (n > width) n = width;
  while (n < width) tmp[n++] = '0';
  while (n) *dst++ = tmp[--n];
  return dst;
}

static char *dbm(char *dst, int seen, int db) {
  if (!seen) return copy(dst, "----");
  *dst++ = '-';
  return dec_pad(dst, (unsigned)(db < 0 ? -db : db), 3);
}

static void chip_line(char *dst, const char *name, int id, int expect) {
  char *p = copy(dst, name);
  p = copy(p, " ");
  if (id < 0) p = copy(p, "--");
  else {
    p = hex_byte(p, (unsigned)id);
    p = copy(p, id == expect ? " OK" : " NO");
  }
  *p = 0;
}

static int nmea_field(const char *line, int index, char *buf, int cap) {
  const char *p = line;
  int n = 0;
  if (*p == '$') p++;
  while (*p && *p != '*') {
    const char *start = p;
    int len;
    while (*p && *p != ',' && *p != '*') p++;
    if (n == index) {
      len = (int)(p - start);
      if (len >= cap) len = cap - 1;
      memcpy(buf, start, (unsigned)len);
      buf[len] = 0;
      return 1;
    }
    n++;
    if (*p == ',') p++;
  }
  if (n == index && cap) {
    buf[0] = 0;
    return 1;
  }
  return 0;
}

static int nmea_kind(const char *type, const char *kind) {
  int n = 0;
  while (type[n]) n++;
  if (n < 3) return 0;
  return type[n - 3] == kind[0] && type[n - 2] == kind[1] && type[n - 1] == kind[2];
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int nmea_checksum(const char *line) {
  const char *p = line;
  unsigned cs = 0;
  int hi;
  int lo;
  if (*p == '$') p++;
  for (; *p && *p != '*'; p++) cs ^= (unsigned char)*p;
  if (*p != '*') return 1;
  hi = hex_nibble(p[1]);
  lo = hex_nibble(p[2]);
  if (hi < 0 || lo < 0) return 0;
  return cs == (unsigned)((hi << 4) | lo);
}

static void copy_field(char *dst, int cap, const char *src) {
  int i = 0;
  if (!src) src = "";
  while (src[i] && i < cap - 1) {
    dst[i] = src[i];
    i++;
  }
  dst[i] = 0;
}

static void format_hms(const char *tim) {
  if (tim[0] < '0' || tim[1] < '0' || tim[2] < '0' || tim[3] < '0' || tim[4] < '0' || tim[5] < '0') {
    gps_time[0] = 0;
    return;
  }
  gps_time[0] = tim[0];
  gps_time[1] = tim[1];
  gps_time[2] = ':';
  gps_time[3] = tim[2];
  gps_time[4] = tim[3];
  gps_time[5] = ':';
  gps_time[6] = tim[4];
  gps_time[7] = tim[5];
  gps_time[8] = 0;
}

static void take_position(const char *lat, const char *ns, const char *lon, const char *ew) {
  if (lat[0]) {
    copy_field(gps_lat, (int)sizeof gps_lat, lat);
    gps_ns = ns[0];
  }
  if (lon[0]) {
    copy_field(gps_lon, (int)sizeof gps_lon, lon);
    gps_ew = ew[0];
  }
}

static void gps_take_line(const char *line) {
  char type[8];
  char tim[16];
  char lat[16];
  char ns[4];
  char lon[16];
  char ew[4];
  char extra[8];
  if (!nmea_checksum(line)) return;
  if (!nmea_field(line, 0, type, (int)sizeof type)) return;
  if (nmea_kind(type, "RMC")) {
    if (!nmea_field(line, 1, tim, (int)sizeof tim)) return;
    if (!nmea_field(line, 2, extra, (int)sizeof extra)) return;
    format_hms(tim);
    gps_status = extra[0] == 'A' || extra[0] == 'V' ? extra[0] : 0;
    if (nmea_field(line, 3, lat, (int)sizeof lat) && nmea_field(line, 4, ns, (int)sizeof ns) &&
        nmea_field(line, 5, lon, (int)sizeof lon) && nmea_field(line, 6, ew, (int)sizeof ew)) {
      take_position(lat, ns, lon, ew);
    }
    return;
  }
  if (!nmea_kind(type, "GGA")) return;
  if (nmea_field(line, 1, tim, (int)sizeof tim) && !gps_time[0]) format_hms(tim);
  if (nmea_field(line, 7, extra, (int)sizeof extra)) {
    int sats = 0;
    int i = 0;
    while (extra[i] >= '0' && extra[i] <= '9') {
      sats = sats * 10 + (extra[i] - '0');
      i++;
    }
    if (i) gps_sats = sats;
  }
  if (nmea_field(line, 8, extra, (int)sizeof extra) && extra[0]) copy_field(gps_hdop, (int)sizeof gps_hdop, extra);
  if (!gps_lat[0] && nmea_field(line, 2, lat, (int)sizeof lat) && nmea_field(line, 3, ns, (int)sizeof ns) &&
      nmea_field(line, 4, lon, (int)sizeof lon) && nmea_field(line, 5, ew, (int)sizeof ew)) {
    take_position(lat, ns, lon, ew);
  }
}

static void gps_take_byte(uint8_t byte) {
  static char line[96];
  static int len;
  static int skip;
  gps_bytes++;
  if (byte == '$') gps_dollars++;
  if (byte == '\r') return;
  if (byte == '\n') {
    if (!skip && len > 0) {
      line[len] = 0;
      gps_take_line(line);
    }
    len = 0;
    skip = 0;
    return;
  }
  if (skip || len >= (int)sizeof line - 1) {
    skip = 1;
    return;
  }
  line[len++] = (char)byte;
}

static int atan2_deg(int y, int x) {
  int neg = 0;
  int angle;
  int den;
  if (x == 0 && y == 0) return 0;
  if (y < 0) {
    neg = 1;
    y = -y;
  }
  if (x >= 0) {
    den = x + y;
    if (den == 0) den = 1;
    angle = 45 - ((x - y) * 45) / den;
  } else {
    den = y - x;
    if (den == 0) den = 1;
    angle = 135 - ((x + y) * 45) / den;
  }
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  return neg ? -angle : angle;
}

/* +X north reads about −150, south about −400. +Y north about +60, south about −150.
   Aimed east sat about 21° south of due east. */
enum { MAG_X_BIAS = -275, MAG_Y_BIAS = -45, HEADING_SHIFT = -21 };

static const char *facing(int x, int y) {
  static const char *name[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int deg = atan2_deg(y, x);
  if (deg < 0) deg += 360;
  deg = (deg + 360 + HEADING_SHIFT) % 360;
  return name[((deg + 22) / 45) & 7];
}

static char *sdec(char *dst, int value) {
  unsigned mag;
  if (value < 0) {
    *dst++ = '-';
    mag = (unsigned)(-value);
  } else {
    *dst++ = '+';
    mag = (unsigned)value;
  }
  return dec_pad(dst, mag, 5);
}

static void axis_line(char *row, const char *name, int x, int y, int z) {
  char *p = copy(row, name);
  if (!compass_seen) {
    p = copy(p, " ----");
    *p = 0;
    return;
  }
  p = copy(p, " ");
  p = sdec(p, x);
  p = copy(p, " ");
  p = sdec(p, y);
  p = copy(p, " ");
  p = sdec(p, z);
  *p = 0;
}

static void draw_gps(void) {
  char row[32];
  char *p;
  p = copy(row, "FIX ");
  *p++ = gps_status ? (char)gps_status : '-';
  p = copy(p, " SAT ");
  if (gps_sats < 0) p = copy(p, "--");
  else p = dec_pad(p, (unsigned)gps_sats, 2);
  p = copy(p, " H ");
  p = copy(p, gps_hdop[0] ? gps_hdop : "----");
  *p = 0;
  text(4, 2, row);
  p = copy(row, gps_lat[0] ? gps_lat : "----");
  if (gps_ns) {
    p = copy(p, " ");
    *p++ = gps_ns;
  }
  *p = 0;
  text(4, 10, row);
  p = copy(row, gps_lon[0] ? gps_lon : "----");
  if (gps_ew) {
    p = copy(p, " ");
    *p++ = gps_ew;
  }
  *p = 0;
  text(4, 18, row);
  p = copy(row, "UTC ");
  p = copy(p, gps_time[0] ? gps_time : "----");
  *p = 0;
  text(4, 26, row);
  axis_line(row, "ACC", acc_x, acc_y, acc_z);
  text(4, 34, row);
  axis_line(row, "MAG", mag_x, mag_y, mag_z);
  if (compass_seen && (mag_x || mag_y)) {
    p = row;
    while (*p) p++;
    *p++ = ' ';
    p = copy(p, facing(mag_y - MAG_Y_BIAS, mag_x - MAG_X_BIAS));
    *p = 0;
  }
  text(4, 42, row);
  p = copy(row, "NMEA ");
  p = dec_pad(p, gps_bytes > 999999u ? 999999u : gps_bytes, 6);
  p = copy(p, " $");
  p = dec_pad(p, gps_dollars > 9999u ? 9999u : gps_dollars, 4);
  *p = 0;
  text(4, 50, row);
}

static void draw(void) {
  char row[32];
  char *p;
  int x;
  int y;
  for (x = 0; x < 1024; x++) fb[x] = 0;
  for (x = 0; x < 128; x++) {
    px(x, 0);
    px(x, 63);
  }
  for (y = 0; y < 64; y++) {
    px(0, y);
    px(127, y);
  }
  if (gps_page) {
    draw_gps();
    return;
  }
  p = copy(row, "BRINGUP EV");
  p = hex_byte(p, contrast);
  p = copy(p, " R");
  *p++ = (char)('0' + (ratio & 7u));
  *p = 0;
  text(4, 2, row);
  text(4, 10, "D6 D9 A3 A4");
  row[0] = ' ';
  row[1] = button_closed(D6, 1u) ? '0' : '1';
  row[2] = ' ';
  row[3] = ' ';
  row[4] = button_closed(D9, 2u) ? '0' : '1';
  row[5] = ' ';
  row[6] = ' ';
  row[7] = button_closed(A3, 4u) ? '0' : '1';
  row[8] = ' ';
  row[9] = ' ';
  row[10] = button_closed(A4, 8u) ? '0' : '1';
  row[11] = 0;
  closed_bits = 0;
  text(4, 18, row);
  chip_line(row, "ACC", acc_id, ACC_ID);
  text(4, 26, row);
  chip_line(row, "MAG", mag_id, MAG_ID);
  text(4, 34, row);
  if (!radio_ready) {
    p = copy(row, "RF ");
    if (radio_id < 0) p = copy(p, "--");
    else {
      p = hex_word(p, (unsigned)radio_id);
      p = copy(p, radio_id == SYNC_ID ? " OK" : " NO");
    }
  } else {
    unsigned shown_tx = tx_count > 999u ? 999u : tx_count;
    unsigned shown_done = tx_done > 999u ? 999u : tx_done;
    unsigned shown_rx = rx_count > 999u ? 999u : rx_count;
    unsigned shown_crc = crc_count > 999u ? 999u : crc_count;
    if (listen_only) p = copy(row, "LIS");
    else {
      p = copy(row, "T");
      p = dec_pad(p, shown_tx, 3);
    }
    p = copy(p, " D");
    p = dec_pad(p, shown_done, 3);
    p = copy(p, " R");
    p = dec_pad(p, shown_rx, 3);
    p = copy(p, " C");
    p = dec_pad(p, shown_crc, 3);
  }
  *p = 0;
  text(4, 42, row);
  if (!radio_ready) {
    p = copy(row, "GPS ");
    p = dec_pad(p, gps_bytes, 6);
    p = copy(p, " $");
    p = dec_pad(p, gps_dollars, 4);
    if (gps_dollars) p = copy(p, " OK");
  } else {
    p = copy(row, "M");
    if (chip_mode < 0) p = copy(p, "-");
    else *p++ = (char)('0' + (chip_mode & 7));
    p = copy(p, " S");
    if (cmd_status < 0) p = copy(p, "-");
    else *p++ = (char)('0' + (cmd_status & 7));
    p = copy(p, " P");
    if (packet_type < 0) p = copy(p, "-");
    else *p++ = (char)('0' + (packet_type & 7));
    p = copy(p, write_ok ? " WOK I" : " WNO I");
    p = dbm(p, inst_seen, inst_rssi);
    p = copy(p, " ");
    p = dbm(p, peak_seen, peak_rssi);
  }
  *p = 0;
  text(4, 50, row);
}

int main(void) {
  uint32_t last;
  int led = 0;
  clocks();
  gpio_init();
  blink();
  lcd_init();
  draw();
  lcd_flush();
  gps_start();
  last = REG(0xE0001004u);
  for (;;) {
    probe();
    draw();
    lcd_flush();
    led = !led;
    drive(LED_RED, led);
    while ((REG(0xE0001004u) - last) < 200u * 1000u * CYCLES_PER_US) {
      gps_pump();
      if (contrast_dirty) lcd_contrast();
      if ((REG(0xE0001004u) - last_radio) >= 5000u * CYCLES_PER_US) {
        last_radio = REG(0xE0001004u);
        radio_service();
      }
    }
    last = REG(0xE0001004u);
  }
}
