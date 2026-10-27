#!/usr/bin/env python3
"""Turn the Beeple PNGs into 64×64 firmware art.

Reads docs/BEEPLE PNG (or the paths you pass). Each picture is fitted to
64×64 before it is packed:

  * Taller or wider than 64: keep the top-left 64×64.
  * Shorter or narrower: sit the picture on the top-left and fill the
    rest with the corner color.
  * A file listed in CROP uses that window instead. Vibey Eye drops its
    top 3 rows so the feet stay in the square.

A picture whose border is black is inverted, so the character is ink and
the background is paper. The packed bytes match firmware/include/art.h.

  python3 tools/beeple_to_art.py
  python3 tools/beeple_to_art.py --write

--write replaces the matching symbols in art.h and art.c, rewrites the
Beeple table in screen.c, and sets BEEPLE_N. Known files keep KNOWN order so a
saved index still means the same mascot. New files are appended in filename order.
"""

import argparse
import importlib.util
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DIR = ROOT / "docs" / "BEEPLE PNG"
SCREEN_C = ROOT / "firmware" / "src" / "screen.c"
BEEPLE_H = ROOT / "firmware" / "include" / "beeple.h"
SIZE = 64

# Filename, C symbol, menu label. Labels stay within 10 letters so they fit
# the left half of the menu in the 5×7 font.
KNOWN = [
    ("Voidfarers V1.png", "art_voidfarers", "Voidfarer"),
    ("OctoHat.png", "art_octohat", "Octohat"),
    ("camnon.png", "art_camnon", "Camnon"),
    ("Vibey Eye V2.png", "art_vibey_eye", "Vibey Eye"),
    ("Alleycat v1.png", "art_alleycat", "Alleycat"),
    ("Beep and silent bop.png", "art_beepbop", "Beep Bop"),
    ("Dino.png", "art_dino", "Dino"),
    ("Midge the Smidge.png", "art_midge", "Midge"),
    ("Oscar.png", "art_oscar", "Oscar"),
    ("Robort v2.png", "art_robot", "Robort v2"),
    ("Spooder.png", "art_spooder", "Spooder"),
    ("TEEF.png", "art_teef", "Teef"),
]

# (x, y) of the 64×64 window. Anything not listed keeps the top-left.
CROP = {
    "Vibey Eye V2.png": (0, 3),
}


def load_packer():
    path = Path(__file__).resolve().parent / "png_to_art.py"
    spec = importlib.util.spec_from_file_location("png_to_art", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def luma(pixel):
    r, g, b, a = pixel
    if a < 128:
        return 255
    return (r + g + b) // 3


def black_background(pixels):
    h = len(pixels)
    w = len(pixels[0]) if h else 0
    if w == 0 or h == 0:
        return False
    samples = []
    for x in range(w):
        samples.append(luma(pixels[0][x]))
        samples.append(luma(pixels[h - 1][x]))
    for y in range(h):
        samples.append(luma(pixels[y][0]))
        samples.append(luma(pixels[y][w - 1]))
    return sum(samples) / len(samples) < 128


def fit_64(pixels, origin=(0, 0)):
    """64×64 starting at origin. A smaller picture stays on the left and the top."""
    ox, oy = origin
    h = len(pixels)
    w = len(pixels[0]) if h else 0
    if w == 0 or h == 0:
        raise SystemExit("image is empty")
    bg = pixels[0][0]
    out = []
    for y in range(SIZE):
        sy = oy + y
        row = []
        for x in range(SIZE):
            sx = ox + x
            if 0 <= sy < h and 0 <= sx < w:
                row.append(pixels[sy][sx])
            else:
                row.append(bg)
        out.append(row)
    return out


def label_ok(label):
    return 1 <= len(label) <= 10


def catalog(paths):
    by_name = {path.name: path for path in paths}
    rows = []
    used = set()
    for filename, symbol, label in KNOWN:
        path = by_name.get(filename)
        if path is None:
            raise SystemExit(f"missing {filename}")
        if not label_ok(label):
            raise SystemExit(f"{label!r} is longer than 10 letters")
        rows.append((path, symbol, label))
        used.add(path.name)
    extras = sorted(path for path in paths if path.name not in used)
    return rows, extras


def extra_row(packer, path):
    symbol = packer.symbol_for(path, None)
    if symbol == "art_beep":
        symbol = "art_beep_mascot"
    label = re.sub(r"[_]+", " ", Path(path).stem).strip()
    label = " ".join(part.capitalize() for part in label.split())
    if len(label) > 10:
        label = label[:10].rstrip()
    return path, symbol, label


def convert(packer, path):
    pixels = packer._read_png(path)
    origin = CROP.get(Path(path).name, (0, 0))
    fitted = fit_64(pixels, origin)
    invert = black_background(pixels)
    w, h, buf = packer.pack(fitted, invert)
    return w, h, buf, invert, origin


def table_text(rows):
    lines = ["static const beeple_def_t BEEPLES[] = {"]
    for _path, symbol, label in rows:
        tag = symbol.upper()
        lines.append(
            f'    {{"{label}", {symbol}, {tag}_W, {tag}_H, false}},'
        )
    lines.append("};")
    return "\n".join(lines)


def write_catalog(rows):
    pager = SCREEN_C.read_text()
    block = table_text(rows)
    pat = r"static const beeple_def_t BEEPLES\[\] = \{.*?\n\};"
    if not re.search(pat, pager, re.S):
        raise SystemExit("screen.c is missing the BEEPLES table")
    pager = re.sub(pat, block, pager, count=1, flags=re.S)
    SCREEN_C.write_text(pager)

    header = BEEPLE_H.read_text()
    if not re.search(r"#define BEEPLE_N \d+", header):
        raise SystemExit("beeple.h is missing BEEPLE_N")
    new = re.sub(r"#define BEEPLE_N \d+", f"#define BEEPLE_N {len(rows)}", header, count=1)
    if new != header:
        BEEPLE_H.write_text(new)


def main(argv):
    parser = argparse.ArgumentParser(description="Convert Beeple PNGs into 64×64 firmware art.")
    parser.add_argument("pngs", nargs="*", type=Path, help="PNG files (default: docs/BEEPLE PNG)")
    parser.add_argument("--write", action="store_true", help="update art.h, art.c, screen.c, and beeple.h")
    args = parser.parse_args(argv)
    packer = load_packer()
    if args.pngs:
        paths = args.pngs
    else:
        if not DEFAULT_DIR.is_dir():
            raise SystemExit(f"no folder {DEFAULT_DIR}")
        paths = sorted(DEFAULT_DIR.glob("*.png"))
    if not paths:
        raise SystemExit("no PNGs")
    rows, extras = catalog(paths)
    for path in extras:
        rows.append(extra_row(packer, path))

    items = []
    for path, symbol, label in rows:
        w, h, buf, invert, origin = convert(packer, path)
        if (w, h) != (SIZE, SIZE):
            raise SystemExit(f"{path.name}: fitted to {w}x{h}")
        items.append((symbol, w, h, buf))
        note = "inverted" if invert else "ink"
        if origin != (0, 0):
            note += f"  crop {origin[0]},{origin[1]}"
        print(f"{label:10} {symbol:16} {path.name}  {note}")

    if args.write:
        packer.write_assets(items)
        write_catalog(rows)
        print(f"wrote {len(rows)} beeples")


if __name__ == "__main__":
    main(sys.argv[1:])
