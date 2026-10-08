"""Parse web geometry from the 32X port's webdata.h (level data originally from mwenge/tempest2k)."""
import re
import os
SRC = os.path.join(os.path.dirname(__file__), '..', 'third_party', 'webdata.h')
def load():
    txt = open(SRC).read()
    out = []
    for m in re.finditer(r'/\* level (\d+): (web\d+) \*/ \{ (\d+), (\d+), (\d+), \{([^}]*)\}, \{([^}]*)\}, \{([^}]*)\} \}', txt):
        lvl, name, lanes, start, closed = int(m[1]), m[2], int(m[3]), int(m[4]), int(m[5])
        verts = [int(v) for v in m[6].split(',')]
        ori = [int(v) for v in m[7].split(',')]
        ang = [int(v) for v in m[8].split(',')]
        nv = lanes + 1
        pts = [(verts[i*2], verts[i*2+1]) for i in range(nv)]
        if closed: pts[-1] = pts[0]
        out.append(dict(level=lvl, name=name, lanes=lanes, start=start, closed=closed, pts=pts, ori=ori[:lanes], ang=ang[:lanes]))
    assert len(out) == 48, len(out)
    return out
if __name__ == '__main__':
    W = load()
    names = sorted(set(w['name'] for w in W), key=lambda s:int(s[3:]))
    print(len(names), 'unique webs')
    for w in W[:16]:
        xs=[p[0] for p in w['pts']]; ys=[p[1] for p in w['pts']]
        print(w['level'], w['name'], w['lanes'], w['closed'], min(xs),max(xs),min(ys),max(ys))
