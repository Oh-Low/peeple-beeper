import { useEffect, useRef } from "react";

const W = 128;
const H = 64;

const ON = [28, 34, 24] as const;
const OFF = [207, 212, 192] as const;

function blit(img: ImageData, buf: Uint8Array): void {
  const d = img.data;
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const on = buf[(y >> 3) * W + x] & (1 << (y & 7));
      const i = (y * W + x) * 4;
      const c = on ? ON : OFF;
      d[i] = c[0];
      d[i + 1] = c[1];
      d[i + 2] = c[2];
      d[i + 3] = 255;
    }
  }
}

export function Screen({
  buffer,
  tick,
  vibeUntil,
  beepUntil,
  vibeDot,
}: {
  buffer: () => Uint8Array;
  tick?: () => void;
  vibeUntil?: () => number;
  beepUntil?: () => number;
  vibeDot?: { current: HTMLElement | null };
}) {
  const wrap = useRef<HTMLDivElement>(null);
  const canvas = useRef<HTMLCanvasElement>(null);
  const ctxRef = useRef<CanvasRenderingContext2D | null>(null);
  const imgRef = useRef<ImageData | null>(null);
  const tickRef = useRef(tick);
  const bufferRef = useRef(buffer);
  const vibeUntilRef = useRef(vibeUntil);
  const beepUntilRef = useRef(beepUntil);
  const vibeDotRef = useRef(vibeDot);
  tickRef.current = tick;
  bufferRef.current = buffer;
  vibeUntilRef.current = vibeUntil;
  beepUntilRef.current = beepUntil;
  vibeDotRef.current = vibeDot;

  useEffect(() => {
    const el = canvas.current;
    if (!el) return;
    const ctx = ctxRef.current ?? el.getContext("2d");
    if (!ctx) return;
    ctxRef.current = ctx;
    if (!imgRef.current) imgRef.current = ctx.createImageData(W, H);

    let cancelled = false;
    let id = 0;
    const loop = () => {
      if (cancelled) return;
      tickRef.current?.();

      const img = imgRef.current;
      if (img) {
        blit(img, bufferRef.current());
        ctx.putImageData(img, 0, 0);
      }

      const now = Date.now();
      const vibrating = (vibeUntilRef.current?.() ?? 0) > now;
      const buzzing =
        vibrating || (beepUntilRef.current?.() ?? 0) > now;
      wrap.current?.classList.toggle("buzzing", buzzing);
      vibeDotRef.current?.current?.classList.toggle("on", vibrating);

      id = requestAnimationFrame(loop);
    };
    id = requestAnimationFrame(loop);
    return () => {
      cancelled = true;
      cancelAnimationFrame(id);
    };
  }, []);

  return (
    <div ref={wrap} className="lcd">
      <canvas
        ref={canvas}
        className="lcd-face"
        width={W}
        height={H}
        aria-label="128 by 64 pager display"
      />
    </div>
  );
}
