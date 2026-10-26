#include "air.h"
#include "beeper.h"
#include "contacts.h"
#include "hal_ram.h"
#include "identity.h"
#include "seed.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;

#define CHECK(cond, msg)           \
  do {                             \
    if (!(cond)) {                 \
      printf("not ok  %s\n", msg); \
      g_fail++;                    \
      return;                      \
    }                              \
  } while (0)

#define CHECK_EQ(a, b, msg)                                                    \
  do {                                                                         \
    int _a = (int)(a), _b = (int)(b);                                          \
    if (_a != _b) {                                                            \
      printf("not ok  %s: %d !== %d\n", msg, _a, _b);                          \
      g_fail++;                                                                \
      return;                                                                  \
    }                                                                          \
  } while (0)

static void pass(const char *name) {
  g_pass++;
  printf("ok  %s\n", name);
}

static bool poll_raw_hal(hal_t *hal, uint8_t *out, uint8_t *len) {
  raw_frame_t f;
  memset(&f, 0, sizeof f);
  if (!hal->radio_poll_raw(hal->ctx, &f)) return false;
  if (out && f.len) memcpy(out, f.bytes, f.len);
  if (len) *len = f.len;
  return true;
}

static void test_falloff(void) {
  int8_t rssi = 0;
  CHECK(air_rssi(0, &rssi) && rssi == -30, "at origin");
  CHECK(air_rssi(75, &rssi), "75ft still on air");
  CHECK(!air_rssi(RADIO_RANGE_FT, &rssi), "100ft silent");
  CHECK(!air_rssi(150, &rssi), "150ft silent");
  CHECK_EQ(air_distance_ft(0, 150), 150, "1 and 3 span");
  CHECK_EQ(air_distance_ft(0, 75), 75, "1 and 2");
  CHECK_EQ(air_distance_ft(75, 150), 75, "2 and 3");
  pass("falloff: linked under 100ft, silent at 100+");
}

static void test_mesh_hear(void) {
  ram_hal_t ra, rb, rc;
  hal_t ha, hb, hc;
  ram_hal_init(&ra, &ha);
  ram_hal_init(&rb, &hb);
  ram_hal_init(&rc, &hc);
  int fa = 0, fb = 75, fc = 150;
  air_mesh_t mesh;
  memset(&mesh, 0, sizeof mesh);
  air_mesh_add(&mesh, &ra, &fa, NULL);
  air_mesh_add(&mesh, &rb, &fb, NULL);
  air_mesh_add(&mesh, &rc, &fc, NULL);
  air_mesh_link(&mesh);

  uint8_t ping[3] = {1, 2, 3};
  ha.radio_send_raw(ha.ctx, ping, sizeof ping);
  uint8_t buf[16], len = 0;
  CHECK(poll_raw_hal(&hb, buf, &len), "B hears A");
  CHECK(!poll_raw_hal(&hc, buf, &len), "C does not hear A");

  hc.radio_send_raw(hc.ctx, ping, sizeof ping);
  CHECK(poll_raw_hal(&hb, buf, &len), "B hears C");
  CHECK(!poll_raw_hal(&ha, buf, &len), "A does not hear C");
  pass("mesh: 0 and 150 do not hear; 75 does");
}

static void make_dir_unit(emu_unit_t *u, ram_hal_t *ram, hal_t *hal, beeper_t *b, const char *name) {
  static uint32_t rng = 1001;
  ram_hal_init(ram, hal);
  ram->rng = rng++;
  beeper_init(b);
  beeper_reset(b, hal);
  u->beeper = b;
  str_copy_max(u->name, sizeof u->name, name, EMU_NAME_MAX - 1);
  u->hal = hal;
}

static void test_directory(void) {
  ram_hal_t ra, rb, rc;
  hal_t ha, hb, hc;
  static beeper_t ba, bb, bc;
  emu_unit_t ua, ub, uc;
  make_dir_unit(&ua, &ra, &ha, &ba, "A");
  make_dir_unit(&ub, &rb, &hb, &bb, "B");
  make_dir_unit(&uc, &rc, &hc, &bc, "C");
  emu_unit_t three[3] = {ua, ub, uc};
  seed_emulator_directory(three, 3);

  CHECK_EQ(three[0].beeper->contacts.n, 2, "A has two contacts");
  CHECK_EQ(three[1].beeper->contacts.n, 2, "B has two contacts");
  CHECK_EQ(three[2].beeper->contacts.n, 2, "C has two contacts");

  uint8_t id_a[NODE_ID_LEN], id_b[NODE_ID_LEN];
  identity_get_node_id(&three[0].beeper->identity, id_a);
  identity_get_node_id(&three[1].beeper->identity, id_b);
  const contact_t *ab = contacts_find_by_id(&three[0].beeper->contacts, id_b);
  const contact_t *ba_c = contacts_find_by_id(&three[1].beeper->contacts, id_a);
  CHECK(ab && ba_c, "A-B contacts");
  CHECK(strcmp(ab->name, "B") == 0, "A sees B");
  CHECK(strcmp(ba_c->name, "A") == 0, "B sees A");
  CHECK_EQ(SESSION_KEY_LEN, 16, "key len");
  CHECK(memcmp(ab->session_key, ba_c->session_key, SESSION_KEY_LEN) == 0, "A-B share a session key");
  pass("emulator directory: each unit has the other two");
}

static void test_introduce_forget(void) {
  ram_hal_t ra, rb, rc, rd;
  hal_t ha, hb, hc, hd;
  static beeper_t ba, bb, bc, bd;
  emu_unit_t ua, ub, uc, ud;
  make_dir_unit(&ua, &ra, &ha, &ba, "A");
  make_dir_unit(&ub, &rb, &hb, &bb, "B");
  make_dir_unit(&uc, &rc, &hc, &bc, "C");
  make_dir_unit(&ud, &rd, &hd, &bd, "D");
  emu_unit_t three[3] = {ua, ub, uc};
  seed_emulator_directory(three, 3);

  uint8_t id_b[NODE_ID_LEN];
  identity_get_node_id(&three[1].beeper->identity, id_b);
  const contact_t *ab = contacts_find_by_id(&three[0].beeper->contacts, id_b);
  CHECK(ab, "A has B");
  uint8_t key_ab[SESSION_KEY_LEN];
  memcpy(key_ab, ab->session_key, SESSION_KEY_LEN);

  introduce_emulator_unit(three, 3, &ud);
  CHECK_EQ(three[0].beeper->contacts.n, 3, "A has B C D");
  CHECK_EQ(ud.beeper->contacts.n, 3, "D has A B C");
  const contact_t *after = contacts_find_by_id(&three[0].beeper->contacts, id_b);
  CHECK(after, "A still has B");
  CHECK(memcmp(after->session_key, key_ab, SESSION_KEY_LEN) == 0, "existing A-B key unchanged");

  forget_emulator_unit(three, 3, &ud);
  CHECK_EQ(three[0].beeper->contacts.n, 2, "A lost D");
  uint8_t id_d[NODE_ID_LEN];
  identity_get_node_id(&ud.beeper->identity, id_d);
  CHECK(contacts_find_by_id(&three[0].beeper->contacts, id_d) == NULL, "no D in A");
  pass("introduce a fourth radio into every contact book; forget drops it");
}

static void boot_unit(beeper_t *b, ram_hal_t *ram, hal_t *hal, uint32_t rng, const char *name) {
  ram_hal_init(ram, hal);
  ram->rng = rng;
  ram->now_ms = 1000;
  beeper_init(b);
  beeper_tick(b, hal);
  identity_set_display_name(&b->identity, name);
}

static void start_add(beeper_t *b, const hal_t *hal) {
  pair_start(&b->pair, &b->identity, hal, b->pager.beeple);
  b->pager.screen = SCREEN_PAIR;
}

static void test_repair_returns(void) {
  ram_hal_t ra, rb;
  hal_t ha, hb;
  beeper_t ba, bb;
  boot_unit(&ba, &ra, &ha, 1, "Ada");
  boot_unit(&bb, &rb, &hb, 2, "Bea");
  int fa = 0, fb = 0;
  air_mesh_t mesh;
  memset(&mesh, 0, sizeof mesh);
  air_mesh_add(&mesh, &ra, &fa, NULL);
  air_mesh_add(&mesh, &rb, &fb, NULL);
  air_mesh_link(&mesh);

  emu_unit_t units[2];
  memset(units, 0, sizeof units);
  units[0].beeper = &ba;
  units[0].hal = &ha;
  units[1].beeper = &bb;
  units[1].hal = &hb;
  str_copy_max(units[0].name, sizeof units[0].name, "Ada", 3);
  str_copy_max(units[1].name, sizeof units[1].name, "Bea", 3);
  seed_emulator_directory(units, 2);
  CHECK_EQ(ba.contacts.n, 1, "already paired");

  start_add(&ba, &ha);
  start_add(&bb, &hb);
  bool done = false;
  for (uint32_t t = 16; t <= 3000; t += 16) {
    ra.now_ms = rb.now_ms = 1000 + t;
    beeper_tick(&ba, &ha);
    beeper_tick(&bb, &hb);
    if (!pair_active(&ba.pair) && !pair_active(&bb.pair)) {
      done = true;
      break;
    }
  }
  CHECK(done, "repeat pair finishes");
  CHECK(!pair_error(&ba.pair) && !pair_error(&bb.pair), "repeat pair has no error");
  CHECK(ba.pager.screen == SCREEN_PEEPLE && bb.pager.screen == SCREEN_PEEPLE, "both leave ADD");
  CHECK_EQ(ba.contacts.n, 1, "book size unchanged");
  uint8_t id_b[NODE_ID_LEN];
  identity_get_node_id(&bb.identity, id_b);
  const contact_t *ab = contacts_find_by_id(&ba.contacts, id_b);
  uint8_t id_a[NODE_ID_LEN];
  identity_get_node_id(&ba.identity, id_a);
  const contact_t *ba_row = contacts_find_by_id(&bb.contacts, id_a);
  CHECK(ab && ba_row, "contact rows remain");
  CHECK(memcmp(ab->session_key, ba_row->session_key, SESSION_KEY_LEN) == 0, "keys still match");
  pass("repeat pair returns to the menu");
}

static void step_at(beeper_t *ba, beeper_t *bb, ram_hal_t *ra, ram_hal_t *rb, const hal_t *ha,
                    const hal_t *hb, uint32_t t) {
  ra->now_ms = rb->now_ms = 1000 + t;
  beeper_tick(ba, ha);
  beeper_tick(bb, hb);
}

static void test_restart_add_new_ephemeral(void) {
  ram_hal_t ra, rb;
  hal_t ha, hb;
  beeper_t ba, bb;
  boot_unit(&ba, &ra, &ha, 11, "Ada");
  boot_unit(&bb, &rb, &hb, 12, "Bea");
  int fa = 0, fb = 0;
  air_mesh_t mesh;
  memset(&mesh, 0, sizeof mesh);
  air_mesh_add(&mesh, &ra, &fa, NULL);
  air_mesh_add(&mesh, &rb, &fb, NULL);
  air_mesh_link(&mesh);

  start_add(&ba, &ha);
  start_add(&bb, &hb);

  uint32_t t = 0;
  bool acked = false;
  for (t = 16; t <= 2000; t += 16) {
    step_at(&ba, &bb, &ra, &rb, &ha, &hb, t);
    if (ba.pair.they_have_us && bb.pair.they_have_us && pair_active(&ba.pair) &&
        pair_active(&bb.pair)) {
      acked = true;
      break;
    }
  }
  CHECK(acked, "both sides have hello_ack");

  uint32_t burst_now = ba.pair.last_hello_at + HELLO_PERIOD_MS;
  while (1000 + t + 32 < burst_now) {
    t += 16;
    step_at(&ba, &bb, &ra, &rb, &ha, &hb, t);
    CHECK(pair_active(&ba.pair) && pair_active(&bb.pair), "handshake still open");
  }
  CHECK_EQ(1000 + t + 16, burst_now - 16, "next tick is the one before the burst");

  uint8_t old_eph[EPH_LEN];
  memcpy(old_eph, bb.pair.our_eph, EPH_LEN);
  uint8_t id_b[NODE_ID_LEN];
  identity_get_node_id(&bb.identity, id_b);
  pair_abort(&bb.pair, &hb);
  start_add(&bb, &hb);
  CHECK(pair_active(&bb.pair), "add restarted");
  CHECK(memcmp(bb.pair.our_eph, old_eph, EPH_LEN) != 0, "restart mints a new ephemeral");
  uint8_t id_now[NODE_ID_LEN];
  identity_get_node_id(&bb.identity, id_now);
  CHECK(node_id_equal(id_b, id_now), "node id unchanged");
  CHECK(!bb.pair.they_have_us, "restart clears the local flag");

  t += 16;
  step_at(&ba, &bb, &ra, &rb, &ha, &hb, t);
  CHECK(pair_active(&bb.pair), "new hello is on the air");
  CHECK(!pair_error(&bb.pair), "no error before the peer burst");

  t += 16;
  step_at(&ba, &bb, &ra, &rb, &ha, &hb, t);
  const char *err_b = pair_error(&bb.pair);
  CHECK(!err_b || strcmp(err_b, "try again") != 0, "peer burst does not abort the restart");
  CHECK(pair_active(&bb.pair), "restart survives the peer burst");
  CHECK(ba.pair.has_pending && memcmp(ba.pair.pending.hello.eph, bb.pair.our_eph, EPH_LEN) == 0,
        "peer took the new hello");
  CHECK(!ba.pair.they_have_us, "new ephemeral drops the old ack");

  bool finished = false;
  for (t += 16; t <= 20000; t += 16) {
    step_at(&ba, &bb, &ra, &rb, &ha, &hb, t);
    err_b = pair_error(&bb.pair);
    CHECK(!err_b || strcmp(err_b, "try again") != 0, "later burst does not abort the restart");
    if (!pair_active(&ba.pair) && !pair_active(&bb.pair)) {
      finished = true;
      break;
    }
  }
  CHECK(finished, "new handshake finishes");
  CHECK(!pair_error(&ba.pair) && !pair_error(&bb.pair), "restarted add has no error");
  CHECK(ba.pager.screen == SCREEN_PEEPLE && bb.pager.screen == SCREEN_PEEPLE, "both leave add");
  const contact_t *ab = contacts_find_by_id(&ba.contacts, id_b);
  uint8_t id_a[NODE_ID_LEN];
  identity_get_node_id(&ba.identity, id_a);
  const contact_t *row = contacts_find_by_id(&bb.contacts, id_a);
  CHECK(ab && row, "contacts stored");
  CHECK(memcmp(ab->session_key, row->session_key, SESSION_KEY_LEN) == 0, "keys match");
  pass("restart add with a new ephemeral still pairs");
}

int main(void) {
  test_falloff();
  test_mesh_hear();
  test_directory();
  test_introduce_forget();
  test_repair_returns();
  test_restart_add_new_ephemeral();
  printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
