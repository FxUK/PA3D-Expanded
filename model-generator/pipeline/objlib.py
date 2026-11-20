r"""Shared builder for the hand-made models (ring, chest, chesstable, sortingtable, ovaltable, books, mail): colours are the nearest texels of
the installed atlas (objects-maps.png), so no atlas change; boxes, cylinders and elliptic rings; one OBJ writer. (1.9.122 refactor: each
script carried its own copy of all of this.)"""
import os, math
import numpy as np
from PIL import Image


class Model:
    def __init__(self, shapes):
        self.A = np.asarray(Image.open(os.path.join(shapes, 'objects-maps.png')).convert('RGB')).astype(int)
        self.H, self.W = self.A.shape[:2]
        self.V, self.T, self.N, self.F = [], [], [], []
        self.tex = {}

    def find(self, c):
        d = ((self.A - np.array(c)) ** 2).sum(axis=2)
        y, x = np.unravel_index(np.argmin(d), d.shape)
        return (x + 0.5) / self.W, 1 - (y + 0.5) / self.H

    def col(self, c):
        """the vt index of the atlas texel nearest colour c (one per colour)"""
        c = tuple(c)
        if c not in self.tex:
            self.T.append(self.find(c))
            self.tex[c] = len(self.T)
        return self.tex[c]

    def quad(self, pts, n, t):
        b = len(self.V)
        self.V.extend(pts)
        self.N.append(n)
        ni = len(self.N)
        self.F.append([(b + 1, t, ni), (b + 2, t, ni), (b + 3, t, ni)])
        self.F.append([(b + 1, t, ni), (b + 3, t, ni), (b + 4, t, ni)])

    def box(self, x0, x1, y0, y1, z0, z1, c, bottom=False, top_only=False, ang=0.0, at=(0.0, 0.0, 0.0)):
        """an axis box (optionally turned by ang about y around 'at'); c = colour tuple or a vt index"""
        t = c if isinstance(c, int) else self.col(c)
        ca, sa = math.cos(ang), math.sin(ang)
        P = lambda x, y, z: (at[0] + x * ca - z * sa, at[1] + y, at[2] + x * sa + z * ca)
        R = lambda n: (n[0] * ca - n[2] * sa, n[1], n[0] * sa + n[2] * ca)
        sides = [([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], (0, 1, 0))]
        if not top_only:
            sides += [([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)), ([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1)),
                      ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0)), ([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1, 0, 0))]
            if bottom: sides.append(([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0)))
        for (p, n) in sides:
            self.quad([P(*q) for q in p], R(n), t)

    def ring(self, rx, rz, y0, y1, c, seg=28, cap_top=True, cap_bottom=False, cx=0.0, cz=0.0):
        """an elliptic (or round) upright cylinder with optional caps"""
        t = c if isinstance(c, int) else self.col(c)
        for i in range(seg):
            a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            p0 = (cx + rx * math.cos(a0), cz + rz * math.sin(a0)); p1 = (cx + rx * math.cos(a1), cz + rz * math.sin(a1))
            b = len(self.V); self.V.extend([(p0[0], y0, p0[1]), (p1[0], y0, p1[1]), (p1[0], y1, p1[1]), (p0[0], y1, p0[1])])
            am = (a0 + a1) / 2; nx, nz = math.cos(am) / rx, math.sin(am) / rz; l = math.hypot(nx, nz)
            self.N.append((nx / l, 0, nz / l)); ni = len(self.N)
            self.F.append([(b + 1, t, ni), (b + 3, t, ni), (b + 2, t, ni)]); self.F.append([(b + 1, t, ni), (b + 4, t, ni), (b + 3, t, ni)])
            for (cap, y, n) in ((cap_top, y1, (0, 1, 0)), (cap_bottom, y0, (0, -1, 0))):
                if not cap: continue
                b = len(self.V); self.V.extend([(cx, y, cz), (p0[0], y, p0[1]), (p1[0], y, p1[1])]); self.N.append(n); ni = len(self.N)
                self.F.append([(b + 1, t, ni), (b + 3, t, ni), (b + 2, t, ni)] if n[1] > 0 else [(b + 1, t, ni), (b + 2, t, ni), (b + 3, t, ni)])

    def cyl(self, cx, cz, y0, y1, r, c, seg=10):
        self.ring(r, r, y0, y1, c, seg, True, False, cx, cz)

    def write(self, path, name, note):
        with open(path, 'w') as fo:
            fo.write('# PAIso generated model: %s (%s)\n\ng %s\n' % (name, note, name))
            for v in self.V: fo.write('v %.4f %.4f %.4f\n' % v)
            for t in self.T: fo.write('vt %.5f %.5f 0.0000\n' % t)
            for n in self.N: fo.write('vn %.4f %.4f %.4f\n' % n)
            for f in self.F: fo.write('f ' + ' '.join('%d/%d/%d' % c for c in f) + '\n')
        return len(self.F)
