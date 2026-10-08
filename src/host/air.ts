import type { HostHal } from "./createHostHal";

/** Host sim: packets stop at this separation. Firmware still has no range gate. */
export const RADIO_RANGE_FT = 100;
const RSSI_AT_ZERO = -30;
/** Linear dB drop across the full range. Unlink is hard at RADIO_RANGE_FT. */
const RSSI_DROP_OVER_RANGE = 60;

export function airDistanceFt(posA: number, posB: number): number {
  return Math.abs(posA - posB);
}

/** `null` = out of range (do not `pushRaw`). */
export function airRssi(distanceFt: number): number | null {
  if (!Number.isFinite(distanceFt) || distanceFt < 0 || distanceFt >= RADIO_RANGE_FT) {
    return null;
  }
  return Math.round(RSSI_AT_ZERO - (distanceFt / RADIO_RANGE_FT) * RSSI_DROP_OVER_RANGE);
}

/** Directed hop. `rssiFn` returns null when the pair is unlinked (no `pushRaw`). */
export type AirHop = {
  from: HostHal;
  to: HostHal;
  rssiFn: () => number | null;
};

/** Bidirectional pair; pass into `linkMesh`. Never includes loopback. */
export function linkAirPair(
  a: HostHal,
  b: HostHal,
  rssiFn: () => number | null,
): AirHop[] {
  return [
    { from: a, to: b, rssiFn },
    { from: b, to: a, rssiFn },
  ];
}

/**
 * Wire a mesh: each radio's TX fans out to its hops. Never loopback.
 * `rssiFn` null = unlinked — do not `pushRaw` (pages have no RSSI gate).
 * Pass `hosts` to clear `onTxRaw` on every radio first (add/remove).
 */
export function linkMesh(hops: AirHop[], hosts?: HostHal[]): void {
  if (hosts) {
    for (let i = 0; i < hosts.length; i++) hosts[i]!.onTxRaw = undefined;
  }
  const byFrom = new Map<HostHal, AirHop[]>();
  for (const hop of hops) {
    if (hop.from === hop.to) continue;
    const list = byFrom.get(hop.from);
    if (list) list.push(hop);
    else byFrom.set(hop.from, [hop]);
  }
  for (const [from, list] of byFrom) {
    from.onTxRaw = (bytes) => {
      for (const hop of list) {
        const rssi = hop.rssiFn();
        if (rssi === null) continue;
        hop.to.pushRaw(bytes, rssi);
      }
    };
  }
}

/** Full mesh of range-gated hops. `feet` is read live so sliders do not need a relink. */
export function hopsForPositions(
  units: { host: HostHal; feet: number }[],
): AirHop[] {
  const hops: AirHop[] = [];
  for (let i = 0; i < units.length; i++) {
    for (let j = i + 1; j < units.length; j++) {
      const a = units[i]!;
      const b = units[j]!;
      hops.push(
        ...linkAirPair(a.host, b.host, () => airRssi(airDistanceFt(a.feet, b.feet))),
      );
    }
  }
  return hops;
}

/** Wire two host radios: A's raw TX is B's RX (and the reverse). Never loopback. */
export function linkAir(
  a: HostHal,
  b: HostHal,
  rssiAB: () => number,
  rssiBA: () => number,
): void {
  linkMesh([
    { from: a, to: b, rssiFn: rssiAB },
    { from: b, to: a, rssiFn: rssiBA },
  ]);
}
