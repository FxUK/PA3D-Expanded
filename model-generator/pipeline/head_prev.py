"""Preview of the DLL's prisoner head overlay (same maths as the isoHeadGeo block in shaders.h), for checking without the game.
usage: python -I head_prev.py STOCK_SHAPES ATLAS_PNG HEADS_RGBA OUT.png [HEADS_H]   (HEADS_H: the heads.h made by heads.py, for the sprite-index table)"""
import sys, math, numpy as np
from PIL import Image
shapes, atlasp, headsp, outp = sys.argv[1:5]
atlas = np.asarray(Image.open(atlasp).convert("RGB"), np.float32) / 255.0; AH, AW = atlas.shape[:2]
_raw = open(headsp, 'rb').read(); SIDE = int(round((len(_raw) / 4) ** 0.5)); GRID = SIDE // 32
heads = np.frombuffer(_raw, np.uint8).reshape(SIDE, SIDE, 4).astype(np.float32) / 255.0
import re as _re
CELL = None
if len(sys.argv) > 5:
    _h = open(sys.argv[5]).read(); CELL = [int(v) for v in _re.search(r'kHeadCell\[\d+\] = \{([^}]*)\}', _h).group(1).split(',')]
GEO = {'prisoner': (-1, 88, 29, 23), 'prisoner2': (0, 87, 1, 22), 'prisoner3': (0, 89, 4, 23), 'prisoner4': (0.5, 88, -8.5, 23)}
def load(path):
    V, T, F = [], [], []
    for l in open(path, encoding="latin1"):
        p = l.split()
        if not p: continue
        if p[0] == "v": V.append(list(map(float, p[1:4])))
        elif p[0] == "vt": T.append(list(map(float, p[1:3])))
        elif p[0] == "f":
            idx = [(int(a.split("/")[0]) - 1, int(a.split("/")[1]) - 1) for a in p[1:]]
            for i in range(1, len(idx) - 1): F.append((idx[0], idx[i], idx[i + 1]))
    return np.array(V), np.array(T), F
def sstep(a, b, x): t = np.clip((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t)
def render(name, head, skin, yaw, size=200):
    V, T, F = load(shapes + "\\" + name + ".obj"); cx, cy, cz, r = GEO[name]; c = np.array([cx, cy, cz])
    y = math.radians(yaw); p = math.radians(20)
    Ry = np.array([[math.cos(y), 0, math.sin(y)], [0, 1, 0], [-math.sin(y), 0, math.cos(y)]]); Rx = np.array([[1, 0, 0], [0, math.cos(p), -math.sin(p)], [0, math.sin(p), math.cos(p)]])
    P = V @ Ry.T @ Rx.T; ctr = (P[:, :2].min(0) + P[:, :2].max(0)) / 2; span = float((P[:, :2].max(0) - P[:, :2].min(0)).max()) * 1.15
    S = (P[:, :2] - ctr) / span * size + size / 2; S[:, 1] = size - S[:, 1]
    img = np.full((size, size, 3), 0.19, np.float32); zb = np.full((size, size), -1e9)
    cell = CELL[head] if CELL else head; cu, cv, cs = (cell % GRID) / GRID, (cell // GRID) / GRID, 1 / GRID; skinC = np.array(skin) / 255.0
    for (a, ta), (b, tb), (cc, tc) in F:
        pa, pb, pc = S[a], S[b], S[cc]
        x0, x1 = int(max(0, min(pa[0], pb[0], pc[0]))), int(min(size - 1, max(pa[0], pb[0], pc[0])) + 1); y0, y1 = int(max(0, min(pa[1], pb[1], pc[1]))), int(min(size - 1, max(pa[1], pb[1], pc[1])) + 1)
        if x1 <= x0 or y1 <= y0: continue
        xs, ys = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        d = (pb[0] - pa[0]) * (pc[1] - pa[1]) - (pc[0] - pa[0]) * (pb[1] - pa[1])
        if abs(d) < 1e-9: continue
        w1 = ((xs - pa[0]) * (pc[1] - pa[1]) - (pc[0] - pa[0]) * (ys - pa[1])) / d; w2 = ((pb[0] - pa[0]) * (ys - pa[1]) - (xs - pa[0]) * (pb[1] - pa[1])) / d; w0 = 1 - w1 - w2
        m = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
        if not m.any(): continue
        z = w0 * P[a, 2] + w1 * P[b, 2] + w2 * P[cc, 2]
        uv = w0[..., None] * T[ta] + w1[..., None] * T[tb] + w2[..., None] * T[tc]
        mp = w0[..., None] * V[a] + w1[..., None] * V[b] + w2[..., None] * V[cc]      # model-space position (isoModelPos)
        tx = np.clip((uv[..., 0] * AW).astype(int), 0, AW - 1); ty = np.clip(((1 - uv[..., 1]) * AH).astype(int), 0, AH - 1); col = atlas[ty, tx].copy()
        dd = mp - c; inHead = (1 - sstep(r * 1.05, r * 1.25, np.hypot(dd[..., 0], dd[..., 2]))) * sstep(-r * 0.95, -r * 0.7, dd[..., 1])
        _dv = np.stack([dd[..., 0] / r, dd[..., 1] / r, dd[..., 2] / r], -1); _dv = _dv / np.maximum(np.linalg.norm(_dv, axis=-1, keepdims=True), 1e-6); hu0 = 0.5 + _dv[..., 0] * 0.47; hv0 = 0.5 - _dv[..., 1] * 0.47; hu = np.clip(hu0, 0.02, 0.98); hv = np.clip(hv0, 0.02, 0.98)
        hx = np.clip(((cu + hu * cs) * SIDE).astype(int), 0, SIDE - 1); hy = np.clip(((cv + hv * cs) * SIDE).astype(int), 0, SIDE - 1); hs = heads[hy, hx].copy(); hs[..., 3] = np.where((hs[..., :3].max(-1) < 0.2) & (np.hypot(hu0 - 0.5, hv0 - 0.5) > 0.34), 0.0, hs[..., 3])
        hmx = hs[..., :3].max(-1); hmn = hs[..., :3].min(-1); grey = (hmx - hmn) < 0.14; front = np.where(grey[..., None], skinC * sstep(0.02, 0.92, hmx)[..., None], hs[..., :3])
        fa = hs[..., 3] * sstep(-0.2, 0.2, _dv[..., 2]) * (1 - sstep(0.62, 0.9, _dv[..., 1]))
        back = np.broadcast_to(skinC, front.shape)
        headC = back * (1 - fa[..., None]) + front * fa[..., None]
        col = col * (1 - inHead[..., None]) + headC * inHead[..., None]
        n = np.cross(V[b] - V[a], V[cc] - V[a]); n = n / (np.linalg.norm(n) + 1e-9); L = np.array([0.4, 0.8, 0.45]); L /= np.linalg.norm(L); shade = 0.55 + 0.45 * max(0.0, float(n @ L))
        sub = zb[y0:y1, x0:x1]; upd = m & (z > sub); sub[upd] = z[upd]; img[y0:y1, x0:x1][upd] = (col * shade)[upd]
    return Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
combos = [(392, (255, 221, 183)), (198, (109, 62, 28)), (361, (203, 148, 108)), (512, (76, 41, 16))] if CELL else [(7, (255, 221, 183)), (22, (109, 62, 28)), (3, (203, 148, 108)), (30, (76, 41, 16))]      # Head7, FemaleHead8, Head19, OldMaleHead1
sheet = Image.new("RGB", (200 * 8, 200 * 4), (30, 30, 30))
for i, nm in enumerate(['prisoner', 'prisoner2', 'prisoner3', 'prisoner4']):
    for j, (hd, sk) in enumerate(combos):
        sheet.paste(render(nm, hd, sk, 0), ((j * 2) * 200, i * 200)); sheet.paste(render(nm, hd, sk, 160), ((j * 2 + 1) * 200, i * 200))
sheet.save(outp)
