r"""Warden statue (user 9 Nov: "tiny and rubbish, redo"): the game's own warden figure, in pale stone, on a plinth, pointing as in its sprite.
usage: python -I statue.py STOCK_SHAPES SHAPES      (STOCK_SHAPES has warden.obj; SHAPES = the installed set with objects-maps.png; writes
SHAPES\d_wardenstatue.obj). Every face takes one of two stone colours already in the atlas (the nearest texels to them), so no atlas change."""
import sys, os, math
import numpy as np
from PIL import Image
STOCK, SH = sys.argv[1], sys.argv[2]
A = np.asarray(Image.open(os.path.join(SH, 'objects-maps.png')).convert('RGB')).astype(int); H, W = A.shape[:2]
def find(c):
    d = ((A - np.array(c)) ** 2).sum(axis=2); y, x = np.unravel_index(np.argmin(d), d.shape)
    return (x + 0.5) / W, 1 - (y + 0.5) / H
STONE, STONE_DK = find((214, 208, 190)), find((150, 146, 132))
V, T, N, F = [], [], [], []
def add_box(x0, x1, y0, y1, z0, z1, uv):
    T.append(uv); t = len(T)
    for (p, n) in [([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], (0, 1, 0)), ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
                   ([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1)), ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0)),
                   ([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1, 0, 0))]:
        b = len(V); V.extend(p); N.append(n); ni = len(N)
        F.append([(b + 1, t, ni), (b + 2, t, ni), (b + 3, t, ni)]); F.append([(b + 1, t, ni), (b + 3, t, ni), (b + 4, t, ni)])
# plinth: a stepped base, a body with a darker inscription panel, a cornice
add_box(-44, 44, 0, 8, -40, 40, STONE_DK)
add_box(-36, 36, 8, 46, -32, 32, STONE)
add_box(-24, 24, 20, 34, 32, 33, STONE_DK)
add_box(-40, 40, 46, 52, -36, 36, STONE)
# the warden, scaled up a little, standing on the plinth; all its faces in stone (lit by the game: the shape carries the detail)
base = 52.0; k = 1.05
vb = len(V); T.append(STONE); ts = len(T)
src = open(os.path.join(STOCK, 'warden.obj'), encoding='latin1').read().splitlines()
SV, SN = [], []
for l in src:
    q = l.split()
    if not q: continue
    if q[0] == 'v': SV.append((float(q[1]) * k, float(q[2]) * k + base, float(q[3]) * k))
    elif q[0] == 'vn': SN.append(tuple(map(float, q[1:4])))
nb = len(N)
V.extend(SV); N.extend(SN)
for l in src:
    q = l.split()
    if q and q[0] == 'f':
        c = []
        for a in q[1:]:
            s = a.split('/')
            c.append((vb + int(s[0]), ts, nb + int(s[2]) if len(s) > 2 and s[2] else 1))
        for i in range(1, len(c) - 1): F.append([c[0], c[i], c[i + 1]])
# the raised arm pointing up and out to its right, as the sprite shows: upper arm, forearm, hand
top = max(v[1] for v in SV); sh_y = base + (top - base) * 0.62; sx = max(v[0] for v in SV) * 0.55      # starts inside the body: no gap at the shoulder
add_box(sx - 8, sx + 14, sh_y - 5, sh_y + 5, -6, 6, STONE)
add_box(sx + 10, sx + 30, sh_y + 3, sh_y + 11, -5, 5, STONE)
add_box(sx + 28, sx + 37, sh_y + 8, sh_y + 17, -6, 6, STONE)
with open(os.path.join(SH, 'd_wardenstatue.obj'), 'w') as fo:
    fo.write('# PAIso generated model: d_WardenStatue (statue.py: the warden figure in stone on a plinth)\n\ng d_WardenStatue\n')
    for v in V: fo.write('v %.4f %.4f %.4f\n' % v)
    for t in T: fo.write('vt %.5f %.5f 0.0000\n' % t)
    for n in N: fo.write('vn %.4f %.4f %.4f\n' % n)
    for f in F: fo.write('f ' + ' '.join('%d/%d/%d' % c for c in f) + '\n')
print('d_wardenstatue: %d vertices, %d triangles' % (len(V), len(F)))
