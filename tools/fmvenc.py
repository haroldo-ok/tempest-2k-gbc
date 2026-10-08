"""FMV encoder: VRAM slot model for GBC 160x144 (playfield 20x17 under the HUD).

Each step = (uploads[(slot, key_id)], nt_writes{cell: (slot, attr_flip)}).
Keys are 2bpp 8x8 tiles, canonical under h/v flip.
"""
from tiles import cells, canon


class Dict:
    def __init__(self):
        self.ids = {}
        self.tiles = []

    def get(self, key):
        i = self.ids.get(key)
        if i is None:
            i = len(self.tiles)
            self.ids[key] = i
            self.tiles.append(key)
        return i


def frame_map(img, D):
    m = {}
    for cell, t in cells(img, top=8).items():
        k, fl = canon(t)
        m[cell] = (D.get(k), fl)
    return m


class VState:
    def __init__(self, slots):
        self.slots = list(slots)
        self.content = {}
        self.nt = {}
        self.age = {s: 0 for s in slots}
        self.clock = 0

    def copy(self):
        v = VState(self.slots)
        v.content = dict(self.content)
        v.nt = dict(self.nt)
        v.age = dict(self.age)
        v.clock = self.clock
        return v

    def referenced(self):
        return set(s for (s, f) in self.nt.values())


def encode_frame(v, fmap):
    v.clock += 1
    ref = v.referenced()
    where = {}
    for s, k in v.content.items():
        where.setdefault(k, s)
    for cell, (s, f) in v.nt.items():
        where[v.content[s]] = s
    need = set(k for (k, f) in fmap.values())
    uploads = []
    free = [s for s in v.slots if s not in ref and v.content.get(s) not in need]
    free.sort(key=lambda s: (v.content.get(s) is not None, v.age[s]))
    fi = 0
    for k in sorted(need):
        if k in where:
            v.age[where[k]] = v.clock
            continue
        if fi >= len(free):
            raise RuntimeError("out of FMV slots (%d need, %d free)" % (len(need), len(free)))
        s = free[fi]
        fi += 1
        old = v.content.get(s)
        if old is not None and where.get(old) == s:
            del where[old]
        v.content[s] = k
        where[k] = s
        v.age[s] = v.clock
        uploads.append((s, k))
    nt = {}
    newnt = {}
    for cell, (k, f) in fmap.items():
        e = (where[k], f)
        newnt[cell] = e
        if v.nt.get(cell) != e:
            nt[cell] = e
    for cell in v.nt:
        if cell not in fmap:
            nt[cell] = None
    v.nt = newnt
    return (uploads, nt)


def converge(v, target):
    steps = []
    tslot = {}
    for cell, (s, f) in target.nt.items():
        tslot[s] = target.content[s]
    T = set(tslot)
    ref = v.referenced()
    scratch = [s for s in v.slots if s not in T and s not in ref]
    ups, nt = [], {}
    moved = {}
    for s in sorted(T):
        if s in ref and v.content.get(s) != tslot[s]:
            f = scratch.pop(0)
            ups.append((f, v.content[s]))
            moved[s] = f
            v.content[f] = v.content[s]
    if moved:
        for cell, (s, fl) in list(v.nt.items()):
            if s in moved:
                v.nt[cell] = (moved[s], fl)
                nt[cell] = v.nt[cell]
        steps.append((ups, nt))
    ups = []
    for s in sorted(T):
        if v.content.get(s) != tslot[s]:
            ups.append((s, tslot[s]))
            v.content[s] = tslot[s]
    nt = {}
    for cell, e in target.nt.items():
        if v.nt.get(cell) != e:
            nt[cell] = e
    for cell in v.nt:
        if cell not in target.nt:
            nt[cell] = None
    v.nt = dict(target.nt)
    steps.append((ups, nt))
    ref = v.referenced()
    v.content = {s: k for s, k in v.content.items() if s in ref}
    return steps


def canonical_state(slots, fmap):
    v = VState(slots)
    order = []
    for cell in sorted(fmap):
        k = fmap[cell][0]
        if k not in order:
            order.append(k)
    assert len(order) <= len(slots), (len(order), len(slots))
    pos = {k: slots[i] for i, k in enumerate(order)}
    for k, s in pos.items():
        v.content[s] = k
    v.nt = {cell: (pos[k], f) for cell, (k, f) in fmap.items()}
    return v
