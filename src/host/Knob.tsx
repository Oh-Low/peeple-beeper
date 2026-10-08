import { useEffect, useRef, type PointerEvent } from "react";
import type { EncoderEvent } from "../firmware/hal";

const DETENT = 24;
const POINTER_SLOP_PX = 8;
const WHEEL_NOTCH = 100;

export function Knob({
  onEvent,
}: {
  onEvent: (ev: EncoderEvent) => void;
}) {
  const angle = useRef(0);
  const acc = useRef(0);
  const wheelAcc = useRef(0);
  const last = useRef<number | null>(null);
  const dragged = useRef(false);
  const origin = useRef<{ x: number; y: number } | null>(null);
  const capturedId = useRef<number | null>(null);
  const skipClick = useRef(false);
  const skipClickTimer = useRef<number>(0);
  const el = useRef<HTMLButtonElement>(null);
  const onEventRef = useRef(onEvent);
  onEventRef.current = onEvent;

  function emitTurn(delta: number): void {
    acc.current += delta;
    while (acc.current >= DETENT) {
      acc.current -= DETENT;
      angle.current += DETENT;
      onEventRef.current("cw");
    }
    while (acc.current <= -DETENT) {
      acc.current += DETENT;
      angle.current -= DETENT;
      onEventRef.current("ccw");
    }
    if (el.current) el.current.style.setProperty("--rot", `${angle.current}deg`);
  }

  function emitPressRelease(): void {
    onEventRef.current("press");
    onEventRef.current("release");
  }

  function pointerAngle(e: PointerEvent<HTMLButtonElement>): number {
    const r = e.currentTarget.getBoundingClientRect();
    return (
      (Math.atan2(
        e.clientY - (r.top + r.height / 2),
        e.clientX - (r.left + r.width / 2),
      ) *
        180) /
      Math.PI
    );
  }

  function clearPointer(): void {
    last.current = null;
    dragged.current = false;
    origin.current = null;
    capturedId.current = null;
  }

  function armSkipClick(): void {
    skipClick.current = true;
    window.clearTimeout(skipClickTimer.current);
    skipClickTimer.current = window.setTimeout(() => {
      skipClick.current = false;
    }, 0);
  }

  useEffect(() => {
    const node = el.current;
    if (!node) return;
    const onWheel = (e: WheelEvent) => {
      if (e.deltaY === 0) return;
      e.preventDefault();
      const notch =
        e.deltaMode === WheelEvent.DOM_DELTA_PIXEL ? WHEEL_NOTCH : 1;
      wheelAcc.current += e.deltaY;
      while (wheelAcc.current >= notch) {
        wheelAcc.current -= notch;
        emitTurn(DETENT);
      }
      while (wheelAcc.current <= -notch) {
        wheelAcc.current += notch;
        emitTurn(-DETENT);
      }
    };
    node.addEventListener("wheel", onWheel, { passive: false });
    return () => {
      node.removeEventListener("wheel", onWheel);
      window.clearTimeout(skipClickTimer.current);
    };
  }, []);

  return (
    <button
      ref={el}
      type="button"
      className="knob"
      aria-label="Rotary encoder. Drag or scroll to turn, click to press."
      onPointerDown={(e) => {
        if (e.button !== 0) return;
        if (capturedId.current !== null && capturedId.current !== e.pointerId) {
          return;
        }
        e.currentTarget.setPointerCapture(e.pointerId);
        capturedId.current = e.pointerId;
        last.current = pointerAngle(e);
        dragged.current = false;
        origin.current = { x: e.clientX, y: e.clientY };
        acc.current = 0;
        skipClick.current = false;
      }}
      onPointerMove={(e) => {
        if (capturedId.current !== e.pointerId) return;
        if (last.current === null || origin.current === null) return;
        const dx = e.clientX - origin.current.x;
        const dy = e.clientY - origin.current.y;
        if (dx * dx + dy * dy > POINTER_SLOP_PX * POINTER_SLOP_PX) {
          dragged.current = true;
        }
        const a = pointerAngle(e);
        let d = a - last.current;
        if (d > 180) d -= 360;
        if (d < -180) d += 360;
        last.current = a;
        if (dragged.current) emitTurn(d);
      }}
      onPointerUp={(e) => {
        if (capturedId.current !== e.pointerId) return;
        const wasDrag = dragged.current;
        if (e.currentTarget.hasPointerCapture(e.pointerId)) {
          e.currentTarget.releasePointerCapture(e.pointerId);
        }
        clearPointer();
        armSkipClick();
        if (!wasDrag) emitPressRelease();
      }}
      onPointerCancel={(e) => {
        if (capturedId.current !== e.pointerId) return;
        if (e.currentTarget.hasPointerCapture(e.pointerId)) {
          e.currentTarget.releasePointerCapture(e.pointerId);
        }
        clearPointer();
        armSkipClick();
      }}
      onLostPointerCapture={() => {
        last.current = null;
        dragged.current = false;
        origin.current = null;
        capturedId.current = null;
      }}
      onClick={() => {
        if (skipClick.current) {
          skipClick.current = false;
          return;
        }
        emitPressRelease();
      }}
    >
      <span className="knob-mark" />
    </button>
  );
}
