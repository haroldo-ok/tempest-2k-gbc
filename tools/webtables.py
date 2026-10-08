"""Per-web projection tables (positions, sizes, angles, flip paths) for GBC 160x144."""
import math
from geom import DEPTH
from sprites import SIZES, NANG

NZ = 81  # z = -4 .. 16 in 0.25 steps


def zval(i):
    return -4.0 + i * 0.25


def clampb(v):
    return max(0, min(255, int(round(v))))


def size_idx(px):
    s = px * 0.9
    return max(0, min(4, int(round((s - 5) / 2.0))))


def lane_geom(web, l, z):
    a = web.proj(*web.pts[l], z)
    b = web.proj(*web.pts[l + 1], z)
    c = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
    w = math.hypot(b[0] - a[0], b[1] - a[1])
    return a, b, c, w


def lane_angle(web, l):
    _, _, c0, _ = lane_geom(web, l, 0.0)
    _, _, c1, _ = lane_geom(web, l, DEPTH)
    th = math.atan2(c1[1] - c0[1], c1[0] - c0[0])
    return int(round(th / (2 * math.pi / NANG))) % NANG, th


def build(web):
    out = bytearray(0x1600)
    n = web.lanes
    out[0] = n
    out[1] = web.closed
    out[2] = min(web.start, n - 1)
    angs = [lane_angle(web, l) for l in range(n)]
    for l in range(n):
        out[4 + l] = angs[l][0]
    for l in range(n):
        a, b, c, w = lane_geom(web, l, 0.0)
        for k in range(8):
            t = (k + 0.5) / 8
            out[0x20 + (l * 8 + k) * 2] = clampb(a[0] + (b[0] - a[0]) * t)
            out[0x20 + (l * 8 + k) * 2 + 1] = clampb(a[1] + (b[1] - a[1]) * t)
    for l in range(n):
        a, b, c, w = lane_geom(web, l, 0.0)
        th0 = angs[l][1]
        outward = (-math.cos(th0), -math.sin(th0))
        for d in (0, 1):
            nl = l + (1 if d else -1)
            if web.closed:
                nl %= n
            nl = max(0, min(n - 1, nl))
            piv = b if d else a
            _, _, ct, _ = lane_geom(web, nl, 0.0)
            best = None
            for sgn in (1, -1):
                ph = sgn * math.pi / 2
                vx, vy = c[0] - piv[0], c[1] - piv[1]
                mx = piv[0] + vx * math.cos(ph) - vy * math.sin(ph)
                my = piv[1] + vx * math.sin(ph) + vy * math.cos(ph)
                score = (mx - c[0]) * outward[0] + (my - c[1]) * outward[1]
                if best is None or score > best[0]:
                    best = (score, sgn)
            sgn = best[1]
            for k in range(8):
                f = (k + 1) / 8.0
                ph = sgn * math.pi * f
                vx, vy = c[0] - piv[0], c[1] - piv[1]
                px = piv[0] + vx * math.cos(ph) - vy * math.sin(ph)
                py = piv[1] + vx * math.sin(ph) + vy * math.cos(ph)
                ex = piv[0] - vx
                ey = piv[1] - vy
                px += (ct[0] - ex) * f
                py += (ct[1] - ey) * f
                ang = int(round((th0 + ph) / (2 * math.pi / NANG))) % NANG
                o = 0x160 + ((l * 2 + d) * 8 + k) * 3
                out[o] = clampb(px)
                out[o + 1] = clampb(py)
                out[o + 2] = ang
    for l in range(n):
        for zi in range(NZ):
            a, b, c, w = lane_geom(web, l, zval(zi))
            o = 0x4C0 + (l * NZ + zi) * 3
            out[o] = clampb(c[0])
            out[o + 1] = clampb(c[1])
            out[o + 2] = size_idx(w)
    return bytes(out)
