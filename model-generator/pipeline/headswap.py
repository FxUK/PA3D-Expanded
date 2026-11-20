r"""Head swap: the four stock prisoner bodies each carry their own head (bald + beard, mohawk crest, cornrows, short hair). Write every
body with every head (16 models) so the DLL can draw each prisoner's body with a head that suits their 2D head.
usage: python -I headswap.py STOCK_SHAPES OUTDIR   ->  OUTDIR/body<V>_head<k>.obj   (V = the body's vertex count as the game draws it,
       k = 0..3 the head's donor: 0 prisoner (bald, beard), 1 prisoner2 (crest), 2 prisoner3 (cornrows), 3 prisoner4 (short hair))"""
import sys, os, math
from PIL import Image
SH, OUT = sys.argv[1], sys.argv[2]; os.makedirs(OUT, exist_ok=True)
# head centre (x, y, z) and radius in OBJ units, per stock body (measured from the models: see dll parts/07_draw.inc kHeadGeo)
BODIES = [('prisoner', (-1.0, 88.0, 29.0), 23.0), ('prisoner2', (0.0, 87.0, 1.0), 22.0), ('prisoner3', (0.0, 89.0, 4.0), 23.0), ('prisoner4', (0.5, 88.0, -8.5), 23.0)]
def load(name):
    V, T, N, F = [], [], [], []
    for l in open(os.path.join(SH, name + '.obj'), encoding='latin1'):
        q = l.split()
        if not q: continue
        if q[0] == 'v': V.append(tuple(map(float, q[1:4])))
        elif q[0] == 'vt': T.append(tuple(map(float, q[1:3])))
        elif q[0] == 'vn': N.append(tuple(map(float, q[1:4])))
        elif q[0] == 'f':
            corners = []
            for a in q[1:]:
                p = a.split('/'); corners.append((int(p[0]) - 1, int(p[1]) - 1 if len(p) > 1 and p[1] else -1, int(p[2]) - 1 if len(p) > 2 and p[2] else -1))
            for i in range(1, len(corners) - 1): F.append((corners[0], corners[i], corners[i + 1]))      # triangles
    return V, T, N, F
ATLAS = Image.open(os.path.join(SH, 'objects-maps.png')).convert('RGB'); AW, AH = ATLAS.size; APX = ATLAS.load()
UNIFORM = [(247, 148, 29), (255, 183, 85)]      # the uniform and its stripe: always body, never head
def texel(T, f):
    u, v = T[f[0][1]] if f[0][1] >= 0 else (0, 0); return APX[min(AW - 1, int((u % 1) * AW)), min(AH - 1, int(((1 - v) % 1) * AH))]
def is_head(V, f, c, r, T=None):
    if T is not None and any(sum(abs(a - b) for a, b in zip(texel(T, f), u)) < 30 for u in UNIFORM): return False
    cx = sum(V[a][0] for a, _, _ in f) / 3; cy = sum(V[a][1] for a, _, _ in f) / 3; cz = sum(V[a][2] for a, _, _ in f) / 3
    return math.dist((cx, cy, cz), c) < 1.45 * r and cy > c[1] - 0.8 * r
models = [load(n) for n, _, _ in BODIES]
for bi, (bname, bc, br) in enumerate(BODIES):
    BV, BT, BN, BF = models[bi]
    body = [f for f in BF if not is_head(BV, f, bc, br, BT)]
    for hi, (hname, hc, hr) in enumerate(BODIES):
        HV, HT, HN, HF = models[hi]
        head = [f for f in HF if is_head(HV, f, hc, hr, HT)]
        k = br / hr
        def mv(p): return (bc[0] + (p[0] - hc[0]) * k, bc[1] + (p[1] - hc[1]) * k, bc[2] + (p[2] - hc[2]) * k)
        out = ['# PA3aiD head swap: %s body + %s head' % (bname, hname), 'g body%d_head%d' % (len(BF) * 3, hi)]
        vv, tt, nn, ff = [], [], [], []
        def emit(src_V, src_T, src_N, faces, xf):
            for f in faces:
                idx = []
                for a, t, n in f:
                    vv.append(xf(src_V[a])); tt.append(src_T[t] if t >= 0 else (0.0, 0.0)); nn.append(src_N[n] if n >= 0 else (0.0, 1.0, 0.0))
                    idx.append(len(vv))
                ff.append(idx)
        emit(BV, BT, BN, body, lambda p: p); emit(HV, HT, HN, head, mv)
        out += ['v %.4f %.4f %.4f' % p for p in vv] + ['vt %.5f %.5f' % p for p in tt] + ['vn %.4f %.4f %.4f' % p for p in nn]
        out += ['f %d/%d/%d %d/%d/%d %d/%d/%d' % (f[0], f[0], f[0], f[1], f[1], f[1], f[2], f[2], f[2]) for f in ff]
        open(os.path.join(OUT, 'body%d_head%d.obj' % (len(BF) * 3, hi)), 'w').write('\n'.join(out) + '\n')
    print('%s: %d triangles, %d kept as body' % (bname, len(BF), len(body)))
print('written to', OUT)
