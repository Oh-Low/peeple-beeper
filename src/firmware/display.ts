import { DISPLAY_BUF_LEN, DISPLAY_HEIGHT, DISPLAY_WIDTH } from "./hal";
import { glyph5 } from "./font5x7";

export const CHAR_W = 6;
export const CHAR_H = 8;

export function bufIndex(x: number, y: number): number {
  return (y >> 3) * DISPLAY_WIDTH + x;
}

export function setPixel(buf: Uint8Array, x: number, y: number, on: boolean): void {
  if (x < 0 || y < 0 || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
  const i = bufIndex(x, y);
  const bit = 1 << (y & 7);
  if (on) buf[i] |= bit;
  else buf[i] &= ~bit;
}

export function getPixel(buf: Uint8Array, x: number, y: number): boolean {
  if (x < 0 || y < 0 || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return false;
  const i = bufIndex(x, y);
  const bit = 1 << (y & 7);
  return (buf[i]! & bit) !== 0;
}

export function clear(buf: Uint8Array): void {
  buf.fill(0);
}

export function invertRect(
  buf: Uint8Array,
  x: number,
  y: number,
  w: number,
  h: number,
): void {
  for (let yy = y; yy < y + h; yy++) {
    for (let xx = x; xx < x + w; xx++) {
      if (xx < 0 || yy < 0 || xx >= DISPLAY_WIDTH || yy >= DISPLAY_HEIGHT) continue;
      const i = bufIndex(xx, yy);
      buf[i] ^= 1 << (yy & 7);
    }
  }
}

export function hLine(buf: Uint8Array, x: number, y: number, w: number): void {
  for (let i = 0; i < w; i++) setPixel(buf, x + i, y, true);
}

export function drawChar(buf: Uint8Array, x: number, y: number, ch: string): void {
  const cols = glyph5(ch);
  for (let cx = 0; cx < 5; cx++) {
    const col = cols[cx] ?? 0;
    for (let cy = 0; cy < 7; cy++) {
      if (col & (1 << cy)) setPixel(buf, x + cx, y + cy, true);
    }
  }
}

/** Draws `s` left-to-right. A glyph that straddles the right edge is pixel-clipped. */
export function drawText(buf: Uint8Array, x: number, y: number, s: string): void {
  let cx = x;
  for (let i = 0; i < s.length; i++) {
    if (cx >= DISPLAY_WIDTH) break;
    drawChar(buf, cx, y, s[i] ?? " ");
    cx += CHAR_W;
  }
}

export function textWidth(s: string): number {
  return s.length * CHAR_W;
}

export function cols(): number {
  return Math.floor(DISPLAY_WIDTH / CHAR_W);
}

export function clip(s: string, maxChars: number): string {
  if (s.length <= maxChars) return s;
  if (maxChars <= 1) return s.slice(0, maxChars);
  return s.slice(0, maxChars - 1) + ".";
}

/** Word-wrap on spaces. UTF-16 code units (ASCII pager). Newlines force a break. */
export function wrap(s: string, width: number): string[] {
  if (s.length === 0 || width <= 0) return [];
  const lines: string[] = [];
  const parts = s.split("\n");
  for (let p = 0; p < parts.length; p++) {
    let rest = parts[p] ?? "";
    if (rest.length === 0) {
      lines.push("");
      continue;
    }
    while (rest.length > 0) {
      if (rest.length <= width) {
        lines.push(rest);
        break;
      }
      let cut = rest.lastIndexOf(" ", width);
      if (cut < 1) cut = width;
      lines.push(rest.slice(0, cut));
      rest = rest.slice(cut).replace(/^ /, "");
    }
  }
  return lines;
}

export function newBuffer(): Uint8Array {
  return new Uint8Array(DISPLAY_BUF_LEN);
}

/** OR-blit a sprite packed with width `sw` (`index=(y>>3)*sw+x`, `bit=1<<(y&7)`). */
export function blitSprite(
  dst: Uint8Array,
  sprite: Uint8Array,
  sw: number,
  sh: number,
  dx: number,
  dy: number,
): void {
  for (let y = 0; y < sh; y++) {
    const page = y >> 3;
    const bit = 1 << (y & 7);
    for (let x = 0; x < sw; x++) {
      const i = page * sw + x;
      if (i < sprite.length && (sprite[i]! & bit)) {
        setPixel(dst, dx + x, dy + y, true);
      }
    }
  }
}
