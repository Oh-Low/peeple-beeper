/** NVM store roundtrip. `npm run test:store` */

import { cannedAdd } from "./canned";
import { contactsList, contactsUpsert, SESSION_KEY_LEN } from "./contacts";
import { createBeeper } from "./beeper";
import { NVM_BYTES } from "./hal";
import { NODE_ID_LEN } from "./identity";
import { createHostHal } from "../host/createHostHal";
import { storeLoad, storeSave } from "./store";

let passed = 0;
let failed = 0;

function test(name: string, fn: () => void): void {
  try {
    fn();
    passed += 1;
    console.log(`ok  ${name}`);
  } catch (err) {
    failed += 1;
    console.error(`not ok  ${name}`);
    console.error(err);
  }
}

function assert(cond: unknown, msg: string): asserts cond {
  if (!cond) throw new Error(msg);
}

function assertEq(a: unknown, b: unknown, msg: string): void {
  if (a !== b) throw new Error(`${msg}: ${String(a)} !== ${String(b)}`);
}

function hex(bytes: Uint8Array): string {
  let s = "";
  for (let i = 0; i < bytes.length; i++) s += bytes[i]!.toString(16).padStart(2, "0");
  return s;
}

test("HAL nvmSave rejects over NVM_BYTES", () => {
  const host = createHostHal();
  assert(!host.hal.nvmSave(new Uint8Array(NVM_BYTES + 1)), "too big");
  assert(host.hal.nvmSave(new Uint8Array(0)), "empty ok");
  assert(host.hal.nvmSave(new Uint8Array(NVM_BYTES)), "exact cap ok");
});

test("store roundtrip: node_id, name, contacts, canned, inbox, outbox", () => {
  const host = createHostHal();
  const a = createBeeper();
  a.identity.nodeId = new Uint8Array(NODE_ID_LEN).fill(0x11);
  a.identity.displayName = "Ada";
  const peer = new Uint8Array(NODE_ID_LEN).fill(0x22);
  const key = new Uint8Array(SESSION_KEY_LEN).fill(0x33);
  assert(
    contactsUpsert(a.contacts, { nodeId: peer, name: "Bob", sessionKey: key, pairedMs: 9 }),
    "upsert",
  );
  assert(cannedAdd(a.pager.canned, "See you"), "canned");
  a.pager.inbox.push({
    from: "Bob",
    body: "hi",
    rssi: -40,
    receivedMs: 1000,
    receivedHh: 14,
    receivedMm: 32,
  });
  a.pager.outbox.push({
    destId: new Uint8Array(peer),
    to: "Bob",
    body: "yo",
    status: "sent",
    sentHh: 9,
    sentMm: 1,
    ackedHh: 9,
    ackedMm: 2,
  });
  a.pager.outbox.push({
    destId: new Uint8Array(peer),
    to: "Bob",
    body: "later",
    status: "sending",
    sentHh: 10,
    sentMm: 5,
    ackedHh: 255,
    ackedMm: 255,
  });

  assert(storeSave(a, host.hal), "save");

  const b = createBeeper();
  assert(storeLoad(b, host.hal), "load");
  assert(b.identity.nodeId, "restored id");
  assertEq(hex(b.identity.nodeId), hex(a.identity.nodeId!), "node_id");
  assertEq(b.identity.displayName, "Ada", "display name");
  const rows = contactsList(b.contacts);
  assertEq(rows.length, 1, "one contact");
  assertEq(rows[0]!.name, "Bob", "contact name");
  assertEq(hex(rows[0]!.sessionKey), hex(key), "session key");
  assertEq(b.pager.canned.saved[0], "See you", "canned");
  assertEq(b.pager.inbox.length, 1, "inbox");
  assertEq(b.pager.inbox[0]!.body, "hi", "inbox body");
  assertEq(b.pager.inbox[0]!.receivedHh, 14, "inbox hour");
  assertEq(b.pager.inbox[0]!.receivedMm, 32, "inbox minute");
  assertEq(b.pager.outbox.length, 2, "outbox");
  assertEq(b.pager.outbox[0]!.status, "sent", "sent kept");
  assertEq(b.pager.outbox[0]!.sentHh, 9, "sent hour");
  assertEq(b.pager.outbox[0]!.ackedMm, 2, "ack minute");
  assertEq(b.pager.outbox[1]!.status, "failed", "sending becomes failed");
  assertEq(b.pager.outbox[1]!.ackedHh, 255, "no ack clock");
});

console.log(`${passed} passed, ${failed} failed`);
if (failed > 0) throw new Error("store tests failed");
