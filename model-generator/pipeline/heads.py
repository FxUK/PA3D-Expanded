r"""Prisoner head sprites for the DLL's 3D head overlay.
The game keeps each prisoner's HeadType as the head sprite's INDEX in objects.spritebank ("[i 392]" = Head7; female heads 173..202,
old heads 507..516, tattoo heads 780..781). This packs every head sprite into one atlas and writes the lookup table.
usage: python -I heads.py DATA OUT
  OUT/heads.rgba  512 x 512 RGBA, 16 x 16 cells of 32 px (raw rows, top-down)      -> PA3aiD\heads.rgba
  OUT/heads.h     kHeadCell[sprite index] = atlas cell (-1 none), kHeadHair[cell] = hair / beard colour 0xRRGGBB (0 = bald)   -> dll\src\heads.h
  OUT/heads.png   contact sheet (head, its hair colour)"""
import sys, os, re
from collections import Counter
from PIL import Image
DATA, OUT = sys.argv[1], sys.argv[2]; CELL = 32; GRID = 16; MAXIDX = 1024
t = open(os.path.join(DATA, 'objects.spritebank'), encoding='latin1').read()
# objects.spritebank is a composite of quads (64 x 64 cells each): (0,0) objects.png, (0,1) people.png, (1,0) special-entities.png
quads = {(0, 0): Image.open(os.path.join(DATA, 'objects.png')).convert('RGBA'), (0, 1): Image.open(os.path.join(DATA, 'people.png')).convert('RGBA'),
         (1, 0): Image.open(os.path.join(DATA, 'special-entities.png')).convert('RGBA')}
heads = []
for m in re.finditer(r'\[i (\d+)\]"\s+Name (\S+)\s+x (\d+)\s+y (\d+)\s+w (\d+)\s+h (\d+)', t):
    idx, name = int(m.group(1)), m.group(2); x, y, w, h = map(int, m.groups()[2:])
    if not re.match(r'(Female|OldFemale|OldMale|Tattoo)?Head(\d+|Gang\d*|Last)$', name) or w != 1 or h != 1 or idx >= MAXIDX: continue
    q = (x // 64, y // 64); src = quads.get(q)
    if src is None: continue
    lx, ly = x - q[0] * 64, y - q[1] * 64
    heads.append((idx, name, src.crop((lx * CELL, ly * CELL, (lx + 1) * CELL, (ly + 1) * CELL))))
heads.sort()
if len(heads) > GRID * GRID: sys.exit('too many heads for the atlas')
# atlas cell -> style (0 bald, 1 crest, 2 lots: curly / cornrows, 3 short): checked by eye on the contact sheet
FIX = {0: 3, 2: 3, 6: 3, 7: 3, 8: 3, 9: 3, 11: 3, 12: 3, 13: 3, 15: 3, 16: 3, 19: 3, 22: 3, 29: 3, 30: 3, 33: 0, 39: 3, 40: 2, 47: 3, 52: 0,
       53: 3, 61: 3, 67: 3, 68: 0, 72: 0, 73: 0, 76: 3, 77: 3, 79: 3, 80: 0, 81: 0, 82: 3, 84: 3, 85: 0, 86: 0}
def luma(c): return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
atlas = Image.new('RGBA', (GRID * CELL, GRID * CELL), (0, 0, 0, 0)); sheet = Image.new('RGB', (len(heads) * 40, 64), (40, 40, 40))
cell_of = [-1] * MAXIDX; hair = []; styles = []; beards = []
for cell, (idx, name, im) in enumerate(heads):
    P = im.load(); cnt = Counter()
    for yy in range(CELL):
        for xx in range(CELL):
            r, g, b, a = P[xx, yy]
            if a < 200 or luma((r, g, b)) < 22: continue      # transparent / outline
            cnt[(r // 8 * 8, g // 8 * 8, b // 8 * 8)] += 1
    hc = 0      # the face is a white mask the game tints with the prisoner's SkinColour; hair / beard = the commonest colour that is not white
    for c, n in cnt.most_common():
        if min(c) >= 200: continue
        if n >= 8: hc = (c[0] << 16) | (c[1] << 8) | c[2]
        break
    # head shape for the 3D head swap: 0 bald (+ beard), 1 crest / mohawk, 2 lots of hair (curly, long, cornrows), 3 short hair
    hr, hg, hb = (hc >> 16) & 255, (hc >> 8) & 255, hc & 255
    isH = lambda p: hc and p[3] >= 200 and abs(p[0] - hr) + abs(p[1] - hg) + abs(p[2] - hb) < 60
    tot = sum(1 for yy in range(CELL) for xx in range(CELL) if isH(P[xx, yy]))
    topC = sum(1 for yy in range(0, 14) for xx in range(12, 20) if isH(P[xx, yy])); topS = sum(1 for yy in range(0, 14) for xx in list(range(2, 10)) + list(range(22, 30)) if isH(P[xx, yy]))
    low = sum(1 for yy in range(20, CELL) for xx in range(CELL) if isH(P[xx, yy]))
    if not hc or tot < 12: style = 0
    elif topC >= 10 and topS < topC * 0.35: style = 1
    elif tot > 230: style = 2
    else: style = 3
    if style != 0 and tot - low < 15 and low >= 15: style = 0      # hair only at the bottom: a bald head with a beard
    # hand corrections from the contact sheet (8 Nov, user's prisoners): the colour count takes glasses / tattoos / beards for hair, and the
    # one 'lots of hair' 3D head is cornrows, so long straight hair looks closer as short hair in its own colour
    if name.startswith('Tattoo'): style = 0; low = 0      # face tattoo, bald: no hair, no beard
    if cell in FIX: style = FIX[cell]
    if style == 0 and not (low >= 15): hc = 0      # bald, no beard: nothing to colour (was glasses / tattoo grey)
    styles.append(style); beards.append(1 if low >= 15 else 0)
    cell_of[idx] = cell; hair.append(hc)
    atlas.paste(im, ((cell % GRID) * CELL, (cell // GRID) * CELL))
    sheet.paste(im.convert('RGB'), (cell * 40 + 4, 4)); sheet.paste(Image.new('RGB', (16, 16), ((hc >> 16) & 255, (hc >> 8) & 255, hc & 255)), (cell * 40 + 12, 42))
open(os.path.join(OUT, 'heads.rgba'), 'wb').write(atlas.tobytes()); atlas.save(os.path.join(OUT, 'heads_atlas.png')); sheet.save(os.path.join(OUT, 'heads.png'))
with open(os.path.join(OUT, 'heads.h'), 'w') as f:
    f.write('// generated by model-generator/pipeline/heads.py from objects.spritebank: a prisoner\'s HeadType is its head sprite\'s index there\n')
    f.write('static const int kHeadGrid = %d;      // PA3aiD\\heads.rgba: %d x %d cells of %d px\n' % (GRID, GRID, GRID, CELL))
    f.write('static const short kHeadCell[%d] = {' % MAXIDX + ','.join(str(c) for c in cell_of) + '};\n')
    f.write('static const uint32_t kHeadHair[%d] = {' % (GRID * GRID) + ','.join('0x%06x' % h for h in hair + [0] * (GRID * GRID - len(hair))) + '};      // per cell, 0 = bald\n')
    f.write('static const unsigned char kHeadStyle[%d] = {' % (GRID * GRID) + ','.join(str(s) for s in styles + [3] * (GRID * GRID - len(styles))) + '};      // 3D head: 0 bald, 1 crest, 2 lots of hair, 3 short\n')
    f.write('static const unsigned char kHeadBeard[%d] = {' % (GRID * GRID) + ','.join(str(b) for b in beards + [0] * (GRID * GRID - len(beards))) + '};\n')
import collections
print('heads:', len(heads), 'styles', dict(collections.Counter(styles)), 'beards', sum(beards))
print(' '.join('%s:%d' % (n, s) for (i, n, _), s in zip(heads, styles)))
