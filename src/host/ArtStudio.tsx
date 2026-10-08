import {
  useEffect,
  useRef,
  useState,
  type ChangeEvent,
  type PointerEvent as ReactPointerEvent,
  type RefObject,
} from "react";
import {
  ART_H_MAX,
  ART_W_MAX,
  artAddAsset,
  artAddFrame,
  artExportTs,
  artFlood,
  artIdent,
  artInvert,
  artLine,
  artLoad,
  artParse,
  artRect,
  artRemoveAsset,
  artRemoveFrame,
  artRename,
  artSave,
  artSerialize,
  artSet,
  artSetDelay,
  cloneFrame,
  createAsset,
  createLibrary,
  type ArtAsset,
  type ArtFrame,
  type ArtLibrary,
  type ArtTool,
} from "./art";

const ZOOM_STEPS = [2, 4, 6, 8, 12, 16, 24, 32] as const;
const ZOOM_DEFAULT = 8;
const INK_RGB = [28, 34, 24] as const;
const PAPER_RGB = [207, 212, 192] as const;

const TOOLS: { id: ArtTool; label: string }[] = [
  { id: "pencil", label: "Pencil" },
  { id: "eraser", label: "Eraser" },
  { id: "fill", label: "Fill" },
  { id: "line", label: "Line" },
  { id: "rect", label: "Rect" },
  { id: "rectfill", label: "Rect fill" },
];

function toolLabel(tool: ArtTool): string {
  return TOOLS.find((t) => t.id === tool)?.label ?? tool;
}

function isShape(tool: ArtTool): boolean {
  return tool === "line" || tool === "rect" || tool === "rectfill";
}

function nearestZoom(n: number): number {
  let best: number = ZOOM_STEPS[0]!;
  let dist = Math.abs(n - best);
  for (let i = 1; i < ZOOM_STEPS.length; i++) {
    const z = ZOOM_STEPS[i]!;
    const d = Math.abs(n - z);
    if (d < dist) {
      best = z;
      dist = d;
    }
  }
  return best;
}

function zoomStep(current: number, dir: 1 | -1): number {
  const z = nearestZoom(current);
  const i = ZOOM_STEPS.indexOf(z as (typeof ZOOM_STEPS)[number]);
  const j = Math.max(0, Math.min(ZOOM_STEPS.length - 1, i + dir));
  return ZOOM_STEPS[j]!;
}

function markAnim(asset: ArtAsset): void {
  if (asset.frames.length > 1) asset.kind = "anim";
}

function snapshot(lib: ArtLibrary): ArtLibrary {
  return { version: 1, assets: lib.assets.slice() };
}

function findAsset(lib: ArtLibrary, id: string | null): ArtAsset | undefined {
  if (!id) return undefined;
  return lib.assets.find((a) => a.id === id);
}

function blitPixels(
  ctx: CanvasRenderingContext2D,
  pixels: Uint8Array,
  w: number,
  h: number,
): void {
  const img = ctx.createImageData(w, h);
  const d = img.data;
  for (let i = 0; i < w * h; i++) {
    const on = pixels[i] !== 0;
    const o = i * 4;
    if (on) {
      d[o] = INK_RGB[0];
      d[o + 1] = INK_RGB[1];
      d[o + 2] = INK_RGB[2];
    } else {
      d[o] = PAPER_RGB[0];
      d[o + 1] = PAPER_RGB[1];
      d[o + 2] = PAPER_RGB[2];
    }
    d[o + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

function paintCanvas(el: HTMLCanvasElement | null, pixels: Uint8Array, w: number, h: number): void {
  if (!el) return;
  if (el.width !== w) el.width = w;
  if (el.height !== h) el.height = h;
  const ctx = el.getContext("2d");
  if (!ctx) return;
  blitPixels(ctx, pixels, w, h);
}

type DragStroke = {
  x0: number;
  y0: number;
  x1: number;
  y1: number;
  mode: "paint" | "shape";
};

function strokePixels(
  asset: ArtAsset,
  frame: ArtFrame,
  tool: ArtTool,
  ink: 0 | 1,
  drag: DragStroke | null,
): Uint8Array {
  if (drag && drag.mode === "shape" && isShape(tool)) {
    const tmp = cloneFrame(frame);
    applyShape(tmp, asset.w, asset.h, tool, ink, drag.x0, drag.y0, drag.x1, drag.y1);
    return tmp.pixels;
  }
  return frame.pixels;
}

function redrawArtCanvases(
  editor: HTMLCanvasElement | null,
  lcd: HTMLCanvasElement | null,
  asset: ArtAsset | undefined,
  frame: ArtFrame | undefined,
  tool: ArtTool,
  ink: 0 | 1,
  drag: DragStroke | null,
  playing: boolean,
  playFrame: number,
): void {
  if (!asset || !frame) {
    const empty = new Uint8Array(8 * 8);
    paintCanvas(editor, empty, 8, 8);
    paintCanvas(lcd, empty, 8, 8);
    return;
  }
  const edit = strokePixels(asset, frame, tool, ink, drag);
  paintCanvas(editor, edit, asset.w, asset.h);
  const lcdFrame = playing ? asset.frames[playFrame] ?? frame : undefined;
  const lcdPixels = playing && lcdFrame ? lcdFrame.pixels : edit;
  paintCanvas(lcd, lcdPixels, asset.w, asset.h);
}

function applyShape(
  frame: ArtFrame,
  w: number,
  h: number,
  tool: ArtTool,
  ink: 0 | 1,
  x0: number,
  y0: number,
  x1: number,
  y1: number,
): void {
  if (tool === "line") artLine(frame, w, h, x0, y0, x1, y1, ink);
  else if (tool === "rect") artRect(frame, w, h, x0, y0, x1, y1, ink, false);
  else if (tool === "rectfill") artRect(frame, w, h, x0, y0, x1, y1, ink, true);
}

function mapPixel(
  el: HTMLCanvasElement,
  clientX: number,
  clientY: number,
  w: number,
  h: number,
  dragging: boolean,
): { x: number; y: number } | null {
  const r = el.getBoundingClientRect();
  if (r.width <= 0 || r.height <= 0) return null;
  const x = Math.floor(((clientX - r.left) / r.width) * w);
  const y = Math.floor(((clientY - r.top) / r.height) * h);
  if (x >= 0 && y >= 0 && x < w && y < h) return { x, y };
  if (!dragging) return null;
  return {
    x: Math.max(0, Math.min(w - 1, x)),
    y: Math.max(0, Math.min(h - 1, y)),
  };
}

export function ArtStudio() {
  const [lib, setLib] = useState<ArtLibrary>(() => {
    try {
      return artLoad();
    } catch {
      return createLibrary();
    }
  });
  const [selectedId, setSelectedId] = useState<string | null>(
    () => artLoad().assets[0]?.id ?? null,
  );
  const [frameIndex, setFrameIndex] = useState(0);
  const [tool, setTool] = useState<ArtTool>("pencil");
  const [ink, setInk] = useState<0 | 1>(1);
  const [playing, setPlaying] = useState(false);
  const [playFrame, setPlayFrame] = useState(0);
  const [cursor, setCursor] = useState<{ x: number; y: number } | null>(null);
  const [showGrid, setShowGrid] = useState(true);
  const [zoom, setZoom] = useState(ZOOM_DEFAULT);
  const [rename, setRename] = useState(() => artLoad().assets[0]?.name ?? "");
  const [rev, setRev] = useState(0);
  const [libraryNote, setLibraryNote] = useState<string | null>(null);
  const [copiedLib, setCopiedLib] = useState(false);
  const centerRef = useRef<HTMLDivElement>(null);
  const fileRef = useRef<HTMLInputElement>(null);
  const libJsonRef = useRef<HTMLTextAreaElement>(null);

  const asset = findAsset(lib, selectedId);
  const frame = asset?.frames[frameIndex] ?? asset?.frames[0];

  const playId = asset?.id;
  const playDelay = asset?.delayMs;
  const playCount = asset?.frames.length ?? 0;

  useEffect(() => {
    if (!playing || !playId || playCount < 1) return;
    const n = playCount;
    const id = window.setInterval(() => {
      setPlayFrame((i) => (i + 1) % n);
    }, playDelay ?? 120);
    return () => window.clearInterval(id);
  }, [playing, playId, playDelay, playCount]);

  function persist(): void {
    artSave(lib);
    setLib(snapshot(lib));
    setRev((n) => n + 1);
  }

  function applyLibrary(next: ArtLibrary, note: string): void {
    artSave(next);
    setLib(snapshot(next));
    setRev((n) => n + 1);
    setPlaying(false);
    const first = next.assets[0];
    setSelectedId(first?.id ?? null);
    setFrameIndex(0);
    setPlayFrame(0);
    setRename(first?.name ?? "");
    setLibraryNote(note);
  }

  function saveLibraryFile(): void {
    const json = artSerialize(lib, true);
    const blob = new Blob([json], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "peeple-beeper-art.json";
    a.click();
    URL.revokeObjectURL(url);
    setLibraryNote(`Saved ${lib.assets.length} asset${lib.assets.length === 1 ? "" : "s"}`);
  }

  async function copyLibraryJson(): Promise<void> {
    const json = artSerialize(lib, true);
    try {
      await navigator.clipboard.writeText(json);
      setCopiedLib(true);
      setLibraryNote(`Copied ${lib.assets.length} asset${lib.assets.length === 1 ? "" : "s"} as JSON`);
      window.setTimeout(() => setCopiedLib(false), 1200);
    } catch {
      setCopiedLib(false);
      const ta = libJsonRef.current;
      if (ta) {
        ta.focus();
        ta.select();
      }
      setLibraryNote("Select the library JSON below and copy it");
    }
  }

  function onPickLibraryFile(e: ChangeEvent<HTMLInputElement>): void {
    const file = e.target.files?.[0];
    e.target.value = "";
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => {
      const text = typeof reader.result === "string" ? reader.result : "";
      const parsed = artParse(text);
      if (!parsed) {
        setLibraryNote("Could not load that file");
        return;
      }
      applyLibrary(parsed, `Loaded ${parsed.assets.length} asset${parsed.assets.length === 1 ? "" : "s"}`);
    };
    reader.onerror = () => setLibraryNote("Could not read that file");
    reader.readAsText(file);
  }

  function selectAsset(id: string): void {
    setPlaying(false);
    setSelectedId(id);
    setFrameIndex(0);
    setPlayFrame(0);
    setRename(findAsset(lib, id)?.name ?? "");
  }

  function newAsset(name: string, w: number, h: number, kind: "icon" | "screen"): void {
    const next = createAsset(name, w, h, kind);
    if (!next) return;
    if (!artAddAsset(lib, next)) return;
    persist();
    setPlaying(false);
    setSelectedId(next.id);
    setFrameIndex(0);
    setPlayFrame(0);
    setRename(next.name);
  }

  function deleteAsset(): void {
    if (!selectedId) return;
    const i = lib.assets.findIndex((a) => a.id === selectedId);
    if (!artRemoveAsset(lib, selectedId)) return;
    const next = lib.assets[Math.min(i, lib.assets.length - 1)];
    persist();
    setPlaying(false);
    setSelectedId(next?.id ?? null);
    setFrameIndex(0);
    setPlayFrame(0);
    setRename(next?.name ?? "");
  }

  function renameAsset(): void {
    if (!asset) return;
    artRename(asset, rename);
    persist();
    setRename(asset.name);
  }

  function addFrame(): void {
    if (!asset) return;
    if (!artAddFrame(asset)) return;
    markAnim(asset);
    persist();
    setFrameIndex(asset.frames.length - 1);
  }

  function removeFrame(): void {
    if (!asset) return;
    if (!artRemoveFrame(asset, frameIndex)) return;
    persist();
    setFrameIndex((i) => Math.max(0, Math.min(i, asset.frames.length - 1)));
  }

  function invertFrame(): void {
    if (!frame) return;
    artInvert(frame);
    persist();
  }

  function fitZoom(): void {
    if (!asset) return;
    const box = centerRef.current;
    if (!box) return;
    const lcdExtra = asset.w + 48;
    const availW = Math.max(ZOOM_STEPS[0]!, box.clientWidth - 24 - lcdExtra);
    const availH = Math.max(ZOOM_STEPS[0]!, box.clientHeight - 52);
    const raw = Math.floor(Math.min(availW / asset.w, availH / asset.h));
    setZoom(nearestZoom(Math.max(ZOOM_STEPS[0]!, raw)));
  }

  useEffect(() => {
    function onKey(e: KeyboardEvent): void {
      if (e.defaultPrevented) return;
      const t = e.target;
      if (t instanceof HTMLElement) {
        const tag = t.tagName;
        if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT" || t.isContentEditable) return;
      }
      if (e.key === "+" || e.key === "=") {
        e.preventDefault();
        setZoom((z) => zoomStep(z, 1));
      } else if (e.key === "-" || e.key === "_") {
        e.preventDefault();
        setZoom((z) => zoomStep(z, -1));
      } else if (e.key === "0" && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        setZoom(ZOOM_DEFAULT);
      }
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  const safeFrame = asset ? Math.max(0, Math.min(frameIndex, asset.frames.length - 1)) : 0;

  return (
    <section className="art-studio">
      <div className="art-menu">
        <span className="art-brand">Art</span>
        <button type="button" className="art-btn" onClick={() => newAsset("icon 8x8", 8, 8, "icon")}>
          New icon 8×8
        </button>
        <button type="button" className="art-btn" onClick={() => newAsset("icon 16x16", 16, 16, "icon")}>
          New 16×16
        </button>
        <button type="button" className="art-btn" onClick={() => newAsset("icon 32x16", 32, 16, "icon")}>
          New 32×16
        </button>
        <button
          type="button"
          className="art-btn"
          onClick={() => newAsset("screen", ART_W_MAX, ART_H_MAX, "screen")}
        >
          New full {ART_W_MAX}×{ART_H_MAX} screen
        </button>
        <button type="button" className="art-btn" onClick={deleteAsset} disabled={!asset}>
          Delete asset
        </button>
        <label className="art-rename">
          Rename
          <input
            value={rename}
            disabled={!asset}
            onChange={(e) => setRename(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter") {
                e.preventDefault();
                renameAsset();
              }
            }}
          />
        </label>
        <button type="button" className="art-btn" onClick={renameAsset} disabled={!asset}>
          Rename
        </button>
        <button
          type="button"
          className="art-btn"
          title="Download the whole library as JSON"
          onClick={saveLibraryFile}
        >
          Save library
        </button>
        <button
          type="button"
          className="art-btn"
          title="Replace the library from a JSON backup"
          onClick={() => fileRef.current?.click()}
        >
          Load library
        </button>
        <button
          type="button"
          className="art-btn"
          title="Copy the whole library JSON to paste elsewhere"
          onClick={() => void copyLibraryJson()}
        >
          {copiedLib ? "Copied JSON" : "Copy JSON"}
        </button>
        <input
          ref={fileRef}
          className="art-file"
          type="file"
          accept="application/json,.json"
          aria-label="Load art library JSON"
          onChange={onPickLibraryFile}
        />
      </div>

      <ArtToolbox tool={tool} ink={ink} disabled={!asset} onTool={setTool} onInk={setInk} onInvert={invertFrame} />

      <div className="art-center" ref={centerRef}>
        <div className="art-center-bar">
          <label className="art-check">
            <input
              type="checkbox"
              checked={showGrid}
              onChange={(e) => setShowGrid(e.target.checked)}
            />
            Grid
          </label>
          <div className="art-zoom">
            <button
              type="button"
              className="art-btn"
              title="Zoom out (−)"
              disabled={nearestZoom(zoom) <= ZOOM_STEPS[0]!}
              onClick={() => setZoom((z) => zoomStep(z, -1))}
            >
              −
            </button>
            <label className="art-zoom-pick">
              Zoom
              <select
                value={nearestZoom(zoom)}
                onChange={(e) => setZoom(Number(e.target.value))}
              >
                {ZOOM_STEPS.map((z) => (
                  <option key={z} value={z}>
                    {z}×
                  </option>
                ))}
              </select>
            </label>
            <button
              type="button"
              className="art-btn"
              title="Zoom in (+)"
              disabled={nearestZoom(zoom) >= ZOOM_STEPS[ZOOM_STEPS.length - 1]!}
              onClick={() => setZoom((z) => zoomStep(z, 1))}
            >
              +
            </button>
            <button type="button" className="art-btn" disabled={!asset} onClick={fitZoom} title="Fit in view">
              Fit
            </button>
          </div>
        </div>
        <ArtCanvas
          asset={asset}
          frameIndex={safeFrame}
          tool={tool}
          ink={ink}
          showGrid={showGrid}
          cell={zoom}
          playing={playing}
          playFrame={playFrame}
          rev={rev}
          onCursor={setCursor}
          onMutate={persist}
          onZoom={(dir) => setZoom((z) => zoomStep(z, dir))}
        />
      </div>

      <div className="art-side">
        <ArtLibraryList lib={lib} selectedId={selectedId} onSelect={selectAsset} />
        <ArtFrames
          asset={asset}
          frameIndex={safeFrame}
          playing={playing}
          rev={rev}
          onSelect={setFrameIndex}
          onAdd={addFrame}
          onRemove={removeFrame}
          onDelay={(ms) => {
            if (!asset) return;
            artSetDelay(asset, ms);
            persist();
          }}
          onPlay={() => {
            setPlayFrame(0);
            setPlaying(true);
          }}
          onStop={() => {
            setPlaying(false);
            setPlayFrame(0);
          }}
        />
      </div>

      <p className="art-status">
        <span>{cursor ? `${cursor.x}, ${cursor.y}` : "—"}</span>
        <span>{asset ? `${asset.w}×${asset.h}` : "—"}</span>
        <span>{zoom}×</span>
        <span>{toolLabel(tool)}</span>
        {libraryNote ? <span>{libraryNote}</span> : null}
      </p>

      <ArtExport asset={asset} lib={lib} rev={rev} jsonRef={libJsonRef} />
    </section>
  );
}

function ArtToolbox({
  tool,
  ink,
  disabled,
  onTool,
  onInk,
  onInvert,
}: {
  tool: ArtTool;
  ink: 0 | 1;
  disabled: boolean;
  onTool: (t: ArtTool) => void;
  onInk: (v: 0 | 1) => void;
  onInvert: () => void;
}) {
  return (
    <div className="art-tools">
      {TOOLS.map((t) => (
        <button
          key={t.id}
          type="button"
          className={tool === t.id ? "art-tool on" : "art-tool"}
          disabled={disabled}
          title={t.label}
          onClick={() => onTool(t.id)}
        >
          {t.label}
        </button>
      ))}
      <button
        type="button"
        className={ink === 1 ? "art-swatch ink on" : "art-swatch ink"}
        disabled={disabled}
        title="Ink"
        onClick={() => onInk(1)}
      >
        Ink
      </button>
      <button
        type="button"
        className={ink === 0 ? "art-swatch paper on" : "art-swatch paper"}
        disabled={disabled}
        title="Paper"
        onClick={() => onInk(0)}
      >
        Paper
      </button>
      <button type="button" className="art-tool art-invert" disabled={disabled} onClick={onInvert}>
        Invert
      </button>
    </div>
  );
}

function ArtCanvas({
  asset,
  frameIndex,
  tool,
  ink,
  showGrid,
  cell,
  playing,
  playFrame,
  rev,
  onCursor,
  onMutate,
  onZoom,
}: {
  asset: ArtAsset | undefined;
  frameIndex: number;
  tool: ArtTool;
  ink: 0 | 1;
  showGrid: boolean;
  cell: number;
  playing: boolean;
  playFrame: number;
  rev: number;
  onCursor: (p: { x: number; y: number } | null) => void;
  onMutate: () => void;
  onZoom: (dir: 1 | -1) => void;
}) {
  const editorRef = useRef<HTMLCanvasElement>(null);
  const lcdRef = useRef<HTMLCanvasElement>(null);
  const wrapRef = useRef<HTMLDivElement>(null);
  const onZoomRef = useRef(onZoom);
  onZoomRef.current = onZoom;
  const dragRef = useRef<DragStroke | null>(null);
  const lastRef = useRef<{ x: number; y: number } | null>(null);

  const frame = asset?.frames[frameIndex];
  const w = asset?.w ?? 8;
  const h = asset?.h ?? 8;

  function redraw(): void {
    redrawArtCanvases(
      editorRef.current,
      lcdRef.current,
      asset,
      frame,
      tool,
      ink,
      dragRef.current,
      playing,
      playFrame,
    );
  }

  useEffect(() => {
    redrawArtCanvases(
      editorRef.current,
      lcdRef.current,
      asset,
      frame,
      tool,
      ink,
      dragRef.current,
      playing,
      playFrame,
    );
  }, [asset, frame, frameIndex, playFrame, playing, rev, tool, ink]);

  useEffect(() => {
    const el = wrapRef.current;
    if (!el) return;
    const onWheel = (e: WheelEvent): void => {
      if (!e.ctrlKey && !e.metaKey) return;
      e.preventDefault();
      onZoomRef.current(e.deltaY < 0 ? 1 : -1);
    };
    el.addEventListener("wheel", onWheel, { passive: false });
    return () => el.removeEventListener("wheel", onWheel);
  }, []);

  function paintAt(x: number, y: number): void {
    if (!asset || !frame) return;
    const value: 0 | 1 = tool === "eraser" ? 0 : ink;
    const last = lastRef.current;
    if (last) artLine(frame, asset.w, asset.h, last.x, last.y, x, y, value);
    else artSet(frame, asset.w, asset.h, x, y, value);
    lastRef.current = { x, y };
    paintCanvas(editorRef.current, frame.pixels, asset.w, asset.h);
    if (!playing) paintCanvas(lcdRef.current, frame.pixels, asset.w, asset.h);
  }

  function onDown(e: ReactPointerEvent<HTMLCanvasElement>): void {
    if (!asset || !frame || e.button !== 0) return;
    const el = editorRef.current;
    if (!el) return;
    const pt = mapPixel(el, e.clientX, e.clientY, asset.w, asset.h, true);
    if (!pt) return;
    e.preventDefault();
    el.setPointerCapture(e.pointerId);
    onCursor(pt);
    if (tool === "fill") {
      artFlood(frame, asset.w, asset.h, pt.x, pt.y, ink);
      onMutate();
      return;
    }
    if (isShape(tool)) {
      dragRef.current = { x0: pt.x, y0: pt.y, x1: pt.x, y1: pt.y, mode: "shape" };
      redraw();
      return;
    }
    dragRef.current = { x0: pt.x, y0: pt.y, x1: pt.x, y1: pt.y, mode: "paint" };
    lastRef.current = null;
    paintAt(pt.x, pt.y);
  }

  function onMove(e: ReactPointerEvent<HTMLCanvasElement>): void {
    if (!asset) {
      onCursor(null);
      return;
    }
    const el = editorRef.current;
    if (!el) return;
    const dragging = dragRef.current !== null;
    const pt = mapPixel(el, e.clientX, e.clientY, asset.w, asset.h, dragging);
    onCursor(pt);
    if (!pt || !dragging) return;
    const drag = dragRef.current!;
    drag.x1 = pt.x;
    drag.y1 = pt.y;
    if (drag.mode === "paint") paintAt(pt.x, pt.y);
    else redraw();
  }

  function endStroke(e: ReactPointerEvent<HTMLCanvasElement>): void {
    const el = editorRef.current;
    if (el && el.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId);
    const drag = dragRef.current;
    dragRef.current = null;
    lastRef.current = null;
    if (!asset || !frame || !drag) {
      redraw();
      return;
    }
    if (drag.mode === "shape" && isShape(tool)) {
      applyShape(frame, asset.w, asset.h, tool, ink, drag.x0, drag.y0, drag.x1, drag.y1);
    }
    onMutate();
  }

  const cssW = w * cell;
  const cssH = h * cell;

  return (
    <div className="art-stage">
      <div
        ref={wrapRef}
        className="art-pixels-wrap"
        style={{ width: cssW, height: cssH, ["--art-cell" as string]: `${cell}px` }}
      >
        <canvas
          ref={editorRef}
          className="art-pixels"
          width={w}
          height={h}
          style={{ width: cssW, height: cssH }}
          aria-label="pixel editor"
          onPointerDown={onDown}
          onPointerMove={onMove}
          onPointerUp={endStroke}
          onPointerCancel={endStroke}
          onPointerLeave={() => {
            if (!dragRef.current) onCursor(null);
          }}
          onContextMenu={(e) => e.preventDefault()}
        />
        {showGrid && asset ? <div className="art-grid" /> : null}
      </div>
      <div className="art-lcd">
        <canvas
          ref={lcdRef}
          className="art-lcd-face"
          width={w}
          height={h}
          style={{ width: w, height: h }}
          aria-label="1:1 LCD preview"
        />
      </div>
    </div>
  );
}

function ArtFrames({
  asset,
  frameIndex,
  playing,
  rev,
  onSelect,
  onAdd,
  onRemove,
  onDelay,
  onPlay,
  onStop,
}: {
  asset: ArtAsset | undefined;
  frameIndex: number;
  playing: boolean;
  rev: number;
  onSelect: (i: number) => void;
  onAdd: () => void;
  onRemove: () => void;
  onDelay: (ms: number) => void;
  onPlay: () => void;
  onStop: () => void;
}) {
  return (
    <div className="art-frames">
      <h3>Frames</h3>
      <div className="art-thumbs">
        {asset
          ? asset.frames.map((fr, i) => (
              <ArtThumb
                key={`${asset.id}-${i}`}
                frame={fr}
                w={asset.w}
                h={asset.h}
                selected={i === frameIndex}
                rev={rev}
                label={`Frame ${i + 1}`}
                onClick={() => onSelect(i)}
              />
            ))
          : null}
      </div>
      <div className="art-frame-ctl">
        <button type="button" className="art-btn" disabled={!asset} onClick={onAdd}>
          Add frame
        </button>
        <button type="button" className="art-btn" disabled={!asset} onClick={onRemove}>
          Delete frame
        </button>
        <label className="art-delay">
          Delay ms
          <input
            type="number"
            min={50}
            max={2000}
            step={10}
            disabled={!asset}
            value={asset?.delayMs ?? 120}
            onChange={(e) => onDelay(Number(e.target.value))}
          />
        </label>
        {playing ? (
          <button type="button" className="art-btn on" disabled={!asset} onClick={onStop}>
            Stop
          </button>
        ) : (
          <button type="button" className="art-btn" disabled={!asset} onClick={onPlay}>
            Play
          </button>
        )}
      </div>
    </div>
  );
}

function ArtThumb({
  frame,
  w,
  h,
  selected,
  rev,
  label,
  onClick,
}: {
  frame: ArtFrame;
  w: number;
  h: number;
  selected: boolean;
  rev: number;
  label: string;
  onClick: () => void;
}) {
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    paintCanvas(ref.current, frame.pixels, w, h);
  }, [frame, frame.pixels, w, h, rev]);
  return (
    <button
      type="button"
      className={selected ? "art-thumb on" : "art-thumb"}
      title={label}
      onClick={onClick}
    >
      <canvas ref={ref} className="art-thumb-face" width={w} height={h} />
    </button>
  );
}

function ArtLibraryList({
  lib,
  selectedId,
  onSelect,
}: {
  lib: ArtLibrary;
  selectedId: string | null;
  onSelect: (id: string) => void;
}) {
  return (
    <div className="art-assets">
      <h3>Assets</h3>
      <ul className="art-asset-list">
        {lib.assets.map((a) => (
          <li key={a.id}>
            <button
              type="button"
              className={a.id === selectedId ? "art-asset on" : "art-asset"}
              onClick={() => onSelect(a.id)}
            >
              <span className="art-asset-name">{a.name}</span>
              <span className="art-asset-meta">
                {a.w}×{a.h} {a.kind}
              </span>
            </button>
          </li>
        ))}
      </ul>
    </div>
  );
}

function ArtExport({
  asset,
  lib,
  rev,
  jsonRef,
}: {
  asset: ArtAsset | undefined;
  lib: ArtLibrary;
  rev: number;
  jsonRef: RefObject<HTMLTextAreaElement | null>;
}) {
  const [copied, setCopied] = useState(false);
  const source = asset ? artExportTs(asset) : "";
  const libraryJson = artSerialize(lib, true);

  async function copy(): Promise<void> {
    if (!source) return;
    try {
      await navigator.clipboard.writeText(source);
      setCopied(true);
      window.setTimeout(() => setCopied(false), 1200);
    } catch {
      setCopied(false);
    }
  }

  return (
    <div className="art-export">
      <div className="art-export-head">
        <h3>Firmware</h3>
        {asset ? <span className="art-ident">{artIdent(asset.name)}</span> : null}
        <button type="button" className="art-btn" disabled={!asset} onClick={() => void copy()}>
          {copied ? "Copied" : "Copy"}
        </button>
      </div>
      <textarea
        className="art-export-ts"
        readOnly
        value={source}
        spellCheck={false}
        data-rev={rev}
      />
      <div className="art-export-head">
        <h3>Library JSON</h3>
        <span className="art-ident">
          {lib.assets.length} asset{lib.assets.length === 1 ? "" : "s"}
        </span>
      </div>
      <textarea
        ref={jsonRef}
        className="art-export-ts art-lib-json"
        readOnly
        value={libraryJson}
        spellCheck={false}
        data-rev={rev}
        aria-label="Art library JSON"
      />
    </div>
  );
}
