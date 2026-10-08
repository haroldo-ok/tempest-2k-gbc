"""Render a web wireframe. Colours: 1 far/dim, 2 mid, 3 rim; +4 = lit."""
from geom import DEPTH
from raster import canvas, line


def draw(web, cam_z=0.0, rot=0.0, img=None, lit=None, top=8):
    if img is None:
        img = canvas()
    nv = web.lanes + 1

    def P(i, z):
        return web.proj(web.pts[i][0], web.pts[i][1], z, cam_z, rot)

    def clip(x, y):
        return y >= top

    def L(c, on):
        return c + 4 if on else c

    def edge_lit(i):
        return lit is not None and (
            i == lit or i == lit + 1 or (web.closed and lit == web.lanes - 1 and i == 0)
        )

    if DEPTH + 14.0 + cam_z > 0.3:
        for i in range(web.lanes):
            line(img, *P(i, DEPTH), *P(i + 1, DEPTH), L(1, i == lit), clip=clip)
    zm = DEPTH * 0.45
    for i in range(nv):
        if web.closed and i == web.lanes:
            continue
        if 14.0 + cam_z + zm > 0.3:
            line(img, *P(i, zm), *P(i, DEPTH), L(1, edge_lit(i)), clip=clip)
        zn = max(0.0, 0.4 - (14.0 + cam_z))
        if zn < zm:
            line(img, *P(i, zn), *P(i, zm), L(2, edge_lit(i)), clip=clip)
    if 14.0 + cam_z > 0.3:
        for i in range(web.lanes):
            if i != lit:
                line(img, *P(i, 0), *P(i + 1, 0), 3, fat=True, clip=clip)
        if lit is not None:
            line(img, *P(lit, 0), *P(lit + 1, 0), 7, fat=True, clip=clip)
    return img
