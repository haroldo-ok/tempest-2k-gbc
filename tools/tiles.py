"""Cut a 160x144 indexed image into 8x8 GBC tiles with flip-aware de-duplication."""
import numpy as np
from raster import W, H


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


def canon(t):
    """Return (key bytes, GBC attr flip bits). Displaying key with those flips yields t."""
    t = (t & 3).astype(np.uint8)
    cands = [
        (t.tobytes(), 0),
        (t[:, ::-1].tobytes(), 0x20),
        (t[::-1, :].tobytes(), 0x40),
        (t[::-1, ::-1].tobytes(), 0x60),
    ]
    k, fl = min(cands, key=lambda x: x[0])
    return k, fl


def cells(img, top=8):
    """Return dict cell_index -> 8x8 array. cell = row*20+col with row 0 at y=top."""
    out = {}
    rows = (H - top) // 8
    cols = W // 8
    for r in range(rows):
        for c in range(cols):
            t = img[top + r * 8 : top + r * 8 + 8, c * 8 : c * 8 + 8]
            if np.any(t):
                out[r * cols + c] = t.copy()
    return out


class TileDict:
    def __init__(self):
        self.tiles = []
        self.index = {}

    def add(self, t):
        """Add 8x8 (values 0-3, lit bit ignored by masking to 3). Returns (id, attr_flip)."""
        t = (t & 3).astype(np.uint8)
        for vf in (0, 1):
            for hf in (0, 1):
                v = t
                if hf:
                    v = v[:, ::-1]
                if vf:
                    v = v[::-1, :]
                key = v.tobytes()
                if key in self.index:
                    return self.index[key], (hf << 5) | (vf << 6)
        key = t.tobytes()
        tid = len(self.tiles)
        self.index[key] = tid
        self.tiles.append(t.copy())
        return tid, 0

    def pack(self):
        return b"".join(gb_tile(t) for t in self.tiles)
