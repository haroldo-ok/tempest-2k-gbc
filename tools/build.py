"""Build all offline data for the GBC port: gen/blob.bin (ROM banks 2..) + gen/assets.h/.c"""
import os
import sys
import math
import struct
import numpy as np
from PIL import Image

from geom import WEBOBJ, BYNAME, UNIQUE
from render_web import draw
from tiles import TileDict, cells, gb_tile
from raster import W, H, canvas
from fmvenc import Dict as FMVDict, frame_map, VState, encode_frame, converge, canonical_state
import webtables
import sprites
import font

QUICK = "--quick" in sys.argv
OUT = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else os.path.join(os.path.dirname(__file__), "..", "gen")
os.makedirs(OUT, exist_ok=True)
webs_to_do = UNIQUE[:3] if QUICK else UNIQUE

BANK = 0x4000
BANK_SIZE = 0x4000
MAP_W, MAP_H = 20, 17  # playfield under HUD
HUD_H = 8
WEB_TILE0 = 41  # tiles 1-40 = font
# GBC: tile index is 8-bit + VRAM bank bit. Bank 0 and bank 1 each give 215 FMV slots.
SLOT_FMV = list(range(41, 256)) + list(range(256 + 41, 512))
N_WARP = 32
N_ZOOM = 32
N_TITLE = 32
MAX_UP = 12  # tile uploads per FMV step (vblank budget)


class Blob:
    def __init__(self, first=2):
        self.first = first
        self.banks = []

    def new_bank(self):
        self.banks.append(bytearray())
        return self.first + len(self.banks) - 1

    def put(self, data, new_bank=False, align=1):
        data = bytes(data)
        if new_bank or not self.banks or (align > 1 and (len(self.banks[-1]) % align)):
            pass
        if new_bank or not self.banks:
            self.new_bank()
        if align > 1:
            pad = (-len(self.banks[-1])) % align
            if pad:
                if len(self.banks[-1]) + pad + len(data) > BANK_SIZE:
                    self.new_bank()
                else:
                    self.banks[-1] += bytes(pad)
        if len(self.banks[-1]) + len(data) > BANK_SIZE:
            self.new_bank()
            if len(data) > BANK_SIZE:
                raise SystemExit("record too large for one bank: %d" % len(data))
        b = self.first + len(self.banks) - 1
        off = len(self.banks[-1])
        self.banks[-1] += data
        return b, off

    def put_stream(self, chunks):
        """Place steps sequentially; 0xFF marker means continue in the next bank."""
        start = None
        for c in chunks:
            c = bytes(c)
            if len(c) + 1 > BANK_SIZE:
                raise SystemExit("FMV step too large: %d" % len(c))
            if not self.banks:
                self.new_bank()
            b = len(self.banks) - 1
            if len(self.banks[b]) + len(c) + 1 > BANK_SIZE:
                self.banks[b].append(0xFF)
                self.new_bank()
                b = len(self.banks) - 1
            if start is None:
                start = (self.first + b, len(self.banks[b]))
            self.banks[b] += c
        return start

    def at(self, bank, off, data):
        i = bank - self.first
        self.banks[i][off : off + len(data)] = data

    def bytes(self):
        out = bytearray()
        for b in self.banks:
            out += b
            if len(b) < BANK_SIZE:
                out += bytes(BANK_SIZE - len(b))
        return bytes(out)


def encode_map(img, pal=1):
    """Return ntiles, tilebytes, map[340], attr[340] using a private dictionary.
    Tile ids are 0-based; caller adds WEB_TILE0."""
    D = TileDict()
    mp = bytearray(MAP_W * MAP_H)
    at = bytearray(MAP_W * MAP_H)
    cs = cells(img, top=HUD_H)
    for idx, t in cs.items():
        tid, fl = D.add(t)
        if tid > 214:
            raise SystemExit("too many unique tiles")
        mp[idx] = tid + WEB_TILE0  # 0 stays blank; web tiles live at 41+
        at[idx] = fl | pal
    return D, bytes(mp), bytes(at)


def text_px(img, s, x, y, c, scale=1):
    for i, ch in enumerate(s):
        if ch == " ":
            continue
        if ch not in font.G:
            continue
        g = font.glyph_px(ch)
        for yy in range(8):
            for xx in range(8):
                if g[yy, xx]:
                    y0 = y + yy * scale
                    x0 = x + (i * 8 + xx) * scale
                    img[y0 : y0 + scale, x0 : x0 + scale] = c


def title_frames(n=N_TITLE):
    w = BYNAME["web5"]
    out = []
    for k in range(n):
        img = draw(w, cam_z=10.0, rot=2 * math.pi * k / n)
        text_px(img, "TEMPEST", 24, 8, 3, 2)
        text_px(img, "2000", 64, 26, 3, 1)
        text_px(img, "PRESS START", 36, 128, 3, 1)
        out.append(img)
    return out


def warp_frames(w):
    out = []
    for k in range(N_WARP):
        t = k / (N_WARP - 1)
        e = (1 - t) ** 2.2
        out.append(draw(w, cam_z=44.0 * e, rot=math.pi * e))
    return out


def zoom_frames(w):
    out, aux = [], []
    for k in range(N_ZOOM):
        t = k / (N_ZOOM - 1)
        cz = -24.0 * (t ** 1.6)
        out.append(draw(w, cam_z=cz))
        aux.append(max(0, min(255, int(round(-cz * 4)))))
    return out, aux


def enc_step(ups, nt, flags, hold=0, aux=0):
    """flags bit0=visible, bit1=last. NT entries are (slot, attr) or None."""
    if len(ups) > 255:
        raise SystemExit("too many uploads in one step")
    b = bytearray([flags, hold, aux, len(ups)])
    for s, k in ups:
        slot8 = s & 255
        bank = 1 if s >= 256 else 0
        b += bytes([slot8, bank, k & 0xFF, (k >> 8) & 0xFF])
    cellsn = sorted(nt)
    runs = []
    for c in cellsn:
        if nt[c] is None:
            ent = (0, 0)
        else:
            slot, fl = nt[c]
            bankbit = 0x08 if slot >= 256 else 0
            ent = (slot & 255, (fl & 0x60) | 1 | bankbit)
        same_row = runs and (runs[-1][0] % 20 + len(runs[-1][1]) < 20)
        contig = runs and runs[-1][0] + len(runs[-1][1]) == c
        if contig and same_row and len(runs[-1][1]) < 20:
            runs[-1][1].append(ent)
        else:
            runs.append([c, [ent]])
    b += struct.pack("<H", len(runs))
    for c, es in runs:
        b += bytes([c % 20, c // 20 + 1, len(es)])
        for tile, attr in es:
            b += bytes([tile, attr])
    return bytes(b)


def split_step(ups, nt, flags, hold=0, aux=0):
    chunks = []
    if not ups:
        chunks.append(enc_step([], nt, flags, hold, aux))
        return chunks
    i = 0
    while i < len(ups):
        part = ups[i : i + MAX_UP]
        i += MAX_UP
        last = i >= len(ups)
        chunks.append(
            enc_step(part, nt if last else {}, flags if last else 0, hold if last else 0, aux if last else 0)
        )
    return chunks


blob = Blob(first=3)  # banks 0=fixed, 1=game, 2=sound; data from 3

# ---- font (40 glyphs x 16 bytes) -----------------------------------------
fontdata = bytearray()
for ch in font.ORDER:
    g = font.glyph_px(ch)
    fontdata += gb_tile(g * 3)  # colour 3 = white in HUD palette
font_loc = blob.put(bytes(fontdata), new_bank=True)

# ---- static sprite tiles --------------------------------------------------
static = sprites.static_tiles()
SAFE_ORDER = ["shot0", "shot1", "shot2", "spiketip", "spikedot", "droid"]
OTHER_ORDER = ["laser0", "laser1", "laser2", "eshot0", "eshot1", "eshot2", "expl0", "expl1", "expl2", "expl3"]
safe = b"".join(static[k] for k in SAFE_ORDER)
other = b"".join(static[k] for k in OTHER_ORDER)
safe_loc = blob.put(safe, align=16)
other_loc = blob.put(other, align=16)

# ---- sprite images: 256 x 64 bytes per bank -------------------------------
imgs, imgbase = sprites.build_images()
assert all(len(im) == 64 for im in imgs)
img_bank0 = None
for i in range(0, len(imgs), 256):
    chunk = b"".join(imgs[i : i + 256])
    loc = blob.put(chunk, new_bank=True)
    if img_bank0 is None:
        img_bank0 = loc[0]
    assert loc[1] == 0

# ---- web tables (2 per bank, 8 KiB each) ----------------------------------
web_loc = {}
for i, name in enumerate(UNIQUE):
    w = BYNAME[name]
    t = webtables.build(w)
    rec = bytearray(0x2000)
    rec[: len(t)] = t
    if i % 2 == 0:
        b, off = blob.put(bytes(rec), new_bank=True)
    else:
        b, off = blob.put(bytes(rec))
    web_loc[name] = (b, off)

# ---- per-web gfx: tiles + map + attr + lit lists --------------------------
gfx_loc = {}
max_tiles = 0
for name in webs_to_do:
    w = BYNAME[name]
    img = draw(w)
    D, mp, at = encode_map(img, pal=1)
    ntiles = len(D.tiles)
    max_tiles = max(max_tiles, ntiles)
    tileb = D.pack()
    # lit cell lists
    base_cs = cells(img, top=HUD_H)
    litb = bytearray()
    for lane in range(w.lanes):
        li = draw(w, lit=lane)
        lc = cells(li, top=HUD_H)
        diff = []
        for c in sorted(set(lc) | set(base_cs)):
            if c not in base_cs:
                continue
            if c not in lc or not np.array_equal(lc[c] & 3, base_cs[c] & 3) or np.any((lc[c] >= 4) != (base_cs[c] >= 4)):
                # highlight if this cell has any lit pixels in the lit image
                if c in lc and np.any(lc[c] >= 4):
                    diff.append(c)
            elif c in lc and np.any(lc[c] >= 4):
                diff.append(c)
        # unique, cap
        diff = sorted(set(diff))[:40]
        litb.append(len(diff))
        for c in diff:
            litb.append(c % 20)           # x
            litb.append(c // 20 + 1)      # y (playfield, HUD is row 0)
    rec = bytearray([ntiles, MAP_W, MAP_H]) + tileb + mp + at + bytes([w.lanes]) + litb
    loc = blob.put(bytes(rec))
    gfx_loc[name] = loc
    print(name, "tiles", ntiles, "lit", len(litb), "at", loc, flush=True)

# webs not built in --quick still need placeholder gfx pointing at first
if QUICK:
    for name in UNIQUE:
        if name not in gfx_loc:
            gfx_loc[name] = gfx_loc[webs_to_do[0]]

# ---- FMV clips: warp-in, zoom-out, title ----------------------------------
FMV = FMVDict()
warp_loc, zoom_loc = {}, {}
for name in webs_to_do:
    w = BYNAME[name]
    base = draw(w)
    fm = frame_map(base, FMV)
    cs = canonical_state(SLOT_FMV, fm)
    # warp-in
    fr = [frame_map(i, FMV) for i in warp_frames(w)]
    v = VState(SLOT_FMV)
    chunks = []
    for f in fr:
        ups, nt = encode_frame(v, f)
        chunks += split_step(ups, nt, 1, 2, 0)
    conv = converge(v, cs)
    for k, st in enumerate(conv):
        last = k == len(conv) - 1
        chunks += split_step(st[0], st[1], 2 if last else 0, 0, 0)
    warp_loc[name] = blob.put_stream(chunks)
    # zoom-out from canonical
    zf, aux = zoom_frames(w)
    v = cs.copy()
    chunks = []
    for k, img in enumerate(zf):
        ups, nt = encode_frame(v, frame_map(img, FMV))
        last = k == len(zf) - 1
        chunks += split_step(ups, nt, 1 | (2 if last else 0), 2, aux[k])
    zoom_loc[name] = blob.put_stream(chunks)
    print(name, "fmv dict", len(FMV.tiles), "warp", warp_loc[name], "zoom", zoom_loc[name], flush=True)

if QUICK:
    for name in UNIQUE:
        if name not in warp_loc:
            warp_loc[name] = warp_loc[webs_to_do[0]]
            zoom_loc[name] = zoom_loc[webs_to_do[0]]

# title intro + looping body
tf = [frame_map(i, FMV) for i in title_frames(N_TITLE)]
v = VState(SLOT_FMV)
intro = []
for f in tf:
    ups, nt = encode_frame(v, f)
    intro += split_step(ups, nt, 1, 3, 0)
ref = v.referenced()
v.content = {s: k for s, k in v.content.items() if s in ref}
target = v.copy()
body = []
for f in tf:
    ups, nt = encode_frame(v, f)
    body += split_step(ups, nt, 1, 3, 0)
conv = converge(v, target)
for k, st in enumerate(conv):
    last = k == len(conv) - 1
    body += split_step(st[0], st[1], 2 if last else 0, 0, 0)
title_loc = blob.put_stream(intro)
title_loop_loc = blob.put_stream(body)
print("title fmv", title_loc, "loop", title_loop_loc, "dict", len(FMV.tiles), flush=True)

# tile dictionary: 1024 tiles x 16 bytes per bank
def pack_key(key):
    a = np.frombuffer(key, np.uint8).reshape(8, 8)
    return gb_tile(a)

dict_bank0 = None
for i in range(0, len(FMV.tiles), 1024):
    loc = blob.put(b"".join(pack_key(k) for k in FMV.tiles[i : i + 1024]), new_bank=True)
    if dict_bank0 is None:
        dict_bank0 = loc[0]
    assert loc[1] == 0
if dict_bank0 is None:
    dict_bank0 = blob.put(b"\x00" * 16, new_bank=True)[0]

# ---- write blob -----------------------------------------------------------
open(os.path.join(OUT, "blob.bin"), "wb").write(blob.bytes())
print("banks", blob.first + len(blob.banks), "blob", len(blob.banks) * BANK_SIZE, "dict tiles", len(FMV.tiles), "max static", max_tiles)

# preview sheets
pal = np.array(
    [[0, 0, 0], [40, 0, 80], [120, 40, 200], [255, 180, 255], [0, 0, 0], [80, 60, 0], [220, 180, 0], [255, 255, 160]],
    np.uint8,
)
sheet = []
for name in UNIQUE[:16]:
    sheet.append(pal[draw(BYNAME[name]) & 7])
if sheet:
    row = np.concatenate(sheet, 1) if False else None
    # 4x4 contact
    tiles_img = []
    for r in range(4):
        rowi = [pal[draw(BYNAME[UNIQUE[r * 4 + c]]) & 7] for c in range(4)]
        tiles_img.append(np.concatenate(rowi, 1))
    Image.fromarray(np.concatenate(tiles_img, 0)).save(os.path.join(OUT, "webs_sheet.png"))
Image.fromarray(pal[title_frames()[0] & 7]).save(os.path.join(OUT, "title.png"))

lvl_web = [UNIQUE.index(w.name) for w in WEBOBJ]
n = len(UNIQUE)

h = open(os.path.join(OUT, "assets.h"), "w")
h.write("/* generated by tools/build.py */\n#ifndef ASSETS_H\n#define ASSETS_H\n")
h.write("#define FONT_BANK %d\n#define FONT_ADDR 0x%04X\n#define FONT_COUNT %d\n" % (font_loc[0], BANK + font_loc[1], len(font.ORDER)))
h.write("#define SAFE_BANK %d\n#define SAFE_ADDR 0x%04X\n#define SAFE_SIZE %d\n" % (safe_loc[0], BANK + safe_loc[1], len(safe)))
h.write("#define OTHER_BANK %d\n#define OTHER_ADDR 0x%04X\n#define OTHER_SIZE %d\n" % (other_loc[0], BANK + other_loc[1], len(other)))
h.write("#define IMG_BANK0 %d\n#define IMG_COUNT %d\n" % (img_bank0, len(imgs)))
for k, v in imgbase.items():
    h.write("#define IMG_%s %d\n" % (k.upper(), v))
h.write("#define NUM_UWEBS %d\n#define NUM_LEVEL_WEBS 48\n" % n)
h.write("#define WEB_TILE0 %d\n" % WEB_TILE0)
h.write("#define DICT_BANK0 %d\n" % dict_bank0)
h.write("#define TITLE_FMV_BANK %d\n#define TITLE_FMV_ADDR 0x%04X\n" % (title_loc[0], BANK + title_loc[1]))
h.write("#define TITLE_LOOP_BANK %d\n#define TITLE_LOOP_ADDR 0x%04X\n" % (title_loop_loc[0], BANK + title_loop_loc[1]))
h.write("extern const unsigned char level_web[48];\n")
h.write("extern const unsigned char uweb_bank[%d];\n" % n)
h.write("extern const unsigned int uweb_addr[%d];\n" % n)
h.write("extern const unsigned char gfx_bank[%d];\n" % n)
h.write("extern const unsigned int gfx_addr[%d];\n" % n)
h.write("extern const unsigned char warp_bank[%d];\n" % n)
h.write("extern const unsigned int warp_addr[%d];\n" % n)
h.write("extern const unsigned char zoom_bank[%d];\n" % n)
h.write("extern const unsigned int zoom_addr[%d];\n" % n)
h.write("#endif\n")
h.close()

c = open(os.path.join(OUT, "assets.c"), "w")
c.write('#include "assets.h"\n')
c.write("const unsigned char level_web[48] = {%s};\n" % ",".join(map(str, lvl_web)))
c.write("const unsigned char uweb_bank[%d] = {%s};\n" % (n, ",".join(str(web_loc[nm][0]) for nm in UNIQUE)))
c.write("const unsigned int uweb_addr[%d] = {%s};\n" % (n, ",".join("0x%04X" % (BANK + web_loc[nm][1]) for nm in UNIQUE)))
c.write("const unsigned char gfx_bank[%d] = {%s};\n" % (n, ",".join(str(gfx_loc[nm][0]) for nm in UNIQUE)))
c.write("const unsigned int gfx_addr[%d] = {%s};\n" % (n, ",".join("0x%04X" % (BANK + gfx_loc[nm][1]) for nm in UNIQUE)))
c.write("const unsigned char warp_bank[%d] = {%s};\n" % (n, ",".join(str(warp_loc[nm][0]) for nm in UNIQUE)))
c.write("const unsigned int warp_addr[%d] = {%s};\n" % (n, ",".join("0x%04X" % (BANK + warp_loc[nm][1]) for nm in UNIQUE)))
c.write("const unsigned char zoom_bank[%d] = {%s};\n" % (n, ",".join(str(zoom_loc[nm][0]) for nm in UNIQUE)))
c.write("const unsigned int zoom_addr[%d] = {%s};\n" % (n, ",".join("0x%04X" % (BANK + zoom_loc[nm][1]) for nm in UNIQUE)))
c.close()
print("wrote", OUT)
