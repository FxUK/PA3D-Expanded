r"""Plain wooden back for a sprite-skinned shelf (user 9 Nov: library bookshelves' backs were the sprite smeared brown).
usage: python -I woodback.py SHAPES NAME [OUT]
The side (+z or -z) whose faces sample the atlas with less colour variety is the back: every face on it (and the relief boxes standing
on it, which are flattened onto it) takes one wood colour, a little darker than the frame."""
import sys, os
import numpy as np
from PIL import Image
SH, NAME = sys.argv[1], sys.argv[2]; OUT = sys.argv[3] if len(sys.argv) > 3 else SH
A = np.asarray(Image.open(os.path.join(SH, 'objects-maps.png')).convert('RGB')).astype(int); H, W = A.shape[:2]
L = open(os.path.join(SH, NAME + '.obj'), encoding='latin1').read().splitlines()
V, T, N = [], [], []
for l in L:
    q = l.split()
    if not q: continue
    if q[0] == 'v': V.append(tuple(map(float, q[1:4])))
    elif q[0] == 'vt': T.append(tuple(map(float, q[1:3])))
    elif q[0] == 'vn': N.append(tuple(map(float, q[1:4])))
def texel(t):
    u, v = T[t - 1]; x = min(W - 1, max(0, int(u * W))); y = min(H - 1, max(0, int((1 - v) * H))); return A[y, x]
zs = [v[2] for v in V]; z0, z1 = min(zs), max(zs)
side = {1: [], -1: []}
for i, l in enumerate(L):
    if not l.startswith('f '): continue
    c = [tuple(int(k) if k else 0 for k in a.split('/')) for a in l.split()[1:]]
    n = N[c[0][2] - 1]
    if abs(n[2]) > 0.9:
        s = 1 if n[2] > 0 else -1
        side[s].append((i, c))
var = {s: np.array([texel(c[0][1]) for _, c in side[s]]).std(axis=0).sum() if side[s] else 0 for s in side}
back = 1 if var[1] < var[-1] else -1
cols = np.array([texel(c[0][1]) for _, c in side[-back]])      # the front's darker wood: the frame colour, from its most common dark-brown texels
wood = np.array([128, 86, 48])      # a mid wood brown (the median of the front came out maroon: book spines)
d = ((A - wood) ** 2).sum(axis=2); y, x = np.unravel_index(np.argmin(d), d.shape)
T.append(((x + 0.5) / W, 1 - (y + 0.5) / H)); tw = len(T)
edge = z1 if back == 1 else z0
out, changed = [], 0
for i, l in enumerate(L):
    if l.startswith('vt ') or l.startswith('f '):
        pass
    out.append(l)
# rewrite: faces facing the back side, lying within 6 units of the back plane, take the wood texel
res = []
for l in L:
    if l.startswith('f '):
        c = [a.split('/') for a in l.split()[1:]]
        n = N[int(c[0][2]) - 1]
        zz = [V[int(a[0]) - 1][2] for a in c]
        if n[2] * back > 0.9 and all(abs(z - edge) < 8 for z in zz):
            l = 'f ' + ' '.join('%s/%d/%s' % (a[0], tw, a[2]) for a in c); changed += 1
    res.append(l)
# the back's relief (book bumps) is pressed flat onto the panel: vertices within 8 of the back go to 2 units inside it
plane = edge - 2 * back
for i, l in enumerate(res):
    if l.startswith('v '):
        q = l.split(); vx, vy, vz = map(float, q[1:4])
        if abs(vz - edge) < 8: res[i] = 'v %.4f %.4f %.4f' % (vx, vy, plane)
# the new vt goes after the last vt line
k = max(i for i, l in enumerate(res) if l.startswith('vt '))
res.insert(k + 1, 'vt %.5f %.5f 0.0000' % T[-1])
# a solid back panel just outside the back: the shelves are open-backed (the books' backs showed through), a real bookcase is boarded
xs = [v[0] for v in V]; ys = [v[1] for v in V]
x0, x1, y0, y1 = min(xs) + 1, max(xs) - 1, min(ys) + 1, max(ys) - 3
pz = edge + 0.5 * back
nv = sum(1 for l in res if l.startswith('v ')); nn = sum(1 for l in res if l.startswith('vn '))
kv = max(i for i, l in enumerate(res) if l.startswith('v ')); res[kv + 1:kv + 1] = ['v %.4f %.4f %.4f' % p for p in [(x0, y0, pz), (x1, y0, pz), (x1, y1, pz), (x0, y1, pz)]]
kn = max(i for i, l in enumerate(res) if l.startswith('vn ')); res.insert(kn + 1, 'vn 0.0000 0.0000 %.4f' % back)
a, b, c, d = nv + 1, nv + 2, nv + 3, nv + 4
quad = [(a, b, c), (a, c, d)] if back == 1 else [(a, c, b), (a, d, c)]
for tr in quad: res.append('f ' + ' '.join('%d/%d/%d' % (i, tw, nn + 1) for i in tr))
res[0] = res[0] + ' (woodback.py: plain wooden back)'
open(os.path.join(OUT, NAME + '.obj'), 'w', encoding='latin1').write('\n'.join(res) + '\n')
print('%s: back is %s z (colour spread %.0f vs %.0f), %d faces -> wood %s' % (NAME, '+' if back == 1 else '-', var[back], var[-back], changed, tuple(int(c) for c in A[y, x])))
