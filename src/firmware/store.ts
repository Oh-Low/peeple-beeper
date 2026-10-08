import { CANNED_SAVED_MAX } from "./canned";

import {
  CONTACTS_MAX,
  contactsList,
  contactsReset,
  contactsUpsert,
  SESSION_KEY_LEN,
  type ContactBook,
} from "./contacts";
import { NODE_ID_LEN, type IdentityStore } from "./identity";
import { NVM_BYTES, PAGE_BODY_MAX, PAGE_FROM_MAX, type Hal } from "./hal";
import { PAGE_KEEP_MAX, CLOCK_NONE, type InboxRow, type OutboxItem, type Pager } from "./pager";

const MAGIC0 = 0x50; // P
const MAGIC1 = 0x42; // B
const MAGIC2 = 0x30; // 0
const MAGIC3 = 0x31; // 1
const STORE_VERSION = 2;

const STATUS_SENDING = 0;
const STATUS_SENT = 1;
const STATUS_FAILED = 2;

export type StoreOwner = {
  identity: IdentityStore;
  contacts: ContactBook;
  pager: Pick<Pager, "inbox" | "outbox" | "canned">;
};

class Writer {
  buf: Uint8Array;
  o = 0;

  constructor(cap: number) {
    this.buf = new Uint8Array(cap);
  }

  remain(): number {
    return this.buf.length - this.o;
  }

  u8(n: number): boolean {
    if (this.remain() < 1) return false;
    this.buf[this.o++] = n & 0xff;
    return true;
  }

  u16(n: number): boolean {
    if (this.remain() < 2) return false;
    this.buf[this.o++] = n & 0xff;
    this.buf[this.o++] = (n >>> 8) & 0xff;
    return true;
  }

  u32(n: number): boolean {
    if (this.remain() < 4) return false;
    this.buf[this.o++] = n & 0xff;
    this.buf[this.o++] = (n >>> 8) & 0xff;
    this.buf[this.o++] = (n >>> 16) & 0xff;
    this.buf[this.o++] = (n >>> 24) & 0xff;
    return true;
  }

  raw(b: Uint8Array): boolean {
    if (this.remain() < b.length) return false;
    this.buf.set(b, this.o);
    this.o += b.length;
    return true;
  }

  str(s: string, maxChars: number): boolean {
    const t = new TextEncoder().encode(s).subarray(0, maxChars);
    if (t.length > 255) return false;
    if (!this.u8(t.length)) return false;
    return this.raw(t);
  }

  finish(): Uint8Array {
    return this.buf.subarray(0, this.o);
  }
}

class Reader {
  buf: Uint8Array;
  o = 0;
  ok = true;

  constructor(buf: Uint8Array) {
    this.buf = buf;
  }

  remain(): number {
    return this.buf.length - this.o;
  }

  u8(): number {
    if (this.remain() < 1) {
      this.ok = false;
      return 0;
    }
    return this.buf[this.o++]!;
  }

  u16(): number {
    const lo = this.u8();
    const hi = this.u8();
    return lo | (hi << 8);
  }

  u32(): number {
    const a = this.u8();
    const b = this.u8();
    const c = this.u8();
    const d = this.u8();
    return (a | (b << 8) | (c << 16) | (d << 24)) >>> 0;
  }

  raw(n: number): Uint8Array {
    if (n < 0 || this.remain() < n) {
      this.ok = false;
      return new Uint8Array(0);
    }
    const sl = this.buf.subarray(this.o, this.o + n);
    this.o += n;
    return sl;
  }

  str(maxChars: number): string {
    const n = this.u8();
    if (!this.ok || n > maxChars) {
      this.ok = false;
      return "";
    }
    return new TextDecoder().decode(this.raw(n));
  }
}

function i8(n: number): number {
  const x = Math.max(-128, Math.min(127, n | 0));
  return x < 0 ? x + 256 : x;
}

function fromI8(n: number): number {
  return n > 127 ? n - 256 : n;
}

function outboxStatusByte(s: OutboxItem["status"]): number {
  if (s === "sent") return STATUS_SENT;
  if (s === "failed") return STATUS_FAILED;
  return STATUS_SENDING;
}

function outboxStatusFrom(n: number): OutboxItem["status"] {
  if (n === STATUS_SENT) return "sent";
  if (n === STATUS_FAILED) return "failed";
  // In-flight TX is not restored; keep the row retryable.
  return "failed";
}

function encodeSlice(
  owner: StoreOwner,
  inbox: InboxRow[],
  outbox: OutboxItem[],
): Uint8Array | undefined {
  const id = owner.identity.nodeId;
  if (!id || id.length !== NODE_ID_LEN) return new Uint8Array(0);

  const w = new Writer(NVM_BYTES);
  if (!w.u8(MAGIC0) || !w.u8(MAGIC1) || !w.u8(MAGIC2) || !w.u8(MAGIC3)) return undefined;
  if (!w.u8(STORE_VERSION)) return undefined;
  if (!w.raw(id)) return undefined;
  if (!w.str(owner.identity.displayName, PAGE_FROM_MAX)) return undefined;

  const contacts = contactsList(owner.contacts);
  if (contacts.length > CONTACTS_MAX) return undefined;
  if (!w.u16(contacts.length)) return undefined;
  for (let i = 0; i < contacts.length; i++) {
    const c = contacts[i]!;
    if (!w.raw(c.nodeId) || !w.str(c.name, PAGE_FROM_MAX)) return undefined;
    if (!w.raw(c.sessionKey) || !w.u32(c.pairedMs >>> 0)) return undefined;
  }

  const saved = owner.pager.canned.saved;
  const cannedN = saved.length > CANNED_SAVED_MAX ? CANNED_SAVED_MAX : saved.length;
  if (!w.u8(cannedN)) return undefined;
  for (let i = 0; i < cannedN; i++) {
    if (!w.str(saved[i]!, PAGE_BODY_MAX)) return undefined;
  }

  if (!w.u16(inbox.length)) return undefined;
  for (let i = 0; i < inbox.length; i++) {
    const p = inbox[i]!;
    if (!w.str(p.from, PAGE_FROM_MAX) || !w.str(p.body, PAGE_BODY_MAX)) return undefined;
    if (!w.u8(i8(p.rssi)) || !w.u32(p.receivedMs >>> 0)) return undefined;
    if (!w.u8(p.receivedHh) || !w.u8(p.receivedMm)) return undefined;
  }

  if (!w.u16(outbox.length)) return undefined;
  for (let i = 0; i < outbox.length; i++) {
    const row = outbox[i]!;
    if (!w.raw(row.destId) || !w.str(row.to, PAGE_FROM_MAX)) return undefined;
    if (!w.str(row.body, PAGE_BODY_MAX) || !w.u8(outboxStatusByte(row.status))) return undefined;
    if (!w.u8(row.sentHh) || !w.u8(row.sentMm)) return undefined;
    if (!w.u8(row.ackedHh) || !w.u8(row.ackedMm)) return undefined;
  }

  return w.finish();
}

/** Pack RAM into ≤ NVM_BYTES. Drops oldest inbox, then outbox, if needed. */
export function storeEncode(owner: StoreOwner): Uint8Array | undefined {
  const inbox = owner.pager.inbox.slice();
  const outbox = owner.pager.outbox.slice();
  for (;;) {
    const bytes = encodeSlice(owner, inbox, outbox);
    if (!bytes) return undefined;
    if (bytes.length <= NVM_BYTES) {
      owner.pager.inbox = inbox;
      owner.pager.outbox = outbox;
      return bytes;
    }
    if (inbox.length > 0) inbox.pop();
    else if (outbox.length > 0) outbox.pop();
    else return undefined;
  }
}

function hashBlob(bytes: Uint8Array): number {
  let h = 2166136261;
  for (let i = 0; i < bytes.length; i++) h = Math.imul(h ^ bytes[i]!, 16777619);
  return (h ^ bytes.length) >>> 0;
}

export function storeDecode(bytes: Uint8Array):
  | {
      nodeId: Uint8Array;
      displayName: string;
      contacts: {
        nodeId: Uint8Array;
        name: string;
        sessionKey: Uint8Array;
        pairedMs: number;
      }[];
      canned: string[];
      inbox: InboxRow[];
      outbox: OutboxItem[];
    }
  | undefined {
  if (bytes.length === 0) return undefined;
  const r = new Reader(bytes);
  if (r.u8() !== MAGIC0 || r.u8() !== MAGIC1 || r.u8() !== MAGIC2 || r.u8() !== MAGIC3) {
    return undefined;
  }
  const ver = r.u8();
  if (ver < 1 || ver > STORE_VERSION) return undefined;

  const nodeId = new Uint8Array(r.raw(NODE_ID_LEN));
  const displayName = r.str(PAGE_FROM_MAX);
  if (!r.ok) return undefined;

  const nContacts = r.u16();
  if (!r.ok || nContacts > CONTACTS_MAX) return undefined;
  const contacts: {
    nodeId: Uint8Array;
    name: string;
    sessionKey: Uint8Array;
    pairedMs: number;
  }[] = [];
  for (let i = 0; i < nContacts; i++) {
    const cid = new Uint8Array(r.raw(NODE_ID_LEN));
    const name = r.str(PAGE_FROM_MAX);
    const sessionKey = new Uint8Array(r.raw(SESSION_KEY_LEN));
    const pairedMs = r.u32();
    if (!r.ok) return undefined;
    contacts.push({ nodeId: cid, name, sessionKey, pairedMs });
  }

  const nCanned = r.u8();
  if (!r.ok || nCanned > CANNED_SAVED_MAX) return undefined;
  const canned: string[] = [];
  for (let i = 0; i < nCanned; i++) {
    const body = r.str(PAGE_BODY_MAX);
    if (!r.ok) return undefined;
    canned.push(body);
  }

  const nInbox = r.u16();
  if (!r.ok || nInbox > PAGE_KEEP_MAX) return undefined;
  const inbox: InboxRow[] = [];
  for (let i = 0; i < nInbox; i++) {
    const from = r.str(PAGE_FROM_MAX);
    const body = r.str(PAGE_BODY_MAX);
    const rssi = fromI8(r.u8());
    const receivedMs = r.u32();
    let receivedHh = 0;
    let receivedMm = 0;
    if (ver >= 2) {
      receivedHh = r.u8();
      receivedMm = r.u8();
    }
    if (!r.ok) return undefined;
    inbox.push({ from, body, rssi, receivedMs, receivedHh, receivedMm });
  }

  const nOut = r.u16();
  if (!r.ok || nOut > PAGE_KEEP_MAX) return undefined;
  const outbox: OutboxItem[] = [];
  for (let i = 0; i < nOut; i++) {
    const destId = new Uint8Array(r.raw(NODE_ID_LEN));
    const to = r.str(PAGE_FROM_MAX);
    const body = r.str(PAGE_BODY_MAX);
    const status = outboxStatusFrom(r.u8());
    let sentHh = 0;
    let sentMm = 0;
    let ackedHh = CLOCK_NONE;
    let ackedMm = CLOCK_NONE;
    if (ver >= 2) {
      sentHh = r.u8();
      sentMm = r.u8();
      ackedHh = r.u8();
      ackedMm = r.u8();
    }
    if (!r.ok) return undefined;
    outbox.push({ destId, to, body, status, sentHh, sentMm, ackedHh, ackedMm });
  }

  if (!r.ok || r.remain() !== 0) return undefined;
  return { nodeId, displayName, contacts, canned, inbox, outbox };
}

export function storeApply(owner: StoreOwner, bytes: Uint8Array): boolean {
  const snap = storeDecode(bytes);
  if (!snap) return false;

  owner.identity.nodeId = new Uint8Array(snap.nodeId);
  owner.identity.displayName = snap.displayName;

  contactsReset(owner.contacts);
  for (let i = 0; i < snap.contacts.length; i++) {
    contactsUpsert(owner.contacts, snap.contacts[i]!);
  }

  owner.pager.canned.saved = snap.canned.slice();
  owner.pager.inbox = snap.inbox.map((p) => ({
    from: p.from,
    body: p.body,
    rssi: p.rssi,
    receivedMs: p.receivedMs,
    receivedHh: p.receivedHh,
    receivedMm: p.receivedMm,
  }));
  owner.pager.outbox = snap.outbox.map((row) => ({
    destId: new Uint8Array(row.destId),
    to: row.to,
    body: row.body,
    status: row.status,
    sentHh: row.sentHh,
    sentMm: row.sentMm,
    ackedHh: row.ackedHh,
    ackedMm: row.ackedMm,
  }));
  return true;
}

export function storeLoad(owner: StoreOwner & { nvmHash: number }, hal: Hal): boolean {
  const buf = new Uint8Array(NVM_BYTES);
  const n = hal.nvmLoad(buf);
  if (n <= 0) return false;
  const slice = buf.subarray(0, n);
  if (!storeApply(owner, slice)) return false;
  storeRememberHash(owner, slice);
  return true;
}

export function storeSave(owner: StoreOwner, hal: Hal): boolean {
  if (!owner.identity.nodeId) {
    return hal.nvmSave(new Uint8Array(0));
  }
  const bytes = storeEncode(owner);
  if (!bytes) return false;
  return hal.nvmSave(bytes);
}

export function storeSync(
  owner: StoreOwner & { nvmHash: number },
  hal: Hal,
): void {
  if (!owner.identity.nodeId) return;
  const bytes = storeEncode(owner);
  if (!bytes) return;
  const h = hashBlob(bytes);
  if (h === owner.nvmHash) return;
  if (!hal.nvmSave(bytes)) return;
  owner.nvmHash = h;
}

export function storeRememberHash(owner: { nvmHash: number }, bytes: Uint8Array): void {
  owner.nvmHash = hashBlob(bytes);
}
