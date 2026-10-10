#!/usr/bin/env python3
"""Splice GBDK code ROM with data blob (banks 2+) and fix the cartridge header."""
import sys

code_path, blob_path, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
code = bytearray(open(code_path, "rb").read())
blob = bytearray(open(blob_path, "rb").read())
if len(sys.argv) > 4:
    blob += open(sys.argv[4], "rb").read()
BLOB_BASE = 3 * 0x4000  # bank 3 onward (0=fixed, 1=game, 2 unused)

need = BLOB_BASE + len(blob)
# pad to next power-of-two ROM size, min 256 KiB
size = 1
while size < need:
    size *= 2
if size < 0x40000:
    size = 0x40000

rom = bytearray(size)
# keep GBDK code/header (banks 0-2)
rom[: min(len(code), len(rom))] = code[: min(len(code), len(rom))]
rom[BLOB_BASE : BLOB_BASE + len(blob)] = blob

# GBC only, MBC5, ROM size, 0 RAM
rom[0x143] = 0xC0
rom[0x147] = 0x19  # MBC5
rom_size_code = {0x8000: 0, 0x10000: 1, 0x20000: 2, 0x40000: 3, 0x80000: 4, 0x100000: 5, 0x200000: 6, 0x400000: 7}
rom[0x148] = rom_size_code.get(size, 5)
rom[0x149] = 0
# title
title = b"T2K-GBC"
rom[0x134:0x134 + len(title)] = title
rom[0x134 + len(title) : 0x143] = b"\x00" * (0x143 - 0x134 - len(title))

# header checksum
x = 0
for i in range(0x134, 0x14D):
    x = (x - rom[i] - 1) & 0xFF
rom[0x14D] = x

# global checksum
s = sum(rom) - rom[0x14E] - rom[0x14F]
rom[0x14E] = (s >> 8) & 0xFF
rom[0x14F] = s & 0xFF

open(out_path, "wb").write(rom)
print("wrote", out_path, "size", len(rom), "banks", len(rom) // 16384)
