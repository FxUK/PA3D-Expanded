r"""Kitchen food in 3D (user 10 Nov: "the food / ingredients now need doing properly"). The DLC food (objects_d11_2: 12 foods, each as a
single ingredient, a crate of it, a bowl / plate of it, a pot of it and a cooked pot / tray of it) had no 3D models - only the 2D art,
which the mod drew as flat extruded cut-outs. This writes a model for every one of those sprites, textured from its own art:
  crates (d_d11stackingredientsNN, stack meals 09-12)   open cardboard crate, flaps out, a lumpy heap of the food (the sprite's contents)
  egg trays (d_d11stackingredients04)                     a stack of pulp trays, eggs on top
  pots (stack meals / cooked 01 02 05-08)                 steel pot with handles, the food inside
  frying pans (stack meals 03 04)                         shallow pan, long handle
  roasting trays (stack meals cooked 03 04)               steel tray with the food in it
  bowls (d_d11foodNN 01 02 05-08)                         small bowl of food
  single items (d_d11ingredientNN, food 03 04 09-12)      raised from the sprite itself: its outline is the footprint, its height a rounded
                                                          dome from the distance to the edge (fruit come out round, a banana a tube,
                                                          bacon and fried egg nearly flat)
usage: python -I food.py OUT DATA
  OUT  = a new folder: food.txt + food.rgba (what the DLL loads: copy them to PA3aiD\food3d), plus each model as .obj with its own
         texture sheet as objects-maps.png (for obj_prev.py)
  DATA = the stock game data folder (PA3aiD\stock_data\full\data)
1.9.128: the game never draws a stack's DLC food with a 3D model (it always uses the 2D sprite, which the mod extrudes), so these models are
NOT installed in data\shapes: the DLL draws them itself wherever that sprite is drawn. They have their own texture sheet (the game's atlas
is left alone).
food.txt: "atlas W H", then per model "model NAME BANKX BANKY W H NTRI" (its sprite's place in objects_d11_2) and NTRI lines of three
vertices, each "u v nx ny nz x y z".
Units: a tile is 100, y up, the front (towards the camera in the game's own view) is +z, the object's centre on the floor at the origin."""
import sys, os, math, re
from PIL import Image, ImageStat
import numpy as np

OUT, DATA = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
BLK = 8
AW = AH = 1024
atlas = Image.new('RGB', (AW, AH), (0, 0, 0))
shelf = [0, 0, 0]      # x, y, height of the current row

def put(img):
    """pack a texture into the food sheet (rows, 2 px apart); returns its uv rectangle, inset by a texel against mip bleeding"""
    img = img.convert('RGB'); w, h = img.size
    if shelf[0] + w + 2 > AW: shelf[0] = 0; shelf[1] += shelf[2] + 2; shelf[2] = 0
    if shelf[1] + h > AH: raise SystemExit('food sheet full')
    x, y = shelf[0], shelf[1]
    atlas.paste(img, (x, y))
    shelf[0] += w + 2; shelf[2] = max(shelf[2], h)
    return ((x + 1) / AW, 1.0 - (y + h - 1) / AH, (x + w - 1) / AW, 1.0 - (y + 1) / AH)      # u0, v0 (bottom), u1, v1 (top)

swatches = {}
def sw(rgb):
    rgb = tuple(int(max(0, min(255, c))) for c in rgb)
    if rgb not in swatches:
        u0, v0, u1, v1 = put(Image.new('RGB', (BLK, BLK), rgb))
        swatches[rgb] = ((u0 + u1) / 2, (v0 + v1) / 2)
    return swatches[rgb]

# ---------------------------------------------------------------- sprites
t = open(os.path.join(DATA, 'objects_d11_2.spritebank'), encoding='latin1').read()
SIM = Image.open(os.path.join(DATA, 'objects_d11_2.png')).convert('RGBA')
SPR = {}
for m in re.finditer(r'Name (\S+)\s+x (\d+)\s+y (\d+)\s+w (\d+)\s+h (\d+)', t):
    n = m.group(1); x, y, w, h = map(int, m.groups()[1:])
    SPR.setdefault(n.lower(), (n, SIM.crop((x * 32, y * 32, (x + w) * 32, (y + h) * 32)), (x, y, w, h)))

def bled(img, mask, passes=4):
    """the sprite's colours with the black outline (dark pixels on the edge of the mask) replaced by the colours next to them"""
    a = np.asarray(img.convert('RGB')).astype(float)
    lum = a @ np.array([0.299, 0.587, 0.114])
    good = mask & (lum > 48)
    for _ in range(passes):
        acc = np.zeros_like(a); cnt = np.zeros(good.shape)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)):
            g = np.roll(np.roll(good, dy, 0), dx, 1); c = np.roll(np.roll(a, dy, 0), dx, 1)
            acc += c * g[..., None]; cnt += g
        grow = (~good) & (cnt > 0)
        a[grow] = acc[grow] / cnt[grow][:, None]
        good = good | grow
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

def crop_tex(name, box):
    """a part of the sprite (x0, y0, x1, y1 in its pixels), outline bled out, put in the atlas"""
    s = SPR[name][1]
    a = np.asarray(s)
    mask = a[:, :, 3] > 100
    img = bled(s, mask).crop(box)
    return put(img.resize((img.width * 2, img.height * 2), Image.BILINEAR))

def mean_rgb(name, box):
    s = SPR[name][1]
    a = np.asarray(s.crop(box)).astype(float)
    m = a[:, :, 3] > 100
    lum = a[:, :, :3] @ np.array([0.299, 0.587, 0.114])
    m &= lum > 48
    if not m.any(): return (128, 128, 128)
    return tuple(a[:, :, :3][m].mean(axis=0))

# ---------------------------------------------------------------- mesh
class Mesh:
    def __init__(s): s.v = []; s.vt = []; s.vn = []; s.f = []
    def tri(s, P, T, N=None, out=None):
        """a triangle; P points, T uvs, N per-vertex normals (or None: flat). Wound so its face normal points along `out` (or N's mean)."""
        e1 = np.subtract(P[1], P[0]); e2 = np.subtract(P[2], P[0]); fn = np.cross(e1, e2)
        l = np.linalg.norm(fn)
        if l < 1e-9: return
        fn = fn / l
        want = out if out is not None else (np.mean(N, axis=0) if N is not None else fn)
        if np.dot(fn, want) < 0: P = [P[0], P[2], P[1]]; T = [T[0], T[2], T[1]]; N = None if N is None else [N[0], N[2], N[1]]; fn = -fn
        bv = len(s.v); bt = len(s.vt)
        s.v += [tuple(p) for p in P]; s.vt += list(T)
        if N is None:
            s.vn.append(tuple(fn)); ni = [len(s.vn)] * 3
        else:
            ni = []
            for n in N:
                n = np.asarray(n, float); n = n / (np.linalg.norm(n) or 1.0); s.vn.append(tuple(n)); ni.append(len(s.vn))
        s.f.append(tuple((bv + i + 1, bt + i + 1, ni[i]) for i in range(3)))
    def quad(s, P, T, out):
        s.tri([P[0], P[1], P[2]], [T[0], T[1], T[2]], out=out); s.tri([P[0], P[2], P[3]], [T[0], T[2], T[3]], out=out)
    def box(s, x0, x1, y0, y1, z0, z1, uv, bottom=False):
        c = (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2
        F = [([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0)), ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
             ([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1)), ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0)),
             ([(x1, y0, z0), (x1, y0, z1), (x1, y1, z1), (x1, y1, z0)], (1, 0, 0))]
        if bottom: F.append(([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0)))
        for P, n in F: s.quad(P, [uv] * 4, n)
    def ring(s, r0, r1, y0, y1, uv, seg=20, cx=0.0, cz=0.0, inward=False, sx=1.0):
        """a (tapered) upright tube from radius r0 at y0 to r1 at y1, smooth normals"""
        for i in range(seg):
            a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            P = [(cx + math.cos(a) * r * sx, y, cz + math.sin(a) * r) for a, r, y in ((a0, r0, y0), (a1, r0, y0), (a1, r1, y1), (a0, r1, y1))]
            sg = -1 if inward else 1
            N = [(sg * math.cos(a) / sx, sg * (r0 - r1) / max(1e-6, y1 - y0) * 0.5, sg * math.sin(a)) for a in (a0, a1, a1, a0)]
            s.tri([P[0], P[1], P[2]], [uv] * 3, [N[0], N[1], N[2]]); s.tri([P[0], P[2], P[3]], [uv] * 3, [N[0], N[2], N[3]])
    def disc(s, r, y, tex_rect, seg=24, cx=0.0, cz=0.0, sx=1.0, bump=0.0):
        """a round top at height y, the texture laid on it from above (a domed centre when bump > 0)"""
        u0, v0, u1, v1 = tex_rect
        def uv(px, pz): return (u0 + (u1 - u0) * (0.5 + (px - cx) / (2 * r * sx)), v1 - (v1 - v0) * (0.5 + (pz - cz) / (2 * r)))
        rings = 4
        for k in range(rings):
            ra, rb = r * k / rings, r * (k + 1) / rings
            ya, yb = y + bump * (1 - (k / rings) ** 2), y + bump * (1 - ((k + 1) / rings) ** 2)
            for i in range(seg):
                a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
                P = [(cx + math.cos(a) * rr * sx, yy, cz + math.sin(a) * rr) for a, rr, yy in ((a0, ra, ya), (a1, ra, ya), (a1, rb, yb), (a0, rb, yb))]
                T = [uv(p[0], p[2]) for p in P]
                if k == 0: s.tri([P[0], P[2], P[3]], [T[0], T[2], T[3]], out=(0, 1, 0))
                else: s.quad(P, T, (0, 1, 0))
    def heap(s, x0, x1, z0, z1, y0, amp, tex_rect, n=14, seed=1):
        """a lumpy pile filling a rectangle, the texture laid on from above; its edge comes down to y0"""
        u0, v0, u1, v1 = tex_rect
        rng = np.random.RandomState(seed)
        ph = rng.uniform(0, 6.28, 6)
        H = np.zeros((n + 1, n + 1)); Pz = {}
        for j in range(n + 1):
            for i in range(n + 1):
                a, b = i / n, j / n
                edge = min(a, 1 - a, b, 1 - b) * 2
                lump = 0.55 + 0.25 * math.sin(a * 9 + ph[0]) * math.sin(b * 8 + ph[1]) + 0.2 * math.sin(a * 17 + b * 13 + ph[2])
                H[j, i] = y0 + amp * min(1.0, edge * 3.0) * lump
        def P(i, j): return (x0 + (x1 - x0) * i / n, H[j, i], z0 + (z1 - z0) * j / n)
        def T(i, j): return (u0 + (u1 - u0) * i / n, v1 - (v1 - v0) * j / n)
        def Nv(i, j):
            dx = (H[j, min(n, i + 1)] - H[j, max(0, i - 1)]) / ((x1 - x0) / n * 2); dz = (H[min(n, j + 1), i] - H[max(0, j - 1), i]) / ((z1 - z0) / n * 2)
            return (-dx, 1.0, -dz)
        for j in range(n):
            for i in range(n):
                q = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                Pq = [P(*c) for c in q]; Tq = [T(*c) for c in q]; Nq = [Nv(*c) for c in q]
                s.tri([Pq[0], Pq[1], Pq[2]], [Tq[0], Tq[1], Tq[2]], [Nq[0], Nq[1], Nq[2]]); s.tri([Pq[0], Pq[2], Pq[3]], [Tq[0], Tq[2], Tq[3]], [Nq[0], Nq[2], Nq[3]])
    def write(s, name):
        with open(os.path.join(OUT, name.lower() + '.obj'), 'w') as fo:
            fo.write('# PAIso generated model: %s (food.py)\n\ng %s\n' % (name, name))
            for x, y, z in s.v: fo.write('v %.4f %.4f %.4f\n' % (x, y, z))
            for u, w in s.vt: fo.write('vt %.5f %.5f 0.0000\n' % (u, w))
            for x, y, z in s.vn: fo.write('vn %.4f %.4f %.4f\n' % (x, y, z))
            for tri in s.f: fo.write('f ' + ' '.join('%d/%d/%d' % c for c in tri) + '\n')
        return len(s.f)

STEEL, STEEL_DK, STEEL_IN = sw((176, 180, 186)), sw((112, 116, 122)), sw((140, 144, 150))
BLACK = sw((34, 34, 38))

# ---------------------------------------------------------------- shapes
def crate(name):
    """open cardboard crate (the stock crates' size), flaps folded out, the food heaped inside"""
    card = mean_rgb(name, (12, 44, 52, 60))
    C, Cin, Cfl = sw(card), sw(tuple(c * 0.62 for c in card)), sw(tuple(min(255, c * 1.08) for c in card))
    food = crop_tex(name, (8, 15, 56, 40))
    m = Mesh()
    X, Z, H, w = 36.0, 38.0, 30.0, 2.0
    m.box(-X, X, 0, H, Z - w, Z, C); m.box(-X, X, 0, H, -Z, -Z + w, C)
    m.box(-X, -X + w, 0, H, -Z + w, Z - w, C); m.box(X - w, X, 0, H, -Z + w, Z - w, C)
    # inner faces darker (the inside of the box): thin liners just inside the walls
    m.box(-X + w, X - w, 2, H - 0.5, Z - w - 0.4, Z - w, Cin); m.box(-X + w, X - w, 2, H - 0.5, -Z + w, -Z + w + 0.4, Cin)
    m.box(-X + w, -X + w + 0.4, 2, H - 0.5, -Z + w, Z - w, Cin); m.box(X - w - 0.4, X - w, 2, H - 0.5, -Z + w, Z - w, Cin)
    m.heap(-X + w, X - w, -Z + w, Z - w, H - 9, 12.0, food, seed=sum(map(ord, name)))
    # four flaps hinged on the top edges, opened out and up (about 35 degrees above flat)
    L, ca, sa = 16.0, math.cos(math.radians(35)), math.sin(math.radians(35))
    for side in range(4):
        if side == 0: P = [(-X, H, Z), (X, H, Z), (X, H + L * sa, Z + L * ca), (-X, H + L * sa, Z + L * ca)]
        elif side == 1: P = [(-X, H, -Z), (X, H, -Z), (X, H + L * sa, -Z - L * ca), (-X, H + L * sa, -Z - L * ca)]
        elif side == 2: P = [(-X, H, -Z), (-X, H, Z), (-X - L * ca, H + L * sa, Z), (-X - L * ca, H + L * sa, -Z)]
        else: P = [(X, H, -Z), (X, H, Z), (X + L * ca, H + L * sa, Z), (X + L * ca, H + L * sa, -Z)]
        c = np.mean(P, axis=0); o = (c[0], 2.0, c[2])
        m.quad(P, [Cfl] * 4, out=o); m.quad(P, [Cin] * 4, out=(-o[0], -2.0, -o[2]))
    return m

def eggtrays(name):
    pulp = mean_rgb(name, (6, 44, 58, 62))
    Pu, Pd = sw(pulp), sw(tuple(c * 0.8 for c in pulp))
    eggs = crop_tex(name, (5, 2, 59, 38))
    m = Mesh()
    X, Z = 34.0, 36.0
    for k in range(4):
        y = k * 6.0
        m.box(-X, X, y, y + 5.0, -Z, Z, Pu if k % 2 else Pd)
    m.heap(-X + 1, X - 1, -Z + 1, Z - 1, 24.0, 7.0, eggs, n=18, seed=4)
    return m

def pot(name):
    food = crop_tex(name, (16, 13, 48, 31))
    m = Mesh()
    R, H = 25.0, 30.0
    m.ring(R * 0.92, R, 0, H, STEEL, seg=24)
    m.ring(R, R - 1.5, H, H + 0.6, STEEL, seg=24)
    m.ring(R - 1.5, R - 1.5, H - 6, H, STEEL_IN, seg=24, inward=True)
    m.disc(R - 1.5, H - 5.0, food, bump=3.0)
    for sx in (-1, 1):      # two loop handles: steel arms out from the rim, a black grip across their ends
        a, b = sorted((sx * (R - 1), sx * (R + 8)))
        m.box(a, b, H - 7, H - 4, -7, -4, STEEL_DK); m.box(a, b, H - 7, H - 4, 4, 7, STEEL_DK)
        a, b = sorted((sx * (R + 6), sx * (R + 9)))
        m.box(a, b, H - 7.5, H - 3.5, -7, 7, BLACK)
    return m

def pan(name):
    food = crop_tex(name, (14, 12, 44, 40))
    rim = mean_rgb(name, (10, 30, 20, 44))
    m = Mesh()
    R = 24.0
    m.ring(R * 0.9, R, 0, 7, sw(rim), seg=24)
    m.ring(R - 1.2, R - 1.2, 2, 7, BLACK, seg=24, inward=True)
    m.disc(R - 1.2, 3.5, food, bump=2.5)
    # the long handle, out to the front right as in the sprite
    a = math.radians(45); ca, sa = math.cos(a), math.sin(a)
    def rot(x, z): return (x * ca - z * sa, x * sa + z * ca)
    for (x0, x1, y0, y1, w) in ((R - 2, R + 26, 4.5, 6.5, 3.0),):
        P = [rot(x0, -w), rot(x1, -w), rot(x1, w), rot(x0, w)]
        top = [(p[0], y1, p[1]) for p in P]; bot = [(p[0], y0, p[1]) for p in P]
        m.quad(top, [BLACK] * 4, (0, 1, 0)); m.quad(bot, [BLACK] * 4, (0, -1, 0))
        for i in range(4):
            j = (i + 1) % 4; q = [bot[i], bot[j], top[j], top[i]]; c = np.mean(q, axis=0)
            m.quad(q, [BLACK] * 4, (c[0] - 0, 0, c[2]) if i in (0, 2) else (c[0], 0, c[2]))
    return m

def tray(name):
    food = crop_tex(name, (8, 6, 56, 42))
    m = Mesh()
    X, Z, H, w = 32.0, 25.0, 8.0, 2.0
    m.box(-X, X, 0, 1.5, -Z, Z, STEEL_DK)
    m.box(-X, X, 0, H, Z - w, Z, STEEL); m.box(-X, X, 0, H, -Z, -Z + w, STEEL)
    m.box(-X, -X + w, 0, H, -Z + w, Z - w, STEEL); m.box(X - w, X, 0, H, -Z + w, Z - w, STEEL)
    m.heap(-X + w, X - w, -Z + w, Z - w, 2.0, 6.0, food, n=16, seed=sum(map(ord, name)))
    return m

def bowl(name):
    # measured from the sprite: the food is the upper half of the bowl's outline, the china its lower part
    a = np.asarray(SPR[name][1]); ys, xs = np.nonzero(a[:, :, 3] > 100)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    food = crop_tex(name, (x0 + 4, y0 + int((y1 - y0) * 0.22), x1 - 4, y0 + int((y1 - y0) * 0.5)))      # below the bowl's back rim
    china = mean_rgb(name, (x0 + 2, y0 + int((y1 - y0) * 0.6), x1 - 2, y1 - 2))
    if china[1] > china[0] * 1.25 or china[0] + china[1] + china[2] < 360: china = (226, 222, 214)      # a white bowl unless the art clearly says otherwise
    m = Mesh()
    R, H = 10.0, 8.0
    m.ring(R * 0.65, R, 0, H, sw(china), seg=18)
    m.disc(R - 0.8, H - 1.5, food, seg=18, bump=1.2)
    return m

def relief(name, flat=1.0, scale=0.8):
    """the sprite raised into 3D: its outline is the footprint, its height a rounded dome from each pixel's distance to the edge"""
    s = SPR[name][1]
    a = np.asarray(s)
    mask = a[:, :, 3] > 100
    lum = a[:, :, :3].astype(float) @ np.array([0.299, 0.587, 0.114])
    # drop the black outline ring: dark pixels within 2 px of the outside
    outside = ~mask; near = outside.copy()
    for _ in range(2):
        near = near | np.roll(near, 1, 0) | np.roll(near, -1, 0) | np.roll(near, 1, 1) | np.roll(near, -1, 1)
    body = mask & ~(near & (lum < 60))
    if body.sum() < 12: body = mask
    ys, xs = np.nonzero(body)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    body = body[y0:y1, x0:x1]
    tex = bled(s, mask).crop((x0, y0, x1, y1))
    rect = put(tex.resize((tex.width * 2, tex.height * 2), Image.BILINEAR))
    u0, v0, u1, v1 = rect
    hh, ww = body.shape
    # distance (in pixels) to the outside, 8-connected
    INF = 999
    d = np.where(body, INF, 0).astype(int)
    d = np.pad(d, 1)
    for _ in range(64):
        n = np.minimum.reduce([np.roll(d, 1, 0), np.roll(d, -1, 0), np.roll(d, 1, 1), np.roll(d, -1, 1)]) + 1
        nd = np.where(d > 0, np.minimum(d, n), 0)
        if (nd == d).all(): break
        d = nd
    d = d[1:-1, 1:-1].astype(float) - 0.5
    R = max(1.0, d.max())
    hp = np.where(body, np.sqrt(np.maximum(0.0, R * R - (R - d) ** 2)), 0.0) * flat      # pixel heights, in pixels
    # vertex heights at pixel corners: the mean of the body pixels around a corner, 0 on the outline
    Hv = np.zeros((hh + 1, ww + 1)); In = np.zeros((hh + 1, ww + 1), bool)
    for j in range(hh + 1):
        for i in range(ww + 1):
            around = [(j - 1, i - 1), (j - 1, i), (j, i - 1), (j, i)]
            inn = [body[q] if 0 <= q[0] < hh and 0 <= q[1] < ww else False for q in around]
            if all(inn): Hv[j, i] = np.mean([hp[q] for q in around]); In[j, i] = True
    cx, cz = ww / 2.0, hh / 2.0
    def P(i, j): return ((i - cx) * scale, Hv[j, i] * scale + 0.3, (j - cz) * scale)
    def T(i, j): return (u0 + (u1 - u0) * i / ww, v1 - (v1 - v0) * j / hh)
    def Nv(i, j):
        dx = (Hv[j, min(ww, i + 1)] - Hv[j, max(0, i - 1)]) / 2.0; dz = (Hv[min(hh, j + 1), i] - Hv[max(0, j - 1), i]) / 2.0
        return (-dx, 1.0, -dz)
    m = Mesh()
    for j in range(hh):
        for i in range(ww):
            if not body[j, i]: continue
            q = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
            Pq = [P(*c) for c in q]; Tq = [T(*c) for c in q]; Nq = [Nv(*c) for c in q]
            m.tri([Pq[0], Pq[1], Pq[2]], [Tq[0], Tq[1], Tq[2]], [Nq[0], Nq[1], Nq[2]]); m.tri([Pq[0], Pq[2], Pq[3]], [Tq[0], Tq[2], Tq[3]], [Nq[0], Nq[2], Nq[3]])
            # a skirt down to the floor along the outline (no see-through edge)
            for (a_, b_), (ni, nj) in (((q[0], q[1]), (i, j - 1)), ((q[1], q[2]), (i + 1, j)), ((q[2], q[3]), (i, j + 1)), ((q[3], q[0]), (i - 1, j))):
                if 0 <= ni < ww and 0 <= nj < hh and body[nj, ni]: continue
                pa, pb = P(*a_), P(*b_)
                if pa[1] < 0.35 and pb[1] < 0.35: continue
                ga, gb = (pa[0], 0.0, pa[2]), (pb[0], 0.0, pb[2])
                c = ((pa[0] + pb[0]) / 2 - (i + 0.5 - cx) * scale, 0, (pa[2] + pb[2]) / 2 - (j + 0.5 - cz) * scale)
                m.quad([ga, gb, pb, pa], [T(*a_), T(*b_), T(*b_), T(*a_)], c)
    return m

FLAT = {3: 0.22, 4: 0.18}      # bacon, fried egg: nearly flat
written = []
TXT = []
def emit(m, key):
    name, _, (bx, by, bw, bh) = SPR[key]
    n = m.write(name)
    written.append((name, n))
    TXT.append('model %s %d %d %d %d %d' % (name, bx, by, bw, bh, len(m.f)))
    for tri in m.f:
        TXT.append(' '.join('%.5f %.5f %.4f %.4f %.4f %.3f %.3f %.3f' % (m.vt[ti - 1] + m.vn[ni - 1] + m.v[vi - 1]) for vi, ti, ni in tri))

for k in range(1, 13):
    nn = '%02d' % k
    emit(eggtrays('d_d11stackingredients' + nn) if k == 4 else crate('d_d11stackingredients' + nn), 'd_d11stackingredients' + nn)
    emit(relief('d_d11ingredient' + nn, FLAT.get(k, 1.0)), 'd_d11ingredient' + nn)
    if k in (1, 2, 5, 6, 7, 8):
        emit(bowl('d_d11food' + nn), 'd_d11food' + nn)
        emit(pot('d_d11stackmeals' + nn), 'd_d11stackmeals' + nn)
        emit(pot('d_d11stackmealscooked' + nn), 'd_d11stackmealscooked' + nn)
    elif k in (3, 4):
        emit(relief('d_d11food' + nn, FLAT.get(k, 1.0)), 'd_d11food' + nn)
        emit(pan('d_d11stackmeals' + nn), 'd_d11stackmeals' + nn)
        emit(tray('d_d11stackmealscooked' + nn), 'd_d11stackmealscooked' + nn)
    else:
        emit(relief('d_d11food' + nn), 'd_d11food' + nn)
        emit(crate('d_d11stackmeals' + nn), 'd_d11stackmeals' + nn)
        emit(crate('d_d11stackmealscooked' + nn), 'd_d11stackmealscooked' + nn)

atlas.save(os.path.join(OUT, 'objects-maps.png'))
open(os.path.join(OUT, 'food.txt'), 'w').write('atlas %d %d\n' % (AW, AH) + '\n'.join(TXT) + '\n')
open(os.path.join(OUT, 'food.rgba'), 'wb').write(atlas.convert('RGBA').tobytes())
print('food: %d models, %d triangles, %d swatches' % (len(written), sum(n for _, n in written), len(swatches)))
for name, n in written: print('  %-28s %5d' % (name, n))
