#include "store.h"

#include "beeple.h"
#include "canned.h"
#include "contacts.h"
#include "util.h"

#include <string.h>

#define MAGIC0 0x50
#define MAGIC1 0x42
#define MAGIC2 0x30
#define MAGIC3 0x31
#define STORE_VERSION 8
#define STATUS_SENDING 0
#define STATUS_SENT 1
#define STATUS_FAILED 2

typedef struct {
  uint8_t *buf;
  size_t cap;
  size_t o;
} writer_t;

typedef struct {
  const uint8_t *buf;
  size_t len;
  size_t o;
  bool ok;
} reader_t;

static size_t w_remain(const writer_t *w) {
  return w->cap > w->o ? w->cap - w->o : 0;
}

static bool w_u8(writer_t *w, uint8_t n) {
  if (w_remain(w) < 1) return false;
  w->buf[w->o++] = n;
  return true;
}

static bool w_u16(writer_t *w, uint16_t n) {
  if (w_remain(w) < 2) return false;
  w->buf[w->o++] = (uint8_t)(n & 0xff);
  w->buf[w->o++] = (uint8_t)((n >> 8) & 0xff);
  return true;
}

static bool w_u32(writer_t *w, uint32_t n) {
  if (w_remain(w) < 4) return false;
  w->buf[w->o++] = (uint8_t)(n & 0xff);
  w->buf[w->o++] = (uint8_t)((n >> 8) & 0xff);
  w->buf[w->o++] = (uint8_t)((n >> 16) & 0xff);
  w->buf[w->o++] = (uint8_t)((n >> 24) & 0xff);
  return true;
}

static bool w_raw(writer_t *w, const uint8_t *b, size_t n) {
  if (w_remain(w) < n) return false;
  if (n) memcpy(w->buf + w->o, b, n);
  w->o += n;
  return true;
}

static bool w_str(writer_t *w, const char *s, size_t max_chars) {
  size_t n = str_ascii_len(s);
  if (n > max_chars) n = max_chars;
  if (n > 255) return false;
  if (!w_u8(w, (uint8_t)n)) return false;
  return w_raw(w, (const uint8_t *)s, n);
}

static uint8_t r_u8(reader_t *r) {
  if (r->len - r->o < 1) {
    r->ok = false;
    return 0;
  }
  return r->buf[r->o++];
}

static uint16_t r_u16(reader_t *r) {
  uint8_t lo = r_u8(r);
  uint8_t hi = r_u8(r);
  return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static uint32_t r_u32(reader_t *r) {
  uint32_t a = r_u8(r);
  uint32_t b = r_u8(r);
  uint32_t c = r_u8(r);
  uint32_t d = r_u8(r);
  return a | (b << 8) | (c << 16) | (d << 24);
}

static bool r_raw(reader_t *r, uint8_t *out, size_t n) {
  if (r->len - r->o < n) {
    r->ok = false;
    if (out && n) memset(out, 0, n);
    return false;
  }
  if (n) memcpy(out, r->buf + r->o, n);
  r->o += n;
  return true;
}

static void r_str(reader_t *r, char *out, size_t cap, size_t max_chars) {
  uint8_t n = r_u8(r);
  if (!r->ok || n > max_chars || n + 1 > cap) {
    r->ok = false;
    if (out && cap) out[0] = 0;
    return;
  }
  if (!r_raw(r, (uint8_t *)out, n)) {
    if (out && cap) out[0] = 0;
    return;
  }
  out[n] = 0;
}

static uint8_t i8_store(int n) {
  if (n < -128) n = -128;
  if (n > 127) n = 127;
  return (uint8_t)(int8_t)n;
}

static int from_i8(uint8_t n) {
  return n > 127 ? (int)n - 256 : (int)n;
}

static uint8_t outbox_status_byte(outbox_status_t s) {
  if (s == OUTBOX_SENT) return STATUS_SENT;
  if (s == OUTBOX_FAILED) return STATUS_FAILED;
  return STATUS_SENDING;
}

static outbox_status_t outbox_status_from(uint8_t n) {
  if (n == STATUS_SENT) return OUTBOX_SENT;
  if (n == STATUS_FAILED) return OUTBOX_FAILED;
  return OUTBOX_FAILED;
}

uint32_t store_hash_blob(const uint8_t *bytes, size_t len) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    h ^= bytes[i];
    h *= 16777619u;
  }
  return h ^ (uint32_t)len;
}

static size_t encode_slice(beeper_t *b, const inbox_row_t *inbox, int inbox_n,
                           const outbox_item_t *outbox, int outbox_n, uint8_t *out, size_t max) {
  if (!b->identity.has_id) return 0;
  writer_t w = {.buf = out, .cap = max, .o = 0};
  if (!w_u8(&w, MAGIC0) || !w_u8(&w, MAGIC1) || !w_u8(&w, MAGIC2) || !w_u8(&w, MAGIC3)) return (size_t)-1;
  if (!w_u8(&w, STORE_VERSION)) return (size_t)-1;
  if (!w_raw(&w, b->identity.node_id, NODE_ID_LEN)) return (size_t)-1;
  if (!w_str(&w, b->identity.display_name, PAGE_FROM_MAX)) return (size_t)-1;

  if (b->contacts.n > CONTACTS_MAX) return (size_t)-1;
  if (!w_u16(&w, (uint16_t)b->contacts.n)) return (size_t)-1;
  for (int i = 0; i < b->contacts.n; i++) {
    const contact_t *c = &b->contacts.rows[i];
    if (!w_raw(&w, c->node_id, NODE_ID_LEN) || !w_str(&w, c->name, PAGE_FROM_MAX)) return (size_t)-1;
    if (!w_raw(&w, c->session_key, SESSION_KEY_LEN) || !w_u32(&w, c->paired_ms)) return (size_t)-1;
    if (!w_u8(&w, c->beeple)) return (size_t)-1;
  }

  int canned_n = b->screen.canned.n;
  if (canned_n > CANNED_SAVED_MAX) canned_n = CANNED_SAVED_MAX;
  if (!w_u8(&w, (uint8_t)canned_n)) return (size_t)-1;
  for (int i = 0; i < canned_n; i++) {
    if (!w_str(&w, b->screen.canned.saved[i], PAGE_BODY_MAX)) return (size_t)-1;
  }

  if (!w_u16(&w, (uint16_t)inbox_n)) return (size_t)-1;
  for (int i = 0; i < inbox_n; i++) {
    const inbox_row_t *p = &inbox[i];
    if (!w_str(&w, p->from, PAGE_FROM_MAX) || !w_str(&w, p->body, PAGE_BODY_MAX)) return (size_t)-1;
    if (!w_u8(&w, i8_store(p->rssi)) || !w_u32(&w, p->received_ms)) return (size_t)-1;
    if (!w_u8(&w, p->received_hh) || !w_u8(&w, p->received_mm)) return (size_t)-1;
  }

  if (!w_u16(&w, (uint16_t)outbox_n)) return (size_t)-1;
  for (int i = 0; i < outbox_n; i++) {
    const outbox_item_t *row = &outbox[i];
    if (!w_raw(&w, row->dest_id, NODE_ID_LEN) || !w_str(&w, row->to, PAGE_FROM_MAX)) return (size_t)-1;
    if (!w_str(&w, row->body, PAGE_BODY_MAX) || !w_u8(&w, outbox_status_byte(row->status))) {
      return (size_t)-1;
    }
    if (!w_u8(&w, row->sent_hh) || !w_u8(&w, row->sent_mm)) return (size_t)-1;
    if (!w_u8(&w, row->acked_hh) || !w_u8(&w, row->acked_mm)) return (size_t)-1;
  }

  if (!w_u8(&w, b->locate.saved_fix ? 1 : 0)) return (size_t)-1;
  if (!w_u32(&w, (uint32_t)b->locate.saved_lat) || !w_u32(&w, (uint32_t)b->locate.saved_lon)) {
    return (size_t)-1;
  }
  if (!w_u8(&w, b->locate.saved_hh) || !w_u8(&w, b->locate.saved_mm)) return (size_t)-1;
  if (!w_u8(&w, b->locate.track_open ? 1 : 0)) return (size_t)-1;
  if (!w_u8(&w, b->screen.show_clock_bat ? 1 : 0)) return (size_t)-1;
  if (!w_u8(&w, b->screen.dark_mode ? 1 : 0)) return (size_t)-1;
  if (!w_u8(&w, (uint8_t)b->screen.beeple)) return (size_t)-1;
  return w.o;
}

size_t store_encode(beeper_t *b, uint8_t *out, size_t max) {
  inbox_row_t inbox[PAGE_KEEP_MAX];
  outbox_item_t outbox[PAGE_KEEP_MAX];
  int inbox_n = b->screen.inbox_n;
  int outbox_n = b->screen.outbox_n;
  if (inbox_n > 0) memcpy(inbox, b->screen.inbox, (size_t)inbox_n * sizeof(inbox_row_t));
  if (outbox_n > 0) memcpy(outbox, b->screen.outbox, (size_t)outbox_n * sizeof(outbox_item_t));

  for (;;) {
    size_t n = encode_slice(b, inbox, inbox_n, outbox, outbox_n, out, max);
    if (n == (size_t)-1) return 0;
    if (n <= NVM_BYTES && n <= max) {
      b->screen.inbox_n = inbox_n;
      if (inbox_n > 0) memcpy(b->screen.inbox, inbox, (size_t)inbox_n * sizeof(inbox_row_t));
      b->screen.outbox_n = outbox_n;
      if (outbox_n > 0) memcpy(b->screen.outbox, outbox, (size_t)outbox_n * sizeof(outbox_item_t));
      return n;
    }
    if (inbox_n > 0) inbox_n--;
    else if (outbox_n > 0) outbox_n--;
    else return 0;
  }
}

bool store_apply(beeper_t *b, const uint8_t *bytes, size_t len) {
  if (len == 0) return false;
  reader_t r = {.buf = bytes, .len = len, .o = 0, .ok = true};
  if (r_u8(&r) != MAGIC0 || r_u8(&r) != MAGIC1 || r_u8(&r) != MAGIC2 || r_u8(&r) != MAGIC3) {
    return false;
  }
  uint8_t ver = r_u8(&r);
  if (ver < 1 || ver > STORE_VERSION) return false;

  uint8_t node_id[NODE_ID_LEN];
  if (!r_raw(&r, node_id, NODE_ID_LEN)) return false;
  char display_name[PAGE_FROM_MAX + 1];
  r_str(&r, display_name, sizeof display_name, PAGE_FROM_MAX);
  if (!r.ok) return false;

  uint16_t n_contacts = r_u16(&r);
  if (!r.ok || n_contacts > CONTACTS_MAX) return false;
  contact_t contacts[CONTACTS_MAX];
  memset(contacts, 0, sizeof contacts);
  for (uint16_t i = 0; i < n_contacts; i++) {
    if (!r_raw(&r, contacts[i].node_id, NODE_ID_LEN)) return false;
    r_str(&r, contacts[i].name, sizeof contacts[i].name, PAGE_FROM_MAX);
    if (!r_raw(&r, contacts[i].session_key, SESSION_KEY_LEN)) return false;
    contacts[i].paired_ms = r_u32(&r);
    contacts[i].beeple = BEEPLE_NONE;
    if (ver >= 8) contacts[i].beeple = r_u8(&r);
    if (!r.ok) return false;
  }

  uint8_t n_canned = r_u8(&r);
  if (!r.ok || n_canned > CANNED_SAVED_MAX) return false;
  char canned[CANNED_SAVED_MAX][PAGE_BODY_MAX + 1];
  memset(canned, 0, sizeof canned);
  for (uint8_t i = 0; i < n_canned; i++) {
    r_str(&r, canned[i], sizeof canned[i], PAGE_BODY_MAX);
    if (!r.ok) return false;
  }

  uint16_t n_inbox = r_u16(&r);
  if (!r.ok || n_inbox > PAGE_KEEP_MAX) return false;
  inbox_row_t inbox[PAGE_KEEP_MAX];
  memset(inbox, 0, sizeof inbox);
  for (uint16_t i = 0; i < n_inbox; i++) {
    r_str(&r, inbox[i].from, sizeof inbox[i].from, PAGE_FROM_MAX);
    r_str(&r, inbox[i].body, sizeof inbox[i].body, PAGE_BODY_MAX);
    inbox[i].rssi = (int8_t)from_i8(r_u8(&r));
    inbox[i].received_ms = r_u32(&r);
    inbox[i].received_hh = 0;
    inbox[i].received_mm = 0;
    if (ver >= 2) {
      inbox[i].received_hh = r_u8(&r);
      inbox[i].received_mm = r_u8(&r);
    }
    if (!r.ok) return false;
  }

  uint16_t n_out = r_u16(&r);
  if (!r.ok || n_out > PAGE_KEEP_MAX) return false;
  outbox_item_t outbox[PAGE_KEEP_MAX];
  memset(outbox, 0, sizeof outbox);
  for (uint16_t i = 0; i < n_out; i++) {
    if (!r_raw(&r, outbox[i].dest_id, NODE_ID_LEN)) return false;
    r_str(&r, outbox[i].to, sizeof outbox[i].to, PAGE_FROM_MAX);
    r_str(&r, outbox[i].body, sizeof outbox[i].body, PAGE_BODY_MAX);
    outbox[i].status = outbox_status_from(r_u8(&r));
    outbox[i].sent_hh = 0;
    outbox[i].sent_mm = 0;
    outbox[i].acked_hh = CLOCK_NONE;
    outbox[i].acked_mm = CLOCK_NONE;
    if (ver >= 2) {
      outbox[i].sent_hh = r_u8(&r);
      outbox[i].sent_mm = r_u8(&r);
      outbox[i].acked_hh = r_u8(&r);
      outbox[i].acked_mm = r_u8(&r);
    }
    if (!r.ok) return false;
  }

  bool saved_fix = false;
  int32_t saved_lat = 0;
  int32_t saved_lon = 0;
  uint8_t saved_hh = 0;
  uint8_t saved_mm = 0;
  bool track_open = false;
  if (ver >= 3) {
    saved_fix = r_u8(&r) != 0;
    saved_lat = (int32_t)r_u32(&r);
    saved_lon = (int32_t)r_u32(&r);
    saved_hh = r_u8(&r);
    saved_mm = r_u8(&r);
  }
  if (ver >= 4) track_open = r_u8(&r) != 0;
  bool show_clock_bat = true;
  if (ver >= 5) show_clock_bat = r_u8(&r) != 0;
  bool dark_mode = false;
  if (ver >= 6) dark_mode = r_u8(&r) != 0;
  int beeple = 0;
  if (ver >= 7) beeple = r_u8(&r);

  if (!r.ok || r.o != r.len) return false;

  memcpy(b->identity.node_id, node_id, NODE_ID_LEN);
  b->identity.has_id = true;
  str_copy_max(b->identity.display_name, sizeof b->identity.display_name, display_name,
               PAGE_FROM_MAX);

  contacts_reset(&b->contacts);
  for (uint16_t i = 0; i < n_contacts; i++) contacts_upsert(&b->contacts, &contacts[i]);

  canned_reset(&b->screen.canned);
  b->screen.canned.n = n_canned;
  for (uint8_t i = 0; i < n_canned; i++) {
    str_copy_max(b->screen.canned.saved[i], sizeof b->screen.canned.saved[i], canned[i], PAGE_BODY_MAX);
  }

  b->screen.inbox_n = n_inbox;
  if (n_inbox) memcpy(b->screen.inbox, inbox, (size_t)n_inbox * sizeof(inbox_row_t));
  b->screen.outbox_n = n_out;
  if (n_out) memcpy(b->screen.outbox, outbox, (size_t)n_out * sizeof(outbox_item_t));
  b->locate.saved_fix = saved_fix;
  b->locate.saved_lat = saved_lat;
  b->locate.saved_lon = saved_lon;
  b->locate.saved_hh = saved_hh;
  b->locate.saved_mm = saved_mm;
  b->locate.track_open = track_open;
  b->screen.show_clock_bat = show_clock_bat;
  b->screen.dark_mode = dark_mode;
  screen_set_beeple(&b->screen, beeple);
  return true;
}

void store_remember_hash(beeper_t *b, const uint8_t *bytes, size_t len) {
  b->nvm_hash = store_hash_blob(bytes, len);
}

bool store_load(beeper_t *b, const hal_t *hal) {
  static uint8_t buf[NVM_BYTES];
  size_t n = hal->nvm_load(hal->ctx, buf, NVM_BYTES);
  if (n <= 0) return false;
  if (!store_apply(b, buf, n)) return false;
  store_remember_hash(b, buf, n);
  return true;
}

bool store_save(beeper_t *b, const hal_t *hal) {
  if (!b->identity.has_id) {
    return hal->nvm_save(hal->ctx, (const uint8_t *)"", 0);
  }
  static uint8_t bytes[NVM_BYTES];
  size_t n = store_encode(b, bytes, sizeof bytes);
  if (!n) return false;
  return hal->nvm_save(hal->ctx, bytes, n);
}

void store_sync(beeper_t *b, const hal_t *hal) {
  if (!b->identity.has_id) return;
  static uint8_t bytes[NVM_BYTES];
  size_t n = store_encode(b, bytes, sizeof bytes);
  if (!n) return;
  uint32_t h = store_hash_blob(bytes, n);
  if (h == b->nvm_hash) return;
  if (!hal->nvm_save(hal->ctx, bytes, n)) return;
  b->nvm_hash = h;
}
