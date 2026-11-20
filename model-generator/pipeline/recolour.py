r"""Recolour (not retexture) a model: every face whose texel is green-dominant is pointed at an atlas texel of the target colour at the same
brightness (user 9 Nov: comfy bed blanket in the 2D sprite's blue). The shape and every other colour stay as they are.
usage: python -I recolour.py SHAPES NAME R G B OUT"""
import sys, os
import numpy as np
from PIL import Image
SH, NAME = sys.argv[1], sys.argv[2]; TGT = np.array([float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])]); OUT = sys.argv[6]
A = np.asarray(Image.open(os.path.join(SH, 'objects-maps.png')).convert('RGB')).astype(int); H, W = A.shape[:2]
L = open(os.path.join(SH, NAME + '.obj'), encoding='latin1').read().splitlines()
T = [tuple(map(float, l.split()[1:3])) for l in L if l.startswith('vt ')]
def texel(t):
    u, v = T[t - 1]; return A[min(H - 1, max(0, int((1 - v) * H))), min(W - 1, max(0, int(u * W)))]
lum = lambda c: 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]
tl = lum(TGT)
newvt, remap = [], {}
def target_for(t):
    if t in remap: return remap[t]
    c = texel(t)
    if not (c[1] >= c[0] - 6 and c[1] > c[2] + 12):      # green, olive or khaki
        remap[t] = None; return None
    want = np.clip(TGT * (lum(c) / max(1.0, tl)) * 1.0, 0, 255)      # same brightness as the green it replaces
    d = ((A - want) ** 2).sum(axis=2); y, x = np.unravel_index(np.argmin(d), d.shape)
    newvt.append(((x + 0.5) / W, 1 - (y + 0.5) / H)); remap[t] = len(T) + len(newvt)
    return remap[t]
out, changed = [], 0
for l in L:
    if l.startswith('f '):
        parts = [a.split('/') for a in l.split()[1:]]
        if len(parts[0]) > 1 and parts[0][1]:
            nt = target_for(int(parts[0][1]))
            if nt:
                l = 'f ' + ' '.join('%s/%d/%s' % (a[0], nt, a[2] if len(a) > 2 else '') for a in parts); changed += 1
    out.append(l)
k = max(i for i, l in enumerate(out) if l.startswith('vt '))
out[k + 1:k + 1] = ['vt %.5f %.5f 0.0000' % t for t in newvt]
if out and out[0].startswith('#'): out[0] += ' (recolour.py: green -> %d,%d,%d)' % tuple(TGT)
else: out.insert(0, '# recolour.py: green -> %d,%d,%d' % tuple(TGT))
open(os.path.join(OUT, NAME + '.obj'), 'w', encoding='latin1').write('\n'.join(out) + '\n')
print('%s: %d faces recoloured, %d green texels mapped' % (NAME, changed, len(newvt)))
