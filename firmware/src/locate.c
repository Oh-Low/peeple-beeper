#include "locate.h"

#include "crypto.h"
#include "gps.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

void locate_init(locate_radio_t *loc) {
  memset(loc, 0, sizeof *loc);
  loc->status = LOCATE_IDLE;
  loc->track_open = true;
}

void locate_reset(locate_radio_t *loc) {
  locate_init(loc);
}

bool locate_busy(const locate_radio_t *loc) {
  return loc && loc->status == LOCATE_SENDING;
}

bool locate_first_wait_done(const locate_radio_t *loc, uint32_t now) {
  return loc && attempt_first_wait_done(&loc->attempt, now);
}

locate_status_t locate_status(const locate_radio_t *loc) {
  return loc ? loc->status : LOCATE_IDLE;
}

void locate_abort(locate_radio_t *loc) {
  if (!loc) return;
  attempt_stop(&loc->attempt);
  loc->status = LOCATE_IDLE;
}

static void put_i32le(uint8_t *p, int32_t v) {
  uint32_t u = (uint32_t)v;
  p[0] = (uint8_t)u;
  p[1] = (uint8_t)(u >> 8);
  p[2] = (uint8_t)(u >> 16);
  p[3] = (uint8_t)(u >> 24);
}

static int32_t get_i32le(const uint8_t *p) {
  uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  return (int32_t)u;
}

static bool read_fix(const hal_t *hal, gps_fix_t *out) {
  memset(out, 0, sizeof *out);
  if (!hal->gps_fix) return false;
  if (!hal->gps_fix(hal->ctx, out)) return false;
  return out->valid;
}

static void remember_fix(locate_radio_t *loc, const hal_t *hal, int32_t lat, int32_t lon) {
  uint8_t hh = 0;
  uint8_t mm = 0;
  if (hal && hal->clock) hal->clock(hal->ctx, &hh, &mm);
  loc->saved_fix = true;
  loc->saved_lat = lat;
  loc->saved_lon = lon;
  loc->saved_hh = hh;
  loc->saved_mm = mm;
}

static void send_loc(locate_radio_t *loc, const hal_t *hal, identity_store_t *identity,
                     const uint8_t *dest_id, const uint8_t *nonce, const uint8_t *session_key,
                     bool deny) {
  uint8_t plain[LOC_PLAIN_LEN];
  memset(plain, 0, sizeof plain);
  if (deny) {
    plain[0] = LOC_FLAG_DENY;
  } else {
    gps_fix_t fix;
    if (read_fix(hal, &fix)) remember_fix(loc, hal, fix.lat_min_e4, fix.lon_min_e4);
    if (loc->saved_fix) {
      plain[0] = LOC_FLAG_FIX;
      put_i32le(plain + 1, loc->saved_lat);
      put_i32le(plain + 5, loc->saved_lon);
      plain[9] = loc->saved_hh;
      plain[10] = loc->saved_mm;
    }
  }
  uint8_t cipher[LOC_PLAIN_LEN];
  uint8_t mac[MAC_LEN];
  page_seal(session_key, nonce, plain, LOC_PLAIN_LEN, cipher, mac);

  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  loc_payload_t p;
  memset(&p, 0, sizeof p);
  memcpy(p.dest_id, dest_id, NODE_ID_LEN);
  memcpy(p.src_id, us, NODE_ID_LEN);
  memcpy(p.nonce, nonce, PAGE_NONCE_LEN);
  p.hop_ttl = PAGE_HOP_TTL;
  memcpy(p.body, cipher, LOC_PLAIN_LEN);
  p.body_len = LOC_PLAIN_LEN;
  memcpy(p.mac, mac, MAC_LEN);

  uint8_t frame[RAW_FRAME_MAX];
  size_t n = frame_encode_loc(&p, frame, sizeof frame);
  if (!n) return;
  relay_mark_seen(&loc->relay, FRAME_LOC, us, nonce);
  hal->radio_send_raw(hal->ctx, frame, n);
  loc->reply_air = true;
}

static void note_tracked(locate_radio_t *loc, const hal_t *hal, const uint8_t *src) {
  uint32_t now = hal->now_ms(hal->ctx);
  if (node_id_equal(loc->tracked_id, src) && loc->tracked_quiet != 0 &&
      !time_reached(now, loc->tracked_quiet)) {
    return;
  }
  memcpy(loc->tracked_id, src, NODE_ID_LEN);
  loc->tracked_note = true;
  loc->tracked_quiet = now + TRACK_WARN_MS;
}

static int grant_slot(locate_radio_t *loc, const uint8_t *id) {
  for (int i = 0; i < TRACK_GRANT_MAX; i++) {
    if (loc->grant[i].on && node_id_equal(loc->grant[i].id, id)) return i;
  }
  return -1;
}

static bool grant_live(locate_radio_t *loc, const uint8_t *id, uint32_t now) {
  int i = grant_slot(loc, id);
  if (i < 0) return false;
  if (time_reached(now, loc->grant[i].until)) {
    loc->grant[i].on = false;
    return false;
  }
  return true;
}

static void grant_remember(locate_radio_t *loc, const uint8_t *id, uint32_t now) {
  int slot = grant_slot(loc, id);
  if (slot < 0) {
    for (int i = 0; i < TRACK_GRANT_MAX; i++) {
      if (!loc->grant[i].on) {
        slot = i;
        break;
      }
    }
  }
  if (slot < 0) {
    slot = 0;
    for (int i = 1; i < TRACK_GRANT_MAX; i++) {
      if (time_reached(loc->grant[slot].until, loc->grant[i].until)) slot = i;
    }
  }
  loc->grant[slot].on = true;
  memcpy(loc->grant[slot].id, id, NODE_ID_LEN);
  loc->grant[slot].until = now + TRACK_ALLOW_MS;
}

static int ask_slot(const locate_radio_t *loc, const uint8_t *id) {
  for (int i = 0; i < loc->ask_n; i++) {
    if (node_id_equal(loc->ask[i].id, id)) return i;
  }
  return -1;
}

static void drop_ask_head(locate_radio_t *loc) {
  if (loc->ask_n <= 0) return;
  loc->ask_n--;
  if (loc->ask_n > 0) {
    memmove(&loc->ask[0], &loc->ask[1], (size_t)loc->ask_n * sizeof loc->ask[0]);
  }
}

static void hold_ask(locate_radio_t *loc, const uint8_t *id, const uint8_t *nonce) {
  int slot = ask_slot(loc, id);
  if (slot >= 0) {
    memcpy(loc->ask[slot].nonce, nonce, PAGE_NONCE_LEN);
    return;
  }
  if (loc->ask_n >= TRACK_ASK_MAX + 1) return;
  memcpy(loc->ask[loc->ask_n].id, id, NODE_ID_LEN);
  memcpy(loc->ask[loc->ask_n].nonce, nonce, PAGE_NONCE_LEN);
  loc->ask_n++;
}

static void gps_power(const hal_t *hal, bool on);

static void hunt_begin(locate_radio_t *loc, const hal_t *hal) {
  loc->fix_hunt = true;
  gps_power(hal, true);
}

static void hunt_end(locate_radio_t *loc, const hal_t *hal, uint32_t now) {
  loc->fix_hunt = false;
  loc->gps_listening = false;
  if (!loc->saved_fix) {
    loc->gps_next_on = now;
    return;
  }
  gps_power(hal, false);
  loc->gps_next_on = now + GPS_SAMPLE_PERIOD_MS;
}

static bool warn_quiet(locate_radio_t *loc, const hal_t *hal, const uint8_t *src) {
  uint32_t now = hal->now_ms(hal->ctx);
  return node_id_equal(loc->tracked_id, src) && loc->tracked_quiet != 0 &&
         !time_reached(now, loc->tracked_quiet);
}

static void arm_ask(locate_radio_t *loc, const hal_t *hal, const loc_req_payload_t *req) {
  uint32_t now = hal->now_ms(hal->ctx);
  bool was_empty = loc->ask_n == 0;
  hold_ask(loc, req->src_id, req->nonce);
  if (!loc->fix_hunt) hunt_begin(loc, hal);
  if (!was_empty) return;
  loc->ask_alert_ready = false;
  loc->ask_alert_at = now + GPS_ALERT_MS;
}

static void arm_open_reply(locate_radio_t *loc, const hal_t *hal, const loc_req_payload_t *req) {
  uint32_t now = hal->now_ms(hal->ctx);
  if (loc->reply_due && !node_id_equal(loc->reply_id, req->src_id)) return;
  bool same = loc->reply_due && node_id_equal(loc->reply_id, req->src_id);
  memcpy(loc->reply_id, req->src_id, NODE_ID_LEN);
  memcpy(loc->reply_nonce, req->nonce, PAGE_NONCE_LEN);
  if (!loc->fix_hunt) hunt_begin(loc, hal);
  if (same) return;
  loc->reply_due = true;
  loc->reply_at = now + GPS_OPEN_REPLY_MS;
  if (!loc->track_open || warn_quiet(loc, hal, req->src_id)) return;
  loc->alert_due = true;
  loc->alert_at = now + GPS_ALERT_MS;
  memcpy(loc->alert_id, req->src_id, NODE_ID_LEN);
}

static void answer_req(locate_radio_t *loc, identity_store_t *identity, contact_book_t *contacts,
                       const hal_t *hal, const loc_req_payload_t *req) {
  const contact_t *contact = contacts_find_by_id(contacts, req->src_id);
  if (!contact) return;
  if (!loc_req_check(contact->session_key, req->nonce, req->mac)) return;
  uint32_t now = hal->now_ms(hal->ctx);
  if (!loc->track_open && !grant_live(loc, req->src_id, now)) {
    arm_ask(loc, hal, req);
    return;
  }
  if (loc->track_open) {
    arm_open_reply(loc, hal, req);
    return;
  }
  send_loc(loc, hal, identity, req->src_id, req->nonce, contact->session_key, false);
}

static void try_accept(locate_radio_t *loc, identity_store_t *identity, contact_book_t *contacts,
                       const hal_t *hal, const loc_payload_t *p) {
  (void)hal;
  if (loc->status != LOCATE_SENDING) return;
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  if (!attempt_from_peer(&loc->attempt, us, p->dest_id, loc->dest_id, p->src_id, p->nonce)) return;
  const contact_t *contact = contacts_find_by_id(contacts, loc->dest_id);
  if (!contact) return;
  uint8_t plain[LOC_PLAIN_LEN];
  if (!page_open(contact->session_key, p->nonce, p->body, p->body_len, p->mac, plain)) return;
  attempt_stop(&loc->attempt);
  if (plain[0] & LOC_FLAG_DENY) {
    loc->status = LOCATE_DENIED;
    return;
  }
  if (plain[0] & LOC_FLAG_FIX) {
    loc->their_lat = get_i32le(plain + 1);
    loc->their_lon = get_i32le(plain + 5);
    loc->has_fix = true;
    loc->loc_hh = plain[9];
    loc->loc_mm = plain[10];
    loc->has_loc_time = true;
    loc->status = LOCATE_FOUND;
  } else if (loc->has_fix) {
    loc->status = LOCATE_FOUND;
  } else {
    loc->their_lat = 0;
    loc->their_lon = 0;
    loc->status = LOCATE_NO_SKY;
  }
}

void locate_on_raw(locate_radio_t *loc, identity_store_t *identity, contact_book_t *contacts,
                   const hal_t *hal, const raw_frame_t *raw) {
  if (!loc || !raw) return;
  int kind = frame_type(raw->bytes, raw->len);
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);

  if (kind == FRAME_LOC_REQ) {
    loc_req_payload_t req;
    if (!frame_decode_loc_req(raw->bytes, raw->len, &req)) return;
    if (node_id_equal(req.dest_id, us)) {
      answer_req(loc, identity, contacts, hal, &req);
      return;
    }
    if (node_id_equal(req.src_id, us)) return;
    relay_on_request(&loc->relay, hal, FRAME_LOC_REQ, FRAME_LOC, req.src_id, req.dest_id, req.nonce,
                     req.hop_ttl, raw->bytes, raw->len);
    return;
  }

  if (kind == FRAME_LOC) {
    loc_payload_t p;
    if (!frame_decode_loc(raw->bytes, raw->len, &p)) return;
    if (node_id_equal(p.dest_id, us)) {
      try_accept(loc, identity, contacts, hal, &p);
      return;
    }
    if (node_id_equal(p.src_id, us)) return;
    relay_on_reply(&loc->relay, hal, FRAME_LOC_REQ, FRAME_LOC, p.src_id, p.dest_id, p.nonce,
                   p.hop_ttl, raw->bytes, raw->len);
  }
}

static bool tx_attempt(locate_radio_t *loc, const hal_t *hal, identity_store_t *identity,
                       contact_book_t *contacts) {
  const contact_t *contact = contacts_find_by_id(contacts, loc->dest_id);
  if (!contact) return false;
  uint8_t nonce[PAGE_NONCE_LEN];
  hal->random_fill(hal->ctx, nonce, PAGE_NONCE_LEN);
  uint8_t us[NODE_ID_LEN];
  identity_get_node_id(identity, us);
  loc_req_payload_t req;
  memset(&req, 0, sizeof req);
  memcpy(req.dest_id, loc->dest_id, NODE_ID_LEN);
  memcpy(req.src_id, us, NODE_ID_LEN);
  memcpy(req.nonce, nonce, PAGE_NONCE_LEN);
  req.hop_ttl = PAGE_HOP_TTL;
  loc_req_mac(contact->session_key, nonce, req.mac);
  uint8_t frame[PAGE_ACK_LEN];
  size_t n = frame_encode_loc_req(&req, frame, sizeof frame);
  if (!n) return false;
  if (!attempt_remember(&loc->attempt, nonce)) return false;
  relay_mark_seen(&loc->relay, FRAME_LOC_REQ, us, nonce);
  hal->radio_send_raw(hal->ctx, frame, n);
  return true;
}

typedef struct {
  locate_radio_t *loc;
  const hal_t *hal;
  identity_store_t *identity;
  contact_book_t *contacts;
} locate_send_t;

static bool locate_send(void *user) {
  locate_send_t *send = user;
  return tx_attempt(send->loc, send->hal, send->identity, send->contacts);
}

static bool begin_send(locate_radio_t *loc) {
  loc->status = LOCATE_SENDING;
  attempt_arm(&loc->attempt, ATTEMPT_LOCATE);
  return true;
}

bool locate_tx(locate_radio_t *loc, const hal_t *hal, const uint8_t *dest_id, contact_book_t *contacts) {
  if (!loc || !hal || !dest_id) return false;
  if (locate_busy(loc)) return false;
  const contact_t *contact = contacts_find_by_id(contacts, dest_id);
  if (!contact) return false;

  loc->their_lat = 0;
  loc->their_lon = 0;
  loc->has_fix = false;
  loc->has_loc_time = false;
  memcpy(loc->dest_id, dest_id, NODE_ID_LEN);
  return begin_send(loc);
}

bool locate_refresh(locate_radio_t *loc, const hal_t *hal, contact_book_t *contacts) {
  if (!loc || !hal) return false;
  if (loc->status == LOCATE_IDLE || locate_busy(loc)) return false;
  if (!contacts_find_by_id(contacts, loc->dest_id)) return false;
  return begin_send(loc);
}

static void outbox_tick(locate_radio_t *loc, identity_store_t *identity, contact_book_t *contacts,
                        const hal_t *hal) {
  if (loc->status != LOCATE_SENDING) return;
  locate_send_t send = {loc, hal, identity, contacts};
  if (attempt_step(&loc->attempt, hal->now_ms(hal->ctx), locate_send, &send) == ATTEMPT_DONE) {
    loc->status = loc->has_fix ? LOCATE_FOUND : LOCATE_NO_REPLY;
  }
}

static void gps_power(const hal_t *hal, bool on) {
  if (hal && hal->gps_power) hal->gps_power(hal->ctx, on);
}

static void gps_listen_begin(locate_radio_t *loc, const hal_t *hal, uint32_t now) {
  loc->gps_listening = true;
  loc->gps_listen_at = now;
  gps_power(hal, true);
}

static void gps_listen_end(locate_radio_t *loc, const hal_t *hal, uint32_t now) {
  loc->gps_listening = false;
  if (!loc->saved_fix) {
    /* EN low resets a search that has not stored a fix. */
    loc->gps_next_on = now;
    return;
  }
  gps_power(hal, false);
  loc->gps_next_on = now + GPS_SAMPLE_PERIOD_MS;
}

void locate_gps_soon(locate_radio_t *loc) {
  if (!loc || loc->gps_listening) return;
  loc->gps_next_on = 0;
}

void locate_gps_capture(locate_radio_t *loc, const hal_t *hal) {
  if (!loc || !hal) return;
  if (loc->gps_listening || loc->fix_hunt) return;
  gps_power(hal, true);
  gps_fix_t fix;
  if (read_fix(hal, &fix)) {
    remember_fix(loc, hal, fix.lat_min_e4, fix.lon_min_e4);
    gps_power(hal, false);
    return;
  }
  loc->gps_listening = true;
  loc->gps_listen_at = hal->now_ms(hal->ctx);
}

void locate_gps_tick(locate_radio_t *loc, const hal_t *hal) {
  if (!loc || !hal) return;
  uint32_t now = hal->now_ms(hal->ctx);
  if (loc->gps_listening || loc->fix_hunt) {
    gps_fix_t fix;
    if (read_fix(hal, &fix)) {
      remember_fix(loc, hal, fix.lat_min_e4, fix.lon_min_e4);
      if (loc->gps_listening && !loc->fix_hunt) gps_listen_end(loc, hal, now);
    } else if (loc->gps_listening && !loc->fix_hunt &&
               elapsed_u32(now, loc->gps_listen_at) >= GPS_LISTEN_MS) {
      gps_listen_end(loc, hal, now);
    }
    if (loc->fix_hunt) return;
    if (loc->gps_listening) return;
  }
  if (time_reached(now, loc->gps_next_on)) gps_listen_begin(loc, hal, now);
}

void locate_clear_reply(locate_radio_t *loc) {
  if (loc) loc->reply_air = false;
}

bool locate_sent_reply(const locate_radio_t *loc) { return loc && loc->reply_air; }

static void flush_track(locate_radio_t *loc, identity_store_t *identity, contact_book_t *contacts,
                        const hal_t *hal) {
  uint32_t now = hal->now_ms(hal->ctx);
  if (loc->fix_hunt) {
    gps_fix_t fix;
    if (read_fix(hal, &fix)) remember_fix(loc, hal, fix.lat_min_e4, fix.lon_min_e4);
  }
  if (loc->ask_n > 0 && !loc->ask_alert_ready && time_reached(now, loc->ask_alert_at))
    loc->ask_alert_ready = true;
  if (loc->alert_due && time_reached(now, loc->alert_at)) {
    note_tracked(loc, hal, loc->alert_id);
    loc->alert_due = false;
  }
  if (!loc->reply_due || !time_reached(now, loc->reply_at)) return;
  const contact_t *contact = contacts_find_by_id(contacts, loc->reply_id);
  uint8_t id[NODE_ID_LEN];
  uint8_t nonce[PAGE_NONCE_LEN];
  memcpy(id, loc->reply_id, NODE_ID_LEN);
  memcpy(nonce, loc->reply_nonce, PAGE_NONCE_LEN);
  loc->reply_due = false;
  if (contact) send_loc(loc, hal, identity, id, nonce, contact->session_key, false);
  if (loc->ask_n == 0) hunt_end(loc, hal, now);
}

void locate_tick(locate_radio_t *loc, identity_store_t *identity, contact_book_t *contacts,
                 const hal_t *hal) {
  if (!loc || !hal) return;
  outbox_tick(loc, identity, contacts, hal);
  relay_tick(&loc->relay, hal);
  flush_track(loc, identity, contacts, hal);
}

void locate_set_open(locate_radio_t *loc, bool open) {
  if (!loc) return;
  loc->track_open = open;
}

bool locate_is_open(const locate_radio_t *loc) { return loc && loc->track_open; }

bool locate_ask_pending(const locate_radio_t *loc) {
  return loc && loc->ask_n > 0 && loc->ask_alert_ready;
}

bool locate_ask_id(const locate_radio_t *loc, uint8_t id[NODE_ID_LEN]) {
  if (!loc || loc->ask_n <= 0 || !id) return false;
  memcpy(id, loc->ask[0].id, NODE_ID_LEN);
  return true;
}

static bool answer_pending(locate_radio_t *loc, const hal_t *hal, identity_store_t *identity,
                           contact_book_t *contacts, bool deny) {
  if (!loc || loc->ask_n <= 0 || !hal) return false;
  const contact_t *contact = contacts_find_by_id(contacts, loc->ask[0].id);
  uint8_t id[NODE_ID_LEN];
  uint8_t nonce[PAGE_NONCE_LEN];
  uint32_t now = hal->now_ms(hal->ctx);
  memcpy(id, loc->ask[0].id, NODE_ID_LEN);
  memcpy(nonce, loc->ask[0].nonce, PAGE_NONCE_LEN);
  drop_ask_head(loc);
  if (!contact) {
    if (loc->ask_n == 0 && !loc->reply_due) hunt_end(loc, hal, now);
    return false;
  }
  send_loc(loc, hal, identity, id, nonce, contact->session_key, deny);
  if (!deny) grant_remember(loc, id, now);
  if (loc->ask_n > 0) {
    loc->ask_alert_ready = false;
    loc->ask_alert_at = now + GPS_ALERT_MS;
  } else if (!loc->reply_due) {
    hunt_end(loc, hal, now);
  }
  return true;
}

bool locate_allow(locate_radio_t *loc, const hal_t *hal, identity_store_t *identity,
                  contact_book_t *contacts) {
  return answer_pending(loc, hal, identity, contacts, false);
}

bool locate_deny(locate_radio_t *loc, const hal_t *hal, identity_store_t *identity,
                 contact_book_t *contacts) {
  return answer_pending(loc, hal, identity, contacts, true);
}

bool locate_tracked(const locate_radio_t *loc) { return loc && loc->tracked_note; }

bool locate_tracked_id(const locate_radio_t *loc, uint8_t id[NODE_ID_LEN]) {
  if (!loc || !loc->tracked_note || !id) return false;
  memcpy(id, loc->tracked_id, NODE_ID_LEN);
  return true;
}

void locate_clear_tracked(locate_radio_t *loc) {
  if (!loc) return;
  loc->tracked_note = false;
}

void locate_format(const locate_radio_t *loc, const hal_t *hal, locate_view_t *view) {
  if (!view) return;
  memset(view, 0, sizeof *view);
  if (!loc) return;
  if (loc->status == LOCATE_DENIED) {
    str_copy_max(view->dist, sizeof view->dist, "denied", 16);
    return;
  }
  if (loc->has_loc_time) {
    snprintf(view->when, sizeof view->when, "%02d:%02d", (int)loc->loc_hh, (int)loc->loc_mm);
  }
  if (loc->status == LOCATE_NO_REPLY && !loc->has_fix) {
    str_copy_max(view->dist, sizeof view->dist, "no reply", 16);
    return;
  }
  if (loc->status == LOCATE_NO_SKY && !loc->has_fix) {
    str_copy_max(view->dist, sizeof view->dist, "no sky", 16);
    return;
  }
  if (!loc->has_fix && loc->status != LOCATE_FOUND) return;
  gps_fix_t mine;
  bool own = read_fix(hal, &mine);
  if (!own && loc->saved_fix) {
    mine.lat_min_e4 = loc->saved_lat;
    mine.lon_min_e4 = loc->saved_lon;
    own = true;
  }
  if (!own) {
    str_copy_max(view->dist, sizeof view->dist, "no own fix", 16);
    return;
  }
  int north = 0;
  int east = 0;
  gps_delta_ft(mine.lat_min_e4, mine.lon_min_e4, loc->their_lat, loc->their_lon, &north, &east);
  int dist = gps_hypot_ft(north, east);
  view->north_ft = north;
  view->east_ft = east;
  view->arrow = north != 0 || east != 0;
  if (dist >= 1000) {
    int tenths = (dist * 10 + 2640) / 5280;
    snprintf(view->dist, sizeof view->dist, "%d.%d mi", tenths / 10, tenths % 10);
  } else {
    snprintf(view->dist, sizeof view->dist, "%d ft", dist);
  }
}
