r"""Kitchen and canteen models, textured from the game's own 2D art (user 9 Nov: "models could be improved and textured better",
"better models with more detail", "extrapolate the texture"; kitchen and canteen first).
usage: python -I kitchen.py IN_SHAPES OUT_SHAPES DATA
  IN_SHAPES  = the shapes folder from BEFORE any kitchen.py run (PA3aiD\models_kitchen_base): its objects-maps.png and every model
  OUT_SHAPES = a new folder: gets the models below and the atlas with their textures painted in
  DATA       = the stock game data folder (PA3aiD\stock_data\full\data): the sprite banks
The game's object sprites are drawn three-quarter: the upper part is the object's top, the lower part its front. Each sprite is split
there; the top goes on the model's top, the front on its front, and the sprite's edge columns are stretched round the sides (the texture
is extrapolated from the 2D art). Relief detail (lips, handles, dials, pans, plinths) is then built on as geometry. Textures go into atlas
blocks that are black and not covered by any model's uv footprint (the rule make_models.py uses), with a one-block margin."""
import sys, os, math, random, re
from PIL import Image, ImageFilter, ImageStat
import numpy as np

IN, OUT, DATA = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(OUT, exist_ok=True)
atlas = Image.open(os.path.join(IN, 'objects-maps.png')).convert('RGB')
AW, AH = atlas.size
BLK = 8
gw, gh = AW // BLK, AH // BLK
A = np.asarray(atlas).astype(int)
blk_max = A.sum(axis=2).reshape(gh, BLK, gw, BLK).max(axis=(1, 3))
free = blk_max <= 24      # every texel (nearly) black

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

def place(wb, hb):
    for y in range(gh - hb):
        for x in range(gw - wb):
            if free[y:y + hb, x:x + wb].all():
                free[max(0, y - 1):y + hb + 1, max(0, x - 1):x + wb + 1] = False
                return x * BLK, y * BLK
    raise SystemExit('atlas full')

def put(img):
    """paint a texture into a free atlas spot; returns its uv rectangle (inset by a texel against mip bleeding)"""
    img = img.convert('RGB'); w, h = img.size
    x, y = place((w + BLK - 1) // BLK, (h + BLK - 1) // BLK)
    atlas.paste(img, (x, y))
    return ((x + 1) / AW, 1.0 - (y + h - 1) / AH, (x + w - 1) / AW, 1.0 - (y + 1) / AH)     # u0, v0 (bottom), u1, v1 (top)

swatches = {}
def sw(rgb):
    rgb = tuple(int(c) for c in rgb)
    if rgb in swatches: return swatches[rgb]
    u0, v0, u1, v1 = put(Image.new('RGB', (BLK, BLK), rgb))
    swatches[rgb] = ('s', ((u0 + u1) / 2, (v0 + v1) / 2))
    return swatches[rgb]
def tex(img): return ('t', put(img))

# ---------------------------------------------------------------- the game's sprites
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
    """the sprite without its black outline, on an opaque background (its own edge colours bled out)"""
    s = SPR[name.lower()]
    a = np.asarray(s)
    ys, xs = np.nonzero(a[:, :, 3] > 100)
    s = s.crop((xs.min() + trim, ys.min() + trim, xs.max() + 1 - trim, ys.max() + 1 - trim))
    bg = Image.new('RGBA', s.size, tuple(int(c) for c in ImageStat.Stat(s.convert('RGB')).mean) + (255,))
    bg.alpha_composite(s)
    return bg.convert('RGB')

def parts(name, split, trim=3):
    """top / front / side / back textures from a three-quarter sprite split at `split` (fraction of its height)"""
    s = sprite(name, trim); W, H = s.size; k = max(1, min(H - 1, int(round(H * split))))
    top = s.crop((0, 0, W, k)); front = s.crop((0, k, W, H))
    col = front.crop((2, 0, min(W, 8), front.height))                       # the front's left edge, stretched into the side
    side = col.resize((max(8, k), front.height), Image.BILINEAR)
    back = Image.new('RGB', (8, 8), tuple(int(c * 0.72) for c in ImageStat.Stat(front).mean))
    return tex(top), tex(front), tex(side), tex(back)

rnd = random.Random(7)
def noise_img(w, h, f):
    img = Image.new('RGB', (w, h)); p = img.load()
    for y in range(h):
        for x in range(w): p[x, y] = tuple(max(0, min(255, int(c))) for c in f(x, y))
    return img
streak = [rnd.uniform(-1, 1) for _ in range(64)]
def steel_f(base):
    def f(x, y):
        s = streak[y % 64] * 7 + rnd.uniform(-3, 3) + (1 - y / 63.0) * 10 - 5
        return (base[0] + s, base[1] + s, base[2] + s + 2)
    return f
TEX_STEEL = tex(noise_img(64, 64, steel_f((188, 192, 196))))
TEX_STEEL_DK = tex(noise_img(64, 64, steel_f((128, 133, 139))))
def frost(x, y):
    v = rnd.random(); k = 60 if v > 0.985 else (25 if v > 0.95 else 0)
    return (52 + k, 62 + k, 74 + k)
TEX_FROST = tex(noise_img(64, 64, frost))

# ---------------------------------------------------------------- mesh
class Mesh:
    def __init__(s): s.v = []; s.vt = []; s.vn = []; s.f = []
    def quad(s, p, n, t):
        bv = len(s.v); bt = len(s.vt); s.v += p; s.vt += t; s.vn.append(n); ni = len(s.vn)
        a, b, c, d = [bv + i + 1 for i in range(4)]; ta, tb, tc, td = [bt + i + 1 for i in range(4)]
        s.f.append(((a, ta, ni), (b, tb, ni), (c, tc, ni))); s.f.append(((a, ta, ni), (c, tc, ni), (d, td, ni)))
    def write(s, name, fname):
        with open(os.path.join(OUT, fname), 'w') as fo:
            fo.write('# PAIso generated model: %s (kitchen.py)\n\ng %s\n' % (name, name))
            for x, y, z in s.v: fo.write('v %.4f %.4f %.4f\n' % (x, y, z))
            for u, w in s.vt: fo.write('vt %.5f %.5f 0.0000\n' % (u, w))
            for x, y, z in s.vn: fo.write('vn %.4f %.4f %.4f\n' % (x, y, z))
            for tri in s.f: fo.write('f ' + ' '.join('%d/%d/%d' % c for c in tri) + '\n')

def tuvs(mat, flip=False):
    if mat[0] == 's': return [mat[1]] * 4
    u0, v0, u1, v1 = mat[1]
    return [(u1, v0), (u0, v0), (u0, v1), (u1, v1)] if flip else [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]

def box(m, X0, X1, Y0, Y1, Z0, Z1, mat, top=None, front=None, side=None, back=None, bottom=False):
    """a box; front = +z. Faces without their own material use mat."""
    top = top or mat; front = front or mat; side = side or mat; back = back or mat
    m.quad([(X0, Y1, Z1), (X1, Y1, Z1), (X1, Y1, Z0), (X0, Y1, Z0)], (0, 1, 0), tuvs(top))
    m.quad([(X0, Y0, Z1), (X1, Y0, Z1), (X1, Y1, Z1), (X0, Y1, Z1)], (0, 0, 1), tuvs(front))
    m.quad([(X1, Y0, Z0), (X0, Y0, Z0), (X0, Y1, Z0), (X1, Y1, Z0)], (0, 0, -1), tuvs(back))
    m.quad([(X0, Y0, Z0), (X0, Y0, Z1), (X0, Y1, Z1), (X0, Y1, Z0)], (-1, 0, 0), tuvs(side))
    m.quad([(X1, Y0, Z1), (X1, Y0, Z0), (X1, Y1, Z0), (X1, Y1, Z1)], (1, 0, 0), tuvs(side, flip=True))
    if bottom: m.quad([(X0, Y0, Z0), (X1, Y0, Z0), (X1, Y0, Z1), (X0, Y0, Z1)], (0, -1, 0), tuvs(mat))

def bounds(fname):
    xs, ys, zs = [], [], []
    for l in open(os.path.join(IN, fname), encoding='latin1'):
        if l.startswith('v '):
            x, y, z = map(float, l.split()[1:4]); xs.append(x); ys.append(y); zs.append(z)
    return min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)

plinth, black, rub = sw((44, 46, 50)), sw((30, 30, 34)), sw((70, 72, 76))
glass = sw((150, 200, 225))
written = []

# ---------------------------------------------------------------- fridge: chest freezer. Front from its sprite (steel panel with the
# control box), sides extrapolated from it; open frosted well with basket dividers (the food the DLL stands in it shows)
X0, X1, _, _, Z0, Z1 = bounds('fridge.obj')
X0 += 1.0; X1 -= 1.0
H = 90.0; rim0 = 5.0
fT, fF, fS, fB = parts('Fridge', 0.66)
m = Mesh()
box(m, X0 + 4, X1 - 4, 0, 6, Z0 + 4, Z1 - 4, plinth)
box(m, X0 + 1.5, X1 - 1.5, 6, H - 6, Z0 + 1.5, Z1 - 1.5, TEX_STEEL, front=fF, side=fS, back=fB, top=TEX_STEEL)
box(m, X0, X1, H - 6, H, Z0, Z0 + rim0, TEX_STEEL); box(m, X0, X1, H - 6, H, Z1 - rim0, Z1, TEX_STEEL)
box(m, X0, X0 + rim0, H - 6, H, Z0 + rim0, Z1 - rim0, TEX_STEEL); box(m, X1 - rim0, X1, H - 6, H, Z0 + rim0, Z1 - rim0, TEX_STEEL)
# (the cabinet's own top at H - 6 is the well floor seen through the rim: frost it)
box(m, X0 + rim0, X1 - rim0, H - 6, H - 5.6, Z0 + rim0, Z1 - rim0, TEX_FROST)
wd = X1 - X0
for k in (1, 2):
    sx = X0 + wd * k / 3.0
    box(m, sx - 1, sx + 1, H - 5.6, H - 1, Z0 + rim0, Z1 - rim0, TEX_STEEL_DK)                 # basket dividers
for zx in (X0 + 1.5, X1 - 1.5):
    for zz in (Z0 + 1.5, Z1 - 1.5): box(m, zx - 2, zx + 2, 6, H - 6.5, zz - 2, zz + 2, TEX_STEEL_DK)     # corner posts
box(m, X0 + 3, X1 - 3, H - 9, H - 7, Z1 - 1.5, Z1 + 0.5, TEX_STEEL_DK)                         # lid seal line
m.write('Fridge', 'fridge.obj'); written.append('fridge')

# lid: two sliding glass panes in a slim steel frame (see-through)
L = Mesh()
box(L, X0, X1, H, H + 2, Z0, Z0 + 3, TEX_STEEL_DK); box(L, X0, X1, H, H + 2, Z1 - 3, Z1, TEX_STEEL_DK)
box(L, X0, X0 + 3, H, H + 2, Z0, Z1, TEX_STEEL_DK); box(L, X1 - 3, X1, H, H + 2, Z0, Z1, TEX_STEEL_DK)
cx = (X0 + X1) / 2
box(L, cx - 1.5, cx + 1.5, H, H + 3, Z0, Z1, TEX_STEEL_DK)
for p0, p1 in ((X0 + 3, cx - 1.5), (cx + 1.5, X1 - 3)):
    a = p0 + (p1 - p0) * 0.15
    box(L, a, a + 2.5, H + 1.6, H + 1.9, Z0 + 6, Z1 - 6, glass)
    box(L, p1 - 9, p1 - 6, H + 2, H + 3.5, (Z0 + Z1) / 2 - 8, (Z0 + Z1) / 2 + 8, TEX_STEEL_DK)
L.write('FridgeLid', 'fridgelid.obj'); L.write('FridgeLid', 'fridge_lid.obj'); written.append('fridgelid')

# ---------------------------------------------------------------- canteen bench: the sprite's seat on top, its edge round the sides, trestles
BX0, BX1, _, _, BZ0, BZ1 = bounds('bench.obj')
bT, bF, bS, bB = parts('Bench', 0.80)
B = Mesh(); seatH = 44.0
box(B, BX0 + 2, BX1 - 2, seatH - 5, seatH, BZ0, BZ1, bF, top=bT, front=bF, side=bS, back=bF)
for a in (BX0 + 18, BX1 - 22):
    box(B, a, a + 4, 0, seatH - 5, BZ0 + 3, BZ1 - 3, TEX_STEEL_DK)
    box(B, a - 3, a + 7, 0, 2, BZ0 + 1, BZ1 - 1, black)
box(B, BX0 + 18, BX1 - 18, 12, 15, (BZ0 + BZ1) / 2 - 1.5, (BZ0 + BZ1) / 2 + 1.5, TEX_STEEL_DK)
B.write('Bench', 'bench.obj'); written.append('bench')

# ---------------------------------------------------------------- tall fridge (front +z, against a wall): glass door full of drinks
TX0, TX1, _, _, TZ0, TZ1 = bounds('d_tallfridge.obj')
TZ0 = TZ1 - 78.0      # user: too deep (the sprite's 1.5 tiles); the front stays where it was
tT, tF, tS, tB = parts('d_TallFridge', 0.22)
T = Mesh(); TH = 180.0
box(T, TX0 + 3, TX1 - 3, 0, 6, TZ0 + 3, TZ1 - 3, plinth)
box(T, TX0, TX1, 6, TH, TZ0, TZ1 - 3, TEX_STEEL, top=tT, front=TEX_STEEL_DK, side=TEX_STEEL, back=tB)
box(T, TX0 + 2, TX1 - 2, 9, TH - 22, TZ1 - 3, TZ1 - 1, TEX_STEEL_DK, front=tF)                 # the glass door, its drinks from the sprite
box(T, TX0 + 2, TX1 - 2, TH - 20, TH - 4, TZ1 - 3, TZ1 - 1, sw((220, 214, 196)))               # lit header panel
box(T, TX1 - 9, TX1 - 6, 50, 120, TZ1 - 1, TZ1 + 3, TEX_STEEL)                                 # bar handle
box(T, TX1 - 9, TX1 - 6, 50, 53, TZ1 - 1, TZ1 + 3, black); box(T, TX1 - 9, TX1 - 6, 117, 120, TZ1 - 1, TZ1 + 3, black)
for k in range(4): box(T, TX0 + 6, TX1 - 6, TH, TH + 1, TZ0 + 8 + k * 6, TZ0 + 10 + k * 6, black)   # top vent
T.write('d_TallFridge', 'd_tallfridge.obj'); written.append('d_tallfridge')

# ---------------------------------------------------------------- tray dispenser (DLC): steel trolley cabinet with a sprung tray stack
DX0, DX1, _, _, DZ0, DZ1 = bounds('d_traydispenser.obj')
D = Mesh(); DH = 76.0
tray, trayd = sw((208, 196, 160)), sw((178, 166, 130))
box(D, DX0 + 3, DX1 - 3, 8, 12, DZ0 + 3, DZ1 - 3, TEX_STEEL_DK)                                 # chassis
for wx in (DX0 + 6, DX1 - 12):
    for wz in (DZ0 + 6, DZ1 - 12): box(D, wx, wx + 6, 0, 8, wz, wz + 6, black)                # castors
box(D, DX0, DX1, 12, DH - 14, DZ0, DZ1, TEX_STEEL)
box(D, DX0, DX0 + 4, DH - 14, DH, DZ0, DZ1, TEX_STEEL); box(D, DX1 - 4, DX1, DH - 14, DH, DZ0, DZ1, TEX_STEEL)
box(D, DX0 + 4, DX1 - 4, DH - 14, DH, DZ0, DZ0 + 4, TEX_STEEL); box(D, DX0 + 4, DX1 - 4, DH - 14, DH, DZ1 - 4, DZ1, TEX_STEEL)
for k in range(5):
    y = DH - 14 + k * 2.4
    box(D, DX0 + 6, DX1 - 6, y, y + 1.6, DZ0 + 6, DZ1 - 6, tray if k % 2 else trayd)
box(D, DX0 + 9, DX1 - 9, DH - 1.6, DH - 1.2, DZ0 + 10, DZ1 - 10, sw((222, 212, 180)))
box(D, DX0 - 4, DX0, 30, DH + 34, DZ0 + 8, DZ0 + 12, TEX_STEEL_DK)                             # push handle, as in the sprite
box(D, DX0 - 4, DX0, DH + 30, DH + 34, DZ0 + 8, DZ1 - 8, TEX_STEEL_DK)
D.write('d_TrayDispenser', 'd_traydispenser.obj'); written.append('d_traydispenser')

# ---------------------------------------------------------------- serving counters (DLC): the sprite's pans on top, its slatted front
def serving_counter(fname, name, spr):
    X0, X1, _, _, Z0, Z1 = bounds(fname)
    sT, sF, sS, sB = parts(spr, 0.58)
    S = Mesh(); Y = 72.0
    box(S, X0 + 3, X1 - 3, 0, 6, Z0 + 3, Z1 - 3, plinth)
    box(S, X0, X1, 6, Y, Z0, Z1 - 4, TEX_STEEL, top=sT, front=sF, side=sS, back=sB)
    box(S, X0 + 4, X1 - 4, Y - 6, Y - 4, Z1 - 4, Z1 + 6, TEX_STEEL)                            # tray slide along the front
    for k in range(4): x = X0 + 10 + k * (X1 - X0 - 20) / 3.0; box(S, x - 1, x + 1, Y - 12, Y - 6, Z1 - 4, Z1 + 5, TEX_STEEL_DK)   # its brackets
    box(S, X0 - 0.5, X1 + 0.5, Y, Y + 1.5, Z0 - 0.5, Z0 + 3, TEX_STEEL_DK)                      # back upstand
    S.write(name, fname); written.append(name)
serving_counter('d_smallservingtable.obj', 'd_SmallServingTable', 'd_SmallServingTable')
if 'd_jungleservingtable' in SPR: serving_counter('d_jungleservingtable.obj', 'd_JungleServingTable', 'd_JungleServingTable')

# ---------------------------------------------------------------- bread oven (DLC): the sprite's three decks on the front, handles and dials
OX0, OX1, _, _, OZ0, OZ1 = bounds('d_breadoven.obj')
oT, oF, oS, oB = parts('d_BreadOven', 0.42)
O = Mesh(); OH = 90.0
box(O, OX0 + 4, OX1 - 4, 0, 8, OZ0 + 4, OZ1 - 4, plinth)
box(O, OX0, OX1, 8, OH, OZ0, OZ1 - 3, TEX_STEEL, top=oT, front=oF, side=oS, back=oB)
for k in range(3):                                                                             # a handle bar on each deck door
    y = 8 + (OH - 8) * (0.70 - k * 0.24)
    box(O, OX0 + 14, OX1 - 30, y, y + 2.5, OZ1 - 3, OZ1 + 2, TEX_STEEL_DK)
    box(O, OX1 - 22, OX1 - 14, y - 6, y + 2, OZ1 - 3, OZ1 - 1, sw((200, 60, 50)))             # its red control
box(O, OX0 + 20, OX0 + 34, OH, OH + 30, OZ0 + 20, OZ0 + 34, TEX_STEEL_DK)                      # flue
O.write('d_BreadOven', 'd_breadoven.obj'); written.append('d_breadoven')

# ---------------------------------------------------------------- display counter (DLC): its sprite's glass top and white front; a frame
CX0, CX1, _, _, CZ0, CZ1 = bounds('d_displaycounter.obj')
cT, cF, cS, cB = parts('d_DisplayCounter', 0.62)
C = Mesh(); CT = 104.0
box(C, CX0 + 3, CX1 - 3, 0, 5, CZ0 + 3, CZ1 - 3, plinth)
box(C, CX0, CX1, 5, CT - 2, CZ0, CZ1, TEX_STEEL, top=cT, front=cF, side=cS, back=cB)
box(C, CX0, CX1, CT - 2, CT, CZ0, CZ0 + 2, TEX_STEEL_DK); box(C, CX0, CX1, CT - 2, CT, CZ1 - 2, CZ1, TEX_STEEL_DK)
box(C, CX0, CX0 + 2, CT - 2, CT, CZ0, CZ1, TEX_STEEL_DK); box(C, CX1 - 2, CX1, CT - 2, CT, CZ0, CZ1, TEX_STEEL_DK)
box(C, CX0 - 1, CX1 + 1, 56, 59, CZ0 - 1, CZ1 + 1, TEX_STEEL)                                  # counter ledge
C.write('d_DisplayCounter', 'd_displaycounter.obj'); written.append('d_displaycounter')

# ---------------------------------------------------------------- canteen sign (on the wall, front +z): the sprite itself as the sign face
NX0, NX1, NY0, NY1, NZ0, NZ1 = bounds('d_canteensign.obj')
N = Mesh()
box(N, NX0, NX1, NY0, NY1, NZ0, NZ0 + 3, sw((40, 42, 46)))
face = tex(sprite('d_CanteenSign', trim=1))
N.quad([(NX0 + 1, NY0 + 1, NZ0 + 3.5), (NX1 - 1, NY0 + 1, NZ0 + 3.5), (NX1 - 1, NY1 - 1, NZ0 + 3.5), (NX0 + 1, NY1 - 1, NZ0 + 3.5)], (0, 0, 1), tuvs(face))
N.write('d_CanteenSign', 'd_canteensign.obj'); written.append('d_canteensign')

# ---------------------------------------------------------------- cells (user 9 Nov: cells next): skin the existing models with their sprite.
# Their shapes are already detailed but flat-coloured; every face is re-textured by where it is: faces looking up take the sprite's top part
# at their (x, z), faces looking to the front its front part at their (x, y), side faces the front's edge stretched by (z, y). Sprites
# that are a plain top view (beds) have no front part: their front and side faces keep their colours.
def skin(fname, spr, split=1.0, trim=3, keep_front=False):
    src = [l.rstrip('\n') for l in open(os.path.join(IN, fname), encoding='latin1')]
    V = [tuple(map(float, l.split()[1:4])) for l in src if l.startswith('v ')]
    nvt = sum(1 for l in src if l.startswith('vt '))
    xs, ys, zs = [v[0] for v in V], [v[1] for v in V], [v[2] for v in V]
    X0, X1, Y0, Y1, Z0, Z1 = min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)
    if split >= 0.999:
        topT = tex(sprite(spr, trim)); frontT = sideT = None
    else:
        topT, frontT, sideT, _ = parts(spr, split, trim)
        if keep_front: frontT = sideT = None
    def at(rect, a, b):      # a, b in 0..1 across / down the image -> uv
        u0, v0, u1, v1 = rect[1]
        a = min(1, max(0, a)); b = min(1, max(0, b))
        return (u0 + (u1 - u0) * a, v1 - (v1 - v0) * b)
    newvt = []; out = []
    for l in src:
        if not l.startswith('f '):
            out.append(l); continue
        c = l.split()[1:]
        idx = [int(q.split('/')[0]) - 1 for q in c]
        P = [V[i] for i in idx]
        ux, uy, uz = P[1][0] - P[0][0], P[1][1] - P[0][1], P[1][2] - P[0][2]
        wx, wy, wz = P[2][0] - P[0][0], P[2][1] - P[0][1], P[2][2] - P[0][2]
        n = (uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx); nl = math.sqrt(sum(q * q for q in n)) or 1.0
        n = tuple(q / nl for q in n)
        uvs = None
        if n[1] > 0.6: uvs = [at(topT, (p[0] - X0) / (X1 - X0 or 1), (p[2] - Z0) / (Z1 - Z0 or 1)) for p in P]
        elif frontT and n[2] > 0.6: uvs = [at(frontT, (p[0] - X0) / (X1 - X0 or 1), 1 - (p[1] - Y0) / (Y1 - Y0 or 1)) for p in P]
        elif sideT and abs(n[0]) > 0.6: uvs = [at(sideT, (p[2] - Z0) / (Z1 - Z0 or 1) if n[0] < 0 else 1 - (p[2] - Z0) / (Z1 - Z0 or 1), 1 - (p[1] - Y0) / (Y1 - Y0 or 1)) for p in P]
        if uvs is None:
            out.append(l); continue
        parts_ = []
        for q, uv_ in zip(c, uvs):
            s = q.split('/')
            newvt.append(uv_)
            s = [s[0], str(nvt + len(newvt))] + (s[2:] if len(s) > 2 else [])
            parts_.append('/'.join(s))
        out.append('f ' + ' '.join(parts_))
    # the new uvs follow the old ones (indices nvt + 1 ...) and come before the normals, so before every face that uses them
    res = []; placed = False
    for l in out:
        if not placed and l.startswith('vn '):
            res += ['vt %.5f %.5f 0.0000' % t for t in newvt]; placed = True
        res.append(l)
    if not placed: res = [l for l in res if not l.startswith('f ')] + ['vt %.5f %.5f 0.0000' % t for t in newvt] + [l for l in res if l.startswith('f ')]
    res = [l.replace('# PAIso generated model:', '# PAIso generated model (sprite-skinned, kitchen.py):', 1) for l in res]
    open(os.path.join(OUT, fname), 'w', encoding='latin1').write('\n'.join(res) + '\n')
    written.append(fname[:-4])

# (the cell models are now skinned - with relief - by skinall.py, which runs after this script on its output, for every generated model;
#  skin() stays here for reference)

atlas.save(os.path.join(OUT, 'objects-maps.png'))
print('kitchen: %s written; %d swatches in the atlas' % (', '.join(written), len(swatches)))
