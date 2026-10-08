import { contactsDelete, contactsFindById, contactsUpsert, SESSION_KEY_LEN } from "../firmware/contacts";
import type { Beeper } from "../firmware/beeper";
import type { Hal } from "../firmware/hal";
import {
  identityGetDisplayName,
  identityGetNodeId,
  identityLoadOrCreate,
  identitySetDisplayName,
} from "../firmware/identity";

export type EmulatorUnit = {
  beeper: Beeper;
  name: string;
  hal: Hal;
};

function pairTwo(a: EmulatorUnit, b: EmulatorUnit): void {
  const key = new Uint8Array(SESSION_KEY_LEN);
  a.hal.randomFill(key);
  const idA = identityGetNodeId(a.beeper.identity);
  const idB = identityGetNodeId(b.beeper.identity);
  contactsUpsert(a.beeper.contacts, {
    nodeId: idB,
    name: identityGetDisplayName(b.beeper.identity),
    sessionKey: key,
    pairedMs: 0,
  });
  contactsUpsert(b.beeper.contacts, {
    nodeId: idA,
    name: identityGetDisplayName(a.beeper.identity),
    sessionKey: key,
    pairedMs: 0,
  });
}

function ensurePair(a: EmulatorUnit, b: EmulatorUnit): void {
  const idA = identityGetNodeId(a.beeper.identity);
  const idB = identityGetNodeId(b.beeper.identity);
  const aHas = contactsFindById(a.beeper.contacts, idB);
  const bHas = contactsFindById(b.beeper.contacts, idA);
  if (aHas && bHas) return;
  if (aHas && !bHas) {
    contactsUpsert(b.beeper.contacts, {
      nodeId: idA,
      name: identityGetDisplayName(a.beeper.identity),
      sessionKey: aHas.sessionKey,
      pairedMs: aHas.pairedMs,
    });
    return;
  }
  if (bHas && !aHas) {
    contactsUpsert(a.beeper.contacts, {
      nodeId: idB,
      name: identityGetDisplayName(b.beeper.identity),
      sessionKey: bHas.sessionKey,
      pairedMs: bHas.pairedMs,
    });
    return;
  }
  pairTwo(a, b);
}

/** Host-only: invent node ids, names, and pairwise session keys so every unit can page every other. */
export function seedEmulatorDirectory(units: EmulatorUnit[]): void {
  for (const u of units) {
    identityLoadOrCreate(u.beeper.identity, u.hal.randomFill);
    if (u.beeper.identity.displayName.length === 0) {
      identitySetDisplayName(u.beeper.identity, u.name);
    }
  }

  for (let i = 0; i < units.length; i++) {
    for (let j = i + 1; j < units.length; j++) {
      ensurePair(units[i]!, units[j]!);
    }
  }
}

/** Pair `newbie` with every existing unit. Does not rekey pairs already in the list. */
export function introduceEmulatorUnit(existing: EmulatorUnit[], newbie: EmulatorUnit): void {
  identityLoadOrCreate(newbie.beeper.identity, newbie.hal.randomFill);
  identitySetDisplayName(newbie.beeper.identity, newbie.name);
  for (let i = 0; i < existing.length; i++) {
    pairTwo(existing[i]!, newbie);
  }
}

/** Drop `gone` from every remaining contact book. */
export function forgetEmulatorUnit(remaining: EmulatorUnit[], gone: EmulatorUnit): void {
  if (!gone.beeper.identity.nodeId) return;
  const id = identityGetNodeId(gone.beeper.identity);
  for (let i = 0; i < remaining.length; i++) {
    contactsDelete(remaining[i]!.beeper.contacts, id);
  }
}
