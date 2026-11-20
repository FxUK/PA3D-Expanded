r"""Double-sided shelf (user 9 Nov: the library bookshelf is double sided): the side with the clearer books (more colour variety) is kept and
mirrored onto the other side; faces that lie wholly on the other side are dropped; faces spanning both halves (top, ends) stay as they are.
usage: python -I mirrorback.py SHAPES_IN NAME OUT      (SHAPES_IN has NAME.obj and objects-maps.png)"""
import sys, os
import numpy as np
from PIL import Image
SH, NAME, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
A = np.asarray(Image.open(os.path.join(SH, 'objects-maps.png')).convert('RGB')).astype(int); H, W = A.shape[:2]
L = open(os.path.join(SH, NAME + '.obj'), encoding='latin1').read().splitlines()
V, T, N, Fs, head = [], [], [], [], []
for l in L:
    q = l.split()
    if not q: continue
    if q[0] == 'v': V.append(tuple(map(float, q[1:4])))
    elif q[0] == 'vt': T.append(tuple(map(float, q[1:3])))
    elif q[0] == 'vn': N.append(tuple(map(float, q[1:4])))
    elif q[0] == 'f': Fs.append([tuple(int(k) for k in a.split('/')) for a in q[1:]])
    elif q[0] in ('#', 'g'): head.append(l)
zs = [v[2] for v in V]; mid = (min(zs) + max(zs)) / 2
def texel(t):
    u, v = T[t - 1]; return A[min(H - 1, max(0, int((1 - v) * H))), min(W - 1, max(0, int(u * W)))]
spread = {}
for s in (-1, 1):
    c = [texel(f[0][1]) for f in Fs if N[f[0][2] - 1][2] * s > 0.9]
    spread[s] = np.array(c).std(axis=0).sum() if c else 0
keep = -1 if spread[-1] >= spread[1] else 1      # the side whose faces point this way is the good one
def side(f):
    z = [V[a[0] - 1][2] for a in f]
    if all((zz - mid) * keep >= -0.01 for zz in z): return 'good'
    if all((zz - mid) * keep <= 0.01 for zz in z): return 'other'
    return 'span'
outV, outN = list(V), list(N)
outF = []
for f in Fs:
    s = side(f)
    if s == 'other': continue
    outF.append(f)
    if s == 'good':
        z = [V[a[0] - 1][2] for a in f]
        if all(abs(zz - mid) < 0.01 for zz in z): continue      # lies on the middle plane: no copy
        nf = []
        for (vi, ti, ni) in f:
            x, y, zz = V[vi - 1]; outV.append((x, y, 2 * mid - zz)); n = N[ni - 1]; outN.append((n[0], n[1], -n[2]))
            nf.append((len(outV), ti, len(outN)))
        outF.append(nf[::-1])      # mirrored: reverse the winding
with open(os.path.join(OUT, NAME + '.obj'), 'w', encoding='latin1') as fo:
    fo.write((head[0] if head else '# PAIso generated model') + ' (mirrorback.py: double sided)\n')
    for h in head[1:]: fo.write(h + '\n')
    for v in outV: fo.write('v %.4f %.4f %.4f\n' % v)
    for t in T: fo.write('vt %.5f %.5f 0.0000\n' % t)
    for n in outN: fo.write('vn %.4f %.4f %.4f\n' % n)
    for f in outF: fo.write('f ' + ' '.join('%d/%d/%d' % a for a in f) + '\n')
print('%s: kept the %s z side (spread %.0f vs %.0f), mirrored about z = %.1f: %d faces' % (NAME, '+' if keep == 1 else '-', spread[keep], spread[-keep], mid, len(outF)))
