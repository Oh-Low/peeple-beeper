import { useEffect, useRef, useState } from "react";
import { beeperHandleText, beeperReset, beeperTick, createBeeper, type Beeper } from "./firmware/beeper";
import { storeLoad, storeSave } from "./firmware/store";
import { airDistanceFt, airRssi, hopsForPositions, linkMesh, RADIO_RANGE_FT } from "./host/air";
import { createHostHal, type HostHal, type TxLog } from "./host/createHostHal";
import {
  forgetEmulatorUnit,
  introduceEmulatorUnit,
  seedEmulatorDirectory,
  type EmulatorUnit,
} from "./host/seed";
import { ArtStudio } from "./host/ArtStudio";
import { InjectPanel } from "./host/InjectPanel";
import { TrafficLog } from "./host/TrafficLog";
import { Knob } from "./host/Knob";
import { Screen } from "./host/Screen";

export const MAX_RADIOS = 16;
const MIN_RADIOS = 1;
const SPACING_FT = 75;

type Radio = {
  id: number;
  name: string;
  feet: number;
  host: HostHal;
  beeper: Beeper;
};

type Bench = {
  radios: Radio[];
  nextId: number;
  nextName: number;
};

function isTextField(target: EventTarget | null): boolean {
  return target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement;
}

function isButtonOrLink(target: EventTarget | null): boolean {
  return target instanceof Element && target.closest("button, a") !== null;
}

export function emulatorRadioName(seq: number): string {
  const letter = String.fromCharCode(65 + (seq % 26));
  const cycle = Math.floor(seq / 26);
  return cycle === 0 ? letter : `${letter}${cycle + 1}`;
}

function asDir(r: Radio): EmulatorUnit {
  return { beeper: r.beeper, name: r.name, hal: r.host.hal };
}

function relink(radios: Radio[]): void {
  const hosts = radios.map((r) => r.host);
  linkMesh(hopsForPositions(radios), hosts);
}

function persistRadios(radios: Radio[]): void {
  for (let i = 0; i < radios.length; i++) {
    const r = radios[i]!;
    storeSave(r.beeper, r.host.hal);
  }
}

function makeRadio(id: number, name: string, feet: number): Radio {
  const host = createHostHal({ persistKey: `peeple-beeper-nvm-v1-${id}` });
  const beeper = createBeeper();
  storeLoad(beeper, host.hal);
  beeper.nvmReady = true;
  return { id, name, feet, host, beeper };
}

function createBench(): Bench {
  const radios = [
    makeRadio(0, emulatorRadioName(0), 0),
    makeRadio(1, emulatorRadioName(1), 75),
    makeRadio(2, emulatorRadioName(2), 150),
  ];
  seedEmulatorDirectory(radios.map(asDir));
  persistRadios(radios);
  relink(radios);
  return { radios, nextId: 3, nextName: 3 };
}

function hearsList(radio: Radio, radios: Radio[]): string {
  const names: string[] = [];
  for (let i = 0; i < radios.length; i++) {
    const other = radios[i]!;
    if (other.id === radio.id) continue;
    if (airRssi(airDistanceFt(radio.feet, other.feet)) === null) continue;
    names.push(other.name);
  }
  if (names.length === 0) return "hears no one";
  return `hears ${names.join(", ")}`;
}

function parseFeet(raw: string): number | undefined {
  if (raw.trim() === "") return 0;
  const n = Number(raw);
  if (!Number.isFinite(n) || n < 0) return undefined;
  return Math.round(n);
}

function findRadio(radios: Radio[], id: number): Radio | undefined {
  for (let i = 0; i < radios.length; i++) {
    if (radios[i]!.id === id) return radios[i];
  }
  return undefined;
}

export default function App() {
  const bench = useRef<Bench | null>(null);
  bench.current ??= createBench();
  const b = bench.current;
  const focusRef = useRef(b.radios[0]!.id);
  const [, setRev] = useState(0);
  const bump = () => setRev((n) => n + 1);

  const [focus, setFocus] = useState(focusRef.current);
  const [view, setView] = useState<"emu" | "art">("emu");
  const viewRef = useRef(view);
  viewRef.current = view;
  const [lastTx, setLastTx] = useState<TxLog | null>(null);
  const lastSampleRef = useRef(0);

  const radios = b.radios;

  const focusDevice = (id: number) => {
    focusRef.current = id;
    setFocus(id);
  };

  const setFeet = (id: number, value: number) => {
    const r = findRadio(b.radios, id);
    if (!r) return;
    r.feet = value;
    bump();
  };

  const addRadio = () => {
    if (b.radios.length >= MAX_RADIOS) return;
    let maxFeet = 0;
    for (let i = 0; i < b.radios.length; i++) {
      if (b.radios[i]!.feet > maxFeet) maxFeet = b.radios[i]!.feet;
    }
    const radio = makeRadio(b.nextId, emulatorRadioName(b.nextName), maxFeet + SPACING_FT);
    b.nextId += 1;
    b.nextName += 1;
    introduceEmulatorUnit(b.radios.map(asDir), asDir(radio));
    b.radios.push(radio);
    persistRadios(b.radios);
    relink(b.radios);
    bump();
  };

  const removeRadio = (id: number) => {
    if (b.radios.length <= MIN_RADIOS) return;
    const idx = b.radios.findIndex((r) => r.id === id);
    if (idx < 0) return;
    const gone = b.radios[idx]!;
    const remaining = b.radios.filter((r) => r.id !== id);
    forgetEmulatorUnit(remaining.map(asDir), asDir(gone));
    beeperReset(gone.beeper, gone.host.hal);
    persistRadios(remaining);
    gone.host.onTxRaw = undefined;
    b.radios = remaining;
    relink(b.radios);
    if (focusRef.current === id) {
      const next = remaining[0]!;
      focusRef.current = next.id;
      setFocus(next.id);
    }
    bump();
  };

  const tickSlot = (slot: Radio) => {
    beeperTick(slot.beeper, slot.host.hal);
    const now = Date.now();
    if (now - lastSampleRef.current < 100) return;
    lastSampleRef.current = now;
    const focused = findRadio(b.radios, focusRef.current) ?? b.radios[0];
    if (!focused) return;
    const tx = focused.host.lastTx();
    setLastTx((prev) => (prev === tx ? prev : tx));
  };

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.repeat) return;
      if (isTextField(e.target)) return;
      if (viewRef.current === "art") return;

      const slot = findRadio(b.radios, focusRef.current) ?? b.radios[0];
      if (!slot) return;
      const host = slot.host;

      if (e.key === "Backspace" || (e.key.length === 1 && e.key !== "Enter")) {
        if (beeperHandleText(slot.beeper, e.key)) {
          e.preventDefault();
          return;
        }
      }

      if (e.key === "ArrowRight" || e.key === "ArrowDown") {
        e.preventDefault();
        host.pushEncoder("cw");
      } else if (e.key === "ArrowLeft" || e.key === "ArrowUp") {
        e.preventDefault();
        host.pushEncoder("ccw");
      } else if (e.key === "Enter" || e.key === " ") {
        if (isButtonOrLink(e.target)) return;
        e.preventDefault();
        host.pushEncoder("press");
        host.pushEncoder("release");
      } else if (e.key === "Escape") {
        e.preventDefault();
        host.pushBack();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  const focused = findRadio(radios, focus) ?? radios[0];

  return (
    <div className={view === "art" ? "app studio" : "app"}>
      <header className="brand">
        <h1>PEEPLE BEEPER</h1>
        <p>Host emulator · 128×64 FSTN · firmware in src/firmware</p>
        <nav className="ws-tabs" aria-label="Workspace">
          <button
            type="button"
            className={view === "emu" ? "ws-tab selected" : "ws-tab"}
            aria-pressed={view === "emu"}
            onClick={() => setView("emu")}
          >
            Emulator
          </button>
          <button
            type="button"
            className={view === "art" ? "ws-tab selected" : "ws-tab"}
            aria-pressed={view === "art"}
            onClick={() => setView("art")}
          >
            Art studio
          </button>
        </nav>
      </header>

      <div className="workspace" hidden={view !== "emu"}>
        <div className="bench-bar">
          <p className="air-legend">
            Distance from origin 0. Radio stops at {RADIO_RANGE_FT} ft. Default A/B/C at 0 / 75 /
            150 so A reaches C only through B.
          </p>
          <button
            type="button"
            className="bench-add"
            onClick={addRadio}
            disabled={radios.length >= MAX_RADIOS}
          >
            Add radio
          </button>
        </div>

        <div className="devices">
          {radios.map((radio) => (
            <DeviceChrome
              key={radio.id}
              label={radio.name}
              feet={radio.feet}
              hears={hearsList(radio, radios)}
              focused={focus === radio.id}
              canRemove={radios.length > MIN_RADIOS}
              slot={radio}
              tick={() => tickSlot(radio)}
              onFocusDevice={() => focusDevice(radio.id)}
              onFeet={(n) => setFeet(radio.id, n)}
              onRemove={() => removeRadio(radio.id)}
            />
          ))}
        </div>

        <InjectPanel
          lastTx={lastTx}
          onReceive={(from, body, rssi) => focused?.host.pushRx(from, body, rssi)}
        />
      </div>

      {view === "art" ? <ArtStudio /> : null}
    </div>
  );
}

function DeviceChrome({
  label,
  feet,
  hears,
  focused,
  canRemove,
  slot,
  tick,
  onFocusDevice,
  onFeet,
  onRemove,
}: {
  label: string;
  feet: number;
  hears: string;
  focused: boolean;
  canRemove: boolean;
  slot: Radio;
  tick: () => void;
  onFocusDevice: () => void;
  onFeet: (n: number) => void;
  onRemove: () => void;
}) {
  const vibeDot = useRef<HTMLSpanElement>(null);
  const [airSeq, setAirSeq] = useState(0);
  const runTick = () => {
    tick();
    const n = slot.host.airSeq();
    setAirSeq((prev) => (prev === n ? prev : n));
  };
  return (
    <div
      className={focused ? "device focused" : "device"}
      tabIndex={0}
      onPointerDown={onFocusDevice}
      onFocus={onFocusDevice}
    >
      <div className="device-head">
        <p className="device-tag">{label}</p>
        <span ref={vibeDot} className="vibe-dot" title="Vibrating" />
        {canRemove ? (
          <button type="button" className="radio-remove" onClick={onRemove}>
            Remove
          </button>
        ) : null}
      </div>
      <label className="range-ctl">
        from 0
        <input
          type="number"
          min={0}
          step={1}
          value={feet}
          aria-label={`${label} distance from origin`}
          onChange={(e) => {
            const n = parseFeet(e.target.value);
            if (n === undefined) return;
            onFeet(n);
          }}
        />
        ft
      </label>
      <p className="hears">{hears}</p>
      <Screen
        buffer={slot.host.buffer}
        tick={runTick}
        vibeUntil={slot.host.vibeUntil}
        beepUntil={slot.host.beepUntil}
        vibeDot={vibeDot}
      />
      <div className="controls">
        <button type="button" className="back" onClick={() => slot.host.pushBack()}>
          Back
        </button>
        <Knob onEvent={(ev) => slot.host.pushEncoder(ev)} />
      </div>
      <TrafficLog host={slot.host} seq={airSeq} label={label} />
    </div>
  );
}