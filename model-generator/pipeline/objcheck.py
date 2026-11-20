import os, sys, math
d = sys.argv[1]; bad = []; big = []; tri_total = 0; sizes = []
for f in sorted(os.listdir(d)):
    if not f.endswith('.obj'): continue
    nv = nt = nn = 0; faces = 0; mn = [1e9] * 3; mx = [-1e9] * 3; err = None; tris = 0
    try:
        for l in open(os.path.join(d, f), encoding='latin1'):
            if l.startswith('v '):
                p = [float(x) for x in l.split()[1:4]]
                if any(math.isnan(x) or math.isinf(x) for x in p): err = 'nan vertex'; break
                nv += 1; mn = [min(a, b) for a, b in zip(mn, p)]; mx = [max(a, b) for a, b in zip(mx, p)]
            elif l.startswith('vt '): nt += 1
            elif l.startswith('vn '): nn += 1
            elif l.startswith('f '):
                faces += 1; parts = l.split()[1:]; tris += max(0, len(parts) - 2)
                for q in parts:
                    ix = q.split('/'); vi = int(ix[0]); ti = int(ix[1]) if len(ix) > 1 and ix[1] else 0
                    if vi > nv or vi == 0 or (ti and ti > nt): err = 'face index out of range'; break
                if err: break
    except Exception as e: err = str(e)
    if err or nv == 0 or faces == 0: bad.append((f, err or 'empty')); continue
    tri_total += tris; ext = [b - a for a, b in zip(mn, mx)]
    if max(ext) > 1200 or tris > 12000: big.append((f, tris, [round(x) for x in ext]))
print('bad:', len(bad)); [print('  ', b) for b in bad[:40]]
print('very large or heavy:', len(big)); [print('  ', b) for b in sorted(big, key=lambda x: -x[1])[:30]]
print('total triangles', tri_total)
