"""Web geometry + projection for the GBC port (160x144, HUD on row 0)."""
import math
from webs import load

CX, CY = 80.0, 78.0
CAM_Z = 14.0
DEPTH = 16.0
WEBS = load()


class Web:
    def __init__(self, w):
        self.__dict__.update(w)
        xs = [p[0] for p in self.pts]
        ys = [p[1] for p in self.pts]
        self.cx = (min(xs) + max(xs)) / 2.0
        self.cy = (min(ys) + max(ys)) / 2.0
        if not self.closed and (max(ys) - min(ys)) < 3:
            self.cy = min(ys) - 6
        F = 96.0
        for (x, y) in self.pts:
            dx, dy = x - self.cx, y - self.cy
            if dx:
                F = min(F, 74.0 * CAM_Z / abs(dx))
            if dy > 0:
                F = min(F, (140 - CY) * CAM_Z / dy)
            if dy < 0:
                F = min(F, (CY - 12) * CAM_Z / -dy)
        self.F = max(28.0, F)

    def proj(self, x, y, z, cam_z=0.0, rot=0.0):
        d = z + CAM_Z + cam_z
        if d < 0.05:
            d = 0.05
        s = self.F / d
        px, py = x - self.cx, y - self.cy
        if rot:
            c, sn = math.cos(rot), math.sin(rot)
            px, py = px * c - py * sn, px * sn + py * c
        return CX + px * s, CY + py * s

    def vert(self, i):
        return self.pts[i]


WEBOBJ = [Web(w) for w in WEBS]
UNIQUE = []
for w in WEBOBJ:
    if w.name not in UNIQUE:
        UNIQUE.append(w.name)
BYNAME = {w.name: w for w in WEBOBJ}

if __name__ == "__main__":
    for w in WEBOBJ[:16]:
        print(w.level, w.name, round(w.F, 1), w.lanes)
