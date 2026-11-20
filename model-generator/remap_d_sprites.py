#!/usr/bin/env python3
"""Give models to objects whose sprites live in the game's second sprite banks (d_* sprites).

The game only keeps model slots for the first 871 sprites (objects.spritebank).  Objects such as
gates, floodlights, benches, gym equipment and wind turbines use d_* sprites from later banks, so they
never get a model.  This script points those objects at *unused* base-bank sprites of the same size
(loose data\\materials*.txt overrides) and renames their model files to the unused sprite's name.
In 3D mode the model is what is drawn, so the object appears with its model.

remap_d_sprites.py <extracted data dir> <generated shapes dir> <out dir>
"""
import os, re, sys, shutil

DATA, SHAPES, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(os.path.join(OUT, 'data', 'shapes'), exist_ok=True)

def read(p): return open(p, encoding='latin1').read()

# --- base bank: sprite list in file order (id = order), size in cells, region
bank = []
for m in re.finditer(r'Name (\S+)\s+x (\d+)\s+y (\d+)\s+w (\d+)\s+h (\d+)', read(os.path.join(DATA, 'objects.spritebank'))):
    n, x, y, w, h = m.group(1), *map(int, m.groups()[1:])
    bank.append((n, x, y, w, h))
print('base bank sprites:', len(bank))

# --- dlc banks: name -> (w,h)
dsize = {}
for f in ('objects_d11.spritebank', 'objects_d11_2.spritebank'):
    p = os.path.join(DATA, f)
    if os.path.exists(p):
        for m in re.finditer(r'Name (\S+)\s+x (\d+)\s+y (\d+)\s+w (\d+)\s+h (\d+)', read(p)):
            dsize[m.group(1).lower()] = (int(m.group(4)), int(m.group(5)))

# --- every word used in any data text file (a sprite named there might be referenced)
used = set()
for root, _, files in os.walk(DATA):
    for fn in files:
        if fn.lower().endswith(('.txt', '.lua', '.xml')) and os.path.getsize(os.path.join(root, fn)) < 20_000_000:
            used |= {w.lower() for w in re.findall(r'[A-Za-z0-9_]+', read(os.path.join(root, fn)))}
stock = {f[:-4].lower().replace('_', '') for f in os.listdir(os.path.join(DATA, 'shapes')) if f.endswith('.obj')}

# names the game code refers to directly (trees, icons, fire frames...) live in the exe as strings
EXE = os.environ.get('PA_EXE', r'E:\SteamLibrary\steamapps\common\Prison Architect\Prison Architect64.exe')
exe_lower = open(EXE, 'rb').read().lower() if os.path.exists(EXE) else b''
print('exe strings checked:', bool(exe_lower))
pool = {}   # (w,h) -> [unused sprite names]
for n, x, y, w, h in bank:
    if x >= 64 or y >= 64: continue                 # people / special-entity sheets
    if n.lower() in used or n.lower().replace('_', '') in stock: continue
    if exe_lower and n.lower().encode() in exe_lower: continue
    pool.setdefault((w, h), []).append(n)
print('unused base sprites by size:', {k: len(v) for k, v in sorted(pool.items())})

gen = {f[:-4].lower(): f for f in os.listdir(SHAPES) if f.startswith('d_') and f.endswith('.obj')}
import json
rep = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(SHAPES))), 'report.json')
priority = set()
if os.path.exists(rep):
    priority = {m[1].lower() for m in json.load(open(rep))['made'] if len(m) > 2 and m[2] == 'outdoor'}
print('priority (outdoor/yard) sprites:', len(priority))
assign = {}          # d_ sprite (lower) -> base sprite name
skipped = []
def patch(text, only=None):
    def blk(m):
        b = m.group(0)
        sp = re.search(r'^\s*Sprite\s+(d_\S+)', b, re.M | re.I)
        if not sp: return b
        d = sp.group(1).lower()
        if d not in gen: return b
        if only is not None and d not in only and d not in assign: return b
        if d not in assign:
            size = dsize.get(d)
            cand = pool.get(size, []) if size else []
            if not cand and only is not None and size:      # priority objects: take the closest-sized free sprite
                best = min((k for k, v in pool.items() if v), key=lambda k: abs(k[0] * k[1] - size[0] * size[1]), default=None)
                cand = pool.get(best, []) if best else []
            if not cand: skipped.append((d, size)); return b
            assign[d] = cand.pop(0)
        return re.sub(r'^(\s*Sprite\s+)' + re.escape(sp.group(1)) + r'\b', lambda mm: mm.group(1) + assign[d], b, flags=re.M | re.I)
    return re.sub(r'BEGIN Object.*?\nEND', blk, text, flags=re.S)

texts = {fn: read(os.path.join(DATA, fn)) for fn in ('materials.txt', 'materials_dlc.txt') if os.path.exists(os.path.join(DATA, fn))}
for fn in texts: texts[fn] = patch(texts[fn], only=priority)      # pass 1: outdoor / yard objects first
for fn in texts: texts[fn] = patch(texts[fn])                      # pass 2: everything else that still fits
for fn in texts:
    p = os.path.join(DATA, fn)
    t = read(p); t2 = texts[fn]
    if t2 != t:
        open(os.path.join(OUT, 'data', fn), 'w', encoding='latin1', newline='').write(t2)
        print('patched', fn)

for d, u in assign.items():
    shutil.copyfile(os.path.join(SHAPES, gen[d]), os.path.join(OUT, 'data', 'shapes', u.lower() + '.obj'))
print('remapped', len(assign), 'objects; no free sprite for', len(skipped), skipped[:12])
with open(os.path.join(OUT, 'remap.txt'), 'w') as f:
    for d, u in sorted(assign.items()): f.write('%s -> %s\n' % (d, u))
