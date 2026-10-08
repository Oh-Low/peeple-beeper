/** Runnable page receipt/relay tests. `npm run test:page` — no browser. */

import { contactsUpsert, createContactBook, SESSION_KEY_LEN, type ContactBook } from "./contacts";
import {
  FRAME_PAGE,
  FRAME_PAGE_ACK,
  frameDecodePage,
  frameDecodePageAck,
  frameEncodePage,
  frameEncodePageAck,
  frameType,
  MAC_LEN,
  PAGE_NONCE_LEN,
} from "./frames";
import {
  DISPLAY_BUF_LEN,
  NVM_BYTES,
  type Hal,
  type Page,
  type RawFrame,
} from "./hal";
import {
  createIdentityStore,
  NODE_ID_LEN,
  type IdentityStore,
} from "./identity";
import {
  createPageRadio,
  PAGE_ATTEMPTS,
  PAGE_RETRY_FAST_MS,
  PAGE_RETRY_SLOW_MS,
  pageSendStatus,
  pageTick,
  pageTx,
  type PageRadio,
} from "./page";

const RELAY_JITTER_MAX_MS = 400;

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

function idByte(n: number): Uint8Array {
  return new Uint8Array(NODE_ID_LEN).fill(n);
}

function hex(bytes: Uint8Array): string {
  let s = "";
  for (let i = 0; i < bytes.length; i++) s += bytes[i]!.toString(16).padStart(2, "0");
  return s;
}

type Clock = { now: number };

type FakeUnit = {
  clock: Clock;
  radio: PageRadio;
  identity: IdentityStore;
  contacts: ContactBook;
  hal: Hal;
  sent: Uint8Array[];
  pages: Page[];
  pushRaw: (bytes: Uint8Array, rssi?: number) => void;
};

function createFakeUnit(nodeId: Uint8Array, clock: Clock): FakeUnit {
  const rawRx: RawFrame[] = [];
  const sent: Uint8Array[] = [];
  const pages: Page[] = [];
  const identity = createIdentityStore();
  identity.nodeId = new Uint8Array(nodeId);
  const contacts = createContactBook();
  const radio = createPageRadio();
  const display = new Uint8Array(DISPLAY_BUF_LEN);
  let rng = 1;

  const hal: Hal = {
    displayClear: () => {
      display.fill(0);
    },
    displaySetPixel: () => {},
    displayBuffer: () => display,
    displayFlush: () => {},
    encoderPoll: () => "none",
    backPoll: () => false,
    radioPollRx: () => false,
    radioSend: () => {},
    randomFill: (buf) => {
      for (let i = 0; i < buf.length; i++) {
        buf[i] = rng & 0xff;
        rng += 1;
      }
    },
    radioSetWhisper: () => {},
    radioWhisper: () => false,
    radioSendRaw: (bytes) => {
      if (bytes.length === 0) return;
      sent.push(bytes.slice());
    },
    radioPollRaw: (out) => {
      const f = rawRx.shift();
      if (!f) return false;
      out.bytes = f.bytes.slice();
      out.rssi = f.rssi;
      out.receivedMs = f.receivedMs;
      return true;
    },
    nvmLoad: (_out) => 0,
    nvmSave: (bytes) => bytes.length <= NVM_BYTES,
    nowMs: () => clock.now,
    clock: () => ({ hh: 0, mm: 0 }),
    batteryPct: () => 100,
    vibe: () => {},
    beep: () => {},
  };

  return {
    clock,
    radio,
    identity,
    contacts,
    hal,
    sent,
    pages,
    pushRaw: (bytes, rssi = -30) => {
      rawRx.push({ bytes: bytes.slice(), rssi, receivedMs: clock.now });
    },
  };
}

function tick(u: FakeUnit): void {
  pageTick(u.radio, u.identity, u.contacts, u.hal, (p) => {
    u.pages.push(p);
  });
}

function pairBooks(a: FakeUnit, b: FakeUnit, nameA: string, nameB: string): void {
  const key = new Uint8Array(SESSION_KEY_LEN).fill(9);
  const idA = a.identity.nodeId!;
  const idB = b.identity.nodeId!;
  assert(contactsUpsert(a.contacts, { nodeId: idB, name: nameB, sessionKey: key, pairedMs: 0 }), "upsert B in A");
  assert(contactsUpsert(b.contacts, { nodeId: idA, name: nameA, sessionKey: key, pairedMs: 0 }), "upsert A in B");
}

function dummyPage(destId: Uint8Array, srcId: Uint8Array, hopTtl: number, nonce?: Uint8Array): Uint8Array {
  const frame = frameEncodePage({
    destId,
    srcId,
    nonce: nonce ?? new Uint8Array(PAGE_NONCE_LEN),
    hopTtl,
    body: new Uint8Array(0),
    mac: new Uint8Array(MAC_LEN),
  });
  assert(frame, "encode PAGE");
  return frame;
}

function dummyAck(destId: Uint8Array, srcId: Uint8Array, hopTtl: number, nonce?: Uint8Array): Uint8Array {
  const frame = frameEncodePageAck({
    destId,
    srcId,
    nonce: nonce ?? new Uint8Array(PAGE_NONCE_LEN),
    hopTtl,
    mac: new Uint8Array(MAC_LEN),
  });
  assert(frame, "encode PAGE_ACK");
  return frame;
}

function takeSent(u: FakeUnit): Uint8Array[] {
  return u.sent.splice(0);
}

test("dest decrypts PAGE, ACKs, delivered; onPage once if repeated", () => {
  const clock: Clock = { now: 0 };
  const a = createFakeUnit(idByte(1), clock);
  const b = createFakeUnit(idByte(2), clock);
  pairBooks(a, b, "Alice", "Bob");

  assert(pageTx(a.radio, a.hal, idByte(2), "hi", a.identity, a.contacts), "pageTx");
  assertEq(pageSendStatus(a.radio), "sending", "status after tx");
  const pageFrames = takeSent(a);
  assertEq(pageFrames.length, 1, "one PAGE on first tx");
  assertEq(frameType(pageFrames[0]!), FRAME_PAGE, "first frame is PAGE");

  b.pushRaw(pageFrames[0]!);
  tick(b);
  assertEq(b.pages.length, 1, "onPage once");
  assertEq(b.pages[0]!.from, "Alice", "from contact name");
  assertEq(b.pages[0]!.body, "hi", "body");
  const acks = takeSent(b);
  assertEq(acks.length, 1, "one PAGE_ACK");
  assertEq(frameType(acks[0]!), FRAME_PAGE_ACK, "ACK type");

  a.pushRaw(acks[0]!);
  tick(a);
  assertEq(pageSendStatus(a.radio), "delivered", "sender delivered");

  b.pushRaw(pageFrames[0]!);
  tick(b);
  assertEq(b.pages.length, 1, "repeat PAGE does not onPage again");
  const acks2 = takeSent(b);
  assertEq(acks2.length, 1, "still ACK on repeat");
});

test("no ACK: 10s then 60s retries, unique nonces, fail after last wait", () => {
  const clock: Clock = { now: 0 };
  const a = createFakeUnit(idByte(1), clock);
  const b = createFakeUnit(idByte(2), clock);
  pairBooks(a, b, "Alice", "Bob");

  assert(pageTx(a.radio, a.hal, idByte(2), "ping", a.identity, a.contacts), "pageTx");
  tick(a);

  const txAt: number[] = [0];
  let t = 0;
  for (let i = 1; i < PAGE_ATTEMPTS; i++) {
    t += i < 4 ? PAGE_RETRY_FAST_MS : PAGE_RETRY_SLOW_MS;
    txAt.push(t);
    clock.now = t;
    tick(a);
  }

  const frames = takeSent(a);
  assertEq(frames.length, PAGE_ATTEMPTS, "9 TX");
  const decoded = frames.map((f) => frameDecodePage(f));
  for (let i = 0; i < decoded.length; i++) assert(decoded[i], `PAGE ${i}`);
  const nonce0 = hex(decoded[0]!.nonce);
  for (let i = 1; i < decoded.length; i++) {
    const ni = hex(decoded[i]!.nonce);
    assert(ni !== nonce0, `nonce tx${i + 1} differs from tx1`);
    for (let j = 0; j < i; j++) {
      assert(ni !== hex(decoded[j]!.nonce), `nonce tx${i + 1} unique`);
    }
  }
  assertEq(pageSendStatus(a.radio), "sending", "still sending after last TX");
  assertEq(txAt[3], 30_000, "4th TX at 30s");
  assertEq(txAt[4], 90_000, "5th TX at 90s");
  assertEq(txAt[8], 330_000, "9th TX at 330s");

  clock.now = t + PAGE_RETRY_SLOW_MS;
  tick(a);
  assertEq(pageSendStatus(a.radio), "failed", "failed after last ACK wait");
  assertEq(takeSent(a).length, 0, "no 10th TX");
});

test("retry new nonce: dest onPage once; ACK of first nonce still delivers", () => {
  const clock: Clock = { now: 0 };
  const a = createFakeUnit(idByte(1), clock);
  const b = createFakeUnit(idByte(2), clock);
  pairBooks(a, b, "Alice", "Bob");

  assert(pageTx(a.radio, a.hal, idByte(2), "hi", a.identity, a.contacts), "pageTx");
  const first = takeSent(a);
  assertEq(first.length, 1, "first TX");

  b.pushRaw(first[0]!);
  tick(b);
  assertEq(b.pages.length, 1, "onPage first copy");
  assertEq(b.pages[0]!.body, "hi", "body without msg_id");
  const ack1 = takeSent(b);
  assertEq(ack1.length, 1, "ACK nonce 1");

  clock.now = PAGE_RETRY_FAST_MS;
  tick(a);
  const second = takeSent(a);
  assertEq(second.length, 1, "retry TX");
  const p0 = frameDecodePage(first[0]!);
  const p1 = frameDecodePage(second[0]!);
  assert(p0 && p1, "decode");
  assert(hex(p0.nonce) !== hex(p1.nonce), "retry nonce differs");

  b.pushRaw(second[0]!);
  tick(b);
  assertEq(b.pages.length, 1, "retry does not onPage again");
  assertEq(takeSent(b).length, 1, "ACK nonce 2");

  a.pushRaw(ack1[0]!);
  tick(a);
  assertEq(pageSendStatus(a.radio), "delivered", "ACK of first nonce counts");
});

test("relay PAGE: ttl--, one TX; already-sent hear does not TX again; ttl=1 dropped", () => {
  const clock: Clock = { now: 0 };
  const c = createFakeUnit(idByte(3), clock);
  const dest = idByte(2);
  const src = idByte(1);
  const nonce = new Uint8Array(PAGE_NONCE_LEN).fill(0xab);

  c.pushRaw(dummyPage(dest, src, 3, nonce));
  tick(c);
  assertEq(takeSent(c).length, 0, "no TX before jitter");
  clock.now = RELAY_JITTER_MAX_MS;
  tick(c);
  const relayed = takeSent(c);
  assertEq(relayed.length, 1, "one relay TX");
  const p = frameDecodePage(relayed[0]!);
  assert(p, "relayed PAGE");
  assertEq(p.hopTtl, 2, "ttl decremented");
  assertEq(hex(p.nonce), hex(nonce), "nonce unchanged");

  const secondHear = clock.now;
  c.pushRaw(dummyPage(dest, src, 3, nonce));
  tick(c);
  clock.now = secondHear + RELAY_JITTER_MAX_MS;
  tick(c);
  assertEq(takeSent(c).length, 0, "already-sent hear does not TX again");

  const c3 = createFakeUnit(idByte(3), clock);
  clock.now = 0;
  c3.pushRaw(dummyPage(dest, src, 1, nonce));
  tick(c3);
  clock.now = RELAY_JITTER_MAX_MS;
  tick(c3);
  assertEq(takeSent(c3).length, 0, "ttl=1 dropped");
});

test("relay PAGE: second hear before TX re-rolls, still sends once", () => {
  const clock: Clock = { now: 0 };
  const c = createFakeUnit(idByte(3), clock);
  const dest = idByte(2);
  const src = idByte(1);
  const nonce = new Uint8Array(PAGE_NONCE_LEN).fill(0xab);

  c.pushRaw(dummyPage(dest, src, 3, nonce));
  tick(c);
  assertEq(takeSent(c).length, 0, "no TX after first hear");

  clock.now = 50;
  c.pushRaw(dummyPage(dest, src, 3, nonce));
  tick(c);
  assertEq(takeSent(c).length, 0, "no TX after second hear");

  clock.now = 50 + RELAY_JITTER_MAX_MS;
  tick(c);
  const relayed = takeSent(c);
  assertEq(relayed.length, 1, "one PAGE relay after re-roll");
  assertEq(frameType(relayed[0]!), FRAME_PAGE, "relayed PAGE");
  const p = frameDecodePage(relayed[0]!);
  assert(p, "decode relayed PAGE");
  assertEq(p.hopTtl, 2, "ttl decremented");
  assertEq(hex(p.nonce), hex(nonce), "nonce unchanged");
});

test("relay PAGE: PAGE_ACK cancels pending PAGE", () => {
  const clock: Clock = { now: 0 };
  const c = createFakeUnit(idByte(3), clock);
  const dest = idByte(2);
  const src = idByte(1);
  const nonce = new Uint8Array(PAGE_NONCE_LEN).fill(0xab);

  c.pushRaw(dummyPage(dest, src, 3, nonce));
  tick(c);
  assertEq(takeSent(c).length, 0, "no TX before ACK");

  c.pushRaw(dummyAck(src, dest, 3, nonce));
  tick(c);

  clock.now = RELAY_JITTER_MAX_MS;
  tick(c);
  const sent = takeSent(c);
  assertEq(sent.filter((f) => frameType(f) === FRAME_PAGE).length, 0, "PAGE_ACK cancels pending PAGE");
});

test("relay PAGE: ACK first then PAGE does not schedule PAGE relay", () => {
  const clock: Clock = { now: 0 };
  const c = createFakeUnit(idByte(3), clock);
  const dest = idByte(2);
  const src = idByte(1);
  const nonce = new Uint8Array(PAGE_NONCE_LEN).fill(0xab);

  c.pushRaw(dummyAck(src, dest, 3, nonce));
  tick(c);

  c.pushRaw(dummyPage(dest, src, 3, nonce));
  tick(c);

  clock.now = RELAY_JITTER_MAX_MS;
  tick(c);
  const sent = takeSent(c);
  assertEq(sent.filter((f) => frameType(f) === FRAME_PAGE).length, 0, "no PAGE relay after ACK-first");
});

test("relay ACK: duplicate ACK before TX re-rolls, still sends once", () => {
  const clock: Clock = { now: 0 };
  const c = createFakeUnit(idByte(3), clock);
  const ackDest = idByte(1);
  const ackSrc = idByte(2);

  c.pushRaw(dummyAck(ackDest, ackSrc, 3));
  tick(c);
  assertEq(takeSent(c).length, 0, "no ACK TX after first hear");

  clock.now = 50;
  c.pushRaw(dummyAck(ackDest, ackSrc, 3));
  tick(c);
  assertEq(takeSent(c).length, 0, "no ACK TX after second hear");

  clock.now = 50 + RELAY_JITTER_MAX_MS;
  tick(c);
  const relayed = takeSent(c);
  assertEq(relayed.length, 1, "one ACK relay after re-roll");
  assertEq(frameType(relayed[0]!), FRAME_PAGE_ACK, "relayed PAGE_ACK");
  const ack = frameDecodePageAck(relayed[0]!);
  assert(ack, "decode relayed ACK");
  assertEq(ack.hopTtl, 2, "ttl decremented");
});

test("relay: different nonce is forwarded again", () => {
  const clock: Clock = { now: 0 };
  const c = createFakeUnit(idByte(3), clock);
  const dest = idByte(2);
  const src = idByte(1);
  const n1 = new Uint8Array(PAGE_NONCE_LEN).fill(0xab);
  const n2 = new Uint8Array(PAGE_NONCE_LEN).fill(0xcd);

  c.pushRaw(dummyPage(dest, src, 3, n1));
  tick(c);
  clock.now = RELAY_JITTER_MAX_MS;
  tick(c);
  assertEq(takeSent(c).length, 1, "relay first nonce");

  c.pushRaw(dummyPage(dest, src, 3, n2));
  tick(c);
  clock.now = RELAY_JITTER_MAX_MS * 2;
  tick(c);
  const relayed = takeSent(c);
  assertEq(relayed.length, 1, "relay second nonce");
  const p = frameDecodePage(relayed[0]!);
  assert(p, "decode");
  assertEq(hex(p.nonce), hex(n2), "second nonce");
  assertEq(p.hopTtl, 2, "ttl decremented");
});

console.log(`${passed} passed, ${failed} failed`);
if (failed > 0) throw new Error("page tests failed");
