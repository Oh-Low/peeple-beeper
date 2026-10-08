import { useEffect, useRef, useState } from "react";
import {
  AIR_FILTER_ALL,
  formatAirLog,
  type AirFilter,
} from "./airlog";
import type { HostHal } from "./createHostHal";

export function TrafficLog({
  host,
  seq,
  label,
}: {
  host: HostHal;
  seq: number;
  label: string;
}) {
  const [filter, setFilter] = useState<AirFilter>(AIR_FILTER_ALL);
  const txRef = useRef<HTMLTextAreaElement>(null);
  const rxRef = useRef<HTMLTextAreaElement>(null);
  const tx = formatAirLog(host.airTx(), filter);
  const rx = formatAirLog(host.airRx(), filter);

  useEffect(() => {
    const txEl = txRef.current;
    const rxEl = rxRef.current;
    if (txEl) txEl.scrollTop = txEl.scrollHeight;
    if (rxEl) rxEl.scrollTop = rxEl.scrollHeight;
  }, [seq, tx, rx]);

  return (
    <div className="air-logs">
      <div className="air-filters">
        <FilterBox
          name={`${label}-page`}
          checked={filter.page}
          onChange={(page) => setFilter({ ...filter, page })}
        >
          PAGE
        </FilterBox>
        <FilterBox
          name={`${label}-ack`}
          checked={filter.ack}
          onChange={(ack) => setFilter({ ...filter, ack })}
        >
          ACK
        </FilterBox>
        <FilterBox
          name={`${label}-pair`}
          checked={filter.pair}
          onChange={(pair) => setFilter({ ...filter, pair })}
        >
          Pair
        </FilterBox>
        <button type="button" className="air-clear" onClick={() => host.airClear()}>
          Clear
        </button>
      </div>
      <label className="air-tap-label">
        TX
        <textarea
          ref={txRef}
          className="air-tap"
          readOnly
          value={tx}
          spellCheck={false}
          aria-label={`${label} TX air`}
        />
      </label>
      <label className="air-tap-label">
        RX
        <textarea
          ref={rxRef}
          className="air-tap"
          readOnly
          value={rx}
          spellCheck={false}
          aria-label={`${label} RX air`}
        />
      </label>
    </div>
  );
}

function FilterBox({
  name,
  checked,
  onChange,
  children,
}: {
  name: string;
  checked: boolean;
  onChange: (on: boolean) => void;
  children: string;
}) {
  return (
    <label className="air-filter">
      <input
        type="checkbox"
        name={name}
        checked={checked}
        onChange={(e) => onChange(e.target.checked)}
      />
      {children}
    </label>
  );
}