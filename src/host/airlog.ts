import {
  FRAME_HELLO,
  FRAME_HELLO_ACK,
  FRAME_PAGE,
  FRAME_PAGE_ACK,
  FRAME_PAIR_LEAVE,
  frameDecodePage,
  frameDecodePageAck,
  frameType,
} from "../firmware/frames";

export type AirEvent = {
  at: number;
  rssi: number | null;
  bytes: Uint8Array;
};

export type AirFilter = {
  page: boolean;
  ack: boolean;
  pair: boolean;
};

export const AIR_FILTER_ALL: AirFilter = { page: true, ack: true, pair: true };

function hexN(bytes: Uint8Array, n: number): string {
  let s = "";
  const m = Math.min(n, bytes.length);
  for (let i = 0; i < m; i++) s += bytes[i]!.toString(16).padStart(2, "0");
  return s;
}

function typeName(kind: number | undefined): string {
  if (kind === FRAME_PAGE) return "PAGE";
  if (kind === FRAME_PAGE_ACK) return "ACK";
  if (kind === FRAME_HELLO) return "HELLO";
  if (kind === FRAME_HELLO_ACK) return "HELLO_ACK";
  if (kind === FRAME_PAIR_LEAVE) return "LEAVE";
  if (kind === undefined) return "?";
  return `0x${kind.toString(16)}`;
}

export function airEventMatches(ev: AirEvent, filter: AirFilter): boolean {
  const kind = frameType(ev.bytes);
  if (kind === FRAME_PAGE) return filter.page;
  if (kind === FRAME_PAGE_ACK) return filter.ack;
  if (kind === FRAME_HELLO || kind === FRAME_HELLO_ACK || kind === FRAME_PAIR_LEAVE) {
    return filter.pair;
  }
  return true;
}

export function formatAirEvent(ev: AirEvent): string {
  const sec = (ev.at / 1000).toFixed(3);
  const kind = frameType(ev.bytes);
  let extra = `${ev.bytes.length}B`;
  if (kind === FRAME_PAGE) {
    const p = frameDecodePage(ev.bytes);
    if (p) extra = `ttl=${p.hopTtl} n=${hexN(p.nonce, 4)} ${extra}`;
  } else if (kind === FRAME_PAGE_ACK) {
    const p = frameDecodePageAck(ev.bytes);
    if (p) extra = `ttl=${p.hopTtl} n=${hexN(p.nonce, 4)} ${extra}`;
  }
  const rssi = ev.rssi === null ? "" : ` ${ev.rssi}dBm`;
  return `${sec}  ${typeName(kind)}  ${extra}${rssi}`;
}

export function formatAirLog(events: readonly AirEvent[], filter: AirFilter): string {
  const lines: string[] = [];
  for (let i = 0; i < events.length; i++) {
    const ev = events[i]!;
    if (!airEventMatches(ev, filter)) continue;
    lines.push(formatAirEvent(ev));
  }
  return lines.join("\n");
}
