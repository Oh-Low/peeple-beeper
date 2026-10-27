#include "mesh.h"
#include "beeper.h"
#include "contacts.h"
#include "emu_hal.h"
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

static void make_dir_unit(emu_unit_t *u, emu_hal_t *ram, hal_t *hal, beeper_t *b, const char *name) {
  static uint32_t rng = 1001;
  emu_hal_init(ram, hal);
  ram->rng = rng++;
  beeper_init(b);
  beeper_reset(b, hal);
  u->beeper = b;
  str_copy_max(u->name, sizeof u->name, name, EMU_NAME_MAX - 1);
  u->hal = hal;
}

static void test_directory(void) {
  emu_hal_t ra, rb, rc;
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
  emu_hal_t ra, rb, rc, rd;
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

int main(void) {
  test_directory();
  test_introduce_forget();
  printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
