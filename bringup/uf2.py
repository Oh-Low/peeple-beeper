#!/usr/bin/env python3

import struct
import sys

MAGIC0 = 0x0A324655
MAGIC1 = 0x9E5D5157
MAGIC_END = 0x0AB16F30
FAMILY_NRF52840 = 0xADA52840
FLAG_FAMILY = 0x00002000
PAYLOAD = 256
BLOCK = 512


def check_vector(blob, base):
    if len(blob) < 8:
        raise SystemExit("image is too small")
    sp, reset = struct.unpack_from("<II", blob, 0)
    if not 0x20000000 <= sp <= 0x20040000:
        raise SystemExit("stack pointer is not in RAM: %#x" % sp)
    if reset & 1 == 0:
        raise SystemExit("reset vector is not Thumb")
    off = (reset & ~1) - base
    if not 0 <= off < len(blob):
        raise SystemExit("reset vector is outside the image")


def pack(blob, base):
    check_vector(blob, base)
    blocks = (len(blob) + PAYLOAD - 1) // PAYLOAD
    out = bytearray()
    for i in range(blocks):
        chunk = blob[i * PAYLOAD:(i + 1) * PAYLOAD]
        chunk = chunk + bytes([0xFF]) * (PAYLOAD - len(chunk))
        header = struct.pack(
            "<IIIIIIII",
            MAGIC0,
            MAGIC1,
            FLAG_FAMILY,
            base + i * PAYLOAD,
            PAYLOAD,
            i,
            blocks,
            FAMILY_NRF52840,
        )
        block = header + chunk + bytes(BLOCK - 4 - len(header) - len(chunk))
        block += struct.pack("<I", MAGIC_END)
        if len(block) != BLOCK:
            raise SystemExit("UF2 block is %d bytes" % len(block))
        out += block
    return bytes(out)


def main(argv):
    if len(argv) != 4:
        raise SystemExit("usage: uf2.py input.bin output.uf2 base_addr")
    blob = open(argv[1], "rb").read()
    base = int(argv[3], 0)
    open(argv[2], "wb").write(pack(blob, base))


if __name__ == "__main__":
    main(sys.argv)
