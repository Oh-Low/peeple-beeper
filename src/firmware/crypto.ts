/** App-layer page crypto. LoRa is clear; we MAC/XOR here. No HAL, no frames. */

export const SESSION_KEY_LEN = 16;

const HASH_LEN = 32;
const MAC_LEN = 8;
const CONFIRM_LEN = 16;

const K = new Uint32Array([
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
]);

function rotr(x: number, n: number): number {
  return (x >>> n) | (x << (32 - n));
}

function loadU32be(b: Uint8Array, i: number): number {
  return ((b[i]! << 24) | (b[i + 1]! << 16) | (b[i + 2]! << 8) | b[i + 3]!) >>> 0;
}

function storeU32be(b: Uint8Array, i: number, x: number): void {
  b[i] = (x >>> 24) & 0xff;
  b[i + 1] = (x >>> 16) & 0xff;
  b[i + 2] = (x >>> 8) & 0xff;
  b[i + 3] = x & 0xff;
}

function concat(...parts: Uint8Array[]): Uint8Array {
  let n = 0;
  for (const p of parts) n += p.length;
  const out = new Uint8Array(n);
  let o = 0;
  for (const p of parts) {
    out.set(p, o);
    o += p.length;
  }
  return out;
}

/** Unsigned-byte lexicographic compare. Negative if a < b. */
function memcmp(a: Uint8Array, b: Uint8Array): number {
  const n = a.length < b.length ? a.length : b.length;
  for (let i = 0; i < n; i++) {
    const d = a[i]! - b[i]!;
    if (d !== 0) return d;
  }
  return a.length - b.length;
}

/** Length-aware; no early return on the first mismatched byte. */
function ctEq(a: Uint8Array, b: Uint8Array): boolean {
  const n = a.length < b.length ? a.length : b.length;
  let d = a.length ^ b.length;
  for (let i = 0; i < n; i++) d |= a[i]! ^ b[i]!;
  return d === 0;
}

function firstBytes(h: Uint8Array, n: number): Uint8Array {
  return h.slice(0, n);
}

function keystreamXor(sessionKey: Uint8Array, nonce: Uint8Array, data: Uint8Array): Uint8Array {
  const block = sha256(concat(sessionKey, nonce));
  const out = new Uint8Array(data.length);
  for (let i = 0; i < data.length; i++) out[i] = data[i]! ^ block[i & 31]!;
  return out;
}

export function sha256(data: Uint8Array): Uint8Array {
  const hs = new Uint32Array([
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
  ]);

  const paddedLen = ((data.length + 9 + 63) >> 6) << 6;
  const padded = new Uint8Array(paddedLen);
  padded.set(data);
  padded[data.length] = 0x80;
  storeU32be(padded, paddedLen - 8, data.length >>> 29);
  storeU32be(padded, paddedLen - 4, (data.length << 3) >>> 0);

  const w = new Uint32Array(64);
  for (let off = 0; off < paddedLen; off += 64) {
    for (let t = 0; t < 16; t++) w[t] = loadU32be(padded, off + t * 4);
    for (let t = 16; t < 64; t++) {
      const x = w[t - 15]!;
      const y = w[t - 2]!;
      const s0 = rotr(x, 7) ^ rotr(x, 18) ^ (x >>> 3);
      const s1 = rotr(y, 17) ^ rotr(y, 19) ^ (y >>> 10);
      w[t] = (s1 + w[t - 7]! + s0 + w[t - 16]!) >>> 0;
    }

    let a = hs[0]!;
    let b = hs[1]!;
    let c = hs[2]!;
    let d = hs[3]!;
    let e = hs[4]!;
    let f = hs[5]!;
    let g = hs[6]!;
    let h = hs[7]!;

    for (let t = 0; t < 64; t++) {
      const S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const ch = (e & f) ^ (~e & g);
      const t1 = (h + S1 + ch + K[t]! + w[t]!) >>> 0;
      const S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const maj = (a & b) ^ (a & c) ^ (b & c);
      const t2 = (S0 + maj) >>> 0;
      h = g;
      g = f;
      f = e;
      e = (d + t1) >>> 0;
      d = c;
      c = b;
      b = a;
      a = (t1 + t2) >>> 0;
    }

    hs[0] = (hs[0]! + a) >>> 0;
    hs[1] = (hs[1]! + b) >>> 0;
    hs[2] = (hs[2]! + c) >>> 0;
    hs[3] = (hs[3]! + d) >>> 0;
    hs[4] = (hs[4]! + e) >>> 0;
    hs[5] = (hs[5]! + f) >>> 0;
    hs[6] = (hs[6]! + g) >>> 0;
    hs[7] = (hs[7]! + h) >>> 0;
  }

  const out = new Uint8Array(HASH_LEN);
  for (let i = 0; i < 8; i++) storeU32be(out, i * 4, hs[i]!);
  return out;
}

export function pairDeriveSessionKey(
  idA: Uint8Array,
  idB: Uint8Array,
  ephA: Uint8Array,
  ephB: Uint8Array,
): Uint8Array {
  const material =
    memcmp(idA, idB) < 0 ? concat(idA, idB, ephA, ephB) : concat(idB, idA, ephB, ephA);
  return firstBytes(sha256(material), SESSION_KEY_LEN);
}

export function pairConfirmHash(helloA: Uint8Array, helloB: Uint8Array): Uint8Array {
  const material = memcmp(helloA, helloB) < 0 ? concat(helloA, helloB) : concat(helloB, helloA);
  return firstBytes(sha256(material), CONFIRM_LEN);
}

export function pageSeal(
  sessionKey: Uint8Array,
  nonce: Uint8Array,
  plaintext: Uint8Array,
): { cipher: Uint8Array; mac: Uint8Array } {
  const cipher = keystreamXor(sessionKey, nonce, plaintext);
  const mac = firstBytes(sha256(concat(sessionKey, nonce, cipher)), MAC_LEN);
  return { cipher, mac };
}

export function pageOpen(
  sessionKey: Uint8Array,
  nonce: Uint8Array,
  cipher: Uint8Array,
  mac: Uint8Array,
): Uint8Array | undefined {
  const expect = firstBytes(sha256(concat(sessionKey, nonce, cipher)), MAC_LEN);
  if (!ctEq(expect, mac)) return undefined;
  return keystreamXor(sessionKey, nonce, cipher);
}

const ACK_TAG = new Uint8Array([0x41, 0x43, 0x4b]); // ASCII "ACK"

export function pageAckMac(sessionKey: Uint8Array, nonce: Uint8Array): Uint8Array {
  return firstBytes(sha256(concat(sessionKey, nonce, ACK_TAG)), MAC_LEN);
}

export function pageAckCheck(
  sessionKey: Uint8Array,
  nonce: Uint8Array,
  mac: Uint8Array,
): boolean {
  return ctEq(pageAckMac(sessionKey, nonce), mac);
}
