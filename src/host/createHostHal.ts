import {
  DISPLAY_BUF_LEN,
  NVM_BYTES,
  PAGE_BODY_MAX,
  PAGE_FROM_MAX,
  RAW_FRAME_MAX,
  type EncoderEvent,
  type Hal,
  type Page,
  type RawFrame,
} from "../firmware/hal";
import type { AirEvent } from "./airlog";

const BATTERY_PCT_STUB = 87;
const BACK_Q_MAX = 4;
const RAW_TX_HEX_PREFIX = 8;
const AIR_LOG_MAX = 160;

export type TxLog = {
  to: string;
  body: string;
  at: number;
};

export type HostHal = {
  hal: Hal;
  pushEncoder: (ev: EncoderEvent) => void;
  pushBack: () => void;
  pushRx: (from: string, body: string, rssi?: number) => void;
  /** Enqueue a raw air frame for `radioPollRaw`. Separate from page `pushRx`. */
  pushRaw: (bytes: Uint8Array, rssi?: number) => void;
  buffer: () => Uint8Array;
  vibeUntil: () => number;
  beepUntil: () => number;
  lastTx: () => TxLog | null;
  airTx: () => readonly AirEvent[];
  airRx: () => readonly AirEvent[];
  airSeq: () => number;
  airClear: () => void;
  /** Set by `linkMesh` / `linkAir`. Invoked after `radioSendRaw` copies; never to self. */
  onTxRaw?: (bytes: Uint8Array) => void;
};

function clipPageField(s: string, max: number): string {
  return s.slice(0, max);
}

function copyRawBytes(bytes: Uint8Array): Uint8Array {
  const n = Math.min(bytes.length, RAW_FRAME_MAX);
  return bytes.slice(0, n);
}

function rawTxNote(bytes: Uint8Array): string {
  const n = Math.min(bytes.length, RAW_TX_HEX_PREFIX);
  let hex = "";
  for (let i = 0; i < n; i++) {
    hex += bytes[i]!.toString(16).padStart(2, "0");
  }
  if (bytes.length > RAW_TX_HEX_PREFIX) {
    return `${hex}… (${bytes.length}B)`;
  }
  return `${hex} (${bytes.length}B)`;
}

function noteAir(list: AirEvent[], ev: AirEvent): void {
  list.push(ev);
  if (list.length > AIR_LOG_MAX) list.shift();
}

function bytesToB64(bytes: Uint8Array): string {
  let s = "";
  for (let i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]!);
  return btoa(s);
}

function b64ToBytes(s: string): Uint8Array | undefined {
  try {
    const bin = atob(s);
    const out = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
    return out;
  } catch {
    return undefined;
  }
}

export function createHostHal(opts?: {
  onTxRaw?: (bytes: Uint8Array) => void;
  persistKey?: string;
}): HostHal {
  const buf = new Uint8Array(DISPLAY_BUF_LEN);
  const encQ: EncoderEvent[] = [];
  const rxQ: Page[] = [];
  const rawRxQ: RawFrame[] = [];
  const backQ: true[] = [];
  let displayGen = 0;
  let vibeUntil = 0;
  let beepUntil = 0;
  let lastTx: TxLog | null = null;
  let whisper = false;
  let airSeq = 0;
  const txAir: AirEvent[] = [];
  const rxAir: AirEvent[] = [];
  const hooks: { onTxRaw?: (bytes: Uint8Array) => void } = {
    onTxRaw: opts?.onTxRaw,
  };
  const t0 = Date.now();
  const nvm = new Uint8Array(NVM_BYTES);
  let nvmLen = 0;

  if (opts?.persistKey && typeof localStorage !== "undefined") {
    const raw = localStorage.getItem(opts.persistKey);
    if (raw) {
      const loaded = b64ToBytes(raw);
      if (loaded && loaded.length <= NVM_BYTES) {
        nvm.set(loaded);
        nvmLen = loaded.length;
      }
    }
  }

  const persistNvm = (): void => {
    if (!opts?.persistKey || typeof localStorage === "undefined") return;
    if (nvmLen === 0) {
      localStorage.removeItem(opts.persistKey);
      return;
    }
    localStorage.setItem(opts.persistKey, bytesToB64(nvm.subarray(0, nvmLen)));
  };

  const nowMs = () => Date.now() - t0;
  const wallMs = () => Date.now();

  const hal: Hal = {
    displayClear: () => {
      buf.fill(0);
    },
    displaySetPixel: (x, y, on) => {
      if (x < 0 || y < 0 || x >= 128 || y >= 64) return;
      const i = (y >> 3) * 128 + x;
      const bit = 1 << (y & 7);
      if (on) buf[i] |= bit;
      else buf[i] &= ~bit;
    },
    displayBuffer: () => buf,
    displayFlush: () => {
      // Host Screen blits every frame; gen is unused unless a consumer polls it.
      displayGen += 1;
    },

    encoderPoll: () => encQ.shift() ?? "none",
    backPoll: () => backQ.shift() ?? false,

    radioPollRx: (out) => {
      const p = rxQ.shift();
      if (!p) return false;
      out.from = p.from;
      out.body = p.body;
      out.rssi = p.rssi;
      out.receivedMs = p.receivedMs;
      return true;
    },
    radioSend: (to, body) => {
      lastTx = {
        to: clipPageField(to, PAGE_FROM_MAX),
        body: clipPageField(body, PAGE_BODY_MAX),
        at: wallMs(),
      };
    },

    randomFill: (fillBuf) => {
      crypto.getRandomValues(fillBuf as Uint8Array<ArrayBuffer>);
    },
    radioSetWhisper: (on) => {
      whisper = on;
    },
    radioWhisper: () => whisper,
    radioSendRaw: (bytes) => {
      if (bytes.length === 0) return;
      const copy = copyRawBytes(bytes);
      lastTx = {
        to: "raw",
        body: rawTxNote(copy),
        at: wallMs(),
      };
      airSeq += 1;
      noteAir(txAir, { at: nowMs(), rssi: null, bytes: copy });
      hooks.onTxRaw?.(copy);
    },
    radioPollRaw: (out) => {
      const f = rawRxQ.shift();
      if (!f) return false;
      out.bytes = new Uint8Array(f.bytes);
      out.rssi = f.rssi;
      out.receivedMs = f.receivedMs;
      return true;
    },

    nvmLoad: (out) => {
      const n = Math.min(nvmLen, out.length);
      if (n > 0) out.set(nvm.subarray(0, n));
      return nvmLen;
    },
    nvmSave: (bytes) => {
      if (bytes.length > NVM_BYTES) return false;
      nvm.fill(0);
      nvm.set(bytes);
      nvmLen = bytes.length;
      persistNvm();
      return true;
    },

    nowMs,
    clock: () => {
      const d = new Date();
      return { hh: d.getHours(), mm: d.getMinutes() };
    },
    batteryPct: () => BATTERY_PCT_STUB,
    vibe: (ms) => {
      // Wall clock: App compares vibeUntil/beepUntil to Date.now().
      vibeUntil = wallMs() + ms;
    },
    beep: (ms) => {
      beepUntil = wallMs() + ms;
    },
  };

  return {
    hal,
    pushEncoder: (ev) => {
      if (ev !== "none") encQ.push(ev);
    },
    pushBack: () => {
      if (backQ.length < BACK_Q_MAX) backQ.push(true);
    },
    pushRx: (from, body, rssi = -40) => {
      rxQ.push({
        from: clipPageField(from, PAGE_FROM_MAX),
        body: clipPageField(body, PAGE_BODY_MAX),
        rssi,
        receivedMs: nowMs(),
      });
    },
    pushRaw: (bytes, rssi = -40) => {
      const copy = copyRawBytes(bytes);
      const at = nowMs();
      airSeq += 1;
      noteAir(rxAir, { at, rssi, bytes: copy });
      rawRxQ.push({
        bytes: copy,
        rssi,
        receivedMs: at,
      });
    },
    buffer: () => buf,
    vibeUntil: () => vibeUntil,
    beepUntil: () => beepUntil,
    lastTx: () => lastTx,
    airTx: () => txAir,
    airRx: () => rxAir,
    airSeq: () => airSeq,
    airClear: () => {
      txAir.length = 0;
      rxAir.length = 0;
      airSeq += 1;
    },
    get onTxRaw() {
      return hooks.onTxRaw;
    },
    set onTxRaw(fn) {
      hooks.onTxRaw = fn;
    },
  };
}
