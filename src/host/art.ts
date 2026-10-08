export const ART_STORE_KEY = "peeple-beeper-art-v1";
export const ART_W_MAX = 128;
export const ART_H_MAX = 64;
export const ART_FRAMES_MAX = 32;
export const ART_ASSETS_MAX = 64;
export const ART_DELAY_MIN_MS = 50;
export const ART_DELAY_MAX_MS = 2000;

export type ArtKind = "icon" | "screen" | "anim";
export type ArtTool = "pencil" | "eraser" | "fill" | "line" | "rect" | "rectfill";

export type ArtFrame = { pixels: Uint8Array };

export type ArtAsset = {
  id: string;
  name: string;
  w: number;
  h: number;
  kind: ArtKind;
  delayMs: number;
  frames: ArtFrame[];
};

export type ArtLibrary = { version: 1; assets: ArtAsset[] };

let fallbackId = 0;

function validDim(n: number, max: number): boolean {
  return Number.isInteger(n) && n >= 1 && n <= max;
}

function validSize(w: number, h: number): boolean {
  return validDim(w, ART_W_MAX) && validDim(h, ART_H_MAX);
}

function packedLen(w: number, h: number): number {
  return Math.ceil(h / 8) * w;
}

function newArtId(): string {
  const bytes = new Uint8Array(8);
  if (typeof crypto !== "undefined" && typeof crypto.getRandomValues === "function") {
    crypto.getRandomValues(bytes);
  } else {
    fallbackId += 1;
    let n = fallbackId;
    for (let i = 7; i >= 0; i--) {
      bytes[i] = n & 0xff;
      n = Math.floor(n / 256);
    }
  }
  let hex = "";
  for (let i = 0; i < bytes.length; i++) hex += bytes[i]!.toString(16).padStart(2, "0");
  return hex;
}

function isRecord(v: unknown): v is Record<string, unknown> {
  return typeof v === "object" && v !== null && !Array.isArray(v);
}

function isArtKind(v: unknown): v is ArtKind {
  return v === "icon" || v === "screen" || v === "anim";
}

function bytesToHexList(bytes: Uint8Array): string {
  const parts: string[] = [];
  for (let i = 0; i < bytes.length; i++) {
    parts.push("0x" + bytes[i]!.toString(16).padStart(2, "0"));
  }
  return parts.join(", ");
}

export function artIndex(w: number, x: number, y: number): number {
  return y * w + x;
}

export function artInBounds(w: number, h: number, x: number, y: number): boolean {
  return x >= 0 && y >= 0 && x < w && y < h;
}

export function artGet(frame: ArtFrame, w: number, h: number, x: number, y: number): 0 | 1 {
  if (!artInBounds(w, h, x, y)) return 0;
  return frame.pixels[artIndex(w, x, y)] ? 1 : 0;
}

export function artSet(
  frame: ArtFrame,
  w: number,
  h: number,
  x: number,
  y: number,
  ink: 0 | 1,
): void {
  if (!artInBounds(w, h, x, y)) return;
  frame.pixels[artIndex(w, x, y)] = ink;
}

export function artClear(frame: ArtFrame, fill: 0 | 1): void {
  frame.pixels.fill(fill);
}

export function artInvert(frame: ArtFrame): void {
  const p = frame.pixels;
  for (let i = 0; i < p.length; i++) p[i] = p[i] ? 0 : 1;
}

export function createFrame(w: number, h: number, fill: 0 | 1 = 0): ArtFrame {
  const pixels = new Uint8Array(w * h);
  if (fill) pixels.fill(1);
  return { pixels };
}

export function cloneFrame(frame: ArtFrame): ArtFrame {
  return { pixels: new Uint8Array(frame.pixels) };
}

export function artLine(
  frame: ArtFrame,
  w: number,
  h: number,
  x0: number,
  y0: number,
  x1: number,
  y1: number,
  ink: 0 | 1,
): void {
  x0 = x0 | 0;
  y0 = y0 | 0;
  x1 = x1 | 0;
  y1 = y1 | 0;
  const dx = Math.abs(x1 - x0);
  const sx = x0 < x1 ? 1 : -1;
  const dy = -Math.abs(y1 - y0);
  const sy = y0 < y1 ? 1 : -1;
  let err = dx + dy;
  for (;;) {
    artSet(frame, w, h, x0, y0, ink);
    if (x0 === x1 && y0 === y1) break;
    const e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

export function artRect(
  frame: ArtFrame,
  w: number,
  h: number,
  x0: number,
  y0: number,
  x1: number,
  y1: number,
  ink: 0 | 1,
  filled: boolean,
): void {
  const minX = Math.min(x0, x1) | 0;
  const maxX = Math.max(x0, x1) | 0;
  const minY = Math.min(y0, y1) | 0;
  const maxY = Math.max(y0, y1) | 0;
  if (filled) {
    for (let y = minY; y <= maxY; y++) {
      for (let x = minX; x <= maxX; x++) artSet(frame, w, h, x, y, ink);
    }
    return;
  }
  for (let x = minX; x <= maxX; x++) {
    artSet(frame, w, h, x, minY, ink);
    artSet(frame, w, h, x, maxY, ink);
  }
  for (let y = minY + 1; y < maxY; y++) {
    artSet(frame, w, h, minX, y, ink);
    artSet(frame, w, h, maxX, y, ink);
  }
}

export function artFlood(
  frame: ArtFrame,
  w: number,
  h: number,
  x: number,
  y: number,
  ink: 0 | 1,
): void {
  x = x | 0;
  y = y | 0;
  if (!artInBounds(w, h, x, y)) return;
  const target = artGet(frame, w, h, x, y);
  if (target === ink) return;
  const stackX = [x];
  const stackY = [y];
  while (stackX.length > 0) {
    const cx = stackX.pop()!;
    const cy = stackY.pop()!;
    if (!artInBounds(w, h, cx, cy)) continue;
    if (artGet(frame, w, h, cx, cy) !== target) continue;
    artSet(frame, w, h, cx, cy, ink);
    stackX.push(cx + 1, cx - 1, cx, cx);
    stackY.push(cy, cy, cy + 1, cy - 1);
  }
}

export function artPack(frame: ArtFrame, w: number, h: number): Uint8Array {
  const packed = new Uint8Array(packedLen(w, h));
  for (let y = 0; y < h; y++) {
    const page = y >> 3;
    const bit = 1 << (y & 7);
    for (let x = 0; x < w; x++) {
      if (frame.pixels[artIndex(w, x, y)]) packed[page * w + x] |= bit;
    }
  }
  return packed;
}

export function artUnpack(packed: Uint8Array, w: number, h: number): ArtFrame {
  const frame = createFrame(w, h, 0);
  for (let y = 0; y < h; y++) {
    const page = y >> 3;
    const bit = 1 << (y & 7);
    for (let x = 0; x < w; x++) {
      const i = page * w + x;
      if (i < packed.length && (packed[i]! & bit)) frame.pixels[artIndex(w, x, y)] = 1;
    }
  }
  return frame;
}

export function createLibrary(): ArtLibrary {
  return { version: 1, assets: [] };
}

export function createAsset(name: string, w: number, h: number, kind: ArtKind): ArtAsset | undefined {
  if (!validSize(w, h)) return undefined;
  const asset: ArtAsset = {
    id: newArtId(),
    name: "",
    w,
    h,
    kind,
    delayMs: 120,
    frames: [createFrame(w, h, 0)],
  };
  artRename(asset, name);
  return asset;
}

export function artAddAsset(lib: ArtLibrary, asset: ArtAsset): boolean {
  if (lib.assets.length >= ART_ASSETS_MAX) return false;
  lib.assets.push(asset);
  return true;
}

export function artRemoveAsset(lib: ArtLibrary, id: string): boolean {
  const i = lib.assets.findIndex((a) => a.id === id);
  if (i < 0) return false;
  lib.assets.splice(i, 1);
  return true;
}

export function artRename(asset: ArtAsset, name: string): void {
  let s = "";
  for (let i = 0; i < name.length && s.length < 32; i++) {
    const c = name.charCodeAt(i);
    if (c >= 32 && c <= 126) s += name[i]!;
  }
  asset.name = s.length < 1 ? "_" : s;
}

export function artAddFrame(asset: ArtAsset): boolean {
  if (asset.frames.length >= ART_FRAMES_MAX) return false;
  const last = asset.frames[asset.frames.length - 1];
  asset.frames.push(last ? cloneFrame(last) : createFrame(asset.w, asset.h, 0));
  return true;
}

export function artRemoveFrame(asset: ArtAsset, i: number): boolean {
  if (asset.frames.length <= 1) return false;
  if (!Number.isInteger(i) || i < 0 || i >= asset.frames.length) return false;
  asset.frames.splice(i, 1);
  return true;
}

export function artSetDelay(asset: ArtAsset, ms: number): void {
  if (!Number.isFinite(ms)) ms = ART_DELAY_MIN_MS;
  asset.delayMs = Math.min(ART_DELAY_MAX_MS, Math.max(ART_DELAY_MIN_MS, Math.round(ms)));
}

export function artSetSize(asset: ArtAsset, w: number, h: number): boolean {
  if (!validSize(w, h)) return false;
  const ow = asset.w;
  const oh = asset.h;
  if (w === ow && h === oh) return true;
  const copyW = Math.min(w, ow);
  const copyH = Math.min(h, oh);
  for (let i = 0; i < asset.frames.length; i++) {
    const src = asset.frames[i]!;
    const dst = createFrame(w, h, 0);
    for (let y = 0; y < copyH; y++) {
      for (let x = 0; x < copyW; x++) {
        dst.pixels[artIndex(w, x, y)] = src.pixels[artIndex(ow, x, y)]!;
      }
    }
    asset.frames[i] = dst;
  }
  asset.w = w;
  asset.h = h;
  return true;
}

export function bytesToB64(bytes: Uint8Array): string {
  let s = "";
  for (let i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]!);
  return btoa(s);
}

export function b64ToBytes(s: string): Uint8Array | undefined {
  try {
    const bin = atob(s);
    const out = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
    return out;
  } catch {
    return undefined;
  }
}

export function artSerialize(lib: ArtLibrary, pretty = false): string {
  const assets = [];
  for (let i = 0; i < lib.assets.length; i++) {
    const a = lib.assets[i]!;
    const frames: string[] = [];
    for (let f = 0; f < a.frames.length; f++) {
      frames.push(bytesToB64(artPack(a.frames[f]!, a.w, a.h)));
    }
    assets.push({
      id: a.id,
      name: a.name,
      w: a.w,
      h: a.h,
      kind: a.kind,
      delayMs: a.delayMs,
      frames,
    });
  }
  const body = { version: 1 as const, assets };
  return pretty ? JSON.stringify(body, null, 2) : JSON.stringify(body);
}

export function artParse(json: string): ArtLibrary | undefined {
  try {
    const raw: unknown = JSON.parse(json);
    if (!isRecord(raw) || raw.version !== 1 || !Array.isArray(raw.assets)) return undefined;
    if (raw.assets.length > ART_ASSETS_MAX) return undefined;
    const lib = createLibrary();
    for (let i = 0; i < raw.assets.length; i++) {
      const a = raw.assets[i];
      if (!isRecord(a)) return undefined;
      if (typeof a.id !== "string" || a.id.length === 0) return undefined;
      if (typeof a.name !== "string") return undefined;
      if (typeof a.w !== "number" || typeof a.h !== "number") return undefined;
      if (!validSize(a.w, a.h)) return undefined;
      if (!isArtKind(a.kind)) return undefined;
      if (typeof a.delayMs !== "number" || !Number.isFinite(a.delayMs)) return undefined;
      if (!Array.isArray(a.frames) || a.frames.length < 1 || a.frames.length > ART_FRAMES_MAX) {
        return undefined;
      }
      const expect = packedLen(a.w, a.h);
      const frames: ArtFrame[] = [];
      for (let f = 0; f < a.frames.length; f++) {
        const b64 = a.frames[f];
        if (typeof b64 !== "string") return undefined;
        const packed = b64ToBytes(b64);
        if (!packed || packed.length !== expect) return undefined;
        frames.push(artUnpack(packed, a.w, a.h));
      }
      const asset: ArtAsset = {
        id: a.id,
        name: "",
        w: a.w,
        h: a.h,
        kind: a.kind,
        delayMs: 120,
        frames,
      };
      artRename(asset, a.name);
      artSetDelay(asset, a.delayMs);
      lib.assets.push(asset);
    }
    return lib;
  } catch {
    return undefined;
  }
}

export function artLoad(): ArtLibrary {
  if (typeof localStorage === "undefined") return createLibrary();
  const raw = localStorage.getItem(ART_STORE_KEY);
  if (!raw) return createLibrary();
  return artParse(raw) ?? createLibrary();
}

export function artSave(lib: ArtLibrary): void {
  if (typeof localStorage === "undefined") return;
  localStorage.setItem(ART_STORE_KEY, artSerialize(lib));
}

export function artIdent(name: string): string {
  let s = "";
  for (let i = 0; i < name.length; i++) {
    const c = name[i]!;
    if (
      (c >= "0" && c <= "9") ||
      (c >= "A" && c <= "Z") ||
      (c >= "a" && c <= "z")
    ) {
      s += c.toUpperCase();
    } else {
      s += "_";
    }
  }
  const first = s.charCodeAt(0);
  if (!(first >= 65 && first <= 90)) s = "ART_" + s;
  return s;
}

export function artExportTs(asset: ArtAsset): string {
  const name = artIdent(asset.name);
  const lines: string[] = [];
  lines.push(`// ST7567 index=(y>>3)*W+x, bit=1<<(y&7), W=${asset.w}`);
  lines.push(`export const ${name}_W = ${asset.w}`);
  lines.push(`export const ${name}_H = ${asset.h}`);
  if (asset.frames.length <= 1) {
    const packed = artPack(asset.frames[0] ?? createFrame(asset.w, asset.h, 0), asset.w, asset.h);
    lines.push(`export const ${name} = Uint8Array.from([${bytesToHexList(packed)}])`);
  } else {
    lines.push(`export const ${name}_DELAY_MS = ${asset.delayMs}`);
    lines.push(`export const ${name}_FRAMES: readonly Uint8Array[] = [`);
    for (let i = 0; i < asset.frames.length; i++) {
      const packed = artPack(asset.frames[i]!, asset.w, asset.h);
      lines.push(`  Uint8Array.from([${bytesToHexList(packed)}]),`);
    }
    lines.push(`]`);
  }
  lines.push(
    `// blit with blitSprite(dst, ${name}, ${name}_W, ${name}_H, dx, dy) from display.ts`,
  );
  return lines.join("\n") + "\n";
}
