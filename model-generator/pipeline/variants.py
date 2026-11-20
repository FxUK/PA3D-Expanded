"""last pipeline step: every numbered sprite variant (workshopsaw2, treestump3, d_cropcarrotplant4 ...) that has no model of its own
gets its base object's finished model, so it is not left as a flat sprite. Crop and flower stages are scaled by growth stage."""
import os, re, sys
data, out = sys.argv[1], sys.argv[2]
sh_out = os.path.join(out, 'data', 'shapes'); sh_stock = os.path.join(data, 'shapes')
have = {}
for d in (sh_stock, sh_out):                      # generated models win over stock
    for f in os.listdir(d):
        if f.lower().endswith('.obj'): have[f[:-4].lower()] = os.path.join(d, f)
names = []
for f in os.listdir(data):
    if f.startswith('objects') and f.endswith('.spritebank'):
        for l in open(os.path.join(data, f), encoding='latin1'):
            m = re.search(r'Name\s+(\S+)', l)
            if m: names.append(m.group(1).lower())
skip = re.compile(r'rubble|strikefire|toolbar|icon|roadgate')
made = []
def scaled_copy(src, dst, f):
    lines = []
    for l in open(src, encoding='latin1'):
        if l.startswith('v '):
            p = l.split(); l = 'v %.4f %.4f %.4f\n' % (float(p[1]) * f, float(p[2]) * f, float(p[3]) * f)
        lines.append(l)
    open(dst, 'w', encoding='latin1').writelines(lines)
for n in names:
    m = re.match(r'^(.*?)(\d+)$', n)
    if not m or n in have or skip.search(n): continue
    base, k = m.group(1), int(m.group(2))
    src = None; sk = None
    for cand, ck in ((base, 0), (base + '1', 1), (base + '0', 0)):
        if cand in have and cand != n: src, sk = have[cand], ck; break
    if not src: continue
    f = 1.0
    if re.search(r'crop.*plant|flowerstage', base) and sk == 1 and k > 1: f = (0.45 + 0.55 * k / 4) / (0.45 + 0.55 / 4)      # growth stages get bigger
    dst = os.path.join(sh_out, n + '.obj'); scaled_copy(src, dst, f); have[n] = dst; made.append(n)
print('variant models:', len(made)); print(', '.join(made))
