r"""Bush (user 9 Nov: "bush is shabby"): a rounded mound of leafy clumps like the 2D sprite - olive greens, lighter on top, darker underneath.
usage: python -I bush.py SHAPES [OUT]      (SHAPES = the installed set with objects-maps.png; writes SHAPES\d_bush.obj)
Each face takes one of several greens already in the atlas (the nearest texels), so no atlas change. Smooth normals per clump: soft lumps, not facets."""
import sys, os, math, random
import numpy as np
from PIL import Image
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
A = np.asarray(Image.open(os.path.join(SH, 'objects-maps.png')).convert('RGB')).astype(int); H, W = A.shape[:2]
def find(c):
    d = ((A - np.array(c)) ** 2).sum(axis=2); y, x = np.unravel_index(np.argmin(d), d.shape)
    return (x + 0.5) / W, 1 - (y + 0.5) / H
GREENS = [find(c) for c in [(52, 62, 22), (74, 86, 30), (98, 110, 40), (124, 136, 52), (150, 162, 70), (176, 184, 92)]]
random.seed(7)
# an icosphere, subdivided once (80 faces)
t = (1 + 5 ** 0.5) / 2
IV = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
IV = [tuple(c / math.sqrt(sum(k * k for k in v)) for c in v) for v in IV]
IF = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8),
      (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
for _ in range(1):      # 80 faces a clump (a prison has hundreds of bushes)
    cache, NF = {}, []
    def mid(a, b):
        k = (min(a, b), max(a, b))
        if k not in cache:
            m = [(IV[a][i] + IV[b][i]) / 2 for i in range(3)]; l = math.sqrt(sum(c * c for c in m)); IV.append(tuple(c / l for c in m)); cache[k] = len(IV) - 1
        return cache[k]
    for a, b, c in IF:
        ab, bc, ca = mid(a, b), mid(b, c), mid(c, a); NF += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
    IF = NF
V, T, N, F = [], [], [], []
for g in GREENS: T.append(g)
def clump(cx, cy, cz, r, sy, lift):
    """one leafy lump: a sphere squashed to sy, its surface lumpy; faces shaded by height and a little noise"""
    b = len(V); nb = len(N)
    ph = [random.uniform(0, 6.28) for _ in range(3)]
    for (x, y, z) in IV:
        n = 1 + 0.10 * math.sin(5 * x + ph[0]) * math.sin(5 * y + ph[1]) + 0.07 * math.sin(7 * z + ph[2]) + random.uniform(-0.04, 0.04)
        V.append((cx + x * r * n, cy + y * r * sy * n, cz + z * r * n)); N.append((x, y, z))
    for (i, j, k) in IF:
        y = (IV[i][1] + IV[j][1] + IV[k][1]) / 3
        w = (y * 0.5 + 0.5) * 3.2 + lift + random.uniform(-0.9, 0.9)      # top lighter, underside darker, speckled like leaves
        ti = max(0, min(len(GREENS) - 1, int(round(w)))) + 1
        F.append([(b + i + 1, ti, nb + i + 1), (b + j + 1, ti, nb + j + 1), (b + k + 1, ti, nb + k + 1)])
# the mound: a big centre lump, a ring of lumps round it (lower), a few on top; about one tile across and two thirds of one tall
clump(0, 30, 0, 30, 0.85, 0.6)
for i in range(7):
    a = 6.2832 * i / 7 + random.uniform(-0.2, 0.2); rr = random.uniform(27, 33)
    clump(math.cos(a) * rr, 20 + random.uniform(-2, 3), math.sin(a) * rr, random.uniform(17, 21), 0.9, 0.0)
for i in range(4):
    a = 6.2832 * i / 4 + 0.6; rr = random.uniform(10, 16)
    clump(math.cos(a) * rr, 44 + random.uniform(0, 5), math.sin(a) * rr, random.uniform(12, 15), 0.85, 1.0)
for i in range(12):      # small sprigs poking out of the surface: the leafy edge the sprite has
    a = random.uniform(0, 6.2832); el = random.uniform(0.1, 1.1); rr = 40
    clump(math.cos(a) * math.cos(el) * rr, 22 + math.sin(el) * 30, math.sin(a) * math.cos(el) * rr, random.uniform(6, 9), 0.9, 0.8)
# sit on the floor
lo = min(v[1] for v in V); S = 0.85; V = [(x * S, (y - lo) * S, z * S) for (x, y, z) in V]      # ~0.85 tile across, like the sprite
with open(os.path.join(OUT, 'd_bush.obj'), 'w') as fo:
    fo.write('# PAIso generated model: d_Bush (bush.py: a mound of leafy clumps)\n\ng d_Bush\n')
    for v in V: fo.write('v %.4f %.4f %.4f\n' % v)
    for t in T: fo.write('vt %.5f %.5f 0.0000\n' % t)
    for n in N: fo.write('vn %.4f %.4f %.4f\n' % n)
    for f in F: fo.write('f ' + ' '.join('%d/%d/%d' % c for c in f) + '\n')
xs = [v[0] for v in V]; ys = [v[1] for v in V]
print('d_bush: %d vertices, %d triangles, x %.0f..%.0f, height %.0f' % (len(V), len(F), min(xs), max(xs), max(ys)))
