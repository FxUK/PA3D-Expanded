"""Do two model folders show the same thing? Same vertices, same faces, and every face samples the same colour from its own
objects-maps.png (texture coordinates may move when colour swatches are handed out in a different order).
usage: python -I samemodels.py SHAPES_A SHAPES_B"""
import sys, os
from PIL import Image
A, B = sys.argv[1], sys.argv[2]
atl = {d: Image.open(os.path.join(d, 'objects-maps.png')).convert('RGBA') for d in (A, B)}
def look(d, f):
    im = atl[d]; W, H = im.size; px = im.load(); V = []; T = []; F = []
    for l in open(os.path.join(d, f), encoding='latin1'):
        q = l.split()
        if not q: continue
        if q[0] == 'v': V.append(tuple(round(float(x), 3) for x in q[1:4]))
        elif q[0] == 'vt': u, v = float(q[1]), float(q[2]); T.append(px[min(W - 1, int(u * W)), min(H - 1, int((1 - v) * H))])
        elif q[0] == 'f':
            F.append(tuple((V[int(a.split('/')[0]) - 1], T[int(a.split('/')[1]) - 1] if len(a.split('/')) > 1 and a.split('/')[1] else None) for a in q[1:]))
    return F
fa = {f for f in os.listdir(A) if f.endswith('.obj')}; fb = {f for f in os.listdir(B) if f.endswith('.obj')}
bad = []; same_bytes = 0
for f in sorted(fa & fb):
    if open(os.path.join(A, f), 'rb').read() == open(os.path.join(B, f), 'rb').read(): same_bytes += 1; continue
    la, lb = look(A, f), look(B, f)
    if la == lb: continue
    if len(la) != len(lb) or any(len(x) != len(y) or any(p[0] != q[0] for p, q in zip(x, y)) for x, y in zip(la, lb)): bad.append(f + ' (shape)'); continue
    deltas = [max(abs(p[1][k] - q[1][k]) for k in range(3)) for x, y in zip(la, lb) for p, q in zip(x, y) if p[1] and q[1]]
    big = sum(1 for d in deltas if d > 24)
    if big: bad.append('%s (colour: %d of %d corners off by >24, worst %d)' % (f, big, len(deltas), max(deltas)))
print('models %d / %d | byte-identical %d | same shape and colours %d | DIFFERENT %d (shape %d, colour %d) | only in A %s | only in B %s' % (
    len(fa), len(fb), same_bytes, len(fa & fb) - same_bytes - len(bad), len(bad), sum('(shape)' in b for b in bad), sum('(colour' in b for b in bad),
    sorted(fa - fb)[:10], sorted(fb - fa)[:10]))
for b in bad: print('  ' + b)
