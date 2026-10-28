#include "board.h"

#include "beeper_hal.h"
#include "sx1262_setup.h"

#include <string.h>

#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

enum { CNF_OUT = 0x1u, CNF_IN_UP = 0xCu, CNF_OD = 6u << 8 };

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
static const Pin A6 = {0, 29};

enum {
  LCD_RATIO = 0x22,
  LCD_EV = 0x34,
  SYNC_REG = 0x0740,
  NVM_MAGIC = 0x50424E76u,
  PAGE = 4096u
};

extern uint8_t __nvm_start[];
extern uint8_t __nvm_end[];

static int sample_on;
static int gps_run;
static int radio_ok;
static int radio_released;
static int compass_ok;
static uint32_t vibe_until;
static uint32_t rtc_epoch;
static uint8_t gps_buf[256] __attribute__((aligned(4)));
static uint8_t enc_prev;
static int enc_acc;
static encoder_event_t enc_q[8];
static uint8_t enc_h, enc_t;
static int raw_a3, stable_a3, raw_a4, stable_a4;
static uint32_t a3_since, a4_since;
static int back_edge;

static void board_sample(void);
static int rng_byte(void);

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

static volatile uint32_t *gpio(Pin p, uint32_t offset) {
  uint32_t base = p.port ? 0x50000300u : 0x50000000u;
  return (volatile uint32_t *)(uintptr_t)(base + offset);
}

static void pin_set(Pin p) { *gpio(p, 0x508) = 1u << p.pin; }
static void pin_clr(Pin p) { *gpio(p, 0x50C) = 1u << p.pin; }
static int pin_get(Pin p) { return (int)((*gpio(p, 0x510) >> p.pin) & 1u); }
static void pin_cnf(Pin p, uint32_t value) { *gpio(p, 0x700u + 4u * p.pin) = value; }

static void drive(Pin p, int high) {
  if (high) pin_set(p);
  else pin_clr(p);
  pin_cnf(p, CNF_OUT);
}

static void pullup(Pin p) { pin_cnf(p, CNF_IN_UP); }

void board_backlight(bool on) {
  if (on) pullup(D13);
  else drive(D13, 0);
}
static void od_low(Pin p) {
  pin_clr(p);
  pin_cnf(p, CNF_OUT | CNF_OD);
}
static void od_release(Pin p) { pin_cnf(p, CNF_OD); }

static void dsb(void) { __asm volatile("dsb" ::: "memory"); }

static uint32_t cycles(void) { return REG(0xE0001004u); }

static void delay_us(uint32_t us) {
  uint32_t start = cycles();
  while ((cycles() - start) < us * 64u) board_sample();
}

static void delay_ms(uint32_t ms) {
  while (ms--) delay_us(1000);
}

static void gps_pump(void) {
  uint32_t n;
  uint32_t i;
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
  for (i = 0; i < n; i++) beeper_gps_push_byte(gps_buf[i]);
  REG(0x40002534u) = (uint32_t)(uintptr_t)gps_buf;
  REG(0x40002538u) = sizeof gps_buf;
  REG(0x40002000u) = 1u;
}

static uint32_t rtc_ms(void) {
  uint32_t now;
  if (REG(0x4000B104u)) {
    REG(0x4000B104u) = 0;
    (void)REG(0x4000B104u);
    rtc_epoch += 1u << 24;
  }
  now = (REG(0x4000B504u) & 0xFFFFFFu) + rtc_epoch;
  return (uint32_t)(((uint64_t)now * 1000ull) / 32768ull);
}

static void enc_push(encoder_event_t ev) {
  uint8_t next = (uint8_t)((enc_h + 1u) & 7u);
  if (next == enc_t) return;
  enc_q[enc_h] = ev;
  enc_h = next;
}

static void note_edge(int *raw, int *stable, uint32_t *since, int down, int is_back) {
  uint32_t now = rtc_ms();
  if (down != *raw) {
    *raw = down;
    *since = now;
  }
  if ((uint32_t)(now - *since) < 20u || *stable == *raw) return;
  *stable = *raw;
  if (is_back) {
    if (down) back_edge = 1;
    return;
  }
  enc_push(down ? ENC_PRESS : ENC_RELEASE);
}

static void note_encoder(void) {
  static const int8_t step_for[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t now = (uint8_t)((pin_get(D6) ? 2u : 0u) | (pin_get(D9) ? 1u : 0u));
  int step;
  if (now == enc_prev) return;
  step = step_for[(enc_prev << 2) | now];
  enc_prev = now;
  if (!step) return;
  enc_acc += step;
  if (enc_acc >= 4) {
    enc_acc = 0;
    enc_push(ENC_CW);
  } else if (enc_acc <= -4) {
    enc_acc = 0;
    enc_push(ENC_CCW);
  }
}

static void board_sample(void) {
  gps_pump();
  if (!sample_on) return;
  note_encoder();
  note_edge(&raw_a3, &stable_a3, &a3_since, !pin_get(A3), 0);
  note_edge(&raw_a4, &stable_a4, &a4_since, !pin_get(A4), 1);
}

static void clocks(void) {
  uint32_t i;
  REG(0xE000EDFCu) |= 1u << 24;
  REG(0xE0001004u) = 0;
  REG(0xE0001000u) |= 1u;
  REG(0x40000100u) = 0;
  REG(0x40000000u) = 1u;
  for (i = 0; i < 5000u && !REG(0x40000100u); i++) delay_us(1);
  REG(0x40000518u) = 1u;
  REG(0x40000104u) = 0;
  REG(0x40000008u) = 1u;
  for (i = 0; i < 100000u && !REG(0x40000104u); i++) delay_us(1);
  if (!REG(0x40000104u)) {
    REG(0x40000518u) = 0;
    REG(0x40000104u) = 0;
    REG(0x40000008u) = 1u;
    for (i = 0; i < 10000u && !REG(0x40000104u); i++) delay_us(1);
  }
  REG(0x4000B508u) = 0;
  REG(0x4000B000u) = 1u;
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
  pin_cnf(A6, 1u << 1); /* Divider output. The digital buffer stays off. */
  raw_a3 = stable_a3 = !pin_get(A3);
  raw_a4 = stable_a4 = !pin_get(A4);
  a3_since = a4_since = rtc_ms();
  enc_prev = (uint8_t)((pin_get(D6) ? 2u : 0u) | (pin_get(D9) ? 1u : 0u));
  sample_on = 1;
}

static void rng_start(void) {
  REG(0x4000D504u) = 1u;
  REG(0x4000D100u) = 0;
  REG(0x4000D000u) = 1u;
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
  lcd_cmd(LCD_RATIO);
  lcd_cmd(0x81);
  lcd_cmd(LCD_EV);
  lcd_cmd(0x2F);
  delay_ms(50);
  lcd_cmd(0xAF);
}

void board_flush(const uint8_t *fb) {
  int page;
  int col;
  if (!fb) return;
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
    int value = i2c_read(i + 1 < n);
    if (value < 0) {
      i2c_stop();
      return 0;
    }
    dst[i] = (uint8_t)value;
  }
  i2c_stop();
  return 1;
}

static int whoami(uint8_t addr, uint8_t reg) {
  uint8_t value = 0;
  if (!i2c_read_regs(addr, reg, &value, 1)) return -1;
  return value;
}

static void compass_init(void) {
  if (whoami(0x19, 0x0F) != 0x33 || whoami(0x1E, 0x4F) != 0x40) return;
  if (!i2c_write_reg(0x19, 0x20, 0x57)) return;
  if (!i2c_write_reg(0x19, 0x23, 0x88)) return;
  if (!i2c_write_reg(0x1E, 0x60, 0x00)) return;
  if (!i2c_write_reg(0x1E, 0x62, 0x10)) return;
  compass_ok = 1;
}

static int isqrt(int64_t n) {
  int64_t x;
  int64_t y;
  if (n <= 0) return 0;
  x = n;
  y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return (int)x;
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

static int16_t le16(const uint8_t *p) {
  return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

bool board_heading(heading_sample_t *out) {
  uint8_t acc[6];
  uint8_t mag[6];
  int ax, ay, az, mx, my;
  int horiz;
  int tilt;
  int deg;
  int64_t mag2;
  int64_t one_g = 16384;
  if (!out) return false;
  memset(out, 0, sizeof *out);
  if (!compass_ok) return false;
  if (!i2c_read_regs(0x19, 0x28 | 0x80, acc, 6)) return false;
  if (!i2c_read_regs(0x1E, 0x68 | 0x80, mag, 6)) return false;
  ax = le16(acc);
  ay = le16(acc + 2);
  az = le16(acc + 4);
  mx = le16(mag);
  my = le16(mag + 2);
  horiz = isqrt((int64_t)ax * ax + (int64_t)ay * ay);
  tilt = atan2_deg(horiz, az < 0 ? -az : az);
  mag2 = (int64_t)ax * ax + (int64_t)ay * ay + (int64_t)az * az;
  deg = atan2_deg(my, mx);
  if (deg < 0) deg += 360;
  out->tilt_deg = tilt;
  out->still = mag2 > (one_g * 85 / 100) * (one_g * 85 / 100) &&
               mag2 < (one_g * 115 / 100) * (one_g * 115 / 100);
  out->motor = vibe_until != 0 && (int32_t)(vibe_until - rtc_ms()) > 0;
  out->heading_deg = deg;
  return true;
}

static int wait_busy(uint32_t us) {
  uint32_t start = cycles();
  while (pin_get(D11)) {
    board_sample();
    if ((cycles() - start) > us * 64u) return 0;
  }
  return 1;
}

static void radio_begin(void) {
  pin_set(A2);
  pin_clr(D10);
}

static void radio_end(void) { pin_set(D10); }

/* BUSY rises as late as 600 ns after NSS. A sooner read looks idle. */
static int radio_written(void) {
  radio_end();
  delay_us(2);
  return wait_busy(100000);
}

static int radio_write(const uint8_t *bytes, int n) {
  int i;
  if (!wait_busy(20000)) return 0;
  radio_begin();
  for (i = 0; i < n; i++) spi_byte(bytes[i]);
  return radio_written();
}

static int radio_read(uint8_t cmd, uint8_t *dst, int n) {
  int i;
  if (!wait_busy(20000)) return 0;
  radio_begin();
  spi_byte(cmd);
  spi_byte(0x00);
  for (i = 0; i < n; i++) dst[i] = spi_byte(0x00);
  radio_end();
  return 1;
}

/* D12 is the SX1262 NRESET. */
static void radio_init(void) {
  uint8_t sync[2];
  uint8_t rx[4];
  if (!radio_released) {
    pin_clr(D12);
    delay_us(200);
    pin_set(D12);
    radio_released = 1;
  }
  if (!wait_busy(100000)) return;
  if (!sx1262_setup(radio_write)) return;
  radio_begin();
  spi_byte(0x1D);
  spi_byte((uint8_t)(SYNC_REG >> 8));
  spi_byte((uint8_t)SYNC_REG);
  spi_byte(0x00);
  sync[0] = spi_byte(0x00);
  sync[1] = spi_byte(0x00);
  radio_end();
  (void)sync;
  rx[0] = 0x82;
  rx[1] = 0xFF;
  rx[2] = 0xFF;
  rx[3] = 0xFF;
  if (!radio_write(rx, 4)) return;
  radio_ok = 1;
}

static void radio_listen(void) {
  uint8_t rx[4] = {0x82, 0xFF, 0xFF, 0xFF};
  radio_write(rx, 4);
}

void board_radio_send(const uint8_t *bytes, size_t len, bool whisper) {
  uint8_t standby[2];
  uint8_t txp[3];
  uint8_t clear[3];
  uint8_t tx[4];
  uint8_t irq[2];
  uint16_t flags;
  uint32_t start;
  int i;
  if (!radio_ok || !bytes || len == 0) return;
  if (len > RAW_FRAME_MAX) len = RAW_FRAME_MAX;
  standby[0] = 0x80;
  standby[1] = 0x00;
  if (!radio_write(standby, 2)) {
    radio_listen();
    return;
  }
  txp[0] = 0x8E;
  txp[1] = whisper ? 0xF7 : 14;
  txp[2] = 0x04;
  if (!radio_write(txp, 3)) {
    radio_listen();
    return;
  }
  if (!sx1262_packet_len(radio_write, (uint8_t)len)) {
    sx1262_packet_len(radio_write, 0xFF);
    radio_listen();
    return;
  }
  radio_begin();
  spi_byte(0x0E);
  spi_byte(0x00);
  for (i = 0; i < (int)len; i++) spi_byte(bytes[i]);
  if (!radio_written()) {
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
  start = rtc_ms();
  for (;;) {
    board_sample();
    if ((uint32_t)(rtc_ms() - start) > 1000u) break;
    if (!radio_read(0x12, irq, 2)) continue;
    flags = (uint16_t)(((uint16_t)irq[0] << 8) | irq[1]);
    if (flags & 0x0001u) break;
    if (flags & 0x0200u) break;
  }
  radio_write(clear, 3);
  sx1262_packet_len(radio_write, 0xFF);
  radio_listen();
}

bool board_radio_poll(raw_frame_t *out) {
  uint8_t irq[2];
  uint8_t stat[2];
  uint8_t pkt[3];
  uint8_t clear[3];
  uint16_t flags;
  int n;
  int i;
  if (!out || !radio_ok) return false;
  if (!radio_read(0x12, irq, 2)) return false;
  flags = (uint16_t)(((uint16_t)irq[0] << 8) | irq[1]);
  if ((flags & 0x0242u) == 0) return false;
  clear[0] = 0x02;
  clear[1] = 0xFF;
  clear[2] = 0xFF;
  if (flags & 0x0040u) {
    radio_write(clear, 3);
    radio_listen();
    return false;
  }
  if ((flags & 0x0002u) == 0) {
    radio_write(clear, 3);
    radio_listen();
    return false;
  }
  if (!radio_read(0x13, stat, 2)) return false;
  n = stat[0];
  if (n <= 0 || n > RAW_FRAME_MAX) {
    radio_write(clear, 3);
    radio_listen();
    return false;
  }
  if (!wait_busy(20000)) return false;
  radio_begin();
  spi_byte(0x1E);
  spi_byte(stat[1]);
  spi_byte(0x00);
  for (i = 0; i < n; i++) out->bytes[i] = spi_byte(0x00);
  radio_end();
  if (!radio_read(0x14, pkt, 3)) {
    radio_write(clear, 3);
    radio_listen();
    return false;
  }
  radio_write(clear, 3);
  radio_listen();
  out->len = (uint8_t)n;
  out->rssi = (int8_t)(-((int)pkt[0]) / 2);
  out->received_ms = rtc_ms();
  return true;
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

static void nvmc_wait(void) {
  while (REG(0x4001E400u) == 0u) board_sample();
  dsb();
}

static void nvmc_erase(uint32_t addr) {
  nvmc_wait();
  REG(0x4001E504u) = 2u;
  dsb();
  REG(0x4001E508u) = addr;
  dsb();
  nvmc_wait();
  REG(0x4001E504u) = 0u;
  dsb();
}

static void nvmc_word(uint32_t addr, uint32_t value) {
  nvmc_wait();
  REG(0x4001E504u) = 1u;
  dsb();
  *(volatile uint32_t *)(uintptr_t)addr = value;
  dsb();
  nvmc_wait();
  REG(0x4001E504u) = 0u;
  dsb();
}

static void nvmc_bytes(uint32_t addr, const uint8_t *src, uint32_t len) {
  uint32_t i = 0;
  while (i < len) {
    uint32_t word = 0xFFFFFFFFu;
    int b;
    for (b = 0; b < 4 && i + (uint32_t)b < len; b++) {
      word &= ~(0xFFu << (8 * b));
      word |= (uint32_t)src[i + (uint32_t)b] << (8 * b);
    }
    nvmc_word(addr + i, word);
    i += 4u;
  }
}

static uint32_t nvm_base(void) { return (uint32_t)(uintptr_t)__nvm_start; }

static uint32_t nvm_bytes(void) {
  return (uint32_t)((uintptr_t)__nvm_end - (uintptr_t)__nvm_start);
}

size_t board_nvm_load(uint8_t *out, size_t max) {
  uint32_t base = nvm_base();
  uint32_t magic = *(volatile uint32_t *)(uintptr_t)base;
  uint32_t len = *(volatile uint32_t *)(uintptr_t)(base + 4u);
  if (magic != NVM_MAGIC || len > NVM_BYTES || len > max) return 0;
  if (len) memcpy(out, (const void *)(uintptr_t)(base + 8u), len);
  return len;
}

bool board_nvm_save(const uint8_t *bytes, size_t len) {
  uint32_t base;
  uint32_t span;
  uint32_t pages;
  uint32_t page;
  uint32_t cap;
  if (len > NVM_BYTES) return false;
  if (len && !bytes) return false;
  base = nvm_base();
  cap = nvm_bytes();
  span = 8u + (uint32_t)len;
  if (span > cap) return false;
  pages = (span + PAGE - 1u) / PAGE;
  for (page = 0; page < pages; page++) nvmc_erase(base + page * PAGE);
  if (len) nvmc_bytes(base + 8u, bytes, (uint32_t)len);
  nvmc_word(base + 4u, (uint32_t)len);
  nvmc_word(base, len ? NVM_MAGIC : 0xFFFFFFFFu);
  return true;
}

static int rng_byte(void) {
  uint32_t start = cycles();
  while (!REG(0x4000D100u)) {
    if ((cycles() - start) > 64u * 1000u) return (int)(cycles() & 0xFFu);
  }
  REG(0x4000D100u) = 0;
  return (int)(REG(0x4000D508u) & 0xFFu);
}

void board_init(void) {
  clocks();
  gpio_init();
  rng_start();
  lcd_init();
  compass_init();
  radio_init();
  gps_start();
}

void board_pump(void) { board_sample(); }

encoder_event_t board_encoder_poll(void) {
  encoder_event_t ev;
  if (enc_t == enc_h) return ENC_NONE;
  ev = enc_q[enc_t];
  enc_t = (uint8_t)((enc_t + 1u) & 7u);
  return ev;
}

bool board_back_poll(void) {
  if (!back_edge) return false;
  back_edge = 0;
  return true;
}

void board_random_fill(uint8_t *buf, size_t len) {
  size_t i;
  for (i = 0; i < len; i++) buf[i] = (uint8_t)rng_byte();
}

/* P0.29 is half the cell. 1200 mAh pack, 200 mA charge. */
static uint8_t lipo_pct(int mv) {
  static const int v[] = {3400, 3650, 3730, 3770, 3790, 3800, 3820, 3870, 3920, 4000, 4110, 4150, 4200};
  static const int p[] = {0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 100};
  int i;
  if (mv <= v[0]) return 0;
  if (mv >= v[12]) return 100;
  for (i = 0; i < 12; i++) {
    if (mv <= v[i + 1]) {
      int span = v[i + 1] - v[i];
      return (uint8_t)(p[i] + (mv - v[i]) * (p[i + 1] - p[i]) / span);
    }
  }
  return 100;
}

static int wait_reg(volatile uint32_t *event, uint32_t us) {
  uint32_t start = cycles();
  while (!*event) {
    if ((cycles() - start) > us * 64u) return 0;
  }
  return 1;
}

static int cell_mv(void) {
  static volatile int16_t sample;
  static int calibrated;
  int raw;
  REG(0x40007500u) = 1u;
  REG(0x40007510u) = 6u;
  REG(0x40007514u) = 0u;
  REG(0x40007518u) = 5u << 16;
  REG(0x400075F0u) = 2u;
  REG(0x4000762Cu) = (uint32_t)(uintptr_t)&sample;
  REG(0x40007630u) = 1u;
  if (!calibrated) {
    REG(0x40007110u) = 0u;
    REG(0x4000700Cu) = 1u;
    if (!wait_reg((volatile uint32_t *)0x40007110u, 10000u)) {
      REG(0x40007500u) = 0u;
      return -1;
    }
    calibrated = 1;
  }
  REG(0x40007100u) = 0u;
  REG(0x40007000u) = 1u;
  if (!wait_reg((volatile uint32_t *)0x40007100u, 1000u)) {
    REG(0x40007500u) = 0u;
    return -1;
  }
  REG(0x40007104u) = 0u;
  REG(0x40007004u) = 1u;
  if (!wait_reg((volatile uint32_t *)0x40007104u, 1000u)) {
    REG(0x40007114u) = 0u;
    REG(0x40007008u) = 1u;
    wait_reg((volatile uint32_t *)0x40007114u, 1000u);
    REG(0x40007500u) = 0u;
    return -1;
  }
  dsb();
  raw = sample;
  REG(0x40007114u) = 0u;
  REG(0x40007008u) = 1u;
  wait_reg((volatile uint32_t *)0x40007114u, 1000u);
  REG(0x40007500u) = 0u;
  if (raw < 0) raw = 0;
  return raw * 3600 / 4096 * 2;
}

bool board_battery(battery_t *out) {
  static battery_t last;
  static uint32_t at;
  static int have;
  int mv;
  uint32_t now;
  int usb;
  if (!out) return false;
  now = rtc_ms();
  if (have && (uint32_t)(now - at) < 1000u) {
    *out = last;
    return true;
  }
  mv = cell_mv();
  if (mv < 0) return false;
  memset(out, 0, sizeof *out);
  if (mv >= 3000) out->pct = lipo_pct(mv);
  usb = (REG(0x40000438u) & 1u) != 0;
  if (usb && out->pct < 100 && mv >= 3000) {
    out->charging = true;
    out->minutes = (uint16_t)((100 - out->pct) * 36 / 10);
    if (out->minutes == 0) out->minutes = 1;
  }
  last = *out;
  at = now;
  have = 1;
  return true;
}

uint32_t board_now_ms(void) {
  board_sample();
  return rtc_ms();
}

void board_gps_power(bool on) { drive(D5, on); }

void board_vibe(uint16_t ms) {
  /* The deadline only tells heading the motor was requested. */
  vibe_until = ms ? rtc_ms() + ms : 0;
}
