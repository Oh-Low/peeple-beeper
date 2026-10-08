/**
 * Binary frames for whisper pairing and paging.
 * Version byte = 1 everywhere.
 *
 * HELLO:      type=0x01, ver, node_id[8], eph[32], nonce[8], name_len u8, name[name_len]
 *             name_len 0..PAGE_FROM_MAX (15). Total 51..66.
 * HELLO_ACK:  type=0x02, ver, src_id[8], dest_id[8], confirm[16]  total 34
 *             “I have your hello.”
 * PAIR_LEAVE: type=0x04, ver, src_id[8], dest_id[8], confirm[16]  total 34
 *             “I am leaving pairing.” Same body as ACK. Only sent after we
 *             have received HELLO_ACK. Receiving LEAVE without HELLO_ACK
 *             is a protocol error.
 * PAGE:       type=0x03, ver, dest[8], src[8], nonce[8], hop_ttl u8, body_len u8, body, mac[8]
 *   hop_ttl is not in the body MAC (pageSeal is sha256(key, nonce, cipher)).
 *   Sealed body plaintext is msg_id[8] || text. text 0..PAGE_BODY_MAX (79).
 *   body_len 0..PAGE_BODY_MAX+PAGE_MSG_ID_LEN. Max PAGE = 36+87 = 123, under 128.
 * PAGE_ACK:   type=0x05, ver, dest[8], src[8], nonce[8], hop_ttl u8, mac[8]  total 35
 *             dest = original PAGE src, src = original PAGE dest, nonce = that PAGE’s nonce.
 */

import { PAGE_BODY_MAX, PAGE_FROM_MAX } from "./hal";
import { NODE_ID_LEN } from "./identity";

export const FRAME_HELLO = 0x01 as const;
export const FRAME_HELLO_ACK = 0x02 as const;
export const FRAME_PAGE = 0x03 as const;
export const FRAME_PAIR_LEAVE = 0x04 as const;
export const FRAME_PAGE_ACK = 0x05 as const;
export const FRAME_VERSION = 1 as const;

export const EPH_LEN = 32 as const;
export const HELLO_NONCE_LEN = 8 as const;
export const CONFIRM_LEN = 16 as const;
export const PAGE_NONCE_LEN = 8 as const;
export const PAGE_MSG_ID_LEN = 8 as const;
export const MAC_LEN = 8 as const;

const HELLO_FIXED = 1 + 1 + NODE_ID_LEN + EPH_LEN + HELLO_NONCE_LEN + 1;
const HELLO_ACK_LEN = 1 + 1 + NODE_ID_LEN + NODE_ID_LEN + CONFIRM_LEN;
/** type, ver, dest, src, nonce, hop_ttl, body_len, mac — body is extra. */
const PAGE_FIXED = 1 + 1 + NODE_ID_LEN + NODE_ID_LEN + PAGE_NONCE_LEN + 1 + 1 + MAC_LEN;
const PAGE_ACK_LEN = 1 + 1 + NODE_ID_LEN + NODE_ID_LEN + PAGE_NONCE_LEN + 1 + MAC_LEN;
const PAGE_HOP_TTL_OFF = 2 + NODE_ID_LEN + NODE_ID_LEN + PAGE_NONCE_LEN;
const PAGE_WIRE_BODY_MAX = PAGE_BODY_MAX + PAGE_MSG_ID_LEN;

export type HelloPayload = {
  nodeId: Uint8Array;
  eph: Uint8Array;
  nonce: Uint8Array;
  name: string;
};

export type HelloAckPayload = {
  srcId: Uint8Array;
  destId: Uint8Array;
  confirm: Uint8Array;
};

/** Same fields as HELLO_ACK; type byte distinguishes leave. */
export type PairLeavePayload = HelloAckPayload;

export type PagePayload = {
  destId: Uint8Array;
  srcId: Uint8Array;
  nonce: Uint8Array;
  hopTtl: number;
  body: Uint8Array;
  mac: Uint8Array;
};

export type PageAckPayload = {
  destId: Uint8Array;
  srcId: Uint8Array;
  nonce: Uint8Array;
  hopTtl: number;
  mac: Uint8Array;
};

export function frameType(bytes: Uint8Array): number | undefined {
  if (bytes.length === 0) return undefined;
  return bytes[0];
}

export function frameEncodeHello(p: HelloPayload): Uint8Array | undefined {
  if (p.nodeId.length !== NODE_ID_LEN) return undefined;
  if (p.eph.length !== EPH_LEN) return undefined;
  if (p.nonce.length !== HELLO_NONCE_LEN) return undefined;

  const nameBytes = new TextEncoder().encode(p.name).subarray(0, PAGE_FROM_MAX);
  const out = new Uint8Array(HELLO_FIXED + nameBytes.length);
  let o = 0;
  out[o++] = FRAME_HELLO;
  out[o++] = FRAME_VERSION;
  out.set(p.nodeId, o);
  o += NODE_ID_LEN;
  out.set(p.eph, o);
  o += EPH_LEN;
  out.set(p.nonce, o);
  o += HELLO_NONCE_LEN;
  out[o++] = nameBytes.length;
  out.set(nameBytes, o);
  return out;
}

export function frameDecodeHello(bytes: Uint8Array): HelloPayload | undefined {
  if (bytes.length < HELLO_FIXED) return undefined;
  if (bytes[0] !== FRAME_HELLO || bytes[1] !== FRAME_VERSION) return undefined;

  let o = 2;
  const nodeId = bytes.slice(o, o + NODE_ID_LEN);
  o += NODE_ID_LEN;
  const eph = bytes.slice(o, o + EPH_LEN);
  o += EPH_LEN;
  const nonce = bytes.slice(o, o + HELLO_NONCE_LEN);
  o += HELLO_NONCE_LEN;
  const nameLen = bytes[o]!;
  o += 1;
  if (nameLen > PAGE_FROM_MAX) return undefined;
  if (bytes.length !== HELLO_FIXED + nameLen) return undefined;
  const name = new TextDecoder().decode(bytes.slice(o, o + nameLen));
  return { nodeId, eph, nonce, name };
}

export function frameEncodeHelloAck(p: HelloAckPayload): Uint8Array | undefined {
  if (p.srcId.length !== NODE_ID_LEN) return undefined;
  if (p.destId.length !== NODE_ID_LEN) return undefined;
  if (p.confirm.length !== CONFIRM_LEN) return undefined;

  const out = new Uint8Array(HELLO_ACK_LEN);
  let o = 0;
  out[o++] = FRAME_HELLO_ACK;
  out[o++] = FRAME_VERSION;
  out.set(p.srcId, o);
  o += NODE_ID_LEN;
  out.set(p.destId, o);
  o += NODE_ID_LEN;
  out.set(p.confirm, o);
  return out;
}

export function frameDecodeHelloAck(bytes: Uint8Array): HelloAckPayload | undefined {
  if (bytes.length !== HELLO_ACK_LEN) return undefined;
  if (bytes[0] !== FRAME_HELLO_ACK || bytes[1] !== FRAME_VERSION) return undefined;

  let o = 2;
  const srcId = bytes.slice(o, o + NODE_ID_LEN);
  o += NODE_ID_LEN;
  const destId = bytes.slice(o, o + NODE_ID_LEN);
  o += NODE_ID_LEN;
  const confirm = bytes.slice(o, o + CONFIRM_LEN);
  return { srcId, destId, confirm };
}

export function frameEncodePairLeave(p: PairLeavePayload): Uint8Array | undefined {
  if (p.srcId.length !== NODE_ID_LEN) return undefined;
  if (p.destId.length !== NODE_ID_LEN) return undefined;
  if (p.confirm.length !== CONFIRM_LEN) return undefined;

  const out = new Uint8Array(HELLO_ACK_LEN);
  let o = 0;
  out[o++] = FRAME_PAIR_LEAVE;
  out[o++] = FRAME_VERSION;
  out.set(p.srcId, o);
  o += NODE_ID_LEN;
  out.set(p.destId, o);
  o += NODE_ID_LEN;
  out.set(p.confirm, o);
  return out;
}

export function frameDecodePairLeave(bytes: Uint8Array): PairLeavePayload | undefined {
  if (bytes.length !== HELLO_ACK_LEN) return undefined;
  if (bytes[0] !== FRAME_PAIR_LEAVE || bytes[1] !== FRAME_VERSION) return undefined;

  let o = 2;
  const srcId = bytes.slice(o, o + NODE_ID_LEN);
  o += NODE_ID_LEN;
  const destId = bytes.slice(o, o + NODE_ID_LEN);
  o += NODE_ID_LEN;
  const confirm = bytes.slice(o, o + CONFIRM_LEN);
  return { srcId, destId, confirm };
}

function hopTtlByte(n: number): number | undefined {
  if (!Number.isInteger(n) || n < 0 || n > 255) return undefined;
  return n;
}

export function frameEncodePage(p: PagePayload): Uint8Array | undefined {
  if (p.destId.length !== NODE_ID_LEN) return undefined;
  if (p.srcId.length !== NODE_ID_LEN) return undefined;
  if (p.nonce.length !== PAGE_NONCE_LEN) return undefined;
  const hopTtl = hopTtlByte(p.hopTtl);
  if (hopTtl === undefined) return undefined;
  if (p.body.length > PAGE_WIRE_BODY_MAX) return undefined;
  if (p.mac.length !== MAC_LEN) return undefined;

  const bodyLen = p.body.length;
  const out = new Uint8Array(PAGE_FIXED + bodyLen);
  let o = 0;
  out[o++] = FRAME_PAGE;
  out[o++] = FRAME_VERSION;
  out.set(p.destId, o);
  o += NODE_ID_LEN;
  out.set(p.srcId, o);
  o += NODE_ID_LEN;
  out.set(p.nonce, o);
  o += PAGE_NONCE_LEN;
  out[o++] = hopTtl;
  out[o++] = bodyLen;
  out.set(p.body, o);
  o += bodyLen;
  out.set(p.mac, o);
  return out;
}

export function frameDecodePage(bytes: Uint8Array): PagePayload | undefined {
  const min = PAGE_FIXED - MAC_LEN;
  if (bytes.length < min) return undefined;
  if (bytes[0] !== FRAME_PAGE || bytes[1] !== FRAME_VERSION) return undefined;

  const bodyLen = bytes[PAGE_HOP_TTL_OFF + 1]!;
  if (bodyLen > PAGE_WIRE_BODY_MAX) return undefined;
  if (bytes.length !== PAGE_FIXED + bodyLen) return undefined;

  let o = 2;
  const destId = bytes.slice(o, o + NODE_ID_LEN);
  o += NODE_ID_LEN;
  const srcId = bytes.slice(o, o + NODE_ID_LEN);
  o += NODE_ID_LEN;
  const nonce = bytes.slice(o, o + PAGE_NONCE_LEN);
  o += PAGE_NONCE_LEN;
  const hopTtl = bytes[o]!;
  o += 1;
  o += 1;
  const body = bytes.slice(o, o + bodyLen);
  o += bodyLen;
  const mac = bytes.slice(o, o + MAC_LEN);
  return { destId, srcId, nonce, hopTtl, body, mac };
}

export function frameEncodePageAck(p: PageAckPayload): Uint8Array | undefined {
  if (p.destId.length !== NODE_ID_LEN) return undefined;
  if (p.srcId.length !== NODE_ID_LEN) return undefined;
  if (p.nonce.length !== PAGE_NONCE_LEN) return undefined;
  const hopTtl = hopTtlByte(p.hopTtl);
  if (hopTtl === undefined) return undefined;
  if (p.mac.length !== MAC_LEN) return undefined;

  const out = new Uint8Array(PAGE_ACK_LEN);
  let o = 0;
  out[o++] = FRAME_PAGE_ACK;
  out[o++] = FRAME_VERSION;
  out.set(p.destId, o);
  o += NODE_ID_LEN;
  out.set(p.srcId, o);
  o += NODE_ID_LEN;
  out.set(p.nonce, o);
  o += PAGE_NONCE_LEN;
  out[o++] = hopTtl;
  out.set(p.mac, o);
  return out;
}

export function frameDecodePageAck(bytes: Uint8Array): PageAckPayload | undefined {
  if (bytes.length !== PAGE_ACK_LEN) return undefined;
  if (bytes[0] !== FRAME_PAGE_ACK || bytes[1] !== FRAME_VERSION) return undefined;

  let o = 2;
  const destId = bytes.slice(o, o + NODE_ID_LEN);
  o += NODE_ID_LEN;
  const srcId = bytes.slice(o, o + NODE_ID_LEN);
  o += NODE_ID_LEN;
  const nonce = bytes.slice(o, o + PAGE_NONCE_LEN);
  o += PAGE_NONCE_LEN;
  const hopTtl = bytes[o]!;
  o += 1;
  const mac = bytes.slice(o, o + MAC_LEN);
  return { destId, srcId, nonce, hopTtl, mac };
}

/** Rewrite hop_ttl on an encoded PAGE or PAGE_ACK without touching body/mac. */
export function frameSetHopTtl(bytes: Uint8Array, ttl: number): boolean {
  if (hopTtlByte(ttl) === undefined) return false;
  if (bytes.length === 0) return false;
  const kind = bytes[0];
  if (kind !== FRAME_PAGE && kind !== FRAME_PAGE_ACK) return false;
  if (bytes[1] !== FRAME_VERSION) return false;
  if (kind === FRAME_PAGE_ACK) {
    if (bytes.length !== PAGE_ACK_LEN) return false;
  } else {
    if (bytes.length < PAGE_FIXED) return false;
    const bodyLen = bytes[PAGE_HOP_TTL_OFF + 1]!;
    if (bytes.length !== PAGE_FIXED + bodyLen) return false;
  }
  bytes[PAGE_HOP_TTL_OFF] = ttl;
  return true;
}
