import { PAGE_FROM_MAX } from "./hal";
import { NODE_ID_LEN, nodeIdEqual, nodeIdToHex } from "./identity";

export const SESSION_KEY_LEN = 16;
/** Fits comfortably in the 100KB NVM partition with inbox/outbox. */
export const CONTACTS_MAX = 256;

export type Contact = {
  nodeId: Uint8Array;
  name: string;
  sessionKey: Uint8Array;
  pairedMs: number;
};

export type ContactBook = {
  rows: Contact[];
};

/** Newest first: new ids unshift; replace keeps the existing slot. */
export function createContactBook(): ContactBook {
  return { rows: [] };
}

function copyContact(c: Contact): Contact {
  return {
    nodeId: new Uint8Array(c.nodeId),
    name: c.name,
    sessionKey: new Uint8Array(c.sessionKey),
    pairedMs: c.pairedMs,
  };
}

function clampName(name: string, nodeId: Uint8Array): string {
  const clipped = name.slice(0, PAGE_FROM_MAX);
  if (clipped.length > 0) return clipped;
  return nodeIdToHex(nodeId).slice(-4);
}

function indexOf(book: ContactBook, nodeId: Uint8Array): number {
  for (let i = 0; i < book.rows.length; i++) {
    const row = book.rows[i];
    if (row && nodeIdEqual(row.nodeId, nodeId)) return i;
  }
  return -1;
}

export function contactsInit(book: ContactBook): void {
  book.rows.length = 0;
}

export function contactsReset(book: ContactBook): void {
  contactsInit(book);
}

export function contactsFindById(book: ContactBook, nodeId: Uint8Array): Contact | undefined {
  const i = indexOf(book, nodeId);
  if (i < 0) return undefined;
  const row = book.rows[i];
  return row ? copyContact(row) : undefined;
}

export function contactsUpsert(book: ContactBook, c: Contact): boolean {
  if (c.nodeId.length !== NODE_ID_LEN) return false;
  if (c.sessionKey.length !== SESSION_KEY_LEN) return false;

  const row: Contact = {
    nodeId: new Uint8Array(c.nodeId),
    name: clampName(c.name, c.nodeId),
    sessionKey: new Uint8Array(c.sessionKey),
    pairedMs: c.pairedMs,
  };

  const i = indexOf(book, c.nodeId);
  if (i >= 0) {
    book.rows[i] = row;
    return true;
  }
  if (book.rows.length >= CONTACTS_MAX) return false;
  book.rows.unshift(row);
  return true;
}

export function contactsList(book: ContactBook): Contact[] {
  return book.rows.map(copyContact);
}

export function contactsDelete(book: ContactBook, nodeId: Uint8Array): boolean {
  const i = indexOf(book, nodeId);
  if (i < 0) return false;
  book.rows.splice(i, 1);
  return true;
}
