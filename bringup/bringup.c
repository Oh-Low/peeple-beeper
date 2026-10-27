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
static uint32_t radio_wait_us = 100000;
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

static void note_buttons(void) {
  if (!pin_get(D6)) closed_bits |= 1u;
  if (!pin_get(D9)) closed_bits |= 2u;
  if (!pin_get(A3)) closed_bits |= 4u;
  if (!pin_get(A4)) closed_bits |= 8u;
  note_encoder();
}

static int button_closed(Pin p, unsigned bit) {
  return !pin_get(p) || (closed_bits & bit);
}

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
  for (i = 0; i < n; i++) {
    gps_bytes++;
    if (gps_buf[i] == '$') gps_dollars++;
  }
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

/* One low pulse on D12, then NRESET stays high. This image never transmits. */
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

static void probe(void) {
  acc_id = whoami(ACC_ADDR, ACC_WHO);
  mag_id = whoami(MAG_ADDR, MAG_WHO);
  radio_id = radio_sync();
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
  p = copy(row, "RF ");
  if (radio_id < 0) p = copy(p, "--");
  else {
    p = hex_word(p, (unsigned)radio_id);
    p = copy(p, radio_id == SYNC_ID ? " OK" : " NO");
  }
  *p = 0;
  text(4, 42, row);
  p = copy(row, "GPS ");
  p = dec_pad(p, gps_bytes, 6);
  p = copy(p, " $");
  p = dec_pad(p, gps_dollars, 4);
  if (gps_dollars) p = copy(p, " OK");
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
    }
    last = REG(0xE0001004u);
  }
}
