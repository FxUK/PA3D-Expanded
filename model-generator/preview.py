#!/usr/bin/env python3
"""Tiny software rasteriser: renders OBJ models with the atlas, iso view, flat lighting."""
import sys, math, numpy as np
from PIL import Image

def load(path):
    V, T, F = [], [], []
    for l in open(path, encoding='latin1'):
        p = l.split()
        if not p: continue
        if p[0] == 'v': V.append(list(map(float, p[1:4])))
        elif p[0] == 'vt': T.append(list(map(float, p[1:3])))
        elif p[0] == 'f':
            idx = [(int(a.split('/')[0]) - 1, int(a.split('/')[1]) - 1 if len(a.split('/')) > 1 and a.split('/')[1] else 0) for a in p[1:]]
            for i in range(1, len(idx) - 1): F.append((idx[0], idx[i], idx[i + 1]))
    return np.array(V), np.array(T) if T else np.zeros((1, 2)), F

def render(path, atlas, size=160, yaw=45, pitch=35.264):
    V, T, F = load(path)
    y, p = math.radians(yaw), math.radians(pitch)
    # world: x right, y up, z toward viewer
    Ry = np.array([[math.cos(y), 0, math.sin(y)], [0, 1, 0], [-math.sin(y), 0, math.cos(y)]])
    Rx = np.array([[1, 0, 0], [0, math.cos(p), -math.sin(p)], [0, math.sin(p), math.cos(p)]])
    P = V @ Ry.T @ Rx.T
    span = max(np.ptp(P[:, 0]), np.ptp(P[:, 1]), 1e-3) * 1.1
    cx, cy = (P[:, 0].max() + P[:, 0].min()) / 2, (P[:, 1].max() + P[:, 1].min()) / 2
    S = (P[:, :2] - [cx, cy]) / span * size + size / 2
    S[:, 1] = size - S[:, 1]
    img = np.full((size, size, 3), 60, np.float32); zb = np.full((size, size), -1e9)
    A = np.asarray(atlas, np.float32); AH, AW = A.shape[:2]
    L = np.array([0.4, 0.8, 0.45]); L /= np.linalg.norm(L)
    for (a, ta), (b, tb), (c, tc) in F:
        pa, pb, pc = S[a], S[b], S[c]
        n = np.cross(V[b] - V[a], V[c] - V[a]); nl = np.linalg.norm(n)
        if nl < 1e-9: continue
        n = n / nl
        shade = 0.55 + 0.45 * max(0.0, float(n @ L))
        x0, x1 = int(max(0, min(pa[0], pb[0], pc[0]))), int(min(size - 1, max(pa[0], pb[0], pc[0])) + 1)
        y0, y1 = int(max(0, min(pa[1], pb[1], pc[1]))), int(min(size - 1, max(pa[1], pb[1], pc[1])) + 1)
        if x1 <= x0 or y1 <= y0: continue
        xs, ys = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        d = (pb[0] - pa[0]) * (pc[1] - pa[1]) - (pc[0] - pa[0]) * (pb[1] - pa[1])
        if abs(d) < 1e-9: continue
        w1 = ((xs - pa[0]) * (pc[1] - pa[1]) - (pc[0] - pa[0]) * (ys - pa[1])) / d
        w2 = ((pb[0] - pa[0]) * (ys - pa[1]) - (xs - pa[0]) * (pb[1] - pa[1])) / d
        w0 = 1 - w1 - w2
        m = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
        if not m.any(): continue
        z = w0 * P[a, 2] + w1 * P[b, 2] + w2 * P[c, 2]
        uv = w0[..., None] * T[ta] + w1[..., None] * T[tb] + w2[..., None] * T[tc]
        tx = np.clip((uv[..., 0] * AW).astype(int), 0, AW - 1); ty = np.clip(((1 - uv[..., 1]) * AH).astype(int), 0, AH - 1)
        col = A[ty, tx] * shade
        sub = zb[y0:y1, x0:x1]; upd = m & (z > sub)
        sub[upd] = z[upd]; img[y0:y1, x0:x1][upd] = col[upd]
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))

if __name__ == '__main__':
    atlas = Image.open(sys.argv[1]).convert('RGB')
    out = sys.argv[2]; files = sys.argv[3:]
    cols = 6; size = 170
    sheet = Image.new('RGB', (cols * size, ((len(files) + cols - 1) // cols) * size), (30, 30, 30))
    for i, f in enumerate(files):
        sheet.paste(render(f, atlas, size), ((i % cols) * size, (i // cols) * size))
    sheet.save(out)
