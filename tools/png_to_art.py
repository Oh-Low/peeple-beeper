#!/usr/bin/env python3
"""Turn a black-and-white PNG into a 1-bit Peeple art asset.

Dark pixels are ink (1). Light pixels, and anything mostly transparent, are
paper (0). Bytes match firmware/include/art.h: bit 0 is the top pixel of an
8-row page, and the index is page * width + x.

  python3 tools/png_to_art.py sprite.png
  python3 tools/png_to_art.py --write one.png two.png
  python3 tools/png_to_art.py --name art_badge badge.png
  python3 tools/png_to_art.py --invert light-on-dark.png

--write adds the symbol to firmware/include/art.h and firmware/src/art.c, or
replaces it when that name is already there. A file named badge.png becomes
art_badge. Pass --name to choose the symbol.
"""

import argparse
import re
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ART_H = ROOT / "firmware" / "include" / "art.h"
ART_C = ROOT / "firmware" / "src" / "art.c"


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def _read_png(path):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"{path}: not a PNG")
    pos = 8
    w = h = depth = color = interlace = None
    idat = b""
    plte = None
    trns = None
    while pos < len(data):
        n = struct.unpack(">I", data[pos : pos + 4])[0]
        tag = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + n]
        pos += 12 + n
        if tag == b"IHDR":
            w, h, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
        elif tag == b"PLTE":
            plte = chunk
        elif tag == b"tRNS":
            trns = chunk
        elif tag == b"IDAT":
            idat += chunk
        elif tag == b"IEND":
            break
    if interlace:
        raise SystemExit(f"{path}: interlaced PNG is not supported")
    if depth not in (1, 2, 4, 8):
        raise SystemExit(f"{path}: bit depth {depth} is not supported")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color)
    if channels is None:
        raise SystemExit(f"{path}: color type {color} is not supported")
    raw = zlib.decompress(idat)
    samples_per_row = w * channels
    bits_per_row = samples_per_row * depth
    stride = (bits_per_row + 7) // 8
    bpp = max(1, (channels * depth + 7) // 8)
    rows = []
    i = 0
    for _y in range(h):
        f = raw[i]
        i += 1
        row = bytearray(raw[i : i + stride])
        i += stride
        prev = rows[-1] if rows else bytearray(stride)
        if f == 1:
            for x in range(stride):
                row[x] = (row[x] + (row[x - bpp] if x >= bpp else 0)) & 255
        elif f == 2:
            for x in range(stride):
                row[x] = (row[x] + prev[x]) & 255
        elif f == 3:
            for x in range(stride):
                left = row[x - bpp] if x >= bpp else 0
                row[x] = (row[x] + ((left + prev[x]) // 2)) & 255
        elif f == 4:
            for x in range(stride):
                a = row[x - bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x - bpp] if x >= bpp else 0
                row[x] = (row[x] + _paeth(a, b, c)) & 255
        elif f != 0:
            raise SystemExit(f"{path}: bad PNG filter {f}")
        rows.append(row)

    def sample(row, index):
        if depth == 8:
            return row[index]
        shift = (8 - depth) - (index * depth) % 8
        return (row[(index * depth) // 8] >> shift) & ((1 << depth) - 1)

    scale = 255 // ((1 << depth) - 1)
    pixels = []
    for y in range(h):
        line = []
        row = rows[y]
        for x in range(w):
            base = x * channels
            if color == 3:
                idx = sample(row, x)
                if plte is None or idx * 3 + 2 >= len(plte):
                    raise SystemExit(f"{path}: palette index {idx} is missing")
                r, g, b = plte[idx * 3 : idx * 3 + 3]
                a = trns[idx] if trns and idx < len(trns) else 255
            elif color == 0:
                v = sample(row, x) * scale
                r = g = b = v
                a = 255
                if trns and len(trns) >= 2 and v == struct.unpack(">H", trns[:2])[0]:
                    a = 0
            elif color == 4:
                v = sample(row, base) * scale
                r = g = b = v
                a = sample(row, base + 1) * scale
            elif color == 2:
                r, g, b = (sample(row, base + k) * scale for k in range(3))
                a = 255
                if trns and len(trns) >= 6:
                    tr, tg, tb = struct.unpack(">HHH", trns[:6])
                    if (r, g, b) == (tr, tg, tb):
                        a = 0
            else:
                r, g, b, a = (sample(row, base + k) * scale for k in range(4))
            line.append((r, g, b, a))
        pixels.append(line)
    return pixels


def pack(pixels, invert):
    h = len(pixels)
    w = len(pixels[0]) if h else 0
    if w == 0 or h == 0:
        raise SystemExit("image is empty")
    pages = (h + 7) // 8
    buf = [0] * (pages * w)
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[y][x]
            if a < 128:
                ink = False
            else:
                dark = (r + g + b) // 3 < 128
                ink = (not dark) if invert else dark
            if ink:
                buf[(y >> 3) * w + x] |= 1 << (y & 7)
    return w, h, buf


def symbol_for(path, name):
    stem = name if name else Path(path).stem
    ident = re.sub(r"[^0-9A-Za-z]+", "_", stem).strip("_").lower()
    if not ident or ident[0].isdigit():
        ident = "art_" + ident
    if not ident.startswith("art_"):
        ident = "art_" + ident
    return ident


def tag_for(symbol):
    return symbol.upper()


def format_array(symbol, buf, width):
    lines = [f"const uint8_t {symbol}[{tag_for(symbol)}_LEN] = {{"]
    pages = len(buf) // width
    for page in range(pages):
        chunk = buf[page * width : (page + 1) * width]
        for i in range(0, width, 8):
            part = chunk[i : i + 8]
            lines.append("    " + ", ".join(f"0x{b:02x}" for b in part) + ",")
    lines.append("};")
    return "\n".join(lines)


def format_snippet(symbol, w, h, buf):
    tag = tag_for(symbol)
    header = "\n".join(
        [
            f"#define {tag}_W {w}",
            f"#define {tag}_H {h}",
            f"#define {tag}_LEN ({tag}_W * (({tag}_H + 7) / 8))",
            f"extern const uint8_t {symbol}[{tag}_LEN];",
        ]
    )
    return header + "\n\n" + format_array(symbol, buf, w) + "\n"


def _sub_define(text, name, value):
    pat = rf"(#define {name} )\d+"
    line = f"#define {name} {value}"
    if re.search(pat, text):
        return re.sub(pat, rf"\g<1>{value}", text, count=1)
    extern = re.search(r"^extern ", text, re.M)
    insert = line + "\n"
    if extern:
        return text[: extern.start()] + insert + text[extern.start() :]
    return text.rstrip() + "\n" + insert + "\n"


def _ensure_len(text, tag):
    name = f"{tag}_LEN"
    if f"#define {name} " in text:
        return text
    line = f"#define {name} ({tag}_W * (({tag}_H + 7) / 8))\n"
    extern = re.search(r"^extern ", text, re.M)
    if extern:
        return text[: extern.start()] + line + text[extern.start() :]
    return text.rstrip() + "\n" + line


def _ensure_extern(text, symbol, tag):
    decl = f"extern const uint8_t {symbol}[{tag}_LEN];"
    if decl in text:
        return text
    return text.rstrip() + "\n" + decl + "\n"


def write_assets(items):
    header = ART_H.read_text()
    source = ART_C.read_text()
    for symbol, w, h, buf in items:
        tag = tag_for(symbol)
        header = _sub_define(header, f"{tag}_W", w)
        header = _sub_define(header, f"{tag}_H", h)
        header = _ensure_len(header, tag)
        header = _ensure_extern(header, symbol, tag)
        block = format_array(symbol, buf, w)
        pat = rf"const uint8_t {symbol}\[.*?\] = \{{.*?\}};"
        if re.search(pat, source, re.S):
            source = re.sub(pat, block, source, count=1, flags=re.S)
        else:
            source = source.rstrip() + "\n\n" + block + "\n"
    if not header.endswith("\n"):
        header += "\n"
    if not source.endswith("\n"):
        source += "\n"
    ART_H.write_text(header)
    ART_C.write_text(source)


def main(argv):
    parser = argparse.ArgumentParser(description="Convert a black-and-white PNG into a 1-bit art asset.")
    parser.add_argument("pngs", nargs="+", type=Path)
    parser.add_argument("--name", help="C symbol for a single PNG (default: art_<filename>)")
    parser.add_argument("--invert", action="store_true", help="light pixels are ink")
    parser.add_argument("--write", action="store_true", help="update firmware/include/art.h and firmware/src/art.c")
    args = parser.parse_args(argv)
    if args.name and len(args.pngs) != 1:
        raise SystemExit("--name applies to one PNG")
    items = []
    snippets = []
    for path in args.pngs:
        symbol = symbol_for(path, args.name)
        w, h, buf = pack(_read_png(path), args.invert)
        items.append((symbol, w, h, buf))
        snippets.append(format_snippet(symbol, w, h, buf))
    if args.write:
        write_assets(items)
        for symbol, w, h, buf in items:
            print(f"{symbol} {w}x{h} {len(buf)} bytes")
    else:
        sys.stdout.write("\n".join(snippets))


if __name__ == "__main__":
    main(sys.argv[1:])
