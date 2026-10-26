#include "seed.h"

#include "contacts.h"
#include "identity.h"
#include "util.h"

#include <string.h>

static void pair_two(emu_unit_t *a, emu_unit_t *b) {
  uint8_t key[SESSION_KEY_LEN];
  a->hal->random_fill(a->hal->ctx, key, SESSION_KEY_LEN);
  uint8_t id_a[NODE_ID_LEN], id_b[NODE_ID_LEN];
  identity_get_node_id(&a->beeper->identity, id_a);
  identity_get_node_id(&b->beeper->identity, id_b);
  char name_a[PAGE_FROM_MAX + 1], name_b[PAGE_FROM_MAX + 1];
  identity_get_display_name(&a->beeper->identity, name_a, sizeof name_a);
  identity_get_display_name(&b->beeper->identity, name_b, sizeof name_b);

  contact_t ca, cb;
  memset(&ca, 0, sizeof ca);
  memset(&cb, 0, sizeof cb);
  memcpy(ca.node_id, id_b, NODE_ID_LEN);
  str_copy_max(ca.name, sizeof ca.name, name_b, PAGE_FROM_MAX);
  memcpy(ca.session_key, key, SESSION_KEY_LEN);
  memcpy(cb.node_id, id_a, NODE_ID_LEN);
  str_copy_max(cb.name, sizeof cb.name, name_a, PAGE_FROM_MAX);
  memcpy(cb.session_key, key, SESSION_KEY_LEN);
  contacts_upsert(&a->beeper->contacts, &ca);
  contacts_upsert(&b->beeper->contacts, &cb);
}

static void ensure_pair(emu_unit_t *a, emu_unit_t *b) {
  uint8_t id_a[NODE_ID_LEN], id_b[NODE_ID_LEN];
  identity_get_node_id(&a->beeper->identity, id_a);
  identity_get_node_id(&b->beeper->identity, id_b);
  const contact_t *a_has = contacts_find_by_id(&a->beeper->contacts, id_b);
  const contact_t *b_has = contacts_find_by_id(&b->beeper->contacts, id_a);
  if (a_has && b_has) return;
  if (a_has && !b_has) {
    contact_t cb;
    memset(&cb, 0, sizeof cb);
    memcpy(cb.node_id, id_a, NODE_ID_LEN);
    identity_get_display_name(&a->beeper->identity, cb.name, sizeof cb.name);
    memcpy(cb.session_key, a_has->session_key, SESSION_KEY_LEN);
    cb.paired_ms = a_has->paired_ms;
    contacts_upsert(&b->beeper->contacts, &cb);
    return;
  }
  if (b_has && !a_has) {
    contact_t ca;
    memset(&ca, 0, sizeof ca);
    memcpy(ca.node_id, id_b, NODE_ID_LEN);
    identity_get_display_name(&b->beeper->identity, ca.name, sizeof ca.name);
    memcpy(ca.session_key, b_has->session_key, SESSION_KEY_LEN);
    ca.paired_ms = b_has->paired_ms;
    contacts_upsert(&a->beeper->contacts, &ca);
    return;
  }
  pair_two(a, b);
}

void seed_emulator_directory(emu_unit_t *units, int n) {
  for (int i = 0; i < n; i++) {
    identity_load_or_create(&units[i].beeper->identity, units[i].hal);
    if (units[i].beeper->identity.display_name[0] == 0) {
      identity_set_display_name(&units[i].beeper->identity, units[i].name);
    }
  }
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) ensure_pair(&units[i], &units[j]);
  }
}

void introduce_emulator_unit(emu_unit_t *existing, int n, emu_unit_t *newbie) {
  identity_load_or_create(&newbie->beeper->identity, newbie->hal);
  identity_set_display_name(&newbie->beeper->identity, newbie->name);
  for (int i = 0; i < n; i++) pair_two(&existing[i], newbie);
}

void forget_emulator_unit(emu_unit_t *remaining, int n, emu_unit_t *gone) {
  if (!gone->beeper->identity.has_id) return;
  uint8_t id[NODE_ID_LEN];
  identity_get_node_id(&gone->beeper->identity, id);
  for (int i = 0; i < n; i++) contacts_delete(&remaining[i].beeper->contacts, id);
}
