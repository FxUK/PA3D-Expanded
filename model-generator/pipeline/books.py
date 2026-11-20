r"""Stacks of books (user 9 Nov: the library's book piles were flat cut-outs of the 2D sprite - the game has the sprites StackBooksSorted /
StackBooksUnsorted but no shape files for them). Writes stackbookssorted(.obj, b, c) and stackbooksunsorted(.obj, b, c): neat piles / a jumble,
the b and c files smaller (the game uses them for smaller stacks, as with stackwood / stackwoodb / stackwoodc).
usage: python -I books.py SHAPES [OUT]      (SHAPES has objects-maps.png). Colours are the nearest atlas texels, so no atlas change."""
import sys, os, random, math
import numpy as np
from PIL import Image
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
A = np.asarray(Image.open(os.path.join(SH, 'objects-maps.png')).convert('RGB')).astype(int); H, W = A.shape[:2]
def find(c):
    d = ((A - np.array(c)) ** 2).sum(axis=2); y, x = np.unravel_index(np.argmin(d), d.shape)
    return (x + 0.5) / W, 1 - (y + 0.5) / H
COVERS = [(150, 30, 30), (40, 100, 50), (40, 60, 140), (30, 120, 120), (120, 70, 30), (90, 30, 90), (180, 150, 40), (60, 60, 70)]
PAGES = (232, 226, 205)
def build(name, piles, jumble, seed):
    random.seed(seed)
    V, T, N, F = [], [], [], []
    tex = {}
    def col(c):
        if c not in tex: T.append(find(c)); tex[c] = len(T)
        return tex[c]
    def book(cx, cy, cz, w, h, d, ang, cover):
        # a box turned by ang about y; the spine (-x side) and covers in colour, the other three edges pages
        ca, sa = math.cos(ang), math.sin(ang)
        def P(x, y, z): return (cx + x * ca - z * sa, cy + y, cz + x * sa + z * ca)
        def R(n): return (n[0] * ca - n[2] * sa, n[1], n[0] * sa + n[2] * ca)
        x0, x1, y0, y1, z0, z1 = -w / 2, w / 2, 0, h, -d / 2, d / 2
        tc, tp = col(cover), col(PAGES)
        sides = [([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], (0, 1, 0), tc), ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1), tp),
                 ([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1), tp), ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0), tc),
                 ([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1, 0, 0), tp)]
        for (p, n, t) in sides:
            b = len(V); V.extend(P(*q) for q in p); N.append(R(n)); ni = len(N)
            F.append([(b + 1, t, ni), (b + 2, t, ni), (b + 3, t, ni)]); F.append([(b + 1, t, ni), (b + 3, t, ni), (b + 4, t, ni)])
        # cover overhang line: a thin coloured band round the page edges near top and bottom is too fine at this size - skipped
    for (px, pz, n) in piles:
        y = 0.0
        for i in range(n):
            w, d, h = random.uniform(22, 30), random.uniform(16, 21), random.uniform(3.5, 6.0)
            ang = random.uniform(-0.6, 0.6) if jumble else random.uniform(-0.06, 0.06)
            ox, oz = (random.uniform(-5, 5), random.uniform(-4, 4)) if jumble else (random.uniform(-1, 1), random.uniform(-1, 1))
            book(px + ox, y, pz + oz, w, h, d, ang + (math.pi / 2 if (jumble and random.random() < 0.3) else 0), random.choice(COVERS))
            y += h
    with open(os.path.join(OUT, name + '.obj'), 'w') as fo:
        fo.write('# PAIso generated model: %s (books.py: %s)\n\ng %s\n' % (name, 'a jumble of books' if jumble else 'neat piles of books', name))
        for v in V: fo.write('v %.4f %.4f %.4f\n' % v)
        for t in T: fo.write('vt %.5f %.5f 0.0000\n' % t)
        for n in N: fo.write('vn %.4f %.4f %.4f\n' % n)
        for f in F: fo.write('f ' + ' '.join('%d/%d/%d' % c for c in f) + '\n')
    print('%s: %d books, %d triangles' % (name, sum(p[2] for p in piles), len(F)))
# a full stack fills about two thirds of a tile: four piles; b = three lower piles; c = one or two
build('stackbookssorted', [(-17, -12, 7), (17, -12, 6), (-17, 12, 6), (17, 12, 7)], False, 1)
build('stackbookssortedb', [(-17, -12, 4), (17, -12, 5), (0, 12, 4)], False, 2)
build('stackbookssortedc', [(-10, 0, 3), (14, 2, 2)], False, 3)
build('stackbooksunsorted', [(-15, -10, 6), (16, -11, 5), (-14, 13, 5), (15, 12, 6)], True, 4)
build('stackbooksunsortedb', [(-15, -10, 4), (16, -6, 3), (0, 13, 4)], True, 5)
build('stackbooksunsortedc', [(-8, 0, 3), (12, 4, 2)], True, 6)
