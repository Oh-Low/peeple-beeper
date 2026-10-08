/**
 * Shared encoder keyboard. Top two lines are the draft; CASE/SYM/NUM plus
 * a QWERTY (or symbol/number) grid fill the rest. One instance for every
 * text field (name, custom page, later fields).
 */

import {
  CHAR_H,
  CHAR_W,
  clip,
  cols,
  drawText,
  hLine,
  invertRect,
} from "./display";
import { DISPLAY_WIDTH } from "./hal";

const TEXT_LINES = 2;
const SEP_Y = CHAR_H * TEXT_LINES;
const MODE_Y = SEP_Y + 2;
const MODE_H = 9;
const KEYS_Y = MODE_Y + MODE_H + 1;
const KEY_H = 9;
const NARROW_W = 12;

export type TextEditPage = "letter" | "sym" | "num";

type CellKind =
  | "case"
  | "sym"
  | "num"
  | "char"
  | "left"
  | "right"
  | "spc"
  | "del"
  | "save"
  | "ok";

type Cell = {
  kind: CellKind;
  label: string;
  ch?: string;
};

export type TextEdit = {
  draft: string;
  maxLen: number;
  page: TextEditPage;
  caps: boolean;
  sel: number;
  /** Insert point: 0..draft.length. */
  caret: number;
  /** Heart key: save to canned. Custom page only. */
  canSave: boolean;
};

export function createTextEdit(): TextEdit {
  return {
    draft: "",
    maxLen: 1,
    page: "letter",
    caps: false,
    sel: 0,
    caret: 0,
    canSave: false,
  };
}

export function textEditBegin(
  edit: TextEdit,
  initial: string,
  maxLen: number,
  canSave = false,
): void {
  edit.draft = initial.slice(0, maxLen);
  edit.maxLen = maxLen;
  edit.page = "letter";
  edit.caps = false;
  edit.sel = 3;
  edit.caret = edit.draft.length;
  edit.canSave = canSave;
}

export function textEditReset(edit: TextEdit): void {
  edit.draft = "";
  edit.maxLen = 1;
  edit.page = "letter";
  edit.caps = false;
  edit.sel = 0;
  edit.caret = 0;
  edit.canSave = false;
}

export function textEditDraft(edit: TextEdit): string {
  return edit.draft.replace(/\s+$/g, "");
}

function charsOf(s: string): Cell[] {
  const out: Cell[] = [];
  for (const ch of s) out.push({ kind: "char", label: ch, ch });
  return out;
}

function actionRow(canSave: boolean): Cell[] {
  const row: Cell[] = [
    { kind: "left", label: "<" },
    { kind: "right", label: ">" },
    { kind: "spc", label: "SPC" },
    { kind: "del", label: "DEL" },
  ];
  if (canSave) row.push({ kind: "save", label: "♥" });
  row.push({ kind: "ok", label: "OK" });
  return row;
}

function modeRow(): Cell[] {
  return [
    { kind: "case", label: "CASE" },
    { kind: "sym", label: "SYM" },
    { kind: "num", label: "NUM" },
  ];
}

function pageRows(edit: TextEdit): Cell[][] {
  const mode = modeRow();
  const actions = actionRow(edit.canSave);
  if (edit.page === "num") {
    return [mode, charsOf("1234567890"), actions];
  }
  if (edit.page === "sym") {
    return [
      mode,
      charsOf(".?!,@#$'\"-"),
      charsOf("&*():;+=/"),
      charsOf("<>[]{}~_\\"),
      actions,
    ];
  }
  const q = edit.caps ? "QWERTYUIOP" : "qwertyuiop";
  const a = edit.caps ? "ASDFGHJKL" : "asdfghjkl";
  const z = edit.caps ? "ZXCVBNM" : "zxcvbnm";
  return [mode, charsOf(q), charsOf(a), charsOf(z), actions];
}

function flatten(edit: TextEdit): Cell[] {
  const rows = pageRows(edit);
  const out: Cell[] = [];
  for (const row of rows) {
    for (const c of row) out.push(c);
  }
  return out;
}

function clampSel(edit: TextEdit): void {
  const n = flatten(edit).length;
  if (n <= 0) {
    edit.sel = 0;
    return;
  }
  if (edit.sel < 0) edit.sel = n - 1;
  if (edit.sel >= n) edit.sel = 0;
}

function clampCaret(edit: TextEdit): void {
  if (edit.caret < 0) edit.caret = 0;
  if (edit.caret > edit.draft.length) edit.caret = edit.draft.length;
}

function insert(edit: TextEdit, ch: string): void {
  if (ch.length !== 1) return;
  if (edit.draft.length >= edit.maxLen) return;
  clampCaret(edit);
  edit.draft = edit.draft.slice(0, edit.caret) + ch + edit.draft.slice(edit.caret);
  edit.caret += 1;
}

function delBefore(edit: TextEdit): void {
  clampCaret(edit);
  if (edit.caret <= 0) return;
  edit.draft = edit.draft.slice(0, edit.caret - 1) + edit.draft.slice(edit.caret);
  edit.caret -= 1;
}

function moveCaret(edit: TextEdit, dir: -1 | 1): void {
  edit.caret += dir;
  clampCaret(edit);
}

function advance(edit: TextEdit): void {
  const n = flatten(edit).length;
  if (n <= 0) return;
  edit.sel = (edit.sel + 1) % n;
}

export function textEditTurn(edit: TextEdit, dir: 1 | -1): void {
  const n = flatten(edit).length;
  if (n <= 0) return;
  edit.sel = (edit.sel + dir + n) % n;
}

/** `"ok"` confirms the field. `"save"` hearts the draft into canned (custom only). */
export function textEditPress(edit: TextEdit): "none" | "ok" | "save" {
  clampSel(edit);
  const cell = flatten(edit)[edit.sel];
  if (!cell) return "none";

  if (cell.kind === "case") {
    if (edit.page === "letter") edit.caps = !edit.caps;
    else {
      edit.page = "letter";
    }
    return "none";
  }
  if (cell.kind === "sym") {
    edit.page = "sym";
    clampSel(edit);
    return "none";
  }
  if (cell.kind === "num") {
    edit.page = "num";
    clampSel(edit);
    return "none";
  }
  if (cell.kind === "left") {
    moveCaret(edit, -1);
    return "none";
  }
  if (cell.kind === "right") {
    moveCaret(edit, 1);
    return "none";
  }
  if (cell.kind === "spc") {
    insert(edit, " ");
    advance(edit);
    return "none";
  }
  if (cell.kind === "del") {
    delBefore(edit);
    return "none";
  }
  if (cell.kind === "save") return "save";
  if (cell.kind === "ok") return "ok";

  if (cell.kind === "char" && cell.ch) {
    insert(edit, cell.ch);
    advance(edit);
  }
  return "none";
}

export function textEditHandleText(edit: TextEdit, key: string): boolean {
  if (key === "Backspace") {
    delBefore(edit);
    return true;
  }
  if (key.length !== 1) return false;
  const code = key.charCodeAt(0);
  if (code < 32 || code > 126) return false;
  insert(edit, key);
  return true;
}

type Rect = { x: number; y: number; w: number; h: number };

function rowRects(row: Cell[], rowIndex: number): Rect[] {
  const y = rowIndex === 0 ? MODE_Y : KEYS_Y + (rowIndex - 1) * KEY_H;
  const h = rowIndex === 0 ? MODE_H : KEY_H;
  const n = row.length;
  const wide = row[0]?.kind === "case" || row[0]?.kind === "left";
  if (wide) {
    const w = Math.floor(DISPLAY_WIDTH / n);
    return row.map((_, i) => ({ x: i * w, y, w, h }));
  }
  const w = NARROW_W;
  return row.map((_, i) => ({ x: 4 + i * w, y, w, h }));
}

function drawCentered(buf: Uint8Array, rect: Rect, label: string): void {
  const tw = label.length * CHAR_W;
  const x = rect.x + Math.max(0, Math.floor((rect.w - tw) / 2));
  const y = rect.y + Math.max(0, Math.floor((rect.h - CHAR_H) / 2));
  drawText(buf, x, y, clip(label, Math.floor(rect.w / CHAR_W) || 1));
}

export function textEditDraw(edit: TextEdit, buf: Uint8Array): void {
  clampCaret(edit);
  const width = cols();
  const vis = width * TEXT_LINES;
  let start = 0;
  if (edit.caret >= vis) start = edit.caret - vis + 1;
  const shown = edit.draft.slice(start, start + vis);
  const caret = edit.caret - start;
  for (let line = 0; line < TEXT_LINES; line++) {
    const slice = shown.slice(line * width, line * width + width);
    if (slice.length > 0) drawText(buf, 0, line * CHAR_H, slice);
  }
  const caretLine = Math.min(TEXT_LINES - 1, Math.floor(caret / width));
  const caretCol = caret % width;
  invertRect(buf, caretCol * CHAR_W, caretLine * CHAR_H, CHAR_W, CHAR_H);

  hLine(buf, 0, SEP_Y, DISPLAY_WIDTH);

  const rows = pageRows(edit);
  let idx = 0;
  for (let r = 0; r < rows.length; r++) {
    const row = rows[r] ?? [];
    const rects = rowRects(row, r);
    for (let c = 0; c < row.length; c++) {
      const cell = row[c];
      const rect = rects[c];
      if (!cell || !rect) continue;
      drawCentered(buf, rect, cell.label);
      const active =
        (cell.kind === "case" && edit.page === "letter") ||
        (cell.kind === "sym" && edit.page === "sym") ||
        (cell.kind === "num" && edit.page === "num");
      if (active) hLine(buf, rect.x + 2, rect.y + rect.h - 1, rect.w - 4);
      if (idx === edit.sel) invertRect(buf, rect.x, rect.y, rect.w, rect.h);
      idx += 1;
    }
  }
}
