/** Host range + directory tests. `npx tsx src/host/sim_test.ts` */

import { contactsFindById, contactsList } from "../firmware/contacts";
import { beeperReset, createBeeper } from "../firmware/beeper";
import { frameEncodePage, MAC_LEN, PAGE_NONCE_LEN } from "../firmware/frames";
import { identityGetNodeId, NODE_ID_LEN } from "../firmware/identity";
import { formatAirLog } from "./airlog";
import { airDistanceFt, airRssi, linkAirPair, linkMesh, RADIO_RANGE_FT } from "./air";
import { createHostHal } from "./createHostHal";
import {
  forgetEmulatorUnit,
  introduceEmulatorUnit,
  seedEmulatorDirectory,
} from "./seed";
import type { RawFrame } from "../firmware/hal";

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

function pollRaw(host: ReturnType<typeof createHostHal>): Uint8Array | undefined {
  const out: RawFrame = { bytes: new Uint8Array(0), rssi: 0, receivedMs: 0 };
  if (!host.hal.radioPollRaw(out)) return undefined;
  return out.bytes;
}

test("falloff: linked under 100ft, silent at 100+", () => {
  assertEq(airRssi(0), -30, "at origin");
  assert(airRssi(75) !== null, "75ft still on air");
  assertEq(airRssi(RADIO_RANGE_FT), null, "100ft silent");
  assertEq(airRssi(150), null, "150ft silent");
  assertEq(airDistanceFt(0, 150), 150, "1 and 3 span");
  assertEq(airDistanceFt(0, 75), 75, "1 and 2");
  assertEq(airDistanceFt(75, 150), 75, "2 and 3");
});

test("mesh: 0 and 150 do not hear; 75 does", () => {
  const pos = { a: 0, b: 75, c: 150 };
  const a = createHostHal();
  const b = createHostHal();
  const c = createHostHal();
  linkMesh([
    ...linkAirPair(a, b, () => airRssi(airDistanceFt(pos.a, pos.b))),
    ...linkAirPair(a, c, () => airRssi(airDistanceFt(pos.a, pos.c))),
    ...linkAirPair(b, c, () => airRssi(airDistanceFt(pos.b, pos.c))),
  ]);

  const ping = new Uint8Array([1, 2, 3]);
  a.hal.radioSendRaw(ping);
  assert(pollRaw(b), "B hears A");
  assertEq(pollRaw(c), undefined, "C does not hear A");

  c.hal.radioSendRaw(ping);
  assert(pollRaw(b), "B hears C");
  assertEq(pollRaw(a), undefined, "A does not hear C");
});

test("emulator directory: each unit has the other two", () => {
  const a = { host: createHostHal(), beeper: createBeeper() };
  const b = { host: createHostHal(), beeper: createBeeper() };
  const c = { host: createHostHal(), beeper: createBeeper() };
  beeperReset(a.beeper, a.host.hal);
  beeperReset(b.beeper, b.host.hal);
  beeperReset(c.beeper, c.host.hal);
  seedEmulatorDirectory([
    { beeper: a.beeper, name: "A", hal: a.host.hal },
    { beeper: b.beeper, name: "B", hal: b.host.hal },
    { beeper: c.beeper, name: "C", hal: c.host.hal },
  ]);

  assertEq(contactsList(a.beeper.contacts).length, 2, "A has two contacts");
  assertEq(contactsList(b.beeper.contacts).length, 2, "B has two contacts");
  assertEq(contactsList(c.beeper.contacts).length, 2, "C has two contacts");

  const idB = identityGetNodeId(b.beeper.identity);
  const idA = identityGetNodeId(a.beeper.identity);
  const ab = contactsFindById(a.beeper.contacts, idB);
  const ba = contactsFindById(b.beeper.contacts, idA);
  assert(ab && ba, "A-B contacts");
  assertEq(ab.name, "B", "A sees B");
  assertEq(ba.name, "A", "B sees A");
  assertEq(ab.sessionKey.length, 16, "key len");
  assert(
    ab.sessionKey.every((byte, i) => byte === ba.sessionKey[i]),
    "A-B share a session key",
  );
});

test("introduce a fourth radio into every contact book; forget drops it", () => {
  const a = { host: createHostHal(), beeper: createBeeper() };
  const b = { host: createHostHal(), beeper: createBeeper() };
  const c = { host: createHostHal(), beeper: createBeeper() };
  const d = { host: createHostHal(), beeper: createBeeper() };
  beeperReset(a.beeper, a.host.hal);
  beeperReset(b.beeper, b.host.hal);
  beeperReset(c.beeper, c.host.hal);
  beeperReset(d.beeper, d.host.hal);
  const three = [
    { beeper: a.beeper, name: "A", hal: a.host.hal },
    { beeper: b.beeper, name: "B", hal: b.host.hal },
    { beeper: c.beeper, name: "C", hal: c.host.hal },
  ];
  seedEmulatorDirectory(three);
  const keyAB = contactsFindById(a.beeper.contacts, identityGetNodeId(b.beeper.identity))!.sessionKey;
  introduceEmulatorUnit(three, { beeper: d.beeper, name: "D", hal: d.host.hal });

  assertEq(contactsList(a.beeper.contacts).length, 3, "A has B C D");
  assertEq(contactsList(d.beeper.contacts).length, 3, "D has A B C");
  const after = contactsFindById(a.beeper.contacts, identityGetNodeId(b.beeper.identity));
  assert(after, "A still has B");
  assert(
    after.sessionKey.every((byte, i) => byte === keyAB[i]),
    "existing A-B key unchanged",
  );

  forgetEmulatorUnit(three, { beeper: d.beeper, name: "D", hal: d.host.hal });
  assertEq(contactsList(a.beeper.contacts).length, 2, "A lost D");
  assertEq(contactsFindById(a.beeper.contacts, identityGetNodeId(d.beeper.identity)), undefined, "no D in A");
});

test("air log: TX/RX recorded; PAGE filter hides other", () => {
  const host = createHostHal();
  const page = frameEncodePage({
    destId: new Uint8Array(NODE_ID_LEN).fill(2),
    srcId: new Uint8Array(NODE_ID_LEN).fill(1),
    nonce: new Uint8Array(PAGE_NONCE_LEN).fill(3),
    hopTtl: 3,
    body: new Uint8Array(0),
    mac: new Uint8Array(MAC_LEN),
  });
  assert(page, "encode");
  host.hal.radioSendRaw(page);
  host.pushRaw(page, -40);
  assertEq(host.airTx().length, 1, "one TX");
  assertEq(host.airRx().length, 1, "one RX");
  const shown = formatAirLog(host.airTx(), { page: true, ack: true, pair: true });
  assert(shown.includes("PAGE"), "PAGE in log");
  assert(shown.includes("ttl=3"), "ttl in log");
  const hidden = formatAirLog(host.airTx(), { page: false, ack: true, pair: true });
  assertEq(hidden, "", "PAGE filtered out");
});

console.log(`${passed} passed, ${failed} failed`);
if (failed > 0) throw new Error("sim tests failed");
