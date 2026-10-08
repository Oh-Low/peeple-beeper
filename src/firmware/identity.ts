import { PAGE_FROM_MAX } from "./hal";

/** 8-byte node_id. Loaded from NVM when present; otherwise invented at boot. */

export const NODE_ID_LEN = 8;

const HEX = "0123456789abcdef";

export type IdentityStore = {
  nodeId: Uint8Array | null;
  displayName: string;
};

export function createIdentityStore(): IdentityStore {
  return { nodeId: null, displayName: "" };
}

export function identityLoadOrCreate(
  store: IdentityStore,
  randomFill: (buf: Uint8Array) => void,
): void {
  if (store.nodeId) return;
  const id = new Uint8Array(NODE_ID_LEN);
  randomFill(id);
  store.nodeId = id;
}

/** Programmer error if called before identityLoadOrCreate. */
export function identityGetNodeId(store: IdentityStore): Uint8Array {
  if (!store.nodeId) throw new Error("identityLoadOrCreate first");
  return new Uint8Array(store.nodeId);
}

export function identityReset(store: IdentityStore): void {
  store.nodeId = null;
  store.displayName = "";
}

/** Name sent in HELLO. Empty falls back to last 4 hex of node id. */
export function identityGetDisplayName(store: IdentityStore): string {
  const clipped = (store.displayName ?? "").slice(0, PAGE_FROM_MAX);
  if (clipped.length > 0) return clipped;
  if (!store.nodeId) return "";
  return nodeIdToHex(store.nodeId).slice(-4);
}

export function identitySetDisplayName(store: IdentityStore, name: string): void {
  let out = "";
  for (const ch of name) {
    const code = ch.charCodeAt(0);
    if (code >= 32 && code <= 126) out += ch;
    if (out.length >= PAGE_FROM_MAX) break;
  }
  store.displayName = out;
}

export function nodeIdEqual(a: Uint8Array, b: Uint8Array): boolean {
  const n = a.length;
  if (n !== b.length) return false;
  for (let i = 0; i < n; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}

export function nodeIdToHex(id: Uint8Array): string {
  let s = "";
  for (let i = 0; i < id.length; i++) {
    const b = id[i];
    s += HEX[b >> 4] + HEX[b & 15];
  }
  return s;
}
