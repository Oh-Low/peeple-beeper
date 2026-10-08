/** Whisper pairing: HELLO, HELLO_ACK, then PAIR_LEAVE. Stay until both sides leave. */

import { contactsUpsert, type ContactBook } from "./contacts";
import { pairConfirmHash, pairDeriveSessionKey } from "./crypto";
import {
  EPH_LEN,
  FRAME_HELLO,
  FRAME_HELLO_ACK,
  FRAME_PAIR_LEAVE,
  HELLO_NONCE_LEN,
  frameDecodeHello,
  frameDecodeHelloAck,
  frameDecodePairLeave,
  frameEncodeHello,
  frameEncodeHelloAck,
  frameEncodePairLeave,
  frameType,
  type HelloAckPayload,
  type HelloPayload,
  type PairLeavePayload,
} from "./frames";
import type { Hal, RawFrame } from "./hal";
import {
  identityGetDisplayName,
  identityGetNodeId,
  identityLoadOrCreate,
  nodeIdEqual,
  type IdentityStore,
} from "./identity";

export const PAIR_WINDOW_MS = 20000;
export const HELLO_PERIOD_MS = 400;
/** Touching vs ~2 m. Host air uses this gate; tunable. */
export const PAIR_RSSI_MIN = -45;

type PairError = "timeout" | "book full" | "try again";

type Pending = {
  helloBytes: Uint8Array;
  hello: HelloPayload;
  rssi: number;
};

export type PairSession = {
  active: boolean;
  startMs: number;
  lastHelloAt: number;
  helloSent: boolean;
  ourEph: Uint8Array | null;
  ourHelloBytes: Uint8Array | null;
  pending: Pending | null;
  /** Peer sent HELLO_ACK: they have our hello. */
  theyHaveUs: boolean;
  lastRssi: number | undefined;
  lastError: PairError | undefined;
};

export function createPairSession(): PairSession {
  return {
    active: false,
    startMs: 0,
    lastHelloAt: 0,
    helloSent: false,
    ourEph: null,
    ourHelloBytes: null,
    pending: null,
    theyHaveUs: false,
    lastRssi: undefined,
    lastError: undefined,
  };
}

function elapsedU32(now: number, start: number): number {
  return (now - start) >>> 0;
}

function bytesEq(a: Uint8Array, b: Uint8Array): boolean {
  const n = a.length;
  if (n !== b.length) return false;
  let d = 0;
  for (let i = 0; i < n; i++) d |= a[i]! ^ b[i]!;
  return d === 0;
}

function copyHello(hello: HelloPayload): HelloPayload {
  return {
    nodeId: new Uint8Array(hello.nodeId),
    eph: new Uint8Array(hello.eph),
    nonce: new Uint8Array(hello.nonce),
    name: hello.name,
  };
}

function confirmFor(session: PairSession): Uint8Array | undefined {
  if (!session.ourHelloBytes || !session.pending) return undefined;
  return pairConfirmHash(session.ourHelloBytes, session.pending.helloBytes);
}

function clearSession(session: PairSession): void {
  session.active = false;
  session.startMs = 0;
  session.lastHelloAt = 0;
  session.helloSent = false;
  session.ourEph = null;
  session.ourHelloBytes = null;
  session.pending = null;
  session.theyHaveUs = false;
  session.lastRssi = undefined;
}

function pairSendHello(session: PairSession, hal: Hal): void {
  if (!session.ourHelloBytes) return;
  hal.radioSendRaw(session.ourHelloBytes);
}

function pairSendAck(session: PairSession, identity: IdentityStore, hal: Hal): void {
  const confirm = confirmFor(session);
  if (!confirm || !session.pending) return;
  const ack = frameEncodeHelloAck({
    srcId: identityGetNodeId(identity),
    destId: session.pending.hello.nodeId,
    confirm,
  });
  if (ack) hal.radioSendRaw(ack);
}

function pairSendLeave(session: PairSession, identity: IdentityStore, hal: Hal): void {
  const confirm = confirmFor(session);
  if (!confirm || !session.pending) return;
  const leave = frameEncodePairLeave({
    srcId: identityGetNodeId(identity),
    destId: session.pending.hello.nodeId,
    confirm,
  });
  if (leave) hal.radioSendRaw(leave);
}

function pairBroadcast(session: PairSession, identity: IdentityStore, hal: Hal): void {
  pairSendHello(session, hal);
  if (session.pending) pairSendAck(session, identity, hal);
  if (session.pending && session.theyHaveUs) pairSendLeave(session, identity, hal);
  session.lastHelloAt = hal.nowMs();
  session.helloSent = true;
}

function pairOnHello(
  session: PairSession,
  identity: IdentityStore,
  hal: Hal,
  rawBytes: Uint8Array,
  hello: HelloPayload,
  rssi: number,
): void {
  if (!session.active || !session.ourHelloBytes) return;
  const us = identityGetNodeId(identity);
  if (nodeIdEqual(hello.nodeId, us)) return;

  if (session.pending && !nodeIdEqual(hello.nodeId, session.pending.hello.nodeId)) {
    session.theyHaveUs = false;
  }

  session.pending = {
    helloBytes: new Uint8Array(rawBytes),
    hello: copyHello(hello),
    rssi,
  };
  session.lastRssi = rssi;
  pairSendAck(session, identity, hal);
}

function pairComplete(session: PairSession, hal: Hal): void {
  pairAbort(session, hal);
  session.lastError = undefined;
  hal.vibe(200);
}

function pairCommit(
  session: PairSession,
  identity: IdentityStore,
  contacts: ContactBook,
  hal: Hal,
): boolean {
  if (!session.pending || !session.ourEph) return false;
  const us = identityGetNodeId(identity);
  const them = session.pending.hello.nodeId;
  const sessionKey = pairDeriveSessionKey(us, them, session.ourEph, session.pending.hello.eph);
  const ok = contactsUpsert(contacts, {
    nodeId: them,
    name: session.pending.hello.name,
    sessionKey,
    pairedMs: hal.nowMs(),
  });
  if (!ok) {
    pairAbort(session, hal);
    session.lastError = "book full";
    return false;
  }
  return true;
}

function ackMatches(
  session: PairSession,
  identity: IdentityStore,
  ack: HelloAckPayload,
): boolean {
  if (!session.pending) return false;
  const us = identityGetNodeId(identity);
  if (!nodeIdEqual(ack.destId, us)) return false;
  if (!nodeIdEqual(ack.srcId, session.pending.hello.nodeId)) return false;
  const confirm = confirmFor(session);
  return !!confirm && bytesEq(confirm, ack.confirm);
}

function pairOnAck(
  session: PairSession,
  identity: IdentityStore,
  ack: HelloAckPayload,
): void {
  if (!session.active || !session.ourHelloBytes || !session.pending) return;
  if (!ackMatches(session, identity, ack)) return;
  session.theyHaveUs = true;
}

function pairOnLeave(
  session: PairSession,
  identity: IdentityStore,
  contacts: ContactBook,
  hal: Hal,
  leave: PairLeavePayload,
): void {
  if (!session.active) return;

  const us = identityGetNodeId(identity);
  if (!nodeIdEqual(leave.destId, us)) return;

  if (!session.theyHaveUs || !session.pending) {
    pairAbort(session, hal);
    session.lastError = "try again";
    return;
  }
  if (!ackMatches(session, identity, leave)) return;

  pairSendLeave(session, identity, hal);
  if (!pairCommit(session, identity, contacts, hal)) return;
  pairComplete(session, hal);
}

export function pairStart(session: PairSession, identity: IdentityStore, hal: Hal): void {
  identityLoadOrCreate(identity, hal.randomFill);
  session.lastError = undefined;
  clearSession(session);

  hal.radioSetWhisper(true);

  const eph = new Uint8Array(EPH_LEN);
  const nonce = new Uint8Array(HELLO_NONCE_LEN);
  hal.randomFill(eph);
  hal.randomFill(nonce);

  const encoded = frameEncodeHello({
    nodeId: identityGetNodeId(identity),
    eph,
    nonce,
    name: identityGetDisplayName(identity),
  });
  if (!encoded) {
    hal.radioSetWhisper(false);
    return;
  }

  session.ourEph = eph;
  session.ourHelloBytes = encoded;
  session.startMs = hal.nowMs();
  session.lastHelloAt = 0;
  session.helloSent = false;
  session.active = true;
  hal.vibe(80);
}

export function pairTick(
  session: PairSession,
  identity: IdentityStore,
  contacts: ContactBook,
  hal: Hal,
): void {
  if (!session.active) return;

  const now = hal.nowMs();
  if (elapsedU32(now, session.startMs) >= PAIR_WINDOW_MS) {
    pairAbort(session, hal);
    session.lastError = "timeout";
    return;
  }

  if (!session.helloSent || elapsedU32(now, session.lastHelloAt) >= HELLO_PERIOD_MS) {
    pairBroadcast(session, identity, hal);
  }

  const incoming: RawFrame = { bytes: new Uint8Array(0), rssi: 0, receivedMs: 0 };
  while (hal.radioPollRaw(incoming)) {
    if (!session.active) continue;
    if (incoming.rssi < PAIR_RSSI_MIN) continue;
    session.lastRssi = incoming.rssi;

    const kind = frameType(incoming.bytes);
    if (kind === FRAME_HELLO) {
      const hello = frameDecodeHello(incoming.bytes);
      if (hello) pairOnHello(session, identity, hal, incoming.bytes, hello, incoming.rssi);
    } else if (kind === FRAME_HELLO_ACK) {
      const ack = frameDecodeHelloAck(incoming.bytes);
      if (ack) pairOnAck(session, identity, ack);
    } else if (kind === FRAME_PAIR_LEAVE) {
      const leave = frameDecodePairLeave(incoming.bytes);
      if (leave) pairOnLeave(session, identity, contacts, hal, leave);
    }
  }
}

export function pairAbort(session: PairSession, hal: Hal): void {
  hal.radioSetWhisper(false);
  clearSession(session);
}

export function pairActive(session: PairSession): boolean {
  return session.active;
}

export function pairDeadlineMs(session: PairSession): number {
  if (!session.active) return 0;
  return (session.startMs + PAIR_WINDOW_MS) >>> 0;
}

export function pairLastRssi(session: PairSession): number | undefined {
  return session.lastRssi;
}

export function pairError(session: PairSession): string | undefined {
  return session.lastError;
}

/** RAM + inactive. Pass `hal` to drop whisper; UI should `pairAbort` if a session is live. */
export function pairReset(session: PairSession, hal?: Hal): void {
  if (hal) hal.radioSetWhisper(false);
  session.lastError = undefined;
  clearSession(session);
}
