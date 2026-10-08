/** TypeScript twin of `hal.h`. Firmware may import this; the host implements it. */

export const DISPLAY_WIDTH = 128;
export const DISPLAY_HEIGHT = 64;
export const DISPLAY_PAGES = 8;
/** HAL-owned ST7567 buffer: `page * 128 + x`, bit `y & 7`. */
export const DISPLAY_BUF_LEN = DISPLAY_WIDTH * DISPLAY_PAGES;

/** NUL-terminated C twin of `from[16]`; max payload chars. */
export const PAGE_FROM_MAX = 15;
/** NUL-terminated C twin of `body[80]`; max payload chars. */
export const PAGE_BODY_MAX = 79;
/** Max raw air frame; C twin of `raw_frame_t.bytes[128]`. */
export const RAW_FRAME_MAX = 128;
/** On-device store partition: node_id, contacts, canned, inbox, outbox. */
export const NVM_BYTES = 100 * 1024;

export type EncoderEvent =
  | "none"
  | "cw"
  | "ccw"
  | "press"
  | "release";

export type Page = {
  from: string;
  body: string;
  rssi: number;
  receivedMs: number;
};

export type RawFrame = {
  bytes: Uint8Array;
  rssi: number;
  receivedMs: number;
};

export type Hal = {
  displayClear: () => void;
  displaySetPixel: (x: number, y: number, on: boolean) => void;
  /** HAL-owned `DISPLAY_BUF_LEN` bytes; ST7567 `page*128+x`, bit `y&7`. */
  displayBuffer: () => Uint8Array;
  displayFlush: () => void;

  /** Dequeue one encoder event, or `"none"` if empty. */
  encoderPoll: () => EncoderEvent;
  /** Consumed edge, not GPIO level. */
  backPoll: () => boolean;

  /** Dequeue one RX packet into `out`, or `false` if empty. */
  radioPollRx: (out: Page) => boolean;
  radioSend: (to: string, body: string) => void;

  /** Fill `buf` with CSPRNG bytes. Host: `crypto.getRandomValues`. */
  randomFill: (buf: Uint8Array) => void;
  /** Whisper = min TX power on real nRF. Host only stores a bool. */
  radioSetWhisper: (on: boolean) => void;
  /** Current whisper mode (firmware/tests). */
  radioWhisper: () => boolean;
  /** Copy and transmit raw bytes. Empty send is a no-op. */
  radioSendRaw: (bytes: Uint8Array) => void;
  /** Dequeue one raw frame into `out`, or `false` if empty. Copy on poll. */
  radioPollRaw: (out: RawFrame) => boolean;

  /**
   * Copy the store blob into `out` (caller provides `NVM_BYTES`).
   * Returns stored length, or 0 if empty.
   */
  nvmLoad: (out: Uint8Array) => number;
  /** Replace the store blob. False if `bytes.length > NVM_BYTES`. */
  nvmSave: (bytes: Uint8Array) => boolean;

  /** Monotonic ms from boot (`uint32` wrap). */
  nowMs: () => number;
  /** Wall-clock hours and minutes. */
  clock: () => { hh: number; mm: number };
  batteryPct: () => number;
  vibe: (ms: number) => void;
  beep: (ms: number) => void;
};
