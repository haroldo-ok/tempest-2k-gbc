#!/usr/bin/env python3
"""Convert Tempest 2000 ProTracker MODs (mwenge/tempest2k) to GBC song rows.

Each row is 3 bytes: bass note, lead note, drum (0=off, 1=kick, 2=snare, 3=hat,
255=hold). Notes are indices into the GBC nfreq table (1..63, 0=mute).

Output: gen/tunes.bin (packed songs) and gen/tunes.h (bank/addr for MBC5).
Tunes are appended after gen/blob.bin starting at ROM bank 3.
"""
from __future__ import annotations

import os
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODDIR = ROOT / "third_party" / "t2k-mods"
GEN = ROOT / "gen"
BLOB_BANK0 = 3

# ProTracker period table, finetune 0, octaves 1–5
_BASE = [856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453]
PERIODS: dict[int, tuple[int, int]] = {}
for oct_ in range(5):
    for semi, p in enumerate(_BASE):
        PERIODS[p >> oct_] = (semi, oct_ + 1)


def closest_note(period: int) -> tuple[int, int] | None:
    if not period:
        return None
    best = None
    bd = 10**9
    for p, n in PERIODS.items():
        d = abs(p - period)
        if d < bd:
            bd = d
            best = n
    return best


def gb_note(semi: int, oct_: int) -> int:
    """PT C-1 → nfreq 0 (C2). Clamp 1..63; 0 is mute."""
    i = (oct_ - 1) * 12 + semi
    if i < 1:
        i = 1
    if i > 63:
        i = 63
    return i


def parse_mod(path: Path):
    d = path.read_bytes()
    slen = d[950]
    order = list(d[952 : 952 + slen])
    npat = max(order) + 1
    pats = d[1084 : 1084 + npat * 1024]
    slens = []
    off = 20
    for _ in range(31):
        slens.append(struct.unpack(">H", d[off + 22 : off + 24])[0] * 2)
        off += 30
    return order, pats, slens


def convert(path: Path) -> tuple[bytes, int]:
    order, pats, slens = parse_mod(path)
    use = [[0] * 32 for _ in range(4)]
    for patn in order:
        pat = pats[patn * 1024 : (patn + 1) * 1024]
        for row in range(64):
            for ch in range(4):
                b = pat[row * 16 + ch * 4 : row * 16 + ch * 4 + 4]
                sample = (b[0] & 0xF0) | (b[2] >> 4)
                if sample:
                    use[ch][sample] += 1
    # Only samples that mostly live on channel 4 (index 3) are drums.
    # Short plucks on ch 0–2 are melody/bass, not noise.
    drums = set()
    for s in range(1, 32):
        tot = sum(use[ch][s] for ch in range(4))
        if tot and use[3][s] * 2 >= tot and slens[s - 1] < 8000:
            drums.add(s)

    speed = 6
    speed_count: dict[int, int] = {}
    out = bytearray()
    ch_note = [0, 0, 0, 0]
    last_b = last_l = 0
    for patn in order:
        pat = pats[patn * 1024 : (patn + 1) * 1024]
        for row in range(64):
            trig = [False, False, False, False]
            drum = 0
            for ch in range(4):
                b = pat[row * 16 + ch * 4 : row * 16 + ch * 4 + 4]
                period = ((b[0] & 0x0F) << 8) | b[1]
                sample = (b[0] & 0xF0) | (b[2] >> 4)
                fx, fp = b[2] & 0x0F, b[3]
                if fx == 0xF and fp > 0 and fp < 32:
                    speed = fp
                    speed_count[fp] = speed_count.get(fp, 0) + 1
                # volume 0 or note-cut: silence this channel
                if (fx == 0xC and fp == 0) or (fx == 0xE and fp == 0xC0):
                    ch_note[ch] = 0
                    continue
                # tone porta: keep sounding pitch, don't jump
                if fx == 3:
                    continue
                if sample in drums:
                    # Hats (tiny / high) drown the lead on GBC — drop them.
                    # Keep kick/snare, and only on a new note trigger.
                    if period:
                        L = slens[sample - 1]
                        if L < 2300 or (period and period < 180):
                            pass  # hat: skip
                        elif period > 400:
                            drum = 1
                        else:
                            drum = 2
                    continue
                if period:
                    n = closest_note(period)
                    if n:
                        ch_note[ch] = gb_note(*n)
                        trig[ch] = True
            pitched = [(ch, n) for ch, n in enumerate(ch_note) if n]
            if pitched:
                bass_ch, bass = min(pitched, key=lambda t: t[1])
                lead_ch, lead = max(pitched, key=lambda t: t[1])
                if bass == lead:
                    if bass < 20:
                        lead, lead_ch = 0, -1
                    else:
                        bass, bass_ch = 0, -1
                # drop bass an octave when it sits on top of the lead
                if bass and lead and bass >= 12 and lead - bass < 12:
                    bass = max(1, bass - 12)
            else:
                bass = lead = 0
                bass_ch = lead_ch = -1

            btrig = bass_ch >= 0 and trig[bass_ch]
            ltrig = lead_ch >= 0 and trig[lead_ch]
            if bass == 0:
                ob = 0
            elif btrig or bass != last_b:
                ob = bass
            else:
                ob = 255
            if lead == 0:
                ol = 0
            elif ltrig or lead != last_l:
                ol = lead
            else:
                ol = 255
            last_b, last_l = bass, lead
            out.extend((ob & 0xFF, ol & 0xFF, drum & 0xFF))

    if speed_count:
        speed = max(speed_count, key=speed_count.get)
    # PAL 50 ticks/s → GBC 60 Hz vblank
    tempo = max(3, min(18, round(speed * 60 / 50)))
    return bytes(out), tempo


# Original T2K modtable / webtunes mapping (1-based modnum → file):
#   1 tune13 theme, 2 tune7, 5 rave4, 6 tune5, 7 tune12
# webtunes: 5,2,6,7,5,2,6,7
SONG_FILES = [
    ("tune13.mod", "title"),
    ("rave4.mod", "levela"),
    ("tune7.mod", "levelb"),
    ("tune5.mod", "levelc"),
    ("tune13.mod", "gameover"),
    ("tune12.mod", "warp"),
    ("tune12.mod", "leveld"),
]


def main() -> None:
    GEN.mkdir(exist_ok=True)
    cache: dict[str, tuple[bytes, int]] = {}
    songs = []
    blob = bytearray()
    for fn, name in SONG_FILES:
        if fn not in cache:
            path = MODDIR / fn
            if not path.exists():
                raise SystemExit(f"missing {path} (from github.com/mwenge/tempest2k)")
            cache[fn] = convert(path)
            print(f"{fn:12} {name:10} rows={len(cache[fn][0]) // 3} tempo={cache[fn][1]}")
        data, tempo = cache[fn]
        # reuse identical payload
        key = (id(data), tempo)  # noqa: not needed
        # find existing identical blob
        found = None
        for i, s in enumerate(songs):
            if s["file"] == fn and s["tempo"] == tempo:
                found = i
                break
        if found is not None and name != SONG_FILES[found][1]:
            songs.append({**songs[found], "name": name, "file": fn})
            continue
        # keep each song inside one 16 KiB bank so the ISR can do one switch
        bank_off = len(blob) % 0x4000
        if bank_off + len(data) > 0x4000:
            blob += b"\x00" * (0x4000 - bank_off)
        songs.append(
            {
                "name": name,
                "file": fn,
                "off": len(blob),
                "rows": len(data) // 3,
                "tempo": tempo,
                "data": data,
            }
        )
        blob += data

    # gameover: same as title but slower
    for s in songs:
        if s["name"] == "gameover":
            s["tempo"] = min(18, s["tempo"] + 3)

    (GEN / "tunes.bin").write_bytes(blob)

    blob_len = (GEN / "blob.bin").stat().st_size if (GEN / "blob.bin").exists() else 0
    base = BLOB_BANK0 * 0x4000 + blob_len

    lines = [
        "/* generated by tools/mod2gbc.py from original T2K ProTracker MODs */",
        "#ifndef TUNES_H",
        "#define TUNES_H",
        "#include <stdint.h>",
        f"#define NSONGS {len(songs)}",
        "static const uint8_t tune_bank[NSONGS] = {",
    ]
    banks, addrs, rows, tempos = [], [], [], []
    for s in songs:
        rom = base + s["off"]
        banks.append(str(rom // 0x4000))
        addrs.append(f"0x{0x4000 + (rom % 0x4000):04X}")
        rows.append(str(s["rows"]))
        tempos.append(str(s["tempo"]))
        print(f"  {s['name']:10} bank {rom // 0x4000} addr 0x{0x4000 + (rom % 0x4000):04X} rows {s['rows']}")
    lines.append("    " + ", ".join(banks))
    lines.append("};")
    lines.append("static const uint16_t tune_addr[NSONGS] = {")
    lines.append("    " + ", ".join(addrs))
    lines.append("};")
    lines.append("static const uint16_t tune_rows[NSONGS] = {")
    lines.append("    " + ", ".join(rows))
    lines.append("};")
    lines.append("static const uint8_t tune_tempo[NSONGS] = {")
    lines.append("    " + ", ".join(tempos))
    lines.append("};")
    lines.append("#endif")
    (GEN / "tunes.h").write_text("\n".join(lines) + "\n")
    print(f"wrote gen/tunes.bin {len(blob)} bytes, gen/tunes.h")


if __name__ == "__main__":
    main()
