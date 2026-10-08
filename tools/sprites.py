"""Pre-rendered, pre-scaled, pre-rotated sprite art for GBC (2bpp, 4 colours).
Every image is a 16x16 box stored as four 8x8 2bpp tiles in 8x16-sprite order
(left column top/bottom, right column top/bottom). Size index 0 is 8px wide.
"""
import math
import numpy as np
from PIL import Image, ImageDraw

SIZES = [5, 7, 9, 11, 13]
NANG = 16
# 4-colour indices (0 = transparent)
T, DARK, MAIN, HI = 0, 1, 2, 3


def frame(a):
    th = a * 2 * math.pi / NANG
    n = (math.cos(th), math.sin(th))
    U = (-n[1], n[0])
    V = (-n[0], -n[1])
    return U, V


def xf(pts, cx, cy, r, a):
    U, V = frame(a)
    return [(cx + r * (x * U[0] + y * V[0]), cy + r * (x * U[1] + y * V[1])) for (x, y) in pts]


def new(sz):
    w = 8 if sz == 0 else 16
    return Image.new("P", (w, 16), 0), ((w - 1) / 2.0, 7.5)


def poly(d, pts, fill, outline):
    if fill:
        d.polygon(pts, fill=fill)
    if outline:
        d.line(pts + [pts[0]], fill=outline, width=1)


def gb_tile(t):
    b = bytearray()
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            v = int(t[y, x]) & 3
            if v & 1:
                lo |= 0x80 >> x
            if v & 2:
                hi |= 0x80 >> x
        b.append(lo)
        b.append(hi)
    return bytes(b)


def pack(img):
    a = np.array(img, np.uint8)
    if a.shape[1] == 8:
        a = np.concatenate([a, np.zeros((16, 8), np.uint8)], 1)
    out = bytearray()
    for col in (0, 1):
        for row in (0, 1):
            t = a[row * 8 : row * 8 + 8, col * 8 : col * 8 + 8]
            out += gb_tile(t)
    return bytes(out)


CLAW = [(-1, 0.55), (-0.75, -0.7), (-0.35, 0.05), (0, -0.35), (0.35, 0.05), (0.75, -0.7), (1, 0.55), (0, 0.95)]
FLIP_L = [(-1, -0.6), (-1, 0.6), (0, 0)]
FLIP_R = [(1, -0.6), (1, 0.6), (0, 0)]
DIAM = [(0, -1), (1, 0), (0, 1), (-1, 0)]
CROWN = [(-1, 0.6), (-0.9, -0.8), (-0.4, 0), (0, -1), (0.4, 0), (0.9, -0.8), (1, 0.6)]
BAR = [(-1, -0.3), (1, -0.3), (1, 0.3), (-1, 0.3)]


def draw_kind(kind, a, anim, sz):
    img, (cx, cy) = new(sz)
    d = ImageDraw.Draw(img)
    r = SIZES[sz] / 2.0
    if kind == "claw":
        poly(d, xf(CLAW, cx, cy, r, a), MAIN, HI)
    elif kind in ("flipper", "mutant"):
        poly(d, xf(FLIP_L, cx, cy, r, a), DARK, MAIN)
        poly(d, xf(FLIP_R, cx, cy, r, a), DARK, MAIN)
    elif kind.startswith("tanker"):
        poly(d, xf(DIAM, cx, cy, r, a), DARK, MAIN)
        poly(d, xf(DIAM, cx, cy, r * 0.42, a), HI, HI)
    elif kind == "spiker":
        pts = []
        for i in range(25):
            t = i / 24.0
            th = t * 3.4 * math.pi + anim * math.pi / 2
            rr = r * (0.15 + 0.85 * t)
            pts.append((cx + rr * math.cos(th), cy + rr * math.sin(th)))
        d.line(pts, fill=MAIN, width=1)
        d.point([(round(cx), round(cy))], fill=HI)
    elif kind == "fuse":
        for k in range(5):
            th = k * 2 * math.pi / 5 + anim * math.pi / 5
            col = (MAIN, HI, HI, MAIN, DARK)[k]
            mid = (cx + r * 0.55 * math.cos(th + 0.5), cy + r * 0.55 * math.sin(th + 0.5))
            end = (cx + r * math.cos(th), cy + r * math.sin(th))
            d.line([(cx, cy), mid, end], fill=col, width=1)
    elif kind == "pulsar":
        col = HI if anim else MAIN
        pts = [(-1 + i * 0.25, (0.45 if i % 2 else -0.45) if 0 < i < 8 else 0) for i in range(9)]
        d.line(xf(pts, cx, cy, r, a), fill=col, width=1)
        if anim:
            d.line(xf([(-1, 0), (1, 0)], cx, cy, r * 0.6, a), fill=DARK, width=1)
    elif kind == "demon":
        poly(d, xf(CROWN, cx, cy, r, a), DARK, MAIN)
    elif kind == "mirror":
        poly(d, xf(BAR, cx, cy, r, a), DARK, HI)
    elif kind == "ufo":
        e = [(cx + r * math.cos(t), cy + r * 0.45 * math.sin(t)) for t in np.linspace(0, 2 * math.pi, 14)[:-1]]
        poly(d, e, MAIN, HI)
        d.ellipse([cx - r * 0.4, cy - r * 0.7, cx + r * 0.4, cy - r * 0.1], fill=DARK)
    elif kind == "powerup":
        rr = r if anim == 0 else r * 0.8
        poly(d, [(cx, cy - rr), (cx + rr, cy), (cx, cy + rr), (cx - rr, cy)], None, HI)
        q = r * (0.45 if anim == 0 else 0.6)
        d.line([(cx - q, cy), (cx + q, cy)], fill=MAIN)
        d.line([(cx, cy - q), (cx, cy + q)], fill=MAIN)
    return img


KINDS = [
    ("claw", 16, 1),
    ("flipper", 16, 1),
    ("mutant", 16, 1),
    ("tanker", 16, 1),
    ("tankerp", 16, 1),
    ("tankerf", 16, 1),
    ("spiker", 1, 4),
    ("fuse", 1, 2),
    ("pulsar", 16, 2),
    ("demon", 16, 1),
    ("mirror", 16, 1),
    ("ufo", 1, 1),
    ("powerup", 1, 2),
]


def build_images():
    imgs, base = [], {}
    for kind, na, nn in KINDS:
        base[kind] = len(imgs)
        for a in range(na):
            for an in range(nn):
                for sz in range(5):
                    ang = a * (NANG // na) if na > 1 else 0
                    imgs.append(pack(draw_kind(kind, ang, an, sz)))
    return imgs, base


def tall(draw_fn):
    img = Image.new("P", (8, 16), 0)
    d = ImageDraw.Draw(img)
    draw_fn(d)
    a = np.array(img, np.uint8)
    return gb_tile(a[0:8]) + gb_tile(a[8:16])


def big(draw_fn):
    img = Image.new("P", (16, 16), 0)
    d = ImageDraw.Draw(img)
    draw_fn(d)
    return pack(img)


def static_tiles():
    S = {}
    for i, r in enumerate((1, 2, 3)):
        S["shot%d" % i] = tall(
            lambda d, r=r: poly(
                d, [(3.5, 7.5 - r), (3.5 + r, 7.5), (3.5, 7.5 + r), (3.5 - r, 7.5)], HI, HI
            )
        )
    S["spiketip"] = tall(
        lambda d: poly(d, [(3.5, 5), (6, 7.5), (3.5, 10), (1, 7.5)], HI, MAIN)
    )
    S["spikedot"] = tall(lambda d: d.rectangle([3, 6, 4, 9], fill=MAIN))
    S["droid"] = tall(
        lambda d: poly(d, [(3.5, 4.5), (6.5, 7.5), (3.5, 10.5), (0.5, 7.5)], None, HI)
    )
    for i, r in enumerate((1, 2, 3)):
        S["laser%d" % i] = tall(
            lambda d, r=r: (
                d.line([(3.5 - r, 7.5), (3.5 + r, 7.5)], fill=HI),
                d.line([(3.5, 7.5 - r - 1), (3.5, 7.5 + r + 1)], fill=MAIN),
            )
        )
    for i, r in enumerate((1, 2, 3)):
        S["eshot%d" % i] = tall(
            lambda d, r=r: (
                d.line([(3.5 - r, 7.5 - r), (3.5 + r, 7.5 + r)], fill=MAIN),
                d.line([(3.5 - r, 7.5 + r), (3.5 + r, 7.5 - r)], fill=HI),
            )
        )
    for f in range(4):
        def ex(d, f=f):
            n = 8
            cols = (HI, HI, MAIN, DARK)
            for k in range(n):
                th = k * 2 * math.pi / n + f * 0.3
                r0, r1 = 1 + f * 1.6, 3 + f * 1.6
                d.line(
                    [
                        (7.5 + r0 * math.cos(th), 7.5 + r0 * math.sin(th)),
                        (7.5 + r1 * math.cos(th), 7.5 + r1 * math.sin(th)),
                    ],
                    fill=cols[f],
                )

        S["expl%d" % f] = big(ex)
    return S
