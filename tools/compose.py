#!/usr/bin/env python3
"""Original GBC chiptunes for the T2K port. Long in-game loops, sparse drums,
melody on pulse / bass on wave (see src/sound.c). Writes gen/tunes.bin + tunes.h.
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "gen"
BLOB_BANK0 = 3

HOLD, MUTE = 255, 0
KICK, SNARE = 1, 2

C, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B = range(12)
Bb, Eb, Ab = As, Ds, Gs


def N(pc: int, oct_: int) -> int:
    """C2=0 in nfreq; 0 is mute so the floor is 1 (C#2)."""
    v = pc + (oct_ - 2) * 12
    if v < 1:
        v = 1
    if v > 63:
        v = 63
    return v


def e8(*notes: int) -> list[int]:
    """Eight 8th-notes → 16 rows (note + hold, or mute+mute)."""
    out: list[int] = []
    for n in notes:
        out += [MUTE, MUTE] if n == 0 else [n, HOLD]
    assert len(out) == 16
    return out


def mel(*ev: tuple[int, int, int]) -> list[int]:
    """(start_row, note, duration_rows) on a 16-row bar."""
    row = [MUTE] * 16
    for st, n, dur in ev:
        for i in range(dur):
            if 0 <= st + i < 16:
                row[st + i] = n if i == 0 else HOLD
    return row


def ks(fill: bool = False) -> list[int]:
    if fill:
        return [KICK, 0, 0, 0, SNARE, 0, SNARE, 0, KICK, 0, 0, 0, SNARE, SNARE, SNARE, 0]
    return [KICK, 0, 0, 0, SNARE, 0, 0, 0, KICK, 0, 0, 0, SNARE, 0, 0, 0]


def quiet() -> list[int]:
    return [0] * 16


def pack(bars: list[tuple[list[int], list[int], list[int]]]) -> bytes:
    out = bytearray()
    for b, l, d in bars:
        assert len(b) == len(l) == len(d) == 16
        for i in range(16):
            out.extend((b[i] & 0xFF, l[i] & 0xFF, d[i] & 0xFF))
    return bytes(out)


def times(n: int, bars: list) -> list:
    out = []
    for _ in range(n):
        out.extend(bars)
    return out


# ---------------------------------------------------------------------------
# Title — "Vector Night"  D minor, slow, almost no drums
# ---------------------------------------------------------------------------
def song_title() -> tuple[bytes, int]:
    arp = e8(N(D, 3), N(F, 3), N(A, 3), N(C, 4), N(D, 4), N(C, 4), N(A, 3), N(F, 3))
    arp2 = e8(N(C, 3), N(E, 3), N(G, 3), N(Bb, 3), N(C, 4), N(Bb, 3), N(G, 3), N(E, 3))
    arp3 = e8(N(Bb, 2), N(D, 3), N(F, 3), N(A, 3), N(Bb, 3), N(A, 3), N(F, 3), N(D, 3))
    arp4 = e8(N(A, 2), N(C, 3), N(E, 3), N(G, 3), N(A, 3), N(G, 3), N(E, 3), N(C, 3))
    L1 = mel((0, N(F, 4), 8), (8, N(A, 4), 8))
    L2 = mel((0, N(D, 5), 6), (6, N(C, 5), 4), (10, N(A, 4), 6))
    L3 = mel((0, N(G, 4), 8), (8, N(F, 4), 4), (12, N(E, 4), 4))
    L4 = mel((0, N(D, 4), 16))
    L5 = mel((0, N(A, 4), 4), (4, N(C, 5), 4), (8, N(D, 5), 4), (12, N(F, 5), 4))
    L0 = quiet()
    d0 = quiet()
    d1 = [KICK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    bars = []
    # 8 bars intro arp
    bars += [(arp, L0, d0)] * 4 + [(arp, L0, d1)] * 4
    # 16 bars melody A
    for _ in range(2):
        bars += [(arp, L1, d0), (arp, L2, d0), (arp2, L3, d0), (arp, L4, d1)]
        bars += [(arp, L1, d0), (arp, L5, d0), (arp3, L3, d0), (arp4, L4, d1)]
    # 8 bars rise
    bars += [(arp, L5, d0), (arp2, L2, d0), (arp3, L5, d0), (arp, L2, d0)] * 2
    # 16 bars melody A again
    for _ in range(2):
        bars += [(arp, L1, d0), (arp, L2, d0), (arp2, L3, d0), (arp, L4, d1)]
        bars += [(arp, L5, d0), (arp, L2, d0), (arp3, L3, d0), (arp4, L1, d1)]
    # 8 bars to loop
    bars += [(arp, L4, d0), (arp, L0, d0), (arp, L1, d0), (arp, L0, d0)] * 2
    return pack(bars), 10


# ---------------------------------------------------------------------------
# In-game A — "Web Rider"  A minor, 150 BPM, ~2.5 min
# ---------------------------------------------------------------------------
def song_level_a() -> tuple[bytes, int]:
    bA = e8(N(A, 3), N(A, 3), N(E, 3), N(A, 3), N(C, 4), N(A, 3), N(G, 3), N(A, 3))
    bF = e8(N(F, 3), N(F, 3), N(C, 3), N(F, 3), N(A, 3), N(F, 3), N(E, 3), N(F, 3))
    bG = e8(N(G, 3), N(G, 3), N(D, 3), N(G, 3), N(B, 3), N(G, 3), N(F, 3), N(G, 3))
    bE = e8(N(E, 3), N(E, 3), N(B, 2), N(E, 3), N(G, 3), N(E, 3), N(D, 3), N(E, 3))
    hook = mel((0, N(E, 5), 4), (4, N(A, 5), 4), (8, N(G, 5), 4), (12, N(E, 5), 4))
    hook2 = mel((0, N(D, 5), 4), (4, N(C, 5), 4), (8, N(D, 5), 4), (12, N(E, 5), 4))
    hook3 = mel((0, N(A, 5), 6), (6, N(G, 5), 2), (8, N(E, 5), 4), (12, N(D, 5), 4))
    ch1 = mel((0, N(C, 6), 4), (4, N(B, 5), 4), (8, N(A, 5), 4), (12, N(E, 5), 4))
    ch2 = mel((0, N(G, 5), 8), (8, N(A, 5), 8))
    br = mel((0, N(E, 4), 8), (8, N(A, 4), 8))
    ans = mel((0, N(C, 5), 4), (4, N(B, 4), 4), (8, N(A, 4), 4), (12, N(E, 4), 4))
    rest = quiet()
    d, df, d0 = ks(), ks(True), quiet()

    def prog(ld, drums):
        return [(bA, ld[0], drums), (bA, ld[1], drums), (bF, ld[2], drums), (bG, ld[3], drums)]

    bars = []
    bars += [(bA, rest, d0), (bA, rest, d), (bF, rest, d), (bG, rest, df)] * 2  # intro 8
    bars += prog([hook, hook2, hook3, hook2], d) * 4  # A 16
    bars += prog([hook, rest, hook2, rest], d) * 2  # A' 8
    bars += prog([ch1, ch2, ch1, hook3], d) * 4  # chorus 16
    bars += [(bA, rest, d), (bA, rest, d0), (bF, br, d), (bG, br, df)] * 2  # break 8
    bars += prog([hook, hook2, hook3, hook2], d) * 4  # A 16
    bars += prog([ch1, ch2, ch1, ch2], d) * 4  # chorus 16
    bars += [(bE, ans, d), (bE, ans, d), (bF, br, d), (bG, ch1, df)] * 2  # bridge 8
    bars += prog([ch1, hook, ch2, hook3], d) * 4  # final 16
    bars += [(bA, hook, d), (bA, rest, d), (bA, hook2, d), (bA, rest, df)] * 2  # outro 8
    return pack(bars), 3


# ---------------------------------------------------------------------------
# In-game B — "Pulse Lane"  E minor
# ---------------------------------------------------------------------------
def song_level_b() -> tuple[bytes, int]:
    bE = e8(N(E, 3), 0, N(E, 3), N(B, 3), N(E, 3), 0, N(G, 3), N(B, 3))
    bC = e8(N(C, 3), 0, N(C, 3), N(G, 3), N(C, 3), 0, N(E, 3), N(G, 3))
    bD = e8(N(D, 3), 0, N(D, 3), N(A, 3), N(D, 3), 0, N(Fs, 3), N(A, 3))
    bB = e8(N(B, 2), 0, N(B, 2), N(Fs, 3), N(B, 2), 0, N(D, 3), N(Fs, 3))
    h1 = mel((2, N(B, 4), 4), (6, N(E, 5), 4), (10, N(G, 5), 6))
    h2 = mel((0, N(Fs, 5), 4), (4, N(E, 5), 4), (8, N(D, 5), 4), (12, N(B, 4), 4))
    h3 = mel((0, N(G, 5), 8), (8, N(A, 5), 4), (12, N(B, 5), 4))
    h4 = mel((0, N(E, 5), 16))
    c1 = mel((0, N(B, 5), 4), (4, N(D, 6), 4), (8, N(B, 5), 4), (12, N(G, 5), 4))
    c2 = mel((0, N(A, 5), 8), (8, N(Fs, 5), 8))
    rest = quiet()
    d, df, d0 = ks(), ks(True), quiet()

    def prog(lds, drums):
        return [(bE, lds[0], drums), (bC, lds[1], drums), (bD, lds[2], drums), (bB, lds[3], drums)]

    bars = []
    bars += [(bE, rest, d0), (bE, rest, d), (bC, rest, d), (bD, rest, df)] * 2
    bars += prog([h1, h2, h3, h4], d) * 4
    bars += prog([h1, rest, h2, rest], d) * 2
    bars += prog([c1, c2, c1, h3], d) * 4
    bars += [(bE, rest, d), (bE, h4, d0), (bC, rest, d), (bD, h4, df)] * 2
    bars += prog([h1, h2, h3, h4], d) * 4
    bars += prog([c1, c2, h3, c2], d) * 4
    bars += [(bB, h2, d), (bB, h1, d), (bC, c2, d), (bD, c1, df)] * 2
    bars += prog([c1, h1, c2, h4], d) * 4
    bars += [(bE, h1, d), (bE, rest, d), (bE, h4, d), (bE, rest, df)] * 2
    return pack(bars), 3


# ---------------------------------------------------------------------------
# In-game C — "Superzapper"  F# minor, busier bass
# ---------------------------------------------------------------------------
def song_level_c() -> tuple[bytes, int]:
    bFs = e8(N(Fs, 3), N(Fs, 3), N(Cs, 3), N(Fs, 3), N(A, 3), N(Fs, 3), N(E, 3), N(Fs, 3))
    bD = e8(N(D, 3), N(D, 3), N(A, 2), N(D, 3), N(Fs, 3), N(D, 3), N(Cs, 3), N(D, 3))
    bE = e8(N(E, 3), N(E, 3), N(B, 2), N(E, 3), N(G, 3), N(E, 3), N(D, 3), N(E, 3))
    bCs = e8(N(Cs, 3), N(Cs, 3), N(Gs, 2), N(Cs, 3), N(E, 3), N(Cs, 3), N(B, 2), N(Cs, 3))
    h1 = mel((0, N(Cs, 5), 4), (4, N(Fs, 5), 4), (8, N(A, 5), 4), (12, N(Gs, 5), 4))
    h2 = mel((0, N(Fs, 5), 6), (6, N(E, 5), 2), (8, N(Cs, 5), 8))
    h3 = mel((0, N(A, 5), 4), (4, N(B, 5), 4), (8, N(Cs, 6), 4), (12, N(A, 5), 4))
    h4 = mel((0, N(Gs, 5), 8), (8, N(Fs, 5), 8))
    c1 = mel((0, N(Cs, 6), 4), (4, N(B, 5), 4), (8, N(A, 5), 4), (12, N(Fs, 5), 4))
    c2 = mel((0, N(E, 5), 4), (4, N(Fs, 5), 12))
    rest = quiet()
    d, df = ks(), ks(True)

    def prog(lds, drums):
        return [(bFs, lds[0], drums), (bD, lds[1], drums), (bE, lds[2], drums), (bCs, lds[3], drums)]

    bars = []
    bars += [(bFs, rest, quiet()), (bFs, rest, d), (bD, rest, d), (bE, rest, df)] * 2
    bars += prog([h1, h2, h3, h4], d) * 4
    bars += prog([h1, rest, h2, rest], d) * 2
    bars += prog([c1, c2, c1, h3], d) * 4
    bars += [(bFs, rest, d), (bFs, h4, quiet()), (bD, rest, d), (bE, h2, df)] * 2
    bars += prog([h1, h2, h3, h4], d) * 4
    bars += prog([c1, c2, h3, c2], d) * 4
    bars += [(bCs, h4, d), (bCs, h1, d), (bD, c2, d), (bE, c1, df)] * 2
    bars += prog([c1, h1, c2, h4], d) * 4
    bars += [(bFs, h1, d), (bFs, rest, d), (bFs, h2, d), (bFs, rest, df)] * 2
    return pack(bars), 3


# ---------------------------------------------------------------------------
# In-game D — "Outta Here"  C minor, bigger
# ---------------------------------------------------------------------------
def song_level_d() -> tuple[bytes, int]:
    bC = e8(N(C, 3), N(C, 3), N(G, 2), N(C, 3), N(Eb, 3), N(C, 3), N(Bb, 2), N(C, 3))
    bAb = e8(N(Ab, 2), N(Ab, 2), N(Eb, 3), N(Ab, 2), N(C, 3), N(Ab, 2), N(G, 2), N(Ab, 2))
    bBb = e8(N(Bb, 2), N(Bb, 2), N(F, 3), N(Bb, 2), N(D, 3), N(Bb, 2), N(C, 3), N(Bb, 2))
    bG = e8(N(G, 2), N(G, 2), N(D, 3), N(G, 2), N(B, 2), N(G, 2), N(F, 2), N(G, 2))
    h1 = mel((0, N(G, 4), 4), (4, N(C, 5), 4), (8, N(Eb, 5), 4), (12, N(G, 5), 4))
    h2 = mel((0, N(F, 5), 8), (8, N(Eb, 5), 4), (12, N(D, 5), 4))
    h3 = mel((0, N(C, 5), 6), (6, N(Bb, 4), 2), (8, N(G, 4), 8))
    h4 = mel((0, N(Eb, 5), 16))
    c1 = mel((0, N(G, 5), 4), (4, N(C, 6), 4), (8, N(Bb, 5), 4), (12, N(G, 5), 4))
    c2 = mel((0, N(Ab, 5), 8), (8, N(G, 5), 8))
    rest = quiet()
    d, df = ks(), ks(True)

    def prog(lds, drums):
        return [(bC, lds[0], drums), (bAb, lds[1], drums), (bBb, lds[2], drums), (bG, lds[3], drums)]

    bars = []
    bars += [(bC, rest, quiet()), (bC, rest, d), (bAb, rest, d), (bBb, rest, df)] * 2
    bars += prog([h1, h2, h3, h4], d) * 4
    bars += prog([h1, rest, h2, rest], d) * 2
    bars += prog([c1, c2, c1, h3], d) * 4
    bars += [(bC, rest, d), (bC, h4, quiet()), (bAb, rest, d), (bBb, h2, df)] * 2
    bars += prog([h1, h2, h3, h4], d) * 4
    bars += prog([c1, c2, h1, c2], d) * 4
    bars += [(bG, h3, d), (bG, h1, d), (bAb, c2, d), (bBb, c1, df)] * 2
    bars += prog([c1, h1, c2, h4], d) * 4
    bars += [(bC, h1, d), (bC, rest, d), (bC, c1, d), (bC, rest, df)] * 2
    return pack(bars), 3


# ---------------------------------------------------------------------------
# Game over — short descending figure
# ---------------------------------------------------------------------------
def song_over() -> tuple[bytes, int]:
    b = e8(N(A, 2), 0, N(A, 2), 0, N(E, 2), 0, N(A, 2), 0)
    b2 = e8(N(F, 2), 0, N(F, 2), 0, N(C, 3), 0, N(E, 2), 0)
    L1 = mel((0, N(A, 4), 8), (8, N(E, 4), 8))
    L2 = mel((0, N(C, 4), 8), (8, N(A, 3), 8))
    L3 = mel((0, N(E, 4), 16))
    L0 = quiet()
    d0 = quiet()
    d1 = [KICK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, SNARE, 0, 0, 0]
    bars = []
    for _ in range(4):
        bars += [(b, L1, d1), (b, L2, d0), (b2, L3, d1), (b2, L0, d0)]
    return pack(bars), 12


# ---------------------------------------------------------------------------
# Warp sting (short; in-game warp actually keeps the level track)
# ---------------------------------------------------------------------------
def song_warp() -> tuple[bytes, int]:
    b = e8(N(C, 3), N(E, 3), N(G, 3), N(C, 4), N(E, 4), N(G, 4), N(C, 5), N(E, 5))
    L = mel((0, N(G, 5), 4), (4, N(C, 6), 4), (8, N(E, 6), 4), (12, N(G, 6), 4))
    d = ks(True)
    bars = [(b, L, d)] * 16
    return pack(bars), 5


SONGS = [
    ("title", song_title),
    ("levela", song_level_a),
    ("levelb", song_level_b),
    ("levelc", song_level_c),
    ("gameover", song_over),
    ("warp", song_warp),
    ("leveld", song_level_d),
]


def write_header(songs: list[dict], blob_len: int) -> None:
    base = BLOB_BANK0 * 0x4000 + blob_len
    banks, addrs, rows, tempos = [], [], [], []
    lines = [
        "/* generated by tools/compose.py — original GBC chiptunes */",
        "#ifndef TUNES_H",
        "#define TUNES_H",
        "#include <stdint.h>",
        f"#define NSONGS {len(songs)}",
        "static const uint8_t tune_bank[NSONGS] = {",
    ]
    for s in songs:
        rom = base + s["off"]
        banks.append(str(rom // 0x4000))
        addrs.append(f"0x{0x4000 + (rom % 0x4000):04X}")
        rows.append(str(s["rows"]))
        tempos.append(str(s["tempo"]))
        print(f"  {s['name']:10} bank {rom // 0x4000} addr 0x{0x4000 + rom % 0x4000:04X} "
              f"rows {s['rows']} (~{s['rows'] * s['tempo'] / 60:.1f}s)")
    lines += [
        "    " + ", ".join(banks),
        "};",
        "static const uint16_t tune_addr[NSONGS] = {",
        "    " + ", ".join(addrs),
        "};",
        "static const uint16_t tune_rows[NSONGS] = {",
        "    " + ", ".join(rows),
        "};",
        "static const uint8_t tune_tempo[NSONGS] = {",
        "    " + ", ".join(tempos),
        "};",
        "#endif",
        "",
    ]
    (GEN / "tunes.h").write_text("\n".join(lines))


def main() -> None:
    GEN.mkdir(exist_ok=True)
    blob = bytearray()
    songs = []
    for name, fn in SONGS:
        data, tempo = fn()
        bank_off = len(blob) % 0x4000
        if bank_off + len(data) > 0x4000:
            blob += b"\x00" * (0x4000 - bank_off)
        songs.append({"name": name, "off": len(blob), "rows": len(data) // 3, "tempo": tempo})
        blob += data
        print(f"{name:10} rows={len(data) // 3} tempo={tempo} bytes={len(data)}")
    (GEN / "tunes.bin").write_bytes(blob)
    blob_len = (GEN / "blob.bin").stat().st_size if (GEN / "blob.bin").exists() else 0
    write_header(songs, blob_len)
    print(f"wrote gen/tunes.bin {len(blob)} bytes")


if __name__ == "__main__":
    main()
