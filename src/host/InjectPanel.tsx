import { useState, type FormEvent } from "react";
import { PAGE_BODY_MAX, PAGE_FROM_MAX } from "../firmware/hal";
import type { TxLog } from "./createHostHal";

const PRESETS = [
  { from: "SAM", body: "you free for food?" },
  { from: "JESS", body: "running late, 10 min" },
  { from: "MOM", body: "call me when you can" },
  { from: "WORK", body: "meeting moved to 3" },
] as const;

export function InjectPanel({
  onReceive,
  lastTx,
}: {
  onReceive: (from: string, body: string, rssi: number) => void;
  lastTx: TxLog | null;
}) {
  const [from, setFrom] = useState("SAM");
  const [body, setBody] = useState("you free for food?");
  const [rssi, setRssi] = useState(-42);

  const emptyBody = body.trim().length === 0;

  function submit(e: FormEvent): void {
    e.preventDefault();
    const f = from.trim() || "???";
    const b = body.trim();
    if (!b) return;
    onReceive(f, b, rssi);
  }

  return (
    <form className="inject" onSubmit={submit}>
      <h2>Plaintext inbox inject</h2>
      <p className="hint">
        Leftover plaintext inject — inbox testing without crypto. Drops a
        page into device A&apos;s RX queue. Air range is the feet-from-origin
        fields on each pane, not this form.
      </p>
      <div className="presets">
        {PRESETS.map((p) => (
          <button
            key={p.from + p.body}
            type="button"
            className="preset"
            onClick={() => {
              setFrom(p.from);
              setBody(p.body);
            }}
          >
            {p.from}
          </button>
        ))}
      </div>
      <label>
        From
        <input
          value={from}
          maxLength={PAGE_FROM_MAX}
          onChange={(e) => setFrom(e.target.value)}
        />
      </label>
      <label>
        Message
        <textarea
          value={body}
          maxLength={PAGE_BODY_MAX}
          rows={3}
          onChange={(e) => setBody(e.target.value)}
        />
      </label>
      <label className="rssi">
        RSSI {rssi} dBm
        <input
          type="range"
          min={-120}
          max={-20}
          value={rssi}
          onChange={(e) => setRssi(Number(e.target.value))}
        />
      </label>
      <button type="submit" className="receive" disabled={emptyBody}>
        Receive page
      </button>
      {emptyBody ? (
        <p className="hint">Enter a message before receiving a page.</p>
      ) : null}
      {lastTx ? (
        <p className="tx" aria-live="polite">
          Last TX to {lastTx.to}: {lastTx.body}
        </p>
      ) : (
        <p className="tx muted">No outbound pages yet</p>
      )}
    </form>
  );
}
