/** Art pack/draw/serialize tests. `npm run test:art` — no browser. */

import { blitSprite, getPixel, newBuffer } from "../firmware/display";
import {
  artAddAsset,
  artExportTs,
  artFlood,
  artGet,
  artLine,
  artPack,
  artParse,
  artRect,
  artSerialize,
  artSet,
  artUnpack,
  createAsset,
  createFrame,
  createLibrary,
} from "./art";

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

function bytesEq(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

test("pack/unpack 8x8 roundtrip", () => {
  const frame = createFrame(8, 8, 0);
  artSet(frame, 8, 8, 0, 0, 1);
  artSet(frame, 8, 8, 1, 0, 1);
  artSet(frame, 8, 8, 0, 7, 1);
  artSet(frame, 8, 8, 7, 7, 1);
  const packed = artPack(frame, 8, 8);
  assertEq(packed.length, 8, "pages*w");
  const back = artUnpack(packed, 8, 8);
  assert(bytesEq(frame.pixels, back.pixels), "pixels");
  assert(bytesEq(packed, artPack(back, 8, 8)), "repack");
});

test("line/rect/flood", () => {
  const frame = createFrame(8, 8, 0);
  artLine(frame, 8, 8, 0, 0, 7, 0, 1);
  for (let x = 0; x < 8; x++) assertEq(artGet(frame, 8, 8, x, 0), 1, `line x=${x}`);
  assertEq(artGet(frame, 8, 8, 0, 1), 0, "below line");

  artRect(frame, 8, 8, 1, 1, 6, 6, 1, false);
  assertEq(artGet(frame, 8, 8, 1, 1), 1, "rect corner");
  assertEq(artGet(frame, 8, 8, 6, 3), 1, "rect side");
  assertEq(artGet(frame, 8, 8, 3, 3), 0, "rect hollow");

  artFlood(frame, 8, 8, 3, 3, 1);
  assertEq(artGet(frame, 8, 8, 3, 3), 1, "flood inside");
  assertEq(artGet(frame, 8, 8, 2, 2), 1, "flood fill");
  assertEq(artGet(frame, 8, 8, 0, 7), 0, "outside box");
});

test("blitSprite onto newBuffer at (2,2)", () => {
  const frame = createFrame(8, 8, 0);
  artSet(frame, 8, 8, 0, 0, 1);
  artSet(frame, 8, 8, 7, 7, 1);
  const dst = newBuffer();
  blitSprite(dst, artPack(frame, 8, 8), 8, 8, 2, 2);
  assert(getPixel(dst, 2, 2), "origin ink");
  assert(getPixel(dst, 9, 9), "corner ink");
  assert(!getPixel(dst, 0, 0), "dst uncleared");
  assert(!getPixel(dst, 2, 3), "neighbor paper");
  assert(!getPixel(dst, 3, 2), "neighbor paper x");
});

test("serialize/parse roundtrip", () => {
  const lib = createLibrary();
  const asset = createAsset("Dot", 8, 8, "icon");
  assert(asset, "createAsset");
  artSet(asset.frames[0]!, 8, 8, 3, 4, 1);
  assert(artAddAsset(lib, asset), "add");
  const json = artSerialize(lib);
  const parsed = artParse(json);
  assert(parsed, "parse");
  assertEq(parsed.assets.length, 1, "one asset");
  const back = parsed.assets[0]!;
  assertEq(back.name, "Dot", "name");
  assertEq(back.w, 8, "w");
  assertEq(back.h, 8, "h");
  assertEq(back.kind, "icon", "kind");
  assertEq(back.delayMs, 120, "delay");
  assertEq(artGet(back.frames[0]!, 8, 8, 3, 4), 1, "pixel");
  assertEq(artGet(back.frames[0]!, 8, 8, 0, 0), 0, "paper");
  assert(artParse("not json") === undefined, "fail closed");
  const pretty = artSerialize(lib, true);
  assert(pretty.includes("\n"), "pretty newlines");
  const prettyParsed = artParse(pretty);
  assert(prettyParsed, "parse pretty");
  assertEq(artGet(prettyParsed.assets[0]!.frames[0]!, 8, 8, 3, 4), 1, "pretty pixel");
});

test("exportTs contains Uint8Array.from and blitSprite comment", () => {
  const asset = createAsset("Glyph", 8, 8, "icon");
  assert(asset, "createAsset");
  artSet(asset.frames[0]!, 8, 8, 0, 0, 1);
  const src = artExportTs(asset);
  assert(src.includes("Uint8Array.from"), "Uint8Array.from");
  assert(src.includes("blitSprite"), "blitSprite comment");
});

test("createAsset rejects 0 or 129 width", () => {
  assert(createAsset("x", 0, 8, "icon") === undefined, "w 0");
  assert(createAsset("x", 129, 8, "icon") === undefined, "w 129");
  assert(createAsset("x", 1, 8, "icon"), "w 1");
  assert(createAsset("x", 128, 8, "icon"), "w 128");
});

console.log(`${passed} passed, ${failed} failed`);
if (failed > 0) throw new Error("art tests failed");
