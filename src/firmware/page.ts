/** Encrypted PAGE send/receive after contacts exist. HELLO frames are pair.ts. */

import { contactsFindById, type ContactBook } from "./contacts";
import { pageAckCheck, pageAckMac, pageOpen, pageSeal } from "./crypto";
import {
  FRAME_HELLO,
  FRAME_HELLO_ACK,
  FRAME_PAGE,
  FRAME_PAGE_ACK,
  FRAME_PAIR_LEAVE,
  PAGE_MSG_ID_LEN,
  PAGE_NONCE_LEN,
  frameDecodePage,
  frameDecodePageAck,
  frameEncodePage,
  frameEncodePageAck,
  frameSetHopTtl,
  frameType,
  type PageAckPayload,
  type PagePayload,
} from "./frames";
import { PAGE_BODY_MAX, type Hal, type Page, type RawFrame } from "./hal";
import { identityGetNodeId, nodeIdEqual, type IdentityStore } from "./identity";

/** Originator hop_ttl: origin plus up to two relay repeats. */
export const PAGE_HOP_TTL = 3;

/** TX at 0/10/20/30s, then every 60s for 5 minutes (9 TX). Fail after the last ACK wait. */
export const PAGE_RETRY_FAST_MS = 10_000;
export const PAGE_RETRY_SLOW_MS = 60_000;
export const PAGE_ATTEMPTS_FAST = 4;
export const PAGE_ATTEMPTS_SLOW = 5;
export const PAGE_ATTEMPTS = PAGE_ATTEMPTS_FAST + PAGE_ATTEMPTS_SLOW;

const SEEN_MAX = 24;
const RELAY_JITTER_MIN_MS = 80;
const RELAY_JITTER_MAX_MS = 400;

export type PageSendStatus = "idle" | "sending" | "delivered" | "failed";

type Outbox = {
  destId: Uint8Array;
  msgId: Uint8Array;
  nonces: Uint8Array[];
  attempts: number;
  nextAt: number;
};

type SeenEntry = {
  kind: number;
  srcId: Uint8Array;
  nonce: Uint8Array;
  pending: Uint8Array | null;
  pendingAt: number;
};

type Delivered = {
  srcId: Uint8Array;
  msgId: Uint8Array;
};

export type PageRadio = {
  status: PageSendStatus;
  outbox: Outbox | null;
  destId: Uint8Array | null;
  body: string;
  seen: SeenEntry[];
  delivered: Delivered[];
};

export function createPageRadio(): PageRadio {
  return {
    status: "idle",
    outbox: null,
    destId: null,
    body: "",
    seen: [],
    delivered: [],
  };
}

export function pageRadioReset(radio: PageRadio): void {
  radio.status = "idle";
  radio.outbox = null;
  radio.destId = null;
  radio.body = "";
  radio.seen.length = 0;
  radio.delivered.length = 0;
}

export function pageBusy(radio: PageRadio): boolean {
  return radio.status === "sending";
}

export function pageSendStatus(radio: PageRadio): PageSendStatus {
  return radio.status;
}

export function pageClearStatus(radio: PageRadio): void {
  if (radio.status === "delivered" || radio.status === "failed") {
    radio.status = "idle";
  }
}

export function pageOutboxTo(radio: PageRadio): Uint8Array | undefined {
  return radio.destId ?? undefined;
}

export function pageOutboxBody(radio: PageRadio): string {
  return radio.body;
}

export function pageOutboxAttempts(radio: PageRadio): number {
  return radio.outbox ? radio.outbox.attempts : 0;
}

function elapsedU32(now: number, start: number): number {
  return (now - start) >>> 0;
}

function timeReached(now: number, deadline: number): boolean {
  return elapsedU32(now, deadline) < 0x80000000;
}

function nonceUsed(box: Outbox, nonce: Uint8Array): boolean {
  for (let i = 0; i < box.nonces.length; i++) {
    const n = box.nonces[i];
    if (n && bytesEq(n, nonce)) return true;
  }
  return false;
}

function deliveredFind(
  radio: PageRadio,
  srcId: Uint8Array,
  msgId: Uint8Array,
): boolean {
  for (let i = 0; i < radio.delivered.length; i++) {
    const e = radio.delivered[i]!;
    if (nodeIdEqual(e.srcId, srcId) && bytesEq(e.msgId, msgId)) return true;
  }
  return false;
}

function deliveredInsert(radio: PageRadio, srcId: Uint8Array, msgId: Uint8Array): void {
  radio.delivered.push({
    srcId: new Uint8Array(srcId),
    msgId: new Uint8Array(msgId),
  });
  if (radio.delivered.length > SEEN_MAX) radio.delivered.shift();
}

function bytesEq(a: Uint8Array, b: Uint8Array): boolean {
  const n = a.length;
  if (n !== b.length) return false;
  for (let i = 0; i < n; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}

/** After TX 1–3 wait 10s; after TX 4–9 wait 60s (last wait is ACK-only). */
function waitAfterAttempt(attempts: number): number {
  if (attempts < PAGE_ATTEMPTS_FAST) return PAGE_RETRY_FAST_MS;
  return PAGE_RETRY_SLOW_MS;
}

function relayJitterMs(hal: Hal): number {
  const r = new Uint8Array(2);
  hal.randomFill(r);
  const u = (r[0]! << 8) | r[1]!;
  const span = RELAY_JITTER_MAX_MS - RELAY_JITTER_MIN_MS;
  return RELAY_JITTER_MIN_MS + (u % (span + 1));
}

function seenFind(
  radio: PageRadio,
  kind: number,
  srcId: Uint8Array,
  nonce: Uint8Array,
): SeenEntry | undefined {
  for (let i = 0; i < radio.seen.length; i++) {
    const e = radio.seen[i]!;
    if (e.kind === kind && nodeIdEqual(e.srcId, srcId) && bytesEq(e.nonce, nonce)) {
      return e;
    }
  }
  return undefined;
}

function seenInsert(
  radio: PageRadio,
  kind: number,
  srcId: Uint8Array,
  nonce: Uint8Array,
): SeenEntry {
  const e: SeenEntry = {
    kind,
    srcId: new Uint8Array(srcId),
    nonce: new Uint8Array(nonce),
    pending: null,
    pendingAt: 0,
  };
  radio.seen.push(e);
  if (radio.seen.length > SEEN_MAX) radio.seen.shift();
  return e;
}

function markSeen(
  radio: PageRadio,
  kind: number,
  srcId: Uint8Array,
  nonce: Uint8Array,
): SeenEntry {
  return seenFind(radio, kind, srcId, nonce) ?? seenInsert(radio, kind, srcId, nonce);
}

function relayDeadline(hal: Hal): number {
  return (hal.nowMs() + relayJitterMs(hal)) >>> 0;
}

function armRelay(entry: SeenEntry, hal: Hal, hopTtl: number, bytes: Uint8Array): void {
  if (hopTtl <= 1) return;
  const copy = new Uint8Array(bytes);
  if (!frameSetHopTtl(copy, hopTtl - 1)) return;
  entry.pending = copy;
  entry.pendingAt = relayDeadline(hal);
}

function rerollPending(entry: SeenEntry, hal: Hal): void {
  if (!entry.pending) return;
  entry.pendingAt = relayDeadline(hal);
}

function cancelPendingPageForAck(radio: PageRadio, ack: PageAckPayload): void {
  const existing = seenFind(radio, FRAME_PAGE, ack.destId, ack.nonce);
  if (existing) existing.pending = null;
}

function considerPageRelay(
  radio: PageRadio,
  hal: Hal,
  p: PagePayload,
  bytes: Uint8Array,
): void {
  if (seenFind(radio, FRAME_PAGE_ACK, p.destId, p.nonce)) {
    markSeen(radio, FRAME_PAGE, p.srcId, p.nonce);
    return;
  }
  const existing = seenFind(radio, FRAME_PAGE, p.srcId, p.nonce);
  if (existing) {
    rerollPending(existing, hal);
    return;
  }
  const entry = seenInsert(radio, FRAME_PAGE, p.srcId, p.nonce);
  armRelay(entry, hal, p.hopTtl, bytes);
}

function considerAckRelay(
  radio: PageRadio,
  hal: Hal,
  ack: PageAckPayload,
  bytes: Uint8Array,
): void {
  cancelPendingPageForAck(radio, ack);
  const existing = seenFind(radio, FRAME_PAGE_ACK, ack.srcId, ack.nonce);
  if (existing) {
    rerollPending(existing, hal);
    return;
  }
  const entry = seenInsert(radio, FRAME_PAGE_ACK, ack.srcId, ack.nonce);
  armRelay(entry, hal, ack.hopTtl, bytes);
}

function sendPageAck(
  radio: PageRadio,
  hal: Hal,
  identity: IdentityStore,
  destId: Uint8Array,
  nonce: Uint8Array,
  sessionKey: Uint8Array,
): void {
  const us = identityGetNodeId(identity);
  const frame = frameEncodePageAck({
    destId,
    srcId: us,
    nonce,
    hopTtl: PAGE_HOP_TTL,
    mac: pageAckMac(sessionKey, nonce),
  });
  if (!frame) return;
  markSeen(radio, FRAME_PAGE_ACK, us, nonce);
  hal.radioSendRaw(frame);
}

function deliverPage(
  radio: PageRadio,
  identity: IdentityStore,
  contacts: ContactBook,
  hal: Hal,
  raw: RawFrame,
  p: PagePayload,
  onPage: (page: Page) => void,
): void {
  const contact = contactsFindById(contacts, p.srcId);
  if (!contact) return;

  const plain = pageOpen(contact.sessionKey, p.nonce, p.body, p.mac);
  if (!plain) return;
  if (plain.length < PAGE_MSG_ID_LEN) return;

  const msgId = plain.subarray(0, PAGE_MSG_ID_LEN);
  const text = new TextDecoder().decode(plain.subarray(PAGE_MSG_ID_LEN));

  if (!deliveredFind(radio, p.srcId, msgId)) {
    deliveredInsert(radio, p.srcId, msgId);
    onPage({
      from: contact.name,
      body: text,
      rssi: raw.rssi,
      receivedMs: raw.receivedMs,
    });
  }

  sendPageAck(radio, hal, identity, p.srcId, p.nonce, contact.sessionKey);
}

function tryOutboxAck(
  radio: PageRadio,
  identity: IdentityStore,
  contacts: ContactBook,
  ack: PageAckPayload,
): void {
  const box = radio.outbox;
  if (!box || radio.status !== "sending") return;

  const us = identityGetNodeId(identity);
  if (!nodeIdEqual(ack.destId, us)) return;
  if (!nodeIdEqual(ack.srcId, box.destId)) return;
  if (!nonceUsed(box, ack.nonce)) return;

  const contact = contactsFindById(contacts, box.destId);
  if (!contact) return;
  if (!pageAckCheck(contact.sessionKey, ack.nonce, ack.mac)) return;

  radio.outbox = null;
  radio.status = "delivered";
}

function onRawFrame(
  radio: PageRadio,
  identity: IdentityStore,
  contacts: ContactBook,
  hal: Hal,
  raw: RawFrame,
  onPage: (page: Page) => void,
): void {
  const kind = frameType(raw.bytes);
  if (kind === FRAME_HELLO || kind === FRAME_HELLO_ACK || kind === FRAME_PAIR_LEAVE) {
    return;
  }

  const us = identityGetNodeId(identity);

  if (kind === FRAME_PAGE) {
    const p = frameDecodePage(raw.bytes);
    if (!p) return;

    if (nodeIdEqual(p.destId, us)) {
      deliverPage(radio, identity, contacts, hal, raw, p, onPage);
      return;
    }
    if (nodeIdEqual(p.srcId, us)) return;
    considerPageRelay(radio, hal, p, raw.bytes);
    return;
  }

  if (kind === FRAME_PAGE_ACK) {
    const ack = frameDecodePageAck(raw.bytes);
    if (!ack) return;

    if (nodeIdEqual(ack.destId, us)) {
      tryOutboxAck(radio, identity, contacts, ack);
      return;
    }
    considerAckRelay(radio, hal, ack, raw.bytes);
  }
}

function txAttempt(
  radio: PageRadio,
  hal: Hal,
  identity: IdentityStore,
  contacts: ContactBook,
  box: Outbox,
): boolean {
  const contact = contactsFindById(contacts, box.destId);
  if (!contact) return false;

  const nonce = new Uint8Array(PAGE_NONCE_LEN);
  hal.randomFill(nonce);

  const text = new TextEncoder().encode(radio.body).subarray(0, PAGE_BODY_MAX);
  const plaintext = new Uint8Array(PAGE_MSG_ID_LEN + text.length);
  plaintext.set(box.msgId, 0);
  plaintext.set(text, PAGE_MSG_ID_LEN);

  const sealed = pageSeal(contact.sessionKey, nonce, plaintext);
  const us = identityGetNodeId(identity);
  const frame = frameEncodePage({
    destId: box.destId,
    srcId: us,
    nonce,
    hopTtl: PAGE_HOP_TTL,
    body: sealed.cipher,
    mac: sealed.mac,
  });
  if (!frame) return false;

  box.nonces.push(new Uint8Array(nonce));
  markSeen(radio, FRAME_PAGE, us, nonce);
  hal.radioSendRaw(frame);
  return true;
}

function outboxTick(
  radio: PageRadio,
  identity: IdentityStore,
  contacts: ContactBook,
  hal: Hal,
): void {
  const box = radio.outbox;
  if (!box || radio.status !== "sending") return;
  const now = hal.nowMs();
  if (!timeReached(now, box.nextAt)) return;

  if (box.attempts >= PAGE_ATTEMPTS) {
    radio.outbox = null;
    radio.status = "failed";
    return;
  }

  if (!txAttempt(radio, hal, identity, contacts, box)) {
    radio.outbox = null;
    radio.status = "failed";
    return;
  }
  box.attempts += 1;
  box.nextAt = (now + waitAfterAttempt(box.attempts)) >>> 0;
}

function relayTick(radio: PageRadio, hal: Hal): void {
  const now = hal.nowMs();
  for (let i = 0; i < radio.seen.length; i++) {
    const e = radio.seen[i]!;
    if (!e.pending) continue;
    if (!timeReached(now, e.pendingAt)) continue;
    hal.radioSendRaw(e.pending);
    e.pending = null;
  }
}

export function pageTx(
  radio: PageRadio,
  hal: Hal,
  destId: Uint8Array,
  body: string,
  identity: IdentityStore,
  contacts: ContactBook,
): boolean {
  if (pageBusy(radio)) return false;

  const contact = contactsFindById(contacts, destId);
  if (!contact) return false;

  const msgId = new Uint8Array(PAGE_MSG_ID_LEN);
  hal.randomFill(msgId);

  const destCopy = new Uint8Array(destId);
  const box: Outbox = {
    destId: destCopy,
    msgId,
    nonces: [],
    attempts: 0,
    nextAt: 0,
  };
  radio.status = "sending";
  radio.destId = destCopy;
  radio.body = body;
  radio.outbox = box;

  if (!txAttempt(radio, hal, identity, contacts, box)) {
    radio.status = "idle";
    radio.outbox = null;
    radio.destId = null;
    radio.body = "";
    return false;
  }
  box.attempts = 1;
  box.nextAt = (hal.nowMs() + waitAfterAttempt(1)) >>> 0;
  return true;
}

export function pageTick(
  radio: PageRadio,
  identity: IdentityStore,
  contacts: ContactBook,
  hal: Hal,
  onPage: (p: Page) => void,
): void {
  const raw: RawFrame = { bytes: new Uint8Array(0), rssi: 0, receivedMs: 0 };

  while (hal.radioPollRaw(raw)) {
    onRawFrame(radio, identity, contacts, hal, raw, onPage);
  }

  outboxTick(radio, identity, contacts, hal);
  relayTick(radio, hal);
}

export function pageRxTick(
  radio: PageRadio,
  identity: IdentityStore,
  contacts: ContactBook,
  hal: Hal,
  onPage: (p: Page) => void,
): void {
  pageTick(radio, identity, contacts, hal, onPage);
}
