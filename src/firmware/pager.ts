import { cannedAdd, cannedList, cannedReset, createCannedBook, type CannedBook } from "./canned";
import { contactsFindById, contactsList, type ContactBook } from "./contacts";
import {
  CHAR_H,
  CHAR_W,
  clear,
  clip,
  cols,
  drawText,
  hLine,
  invertRect,
  newBuffer,
  textWidth,
  wrap,
} from "./display";
import {
  DISPLAY_HEIGHT,
  DISPLAY_WIDTH,
  PAGE_BODY_MAX,
  PAGE_FROM_MAX,
  type Hal,
  type Page,
} from "./hal";
import {
  identityGetDisplayName,
  identitySetDisplayName,
  type IdentityStore,
} from "./identity";
import {
  PAGE_ATTEMPTS,
  pageBusy,
  pageClearStatus,
  pageOutboxAttempts,
  pageSendStatus,
  pageTx,
  type PageRadio,
} from "./page";
import {
  pairAbort,
  pairActive,
  pairDeadlineMs,
  pairError,
  pairLastRssi,
  pairStart,
  type PairSession,
} from "./pair";
import {
  createTextEdit,
  textEditBegin,
  textEditDraft,
  textEditDraw,
  textEditHandleText,
  textEditPress,
  textEditReset,
  textEditTurn,
  type TextEdit,
} from "./textedit";

/** Inbox and outbox rows kept in RAM and NVM. */
export const PAGE_KEEP_MAX = 256;
const FLASH_MS = 400;

type Screen =
  | "menu"
  | "inbox"
  | "read"
  | "compose"
  | "canned"
  | "alert"
  | "pair"
  | "settings"
  | "nameedit"
  | "bodyedit"
  | "sending"
  | "sent"
  | "failed"
  | "outbox"
  | "outread";

const MENU = ["Inbox", "Compose", "Outbox", "Add contact"] as const;
const MENU_COG = MENU.length;

type OutboxStatus = "sending" | "sent" | "failed";

/** 255 = no clock (not yet ACKed). Local HH:MM, not on air. */
export const CLOCK_NONE = 255;

export type InboxRow = {
  from: string;
  body: string;
  rssi: number;
  receivedMs: number;
  receivedHh: number;
  receivedMm: number;
};

export type OutboxItem = {
  destId: Uint8Array;
  to: string;
  body: string;
  status: OutboxStatus;
  sentHh: number;
  sentMm: number;
  ackedHh: number;
  ackedMm: number;
};

export type Pager = {
  identity: IdentityStore;
  contacts: ContactBook;
  pair: PairSession;
  radio: PageRadio;
  screen: Screen;
  sel: number;
  inbox: InboxRow[];
  outbox: OutboxItem[];
  reading: number;
  alert: Page | null;
  flashStart: number;
  flashMs: number;
  buf: Uint8Array;
  composeDest: Uint8Array | null;
  edit: TextEdit;
  canned: CannedBook;
  vibeTest: boolean;
  sentTo: string;
  sentBody: string;
};

export function createPager(
  identity: IdentityStore,
  contacts: ContactBook,
  pair: PairSession,
  radio: PageRadio,
): Pager {
  return {
    identity,
    contacts,
    pair,
    radio,
    screen: "menu",
    sel: 0,
    inbox: [],
    outbox: [],
    reading: 0,
    alert: null,
    flashStart: 0,
    flashMs: 0,
    buf: newBuffer(),
    composeDest: null,
    edit: createTextEdit(),
    canned: createCannedBook(),
    vibeTest: false,
    sentTo: "",
    sentBody: "",
  };
}

function elapsedU32(now: number, start: number): number {
  return (now - start) >>> 0;
}

function deadlineLive(now: number, start: number, ms: number): boolean {
  return ms > 0 && elapsedU32(now, start) < ms;
}

function unreadCount(pager: Pager): number {
  return pager.inbox.length;
}

function pad2(n: number): string {
  return n < 10 ? `0${n}` : String(n);
}

function clampHour(n: number): number {
  if (n < 0) return 0;
  if (n > 23) return 23;
  return n | 0;
}

function clampMinute(n: number): number {
  if (n < 0) return 0;
  if (n > 59) return 59;
  return n | 0;
}

function stampClock(hal: Hal): { hh: number; mm: number } {
  const c = hal.clock();
  return { hh: clampHour(c.hh), mm: clampMinute(c.mm) };
}

function formatHm(hh: number, mm: number): string {
  if (hh > 23 || mm > 59) return "--:--";
  return `${pad2(hh)}:${pad2(mm)}`;
}

function copyPage(src: Page, dst: Page): void {
  dst.from = src.from.slice(0, PAGE_FROM_MAX);
  dst.body = src.body.slice(0, PAGE_BODY_MAX);
  dst.rssi = src.rssi;
  dst.receivedMs = src.receivedMs;
}

function pushOutbox(pager: Pager, destId: Uint8Array, to: string, body: string, hal: Hal): void {
  const t = stampClock(hal);
  pager.outbox.unshift({
    destId: new Uint8Array(destId),
    to: to.slice(0, PAGE_FROM_MAX),
    body: body.slice(0, PAGE_BODY_MAX),
    status: "sending",
    sentHh: t.hh,
    sentMm: t.mm,
    ackedHh: CLOCK_NONE,
    ackedMm: CLOCK_NONE,
  });
  if (pager.outbox.length > PAGE_KEEP_MAX) pager.outbox.pop();
}

function finishOutbox(pager: Pager, next: "sent" | "failed", hal: Hal): void {
  for (let i = 0; i < pager.outbox.length; i++) {
    const row = pager.outbox[i];
    if (row && row.status === "sending") {
      row.status = next;
      if (next === "sent") {
        const t = stampClock(hal);
        row.ackedHh = t.hh;
        row.ackedMm = t.mm;
      }
      return;
    }
  }
}

function outboxMark(item: OutboxItem): string {
  if (item.status === "sending") return "*";
  if (item.status === "failed") return "X";
  return "+";
}

function promoteOutbox(pager: Pager, item: OutboxItem): void {
  const i = pager.outbox.indexOf(item);
  if (i > 0) {
    pager.outbox.splice(i, 1);
    pager.outbox.unshift(item);
  }
  item.status = "sending";
  item.ackedHh = CLOCK_NONE;
  item.ackedMm = CLOCK_NONE;
}

function pushInbox(pager: Pager, p: Page, hal: Hal): void {
  const t = stampClock(hal);
  const row: InboxRow = {
    from: "",
    body: "",
    rssi: 0,
    receivedMs: 0,
    receivedHh: t.hh,
    receivedMm: t.mm,
  };
  copyPage(p, row);
  pager.inbox.unshift(row);
  if (pager.inbox.length > PAGE_KEEP_MAX) pager.inbox.pop();
}

function clampSel(pager: Pager, max: number): void {
  if (max <= 0) {
    pager.sel = 0;
    return;
  }
  if (pager.sel < 0) pager.sel = max - 1;
  if (pager.sel >= max) pager.sel = 0;
}

function beginNameEdit(pager: Pager): void {
  pager.screen = "nameedit";
  textEditBegin(pager.edit, identityGetDisplayName(pager.identity), PAGE_FROM_MAX);
}

function beginBodyEdit(pager: Pager): void {
  pager.screen = "bodyedit";
  textEditBegin(pager.edit, "", PAGE_BODY_MAX, true);
}

function saveNameEdit(pager: Pager): void {
  identitySetDisplayName(pager.identity, textEditDraft(pager.edit));
  pager.screen = "settings";
  pager.sel = 1;
}

function cancelNameEdit(pager: Pager): void {
  pager.screen = "settings";
  pager.sel = 1;
}

function sendPage(
  pager: Pager,
  hal: Hal,
  dest: Uint8Array,
  body: string,
  reuse?: OutboxItem,
): void {
  if (!body) return;
  if (pageBusy(pager.radio)) return;
  if (!pageTx(pager.radio, hal, dest, body, pager.identity, pager.contacts)) return;
  const contact = contactsFindById(pager.contacts, dest);
  pager.sentTo = contact ? contact.name : "";
  pager.sentBody = body.slice(0, PAGE_BODY_MAX);
  if (reuse) promoteOutbox(pager, reuse);
  else pushOutbox(pager, dest, pager.sentTo, pager.sentBody, hal);
  pager.screen = "outbox";
  pager.sel = pager.outbox.length ? 1 : 0;
}

function retryOutbox(pager: Pager, hal: Hal, item: OutboxItem): void {
  if (item.status !== "failed") return;
  sendPage(pager, hal, item.destId, item.body, item);
}

function sendBodyEdit(pager: Pager, hal: Hal): void {
  const body = textEditDraft(pager.edit);
  const dest = pager.composeDest;
  if (!dest) {
    pager.screen = "canned";
    pager.sel = 1;
    return;
  }
  if (!body) {
    pager.screen = "canned";
    pager.sel = 1;
    return;
  }
  sendPage(pager, hal, dest, body);
}

function cancelBodyEdit(pager: Pager): void {
  pager.screen = "canned";
  pager.sel = 1;
}

function cannedRows(pager: Pager): number {
  return cannedList(pager.canned).length + 2;
}

function saveCannedDraft(pager: Pager, hal: Hal): void {
  if (!cannedAdd(pager.canned, textEditDraft(pager.edit))) return;
  hal.vibe(80);
}

function dismissSent(pager: Pager, hal: Hal): void {
  if (pager.alert) {
    pager.screen = "alert";
    pager.flashStart = hal.nowMs();
    pager.flashMs = FLASH_MS;
    return;
  }
  pager.screen = "outbox";
  pager.sel = pager.outbox.length ? 1 : 0;
}

function onTurn(pager: Pager, dir: 1 | -1): void {
  if (
    pager.screen === "alert" ||
    pager.screen === "read" ||
    pager.screen === "pair" ||
    pager.screen === "sending" ||
    pager.screen === "sent" ||
    pager.screen === "failed" ||
    pager.screen === "outread"
  ) {
    return;
  }
  if (pager.screen === "nameedit" || pager.screen === "bodyedit") {
    textEditTurn(pager.edit, dir);
    return;
  }
  pager.sel += dir;
  if (pager.screen === "menu") clampSel(pager, MENU.length + 1);
  else if (pager.screen === "inbox") clampSel(pager, pager.inbox.length + 1);
  else if (pager.screen === "outbox") clampSel(pager, pager.outbox.length + 1);
  else if (pager.screen === "compose") {
    clampSel(pager, contactsList(pager.contacts).length + 1);
  } else if (pager.screen === "canned") clampSel(pager, cannedRows(pager));
  else if (pager.screen === "settings") clampSel(pager, 3);
}

function stopVibeTest(pager: Pager, hal: Hal): void {
  pager.vibeTest = false;
  hal.vibe(0);
}

function onBack(pager: Pager, hal: Hal): void {
  if (pager.screen === "sending") {
    pager.screen = "outbox";
    pager.sel = pager.outbox.length ? 1 : 0;
    return;
  }
  if (pager.screen === "pair") {
    pairAbort(pager.pair, hal);
    pager.screen = "menu";
    pager.sel = 0;
    pager.composeDest = null;
    return;
  }
  if (pager.screen === "nameedit") {
    cancelNameEdit(pager);
    return;
  }
  if (pager.screen === "bodyedit") {
    cancelBodyEdit(pager);
    return;
  }
  if (pager.screen === "settings") {
    stopVibeTest(pager, hal);
    pager.screen = "menu";
    pager.sel = MENU_COG;
    return;
  }
  if (pager.screen === "alert") {
    pager.screen = "read";
    pager.reading = 0;
    pager.alert = null;
    return;
  }
  if (pager.screen === "sent" || pager.screen === "failed") {
    dismissSent(pager, hal);
    return;
  }
  if (pager.screen === "canned") {
    pager.screen = "compose";
    pager.composeDest = null;
    pager.sel = 0;
    return;
  }
  if (pager.screen === "outread") {
    pager.screen = "outbox";
    pager.sel = Math.min(pager.reading + 1, pager.outbox.length);
    return;
  }
  if (pager.screen === "outbox") {
    pager.screen = "menu";
    pager.sel = 2;
    return;
  }
  if (
    pager.screen === "read" ||
    pager.screen === "inbox" ||
    pager.screen === "compose"
  ) {
    pager.screen = "menu";
    pager.sel = 0;
    pager.composeDest = null;
    return;
  }
}

function onPress(pager: Pager, hal: Hal): void {
  if (pager.screen === "pair" || pager.screen === "sending") return;

  if (pager.screen === "sent" || pager.screen === "failed") {
    dismissSent(pager, hal);
    return;
  }

  if (pager.screen === "alert" && pager.alert) {
    pager.reading = 0;
    pager.screen = "read";
    pager.alert = null;
    return;
  }
  if (pager.screen === "read") {
    pager.screen = "inbox";
    pager.sel = Math.min(pager.reading + 1, pager.inbox.length);
    return;
  }
  if (pager.screen === "menu") {
    if (pager.sel === 0) {
      pager.screen = "inbox";
      pager.sel = pager.inbox.length ? 1 : 0;
    } else if (pager.sel === 1) {
      pager.screen = "compose";
      pager.composeDest = null;
      pager.sel = contactsList(pager.contacts).length ? 1 : 0;
    } else if (pager.sel === 2) {
      pager.screen = "outbox";
      pager.sel = pager.outbox.length ? 1 : 0;
    } else if (pager.sel === 3) {
      pairStart(pager.pair, pager.identity, hal);
      if (pairActive(pager.pair)) {
        pager.screen = "pair";
        pager.sel = 0;
      }
    } else if (pager.sel === MENU_COG) {
      pager.screen = "settings";
      pager.sel = 1;
    }
    return;
  }
  if (pager.screen === "settings") {
    if (pager.sel === 0) {
      stopVibeTest(pager, hal);
      pager.screen = "menu";
      pager.sel = MENU_COG;
      return;
    }
    if (pager.sel === 1) {
      stopVibeTest(pager, hal);
      beginNameEdit(pager);
      return;
    }
    pager.vibeTest = true;
    hal.vibe(250);
    return;
  }
  if (pager.screen === "nameedit") {
    if (textEditPress(pager.edit) === "ok") saveNameEdit(pager);
    return;
  }
  if (pager.screen === "bodyedit") {
    const result = textEditPress(pager.edit);
    if (result === "ok") sendBodyEdit(pager, hal);
    else if (result === "save") saveCannedDraft(pager, hal);
    return;
  }
  if (pager.screen === "inbox") {
    if (pager.sel === 0) {
      pager.screen = "menu";
      pager.sel = 0;
      return;
    }
    const idx = pager.sel - 1;
    if (idx >= 0 && idx < pager.inbox.length) {
      pager.reading = idx;
      pager.screen = "read";
    }
    return;
  }
  if (pager.screen === "outbox") {
    if (pager.sel === 0) {
      pager.screen = "menu";
      pager.sel = 2;
      return;
    }
    const idx = pager.sel - 1;
    if (idx >= 0 && idx < pager.outbox.length) {
      const item = pager.outbox[idx];
      if (item && item.status === "failed") {
        retryOutbox(pager, hal, item);
        return;
      }
      pager.reading = idx;
      pager.screen = "outread";
    }
    return;
  }
  if (pager.screen === "outread") {
    const item = pager.outbox[pager.reading];
    if (item && item.status === "failed") {
      retryOutbox(pager, hal, item);
      return;
    }
    pager.screen = "outbox";
    pager.sel = Math.min(pager.reading + 1, pager.outbox.length);
    return;
  }
  if (pager.screen === "compose") {
    if (pager.sel === 0) {
      pager.screen = "menu";
      pager.sel = 1;
      pager.composeDest = null;
      return;
    }
    const list = contactsList(pager.contacts);
    const dest = list[pager.sel - 1];
    if (!dest) return;
    pager.composeDest = new Uint8Array(dest.nodeId);
    pager.screen = "canned";
    pager.sel = 1;
    return;
  }
  if (pager.screen === "canned") {
    if (pager.sel === 0) {
      pager.screen = "compose";
      pager.composeDest = null;
      pager.sel = 1;
      return;
    }
    if (pager.sel === 1) {
      beginBodyEdit(pager);
      return;
    }
    const dest = pager.composeDest;
    const body = cannedList(pager.canned)[pager.sel - 2];
    if (dest && body) sendPage(pager, hal, dest, body);
  }
}

export function pagerOnPage(pager: Pager, hal: Hal, page: Page): void {
  pushInbox(pager, page, hal);
  const alert: Page = { from: "", body: "", rssi: 0, receivedMs: 0 };
  copyPage(page, alert);
  pager.alert = alert;
  pager.flashStart = hal.nowMs();
  pager.flashMs = FLASH_MS;
  hal.vibe(250);
  hal.beep(80);
  if (pager.screen === "sending") return;
  pager.screen = "alert";
}

export function pagerGoMenu(pager: Pager): void {
  pager.screen = "menu";
  pager.sel = 0;
  pager.composeDest = null;
  pager.alert = null;
}

function header(buf: Uint8Array, title: string, hal: Hal): void {
  const { hh, mm } = hal.clock();
  const clock = `${pad2(hh)}:${pad2(mm)}`;
  drawText(buf, 1, 1, clip(title, 12));
  drawText(buf, DISPLAY_WIDTH - textWidth(clock) - 1, 1, clock);
  invertRect(buf, 0, 0, DISPLAY_WIDTH, CHAR_H + 2);
}

function drawMenu(pager: Pager, hal: Hal, buf: Uint8Array): void {
  header(buf, "PEEPLE", hal);
  const n = unreadCount(pager);
  const pending = pageBusy(pager.radio);
  const labels = [
    n ? `Inbox (${n})` : "Inbox",
    "Compose",
    pending ? "Outbox (*)" : "Outbox",
    "Add contact",
  ];
  for (let i = 0; i < labels.length; i++) {
    const y = 13 + i * 10;
    drawText(buf, 2, y, labels[i]!);
    if (i === pager.sel) invertRect(buf, 0, y - 1, DISPLAY_WIDTH, 9);
  }
  drawText(buf, 2, 55, `${hal.batteryPct()}%`);
  const cogX = DISPLAY_WIDTH - CHAR_W - 1;
  drawText(buf, cogX, 55, "⚙");
  if (pager.sel === MENU_COG) invertRect(buf, cogX - 2, 54, CHAR_W + 4, 9);
}

function drawInbox(pager: Pager, hal: Hal, buf: Uint8Array): void {
  header(buf, "INBOX", hal);
  const rows = [
    "< back",
    ...pager.inbox.map((p) =>
      clip(`${formatHm(p.receivedHh, p.receivedMm)} ${p.from}  ${p.body}`, cols() - 1),
    ),
  ];
  drawList(pager, buf, rows);
  if (pager.inbox.length === 0) drawText(buf, 14, 36, "no pages");
}

function drawOutbox(pager: Pager, hal: Hal, buf: Uint8Array): void {
  header(buf, "OUTBOX", hal);
  const rows = [
    "< back",
    ...pager.outbox.map((p) =>
      clip(`${outboxMark(p)} ${formatHm(p.sentHh, p.sentMm)} ${p.to}  ${p.body}`, cols() - 1),
    ),
  ];
  drawList(pager, buf, rows);
  if (pager.outbox.length === 0) drawText(buf, 14, 36, "no pages");
}

function drawOutRead(pager: Pager, hal: Hal, buf: Uint8Array): void {
  const p = pager.outbox[pager.reading];
  if (!p) {
    drawOutbox(pager, hal, buf);
    return;
  }
  const title =
    p.status === "sending"
      ? `retry ${pageOutboxAttempts(pager.radio)}/${PAGE_ATTEMPTS}`
      : p.status === "failed"
        ? "X  press to retry"
        : "delivered";
  drawText(buf, 2, 2, clip(title, cols()));
  invertRect(buf, 0, 0, DISPLAY_WIDTH, CHAR_H + 4);
  drawText(buf, 2, 14, clip(`To : ${p.to}`, cols()));
  drawText(
    buf,
    2,
    24,
    clip(`sent ${formatHm(p.sentHh, p.sentMm)}  ack ${formatHm(p.ackedHh, p.ackedMm)}`, cols()),
  );
  const lines = wrap(p.body, cols());
  let y = 36;
  for (const line of lines.slice(0, 3)) {
    drawText(buf, 2, y, line);
    y += CHAR_H + 1;
  }
}

function drawRead(pager: Pager, hal: Hal, buf: Uint8Array): void {
  const p = pager.inbox[pager.reading];
  if (!p) {
    drawInbox(pager, hal, buf);
    return;
  }
  header(buf, clip(p.from, 10), hal);
  drawText(buf, 2, 14, formatHm(p.receivedHh, p.receivedMm));
  const lines = wrap(p.body, cols());
  let y = 24;
  for (const line of lines.slice(0, 3)) {
    drawText(buf, 2, y, line);
    y += CHAR_H + 1;
  }
  const rssi = `${p.rssi} dBm`;
  drawText(buf, DISPLAY_WIDTH - textWidth(rssi) - 1, 54, rssi);
  hLine(buf, 0, 62, DISPLAY_WIDTH);
}

function drawList(
  pager: Pager,
  buf: Uint8Array,
  rows: string[],
): void {
  const top = 12;
  const vis = 4;
  let start = 0;
  if (pager.sel >= vis) start = pager.sel - vis + 1;
  for (let i = 0; i < vis; i++) {
    const idx = start + i;
    if (idx >= rows.length) break;
    const y = top + i * 12;
    drawText(buf, 2, y, rows[idx] ?? "");
    if (idx === pager.sel) invertRect(buf, 0, y - 1, DISPLAY_WIDTH, 10);
  }
}

function drawCompose(pager: Pager, hal: Hal, buf: Uint8Array): void {
  header(buf, "COMPOSE", hal);
  const list = contactsList(pager.contacts);
  if (list.length === 0) {
    drawList(pager, buf, ["< back"]);
    drawText(buf, 14, 36, "pair first");
    return;
  }
  drawList(pager, buf, ["< back", ...list.map((c) => clip(c.name, cols() - 1))]);
}

function drawCanned(pager: Pager, hal: Hal, buf: Uint8Array): void {
  const dest = pager.composeDest;
  const contact = dest ? contactsFindById(pager.contacts, dest) : undefined;
  header(buf, contact ? clip(contact.name, 12) : "COMPOSE", hal);
  drawList(pager, buf, ["< back", "Custom", ...cannedList(pager.canned)]);
}

function drawOutcome(pager: Pager, buf: Uint8Array, title: string): void {
  drawText(buf, 2, 2, title);
  invertRect(buf, 0, 0, DISPLAY_WIDTH, CHAR_H + 4);
  drawText(buf, 2, 16, clip(`To : ${pager.sentTo}`, cols()));
  const lines = wrap(pager.sentBody, cols());
  let y = 28;
  for (const line of lines.slice(0, 4)) {
    drawText(buf, 2, y, line);
    y += CHAR_H + 1;
  }
}

function drawSending(pager: Pager, buf: Uint8Array): void {
  drawOutcome(pager, buf, "Sending...");
}

function drawSent(pager: Pager, buf: Uint8Array): void {
  drawOutcome(pager, buf, "Message Sent!");
}

function drawFailed(pager: Pager, buf: Uint8Array): void {
  drawOutcome(pager, buf, "No receipt");
}

function drawPair(pager: Pager, hal: Hal, buf: Uint8Array): void {
  header(buf, "ADD", hal);
  const now = hal.nowMs();
  const rem = pairActive(pager.pair)
    ? elapsedU32(pairDeadlineMs(pager.pair), now)
    : 0;
  const secs = Math.ceil(rem / 1000);
  drawText(buf, 2, 16, `${secs}s`);
  drawText(buf, 2, 28, "hold together");
  const rssi = pairLastRssi(pager.pair);
  if (rssi !== undefined) drawText(buf, 2, 40, `${rssi} dBm`);
  const err = pairError(pager.pair);
  if (err === "try again") drawText(buf, 2, 52, "ERROR: Try again");
  else if (err) drawText(buf, 2, 52, clip(err, cols()));
}

function drawSettings(pager: Pager, hal: Hal, buf: Uint8Array): void {
  header(buf, "SETTINGS", hal);
  const vibe = pager.vibeTest ? "Vibe tester  ON" : "Vibe tester";
  drawList(pager, buf, ["< back", "Display name", vibe]);
  const name = identityGetDisplayName(pager.identity);
  drawText(buf, 14, 52, clip(name, cols() - 3));
}

function drawTextEdit(pager: Pager, buf: Uint8Array): void {
  textEditDraw(pager.edit, buf);
}

function drawAlert(pager: Pager, hal: Hal, buf: Uint8Array): void {
  const p = pager.alert;
  drawText(buf, 34, 2, "* PAGE *");
  invertRect(buf, 0, 0, DISPLAY_WIDTH, 11);
  if (!p) return;
  drawText(buf, 2, 16, clip(p.from, cols()));
  const lines = wrap(p.body, cols());
  let y = 28;
  for (const line of lines.slice(0, 3)) {
    drawText(buf, 2, y, line);
    y += CHAR_H + 1;
  }
  if (deadlineLive(hal.nowMs(), pager.flashStart, pager.flashMs)) {
    invertRect(buf, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  }
}

function bufEq(a: Uint8Array, b: Uint8Array): boolean {
  const n = a.length;
  if (n !== b.length) return false;
  for (let i = 0; i < n; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}

export function pagerInput(pager: Pager, hal: Hal): void {
  if (hal.backPoll()) onBack(pager, hal);

  for (;;) {
    const ev = hal.encoderPoll();
    if (ev === "none") break;
    if (pager.screen === "pair" || pager.screen === "sending") continue;
    if (ev === "cw") onTurn(pager, 1);
    else if (ev === "ccw") onTurn(pager, -1);
    else if (ev === "press") onPress(pager, hal);
    else if (ev === "release") {
      // Select is the press edge. Release is the other half of the same
      // click and must not fire a second action.
    }
  }
}

export function pagerRender(pager: Pager, hal: Hal): void {
  if (pager.vibeTest) {
    if (pager.screen === "settings") hal.vibe(250);
    else stopVibeTest(pager, hal);
  }
  const buf = pager.buf;
  clear(buf);
  if (pager.screen === "menu") drawMenu(pager, hal, buf);
  else if (pager.screen === "inbox") drawInbox(pager, hal, buf);
  else if (pager.screen === "outbox") drawOutbox(pager, hal, buf);
  else if (pager.screen === "outread") drawOutRead(pager, hal, buf);
  else if (pager.screen === "read") drawRead(pager, hal, buf);
  else if (pager.screen === "compose") drawCompose(pager, hal, buf);
  else if (pager.screen === "canned") drawCanned(pager, hal, buf);
  else if (pager.screen === "sending") drawSending(pager, buf);
  else if (pager.screen === "sent") drawSent(pager, buf);
  else if (pager.screen === "failed") drawFailed(pager, buf);
  else if (pager.screen === "pair") drawPair(pager, hal, buf);
  else if (pager.screen === "settings") drawSettings(pager, hal, buf);
  else if (pager.screen === "nameedit" || pager.screen === "bodyedit") {
    drawTextEdit(pager, buf);
  }
  else drawAlert(pager, hal, buf);

  const dst = hal.displayBuffer();
  if (bufEq(dst, buf)) return;
  dst.set(buf);
  hal.displayFlush();
}

export function pagerHandleText(pager: Pager, key: string): boolean {
  if (pager.screen !== "nameedit" && pager.screen !== "bodyedit") return false;
  return textEditHandleText(pager.edit, key);
}

export function pagerPollSend(pager: Pager, hal: Hal): void {
  const status = pageSendStatus(pager.radio);
  if (status !== "delivered" && status !== "failed") return;
  finishOutbox(pager, status === "delivered" ? "sent" : "failed", hal);
  pageClearStatus(pager.radio);
  if (status === "failed") {
    if (pager.screen === "sending") {
      pager.screen = "outbox";
      pager.sel = pager.outbox.length ? 1 : 0;
    }
    return;
  }
  if (
    pager.screen === "pair" ||
    pager.screen === "alert" ||
    pager.screen === "nameedit" ||
    pager.screen === "bodyedit"
  ) {
    return;
  }
  pager.screen = "sent";
}

export function pagerReset(pager: Pager): void {
  pager.screen = "menu";
  pager.sel = 0;
  pager.inbox = [];
  pager.outbox = [];
  pager.reading = 0;
  pager.alert = null;
  pager.flashStart = 0;
  pager.flashMs = 0;
  pager.composeDest = null;
  pager.sentTo = "";
  pager.sentBody = "";
  textEditReset(pager.edit);
  cannedReset(pager.canned);
  pager.vibeTest = false;
  clear(pager.buf);
}
