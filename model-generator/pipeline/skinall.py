r"""Skin every generated model with its own sprite and turn the sprite's look into relief (user 9 Nov: "skin them all", "with some
detail, try and convert the look also into 3D").
usage: python -I skinall.py IN_SHAPES OUT_SHAPES DATA [name ...]
  IN_SHAPES  = kitchen.py's output (its atlas already holds the kitchen textures)
  OUT_SHAPES = new folder: every model that passed, plus the atlas
  DATA       = stock game data (sprite banks)
  names      = only these models (file names without .obj); default all generated ones
Per model:
  1. Find its sprite (file name, then its 'g' name). Game sprites are three-quarter (upper part = top, lower part = front) or plain top
     views; try several split lines. Each face is re-textured by where it is: up faces from the top part at (x, z), front faces from the
     front part at (x, y), sides from the front's edge stretched by (z, y).
  2. Keep the split whose colours best match the model's own colours (the swatches it was painted with came from the same sprite), and
     only skin the model if they match well - a sprite that does not fit the shape (a TV on a stand, a whole shower cubicle) is skipped.
  3. Relief: over the model's up faces lay a low height field from the top part of the sprite - bright areas (pillows, raised trims)
     stand up, mid tones sit at a base level, dark outlines become grooves - merged into runs, with walls where heights change.
Skipped: models kitchen.py built, people and animals, doors, the serving table (its food tail must stay last), very heavy models.
Textures are capped at 96 px a side to keep atlas space. Writes skinall_report.txt in OUT_SHAPES."""
import sys, os, math, re
from PIL import Image, ImageStat
import numpy as np

IN, OUT, DATA = sys.argv[1], sys.argv[2], sys.argv[3]
ONLY = set(n.lower() for n in sys.argv[4:])
os.makedirs(OUT, exist_ok=True)
atlas = Image.open(os.path.join(IN, 'objects-maps.png')).convert('RGB')
if atlas.size[0] < 4096:      # room for every model's textures: double the atlas, every texel copied 2 x 2 (uvs are 0..1: nothing moves)
    atlas = atlas.resize((atlas.size[0] * 2, atlas.size[1] * 2), Image.NEAREST)
AW, AH = atlas.size
BLK = 8
gw, gh = AW // BLK, AH // BLK
A0 = np.asarray(atlas).astype(np.float32)      # the atlas as it was: the models' own colours
blk_max = A0.sum(axis=2).reshape(gh, BLK, gw, BLK).max(axis=(1, 3))
free = blk_max <= 24

def uv_blocks(folder):
    out = set()
    for f in os.listdir(folder):
        if not f.lower().endswith('.obj'): continue
        T = []
        for l in open(os.path.join(folder, f), encoding='latin1'):
            q = l.split()
            if not q: continue
            if q[0] == 'vt': T.append((float(q[1]), float(q[2])))
            elif q[0] == 'f':
                uvs = [T[int(a.split('/')[1]) - 1] for a in q[1:] if len(a.split('/')) > 1 and a.split('/')[1]]
                if not uvs: continue
                xs = [u * gw for u, v in uvs]; ys = [(1.0 - v) * gh for u, v in uvs]
                for by in range(int(math.floor(min(ys))), int(math.floor(max(ys))) + 1):
                    for bx in range(int(math.floor(min(xs))), int(math.floor(max(xs))) + 1): out.add((by % gh, bx % gw))
    return out
for by, bx in uv_blocks(IN): free[by, bx] = False
used = ~free
grown = used.copy()
for dy in (-1, 0, 1):
    for dx in (-1, 0, 1): grown |= np.roll(np.roll(used, dy, 0), dx, 1)
free = ~grown
print('free atlas: %d of %d blocks' % (int(free.sum()), gw * gh))

def place(wb, hb):
    for y in range(gh - hb):
        row = free[y:y + hb]
        for x in range(gw - wb):
            if row[:, x:x + wb].all():
                free[max(0, y - 1):y + hb + 1, max(0, x - 1):x + wb + 1] = False
                return x * BLK, y * BLK
    return None
def put(img):
    img = img.convert('RGB'); w, h = img.size
    p = place((w + BLK - 1) // BLK, (h + BLK - 1) // BLK)
    if p is None: return None
    x, y = p
    atlas.paste(img, (x, y))
    return ((x + 1) / AW, 1.0 - (y + h - 1) / AH, (x + w - 1) / AW, 1.0 - (y + 1) / AH)

# ---------------------------------------------------------------- sprites
banks = [('objects.spritebank', 'objects.png'), ('objects_d11.spritebank', 'objectsTexture_d11.png'), ('objects_d11_2.spritebank', 'objects_d11_2.png')]
SPR = {}
for sbf, imf in banks:
    p = os.path.join(DATA, sbf)
    if not os.path.exists(p): continue
    t = open(p, encoding='latin1').read(); im = Image.open(os.path.join(DATA, imf)).convert('RGBA')
    for m in re.finditer(r'Name (\S+)\s+x (\d+)\s+y (\d+)\s+w (\d+)\s+h (\d+)', t):
        n = m.group(1).lower(); x, y, w, h = map(int, m.groups()[1:])
        if n in SPR or (sbf == 'objects.spritebank' and (x >= 64 or y >= 64)): continue
        SPR[n] = im.crop((x * 32, y * 32, (x + w) * 32, (y + h) * 32))
def sprite(name, trim=3):
    s = SPR[name]; a = np.asarray(s)
    ys, xs = np.nonzero(a[:, :, 3] > 100)
    if len(xs) < 16: return None
    x0, y0, x1, y1 = xs.min() + trim, ys.min() + trim, xs.max() + 1 - trim, ys.max() + 1 - trim
    if x1 - x0 < 6 or y1 - y0 < 6: return None
    s = s.crop((x0, y0, x1, y1))
    bg = Image.new('RGBA', s.size, tuple(int(c) for c in ImageStat.Stat(s.convert('RGB')).mean) + (255,))
    bg.alpha_composite(s)
    return bg.convert('RGB')
def cap(img, mx=96):
    w, h = img.size; k = min(1.0, mx / max(w, h))
    return img if k >= 1.0 else img.resize((max(4, int(w * k)), max(4, int(h * k))), Image.BILINEAR)

ENTITY = re.compile(r'prisoner|guard|warden|doctor|cook|janitor|gardener|workman|foreman|psychologist|accountant|lawyer|chief|armed|'
                    r'dog|sniper|soldier|riot|fireman|paramedic|visitor|actor|avatar|officer|board|chaplain|spiritual|teacher|'
                    r'door|servingtable$|^servingtable|link|tree|shrub|bush|plant|crop|flower|grass|heads?\d*$', re.I)

def load(fn):
    L = open(fn, encoding='latin1').read().splitlines()
    V, T, N, F = [], [], [], []
    for l in L:
        q = l.split()
        if not q: continue
        if q[0] == 'v': V.append(tuple(map(float, q[1:4])))
        elif q[0] == 'vt': T.append((float(q[1]), float(q[2])))
        elif q[0] == 'vn': N.append(tuple(map(float, q[1:4])))
        elif q[0] == 'f':
            c = []
            for a in q[1:]:
                s = a.split('/')
                c.append((int(s[0]) - 1, int(s[1]) - 1 if len(s) > 1 and s[1] else -1, int(s[2]) - 1 if len(s) > 2 and s[2] else -1))
            F.append(c)
    head = [l for l in L if l.startswith('#') or l.startswith('g ') or l.startswith('o ') or l.startswith('mtllib') or l.startswith('usemtl')]
    return head, V, T, N, F

def atlas_rgb(u, v):
    x = min(AW - 1, int((u % 1.0) * AW)); y = min(AH - 1, int(((1.0 - v) % 1.0) * AH))
    return A0[y, x]

def tri_info(V, f):
    P = [V[c[0]] for c in f[:3]]
    ux, uy, uz = P[1][0] - P[0][0], P[1][1] - P[0][1], P[1][2] - P[0][2]
    wx, wy, wz = P[2][0] - P[0][0], P[2][1] - P[0][1], P[2][2] - P[0][2]
    n = (uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx); a = math.sqrt(sum(q * q for q in n))
    return P, ((n[0] / a, n[1] / a, n[2] / a) if a > 1e-9 else (0, 0, 0)), a * 0.5

def regions(img, split):
    W, H = img.size; k = max(1, min(H, int(round(H * split))))
    top = img.crop((0, 0, W, k))
    if split >= 0.999: return top, None, None
    front = img.crop((0, k, W, H))
    if front.height < 3: return top, None, None
    col = front.crop((1, 0, min(W, 6), front.height))
    side = col.resize((max(8, k), front.height), Image.BILINEAR)
    return top, front, side

def sample(arr, a, b):
    h, w = arr.shape[:2]
    return arr[min(h - 1, max(0, int(b * h))), min(w - 1, max(0, int(a * w)))]

def classify(n, has_front):
    if n[1] > 0.6: return 'top'
    if has_front and n[2] > 0.6: return 'front'
    if has_front and abs(n[0]) > 0.6: return 'side'
    return None

def coords(kind, p, n, B):
    X0, X1, Y0, Y1, Z0, Z1 = B
    fx = (p[0] - X0) / (X1 - X0 or 1); fy = (p[1] - Y0) / (Y1 - Y0 or 1); fz = (p[2] - Z0) / (Z1 - Z0 or 1)
    if kind == 'top': return fx, fz
    if kind == 'front': return fx, 1 - fy
    return (fz if n[0] < 0 else 1 - fz), 1 - fy

def upmap(F, info, cxs, czs, ny_min):
    """the highest up-facing surface over each grid point (x = cxs, z = czs); -1e9 where there is none"""
    Hm = np.full((len(czs), len(cxs)), -1e9)
    for f, (P, n, a) in zip(F, info):
        if n[1] <= ny_min: continue
        (ax, ay, az), (bx, by_, bz), (qx, qy, qz) = P
        i0, i1 = np.searchsorted(cxs, min(ax, bx, qx)), np.searchsorted(cxs, max(ax, bx, qx))
        j0, j1 = np.searchsorted(czs, min(az, bz, qz)), np.searchsorted(czs, max(az, bz, qz))
        if i1 <= i0 or j1 <= j0: continue
        X, Z = np.meshgrid(cxs[i0:i1], czs[j0:j1])
        d = (bz - qz) * (ax - qx) + (qx - bx) * (az - qz)
        if abs(d) < 1e-9: continue
        l1 = ((bz - qz) * (X - qx) + (qx - bx) * (Z - qz)) / d; l2 = ((qz - az) * (X - qx) + (ax - qx) * (Z - qz)) / d; l3 = 1 - l1 - l2
        inside = (l1 >= -1e-6) & (l2 >= -1e-6) & (l3 >= -1e-6)
        y = l1 * ay + l2 * by_ + l3 * qy
        sub = Hm[j0:j1, i0:i1]
        Hm[j0:j1, i0:i1] = np.where(inside & (y > sub), y, sub)
    return Hm

SPLITS = [1.0, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2]
STOCK_SHAPES = set(os.listdir(os.path.join(DATA, 'shapes'))) if os.path.isdir(os.path.join(DATA, 'shapes')) else set()
def edge_splits(img):
    """where the top meets the front: rows with a strong horizontal edge running across most of the width (second pass, user: the
    washing machine took a split by colour alone and smeared its drum over the top). Returns candidate fractions, strongest first."""
    a = np.asarray(img.convert('L')).astype(np.float32)
    H, W = a.shape
    if H < 12: return []
    d = np.abs(a[1:, :] - a[:-1, :])                      # change from one row to the next
    strong = (d > 18).mean(axis=1)                        # how much of the width changes there
    out = []
    for y in np.argsort(-strong):
        f = (y + 1) / H
        if strong[y] < 0.55: break
        if 0.2 <= f <= 0.85 and all(abs(f - o) > 0.06 for o in out): out.append(float(f))
        if len(out) >= 3: break
    return out
# looked wrong in the preview sheets: the sprite does not show the shape the model has
BLACKLIST = {'tvlarge', 'radio', 'd_showerpillar',
             # second pass (user: "take another look at objects"): messy fronts or blobs in the two-view sheets
             'd_advancedautogatherer', 'd_birthdaypresentclosed', 'd_d11ingredient01', 'd_flour', 'd_paperdragontail', 'd_solarwalllight',
             'pipevalve', 'pipevalve_lagged',
             # in game (user 9 Nov): the comfy bed (superiorbed) - the game's own bed model is used instead; coat stand and brown sofa looked bad
             'superiorbed', 'd_coatstand', 'd_doublebrownsofa', 'leatherchair', 'd_leatherchair', 'd_leathersofa'}
report = []
done = 0
names = sorted(f[:-4] for f in os.listdir(IN) if f.lower().endswith('.obj'))
for name in names:
    if ONLY and name.lower() not in ONLY: continue
    fn = os.path.join(IN, name + '.obj')
    first = open(fn, encoding='latin1').readline()
    if 'PAIso generated model' not in first or 'kitchen.py' in first: continue
    if ENTITY.search(name): report.append('%-28s skip (person / door / tree / serving table)' % name); continue
    if os.path.exists(os.path.join(DATA, 'shapes', name + '.obj')) or any(f.lower() == name.lower() + '.obj' for f in STOCK_SHAPES):
        report.append('%-28s skip (the game has its own model: user 9 Nov - vanilla models stay)' % name); continue
    head, V, T, N, F = load(fn)
    if len(F) > 6000 or not V or not T: report.append('%-28s skip (%d faces)' % (name, len(F))); continue
    g = next((l.split(None, 1)[1].strip().lower() for l in head if l.startswith('g ')), name.lower())
    sp = name.lower() if name.lower() in SPR else (g if g in SPR else None)
    if not sp: report.append('%-28s skip (no sprite)' % name); continue
    img = sprite(sp)
    if img is None: report.append('%-28s skip (empty sprite)' % name); continue
    xs, ys, zs = [v[0] for v in V], [v[1] for v in V], [v[2] for v in V]
    B = (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))
    info = [tri_info(V, f) for f in F]
    total = sum(a for _, _, a in info) or 1.0
    best = None
    es = edge_splits(img)
    cands = (es + [1.0]) if es else [1.0] + SPLITS[1:]      # a clear top / front edge in the art: only splits there (or a plain top view)
    for split in cands:
        top, front, side = regions(img, split)
        arrs = {'top': np.asarray(top).astype(np.float32), 'front': None if front is None else np.asarray(front).astype(np.float32),
                'side': None if side is None else np.asarray(side).astype(np.float32)}
        err = 0.0; cov = 0.0
        for f, (P, n, a) in zip(F, info):
            kind = classify(n, front is not None)
            if not kind or a <= 0: continue
            c = (sum(p[0] for p in P) / 3, sum(p[1] for p in P) / 3, sum(p[2] for p in P) / 3)
            pa, pb = coords(kind, c, n, B)
            new = sample(arrs[kind], pa, pb)
            if f[0][1] < 0: continue
            tu = sum(T[q[1]][0] for q in f[:3]) / 3; tv = sum(T[q[1]][1] for q in f[:3]) / 3
            old = atlas_rgb(tu, tv)
            err += a * float(np.abs(new - old).mean()); cov += a
        if cov < total * 0.35: continue
        e = err / cov
        if best is None or e < best[0]: best = (e, split, cov / total)
    if best is None: report.append('%-28s skip (too little of it faces up / front)' % name); continue
    if name.lower() in BLACKLIST: report.append('%-28s skip (blacklisted after preview)' % name); continue
    if best[0] > 75.0: report.append('%-28s skip (sprite does not match: colour error %.0f)' % (name, best[0])); continue
    e, split, cov = best
    top, front, side = regions(img, split)
    # the outline seen from above must match the sprite's: the model's up faces against the top part's opaque area
    X0_, X1_, _, _, Z0_, Z1_ = B
    fgx, fgz = 24, 24
    occ = upmap(F, info, X0_ + (np.arange(fgx) + 0.5) * (X1_ - X0_) / fgx, Z0_ + (np.arange(fgz) + 0.5) * (Z1_ - Z0_) / fgz, 0.3) > -1e8
    al = np.asarray(SPR[sp].getchannel('A').crop(SPR[sp].getbbox() or (0, 0, 1, 1)))
    k = max(1, int(round(al.shape[0] * split)))
    am = np.asarray(Image.fromarray(al[:k]).resize((fgx, fgz), Image.BOX)) > 100
    iou = float((occ & am).sum()) / max(1.0, float((occ | am).sum()))
    if iou < 0.45: report.append('%-28s skip (outline does not match the sprite: %.2f)' % (name, iou)); continue
    rects = {'top': put(cap(top))}
    if front is not None: rects['front'] = put(cap(front)); rects['side'] = put(cap(side, 48))
    if any(r is None for r in rects.values()): report.append('%-28s skip (atlas full)' % name); break
    def uv_at(kind, a, b):
        u0, v0, u1, v1 = rects[kind]
        a = min(1, max(0, a)); b = min(1, max(0, b))
        return (u0 + (u1 - u0) * a, v1 - (v1 - v0) * b)
    T2 = list(T); F2 = []
    for f, (P, n, a) in zip(F, info):
        kind = classify(n, front is not None)
        if not kind:
            F2.append(f); continue
        nf = []
        for (vi, ti, ni) in f:
            pa, pb = coords(kind, V[vi], n, B)
            T2.append(uv_at(kind, pa, pb)); nf.append((vi, len(T2) - 1, ni))
        F2.append(nf)
    # ---- relief over the up faces, from the top part of the sprite
    V2 = list(V); N2 = list(N)
    X0, X1, Y0, Y1, Z0, Z1 = B
    cell = 6.0
    gx, gz = max(1, int((X1 - X0) / cell)), max(1, int((Z1 - Z0) / cell))
    while gx * gz > 2500: cell *= 1.25; gx, gz = max(1, int((X1 - X0) / cell)), max(1, int((Z1 - Z0) / cell))
    cxs = X0 + (np.arange(gx) + 0.5) * (X1 - X0) / gx; czs = Z0 + (np.arange(gz) + 0.5) * (Z1 - Z0) / gz
    Hm = upmap(F, info, cxs, czs, 0.6)
    have = Hm > -1e8
    # relief only on the model's main flat top, kept one cell in from its edge: on rounded tops and outlines it made stair steps
    if have.any():
        hq = np.round(Hm[have] / 2.0)
        vals, cnts = np.unique(hq, return_counts=True)
        mode = vals[np.argmax(cnts)] * 2.0
        flat = have & (np.abs(Hm - mode) <= 1.5)
        inner = flat.copy()
        inner[1:] &= flat[:-1]; inner[:-1] &= flat[1:]; inner[:, 1:] &= flat[:, :-1]; inner[:, :-1] &= flat[:, 1:]
        inner[0, :] = inner[-1, :] = False; inner[:, 0] = inner[:, -1] = False
        have = inner if flat.sum() >= 0.35 * (gx * gz) else np.zeros_like(have)
    relief_tris = 0
    if have.sum() >= 6:
        ta = np.asarray(top.convert('L').resize((gx, gz), Image.BOX)).astype(np.float32) / 255.0
        med = float(np.median(ta[have])); hi = float(np.percentile(ta[have], 90))
        span = max(0.08, hi - med)
        lev = np.where(ta < 0.20, -1, np.where(ta > med + span * 0.75, 2, np.where(ta > med + span * 0.3, 1, 0)))
        Y = np.where(have, Hm + 1.6 + lev * 1.5, np.nan)
        up = len(N2); N2.append((0.0, 1.0, 0.0))
        nx_p = len(N2); N2.append((1.0, 0.0, 0.0)); nx_m = len(N2); N2.append((-1.0, 0.0, 0.0))
        nz_p = len(N2); N2.append((0.0, 0.0, 1.0)); nz_m = len(N2); N2.append((0.0, 0.0, -1.0))
        ex = (X1 - X0) / gx; ez = (Z1 - Z0) / gz
        def vert(x, y, z):
            V2.append((x, y, z)); return len(V2) - 1
        def tvert(x, z):
            T2.append(uv_at('top', (x - X0) / (X1 - X0 or 1), (z - Z0) / (Z1 - Z0 or 1))); return len(T2) - 1
        def quad(p, t, ni):
            vi = [vert(*q) for q in p]
            F2.append([(vi[0], t[0], ni), (vi[1], t[1], ni), (vi[2], t[2], ni)]); F2.append([(vi[0], t[0], ni), (vi[2], t[2], ni), (vi[3], t[3], ni)])
        for j in range(gz):
            za, zb = Z0 + j * ez, Z0 + (j + 1) * ez
            i = 0
            while i < gx:
                if not have[j, i]: i += 1; continue
                k = i
                while k + 1 < gx and have[j, k + 1] and abs(Y[j, k + 1] - Y[j, i]) < 1e-3: k += 1
                xa, xb = X0 + i * ex, X0 + (k + 1) * ex; y = float(Y[j, i])
                quad([(xa, y, zb), (xb, y, zb), (xb, y, za), (xa, y, za)], [tvert(xa, zb), tvert(xb, zb), tvert(xb, za), tvert(xa, za)], up)
                relief_tris += 2
                i = k + 1
        for j in range(gz):
            for i in range(gx):
                if not have[j, i]: continue
                y = float(Y[j, i]); x0_, x1_ = X0 + i * ex, X0 + (i + 1) * ex; z0_, z1_ = Z0 + j * ez, Z0 + (j + 1) * ez
                tc = tvert((x0_ + x1_) / 2, (z0_ + z1_) / 2)
                for (di, dj) in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    ii, jj = i + di, j + dj
                    yn = float(Y[jj, ii]) if 0 <= ii < gx and 0 <= jj < gz and have[jj, ii] else float(Hm[j, i])
                    if yn >= y - 1e-3: continue
                    if di == 1: quad([(x1_, yn, z1_), (x1_, yn, z0_), (x1_, y, z0_), (x1_, y, z1_)], [tc] * 4, nx_p)
                    elif di == -1: quad([(x0_, yn, z0_), (x0_, yn, z1_), (x0_, y, z1_), (x0_, y, z0_)], [tc] * 4, nx_m)
                    elif dj == 1: quad([(x0_, yn, z1_), (x1_, yn, z1_), (x1_, y, z1_), (x0_, y, z1_)], [tc] * 4, nz_p)
                    else: quad([(x1_, yn, z0_), (x0_, yn, z0_), (x0_, y, z0_), (x1_, y, z0_)], [tc] * 4, nz_m)
                    relief_tris += 2
    with open(os.path.join(OUT, name + '.obj'), 'w', encoding='latin1') as fo:
        fo.write('# PAIso generated model (sprite-skinned with relief, skinall.py)\n')
        for l in head:
            if not l.startswith('#'): fo.write(l + '\n')
        for v in V2: fo.write('v %.4f %.4f %.4f\n' % v)
        for t in T2: fo.write('vt %.5f %.5f 0.0000\n' % t)
        for n in N2: fo.write('vn %.4f %.4f %.4f\n' % n)
        for f in F2: fo.write('f ' + ' '.join('%d/%d/%d' % (a + 1, b + 1, c + 1) if c >= 0 else '%d/%d' % (a + 1, b + 1) for a, b, c in f) + '\n')
    done += 1
    report.append('%-28s SKINNED split %.2f colour error %.0f, %.0f%% of it textured, relief +%d triangles (was %d)' % (name, split, e, cov * 100, relief_tris, len(F)))

atlas.save(os.path.join(OUT, 'objects-maps.png'))
open(os.path.join(OUT, 'skinall_report.txt'), 'w').write('\n'.join(report) + '\n')
print('skinall: %d models skinned, %d skipped; free atlas left %d blocks' % (done, len(report) - done, int(free.sum())))
