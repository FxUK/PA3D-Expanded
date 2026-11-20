#!/usr/bin/env python3
"""PAIso model pack generator.

For every Prison Architect object whose sprite has no 3D model, build a chunky
cartoon "cookie-cutter" model: the sprite's silhouette is extruded to a height
that suits the object, the top carries the sprite artwork, the sides are
painted with the sprite's own edge colours (slightly darkened), and the top
edge gets a small bevel highlight band.  Trees get a trunk + canopy archetype.

Inputs : extracted game data dir (data/...)
Outputs: <out>/data/shapes/*.obj and an extended objects-maps.png atlas.
"""
import os, re, sys, math, json, shutil
from PIL import Image, ImageStat, ImageFilter

DATA = sys.argv[1]           # .../data
OUT = sys.argv[2]            # output root (gets data/shapes)
CELL = 32                    # px per spritebank cell
UNITS_PER_CELL = 50          # 100 model units per tile, 2 cells per tile
GRID_PX = 5                 # silhouette resolution (px of source sprite per voxel column)

shapes_dir = os.path.join(DATA, 'shapes')
have = {f[:-4].lower() for f in os.listdir(shapes_dir) if f.endswith('.obj')}

# earlier PAIso installs replaced some stock models (guards, table, toilet...) with restyled ones;
# list them in force.txt (one name per line) to regenerate them against the current atlas
_ff = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'force.txt')
if os.path.exists(_ff):
    have -= {l.strip().lower() for l in open(_ff) if l.strip()}

banks = [('objects.spritebank', 'objects.png'),
         ('objects_d11.spritebank', 'objectsTexture_d11.png'),
         ('objects_d11_2.spritebank', 'objects_d11_2.png')]
sprites = {}
for sbf, imf in banks:
    if not os.path.exists(os.path.join(DATA, sbf)):   # DLC bank not installed (base game)
        continue
    t = open(os.path.join(DATA, sbf), encoding='latin1').read()
    im = Image.open(os.path.join(DATA, imf)).convert('RGBA')
    quads = {(0, 0): im}
    if sbf == 'objects.spritebank':   # the game composites these sheets into one 4096x4096 bank
        quads[(0, 1)] = Image.open(os.path.join(DATA, 'people.png')).convert('RGBA')
        quads[(1, 0)] = Image.open(os.path.join(DATA, 'special-entities.png')).convert('RGBA')
    for m in re.finditer(r'Name (\S+)\s+x (\d+)\s+y (\d+)\s+w (\d+)\s+h (\d+)', t):
        n = m.group(1); x, y, w, h = map(int, m.groups()[1:])
        if n.lower() in sprites: continue
        q = (x // 64, y // 64) if sbf == 'objects.spritebank' else (0, 0)
        src = quads.get(q)
        if src is None: continue
        lx, ly = (x - q[0] * 64, y - q[1] * 64) if sbf == 'objects.spritebank' else (x, y)
        box = (lx * CELL, ly * CELL, (lx + w) * CELL, (ly + h) * CELL)
        if box[2] > src.width or box[3] > src.height: continue
        sprites[n.lower()] = (n, w, h, src.crop(box))

# ---------------------------------------------------------------- objects
objs = []
sprite_scale = {}
for mf in ['materials.txt', 'materials_dlc.txt']:
    t = open(os.path.join(DATA, mf), encoding='latin1').read()
    for blk in re.findall(r'BEGIN Object(.*?)\nEND', t, re.S):
        n = re.search(r'Name\s+(\S+)', blk).group(1)
        sp = (re.findall(r'\bSprite\s+(\S+)', blk) or [n])[0]
        props = set(re.findall(r'Properties\s+(\S+)', blk))
        _sc = re.search(r'\bSpriteScale\s+([0-9.]+)', blk)
        sprite_scale.setdefault(sp.lower(), float(_sc.group(1)) if _sc else 1.0)
        objs.append((n, sp, props))

SKIP_PROPS = {'BuiltOnWall', 'Door', 'Window'}   # Utility (power, water, pipes, cables) now gets low 3D models too
SKIP_NAMES = re.compile(r'(cancel|dismantle|sell|tidy|showall|wire|cutscene|polaroid|collectable|'
                        r'marker|zone|light$|^light|sprinkler|drain|rug|mat$|playmat|prayermat|graffiti|'
                        r'puddle|blood|vomit|stain|footprint|logiccircuit|logicbridge|servo|pressurepad|'
                        r'statuslight|doortimer|powerswitch|pipevalve|grassturf|banner|fire$|smoke|'
                        r'rubble|crack|hole|tunnel|dummy|toolbar)', re.I)

# height in tiles by keyword (first match wins)
HEIGHTS = [
    (r'pipe|cable', 0.12),
    (r'prayermat|mat$', 0.04),
    (r'bookshelf|bookcase', 1.4),
    (r'tray|plate|meal|food|mail|letter|paper|book|uniform|rubble|trash|garbage', 0.05),
    (r'bridge|helipad|dock|stepping|walkway|wallpanel|checkpoint|doo$|droppings|letter|mooring', 0.06),
    (r'boxingring|fightring', 0.3),
    (r'gnome|flamingo|statue|gravestone', 0.8), (r'dogbowl|ratdroppings|rattrap|pushupstones|dogdoo|fertilizer|jackolantern|haybale', 0.25), (r'(^|_)rat$', 0.1),
    (r'tree|plant|bush|cactus|conifer|sakura|crop|barley|wheat|corn|fern|hedge|topiary|ivy|flower', 'tree'),
    (r'bunk', 1.25), (r'bed|cot|crib|mattress|slab', 0.5),
    (r'table|desk|counter|bench(?!.*full)|workbench|sortingtable', 0.66),
    (r'chair|stool|seat|pew|sofa|couch', 0.42),
    (r'shelf|bookcase|bookshelf|locker|cabinet|wardrobe|drawers|fridge|freezer|vending|machine|'
     r'arcade|rack|cupboard|station|boiler|generator|capacitor|press|saw|oven|cooker|washer|dryer|'
     r'pump|filingcabinet|safe|crate$', 1.4),
    (r'sink|basin|toilet|urinal|dryer', 0.85),
    (r'shower', 1.9), (r'tv|television|monitor|radio|phone|boombox', 0.9),
    (r'altar|podium|lectern|piano|drum|amp', 1.0),
    (r'barrier|gate|fence|bars', 1.2), (r'stack|pile|box|bag|sack|log|wood|metal|brick|plate', 0.2),
    (r'tray|meal|food|book|mail|paper|newspaper|uniform|bleach|ingredient', 0.12),
]
def height_for(name, props):
    if 'Vehicle' in props: return 1.3
    for pat, h in HEIGHTS:
        if re.search(pat, name, re.I): return h
    if 'Material' in props or 'Rubbish' in props: return 0.3
    return 0.7

# ---------------------------------------------------------------- atlas
atlas = Image.open(os.path.join(shapes_dir, 'objects-maps.png')).convert('RGB')
AW, AH = atlas.size
BLK = 8
gw, gh = AW // BLK, AH // BLK
free = [[True] * gw for _ in range(gh)]
px = atlas.load()
for by in range(gh):
    for bx in range(gw):
        for yy in range(by * BLK, by * BLK + BLK, 2):
            for xx in range(bx * BLK, bx * BLK + BLK, 2):
                r, g, b = px[xx, yy]
                if r + g + b > 24: free[by][bx] = False; break
            if not free[by][bx]: break
# black texels that the game's own models use (outlines, eyes, dark trims) read as empty above: never hand those blocks out
# (they were being painted over with swatches - about 300 stock texels, e.g. black parts of people turning grey or brown)
def uv_blocks(folder, bw, bh):
    """every atlas block (of a bw x bh grid) that a folder's models' textured triangles cover - their whole uv footprint, not only
    the corners (people's faces are textured areas) - wrapped like the GPU (some stock uvs are outside 0..1)"""
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
                xs = [u * bw for u, v in uvs]; ys = [(1.0 - v) * bh for u, v in uvs]
                for by in range(int(math.floor(min(ys))), int(math.floor(max(ys))) + 1):
                    for bx in range(int(math.floor(min(xs))), int(math.floor(max(xs))) + 1): out.add((by % bh, bx % bw))
    return out
for (_by, _bx) in uv_blocks(shapes_dir, gw, gh): free[_by][_bx] = False
# keep a small margin around used areas so mip filtering never bleeds between
# placements.  1 block (8px) is enough: sprite art already has its edge colours
# bled into the transparent pixels, and swatches are solid colour.
MARGIN = 1
used = [[not free[y][x] for x in range(gw)] for y in range(gh)]
for y in range(gh):
    for x in range(gw):
        if used[y][x]:
            for dy in range(-MARGIN, MARGIN + 1):
                for dx in range(-MARGIN, MARGIN + 1):
                    if 0 <= y + dy < gh and 0 <= x + dx < gw: free[y + dy][x + dx] = False

def place(wb, hb):
    """first-fit search for a wb x hb block rectangle of free cells"""
    for y in range(gh - hb):
        x = 0
        while x < gw - wb:
            ok = True
            for yy in range(y, y + hb):
                row = free[yy]
                for xx in range(x, x + wb):
                    if not row[xx]: ok = False; x = xx + 1; break
                if not ok: break
            if ok:
                for yy in range(y, y + hb):
                    for xx in range(x, x + wb): free[yy][xx] = False
                return x * BLK, y * BLK
    return None

def uv(ax, ay):  # atlas pixel -> OBJ uv (v flipped)
    return ax / AW, 1.0 - ay / AH

# palette swatches for side colours: 8x8 px each, sample centre
swatch_cache = {}
def swatch(rgb):
    key = tuple(int(c) // 6 * 6 for c in rgb)
    if key in swatch_cache: return swatch_cache[key]
    p = place(1, 1)
    if p is None: raise SystemExit('atlas full (swatches)')
    for yy in range(p[1], p[1] + BLK):
        for xx in range(p[0], p[0] + BLK): px[xx, yy] = key
    swatch_cache[key] = uv(p[0] + BLK / 2, p[1] + BLK / 2)
    return swatch_cache[key]

# ---------------------------------------------------------------- mesh
WRITERS = {}; COOKIES = set(); MAINW = {}
class Mesh:
    def __init__(s): s.v = []; s.vt = []; s.vn = []; s.f = []
    def quad(s, p, n, t):
        """p: 4 points (CCW seen from outside), t: 4 uvs"""
        base_v = len(s.v); base_t = len(s.vt); s.v += p; s.vt += t
        s.vn.append(n); ni = len(s.vn)
        a, b, c, d = [base_v + i + 1 for i in range(4)]
        ta, tb, tc, td = [base_t + i + 1 for i in range(4)]
        s.f.append(((a, ta, ni), (b, tb, ni), (c, tc, ni)))
        s.f.append(((a, ta, ni), (c, tc, ni), (d, td, ni)))
    def write(s, path, name):
        WRITERS[os.path.basename(path).lower()] = sys._getframe(1).f_lineno      # which line wrote each file last (finds plain sprite slabs)
        with open(path, 'w') as fo:
            fo.write('# PAIso generated model: %s\n\ng %s\n' % (name, name))
            for x, y, z in s.v: fo.write('v %.4f %.4f %.4f\n' % (x, y, z))
            for u, w in s.vt: fo.write('vt %.5f %.5f 0.0000\n' % (u, w))
            for x, y, z in s.vn: fo.write('vn %.4f %.4f %.4f\n' % (x, y, z))
            for tri in s.f: fo.write('f ' + ' '.join('%d/%d/%d' % c for c in tri) + '\n')

def body_colour(img):
    """dominant non-outline colour of a sprite"""
    px = [p for p in zip(*[iter(img.convert('RGBA').tobytes())] * 4) if p[3] > 128]
    px = [p for p in px if (p[0] * 299 + p[1] * 587 + p[2] * 114) / 1000 > 45] or px
    if not px: return (128, 128, 128)
    # coarse histogram mode
    from collections import Counter
    q = Counter((p[0] // 32, p[1] // 32, p[2] // 32) for p in px).most_common(1)[0][0]
    sel = [p for p in px if (p[0] // 32, p[1] // 32, p[2] // 32) == q]
    return tuple(sum(p[i] for p in sel) // len(sel) for i in range(3))

def shade(rgb, k): return tuple(max(0, min(255, int(c * k))) for c in rgb)

def cookie(name, w, h, img, height_tiles, tex_origin, tex_scale):
    """Extrude the sprite silhouette.  Model x <- sprite x, model -z <- sprite top."""
    W, H = img.size
    gx, gy = max(1, W // GRID_PX), max(1, H // GRID_PX)
    small = img.resize((gx, gy), Image.BOX)
    a = small.load()
    solid = [[a[x, y][3] > 110 for x in range(gx)] for y in range(gy)]
    if sum(map(sum, solid)) == 0: return None
    sx = w * UNITS_PER_CELL / gx            # model units per grid column
    sz = h * UNITS_PER_CELL / gy
    x0 = -w * UNITS_PER_CELL / 2; z0 = -h * UNITS_PER_CELL / 2
    Ht = height_tiles * 100.0
    m = Mesh()
    ox, oy = tex_origin
    def tuv(px_, py_):  # source-sprite pixel -> atlas uv
        return uv(ox + px_ * tex_scale, oy + py_ * tex_scale)
    # low-poly: close pin-holes so the outline has fewer stair-steps
    if gx * gy > 40:
        for _ in range(2):
            fill = []
            for y in range(1, gy - 1):
                for x in range(1, gx - 1):
                    if not solid[y][x] and (solid[y-1][x] + solid[y+1][x] + solid[y][x-1] + solid[y][x+1]) >= 3:
                        fill.append((y, x))
            for y, x in fill: solid[y][x] = True
    cells = [(x, y) for y in range(gy) for x in range(gx) if solid[y][x]]
    bx0 = min(c[0] for c in cells); bx1 = max(c[0] for c in cells) + 1
    by0 = min(c[1] for c in cells); by1 = max(c[1] for c in cells) + 1
    if len(cells) >= 0.80 * (bx1 - bx0) * (by1 - by0) and (bx1 - bx0) * (by1 - by0) >= 6:
        # mostly rectangular -> clean chamfered block with the sprite art on top
        X0, X1 = x0 + bx0 * sx, x0 + bx1 * sx
        Z0, Z1 = z0 + by0 * sz, z0 + by1 * sz
        band = min(Ht * 0.16, 9.0)
        ins = min(band * 0.9, (X1 - X0) * 0.12, (Z1 - Z0) * 0.12)
        bc = body_colour(img)
        body = swatch(shade(bc, 0.80)); lip = swatch(shade(bc, 1.12))
        Yb = Ht - band
        m.quad([(X0, 0, Z1), (X1, 0, Z1), (X1, Yb, Z1), (X0, Yb, Z1)], (0, 0, 1), [body] * 4)
        m.quad([(X1, 0, Z0), (X0, 0, Z0), (X0, Yb, Z0), (X1, Yb, Z0)], (0, 0, -1), [body] * 4)
        m.quad([(X0, 0, Z0), (X0, 0, Z1), (X0, Yb, Z1), (X0, Yb, Z0)], (-1, 0, 0), [body] * 4)
        m.quad([(X1, 0, Z1), (X1, 0, Z0), (X1, Yb, Z0), (X1, Yb, Z1)], (1, 0, 0), [body] * 4)
        k = 0.7071
        m.quad([(X0, Yb, Z1), (X1, Yb, Z1), (X1 - ins, Ht, Z1 - ins), (X0 + ins, Ht, Z1 - ins)], (0, k, k), [lip] * 4)
        m.quad([(X1, Yb, Z0), (X0, Yb, Z0), (X0 + ins, Ht, Z0 + ins), (X1 - ins, Ht, Z0 + ins)], (0, k, -k), [lip] * 4)
        m.quad([(X0, Yb, Z0), (X0, Yb, Z1), (X0 + ins, Ht, Z1 - ins), (X0 + ins, Ht, Z0 + ins)], (-k, k, 0), [lip] * 4)
        m.quad([(X1, Yb, Z1), (X1, Yb, Z0), (X1 - ins, Ht, Z0 + ins), (X1 - ins, Ht, Z1 - ins)], (k, k, 0), [lip] * 4)
        p0, p1 = bx0 * GRID_PX, bx1 * GRID_PX; q0, q1 = by0 * GRID_PX, by1 * GRID_PX
        m.quad([(X0 + ins, Ht, Z1 - ins), (X1 - ins, Ht, Z1 - ins), (X1 - ins, Ht, Z0 + ins), (X0 + ins, Ht, Z0 + ins)], (0, 1, 0),
               [tuv(p0, q1), tuv(p1, q1), tuv(p1, q0), tuv(p0, q0)])
        if Ht >= 30 and (X1 - X0) > 24 and (Z1 - Z0) > 24 and re.search(r'cabinet|locker|fridge|freezer|machine|dispenser|shelf|cupboard|drawers|generator|station|safe|rack|capacitor|oven|cooker|washer|kiln|boiler|chest|dryer|router|transformer|fan', name, re.I):       # cabinet-like blocks get detail: dark plinth, door seam, handle, side grooves
            dark = swatch(shade(bc, 0.45)); mid = swatch(shade(bc, 0.65)); pl = min(8.0, Ht * 0.1)
            box(m, X0 - 0.8, X1 + 0.8, 0, pl, Z0 - 0.8, Z1 + 0.8, dark)
            cxm = (X0 + X1) / 2
            box(m, cxm - 0.7, cxm + 0.7, pl + 3, Yb - 3, Z1, Z1 + 0.9, mid)
            box(m, cxm - 7, cxm - 4, Ht * 0.42, Ht * 0.58, Z1, Z1 + 3.0, swatch((214, 216, 220)))
            box(m, cxm + 4, cxm + 7, Ht * 0.42, Ht * 0.58, Z1, Z1 + 3.0, swatch((214, 216, 220)))
            for gy in (pl + (Yb - pl) * 0.33, pl + (Yb - pl) * 0.66):
                box(m, X1, X1 + 0.8, gy, gy + 1.4, Z0 + 4, Z1 - 4, mid); box(m, X0 - 0.8, X0, gy, gy + 1.4, Z0 + 4, Z1 - 4, mid)
        return m
    # greedy merge rows into runs for the top
    for y in range(gy):
        x = 0
        while x < gx:
            if not solid[y][x]: x += 1; continue
            x1 = x
            while x1 < gx and solid[y][x1]: x1 += 1
            X0, X1 = x0 + x * sx, x0 + x1 * sx
            Z0, Z1 = z0 + y * sz, z0 + (y + 1) * sz
            p0, p1 = x * GRID_PX, x1 * GRID_PX
            q0, q1 = y * GRID_PX, (y + 1) * GRID_PX
            m.quad([(X0, Ht, Z1), (X1, Ht, Z1), (X1, Ht, Z0), (X0, Ht, Z0)], (0, 1, 0),
                   [tuv(p0, q1), tuv(p1, q1), tuv(p1, q0), tuv(p0, q0)])
            x = x1
    # sides on silhouette boundaries, coloured from the edge texel, with a light bevel band on top
    band = min(Ht * 0.12, 6.0)
    bc = body_colour(img)
    body_uv = swatch(shade(bc, 0.82)); lip_uv = swatch(shade(bc, 1.08))
    def side(xa, za, xb, zb, n, rgb):
        body, lip = body_uv, lip_uv
        m.quad([(xa, 0, za), (xb, 0, zb), (xb, Ht - band, zb), (xa, Ht - band, za)], n, [body] * 4)
        m.quad([(xa, Ht - band, za), (xb, Ht - band, zb), (xb, Ht, zb), (xa, Ht, za)], n, [lip] * 4)
    for y in range(gy):
        for x in range(gx):
            if not solid[y][x]: continue
            c = a[x, y][:3]
            X0, X1 = x0 + x * sx, x0 + (x + 1) * sx
            Z0, Z1 = z0 + y * sz, z0 + (y + 1) * sz
            if y == 0 or not solid[y - 1][x]: side(X1, Z0, X0, Z0, (0, 0, -1), c)
            if y == gy - 1 or not solid[y + 1][x]: side(X0, Z1, X1, Z1, (0, 0, 1), c)
            if x == 0 or not solid[y][x - 1]: side(X0, Z0, X0, Z1, (-1, 0, 0), c)
            if x == gx - 1 or not solid[y][x + 1]: side(X1, Z1, X1, Z0, (1, 0, 0), c)
    return m

def box(m, X0, X1, Y0, Y1, Z0, Z1, uvc):
    t = [uvc] * 4
    m.quad([(X0, Y1, Z1), (X1, Y1, Z1), (X1, Y1, Z0), (X0, Y1, Z0)], (0, 1, 0), t)
    m.quad([(X0, Y0, Z1), (X1, Y0, Z1), (X1, Y1, Z1), (X0, Y1, Z1)], (0, 0, 1), t)
    m.quad([(X1, Y0, Z0), (X0, Y0, Z0), (X0, Y1, Z0), (X1, Y1, Z0)], (0, 0, -1), t)
    m.quad([(X0, Y0, Z0), (X0, Y0, Z1), (X0, Y1, Z1), (X0, Y1, Z0)], (-1, 0, 0), t)
    m.quad([(X1, Y0, Z1), (X1, Y0, Z0), (X1, Y1, Z0), (X1, Y1, Z1)], (1, 0, 0), t)

def table(name, w, h, img, height_tiles, tex_origin, tex_scale, legs=True):
    """textured top slab + four legs (tables, desks, benches)"""
    W, H = img.size
    X = w * UNITS_PER_CELL / 2 * 0.96; Z = h * UNITS_PER_CELL / 2 * 0.96
    Ht = height_tiles * 100.0; th = min(10.0, Ht * 0.18)
    bc = body_colour(img)
    edge = swatch(shade(bc, 0.85)); leg = swatch(shade(bc, 0.6))
    ox, oy = tex_origin
    t = [uv(ox, oy + H * tex_scale), uv(ox + W * tex_scale, oy + H * tex_scale), uv(ox + W * tex_scale, oy), uv(ox, oy)]
    m = Mesh()
    m.quad([(-X, Ht, Z), (X, Ht, Z), (X, Ht, -Z), (-X, Ht, -Z)], (0, 1, 0), t)
    for (xa, za, xb, zb, n) in [(-X, Z, X, Z, (0, 0, 1)), (X, -Z, -X, -Z, (0, 0, -1)), (-X, -Z, -X, Z, (-1, 0, 0)), (X, Z, X, -Z, (1, 0, 0))]:
        m.quad([(xa, Ht - th, za), (xb, Ht - th, zb), (xb, Ht, zb), (xa, Ht, za)], n, [edge] * 4)
    m.quad([(-X, Ht - th, -Z), (X, Ht - th, -Z), (X, Ht - th, Z), (-X, Ht - th, Z)], (0, -1, 0), [edge] * 4)
    if legs:
        l = min(7.0, X * 0.15, Z * 0.15)
        for sx in (-1, 1):
            for sz in (-1, 1):
                cx, cz = sx * (X - l * 1.5), sz * (Z - l * 1.5)
                box(m, cx - l, cx + l, 0, Ht - th, cz - l, cz + l, leg)
    return m

def region_colour(img, x0, y0, x1, y1, fallback):
    W, H = img.size
    c = img.crop((int(x0 * W), int(y0 * H), int(x1 * W), int(y1 * H)))
    try: return body_colour(c)
    except Exception: return fallback

QUALITY = 1.0          # more segments on every round shape
def lathe(m, profile, segs, uvc, cx=0.0, cz=0.0, sx=1.0, sz=1.0, mapuv=None):
    """surface of revolution around Y. profile: [(radius, y), ...] bottom->top"""
    segs = max(6, min(28, int(round(segs * QUALITY))))
    for i in range(len(profile) - 1):
        (r0, y0), (r1, y1) = profile[i], profile[i + 1]
        for k in range(segs):
            a0 = 2 * math.pi * k / segs; a1 = 2 * math.pi * (k + 1) / segs
            p = [(cx + r0 * math.cos(a0) * sx, y0, cz + r0 * math.sin(a0) * sz),
                 (cx + r0 * math.cos(a1) * sx, y0, cz + r0 * math.sin(a1) * sz),
                 (cx + r1 * math.cos(a1) * sx, y1, cz + r1 * math.sin(a1) * sz),
                 (cx + r1 * math.cos(a0) * sx, y1, cz + r1 * math.sin(a0) * sz)]
            am = (a0 + a1) / 2; dr = r0 - r1; dy = y1 - y0
            n = (math.cos(am) * dy, dr, math.sin(am) * dy); nl = math.sqrt(sum(c * c for c in n)) or 1
            # winding: make the quad face outward
            m.quad([p[0], p[3], p[2], p[1]], tuple(c / nl for c in n), [mapuv(*p[0]), mapuv(*p[3]), mapuv(*p[2]), mapuv(*p[1])] if mapuv else [uvc] * 4)
    top_r, top_y = profile[-1]
    if top_r > 0.01:
        for k in range(segs):
            a0 = 2 * math.pi * k / segs; a1 = 2 * math.pi * (k + 1) / segs
            tp = [(cx, top_y, cz), (cx + top_r * math.cos(a1) * sx, top_y, cz + top_r * math.sin(a1) * sz),
                  (cx + top_r * math.cos(a0) * sx, top_y, cz + top_r * math.sin(a0) * sz), (cx, top_y, cz)]
            m.quad(tp, (0, 1, 0), [mapuv(*q) for q in tp] if mapuv else [uvc] * 4)

def _people_colours(img):
    """(body, skin, hair/hat, face_only) read from a character sprite, ignoring the black outline and transparent pixels"""
    W, H = img.size; sm = img.resize((48, 48), Image.BOX); px = sm.load()
    pts = [(x, y, px[x, y]) for y in range(48) for x in range(48) if px[x, y][3] > 150]
    if not pts: return (90, 90, 100), (230, 190, 150), (90, 70, 50), False
    y0 = min(p[1] for p in pts); y1 = max(p[1] for p in pts); hh = max(1, y1 - y0)
    def is_skin(c): r, g, b = c[:3]; return r > 150 and r >= g >= b and 25 < r - b < 140 and g > 90
    def is_line(c): return c[0] + c[1] + c[2] < 150
    head = [p for p in pts if p[1] < y0 + hh * 0.45 and not is_line(p[2])]
    body = [p for p in pts if p[1] > y0 + hh * 0.55 and not is_line(p[2])]
    skin_pts = [p[2][:3] for p in head if is_skin(p[2])]
    def med(cs, fb):
        if not cs: return fb
        cs = sorted(cs, key=lambda c: c[0] + c[1] + c[2]); c = cs[len(cs) // 2]; return (c[0], c[1], c[2])
    skin = med(skin_pts, (232, 190, 150))
    top = med([p[2][:3] for p in pts if p[1] < y0 + hh * 0.28 and not is_line(p[2]) and not is_skin(p[2])], (90, 70, 50))
    body_nonskin = [p[2][:3] for p in body if not is_skin(p[2])]
    face_only = len(body_nonskin) < 0.35 * max(1, len(body))          # a close-up face (no clothes in the sprite)
    bodyc = med(body_nonskin, None) if not face_only else None
    if bodyc is None: bodyc = tuple(max(0, int(v * 0.55 + 30)) for v in (top[2], top[0], top[1]))      # no clothes drawn: a coat in a colour taken from the hat / hair
    return bodyc, skin, top, face_only



def bust(name, img, ov=None):
    """a smooth, simple character in one of five body shapes measured from the stock people, with an outfit and hairstyle chosen from the role in its name"""
    body, skin, top, face_only = _people_colours(img)
    if ov: body = ov.get('body', body); skin = ov.get('skin', skin); top = ov.get('top', top)
    n = name.lower(); sd = sum(ord(ch) * (i + 3) for i, ch in enumerate(n))
    bare = re.match(r'prisonernaked', n) is not None
    if re.match(r'(prisonernaked|tattoobody|femalebody)', n):                              # these sprites are blank white placeholders: choose skin, hair and (for visitors) clothes ourselves
        skin = [(250, 200, 150), (206, 140, 96), (150, 100, 70), (246, 214, 186)][sd % 4]; top = [(74, 50, 34), (30, 28, 30), (150, 110, 60), (190, 160, 100), (110, 70, 40)][(sd // 4) % 5]
        if not bare: body = [(70, 96, 140), (110, 70, 76), (70, 110, 86), (150, 126, 80), (96, 86, 120), (60, 64, 72)][(sd // 9) % 6]
    lum = lambda c: 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
    if re.match(r'(fig|ceo|detective|edward|morgan|nico|kerry|benedict|heller|hurst|keane|negotiator|thedon|anthony|sonnyirl|cameraman|newsreporter)', n) and not re.search(r'guard', n):      # close-up face sprites carry no clothes: pick a varied coat colour
        body = [(70, 96, 140), (110, 70, 76), (70, 110, 86), (150, 126, 80), (96, 86, 120), (60, 64, 72), (150, 90, 60), (90, 120, 140), (120, 100, 90), (80, 80, 100)][(sd // 3) % 10]
    SKINS = [(250, 205, 160), (232, 180, 135), (206, 150, 108), (170, 118, 82), (126, 86, 60), (244, 214, 190)]
    HAIRS = [(40, 28, 20), (74, 50, 34), (110, 70, 40), (150, 110, 60), (206, 176, 110), (30, 28, 30), (172, 172, 176), (140, 66, 38)]
    COATS = [(58, 74, 106), (96, 66, 70), (150, 150, 152), (120, 104, 76), (70, 70, 84), (60, 62, 68), (118, 84, 62), (86, 100, 116), (104, 92, 84), (168, 156, 140), (140, 120, 112), (78, 92, 130), (46, 52, 70), (130, 96, 80)]
    guardish = re.search(r'guard|security|soldier|armour|armed|riot|sniper|elite|bounty|henchman|handler', n) is not None
    if not bare: skin = SKINS[sd % 6]                                                    # human skin tones only
    top = HAIRS[(sd // 4) % 8]                                                           # natural hair only
    if guardish: body = (46, 112, 132)                                                   # the game's guard teal, never military green
    elif not re.search(r'prisoner|zombie|insane|doctor|psych|therap|shrink|paramedic|nurse|orderly|fire|truck|gritter|repair|water|pest|worker|workman|builder', n): body = COATS[(sd // 9) % len(COATS)]   # everyday clothes in muted colours
    SKINS = [(250, 205, 160), (232, 180, 135), (206, 150, 108), (170, 118, 82), (126, 86, 60), (244, 214, 190)]
    HAIRS = [(40, 28, 20), (74, 50, 34), (110, 70, 40), (150, 110, 60), (206, 176, 110), (30, 28, 30), (172, 172, 176), (140, 66, 38)]
    COATS = [(58, 74, 106), (96, 66, 70), (150, 150, 152), (120, 104, 76), (70, 70, 84), (60, 62, 68), (118, 84, 62), (86, 100, 116), (104, 92, 84), (168, 156, 140), (140, 120, 112), (78, 92, 130), (46, 52, 70), (130, 96, 80)]
    guardish = re.search(r'guard|security|soldier|armour|armed|riot|sniper|elite|bounty|henchman|handler', n) is not None
    if not bare: skin = SKINS[sd % 6]                                                    # human skin tones only
    top = HAIRS[(sd // 4) % 8]                                                           # natural hair only
    if guardish: body = (46, 112, 132)                                                   # the game's guard teal, never military green
    elif not re.search(r'prisoner|zombie|insane|doctor|psych|therap|shrink|paramedic|nurse|orderly|fire|truck|gritter|repair|water|pest|worker|workman|builder', n): body = COATS[(sd // 9) % len(COATS)]   # everyday clothes in muted colours    if lum(body) > 215: body = shade(body, 0.85)
    if lum(body) < 40: body = tuple(min(255, c + 28) for c in body)
    female = re.search(r'female|daughter|wife|kerry|sarah|nurse|girl|lady|woman', n) is not None or (re.match(r'(fig|edward|morgan|nico|henchman)', n) is None and re.search(r'guard|soldier|prisoner|workman|farmer|priest|teacher|parole|detective|ceo|security|elite|gardener|janitor|repair|water|pest|truck|fire', n) is None and sd % 2 == 0)
    ties = [(176, 40, 44), (40, 70, 150), (230, 200, 70), (60, 120, 90), (120, 60, 130)]; shirts = [(236, 232, 220), (220, 232, 240), (240, 226, 200), (210, 222, 206)]
    blouses = [(70, 150, 160), (150, 130, 190), (236, 226, 200), (120, 170, 130), (214, 120, 140), (96, 132, 190)]          # soft colours that cannot be mistaken for the orange prisoner uniform
    if bare: style, outfit, acc, bcol, body = 1, 'briefs', [(240, 240, 236), (96, 130, 190), (190, 70, 70)][sd % 3], skin, skin
    elif re.search(r'prisoner|zombie|insane', n): style, outfit, acc, bcol = 1, 'jumpsuit', None, (226, 128, 36)
    elif re.search(r'doctor|psych|therap|shrink|paramedic|nurse|orderly', n): style, outfit, acc, bcol = 5, 'labcoat', (60, 150, 170), (214, 216, 218)
    elif re.search(r'guard|security|soldier|armour|riot|sniper|elite|bounty|henchman|handler', n): style, outfit, acc, bcol = 2, 'suit', ties[sd % 5], body
    elif re.search(r'fire|truck|gritter|repair|water|pest|worker|workman|builder', n): style, outfit, acc, bcol = 4, ('hivis' if sd % 2 else 'overalls'), shirts[sd % 4], ((214, 226, 70) if sd % 2 else body)
    elif re.search(r'farmer|gardener|janitor|cook|chef', n): style, outfit, acc, bcol = 4, 'overalls', shirts[sd % 4], body
    elif re.search(r'priest|teacher|parole|lawyer|ceo|detective|chief|warden|foreman', n): style, outfit, acc, bcol = 3 if re.search(r'priest', n) else 2, 'suit', ties[sd % 5], body
    else:                                                                                  # visitors and everybody else: any shape except the prisoner's round one
        style = (2, 3, 4, 5)[(sd // 3) % 4]; outfit = ('blouse' if female else 'suit') if sd % 3 else 'overalls'
        acc = ties[sd % 5] if outfit != 'overalls' else shirts[sd % 4]; bcol = body
        if outfit == 'blouse': bcol = blouses[sd % len(blouses)]; acc = (236, 232, 220)
        elif lum(bcol) > 120 and (bcol[0] - bcol[2]) > 70 and bcol[0] > 190: bcol = (90, 100, 130)                         # never an orange-ish coat on a visitor
    pool = (['bob', 'long', 'pony', 'bun', 'pigtails', 'fringe', 'bob', 'cap', 'long', 'cap'] if female else ['cap', 'quiff', 'receding', 'fringe', 'bald', 'quiff', 'cap', 'receding', 'fringe', 'cap', 'quiff', 'fringe'])
    hair = pool[(sd // 5 + sd // 13) % len(pool)]
    extras = ()
    if not female and not bare: extras = tuple(e for e, ok in (('beard', sd % 5 == 0), ('stache', sd % 7 == 1), ('glasses', sd % 4 == 1)) if ok)
    if female and sd % 5 == 1: extras = ('glasses',)
    hat = shade(bcol, 0.78) if (outfit == 'suit' and style == 2 and re.search(r'guard|security|soldier|armour|riot|elite|handler', n)) else None
    if hat: hair = 'cap'; extras = tuple(e for e in extras if e != 'beard')
    return person(style, outfit, bcol, acc, skin, top, hair, hat, extras)
def _blob(m, cx, cy, cz, r, col, sq=0.85, segs=8, mapuv=None):
    """low-poly squashed sphere"""
    prof = [(r * 0.2, cy - r * sq * 0.98)] + [(r * math.cos(math.radians(a)), cy + r * sq * math.sin(math.radians(a))) for a in ((-45, 0, 45) if r < 8 else (-60, -30, 0, 30, 60))] + [(r * 0.25, cy + r * sq * 0.97)]
    lathe(m, prof, segs, None if mapuv else swatch(col), cx=cx, cz=cz, mapuv=mapuv)

def _cone(m, cx, y0, cz, r, hgt, col, segs=8):
    lathe(m, [(r, y0), (r * 0.8, y0 + hgt * 0.35), (r * 0.45, y0 + hgt * 0.7), (0.0, y0 + hgt)], segs, swatch(col), cx=cx, cz=cz)

def tree(name, w, h, img):
    """Rounded low-poly vegetation: round trees (with fruit), conifers, bushes and topiary, crop tufts, saplings, stumps and cacti, tinted from the sprite."""
    n = name.lower(); R = min(w, h) * UNITS_PER_CELL / 2
    try: avg = body_colour(img)
    except Exception: avg = (70, 140, 60)
    if not (avg[1] >= avg[0] and avg[1] >= avg[2]): avg = (avg[0] // 2 + 40, max(avg[1], 130), avg[2] // 2 + 30)
    sd = sum(ord(ch) for ch in n); m = Mesh()
    young = re.search(r'young|sapling', n) is not None
    k = 0.55 if young else 1.0
    bark = (112, 78, 48); bark2 = (150, 112, 70)
    def trunk(h_, r_):
        lathe(m, [(r_ * 1.3, 0), (r_, h_ * 0.4), (r_ * 0.8, h_)], 7, swatch(bark))
    if re.search(r'stump', n):
        lathe(m, [(R * 0.20, 0), (R * 0.17, R * 0.30), (R * 0.14, R * 0.34)], 8, swatch(bark)); lathe(m, [(R * 0.14, R * 0.34), (R * 0.14, R * 0.35)], 8, swatch(bark2)); return m
    if re.search(r'cactus', n):
        g = (74, 150, 84); lathe(m, [(R * 0.16, 0), (R * 0.2, R * 0.5), (R * 0.14, R * 1.0), (0, R * 1.1)], 8, swatch(g))
        for sx_ in (-1, 1): box(m, sx_ * R * 0.14 - 4, sx_ * R * 0.14 + 4, R * 0.4, R * 0.5, -4, 4, swatch(g)); box(m, sx_ * R * 0.26 - 4, sx_ * R * 0.26 + 4, R * 0.4, R * (0.75 + 0.1 * sx_), -4, 4, swatch(g))
        return m
    if re.search(r'conifer|pine|spruce|snowy', n):
        snow = re.search(r'snowy', n) is not None; dk = shade(avg, 0.75)
        trunk(R * 0.5 * k, R * 0.1)
        for i_, (rr, yy, hh) in enumerate(((0.95, R * 0.3 * k, 0.75), (0.72, R * 0.75 * k, 0.7), (0.5, R * 1.15 * k, 0.65))):
            _cone(m, 0, yy, 0, R * rr * k, R * hh * k, (236, 240, 244) if (snow and i_ == 2) else shade(dk, 1.0 + 0.1 * i_))
        return m
    if re.search(r'crop|barley|wheat|corn|cabbage|carrot|potato|tomato|flowerstage|flower(?!planter)', n):
        cols = {'barley': (210, 180, 90), 'wheat': (226, 196, 100), 'corn': (120, 170, 60), 'tomato': (86, 150, 60), 'flower': (230, 110, 140)}
        base = next((v for kk, v in cols.items() if kk in n), avg)
        step = R * 0.5
        for ix in (-1, 0, 1):
            for iz in (-1, 0, 1):
                jx, jz = ((sd + ix * 7 + iz * 13) % 9 - 4) * R * 0.02, ((sd * 3 + ix * 5 + iz * 11) % 9 - 4) * R * 0.02
                hh = R * (0.45 + 0.12 * ((ix + iz + sd) % 3)) * (1.6 if 'corn' in n else 1.0)
                _cone(m, ix * step + jx, 0, iz * step + jz, R * 0.17, hh, shade(base, 0.9 + 0.1 * ((ix + iz) % 2)), segs=6)
                if re.search(r'flower', n): _blob(m, ix * step + jx, hh, iz * step + jz, R * 0.12, (240, 210, 70), 0.7, 6)
        return m
    if re.search(r'bush|fern|border|ivy|hedge|topiary|planter|willow|vines', n):
        rr = R * 0.62 * k
        if 'topiary' in n:
            trunk(R * 0.3, R * 0.06); _blob(m, 0, R * 0.3 + rr * 0.7, 0, rr * 0.75, shade(avg, 1.1), 0.95, 9)
            if re.search(r'cat|dog|deer|rex|swan|warden', n): _blob(m, rr * 0.35, R * 0.3 + rr * 1.5, 0, rr * 0.38, shade(avg, 1.2), 0.9, 7)
        else:
            for ox_, oz_, s_ in ((0, 0, 1.0), (rr * 0.55, rr * 0.2, 0.7), (-rr * 0.5, -rr * 0.25, 0.75)):
                _blob(m, ox_, rr * 0.55 * s_, oz_, rr * s_, shade(avg, 0.9 + 0.12 * s_), 0.75, 8)
        return m
    # round tree: trunk + three overlapping canopy blobs; fruit trees get coloured fruit
    ch = R * 0.62 * k
    trunk(ch * 1.05, R * 0.11 * k)
    cr = R * 0.58 * k
    blobs = ((0, ch + cr * 0.55, 0, cr, 1.12), (cr * 0.55, ch + cr * 0.35, cr * 0.25, cr * 0.72, 0.95), (-cr * 0.45, ch + cr * 0.4, -cr * 0.3, cr * 0.76, 0.85))
    if 'sakura' in n or 'cherry' in n: avg = (236, 150, 176)
    elif 'spooky' in n: avg = (110, 80, 120)
    for bx, by, bz, br, sh_ in blobs: _blob(m, bx, by, bz, br, shade(avg, sh_), 0.88, 9)
    fr = next((c_ for kk, c_ in (('apple', (210, 50, 44)), ('orange', (240, 150, 40)), ('banana', (240, 214, 70)), ('peach', (244, 160, 130))) if kk in n), None)
    if fr and not young:
        for i_ in range(7):
            ang = i_ * 2.4 + sd; rr_ = cr * (0.95 - 0.12 * (i_ % 3)); _blob(m, math.cos(ang) * rr_, ch + cr * (0.35 + 0.16 * (i_ % 4)), math.sin(ang) * rr_, cr * 0.16, fr, 1.0, 6)
    return m
# ---------------------------------------------------------------- hand-built archetypes
# Real multi-part models for the objects people look at most. Built in sprite space:
# model x <- sprite x, model -z <- sprite top (so the cistern / pillow / chair back sits at -z).
def solid_bbox(img):
    W, H = img.size
    bb = img.split()[3].point(lambda a: 255 if a > 110 else 0).getbbox() or (0, 0, W, H)
    return bb[0] / W, bb[1] / H, bb[2] / W, bb[3] / H

def frame_of(w, h, img, shrink=1.0):
    u0, v0, u1, v1 = solid_bbox(img)
    X0 = (u0 - 0.5) * w * UNITS_PER_CELL * 2 * 0.5 * 2 / 2   # = (u0-0.5)*w*50
    X0 = (u0 - 0.5) * w * UNITS_PER_CELL; X1 = (u1 - 0.5) * w * UNITS_PER_CELL
    Z0 = (v0 - 0.5) * h * UNITS_PER_CELL; Z1 = (v1 - 0.5) * h * UNITS_PER_CELL
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    hx, hz = (X1 - X0) / 2 * shrink, (Z1 - Z0) / 2 * shrink
    return cx - hx, cx + hx, cz - hz, cz + hz

WHITE = (236, 236, 232); PORC = (214, 216, 216); WATER = (64, 56, 50); STEEL = (120, 124, 130); DARK = (58, 60, 66)


def arch_sink(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = lambda r: swatch(r)
    d = Z1 - Z0; wd = X1 - X0; cx = (X0 + X1) / 2
    box(m, cx - wd * 0.14, cx + wd * 0.14, 0, 66, Z0 + d * 0.25, Z1 - d * 0.25, c(PORC))   # pedestal
    box(m, X0, X1, 66, 88, Z0, Z1, c(WHITE))                                              # basin block
    box(m, X0 + wd * 0.16, X1 - wd * 0.16, 88, 89, Z0 + d * 0.30, Z1 - d * 0.14, c((150, 156, 164)))  # bowl
    box(m, cx - 3, cx + 3, 89, 106, Z0 + d * 0.10, Z0 + d * 0.10 + 6, c(STEEL))           # tap
    return m

def arch_bed(w, h, img, levels=1):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = lambda r: swatch(r)
    d = Z1 - Z0; wd = X1 - X0
    blanket = region_colour(img, 0.1, 0.45, 0.9, 0.8, (120, 150, 100))
    sheet = (232, 234, 228)
    post = 5.0
    top = 16 + 72 * (levels - 1) + 30
    for sx in (X0, X1 - post * 2):
        for sz in (Z0, Z1 - post * 2):
            box(m, sx, sx + post * 2, 0, top + (14 if levels > 1 else 0) , sz, sz + post * 2, c(DARK))   # corner posts
    for lv in range(levels):
        y = 16 + 72 * lv
        box(m, X0 + post, X1 - post, y, y + 10, Z0 + post, Z1 - post, c(STEEL))                       # frame
        box(m, X0 + post * 1.2, X1 - post * 1.2, y + 10, y + 26, Z0 + post * 1.2, Z1 - post * 1.2, c(sheet))   # mattress
        box(m, X0 + post * 1.2, X1 - post * 1.2, y + 26, y + 28, Z0 + d * 0.28, Z1 - post * 1.2, c(blanket))   # blanket
        box(m, X0 + wd * 0.15, X1 - wd * 0.15, y + 26, y + 34, Z0 + post * 1.4, Z0 + d * 0.22, c((250, 250, 250)))  # pillow
        box(m, X0 + post, X1 - post, y + 10, y + 36, Z0, Z0 + post, c(STEEL))                         # headboard
        box(m, X0 + post, X1 - post, y + 10, y + 28, Z1 - post, Z1, c(STEEL))                          # footboard
    return m


def body_of(img, fb=(150, 150, 150)):
    try: return body_colour(img)
    except Exception: return fb

def gen_cabinet(w, h, img, H):
    """lockers, fridges, shelves, vending: plinth + body + projecting cornice"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; bc = body_of(img)
    pl = min(8.0, H * 0.08); cap = min(7.0, H * 0.07); ins = min(3.0, (X1 - X0) * 0.04)
    box(m, X0 + ins, X1 - ins, 0, pl, Z0 + ins, Z1 - ins, c(shade(bc, 0.45)))
    box(m, X0, X1, pl, H - cap, Z0, Z1, c(shade(bc, 0.85)))
    box(m, X0 - 1.5, X1 + 1.5, H - cap, H, Z0 - 1.5, Z1 + 1.5, c(shade(bc, 1.12)))
    # door seam + handle on the front (+z)
    cx = (X0 + X1) / 2
    box(m, cx - 0.8, cx + 0.8, pl + 4, H - cap - 4, Z1, Z1 + 1.0, c(shade(bc, 0.45)))
    box(m, cx - 6, cx - 3, H * 0.45, H * 0.6, Z1, Z1 + 3.0, c((210, 212, 216)))
    return m

def gen_appliance(w, h, img, H):
    """washers, ovens, boilers: feet + body + control strip + round door"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; bc = body_of(img)
    ft = 5.0
    for sx in (X0 + 3, X1 - 9):
        for sz in (Z0 + 3, Z1 - 9): box(m, sx, sx + 6, 0, ft, sz, sz + 6, c(DARK))
    box(m, X0, X1, ft, H, Z0, Z1, c(shade(bc, 0.9)))
    box(m, X0, X1, H - 14, H, Z0, Z0 + (Z1 - Z0) * 0.35, c(shade(bc, 0.6)))           # control strip at the back
    cx, cy = (X0 + X1) / 2, (ft + H - 14) / 2; r = min((X1 - X0), H - ft - 14) * 0.32
    box(m, cx - r, cx + r, cy - r, cy + r, Z1, Z1 + 2.5, c((50, 54, 62)))              # door glass
    box(m, cx - r * 0.7, cx + r * 0.7, cy - r * 0.7, cy + r * 0.7, Z1 + 2.5, Z1 + 3.0, c((120, 140, 160)))
    return m

def gen_seat(w, h, img, H, arms=False):
    """sofas, armchairs, stools, pews: base + cushion + back (at -z) + optional arms"""
    X0, X1, Z0, Z1 = frame_of(w, h, img, 0.96)
    m = Mesh(); c = swatch; bc = body_of(img)
    d = Z1 - Z0; wd = X1 - X0
    box(m, X0, X1, 0, 16, Z0, Z1, c(shade(bc, 0.55)))
    box(m, X0 + (wd * 0.12 if arms else 0), X1 - (wd * 0.12 if arms else 0), 16, 34, Z0 + d * 0.22, Z1, c(shade(bc, 1.05)))
    box(m, X0, X1, 16, min(H, 62), Z0, Z0 + d * 0.22, c(shade(bc, 0.85)))
    if arms:
        aw = wd * 0.12
        box(m, X0, X0 + aw, 16, 44, Z0 + d * 0.1, Z1, c(shade(bc, 0.8)))
        box(m, X1 - aw, X1, 16, 44, Z0 + d * 0.1, Z1, c(shade(bc, 0.8)))
    return m

def gen_screen(w, h, img, H):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; bc = body_of(img); cx = (X0 + X1) / 2; cz = (Z0 + Z1) / 2
    box(m, cx - 10, cx + 10, 0, 6, cz - 10, cz + 10, c(DARK))
    box(m, cx - 3, cx + 3, 6, 16, cz - 3, cz + 3, c(DARK))
    box(m, X0, X1, 16, H, cz - 6, cz + 6, c(shade(bc, 0.5)))
    box(m, X0 + 3, X1 - 3, 19, H - 3, cz + 6, cz + 7, c((40, 70, 90)))                 # screen on the front
    return m

def gen_barrel(w, h, img, H):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); bc = body_of(img)
    r = min(X1 - X0, Z1 - Z0) / 2
    lathe(m, [(r * 0.9, 0), (r, 4), (r, H - 4), (r * 0.9, H), (r * 0.8, H)], 10, swatch(shade(bc, 0.9)), cx=(X0 + X1) / 2, cz=(Z0 + Z1) / 2)
    return m

GENERIC = [
    (r'sink|basin', lambda w, h, i, H: arch_sink(w, h, i)),
    (r'washer|dryer|oven|cooker|boiler|generator|press|sawmill|saw$|pump|capacitor|furnace|kiln', gen_appliance),
    (r'locker|cabinet|wardrobe|fridge|freezer|shelf|bookcase|drawer|cupboard|safe$|vending|arcade|rack|dispenser|crate|radio|boombox', gen_cabinet),
    (r'sofa|couch|armchair|settee|sofachair', lambda w, h, i, H: gen_seat(w, h, i, H, True)),
    (r'chair|stool|seat|toolbox', lambda w, h, i, H: gen_seat(w, h, i, H, False)),
    (r'^tv|tv$|television|monitor|tracking', gen_screen),
    (r'barrel|bin$|drum|bucket|keg|extinguisher', gen_barrel),
]
def generic_for(name, sname):
    if re.search(r'table|desk|bench|counter|booth|bed|cot', name + ' ' + sname, re.I): return None
    for pat, fn in GENERIC:
        if re.search(pat, name + ' ' + sname, re.I): return fn
    return None

CHROME = (190, 196, 204)


def arch_servo(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch
    cx = (X0 + X1) / 2; cz = (Z0 + Z1) / 2; wd = X1 - X0; d = Z1 - Z0
    box(m, X0, X1, 0, 14, Z0, Z0 + d * 0.55, c((146, 138, 130)))                              # housing
    lathe(m, [(wd * 0.3, 14), (wd * 0.3, 30), (wd * 0.22, 32)], 10, c((70, 72, 78)), cx=cx, cz=Z0 + d * 0.27)   # motor
    gz = Z0 + d * 0.78
    lathe(m, [(wd * 0.42, 0), (wd * 0.46, 3), (wd * 0.42, 6), (wd * 0.46, 9), (wd * 0.4, 12)], 8, c((84, 80, 76)), cx=cx, cz=gz)  # cog
    box(m, cx - 3, cx + 3, 12, 14, Z0 + d * 0.5, gz, c((60, 50, 44)))
    m.v = [(x, y + 150.0, z) for (x, y, z) in m.v]        # door servos sit on top of the door frame, not on the floor
    return m


def _bench(w, h, img, wood=(244, 244, 246), wood2=(228, 230, 234), iron=(70, 96, 128), seats=1, backs=0, plank=7, arms=False, bamboo=False):
    """park-bench style: slatted seat and back, cast-iron end frames with armrests; runs along the long side (z is depth, front = +z)"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; wood2 = wood2 or shade(wood, 0.82)
    if seats == 1 and not backs:                                                         # a single plain slab on thin legs, like the table top
        d = Z1 - Z0; zc = (Z0 + Z1) / 2; sd = min(d * 0.62, 62)
        box(m, X0 + 2, X1 - 2, 42, 47, zc - sd / 2, zc + sd / 2, c(wood))                    # one slab
        for lx in (X0 + 10, X1 - 13):
            for lz in (zc - sd / 2 + 6, zc + sd / 2 - 9): box(m, lx, lx + 3, 0, 42, lz, lz + 3, c((150, 154, 160)))   # thin legs
        return m
    d = Z1 - Z0; zc = (Z0 + Z1) / 2; sd = min(d * 0.52, 50); zf = zc + sd / 2; zb = zc - sd / 2        # seat front / back edge
    sx0, sx1 = X0 + 4, X1 - 4
    gap = 2.0; sl = (sd - gap * (seats - 1)) / seats
    for k in range(seats):                                                           # seat boards, slightly sloped back to front
        z0 = zb + k * (sl + gap); y = 44 - 2 * (1 - k / max(1, seats - 1))
        box(m, sx0, sx1, y, y + plank * 0.8, z0, z0 + sl, c(wood if k % 2 == 0 else wood2))
        if seats == 1:                                                                          # thick moulded plastic slab so it reads as a solid bench from far away
            box(m, sx0, sx1, y + plank * 0.8, y + plank * 0.8 + 3, z0 + 2, z0 + sl - 2, c(wood))     # raised top
            box(m, sx0 + 1, sx1 - 1, y - 9, y, z0 + 4, z0 + sl - 4, c((120, 150, 176)))             # coloured apron under the seat
            box(m, sx0, sx1, y + plank * 0.8 + 3, y + plank * 0.8 + 4.5, z0 + sl * 0.46, z0 + sl * 0.54, c((120, 150, 176)))   # coloured centre stripe
        if bamboo:
            for nx in range(1, int((sx1 - sx0) // 36)):                              # bamboo nodes
                box(m, sx0 + nx * 36, sx0 + nx * 36 + 3, y - 1, y + plank * 0.8 + 1, z0 - 0.5, z0 + sl + 0.5, c(shade(wood, 0.6)))
    bh = 17
    for k in range(backs):                                                           # back boards
        y0 = 58 + k * (bh + 3)
        box(m, sx0, sx1, y0, y0 + bh, zb - 6, zb - 6 + plank * 0.7, c(wood if k % 2 == 0 else wood2))
    for lx in (X0 + 6, X1 - 6 - 7):                                                  # end frames
        box(m, lx, lx + 7, 0, 46, zf - 9, zf - 3, c(iron))                           # front leg
        box(m, lx, lx + 7, 0, 46, zb + 2, zb + 8, c(iron))                           # rear leg
        if backs: box(m, lx, lx + 7, 46, 58 + backs * (bh + 3), zb - 8, zb - 1, c(iron))      # back upright (only when there is a backrest)
        box(m, lx, lx + 7, 14, 20, zb + 2, zf - 3, c(iron))                          # lower stretcher
        if arms:
            box(m, lx - 1, lx + 8, 62, 67, zb - 6, zf, c(iron)); box(m, lx, lx + 7, 46, 62, zf - 6, zf - 2, c(iron))   # armrest + post
    return m
def arch_bench(w, h, img): return _bench(w, h, img)
def bench_small(w, h, img): return _bench(w, h, img)
def arch_visitor(w, h, img):
    """visitor table: a thick laminate top with a darker edge band and apron, chunky legs with feet, and padded stools on chrome stems"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch
    wd = X1 - X0; d = Z1 - Z0
    tz0, tz1 = Z0 + d * 0.27, Z0 + d * 0.78
    lam, edge, leg, chrome = (222, 178, 130), (150, 104, 66), (92, 98, 108), (200, 204, 210)
    box(m, X0, X1, 66, 74, tz0, tz1, c(edge))                                           # top slab (edge colour sides)
    box(m, X0 + 1.5, X1 - 1.5, 74, 75.5, tz0 + 1.5, tz1 - 1.5, c(lam))                  # laminate surface
    box(m, X0 + 4, X1 - 4, 58, 66, tz0 + 4, tz0 + 8, c(edge)); box(m, X0 + 4, X1 - 4, 58, 66, tz1 - 8, tz1 - 4, c(edge))   # apron rails
    for lx in (X0 + 6, X1 - 14):
        for lz in (tz0 + 4, tz1 - 12):
            box(m, lx, lx + 8, 4, 58, lz, lz + 8, c(leg)); box(m, lx - 2, lx + 10, 0, 4, lz - 2, lz + 10, c((60, 64, 72)))   # leg + foot
    for (u, v) in ((0.15, 0.1), (0.47, 0.1), (0.78, 0.1), (0.47, 0.9)):                 # moulded chairs on a steel frame, backs away from the table
        sx, sz = X0 + u * wd, Z0 + v * d; f = -1 if v < 0.5 else 1                          # f: the side the back is on
        seat, seatd, frame_ = c((36, 104, 170)), c((28, 82, 136)), c((80, 86, 96))
        for ox in (-9, 7):
            for oz in (-9, 7): box(m, sx + ox, sx + ox + 2, 0, 40, sz + oz, sz + oz + 2, c(chrome))      # four legs
        box(m, sx - 9, sx + 9, 2, 4, sz - 9, sz + 9, frame_)                                  # foot frame bolted to the floor
        box(m, sx - 11, sx + 11, 40, 44, sz - 11, sz + 11, seatd); box(m, sx - 10, sx + 10, 44, 45.5, sz - 10, sz + 10, seat)   # seat
        bz0 = sz + f * 9 - (2 if f > 0 else 0); box(m, sx - 11, sx + 11, 44, 78, bz0, bz0 + 2.5, seat)     # back rest
        box(m, sx - 9, sx + 9, 72, 76, bz0 - f * 1, bz0 + 2.5 - f * 1, seatd)
    return m

def arch_drum(w, h, img):
    """chemical drum (bleach): green body, ribs, blue lid, yellow hazard label on the front"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    r = min(X1 - X0, Z1 - Z0) / 2
    lathe(m, [(r * 0.92, 0), (r, 3), (r, 54), (r * 0.94, 57)], 10, c((98, 196, 96)), cx=cx, cz=cz)   # body
    lathe(m, [(r * 1.03, 14), (r * 1.03, 18)], 10, c((70, 150, 72)), cx=cx, cz=cz)                    # ribs
    lathe(m, [(r * 1.03, 38), (r * 1.03, 42)], 10, c((70, 150, 72)), cx=cx, cz=cz)
    lathe(m, [(r * 1.0, 57), (r * 1.0, 63), (r * 0.8, 65), (r * 0.2, 65)], 10, c((60, 120, 200)), cx=cx, cz=cz)   # lid
    box(m, cx - r * 0.38, cx + r * 0.38, 20, 36, cz + r * 0.78, cz + r * 1.02, c((252, 214, 30)))      # label
    box(m, cx - r * 0.1, cx + r * 0.1, 24, 32, cz + r * 1.02, cz + r * 1.06, c((30, 30, 30)))
    return m

# ---------------------------------------------------------------- outdoor / utility objects
def oquad(m, pts, centre, uvc):
    """quad with outward-facing winding and normal, decided from the solid's centre"""
    a, b, c = pts[0], pts[1], pts[2]
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]; vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    n = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
    nl = math.sqrt(sum(k * k for k in n)) or 1.0; n = tuple(k / nl for k in n)
    mid = tuple(sum(p[i] for p in pts) / 4.0 for i in range(3))
    if sum(n[i] * (mid[i] - centre[i]) for i in range(3)) < 0: pts = pts[::-1]; n = tuple(-k for k in n)
    m.quad(list(pts), n, [uvc] * 4)

def prism(m, prof, a0, a1, axis, uvc):
    """convex 4-point profile extruded between a0 and a1. axis 'z': profile in (x,y); axis 'x': profile in (z,y)"""
    P = (lambda u, v, a: (u, v, a)) if axis == 'z' else (lambda u, v, a: (a, v, u))
    cen = tuple(sum(P(u, v, (a0 + a1) / 2)[i] for u, v in prof) / 4.0 for i in range(3))
    oquad(m, [P(u, v, a0) for u, v in prof], cen, uvc)
    oquad(m, [P(u, v, a1) for u, v in prof], cen, uvc)
    for i in range(4):
        (u0, v0), (u1, v1) = prof[i], prof[(i + 1) % 4]
        oquad(m, [P(u0, v0, a0), P(u1, v1, a0), P(u1, v1, a1), P(u0, v0, a1)], cen, uvc)

GREY = (150, 154, 160); LAMP = (255, 236, 150)

def arch_lamppost(w, h, img, H=150.0, heads=1, big=False):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    s = 0.5 if H < 100 else 1.0            # the small indoor "Light" is a floor lamp, not a street lamp
    lathe(m, [(9 * s, 0), (9 * s, 4), (4 * s, 8), (3 * s, H)], 8, c(DARK), cx=cx, cz=cz)
    box(m, cx - 14 * s, cx + 14 * s, H, H + 6 * s, cz - 12 * s, cz + 12 * s, c(GREY))
    for i in range(heads):
        ox = (i - (heads - 1) / 2.0) * 22
        box(m, cx + ox - 9 * s, cx + ox + 9 * s, H + 6 * s, H + 22 * s, cz - 9 * s, cz + 9 * s, c(LAMP))
        box(m, cx + ox - 11 * s, cx + ox + 11 * s, H + 22 * s, H + 26 * s, cz - 11 * s, cz + 11 * s, c(GREY))
    return m

def arch_ceiling_light(w, h, img):
    """a ceiling light: a ring fitting fixed at the top of the wall height with a glowing underside; open in the middle so it never blocks the view down into the room"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    r = max(5.0, min(X1 - X0, Z1 - Z0) * 0.15); y = 186.0          # half the size it was
    lathe(m, [(r * 1.15, y), (r * 1.15, y + 5), (r * 0.72, y + 5)], 14, c((150, 152, 158)), cx=cx, cz=cz)      # outer wall + top of the ring
    lathe(m, [(r * 0.72, y + 5), (r * 0.72, y)], 14, c((150, 152, 158)), cx=cx, cz=cz)                          # inner wall
    lathe(m, [(r * 0.74, y - 0.5), (r * 1.13, y - 0.5)], 14, c(LAMP), cx=cx, cz=cz)                             # glowing underside
    box(m, cx - 2, cx + 2, y + 1, y + 5, cz - r * 0.9, cz + r * 0.9, c((150, 152, 158)))                          # two thin spokes
    box(m, cx - r * 0.9, cx + r * 0.9, y + 1, y + 5, cz - 2, cz + 2, c((150, 152, 158)))
    return m
def arch_searchlight(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    lathe(m, [(10, 0), (10, 5), (4, 10), (4, 120)], 8, c(DARK), cx=cx, cz=cz)
    lathe(m, [(22, 120), (22, 150), (18, 154)], 10, c(GREY), cx=cx, cz=cz)
    box(m, cx - 17, cx + 17, 122, 148, cz + 20, cz + 23, c(LAMP))
    return m

def arch_rod(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    lathe(m, [(7, 0), (7, 6), (2.5, 14), (2, 200), (0.5, 240)], 8, c((190, 196, 204)), cx=cx, cz=cz)
    return m

def arch_solarlight(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    lathe(m, [(5, 0), (4, 4), (3, 60)], 8, c(DARK), cx=cx, cz=cz)
    prism(m, [(cz - 14, 60), (cz + 14, 76), (cz + 14, 79), (cz - 14, 63)], cx - 14, cx + 14, 'x', c((40, 70, 140)))
    box(m, cx - 6, cx + 6, 44, 58, cz - 6, cz + 6, c(LAMP))
    return m

def arch_turbine(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    lathe(m, [(14, 0), (14, 4), (8, 260), (7, 262)], 10, c((236, 238, 240)), cx=cx, cz=cz)
    box(m, cx - 9, cx + 9, 258, 276, cz - 20, cz + 14, c((226, 228, 232)))
    hz = cz + 16
    lathe(m, [(0, 0)], 3, c(GREY), cx=cx, cz=cz)   # keeps lathe import path simple
    for ang in (90, 210, 330):
        a = math.radians(ang); dx, dy = math.cos(a), math.sin(a); nx, ny = -dy, dx
        ln, wd = 110.0, 8.0
        cxh, cyh = cx, 267.0
        pts = [(cxh + nx * wd * 0.6, cyh + ny * wd * 0.6), (cxh + dx * ln + nx * 1.5, cyh + dy * ln + ny * 1.5),
               (cxh + dx * ln - nx * 1.5, cyh + dy * ln - ny * 1.5), (cxh - nx * wd * 0.6, cyh - ny * wd * 0.6)]
        prism(m, pts, hz, hz + 3, 'z', c((240, 242, 244)))
    return m


def arch_transformer(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = gen_appliance(w, h, img, 90.0); c = swatch
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    for dx in (-1, 0, 1):
        lathe(m, [(7, 90), (7, 100), (5, 108), (3, 112)], 8, c((196, 120, 70)), cx=cx + dx * (X1 - X0) * 0.28, cz=cz)
    return m

def arch_weather(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = gen_cabinet(w, h, img, 110.0); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    lathe(m, [(0, 118), (20, 124), (26, 134), (22, 136), (0, 128)], 12, c((226, 230, 236)), cx=cx, cz=cz)
    return m

def arch_dummy(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    box(m, cx - 16, cx + 16, 0, 5, cz - 16, cz + 16, c(DARK))
    lathe(m, [(12, 5), (15, 14), (15, 70), (11, 78)], 8, c((196, 150, 90)), cx=cx, cz=cz)
    lathe(m, [(0, 78), (9, 84), (11, 96), (6, 104), (0, 106)], 8, c((214, 170, 110)), cx=cx, cz=cz)
    box(m, cx - 30, cx + 30, 56, 63, cz - 4, cz + 4, c((150, 104, 60)))
    return m

def arch_sprinkler(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    lathe(m, [(9, 0), (9, 5), (4, 7), (3, 14), (5, 16)], 8, c((70, 90, 70)), cx=cx, cz=cz)
    return m

def arch_gate(w, h, img):
    """two posts, top and bottom rails and vertical bars; runs along the longer side"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; H = 120.0
    steel = (96, 100, 108)
    if (X1 - X0) >= (Z1 - Z0):
        zc = (Z0 + Z1) / 2; t = 4.0
        box(m, X0, X0 + 8, 0, H + 8, zc - 5, zc + 5, c(DARK)); box(m, X1 - 8, X1, 0, H + 8, zc - 5, zc + 5, c(DARK))
        box(m, X0 + 8, X1 - 8, H - 10, H, zc - t / 2, zc + t / 2, c(steel)); box(m, X0 + 8, X1 - 8, 12, 20, zc - t / 2, zc + t / 2, c(steel))
        n = max(3, int((X1 - X0) // 14))
        for i in range(n):
            bx = X0 + 12 + (X1 - X0 - 24) * i / max(1, n - 1)
            box(m, bx - 1.5, bx + 1.5, 20, H - 10, zc - 1.5, zc + 1.5, c(steel))
    else:
        xc = (X0 + X1) / 2; t = 4.0
        box(m, xc - 5, xc + 5, 0, H + 8, Z0, Z0 + 8, c(DARK)); box(m, xc - 5, xc + 5, 0, H + 8, Z1 - 8, Z1, c(DARK))
        box(m, xc - t / 2, xc + t / 2, H - 10, H, Z0 + 8, Z1 - 8, c(steel)); box(m, xc - t / 2, xc + t / 2, 12, 20, Z0 + 8, Z1 - 8, c(steel))
        n = max(3, int((Z1 - Z0) // 14))
        for i in range(n):
            bz = Z0 + 12 + (Z1 - Z0 - 24) * i / max(1, n - 1)
            box(m, xc - 1.5, xc + 1.5, 20, H - 10, bz - 1.5, bz + 1.5, c(steel))
    return m

OBJ_ARCH = {
    'light': arch_ceiling_light, 'floodlight': lambda w, h, i: arch_lamppost(w, h, i, 220.0, 3),
    'searchlight': arch_searchlight, 'advancedsearchlight': arch_searchlight, 'solarlight': arch_solarlight,
    'lightningrod': arch_rod, 'windturbine': arch_turbine, 'solarwindhybrid': arch_turbine,
    'backupgenerator': lambda w, h, i: gen_appliance(w, h, i, 90.0), 'battery': lambda w, h, i: gen_cabinet(w, h, i, 70.0),
    'powercapacitormaxplus': lambda w, h, i: gen_cabinet(w, h, i, 100.0), 'powerexportmeter': lambda w, h, i: gen_cabinet(w, h, i, 80.0),
    'transformer': arch_transformer, 'weathermachine': arch_weather, 'trainingdummy': arch_dummy,
    'sprinkler': arch_sprinkler, 'cropsprinkler': arch_sprinkler, 'waterpumpstation': lambda w, h, i: arch_waterstation(w, h, i),
    'waterboiler': lambda w, h, i: gen_appliance(w, h, i, 120.0),
    'gate': arch_gate, 'doublegate': arch_gate, 'whitepicketgate': arch_gate, 'roadgate': arch_gate, 'cagegate': arch_gate,
    'fencegatestandard': arch_gate, 'fencegatestaff': arch_gate, 'fencegatevisitor': arch_gate, 'fencegateguard': arch_gate,
    'roadbarrier': arch_gate,
}

def arch_punchbag(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    box(m, cx - 18, cx + 18, 0, 6, cz - 18, cz + 18, c(DARK))
    box(m, cx - 3, cx + 3, 6, 190, cz - 3, cz + 3, c(STEEL))
    box(m, cx - 3, cx + 40, 184, 190, cz - 3, cz + 3, c(STEEL))
    lathe(m, [(14, 40), (16, 60), (16, 130), (14, 150), (3, 152)], 10, c((176, 36, 36)), cx=cx + 36, cz=cz)
    return m




def arch_ring(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch
    box(m, X0, X1, 0, 22, Z0, Z1, c((70, 90, 160)))
    box(m, X0 + 6, X1 - 6, 22, 24, Z0 + 6, Z1 - 6, c((200, 200, 210)))
    for sx in (X0 + 8, X1 - 14):
        for sz in (Z0 + 8, Z1 - 14): box(m, sx, sx + 6, 22, 130, sz, sz + 6, c(DARK))
    for y in (60, 90, 118):
        box(m, X0 + 10, X1 - 10, y, y + 3, Z0 + 9, Z0 + 12, c((196, 40, 40))); box(m, X0 + 10, X1 - 10, y, y + 3, Z1 - 12, Z1 - 9, c((196, 40, 40)))
        box(m, X0 + 9, X0 + 12, y, y + 3, Z0 + 10, Z1 - 10, c((196, 40, 40))); box(m, X1 - 12, X1 - 9, y, y + 3, Z0 + 10, Z1 - 10, c((196, 40, 40)))
    return m

YARD = {'d_punchbag': arch_punchbag, 
        'd_boxingring': arch_ring, 'd_benchfullrotate': arch_bench, 'd_benchsmallfullrotate': bench_small}

def arch_mat(w, h, img, col=None, border=None):
    """prayer / gym / door mats: a thin cushion with a darker border and a lighter inlay"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; bc = col or body_of(img, (232, 232, 226))
    t = 3.0 if border else 5.0; b = min((X1 - X0), (Z1 - Z0)) * 0.1
    box(m, X0, X1, 0, t * 0.6, Z0, Z1, c(border or shade(bc, 0.78)))                        # border
    box(m, X0 + b * 0.5, X1 - b * 0.5, t * 0.6, t, Z0 + b * 0.5, Z1 - b * 0.5, c(shade(bc, 0.9)))
    box(m, X0 + b, X1 - b, t, t + 1.2, Z0 + b, Z1 - b, c(shade(bc, 1.08)))        # inlay
    return m

def arch_booth(w, h, img):
    """secure visitation booth: panelled end partitions, a laminate counter with cabinet fronts, a framed glass screen with speaker grille and a phone each side, and a padded chair each end"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch
    d = Z1 - Z0; wd = X1 - X0; zc = (Z0 + Z1) / 2; xc = (X0 + X1) / 2
    panel, trim, steel, lam = (222, 226, 232), (168, 174, 184), (126, 132, 142), (214, 204, 186)
    for ex in (X0, X1 - 8):
        box(m, ex, ex + 8, 0, 136, Z0, Z1, c(panel)); box(m, ex - 0.8, ex + 8.8, 128, 136, Z0 - 0.8, Z1 + 0.8, c(trim)); box(m, ex - 0.8, ex + 8.8, 0, 6, Z0 - 0.8, Z1 + 0.8, c(trim))   # partition + cap + kick
    cd = d * 0.22
    box(m, X0 + 8, X1 - 8, 4, 70, zc - cd, zc + cd, c((162, 166, 176)))                                                           # counter cabinet
    for sgn in (-1, 1):                                                                                                          # cabinet doors + handles on both faces
        zf = zc + sgn * cd
        for k in range(3):
            px = X0 + 14 + k * (wd - 28) / 3
            box(m, px, px + (wd - 28) / 3 - 4, 10, 64, zf if sgn > 0 else zf - 1.2, zf + 1.2 if sgn > 0 else zf, c((150, 154, 164)))
            box(m, px + (wd - 28) / 6 - 6, px + (wd - 28) / 6 + 6, 52, 55, zf + (1.2 if sgn > 0 else -3.2), zf + (3.2 if sgn > 0 else -1.2), c((206, 208, 214)))
    box(m, X0 + 8, X1 - 8, 70, 76, zc - cd - 3, zc + cd + 3, c(lam))                                                              # counter top
    box(m, X0 + 8, X1 - 8, 70, 72, zc - cd - 3, zc + cd + 3, c((120, 112, 100)))                                                  # edge band
    box(m, X0 + 8, X1 - 8, 76, 134, zc - 2.5, zc + 2.5, c((196, 226, 238)))                                                      # glass pane
    for yy in (76, 132): box(m, X0 + 8, X1 - 8, yy, yy + 3, zc - 4, zc + 4, c(steel))                                            # steel rails
    for xx in (X0 + 8, X1 - 11): box(m, xx, xx + 3, 76, 135, zc - 4, zc + 4, c(steel))                                           # steel uprights
    for k in range(4):                                                                                                          # speaker grille
        box(m, xc - 9 + k * 5, xc - 6 + k * 5, 96, 112, zc - 3.2, zc + 3.2, c((70, 76, 86)))
    for sgn in (-1, 1):
        zz = zc + sgn * (cd + 26)
        box(m, xc + 24, xc + 38, 76, 80, zc + sgn * (cd - 6) - 4, zc + sgn * (cd - 6) + 4, c((40, 44, 52)))                       # phone cradle
        box(m, xc + 25, xc + 37, 80, 84, zc + sgn * (cd - 6) - 2, zc + sgn * (cd - 6) + 2, c((24, 26, 30)))                       # handset
        lathe(m, [(14, 0), (14, 2), (3.5, 3), (3.5, 28)], 5, c((60, 64, 72)), cx=xc, cz=zz)                                      # chair base + column
        for ang in range(5):
            aa = 2 * math.pi * ang / 5; box(m, xc + 14 * math.cos(aa) - 1.5, xc + 14 * math.cos(aa) + 1.5, 0, 4, zz + 14 * math.sin(aa) - 1.5, zz + 14 * math.sin(aa) + 1.5, c((40, 44, 52)))
        box(m, xc - 17, xc + 17, 28, 36, zz - 17, zz + 17, c((36, 104, 170))); box(m, xc - 16, xc + 16, 36, 42, zz - 16, zz + 16, c((50, 124, 192)))      # padded seat
        zb = zz + sgn * 14
        box(m, xc - 16, xc + 16, 42, 86, zb - 3.5, zb + 3.5, c((36, 104, 170))); box(m, xc - 14, xc + 14, 46, 82, zb - 4.5 * (1 if sgn < 0 else -1) - 1, zb - 4.5 * (1 if sgn < 0 else -1) + 1, c((50, 124, 192)))   # backrest + cushion
        for ax in (xc - 20, xc + 17): box(m, ax, ax + 3, 42, 60, zz - 14, zz + 12, c((46, 50, 58)))                                  # armrests
    return m
def arch_picnic(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; wood = (150, 98, 56); dk = (96, 62, 36)
    zc = (Z0 + Z1) / 2; d = Z1 - Z0
    box(m, X0 + 6, X1 - 6, 70, 78, zc - d * 0.2, zc + d * 0.2, c(wood))                      # table top
    for sgn in (-1, 1):
        zb = zc + sgn * d * 0.36
        box(m, X0 + 6, X1 - 6, 38, 44, zb - 9, zb + 9, c(wood))                               # bench seat
        for lx in (X0 + 14, X1 - 20): box(m, lx, lx + 6, 0, 38, zb - 4, zb + 4, c(dk))
    for lx in (X0 + 14, X1 - 20): box(m, lx, lx + 6, 0, 70, zc - 6, zc + 6, c(dk))
    return m

def arch_cactus(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    lathe(m, [(13, 0), (15, 22), (13, 24), (0, 24)], 10, c((170, 90, 50)), cx=cx, cz=cz)
    lathe(m, [(9, 24), (10, 60), (6, 74), (0, 76)], 10, c((60, 130, 60)), cx=cx, cz=cz)
    box(m, cx + 8, cx + 26, 44, 50, cz - 3, cz + 3, c((60, 130, 60))); box(m, cx + 22, cx + 28, 44, 66, cz - 3, cz + 3, c((60, 130, 60)))
    box(m, cx - 26, cx - 8, 34, 40, cz - 3, cz + 3, c((60, 130, 60))); box(m, cx - 28, cx - 22, 34, 54, cz - 3, cz + 3, c((60, 130, 60)))
    return m
def arch_coatstand(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; wood = (110, 70, 40)
    lathe(m, [(20, 0), (20, 4), (4, 8)], 10, c(wood), cx=cx, cz=cz); box(m, cx - 3, cx + 3, 6, 170, cz - 3, cz + 3, c(wood))
    for ax, az in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        box(m, cx + ax * 4 - (3 if az else 12 * (ax < 0)), cx + ax * 14 + (3 if az else 0) + (0 if ax >= 0 else 0), 150, 156, cz + az * 4 - (3 if ax else 0), cz + az * 14 + (3 if ax else 0), c(wood))
    lathe(m, [(14, 170), (14, 176), (8, 190), (0, 192)], 10, c((60, 60, 80)), cx=cx + 14, cz=cz)
    return m
def arch_pew(w, h, img):
    """church pew: shaped end panels (tall at the back, low arm at the front), a sloped seat, a two-rail back with a gap, a hymn-book shelf and a cushion strip"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; wood = (150, 98, 54); dk = (104, 64, 36); lt = (176, 124, 72)
    d = Z1 - Z0; zb = Z0 + d * 0.16; zf = Z1 - d * 0.14
    for ex in (X0 + 2, X1 - 12):
        box(m, ex, ex + 10, 0, 100, zb - 2, zb + 12, c(dk))                              # tall back post
        box(m, ex, ex + 10, 0, 64, zf - 12, zf + 2, c(dk))                               # lower front post
        box(m, ex, ex + 10, 56, 64, zb + 12, zf - 12, c(dk))                             # arm
        box(m, ex, ex + 10, 0, 12, zb + 12, zf - 12, c(dk))                              # foot rail
        box(m, ex - 1, ex + 11, 98, 104, zb - 3, zb + 13, c(lt))                         # cap on the tall post
    box(m, X0 + 12, X1 - 12, 40, 45, zb + 6, zf - 2, c(wood))                            # seat
    box(m, X0 + 14, X1 - 14, 45, 48, zb + 10, zf - 12, c((140, 34, 40)))                 # cushion strip
    box(m, X0 + 12, X1 - 12, 52, 66, zb, zb + 7, c(wood))                                # lower back rail
    box(m, X0 + 12, X1 - 12, 82, 98, zb, zb + 7, c(wood))                                # upper back rail
    box(m, X0 + 12, X1 - 12, 66, 82, zb + 1, zb + 4, c(dk))                              # recessed gap
    box(m, X0 + 12, X1 - 12, 96, 101, zb - 2, zb + 9, c(lt))                             # top moulding
    box(m, X0 + 12, X1 - 12, 22, 25, zf - 1, zf + 3, c(dk))                              # hymn-book shelf at the front
    return m
def arch_altar(w, h, img):
    """altar: stepped stone dais, panelled table with a red cloth and gold trim, a cross and two candles, a carved back screen"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch
    wd = X1 - X0; d = Z1 - Z0; cx = (X0 + X1) / 2; zc = (Z0 + Z1) / 2
    stone = (176, 170, 160); wood = (120, 78, 44); red = (150, 30, 36); gold = (226, 186, 70)
    box(m, X0, X1, 0, 7, Z0, Z1, c(shade(stone, 0.85)))                                      # lower step
    box(m, X0 + wd * 0.06, X1 - wd * 0.06, 7, 14, Z0 + d * 0.06, Z1 - d * 0.06, c(stone))   # upper step
    tx0, tx1, tz0, tz1 = X0 + wd * 0.18, X1 - wd * 0.18, zc - d * 0.14, zc + d * 0.2
    box(m, tx0, tx1, 14, 66, tz0, tz1, c(wood))                                             # table body
    for k in range(3):                                                                       # raised panels on the front
        px = tx0 + 8 + k * (tx1 - tx0 - 16) / 3.0
        box(m, px, px + (tx1 - tx0 - 16) / 3.0 - 5, 22, 58, tz1, tz1 + 2, c(shade(wood, 1.2)))
    box(m, tx0 - 3, tx1 + 3, 66, 72, tz0 - 3, tz1 + 3, c(shade(wood, 0.8)))                 # top slab
    box(m, tx0, tx1, 72, 74, tz0 + 2, tz1 - 2, c(red))                                      # cloth
    box(m, tx0 - 1, tx1 + 1, 36, 70, tz1 - 1, tz1 + 3, c(red))                              # cloth hangs over the front
    box(m, tx0 - 1, tx1 + 1, 62, 66, tz1 - 1, tz1 + 4, c(gold))                             # gold hem
    box(m, cx - 3, cx + 3, 74, 108, zc - 3, zc + 3, c(gold)); box(m, cx - 12, cx + 12, 92, 98, zc - 3, zc + 3, c(gold))   # cross
    for s in (-1, 1):                                                                        # candles
        ccx = cx + s * (tx1 - tx0) * 0.36
        lathe(m, [(5, 74), (5, 94), (3, 96)], 8, c((240, 236, 214)), cx=ccx, cz=zc)
        lathe(m, [(2, 96), (0, 102)], 6, c((255, 190, 60)), cx=ccx, cz=zc)
    box(m, X0 + wd * 0.1, X1 - wd * 0.1, 14, 150, Z0 + d * 0.04, Z0 + d * 0.16, c(shade(wood, 0.9)))   # back screen
    box(m, X0 + wd * 0.1 - 3, X1 - wd * 0.1 + 3, 150, 158, Z0 + d * 0.02, Z0 + d * 0.18, c(gold))
    return m
def arch_dogcrate(w, h, img):
    """cosy dog crate: open front so the dog can lean out; barred back and sides, solid roof, a plump bed, blanket, pillow and a bone"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    wd = X1 - X0; d = Z1 - Z0; H = 150.0; steel = (150, 156, 164); dk = (70, 74, 82)
    box(m, X0 + 2, X1 - 2, 0, 8, Z0 + 2, Z1 - 2, c((110, 84, 60)))                               # wooden tray
    for px in (X0 + 3, X1 - 7):
        for pz in (Z0 + 3, Z1 - 7): box(m, px, px + 4, 8, H, pz, pz + 4, c(dk))                   # corner posts
    box(m, X0 + 2, X1 - 2, H, H + 5, Z0 + 2, Z1 - 2, c(shade(steel, 0.9)))                        # roof
    box(m, X0 + 3, X1 - 3, H - 8, H, Z1 - 7, Z1 - 3, c(dk))                                      # open-front lintel
    n = max(5, int(wd // 9))
    for k in range(n):
        bx = X0 + 9 + (wd - 18) * k / max(1, n - 1)
        box(m, bx - 1.2, bx + 1.2, 8, H, Z0 + 3, Z0 + 5, c(steel))                                # back bars
    nz = max(4, int(d // 9))
    for k in range(nz):
        bz = Z0 + 9 + (d - 18) * k / max(1, nz - 1)
        box(m, X0 + 3, X0 + 5, 8, H, bz - 1.2, bz + 1.2, c(steel)); box(m, X1 - 5, X1 - 3, 8, H, bz - 1.2, bz + 1.2, c(steel))
    for y in (46, 90, 126): box(m, X0 + 3, X1 - 3, y, y + 2, Z0 + 2.5, Z0 + 5.5, c(dk))
    # cosy interior
    bed = (176, 120, 84); blanket = (96, 124, 170); pillow = (238, 226, 200)
    box(m, X0 + 7, X1 - 7, 8, 20, Z0 + 8, Z1 - 3, c(bed))                                        # plump bed
    box(m, X0 + 7, X1 - 7, 20, 22, Z0 + 8, Z1 - 3, c(shade(bed, 1.12)))
    box(m, X0 + 10, X0 + wd * 0.55, 22, 26, Z0 + 12, Z1 - 12, c(blanket))                         # folded blanket
    box(m, X0 + wd * 0.58, X1 - 10, 22, 32, Z0 + 10, Z0 + 10 + d * 0.38, c(pillow))               # pillow at the back
    box(m, X1 - 20, X1 - 9, 22, 26, Z1 - 16, Z1 - 12, c((236, 230, 214)))                          # bone
    box(m, X1 - 22, X1 - 18, 22, 28, Z1 - 17, Z1 - 11, c((236, 230, 214))); box(m, X1 - 11, X1 - 7, 22, 28, Z1 - 17, Z1 - 11, c((236, 230, 214)))
    return m
FRIDGE_H = 100      # kitchen chest freezer + lid height (was 75: looked too small next to the cookers and sinks)
def arch_fridge(w, h, img):
    """a domestic-style chest freezer: low white enamel tub on a dark plinth, rounded corners, thermostat and lights low on the front, side vent; the lid is a separate object"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)      # fills its whole footprint, as the 2D art does (it was cut to 0.8 tile deep and looked too small)
    m = Mesh(); c = swatch
    wd = X1 - X0; white, whited, plinth, grey = c((236, 238, 238)), c((206, 210, 212)), c((60, 62, 66)), c((150, 154, 158))
    H = FRIDGE_H - 10
    box(m, X0 + 3, X1 - 3, 0, 5, Z0 + 3, Z1 - 3, plinth)                                    # plinth
    box(m, X0 + 2, X1 - 2, 5, H - 1, Z0, Z1, white)                                          # tub (its top sits under the rim: no shared faces)
    box(m, X0, X0 + 2, 6, H - 1.5, Z0 + 2, Z1 - 2, white); box(m, X1 - 2, X1, 6, H - 1.5, Z0 + 2, Z1 - 2, white)   # rounded corners (stepped)
    box(m, X0 + 2, X1 - 2, H - 3, H, Z0 - 0.6, Z1 + 0.6, whited)                             # top rim
    box(m, X1 - 26, X1 - 8, 10, 20, Z1, Z1 + 1, grey)                                         # thermostat plate
    lathe(m, [(0, 0), (3, 0), (3, 1.5), (0, 1.5)], 10, plinth, cx=X1 - 20, cz=Z1 + 1)       # (a dial, laid flat; reads as a knob)
    box(m, X1 - 13, X1 - 11, 16, 18, Z1 + 1, Z1 + 1.6, c((60, 200, 90))); box(m, X1 - 13, X1 - 11, 12, 14, Z1 + 1, Z1 + 1.6, c((220, 60, 50)))
    for k in range(4): box(m, X1, X1 + 0.8, 10 + k * 4, 12 + k * 4, Z0 + 8, Z1 - 8, plinth)   # side vent slots
    box(m, X0 + 6, X0 + 30, H - 12, H - 8, Z1, Z1 + 0.6, c((70, 120, 190)))                   # badge
    return m

def arch_fridge_lid(w, h, img):
    if 'fridge' in sprites: _n, _w, _h, _i = sprites['fridge']; X0, X1, Z0, Z1 = frame_of(_w, _h, _i)      # sit exactly on the freezer body
    else: X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; wd = X1 - X0; H = FRIDGE_H - 10      # lid sits on the body's rim (body top = H)
    white, seal, handle = c((242, 244, 244)), c((120, 124, 128)), c((150, 156, 162))
    box(m, X0 + 1.5, X1 - 1.5, H + 0.5, H + 2.0, Z0 + 0.5, Z1 + 0.5, seal)                    # rubber seal
    box(m, X0 + 1, X1 - 1, H + 2, H + 10, Z0, Z1 + 1, white)                                 # one deep lid
    box(m, X0 + 3, X1 - 3, H + 9, H + 11, Z0 + 2, Z1 - 1, white)                             # domed top (starts inside the lid: no shared faces to flicker)
    box(m, X0 + wd * 0.35, X1 - wd * 0.35, H, H + 6, Z1 + 1, Z1 + 4, handle)                 # handle under the front edge
    for hx in (X0 + 10, X1 - 10): box(m, hx - 4, hx + 4, H - 2, H + 6, Z0 - 2, Z0, seal)    # hinges at the back
    return m
def arch_locker(w, h, img):
    """guard locker: steel cabinet with vent slats, a number plate and a latch"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    H = 140.0
    m = gen_cabinet(w, h, img, H); c = swatch
    cx = (X0 + X1) / 2; wd = X1 - X0
    for i in range(4):
        y = H * 0.74 + i * 6
        box(m, X0 + wd * 0.2, X1 - wd * 0.2, y, y + 2.5, Z1, Z1 + 1.2, c((30, 36, 44)))        # vent slats
    box(m, cx - 7, cx + 7, H * 0.62, H * 0.68, Z1, Z1 + 1.0, c((226, 228, 232)))                # label plate
    box(m, cx + wd * 0.24, cx + wd * 0.3, H * 0.4, H * 0.5, Z1, Z1 + 3.5, c((230, 232, 236)))   # latch
    D = 92.0      # deep enough that back-to-back rows almost touch, not so deep they run into each other: rows placed back to back no longer run into each other
    if Z1 - Z0 > D: k = D / (Z1 - Z0); m.v = [(x, y, Z1 - (Z1 - z) * k) for (x, y, z) in m.v]
    return m

ARCH = {
    'visitationsecure': arch_booth, 'guardlocker': arch_locker,
    'fridge': arch_fridge, 'fridgelid': arch_fridge_lid,
    'stackbleach': arch_drum,
    'doorservo': arch_servo, 'doorservo2': arch_servo,
    'bench': arch_bench, 'visitortable': arch_visitor, 'visitortablesecure': arch_visitor,
    'sink': lambda w, h, img: arch_sink(w, h, img),
    'bed': lambda w, h, img: arch_bed(w, h, img, 1),
    'cot': lambda w, h, img: arch_bed(w, h, img, 1),
    'bunkbed': lambda w, h, img: arch_bed(w, h, img, 2),
}
OBJ_ARCH.update({'gymmat': arch_mat, 'playmat': arch_mat, 'doormat': arch_mat})

# ---------------------------------------------------------------- doors and windows
def arch_door_double(w, h, img, col=(150, 104, 60)):
    W = w * 25.0; m = Mesh(); c = swatch
    box(m, -W - 4, W + 4, 150, 160, -9, 9, c(shade(col, 0.6)))                                   # lintel
    for sx in (-1, 1):
        x0, x1 = (2, W) if sx > 0 else (-W, -2)
        box(m, x0, x1, 0, 150, -5, 5, c(col))
        box(m, x0 + 8, x1 - 8, 14, 136, 5, 6.2, c(shade(col, 1.15)))                             # raised panel
        box(m, x0 + 8, x1 - 8, 14, 136, -6.2, -5, c(shade(col, 1.15)))
        hx = 8 if sx > 0 else -12
        box(m, hx, hx + 4, 70, 80, 5, 9, c((220, 222, 226)))                                     # handle
    return m

def arch_window(w, h, img, glass=(190, 222, 238), frame=(120, 84, 50), mullion=1, stained=False):
    """Walls are half a tile thick (+-25): a window is a frame + pane on BOTH wall faces, just proud of the surface."""
    W = w * 25.0; m = Mesh(); c = swatch; y0, y1 = 128.0, 192.0; t = 7.0   # the upper part of the 3D wall (the game draws these lower than their model numbers suggest)
    cols = [(200, 60, 60), (230, 190, 50), (60, 120, 200), (70, 170, 90), (170, 80, 190), (230, 130, 50)]
    for s in (1, -1):
        za, zb = (41.0, 47.0) if s > 0 else (-47.0, -41.0)      # just outside the 3D wall faces (+-37.5)
        box(m, -W, W, y0, y0 + t, za, zb, c(frame)); box(m, -W, W, y1 - t, y1, za, zb, c(frame))
        box(m, -W, -W + t, y0, y1, za, zb, c(frame)); box(m, W - t, W, y0, y1, za, zb, c(frame))
        pa, pb = (40.5, 45.0) if s > 0 else (-45.0, -40.5)
        if stained:
            cw = (2 * W - 2 * t) / 3.0; ch = (y1 - y0 - 2 * t) / 2.0
            for i in range(3):
                for j in range(2):
                    box(m, -W + t + i * cw + 1, -W + t + (i + 1) * cw - 1, y0 + t + j * ch + 1, y0 + t + (j + 1) * ch - 1, pa, pb, c(cols[(i + j * 3) % 6]))
        else:
            box(m, -W + t, W - t, y0 + t, y1 - t, pa, pb, c(glass))
        for k in range(1, mullion + 1):
            x = -W + (2 * W) * k / (mullion + 1.0)
            box(m, x - 2, x + 2, y0, y1, za, zb, c(frame))
        box(m, -W + t, W - t, (y0 + y1) / 2 - 2, (y0 + y1) / 2 + 2, za, zb, c(frame))
    # the game fits a window model's height to the wall: anchor its extent from the floor (0) to the wall top (198) with two invisible, zero-area faces,
    # so the frame keeps its place in the upper part of the wall instead of being stretched down to the floor
    for _y in (0.0, 198.0): m.quad([(-W, _y, 0.0), (-W, _y, 0.0), (-W, _y, 0.0), (-W, _y, 0.0)], (0, 1, 0), [c(frame)] * 4)
    return m
def arch_solitary_door(w, h, img):
    """solitary-confinement door: a riveted steel leaf in the game's door frame coordinates (hinge at x = 0, 100 wide, 150 tall), detailed on BOTH faces:
    observation slot, food hatch, ribs, bolt and hinge plates"""
    m = Mesh(); c = swatch
    steel, dk, rv = (140, 146, 154), (78, 82, 90), (176, 182, 190)
    X0, X1, H, t = 0.0, 100.0, 150.0, 7.0
    box(m, X0 - 1, X1 + 1, 0, H, -t, t, c(dk))                                   # frame / backing plate
    box(m, X0 + 3, X1 - 3, 3, H - 3, -t - 1.5, t + 1.5, c(steel))                # door leaf
    for s in (1, -1):
        def zz(a, b): return (a, b) if s > 0 else (-b, -a)
        for yy in (22, 74, 128): box(m, X0 + 3, X1 - 3, yy, yy + 4, *zz(t + 1.5, t + 2.6), c(dk))      # horizontal ribs
        box(m, X0 + 30, X1 - 30, 104, 116, *zz(t + 1.5, t + 3), c((18, 20, 24)))          # observation slot
        box(m, X0 + 28, X1 - 28, 102, 104, *zz(t + 1.5, t + 3.2), c(rv)); box(m, X0 + 28, X1 - 28, 116, 118, *zz(t + 1.5, t + 3.2), c(rv))
        box(m, X0 + 26, X1 - 26, 36, 58, *zz(t + 1.5, t + 3), c(dk))                       # food hatch
        box(m, X0 + 38, X1 - 38, 44, 49, *zz(t + 3, t + 5), c(rv))                         # hatch handle
        box(m, X1 - 22, X1 - 8, 64, 70, *zz(t + 1.5, t + 6), c(rv))                       # bolt housing
        box(m, X1 - 20, X1 - 10, 66, 68, *zz(t + 6, t + 12), c(rv))                       # bolt
        for yy in (18, 70, 124): box(m, X0 + 1, X0 + 9, yy, yy + 14, *zz(t + 1.5, t + 4), c(dk))     # hinge plates
        for yy in (10, 64, 140):
            for xx in (X0 + 14, X0 + 50, X1 - 14): box(m, xx - 1.5, xx + 1.5, yy - 1.5, yy + 1.5, *zz(t + 1.5, t + 3.2), c(rv))   # rivets
    return m
ARCH.update({
    'solitarydoor': arch_solitary_door, 'doubledoor': arch_door_double, 'doublestaffdoor': lambda w, h, i: arch_door_double(w, h, i, (70, 110, 160)),
    'window': lambda w, h, i: arch_window(w, h, i, mullion=1), 'windowlarge': lambda w, h, i: arch_window(w, h, i, mullion=3),
    'woodenwindowsmall': lambda w, h, i: arch_window(w, h, i, frame=(110, 74, 40), mullion=1),
    'woodenwindowlarge': lambda w, h, i: arch_window(w, h, i, frame=(110, 74, 40), mullion=3),
    'stainedglasswindow': lambda w, h, i: arch_window(w, h, i, frame=(90, 70, 60), stained=True),
})
# ---------------------------------------------------------------- smooth, simple people in five body styles (samples first: the user picks)
_tex_cache = {}
def tswatch(rgb, kind='cloth'):
    """a small procedural texture tile (cloth weave, knit ribs, hair strands, skin) in the given colour, placed in the atlas: returns 4 uvs"""
    USE_TEX = False                      # small texture tiles blur into their neighbours at game distances (mip-mapping): flat colours look far cleaner
    if not USE_TEX: s_ = swatch(rgb); return [s_] * 4
    key = (tuple(int(c) // 24 * 24 + 12 for c in rgb), kind)
    if key in _tex_cache: return _tex_cache[key]
    p = place(2, 2)
    if p is None: s_ = swatch(rgb); _tex_cache[key] = [s_] * 4; return _tex_cache[key]
    import random; rnd = random.Random(hash(key) & 0xffff)
    rgb = key[0]
    for yy in range(16):
        for xx in range(16):
            if kind == 'cloth': v = 1.0 - (0.10 if (xx + yy) % 4 == 0 else 0.0) + (0.05 if (xx - yy) % 6 == 0 else 0.0) + rnd.uniform(-0.035, 0.035)
            elif kind == 'knit': v = 1.0 - (0.11 if xx % 3 == 0 else 0.0) + (0.04 if (yy + xx // 3) % 2 == 0 else -0.02) + rnd.uniform(-0.03, 0.03)
            elif kind == 'hair': v = 1.0 + 0.10 * math.sin(xx * 1.7 + 0.5 * math.sin(yy * 0.4)) + 0.05 * math.sin(xx * 0.55) + rnd.uniform(-0.03, 0.03)
            elif kind == 'skin': v = 1.0 + rnd.uniform(-0.025, 0.025) + (0.025 if ((xx // 6) + (yy // 6)) % 2 == 0 else 0.0)
            else: v = 1.0
            c = tuple(max(0, min(255, int(ch * v))) for ch in rgb)
            for dy in range(1):
                px[p[0] + xx, p[1] + yy] = c
    u0, u1 = (p[0] + 2) / AW, (p[0] + 14) / AW; v0, v1 = 1.0 - (p[1] + 2) / AH, 1.0 - (p[1] + 14) / AH
    r = [(u0, v1), (u0, v0), (u1, v0), (u1, v1)]; _tex_cache[key] = r; return r

def quadn(m, p, ns, t):
    """quad with a normal per corner (smooth shading)"""
    base_v = len(m.v); base_t = len(m.vt); m.v += p; m.vt += t
    ni = []
    for n in ns: m.vn.append(n); ni.append(len(m.vn))
    a, b, c, d = [base_v + i + 1 for i in range(4)]; ta, tb, tc, td = [base_t + i + 1 for i in range(4)]
    m.f.append(((a, ta, ni[0]), (b, tb, ni[1]), (c, tc, ni[2]))); m.f.append(((a, ta, ni[0]), (c, tc, ni[2]), (d, td, ni[3])))

def slathe(m, profile, segs, col, cx=0.0, cz=0.0, sx=1.0, sz=1.0, tex=None):
    """smooth surface of revolution: profile [(radius, y), ...] bottom->top, normals blended across rings and around"""
    uvc = swatch(col) if not tex else None; TU = tswatch(col, tex) if tex else None; P = len(profile)
    def nrm(i, ang):
        r0, y0 = profile[max(0, i - 1)]; r1, y1 = profile[min(P - 1, i + 1)]
        dr, dy = r1 - r0, y1 - y0; l = math.hypot(dr, dy) or 1.0
        nr, ny = dy / l, -dr / l; return (nr * math.cos(ang) / sx, ny, nr * math.sin(ang) / sz)
    def unit(v): l = math.sqrt(sum(c * c for c in v)) or 1.0; return tuple(c / l for c in v)
    for i in range(P - 1):
        (r0, y0), (r1, y1) = profile[i], profile[i + 1]
        for k in range(segs):
            a0 = 2 * math.pi * k / segs; a1 = 2 * math.pi * (k + 1) / segs
            p = [(cx + r0 * math.cos(a0) * sx, y0, cz + r0 * math.sin(a0) * sz), (cx + r0 * math.cos(a1) * sx, y0, cz + r0 * math.sin(a1) * sz),
                 (cx + r1 * math.cos(a1) * sx, y1, cz + r1 * math.sin(a1) * sz), (cx + r1 * math.cos(a0) * sx, y1, cz + r1 * math.sin(a0) * sz)]
            ns = [unit(nrm(i, a0)), unit(nrm(i, a1)), unit(nrm(i + 1, a1)), unit(nrm(i + 1, a0))]
            quadn(m, [p[0], p[3], p[2], p[1]], [ns[0], ns[3], ns[2], ns[1]], TU if TU else [uvc] * 4)
    top_r, top_y = profile[-1]
    if top_r > 0.01:
        for k in range(segs):
            a0 = 2 * math.pi * k / segs; a1 = 2 * math.pi * (k + 1) / segs
            m.quad([(cx, top_y, cz), (cx + top_r * math.cos(a1) * sx, top_y, cz + top_r * math.sin(a1) * sz), (cx + top_r * math.cos(a0) * sx, top_y, cz + top_r * math.sin(a0) * sz), (cx, top_y, cz)], (0, 1, 0), TU if TU else [uvc] * 4)

def _ball(m, cx, cy, cz, rx, ry, rz, col, segs=16, rings=9, lo=-90, hi=90, tex=None):
    prof = []
    for i in range(rings + 1):
        a = math.radians(lo + (hi - lo) * i / rings); prof.append((max(0.01, math.cos(a)), math.sin(a)))
    pts = [(r, y) for r, y in prof]
    slathe(m, [(r * rx, cy + y * ry) for r, y in pts], segs, col, cx=cx, cz=cz, sz=(rz / rx), tex=tex)

BODY_STYLES = {
    # measured from the stock characters: (name, profile (radius, y), sx, sz, head radius, head y)
    1: ('round', [(0.01, 0), (50, 0), (49, 12), (47, 36), (43, 48), (29, 60), (23, 72), (14, 78)], 1.0, 1.0, 22, 92),                      # prisoner / sonny: big bell, small shoulders
    2: ('bust',  [(0.01, 0), (22, 0), (27, 12), (31, 24), (33, 36), (33, 48), (33, 62), (28, 72), (17, 80), (9, 84)], 1.46, 1.0, 22, 100),    # guard / chief: wide shoulders, shallow body
    3: ('slim',  [(0.01, 0), (15, 0), (17, 12), (14, 36), (15, 50), (17, 64), (20, 78), (14, 86)], 1.1, 1.0, 20, 96),                          # priest / janitor: tall and narrow
    4: ('bell',  [(0.01, 0), (45, 0), (43, 12), (41, 24), (37, 36), (34, 48), (27, 60), (19, 72), (10, 78)], 1.0, 0.97, 21, 92),               # workman / gardener / cook
    5: ('coat',  [(0.01, 0), (39, 0), (38, 12), (35, 24), (32, 36), (26, 48), (28, 60), (23, 72), (12, 78)], 1.0, 0.97, 20, 90),              # doctor / paramedic: coat
}
def _rad(prof, y):
    for i in range(len(prof) - 1):
        (r0, y0), (r1, y1) = prof[i], prof[i + 1]
        if y0 <= y <= y1 and y1 > y0: return r0 + (r1 - r0) * (y - y0) / (y1 - y0)
    return prof[-1][0] if y > prof[-1][1] else prof[0][0]
def band(m, prof, sx, sz, y0, y1, a0d, a1d, col, off=0.9, rows=6):
    """a thin smooth patch laid on the body surface (outfit detail): heights y0..y1, angles a0d..a1d degrees (90 = straight to the front)"""
    uvc = swatch(col); na = max(1, int(abs(a1d - a0d) / 14) + 1)
    for i in range(rows):
        ya, yb = y0 + (y1 - y0) * i / rows, y0 + (y1 - y0) * (i + 1) / rows; ra, rb = _rad(prof, ya) + off, _rad(prof, yb) + off
        for k in range(na):
            t0 = math.radians(a0d + (a1d - a0d) * k / na); t1 = math.radians(a0d + (a1d - a0d) * (k + 1) / na)
            def P(r, y, th): return (r * sx * math.cos(th), y, r * sz * math.sin(th))
            def N(th): l = math.hypot(math.cos(th) / sx, math.sin(th) / sz) or 1; return (math.cos(th) / sx / l, 0.12, math.sin(th) / sz / l)
            quadn(m, [P(ra, ya, t0), P(rb, yb, t0), P(rb, yb, t1), P(ra, ya, t1)], [N(t0), N(t0), N(t1), N(t1)], [uvc] * 4)
def ring(m, prof, sx, sz, y0, y1, col, off=0.9): band(m, prof, sx, sz, y0, y1, 0, 360, col, off, rows=1)

def _hball(m, *a, **k):
    k['tex'] = 'hair'; return _ball(m, *a, **k)

def hair_of(m, kind, hy, hr, top, skin):
    if kind == 'cap': _hball(m, 0, hy + 2.0, -2.5, hr * 1.03, hr * 1.02, hr * 1.04, top, 22, 6, lo=24, hi=90); _hball(m, 0, hy - 1.0, -hr * 0.45, hr * 1.0, hr * 0.9, hr * 0.62, top, 20, 5, lo=-40, hi=60)
    elif kind == 'fringe':
        _hball(m, 0, hy + 1.5, -2.0, hr * 1.06, hr * 1.04, hr * 1.05, top, 22, 7, lo=-10, hi=90); _hball(m, hr * 0.15, hy + hr * 0.62, hr * 0.62, hr * 0.8, hr * 0.28, hr * 0.5, top, 22, 5, lo=-20, hi=90)
    elif kind == 'bob': _hball(m, 0, hy + 1.5, -2.5, hr * 1.1, hr * 1.06, hr * 1.08, top, 22, 8, lo=-35, hi=90); _hball(m, 0, hy - hr * 0.1, -hr * 0.2, hr * 1.14, hr * 0.62, hr * 0.98, top, 22, 5, lo=-90, hi=40)
    elif kind == 'long': _hball(m, 0, hy + 1.5, -2.5, hr * 1.07, hr * 1.06, hr * 1.05, top, 22, 8, lo=-5, hi=90); _hball(m, 0, hy - hr * 0.7, -hr * 0.55, hr * 1.0, hr * 1.5, hr * 0.62, top, 18, 8)
    elif kind == 'pony': _hball(m, 0, hy + 1.5, -1.5, hr * 1.06, hr * 1.04, hr * 1.04, top, 22, 6, lo=8, hi=90); _hball(m, 0, hy + hr * 0.15, -hr * 1.12, hr * 0.38, hr * 0.38, hr * 0.38, top, 12, 6); _hball(m, 0, hy - hr * 0.55, -hr * 1.22, hr * 0.3, hr * 0.78, hr * 0.3, top, 12, 6)
    elif kind == 'bun': _hball(m, 0, hy + 1.5, -1.5, hr * 1.06, hr * 1.04, hr * 1.04, top, 22, 6, lo=8, hi=90); _hball(m, 0, hy + hr * 1.02, -hr * 0.35, hr * 0.42, hr * 0.42, hr * 0.42, top, 12, 6)
    elif kind == 'curly':
        _hball(m, 0, hy + hr * 0.25, -hr * 0.25, hr * 1.22, hr * 0.95, hr * 1.18, top, 22, 6, lo=-25, hi=90)
        for i in range(7): ang = i * 0.9; _hball(m, hr * 1.05 * math.cos(ang), hy + hr * (0.45 + 0.2 * (i % 2)), hr * 0.95 * math.sin(ang) - hr * 0.2, hr * 0.3, hr * 0.3, hr * 0.3, top, 8, 4)
    elif kind == 'afro': _hball(m, 0, hy + hr * 0.3, -hr * 0.1, hr * 1.4, hr * 1.25, hr * 1.35, top, 22, 8, lo=-40, hi=90)
    elif kind == 'bald':
        for sgn in (-1, 1): _hball(m, sgn * hr * 0.98, hy + hr * 0.05, -hr * 0.1, hr * 0.14, hr * 0.5, hr * 0.5, top, 8, 4)
        _hball(m, 0, hy + 0.5, -hr * 0.6, hr * 0.78, hr * 0.5, hr * 0.5, top, 22, 5, lo=-30, hi=90)
    elif kind == 'quiff':
        _hball(m, 0, hy + 1.5, -1.5, hr * 1.06, hr * 1.04, hr * 1.04, top, 22, 6, lo=8, hi=90); _hball(m, hr * 0.1, hy + hr * 0.95, hr * 0.35, hr * 0.62, hr * 0.42, hr * 0.62, top, 14, 6)
    elif kind == 'spiky':
        _hball(m, 0, hy + 1.5, -1.5, hr * 1.05, hr * 1.02, hr * 1.03, top, 22, 6, lo=20, hi=90)
        for i in range(9): ang = i * 0.7; _hball(m, hr * 0.72 * math.cos(ang), hy + hr * 0.95, hr * 0.72 * math.sin(ang) - hr * 0.1, hr * 0.22, hr * 0.62, hr * 0.22, top, 8, 4, lo=-30)
    elif kind == 'mohawk':
        for i in range(6): _hball(m, 0, hy + hr * (0.78 + 0.1 * (i % 2)), (i - 2.5) * hr * 0.34, hr * 0.2, hr * 0.62, hr * 0.2, top, 8, 4, lo=-30)
    elif kind == 'receding':
        _hball(m, 0, hy + 1.0, -hr * 0.55, hr * 1.04, hr * 0.98, hr * 0.7, top, 22, 6, lo=0, hi=90)
        for sg in (-1, 1): _hball(m, sg * hr * 0.98, hy + hr * 0.1, -hr * 0.1, hr * 0.15, hr * 0.5, hr * 0.5, top, 8, 4)
    elif kind == 'pigtails':
        _hball(m, 0, hy + 1.5, -1.5, hr * 1.06, hr * 1.04, hr * 1.04, top, 22, 6, lo=8, hi=90)
        for sg in (-1, 1): _hball(m, sg * hr * 1.12, hy - hr * 0.1, -hr * 0.2, hr * 0.36, hr * 0.36, hr * 0.36, top, 10, 5); _hball(m, sg * hr * 1.2, hy - hr * 0.75, -hr * 0.2, hr * 0.26, hr * 0.7, hr * 0.26, top, 8, 5)
    elif kind == 'sidepart': _hball(m, 0, hy + 1.5, -1.5, hr * 1.06, hr * 1.05, hr * 1.04, top, 22, 6, lo=-5, hi=90); band_h = 0

def person(style, outfit, body, accent, skin, top, hair='cap', hat=None, extras=()):
    nm, prof, sx, sz, hr, hy = BODY_STYLES[style]; m = Mesh(); dark = (32, 28, 30); topy = prof[-1][1]
    slathe(m, prof, 22, body, sx=sx, sz=sz, tex=('knit' if outfit in ('jumpsuit', 'blouse') else 'cloth'))
    ys = lambda f: topy * f
    if outfit == 'suit':                                                         # jacket with a V of white shirt, a tie and two buttons
        band(m, prof, sx, sz, ys(0.52), ys(1.0), 90 - 16, 90 + 16, (240, 240, 236))
        band(m, prof, sx, sz, ys(0.38), ys(0.98), 90 - 4, 90 + 4, accent, off=1.5)
        for yy in (0.3, 0.18): band(m, prof, sx, sz, ys(yy), ys(yy) + 3, 90 - 3, 90 + 3, shade(body, 0.5), off=1.6, rows=1)
        band(m, prof, sx, sz, ys(0.55), ys(0.98), 90 + 15, 90 + 19, shade(body, 0.7), off=1.1); band(m, prof, sx, sz, ys(0.55), ys(0.98), 90 - 19, 90 - 15, shade(body, 0.7), off=1.1)   # lapels
    elif outfit == 'overalls':                                                    # light shirt up top, bib and two straps in the body colour
        ring(m, prof, sx, sz, ys(0.5), ys(0.96), accent, off=0.7)
        band(m, prof, sx, sz, ys(0.2), ys(0.62), 90 - 34, 90 + 34, body, off=1.2)
        for sg in (-1, 1): band(m, prof, sx, sz, ys(0.58), ys(0.98), 90 + sg * 20, 90 + sg * 34, body, off=1.3)
        for sg in (-1, 1): band(m, prof, sx, sz, ys(0.55), ys(0.6), 90 + sg * 25, 90 + sg * 31, (216, 190, 80), off=1.6, rows=1)    # buttons
    elif outfit == 'labcoat':                                                     # long light coat, centre seam, lapels, pocket, stethoscope hint
        band(m, prof, sx, sz, ys(0.1), ys(1.0), 90 - 2.2, 90 + 2.2, shade(body, 0.62), off=1.0)
        band(m, prof, sx, sz, ys(0.55), ys(0.98), 90 + 3, 90 + 16, shade(body, 0.8), off=1.2); band(m, prof, sx, sz, ys(0.55), ys(0.98), 90 - 16, 90 - 3, shade(body, 0.8), off=1.2)
        band(m, prof, sx, sz, ys(0.3), ys(0.42), 90 + 14, 90 + 28, shade(body, 0.78), off=1.2)
        for sg in (-1, 1): band(m, prof, sx, sz, ys(0.6), ys(0.96), 90 + sg * 12, 90 + sg * 14, accent, off=1.7)
    elif outfit == 'hivis':                                                       # bright vest: two grey reflective bands and two front strips
        for yy in (0.28, 0.5): ring(m, prof, sx, sz, ys(yy), ys(yy) + 8, (206, 210, 204), off=0.9)
        for sg in (-1, 1): band(m, prof, sx, sz, ys(0.1), ys(0.98), 90 + sg * 9, 90 + sg * 17, (206, 210, 204), off=1.1)
    elif outfit == 'jumpsuit':                                                    # one piece with a lighter centre seam and chest pocket
        band(m, prof, sx, sz, ys(0.05), ys(1.0), 90 - 3, 90 + 3, shade(body, 1.25), off=0.9)
        band(m, prof, sx, sz, ys(0.55), ys(0.66), 90 + 12, 90 + 30, shade(body, 0.82), off=1.1)
        ring(m, prof, sx, sz, ys(0.0), ys(0.07), shade(body, 0.8), off=0.7)
    elif outfit == 'briefs':                                                      # a naked prisoner in underwear: boxers with a waistband, and a simple chest line
        ring(m, prof, sx, sz, ys(0.0), ys(0.34), accent, off=0.9); ring(m, prof, sx, sz, ys(0.30), ys(0.37), shade(accent, 0.75), off=1.2)
        for sg in (-1, 1): band(m, prof, sx, sz, ys(0.62), ys(0.68), 90 + sg * 12, 90 + sg * 20, shade(body, 0.82), off=1.0, rows=1)
    elif outfit == 'blouse':                                                      # soft round neckline in skin tone, belt and skirt hem
        band(m, prof, sx, sz, ys(0.7), ys(1.0), 90 - 22, 90 + 22, skin, off=0.8)
        ring(m, prof, sx, sz, ys(0.4), ys(0.46), accent, off=0.9)
        ring(m, prof, sx, sz, ys(0.0), ys(0.07), shade(body, 0.8), off=0.7)
    _ball(m, 0, hy, 0, hr, hr * 1.05, hr * 0.98, skin, 22, 10, tex='skin')                               # head
    hair_of(m, hair, hy, hr, top, skin)
    if hat: _ball(m, 0, hy + hr * 0.45, 0, hr * 1.12, hr * 0.55, hr * 1.12, hat, 22, 6, lo=0, hi=90); box(m, -hr * 0.95, hr * 0.95, hy + hr * 0.42, hy + hr * 0.52, hr * 0.7, hr * 1.5, swatch(shade(hat, 0.8)))   # cap + brim
    for sgn in (-1, 1):                                                                                     # eyes: a white ball with a dark pupil
        _ball(m, sgn * hr * 0.36, hy + hr * 0.05, hr * 0.88, 3.6, 3.4, 2.4, (246, 246, 242), 12, 6); _ball(m, sgn * hr * 0.36, hy + hr * 0.05, hr * 0.88 + 1.9, 1.9, 1.9, 1.2, dark, 8, 4)
    if 'beard' in extras: _hball(m, 0, hy - hr * 0.5, hr * 0.5, hr * 0.78, hr * 0.5, hr * 0.5, top, 18, 6, lo=-90, hi=30)
    if 'stache' in extras: _hball(m, 0, hy - hr * 0.22, hr * 0.9, hr * 0.42, hr * 0.12, hr * 0.14, top, 12, 4)
    if 'glasses' in extras:
        for sgn in (-1, 1): box(m, sgn * hr * 0.36 - 4.5, sgn * hr * 0.36 + 4.5, hy - hr * 0.08, hy + hr * 0.22, hr * 0.93, hr * 0.99, swatch((36, 34, 40)))
        box(m, -hr * 0.12, hr * 0.12, hy + hr * 0.06, hy + hr * 0.1, hr * 0.93, hr * 0.99, swatch((36, 34, 40)))
    return m

_SAMP = os.path.join(OUT, 'samples'); os.makedirs(_SAMP, exist_ok=True)
_SK = [(250, 200, 150), (206, 140, 96), (130, 84, 56), (246, 214, 186)]
_OUT = [('suit', (64, 82, 116), (180, 40, 44)), ('overalls', (60, 84, 62), (230, 226, 214)), ('labcoat', (214, 216, 218), (60, 150, 170)), ('hivis', (214, 226, 70), None), ('jumpsuit', (226, 128, 36), None), ('blouse', (70, 150, 160), (236, 232, 220))]
for _oi, (_o, _b, _a) in enumerate(_OUT):
    for _st in BODY_STYLES: person(_st, _o, _b, _a, _SK[_st % 4], (74, 50, 34), 'cap').write(os.path.join(_SAMP, 'outfit%d_style%d.obj' % (_oi + 1, _st)), 'sample')
_HAIR = ['cap', 'fringe', 'bob', 'long', 'pony', 'bun', 'curly', 'afro', 'bald']
for _hi, _h in enumerate(_HAIR):
    for _k, _c in enumerate([(74, 50, 34), (232, 196, 96), (30, 28, 30), (190, 192, 198)]):
        person(2 if _k % 2 else 3, 'suit', (64, 82, 116), (180, 40, 44), _SK[_k], _c, _h).write(os.path.join(_SAMP, 'hair%d_%d.obj' % (_hi + 1, _k + 1)), 'sample')
# ---------------------------------------------------------------- final batch: props that were jagged extruded towers
def arch_chandelier(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; R = min(X1 - X0, Z1 - Z0) * 0.42
    gold, glow = (214, 176, 78), (255, 232, 150)
    box(m, cx - 2, cx + 2, 160, 200, cz - 2, cz + 2, c((90, 90, 96)))                                  # chain
    lathe(m, [(R, 138), (R, 146), (R * 0.92, 148)], 12, c(gold), cx=cx, cz=cz)                      # ring
    lathe(m, [(10, 120), (12, 134), (4, 160)], 8, c(gold), cx=cx, cz=cz)                              # centre column
    for k in range(8):
        a = 2 * math.pi * k / 8; bx, bz = cx + R * math.cos(a), cz + R * math.sin(a)
        box(m, bx - 1.5, bx + 1.5, 142, 156, bz - 1.5, bz + 1.5, c(gold)); _blob(m, bx, 160, bz, 5.5, glow, 1.2, 6)   # candle arm + bulb
    return m
def arch_birdcage(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; R = min(X1 - X0, Z1 - Z0) * 0.34
    gold = (206, 170, 70)
    lathe(m, [(R * 0.5, 0), (R * 0.45, 30), (R * 0.2, 34)], 8, c((110, 84, 50)), cx=cx, cz=cz)          # stand
    lathe(m, [(R, 34), (R, 40)], 12, c((96, 74, 44)), cx=cx, cz=cz)                                    # base
    for k in range(12):
        a = 2 * math.pi * k / 12; bx, bz = cx + R * 0.95 * math.cos(a), cz + R * 0.95 * math.sin(a)
        box(m, bx - 0.9, bx + 0.9, 40, 98, bz - 0.9, bz + 0.9, c(gold))
    lathe(m, [(R, 98), (R * 0.8, 108), (R * 0.4, 118), (0, 126)], 12, c(gold), cx=cx, cz=cz)           # dome
    box(m, cx - R * 0.8, cx + R * 0.8, 58, 60, cz - 1, cz + 1, c((140, 100, 60)))                      # perch
    _blob(m, cx + 4, 66, cz, 6, (240, 200, 60), 0.9, 6)                                                # bird
    return m
def arch_flamingo(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    pk, dk = (240, 120, 170), (210, 84, 140)
    box(m, cx - 6, cx - 4, 0, 46, cz - 1, cz + 1, c(dk)); box(m, cx + 4, cx + 6, 0, 40, cz - 1, cz + 1, c(dk))     # legs
    _blob(m, cx, 58, cz, 16, pk, 0.8, 8)                                                                # body
    box(m, cx + 8, cx + 12, 62, 100, cz - 2, cz + 2, c(pk)); box(m, cx + 8, cx + 24, 96, 101, cz - 2, cz + 2, c(pk))   # neck up and over
    _blob(m, cx + 24, 96, cz, 4.5, pk, 1.0, 6); box(m, cx + 27, cx + 34, 92, 96, cz - 1.5, cz + 1.5, c((60, 50, 50)))    # head + beak
    return m
def arch_rat(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; gr = (120, 118, 122)
    _blob(m, cx, 6, cz, 8, gr, 0.7, 7); _blob(m, cx + 9, 6, cz, 5, gr, 0.7, 6); box(m, cx - 24, cx - 8, 2, 3.5, cz - 0.7, cz + 0.7, c((200, 150, 150)))   # body, head, tail
    box(m, cx + 8, cx + 10, 9, 12, cz - 4, cz - 2, c((220, 170, 170))); box(m, cx + 8, cx + 10, 9, 12, cz + 2, cz + 4, c((220, 170, 170)))
    return m
def arch_cctv_camo(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; gn = (84, 110, 70)
    lathe(m, [(7, 0), (5, 150), (4, 160)], 8, c((70, 74, 80)), cx=cx, cz=cz)                              # pole
    box(m, cx - 9, cx + 9, 150, 166, cz - 8, cz + 14, c(gn)); box(m, cx - 6, cx + 6, 154, 162, cz + 14, cz + 17, c((24, 26, 30)))   # camera body + lens
    box(m, cx - 9, cx + 9, 166, 169, cz - 10, cz + 15, c((60, 84, 52)))                                  # sun shield
    return m
ARCH.update({'d_chandelier': arch_chandelier, 'd_birdcage': arch_birdcage, 'd_pinkflamingo': arch_flamingo,
             'd_rat': arch_rat, 'd_cctv_camo': arch_cctv_camo})
def arch_waterstation(w, h, img, lagged=False):
    """water pump station: concrete plinth, steel pump housing with louvres, a tank, an elbow pipe run, valve wheels, a gauge and a small control cabinet"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; wd, dp = X1 - X0, Z1 - Z0
    steel, dsteel, pipe = (118, 130, 146), (84, 94, 108), ((206, 208, 200) if lagged else (150, 156, 164))
    red, conc = (200, 52, 48), (196, 198, 200)
    box(m, X0, X1, 0, 8, Z0, Z1, c(conc)); box(m, X0 + 4, X1 - 4, 8, 10, Z0 + 4, Z1 - 4, c((170, 172, 176)))                  # plinth + inset
    hx0, hx1, hz0, hz1 = X0 + wd * 0.08, X0 + wd * 0.56, Z0 + dp * 0.10, Z0 + dp * 0.60
    box(m, hx0, hx1, 10, 74, hz0, hz1, c(steel)); box(m, hx0 - 2, hx1 + 2, 74, 80, hz0 - 2, hz1 + 2, c(dsteel))                # housing + cap
    for k in range(5): box(m, hx0 + 10 + k * (hx1 - hx0 - 24) / 4, hx0 + 16 + k * (hx1 - hx0 - 24) / 4, 22, 62, hz1, hz1 + 1.5, c((40, 46, 56)))   # louvres
    box(m, hx0 + 6, hx1 - 6, 8 + 2, 18, hz1, hz1 + 2, c(dsteel))                                                                  # kick plate
    tr = min(wd, dp) * 0.17; tcx, tcz = X0 + wd * 0.78, Z0 + dp * 0.30
    lathe(m, [(tr, 10), (tr, 110), (tr * 0.86, 122), (tr * 0.5, 130), (0, 134)], 14, c((76, 118, 170)), cx=tcx, cz=tcz)                # tank
    for yy in (40, 80): lathe(m, [(tr + 1.5, yy), (tr + 1.5, yy + 4)], 14, c(dsteel), cx=tcx, cz=tcz)                          # tank bands
    pz = Z0 + dp * 0.80                                                                                                         # pipe run along the front
    box(m, X0 + wd * 0.10, X0 + wd * 0.90, 28, 40, pz - 6, pz + 6, c(pipe))
    for ex in (X0 + wd * 0.10, X0 + wd * 0.34, X0 + wd * 0.62, X0 + wd * 0.90): box(m, ex - 2, ex + 2, 26, 42, pz - 8, pz + 8, c(dsteel))   # flanges
    box(m, hx0 + (hx1 - hx0) * 0.5 - 6, hx0 + (hx1 - hx0) * 0.5 + 6, 40, 90 if False else 74, pz - 6, pz + 6, c(pipe))        # riser to the housing
    box(m, tcx - 6, tcx + 6, 28, 40, tcz + tr, pz + 6, c(pipe))                                                                # tank feed
    for vx in (X0 + wd * 0.22, X0 + wd * 0.74):
        box(m, vx - 2, vx + 2, 40, 56, pz - 2, pz + 2, c(dsteel)); lathe(m, [(9, 56), (9, 59)], 10, c(red), cx=vx, cz=pz)      # valve stem + wheel
    gx, gz = hx1 - 12, hz1 + 3
    box(m, gx - 8, gx + 8, 52, 68, hz1, hz1 + 3, c((230, 232, 236))); box(m, gx - 1, gx + 1, 56, 66, hz1 + 3, hz1 + 4, c(red))   # gauge face + needle
    box(m, X0 + wd * 0.84, X0 + wd * 0.94, 10, 58, Z0 + dp * 0.64, Z0 + dp * 0.74, c((90, 96, 104)))                            # control cabinet
    box(m, X0 + wd * 0.86, X0 + wd * 0.92, 40, 46, Z0 + dp * 0.74, Z0 + dp * 0.75, c((60, 210, 100)))                          # status LED
    return m
ARCH.update({'d_waterstation': arch_waterstation, 'd_waterstation_lagged': lambda w, h, i: arch_waterstation(w, h, i, True)})
STOCK_COPY = {'remotedoor': 'staffDoor',
    'd_visitordoor': 'door', 'd_securedoor': 'staffDoor', 'd_secretdoor': 'door', 'd_keycarddoor': 'staffDoor', 'd_keycarddoorlarge': 'jaildoorlarge',
    'd_jaildoorred': 'jaildoor', 'd_jaildoorgrey': 'jaildoor', 'd_jaildoororange': 'jaildoor', 'd_jaildooryellow': 'jaildoor', 'd_jaildoorwhite': 'jaildoor', 'd_rustydoor': 'jaildoor',
    'd_jaildoorlargered': 'jaildoorlarge', 'd_jaildoorlargegrey': 'jaildoorlarge', 'd_jaildoorlargeorange': 'jaildoorlarge', 'd_jaildoorlargeyellow': 'jaildoorlarge', 'd_jaildoorlargewhite': 'jaildoorlarge', 'd_rustydoorlarge': 'jaildoorlarge',
    'd_remotedoororange': 'staffDoor', 'd_remotedoorred': 'staffDoor', 'd_remotedooryellow': 'staffDoor', 'd_remotedoorwhite': 'staffDoor', 'd_remotedoormaroon': 'staffDoor', 'd_remotedoorblack': 'staffDoor',
    'd_staffdoorblue': 'staffDoor', 'd_staffdoororange': 'staffDoor', 'd_barndoor': 'door'}

def rot180(m):
    m.v = [(-x, y, -z) for (x, y, z) in m.v]; m.vn = [(-x, y, -z) for (x, y, z) in m.vn]; return m
YARD.update({'d_outdoorpicnictable': arch_picnic})
YARD.update({'d_plantcactus': arch_cactus, 'd_plant': arch_cactus, 'd_coatstand': arch_coatstand})
ARCH.update({'pews': arch_pew, 'churchpew': arch_pew})
ARCH.update({'altar': arch_altar, 'churchaltar': arch_altar})
ARCH.update({'dogcrate': arch_dogcrate})
# the game's own models are better than anything generated here: never replace them
# (stock file names sometimes carry underscores: fridge_lid.obj <-> sprite FridgeLid)
stock_norm = {h.replace('_', '') for h in have}
for _k in [k for k in ARCH if k in stock_norm and k not in ('fridgelid', 'bench', 'churchaltar', 'fridge', 'foodtray', 'dogcrate')]: del ARCH[_k]   # the stock lid's frost art renders as crumpled sheets: keep ours

# ---------------------------------------------------------------- run
_pcol = {}
os.makedirs(os.path.join(OUT, 'data', 'shapes'), exist_ok=True)
report = {'made': [], 'skipped': {}}
done = set()
for name, sp, props in objs:
    key = sp.lower()
    if key in done: continue
    if (key in have or key.replace('_', '') in stock_norm) and key not in ARCH: continue
    if (name.lower() in OBJ_ARCH or key in YARD) and key in sprites:
        sname, w, h, img = sprites[key]
        mesh = YARD[key](w, h, img) if key in YARD else OBJ_ARCH[name.lower()](w, h, img)
        if mesh is not None:
            mesh.write(os.path.join(OUT, 'data', 'shapes', key + '.obj'), sname)
            report['made'].append((name, sname, 'outdoor')); done.add(key)
        continue
    if key in ('accountant', 'actor'):   # sprite index 0/1: slot 0 is the default for sprite-less things
        report['skipped'][name] = 'reserved sprite index'; continue
    if key not in sprites: report['skipped'][name] = 'no sprite image'; continue
    if key not in ARCH and props & SKIP_PROPS: report['skipped'][name] = 'category'; continue
    if key not in ARCH and not re.search(r'prayermat', name, re.I) and SKIP_NAMES.search(name): report['skipped'][name] = 'flat by design'; continue
    sname, w, h, img = sprites[key]
    hgt = height_for(name + ' ' + sp, props)
    if 'Entity' in props:
        if w != 2 or h != 2: report['skipped'][name] = 'odd entity size'; continue
        mesh = bust(sname, img)
        _bc0, _sk0, _tp0, _fo0 = _people_colours(img); _pcol[key] = {'name': sname, 'body': list(_bc0), 'skin': list(_sk0), 'top': list(_tp0), 'bare': False}
    elif key in ARCH:
        mesh = ARCH[key](w, h, img)
    elif generic_for(name, sname) and hgt != 'tree':
        mesh = generic_for(name, sname)(w, h, img, hgt * 100.0)
    elif hgt == 'tree':
        mesh = tree(sname, w, h, img)
    else:
        # place sprite art in the atlas (downscale big sprites to <=128 px)
        scale = min(1.0, 128.0 / max(img.size))
        tw, th = max(1, int(img.width * scale)), max(1, int(img.height * scale))
        spot = place((tw + 2 * BLK - 1) // BLK + 1, (th + 2 * BLK - 1) // BLK + 1)
        if spot is None: report['skipped'][name] = 'atlas full'; continue
        ox, oy = spot[0] + 4, spot[1] + 4
        art = img.resize((tw, th), Image.LANCZOS)
        # transparent pixels (the corners around a round table top, the gaps in a bed frame) took black and showed as black patches on the
        # 3D model: fill them with the art's own average colour instead, then bleed the edge colours a little
        _ra = art.convert('RGBA'); _al = _ra.getchannel('A'); _rgb = _ra.convert('RGB')
        _st = ImageStat.Stat(_rgb, mask=_al.point(lambda a: 255 if a > 128 else 0)); _mean = tuple(int(c) for c in _st.mean) if _st.count[0] > 0 else (128, 110, 90)
        bg = Image.new('RGB', art.size, _mean)
        solidc = _rgb.filter(ImageFilter.MaxFilter(5)); _edge = Image.new('RGB', art.size, _mean); _edge.paste(solidc, (0, 0), _al.filter(ImageFilter.MaxFilter(5)))
        bg.paste(_edge, (0, 0)); bg.paste(_rgb, (0, 0), _al)
        atlas.paste(bg, (ox, oy))
        if re.search(r'table|desk(?!.*station)|bench|counter|booth', name + ' ' + sname, re.I) and \
           not re.search(r'tennis|football|sorting|servingtable', name, re.I):
            mesh = table(sname, w, h, img, hgt, (ox, oy), scale, legs=not re.search(r'counter|booth', name, re.I))
        else:
            mesh = cookie(sname, w, h, img, hgt, (ox, oy), scale); COOKIES.add(key + '.obj')
            _g0 = GRID_PX
            while mesh is not None and len(mesh.f) > 700 and GRID_PX < 24:      # keep frame cost down: coarser outline for heavy meshes
                GRID_PX *= 2; mesh = cookie(sname, w, h, img, hgt, (ox, oy), scale)
            GRID_PX = _g0
    if mesh is None: report['skipped'][name] = 'empty sprite'; continue
    _s = sprite_scale.get(key, 1.0) * {'prayermat': 0.6}.get(key, 1.0)   # the mat art is the opaque part of a 4x4-cell sprite: 1.2 x was too big
    if abs(_s - 1.0) > 0.01 and _s > 0.05:
        mesh.v = [(x * _s, y * _s, z * _s) for (x, y, z) in mesh.v]
    mesh.write(os.path.join(OUT, 'data', 'shapes', key + '.obj'), sname); MAINW[key + '.obj'] = WRITERS[key + '.obj']
    if key == 'fridgelid':   # the stock file is called fridge_lid.obj: replace it too
        mesh.write(os.path.join(OUT, 'data', 'shapes', 'fridge_lid.obj'), sname)
    report['made'].append((name, sname, hgt))
    done.add(key)

# doors with no model of their own borrow the game's stock door model (same atlas region)
for _k, _src in STOCK_COPY.items():
    _p = os.path.join(shapes_dir, _src + '.obj')
    if _k in sprites and os.path.exists(_p):
        shutil.copyfile(_p, os.path.join(OUT, 'data', 'shapes', _k + '.obj')); report['made'].append((_k, _k, 'stock-copy'))

# single-item resource models (crates, bricks, concrete, planks...) are also drawn about twice the size of their 2D sprites
for _f in ('box.obj', 'brick.obj', 'concrete.obj', 'sheetmetal.obj', 'wood.obj', 'log.obj', 'bleach.obj', 'girder.obj'):
    _src = os.path.join(shapes_dir, _f)
    if os.path.exists(_src) and not os.path.exists(os.path.join(OUT, 'data', 'shapes', _f)):
        _out = []
        for _l in open(_src, encoding='latin1'):
            if _l.startswith('v '):
                _p = _l.split(); _l = 'v %.4f %.4f %.4f\n' % (float(_p[1]) * 0.6, float(_p[2]) * 0.6, float(_p[3]) * 0.6)
            _out.append(_l)
        open(os.path.join(OUT, 'data', 'shapes', _f), 'w', encoding='latin1').writelines(_out)

# Doors: the game's own door models are a full tile wide and swing like barn doors; make the leaves narrower. Scaled about the
# hinge / centre in x only (depth and height untouched); solitary and remote doors reuse these models.
DOOR_WIDTH = {'door': 1.0, 'staffdoor': 1.0, 'jaildoor': 1.0, 'jaildoor_frame': 1.0, 'jaildoorlarge': 1.0, 'jaildoorlarge_frame': 1.0, 'metaldetector': 0.55}   # width factors; heights use DOOR_HEIGHT
DOOR_HEIGHT = 1.0
def _scaled_x(src, factor, dst):
    _out = []
    for _l in open(src, encoding='latin1'):
        if _l.startswith('v '):
            _p = _l.split()
            _l = 'v %.4f %.4f %.4f' % (float(_p[1]) * factor, float(_p[2]) * DOOR_HEIGHT, float(_p[3])) + chr(10)
        _out.append(_l)
    open(dst, 'w', encoding='latin1').writelines(_out)
_stock_files = {_f.lower(): _f for _f in os.listdir(shapes_dir)}
for _n, _fac in DOOR_WIDTH.items():
    if _n + '.obj' in _stock_files:
        _scaled_x(os.path.join(shapes_dir, _stock_files[_n + '.obj']), _fac, os.path.join(OUT, 'data', 'shapes', _n + '.obj'))
for _k, _src in STOCK_COPY.items():
    _s = _src.lower()
    if _k in sprites and _s in DOOR_WIDTH and _s + '.obj' in _stock_files:
        _scaled_x(os.path.join(shapes_dir, _stock_files[_s + '.obj']), DOOR_WIDTH[_s], os.path.join(OUT, 'data', 'shapes', _k + '.obj'))

# The game's own resource-pile models (stackwood, stacksheetmetal, stackbrick...) are drawn about twice the size of the
# one-tile piles in the 2D game. Write scaled-down copies (UVs untouched, so they keep the original art).
STACK_SCALE = 0.55
for _f in sorted(os.listdir(shapes_dir)):
    if not (_f.lower().startswith('stack') and _f.lower().endswith('.obj')): continue
    if os.path.exists(os.path.join(OUT, 'data', 'shapes', _f.lower())): continue
    _lines = []
    _k = STACK_SCALE * (0.64 if 'foodtray' in _f.lower() else 1.0)     # food-tray stacks are the worst offenders: ~0.35 overall
    if 'stackingredients' in _f.lower(): _k = 1.05      # ingredient boxes are drawn about a tile wide in 2D (the kitchen looked empty): keep them full size
    for _l in open(os.path.join(shapes_dir, _f), encoding='latin1'):
        if _l.startswith('v '):
            _p = _l.split(); _l = 'v %.4f %.4f %.4f\n' % (float(_p[1]) * _k, float(_p[2]) * _k, float(_p[3]) * _k)
        _lines.append(_l)
    open(os.path.join(OUT, 'data', 'shapes', _f), 'w', encoding='latin1').writelines(_lines)
    report['made'].append((_f[:-4], _f[:-4], 'stack-scaled'))

# The 'Bench' object uses base sprite Bench (id 34): the game's own bench.obj is a flat slab, so write a real bench for it.
if 'bench' in sprites:
    _n, _w, _h, _img = sprites['bench']
    arch_bench(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', 'bench.obj'), _n)

# every tree sprite of the later banks gets the chunky tree archetype (the game has no 3D tree for them)
for _key, (_n, _w, _h, _img) in sprites.items():
    if _key.startswith('d_') and 'tree' in _key and 'toolbar' not in _key and not os.path.exists(os.path.join(OUT, 'data', 'shapes', _key + '.obj')):
        try: tree(_n, _w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _key + '.obj'), _n)
        except Exception as _e: pass

# ---------------------------------------------------------------- people sprites that are not an object's own sprite (prisoner variants, naked / tattooed bodies,
# duplicate staff skins, campaign characters): without a model the game draws nothing, i.e. invisible people
PEOPLE_RE = re.compile(r'^(PrisonerNaked\w*|PrisonerFemale\d?|TattooBody\w+|FemaleBody\d?|Priest\d|Soldier\d|Teacher\d|EliteOp\d|Farmer\d|ParoleBoardMember\d|ParoleLawyer\d|PalermoDaughter\d|'
                       r'GuardArmour\d?|SecurityGuard|Fig\d+|Warden\w+|Keane|CameraMan|NewsReporter|Kerry\w*|Morgan\w*|Benedict\w*|Ceo|Detective|Edward\w*|Heller|Henchman\w*|Hurst|Negotiator|Nico\w*|TheDon\w*|AnthonyIRL|SonnyIRL)$', re.I)
_ppl = 0
for _key, (_n, _w, _h, _img) in sprites.items():
    if not PEOPLE_RE.match(_n) or _w != 2 or _h != 2: continue
    _out = os.path.join(OUT, 'data', 'shapes', _key + '.obj')
    if os.path.exists(_out) or _key in have or _key.replace('_', '') in stock_norm: continue
    _ov = None
    if re.match(r'(?i)PrisonerFemale', _n): _ov = {'body': (232, 112, 28), 'top': (96, 64, 40)}          # female prisoner: the orange uniform
    elif False and re.match(r'(?i)PrisonerNaked', _n): _ov = {'body': (232, 190, 150), 'skin': (232, 190, 150), 'top': (80, 56, 40)}   # a naked prisoner: bare skin
    elif False and re.match(r'(?i)(TattooBody|FemaleBody)', _n):                                       # visitors use these body sprites: they are never naked, so dress them in ordinary clothes
        _pal = [(70, 96, 140), (120, 60, 64), (66, 110, 84), (150, 126, 80), (96, 84, 120), (60, 64, 72), (170, 90, 60), (110, 130, 150)]; _sdv = sum(ord(ch) for ch in _n)
        _ov = {'body': _pal[_sdv % len(_pal)], 'skin': (232, 190, 150), 'top': [(70, 48, 32), (30, 28, 30), (150, 110, 60), (110, 70, 40), (190, 160, 100)][_sdv % 5]}
    try:
        bust(_n, _img, _ov).write(_out, _n); _ppl += 1
        _bc, _sk, _tp, _fo = _people_colours(_img); _pcol[_key] = {'name': _n, 'body': list(_ov['body'] if _ov and 'body' in _ov else _bc), 'skin': list(_ov['skin'] if _ov and 'skin' in _ov else _sk), 'top': list(_ov['top'] if _ov and 'top' in _ov else _tp), 'bare': bool(_ov and _ov.get('body') == _ov.get('skin'))}
    except Exception as _e: print('people model failed', _n, _e)
report['made'].append(('people-variants', str(_ppl), 'bust'))
json.dump(_pcol, open(os.path.join(OUT, 'people_colours.json'), 'w'))
# ---------------------------------------------------------------- batch: wall fittings and tray stacks
def _plate(m, X0, X1, Y0, Y1, Z0, d, col):
    box(m, X0, X1, Y0, Y1, Z0, Z0 + d, swatch(col))
def wall_light(w, h, img, solar=False):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); cx = (X0 + X1) / 2
    _plate(m, cx - 14, cx + 14, 112, 148, Z0, 4, (90, 94, 102))
    box(m, cx - 9, cx + 9, 118, 142, Z0 + 4, Z0 + 18, swatch((250, 232, 150)))
    box(m, cx - 11, cx + 11, 142, 146, Z0 + 4, Z0 + 20, swatch((70, 74, 80)))
    if solar: box(m, cx - 12, cx + 12, 150, 153, Z0, Z0 + 14, swatch((34, 62, 130)))
    return m
def painting(w, h, img, tone):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); cx = (X0 + X1) / 2
    wd = min(X1 - X0, 80) / 2
    _plate(m, cx - wd, cx + wd, 84, 144, Z0, 5, (116, 80, 44))
    _plate(m, cx - wd + 5, cx + wd - 5, 89, 139, Z0 + 5, 2, tone)
    _plate(m, cx - wd + 12, cx + wd - 12, 118, 130, Z0 + 7, 1, shade(tone, 1.3))
    return m
def sign_plate(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); bc = body_colour(img) if img is not None else (200, 200, 200)
    _plate(m, X0 + 4, X1 - 4, 92, 126, Z0, 4, (70, 72, 78))
    _plate(m, X0 + 8, X1 - 8, 96, 122, Z0 + 4, 2, bc)
    return m
def tray_stack(n, dirty):
    m = Mesh(); c = swatch
    col = (150, 138, 112) if dirty else (206, 210, 216)
    for k in range(n):
        y = k * 3.2; jx = ((k * 7) % 5) - 2; jz = ((k * 11) % 5) - 2
        box(m, -26 + jx, 26 + jx, y, y + 2.6, -15 + jz, 15 + jz, c(col))
        box(m, -26 + jx, -24 + jx, y + 2.6, y + 4.2, -15 + jz, 15 + jz, c(shade(col, 0.85)))
        box(m, 24 + jx, 26 + jx, y + 2.6, y + 4.2, -15 + jz, 15 + jz, c(shade(col, 0.85)))
    return m
_wall_items = {'d_walllight': lambda w, h, i: wall_light(w, h, i), 'd_solarwalllight': lambda w, h, i: wall_light(w, h, i, True),
               'd_paintingfull1': lambda w, h, i: painting(w, h, i, (70, 110, 150)), 'd_paintingfull2': lambda w, h, i: painting(w, h, i, (140, 100, 60))}
_wall_items.update({'d_' + k: sign_plate for k in ('armourysign', 'leftarrowsign', 'rightarrowsign', 'canteensign', 'infirmarysign', 'showersign', 'toiletsign',
    'bakerysign', 'gymsign', 'noentrysign', 'phonesign', 'restaurantsign', 'visitationsign')})
for _k, _fn in _wall_items.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'wall-fitting'))
        except Exception as _e: print('wall fitting failed', _k, _e)
for _base, _dirty in (('stackfoodtrays', False), ('stackfoodtraysdirty', True)):
    for _suffix, _n in (('', 5), ('b', 9), ('c', 14)):
        tray_stack(_n, _dirty).write(os.path.join(OUT, 'data', 'shapes', _base + _suffix + '.obj'), _base)
# ---------------------------------------------------------------- batch 2: more windows, fences and decor
def barbed_wire(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cz = (Z0 + Z1) / 2
    for lx in (X0 + 4, X1 - 8): box(m, lx, lx + 4, 0, 40, cz - 2, cz + 2, c((80, 80, 86)))
    for y in (14, 28, 40): box(m, X0, X1, y, y + 1.5, cz - 1, cz + 1, c((150, 154, 160)))
    n = max(4, int((X1 - X0) // 12))
    for k in range(n):
        x = X0 + 6 + (X1 - X0 - 12) * k / max(1, n - 1)
        for y in (14, 28, 40): box(m, x - 1.5, x + 1.5, y - 2, y + 3.5, cz - 3, cz + 3, c((120, 124, 130)))
    return m
def paper_dragon(w, h, img, part):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx = (X0 + X1) / 2; zc = (Z0 + Z1) / 2
    red = (196, 40, 40); gold = (230, 190, 60)
    if part == 'head':
        lathe(m, [(14, 18), (22, 30), (22, 46), (12, 58), (0, 60)], 10, c(red), cx=cx, cz=zc)
        box(m, cx - 6, cx + 6, 26, 32, zc + 14, zc + 34, c(gold))
        for s in (-1, 1): box(m, cx + s * 14 - 2, cx + s * 14 + 2, 52, 76, zc - 3, zc + 3, c(gold))
    else:
        lathe(m, [(18, 14), (20, 30), (18, 46), (10, 52), (0, 54)], 10, c(red if part == 'body' else shade(red, 0.85)), cx=cx, cz=zc)
        box(m, cx - 2, cx + 2, 46, 62, zc - 8, zc + 8, c(gold))
    return m
def climbing_rope(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx = (X0 + X1) / 2; zc = (Z0 + Z1) / 2
    lathe(m, [(1.8, 0), (1.8, 150)], 6, c((176, 140, 90)), cx=cx, cz=zc)
    for k in range(8): box(m, cx - 2.4, cx + 2.4, 8 + k * 18, 11 + k * 18, zc - 2.4, zc + 2.4, c((140, 108, 66)))
    box(m, cx - 14, cx + 14, 146, 152, zc - 3, zc + 3, c((80, 80, 86)))
    return m
def shop_front(w, h, img):
    """a shop kiosk: wooden posts, a panelled counter with goods, shelves of stock behind it, a painted sign board and a striped awning over the
    customer side (+z). Exactly the 3D wall's depth, so it also fits a wall opening."""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    H, CT = 168, 80
    wood, woodd, woodl, sign, signd = c((150, 100, 56)), c((104, 66, 36)), c((186, 136, 84)), c((40, 92, 140)), c((240, 222, 120))
    red, white = c((200, 50, 50)), c((240, 236, 226))
    zb, zf = -50.0, 50.0      # a little deeper than the 3D wall (+-37.5), so its faces stand just outside a wall it sits in
    for x0 in (X0, X1 - 8):
        box(m, x0, x0 + 8, 0, H, zb, zb + 8, woodd); box(m, x0, x0 + 8, 0, H, zf - 8, zf, woodd)          # four corner posts
    box(m, X0 + 8, X1 - 8, 0, CT, zf - 10, zf - 4, wood)                                                    # counter front
    for k in range(3): x = X0 + 8 + (X1 - X0 - 16) * (k + 0.5) / 3; box(m, x - (X1 - X0 - 16) / 7, x + (X1 - X0 - 16) / 7, 12, CT - 12, zf - 4, zf - 3, woodl)   # panels
    box(m, X0 + 2, X1 - 2, CT, CT + 5, zf - 22, zf + 2, woodl)                                              # counter top
    box(m, X0 + 8, X1 - 8, 0, H - 30, zb + 2, zb + 6, woodd)                                                # back board
    goods = [c((210, 80, 70)), c((70, 130, 200)), c((230, 200, 80)), c((90, 170, 100)), c((200, 120, 190)), c((240, 150, 60))]
    ng = max(4, int((X1 - X0 - 20) // 14))
    for r, y in enumerate((40, 76, 112)):                                                                   # shelves of stock behind the counter
        box(m, X0 + 8, X1 - 8, y, y + 3, zb + 6, zb + 22, woodl)
        for k in range(ng):
            gx = X0 + 10 + k * (X1 - X0 - 20) / ng; box(m, gx, gx + (X1 - X0 - 20) / ng - 3, y + 3, y + 12 + (k * 5 + r * 3) % 8, zb + 8, zb + 20, goods[(k + r) % len(goods)])
    for k in range(ng):                                                                                     # goods on the counter
        gx = X0 + 10 + k * (X1 - X0 - 20) / ng
        if k % 2 == 0: box(m, gx, gx + (X1 - X0 - 20) / ng - 4, CT + 5, CT + 12, zf - 18, zf - 8, goods[(k + 3) % len(goods)])
    box(m, X0, X1, H - 30, H, zf - 6, zf - 2, sign); box(m, X0 + 6, X1 - 6, H - 22, H - 8, zf - 2, zf - 1.4, signd)   # sign board
    box(m, X0, X1, H - 4, H, zb, zf - 2, woodd)                                                             # roof rail
    n = max(5, int((X1 - X0) // 14))
    for k in range(n):                                                                                      # striped awning sloping out over the customers
        x0 = X0 + (X1 - X0) * k / n; x1 = X0 + (X1 - X0) * (k + 1) / n; col = red if k % 2 == 0 else white
        box(m, x0, x1, H - 36, H - 30, zf - 2, zf + 12, col)
        box(m, x0, x1, H - 44, H - 36, zf + 10, zf + 22, col)
        box(m, x0, x1, H - 56, H - 44, zf + 20, zf + 24, col)
    return m
def glass_window(w, h, img, frame, stained=False):
    return arch_window(w, h, img, frame=frame, mullion=max(1, w - 1), stained=stained)
_batch2 = {'barbedwire': barbed_wire, 'climbingrope': climbing_rope, 'shopfront': shop_front,
           'd_paperdragonhead': lambda w, h, i: paper_dragon(w, h, i, 'head'), 'd_paperdragonbody': lambda w, h, i: paper_dragon(w, h, i, 'body'), 'd_paperdragontail': lambda w, h, i: paper_dragon(w, h, i, 'tail'),
           'd_windowclassy': lambda w, h, i: glass_window(w, h, i, (226, 220, 206)), 'd_windowclassylarge': lambda w, h, i: glass_window(w, h, i, (226, 220, 206)),
           'd_windowglass': lambda w, h, i: glass_window(w, h, i, (150, 160, 168)), 'd_windowglasslarge': lambda w, h, i: glass_window(w, h, i, (150, 160, 168)),
           'd_woodenwindowsmall': lambda w, h, i: glass_window(w, h, i, (110, 74, 40)), 'd_woodenwindowlarge': lambda w, h, i: glass_window(w, h, i, (110, 74, 40))}
for _k, _fn in _batch2.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch2'))
        except Exception as _e: print('batch2 failed', _k, _e)
# ---------------------------------------------------------------- batch 3: utilities and ground details that were left flat
def b3_fire(w, h, img, s=1.0):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    for dx, dz, ang in ((-14, 0, 0), (14, 4, 1), (0, -12, 2), (2, 12, 3)):
        box(m, cx + dx * s - 14 * s, cx + dx * s + 14 * s, 0, 8 * s, cz + dz * s - 4 * s, cz + dz * s + 4 * s, c((70, 44, 26)))
    lathe(m, [(16 * s, 4), (20 * s, 14 * s), (10 * s, 42 * s), (0, 62 * s)], 8, c((236, 92, 24)), cx=cx, cz=cz)
    lathe(m, [(9 * s, 8), (11 * s, 18 * s), (5 * s, 38 * s), (0, 50 * s)], 8, c((252, 200, 60)), cx=cx, cz=cz)
    return m
def b3_drain(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    box(m, X0 + 4, X1 - 4, 0, 2.5, Z0 + 4, Z1 - 4, c((58, 60, 66)))
    n = max(4, int((X1 - X0) // 9))
    for k in range(n):
        x = X0 + 8 + (X1 - X0 - 16) * k / max(1, n - 1); box(m, x - 1.2, x + 1.2, 2.5, 4, Z0 + 6, Z1 - 6, c((120, 124, 132)))
    return m
def b3_console(w, h, img, col=(60, 64, 72), lamp=(90, 220, 110)):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    box(m, cx - 18, cx + 18, 0, 20, cz - 14, cz + 14, c(col))
    box(m, cx - 14, cx + 14, 20, 30, cz - 12, cz + 8, c(shade(col, 0.8)))
    box(m, cx - 10, cx + 10, 22, 27, cz + 8, cz + 11, c(lamp))
    return m
def b3_pad(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    box(m, X0 + 3, X1 - 3, 0, 3, Z0 + 3, Z1 - 3, c((96, 100, 108)))
    box(m, X0 + 9, X1 - 9, 3, 4.5, Z0 + 9, Z1 - 9, c((200, 60, 50)))
    return m
def b3_lamp(w, h, img, col):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    lathe(m, [(7, 0), (7, 3), (2.5, 6), (2.5, 46)], 8, c((70, 72, 78)), cx=cx, cz=cz)
    box(m, cx - 8, cx + 8, 46, 62, cz - 8, cz + 8, c(col))
    box(m, cx - 10, cx + 10, 62, 66, cz - 10, cz + 10, c((70, 72, 78)))
    return m
def b3_turf(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    box(m, X0 + 2, X1 - 2, 0, 5, Z0 + 2, Z1 - 2, c((110, 84, 50)))
    box(m, X0 + 2, X1 - 2, 5, 9, Z0 + 2, Z1 - 2, c((74, 150, 56)))
    return m
_b3 = {'fire': lambda w, h, i: b3_fire(w, h, i), 'strikefire': lambda w, h, i: b3_fire(w, h, i, 0.7), 'drain': b3_drain, 'wooddrain': b3_drain,
       'doortimer': lambda w, h, i: b3_console(w, h, i), 'statuslight': lambda w, h, i: b3_lamp(w, h, i, (90, 220, 110)),
       'lightgreen': lambda w, h, i: b3_lamp(w, h, i, (90, 220, 110)), 'lightred': lambda w, h, i: b3_lamp(w, h, i, (230, 70, 60)),
       'pressurepad': b3_pad, 'grassturf': b3_turf}
_sprite_alias = {'doortimer': 'doortimer', 'lightgreen': 'lightgreen', 'lightred': 'lightred', 'grassturf': 'grassturf', 'strikefire': 'strikefire'}
for _k, _fn in _b3.items():
    _sk = {'wooddrain': 'd_wooddrain', 'lightgreen': 'd_lightgreen', 'lightred': 'd_lightred', 'rubble': 'rubble1'}.get(_k, _k)
    if _sk in sprites and not os.path.exists(os.path.join(OUT, 'data', 'shapes', _sk + '.obj')):
        _n, _w, _h, _img = sprites[_sk]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _sk + '.obj'), _n); report['made'].append((_k, _sk, 'batch3'))
        except Exception as _e: print('batch3 failed', _k, _e)
# serving-table lid: the game draws it as a flat sprite on top of the 3D serving table. A closed steel lid slab with a handle, at the table-top height.
if 'servingtablelid' in sprites:
    _n, _w, _h, _img = sprites['servingtablelid']
    _X0, _X1, _Z0, _Z1 = frame_of(_w, _h, _img); _m = Mesh(); _c = swatch; _cx, _zc = (_X0 + _X1) / 2, (_Z0 + _Z1) / 2
    _top = 77.0
    box(_m, _X0 + 4, _X1 - 4, _top, _top + 5, _Z0 + 6, _Z1 - 6, _c((200, 204, 210)))
    box(_m, _X0 + 2, _X1 - 2, _top, _top + 2, _Z0 + 4, _Z1 - 4, _c((150, 154, 160)))
    for _hx in (_X0 + (_X1 - _X0) * 0.25, _X0 + (_X1 - _X0) * 0.75): box(_m, _hx - 8, _hx + 8, _top + 5, _top + 9, _zc - 2, _zc + 2, _c((70, 72, 78)))
    _m.write(os.path.join(OUT, 'data', 'shapes', 'servingtablelid.obj'), _n)
    report['made'].append(('ServingTableLid', 'servingtablelid', 'batch3'))
# ---------------------------------------------------------------- batch 4: switches, valves, banners
def b4_switch(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    box(m, cx - 18, cx + 18, 0, 6, cz - 16, cz + 16, c((70, 72, 78)))
    box(m, cx - 14, cx + 14, 6, 40, cz - 10, cz + 10, c((184, 188, 196)))
    box(m, cx - 10, cx + 10, 40, 44, cz - 8, cz + 8, c((120, 124, 132)))
    box(m, cx - 3, cx + 3, 44, 66, cz - 3, cz + 3, c((210, 50, 40)))
    box(m, cx - 8, cx + 8, 64, 72, cz - 5, cz + 5, c((210, 50, 40)))
    return m
def b4_valve(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    box(m, X0, X1, 6, 20, cz - 6, cz + 6, c((92, 98, 108)))
    box(m, cx - 5, cx + 5, 20, 34, cz - 5, cz + 5, c((92, 98, 108)))
    lathe(m, [(14, 34), (14, 38), (11, 40), (11, 42)], 10, c((200, 50, 40)), cx=cx, cz=cz)
    box(m, cx - 14, cx + 14, 36, 40, cz - 1.5, cz + 1.5, c((200, 50, 40))); box(m, cx - 1.5, cx + 1.5, 36, 40, cz - 14, cz + 14, c((200, 50, 40)))
    return m
def b4_banner(w, h, img, col):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    for lx in (X0 + 6, X1 - 10): box(m, lx, lx + 4, 0, 120, cz - 2, cz + 2, c((90, 92, 98)))
    box(m, X0 + 4, X1 - 4, 112, 118, cz - 2, cz + 2, c((90, 92, 98)))
    box(m, X0 + 8, X1 - 8, 52, 112, cz - 1.5, cz + 1.5, c(col))
    box(m, X0 + 8, X1 - 8, 104, 112, cz - 2, cz + 2, c(shade(col, 0.8)))
    return m
def b4_van(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    wd = X1 - X0; d = Z1 - Z0
    box(m, X0 + wd * 0.05, X1 - wd * 0.05, 14, 70, Z0 + d * 0.08, Z1 - d * 0.08, c((236, 236, 232)))
    box(m, X1 - wd * 0.30, X1 - wd * 0.05, 44, 86, Z0 + d * 0.1, Z1 - d * 0.1, c((236, 236, 232)))
    box(m, X1 - wd * 0.24, X1 - wd * 0.06, 56, 80, Z0 + d * 0.12, Z1 - d * 0.12, c((90, 120, 150)))
    box(m, X0 + wd * 0.05, X1 - wd * 0.05, 30, 40, Z0 + d * 0.08 - 0.5, Z1 - d * 0.08 + 0.5, c((60, 130, 190)))
    for fx in (X0 + wd * 0.22, X1 - wd * 0.2):
        for s2 in (Z0 + d * 0.06, Z1 - d * 0.06): lathe(m, [(12, 0), (12, 14)], 10, c((40, 42, 46)), cx=fx, cz=s2)
    return m
_b4 = {'electricalswitch': b4_switch, 'pipevalve': b4_valve, 'pipevalve_lagged': b4_valve, 'banner': lambda w, h, i: b4_banner(w, h, i, (170, 40, 44)),
       'd_coupletbanner': lambda w, h, i: b4_banner(w, h, i, (190, 30, 30)), 'd_drainagevan': b4_van}
for _k, _fn in _b4.items():
    if _k in sprites and not os.path.exists(os.path.join(OUT, 'data', 'shapes', _k + '.obj')):
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch4'))
        except Exception as _e: print('batch4 failed', _k, _e)
# ---------------------------------------------------------------- batch 6: proper models for things the generic sprite block made crude
# (computer station, standing fan, electric chair, large TV). Units: 100 per tile, y up, front = +z. These replace the generic models.
def zcyl(m, cx, cy, z0, z1, r, col, segs=16, caps=True):
    """cylinder lying along z (front-facing discs, fan housings)"""
    t = [col] * 4
    for i in range(segs):
        a0 = 2 * math.pi * i / segs; a1 = 2 * math.pi * (i + 1) / segs
        p0 = (cx + math.cos(a0) * r, cy + math.sin(a0) * r); p1 = (cx + math.cos(a1) * r, cy + math.sin(a1) * r)
        am = (a0 + a1) / 2; n = (math.cos(am), math.sin(am), 0)
        m.quad([(p0[0], p0[1], z0), (p1[0], p1[1], z0), (p1[0], p1[1], z1), (p0[0], p0[1], z1)], n, t)
        if caps:
            m.quad([(cx, cy, z1), (p0[0], p0[1], z1), (p1[0], p1[1], z1), (cx, cy, z1)], (0, 0, 1), t)
            m.quad([(cx, cy, z0), (p1[0], p1[1], z0), (p0[0], p0[1], z0), (cx, cy, z0)], (0, 0, -1), t)
def zring(m, cx, cy, zc, R, th, dep, col, segs=24):
    """a flat ring facing +z (fan guard rims): R = radius, th = bar thickness, dep = depth"""
    t = [col] * 4; z0, z1 = zc - dep / 2, zc + dep / 2
    for i in range(segs):
        a0 = 2 * math.pi * i / segs; a1 = 2 * math.pi * (i + 1) / segs
        o0 = (cx + math.cos(a0) * (R + th), cy + math.sin(a0) * (R + th)); o1 = (cx + math.cos(a1) * (R + th), cy + math.sin(a1) * (R + th))
        i0 = (cx + math.cos(a0) * R, cy + math.sin(a0) * R); i1 = (cx + math.cos(a1) * R, cy + math.sin(a1) * R)
        am = (a0 + a1) / 2
        m.quad([(i0[0], i0[1], z1), (o0[0], o0[1], z1), (o1[0], o1[1], z1), (i1[0], i1[1], z1)], (0, 0, 1), t)
        m.quad([(o0[0], o0[1], z0), (i0[0], i0[1], z0), (i1[0], i1[1], z0), (o1[0], o1[1], z0)], (0, 0, -1), t)
        m.quad([(o0[0], o0[1], z0), (o1[0], o1[1], z0), (o1[0], o1[1], z1), (o0[0], o0[1], z1)], (math.cos(am), math.sin(am), 0), t)
def b6_computer(w, h, img):
    """the game's sprite: a desk across the +z half with an old beige computer facing the chair, and the chair in the -z half"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    wood, wedge, wdark, metal = c((156, 112, 70)), c((126, 88, 52)), c((96, 66, 40)), c((150, 152, 156))
    beige, beiged, scr, scr2, keys = c((222, 214, 190)), c((188, 180, 156)), c((40, 70, 60)), c((120, 200, 150)), c((200, 192, 170))
    cx = (X0 + X1) / 2; wd = X1 - X0; zm = Z0 + (Z1 - Z0) * 0.45; top = 72
    dz0, dz1 = zm + 6, Z1 - 6                                                                     # a small desk
    box(m, X0 + 2, X1 - 2, top - 4, top, dz0, dz1, wood); box(m, X0 + 2, X1 - 2, top - 6, top - 4, dz0, dz0 + 2, wedge)
    for sx in (X0 + 4, X1 - 8):
        box(m, sx, sx + 4, 0, top - 6, dz0 + 3, dz0 + 7, wdark); box(m, sx, sx + 4, 0, top - 6, dz1 - 7, dz1 - 3, wdark)   # legs
    box(m, X0 + 6, X1 - 6, top - 16, top - 6, dz1 - 6, dz1 - 3, wdark)                            # back rail
    box(m, X0 + 8, X0 + 8 + min(28.0, wd * 0.4), top - 14, top - 6, dz0 + 3, dz0 + 26, wedge)     # drawer
    box(m, X0 + 14, X0 + 22, top - 11, top - 9, dz0 + 1, dz0 + 3, metal)
    # the computer: a beige CRT with its screen towards the chair (-z), keyboard and mouse in front of it
    mz1 = dz1 - 5; mz0 = mz1 - 26; mw = min(36.0, wd * 0.5)
    box(m, cx - mw / 2, cx + mw / 2, top, top + 4, mz0 - 2, mz1, beiged)                          # base unit
    box(m, cx - mw / 2 + 2, cx + mw / 2 - 2, top + 4, top + 38, mz0 + 6, mz1 - 2, beige)          # CRT body (deep)
    box(m, cx - mw / 2 + 1, cx + mw / 2 - 1, top + 4, top + 40, mz0, mz0 + 6, beige)              # front bezel
    box(m, cx - mw / 2 + 5, cx + mw / 2 - 5, top + 9, top + 35, mz0 - 0.6, mz0, scr)              # screen (faces -z)
    for r in range(4): box(m, cx - mw / 2 + 8, cx - mw / 2 + 8 + 14 + (r * 7) % 9, top + 29 - r * 5, top + 30.5 - r * 5, mz0 - 0.9, mz0 - 0.6, scr2)   # green text
    box(m, cx + mw / 2 - 9, cx + mw / 2 - 6, top + 5, top + 7, mz0 - 0.6, mz0, c((90, 200, 90)))  # power light
    kz1 = mz0 - 6; kz0 = kz1 - 14
    box(m, cx - 20, cx + 20, top, top + 3, kz0, kz1, beiged)
    for r in range(3): box(m, cx - 18, cx + 18, top + 3, top + 3.8, kz0 + 2 + r * 4, kz0 + 4.6 + r * 4, keys)
    box(m, cx + 25, cx + 32, top, top + 3, kz0 + 2, kz0 + 11, beige)                              # mouse
    box(m, cx + 27, cx + 30, top + 1, top + 2, kz0 + 11, mz0, c((60, 60, 60)))                    # its lead
    # office chair facing the desk
    chz = Z0 + (zm - Z0) * 0.55; seat, frame_ = c((52, 70, 110)), c((50, 50, 54))
    for k in range(5):
        a = math.radians(90 + 72 * k); ex, ez = cx + math.cos(a) * 18, chz + math.sin(a) * 18
        box(m, min(cx, ex) - 1.5, max(cx, ex) + 1.5, 3, 6, min(chz, ez) - 1.5, max(chz, ez) + 1.5, frame_)
        lathe(m, [(0, 0), (2.4, 0), (2.4, 3), (0, 3)], 8, frame_, cx=ex, cz=ez)                   # casters
    lathe(m, [(2.6, 6), (2.6, 40)], 10, metal, cx=cx, cz=chz)                                      # gas lift
    box(m, cx - 17, cx + 17, 40, 46, chz - 16, chz + 16, seat)                                     # seat
    box(m, cx - 15, cx + 15, 50, 88, chz - 20, chz - 15, seat)                                     # back rest
    box(m, cx - 3, cx + 3, 44, 52, chz - 18, chz - 14, frame_)
    return m
def b6_fan(w, h, img):
    m = Mesh(); c = swatch
    base, pole, body, cage, blade, hub = c((52, 54, 60)), c((176, 178, 182)), c((210, 212, 214)), c((190, 194, 198)), c((140, 200, 220)), c((70, 120, 140))
    lathe(m, [(0, 0), (24, 0), (24, 2), (20, 6), (6, 8), (0, 8)], 22, base)                    # weighted round base
    lathe(m, [(2.6, 8), (2.6, 100)], 12, pole); lathe(m, [(3.6, 100), (3.6, 106)], 12, hub)    # pole and height collar
    lathe(m, [(2.2, 106), (2.2, 116)], 12, pole)
    cy = 122
    zcyl(m, 0, cy, -22, -6, 9, body, 16)                                                        # motor housing behind the blades
    zcyl(m, 0, cy, -26, -22, 6, body, 14)
    for k in range(3):                                                                          # three swept blades
        a = math.radians(90 + 120 * k); ca, sa = math.cos(a), math.sin(a)
        pts = [(ca * 4 - sa * 5, sa * 4 + ca * 5), (ca * 27 - sa * 9, sa * 27 + ca * 9), (ca * 29 + sa * 4, sa * 29 - ca * 4), (ca * 4 + sa * 5, sa * 4 - ca * 5)]
        m.quad([(x, cy + y, 0.5) for x, y in pts], (0, 0, 1), [blade] * 4)
        m.quad([(x, cy + y, -0.5) for x, y in reversed(pts)], (0, 0, -1), [blade] * 4)
    zcyl(m, 0, cy, -4, 3, 5, hub, 12)                                                          # spinner
    for zc in (-5, 7): zring(m, 0, cy, zc, 31, 2, 2, cage, 18)                                 # guard rims, back and front
    zring(m, 0, cy, 1, 32, 1.4, 12, cage, 18)                                                  # guard band joining them
    for zc in (-5, 7):
        if zc > 0: zring(m, 0, cy, zc, 17, 0.8, 1, cage, 14)                          # inner guard rings
    for k in range(8):                                                                          # guard spokes, front
        a = math.radians(k * 45); x, y = math.cos(a) * 31, math.sin(a) * 31
        n = 3
        for i in range(n):
            x0, y0 = x * i / n, y * i / n; x1, y1 = x * (i + 1) / n, y * (i + 1) / n
            box(m, min(x0, x1) - 0.7, max(x0, x1) + 0.7, cy + min(y0, y1) - 0.7, cy + max(y0, y1) + 0.7, 6.5, 8, cage)
    return m
def round_table(w, h, img, plates=True):
    """a round or oval table with a turned pedestal: the top follows the sprite's footprint"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; bc = body_of(img)
    wood = c(bc); edge = c(tuple(int(v * 0.78) for v in bc)); dark = c(tuple(int(v * 0.6) for v in bc))
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; rx, rz = (X1 - X0) / 2 - 1, (Z1 - Z0) / 2 - 1; r = min(rx, rz); sx, sz = rx / r, rz / r
    top = 72
    lathe(m, [(0, top - 3), (r - 2, top - 3), (r, top - 1.5), (r, top), (0, top)], 26, wood, cx=cx, cz=cz, sx=sx, sz=sz)
    lathe(m, [(r - 1, top - 3), (r - 1, top - 6), (r - 6, top - 6), (r - 6, top - 3)], 26, edge, cx=cx, cz=cz, sx=sx, sz=sz)   # apron under the edge
    lathe(m, [(5, 4), (5, top - 6), (9, top - 6)], 14, dark, cx=cx, cz=cz)                       # pedestal column
    lathe(m, [(0, 0), (min(r * 0.6, 26), 0), (min(r * 0.6, 26), 3), (8, 8), (0, 8)], 18, dark, cx=cx, cz=cz, sx=sx, sz=sz)      # spreading foot
    if plates:
        n = 4 if r > 26 else 2
        for k in range(n):
            a = 2 * math.pi * k / n + 0.4; px_, pz_ = cx + math.cos(a) * r * 0.58 * sx, cz + math.sin(a) * r * 0.58 * sz
            lathe(m, [(0, top), (7, top), (8, top + 1.4), (0, top + 1.4)], 14, c((236, 236, 230)), cx=px_, cz=pz_)
    return m
def b6_electric_chair(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    wood, wdark, strap, metal, cable, plat = c((112, 74, 42)), c((78, 50, 28)), c((40, 34, 30)), c((150, 154, 160)), c((30, 30, 34)), c((90, 92, 96))
    cx = (X0 + X1) / 2; cz = (Z0 + Z1) / 2; hw = min(36.0, (X1 - X0) / 2); hd = min(36.0, (Z1 - Z0) / 2)
    box(m, cx - hw - 8, cx + hw + 8, 0, 6, cz - hd - 8, cz + hd + 8, plat)                    # raised platform
    for sx in (-1, 1):
        for sz in (-1, 1): box(m, cx + sx * (hw - 6) - 4, cx + sx * (hw - 6) + 4, 6, 44, cz + sz * (hd - 6) - 4, cz + sz * (hd - 6) + 4, wdark)   # heavy legs
    box(m, cx - hw, cx + hw, 44, 52, cz - hd, cz + hd, wood)                                   # seat
    box(m, cx - hw, cx + hw, 52, 140, cz - hd, cz - hd + 9, wood)                              # tall back
    box(m, cx - hw + 6, cx + hw - 6, 140, 150, cz - hd, cz - hd + 9, wdark)                    # back top rail
    for sx in (-1, 1):
        box(m, cx + sx * hw - 5, cx + sx * hw + 5, 52, 76, cz + hd - 10, cz + hd - 2, wdark)   # arm posts
        box(m, cx + sx * hw - 6, cx + sx * hw + 6, 76, 82, cz - hd + 6, cz + hd, wood)          # arm rests
        box(m, cx + sx * hw - 7, cx + sx * hw + 7, 78, 84, cz + 6, cz + 14, strap)              # wrist straps
        box(m, cx + sx * (hw - 10) - 7, cx + sx * (hw - 10) + 7, 14, 20, cz + hd - 4, cz + hd + 2, strap)   # ankle straps
    box(m, cx - hw + 2, cx + hw - 2, 96, 104, cz - hd + 9, cz - hd + 11, strap)               # chest strap
    lathe(m, [(0, 160), (14, 162), (17, 172), (12, 182), (0, 184)], 14, metal, cx=cx, cz=cz - hd + 18)      # the cap
    box(m, cx - 2, cx + 2, 150, 182, cz - hd + 2, cz - hd + 6, metal)                          # cap arm from the back
    box(m, cx + 14, cx + 18, 6, 176, cz - hd - 2, cz - hd + 2, cable)                          # cable to the cap
    return m
def b6_large_tv(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    cab, cabd, body, scr, glow, leg = c((120, 84, 52)), c((84, 58, 34)), c((36, 36, 40)), c((26, 34, 46)), c((90, 140, 190)), c((60, 60, 64))
    zb, zf = Z0 + 4, Z1 - 4; wd = X1 - X0
    box(m, X0 + 2, X1 - 2, 0, 40, zb, zf, cab)                                                 # low cabinet
    box(m, X0 + 6, (X0 + X1) / 2 - 2, 6, 34, zf, zf + 1, cabd); box(m, (X0 + X1) / 2 + 2, X1 - 6, 6, 34, zf, zf + 1, cabd)   # doors
    box(m, (X0 + X1) / 2 - 12, (X0 + X1) / 2 + 12, 40, 46, zb + 6, zb + 22, leg)              # TV foot
    box(m, (X0 + X1) / 2 - 3, (X0 + X1) / 2 + 3, 46, 56, zb + 12, zb + 16, leg)
    tw = wd * 0.46
    box(m, (X0 + X1) / 2 - tw, (X0 + X1) / 2 + tw, 54, 54 + tw * 1.12, zb + 8, zb + 16, body)                  # TV body
    box(m, (X0 + X1) / 2 - tw + 4, (X0 + X1) / 2 + tw - 4, 58, 50 + tw * 1.12, zb + 16, zb + 17, scr)         # screen
    box(m, (X0 + X1) / 2 - tw, (X0 + X1) / 2 + tw, 50, 54, zb + 9, zb + 17, body)                             # speaker bar under the screen
    box(m, (X0 + X1) / 2 + tw - 10, (X0 + X1) / 2 + tw - 7, 51, 53, zb + 17, zb + 17.4, glow)                  # standby light
    return m
def b6_bed(w, h, img, style):
    """old: a sagging metal-frame bed with a thin ticking mattress and a folded blanket; comfy: a wooden bed with a thick mattress, duvet and two pillows. Head at -z."""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    wd, d = X1 - X0, Z1 - Z0
    if style == 'old':
        tube, mat, matd, blank, blankd, pil = c((70, 72, 76)), c((196, 196, 188)), c((176, 180, 184)), c((104, 110, 78)), c((80, 86, 58)), c((214, 212, 204))
        for sx in (X0 + 1, X1 - 5):
            for sz in (Z0 + 1, Z1 - 5): box(m, sx, sx + 4, 0, 42, sz, sz + 4, tube)                                  # legs
            box(m, sx, sx + 4, 34, 38, Z0 + 1, Z1 - 1, tube)                                                         # side rails
        box(m, X0 + 1, X1 - 1, 34, 38, Z0 + 1, Z0 + 5, tube); box(m, X0 + 1, X1 - 1, 34, 38, Z1 - 5, Z1 - 1, tube)
        box(m, X0 + 1, X1 - 1, 64, 67, Z0 + 1, Z0 + 4, tube); box(m, X0 + 1, X1 - 1, 52, 55, Z1 - 4, Z1 - 1, tube)    # head and foot rails
        for k in range(1, 6):
            x = X0 + wd * k / 6; box(m, x - 1, x + 1, 38, 64, Z0 + 1.5, Z0 + 3.5, tube); box(m, x - 1, x + 1, 38, 52, Z1 - 3.5, Z1 - 1.5, tube)   # bars
        for sx in (X0 + 1, X1 - 5): box(m, sx, sx + 4, 42, 67, Z0 + 1, Z0 + 5, tube); box(m, sx, sx + 4, 42, 55, Z1 - 5, Z1 - 1, tube)
        n = 5                                                                                                            # thin mattress that sags in the middle
        for k in range(n):
            z0 = Z0 + 5 + (d - 10) * k / n; z1 = Z0 + 5 + (d - 10) * (k + 1) / n; sag = 3.0 * math.sin(math.pi * (k + 0.5) / n)
            box(m, X0 + 4, X1 - 4, 38 - sag, 46 - sag, z0, z1, mat)
            for j in range(6): box(m, X0 + 4 + (wd - 8) * (j + 0.45) / 6, X0 + 4 + (wd - 8) * (j + 0.55) / 6, 46 - sag, 46.3 - sag, z0, z1, matd)   # ticking stripes
        box(m, X0 + 6, X1 - 6, 43, 49, Z1 - 5 - d * 0.22, Z1 - 6, blank); box(m, X0 + 6, X1 - 6, 49, 51, Z1 - 5 - d * 0.22, Z1 - 5 - d * 0.12, blankd)   # folded blanket at the foot
        box(m, X0 + wd * 0.18, X1 - wd * 0.18, 45, 50, Z0 + 6, Z0 + 6 + d * 0.16, pil)                                # flat pillow
    else:
        wood, woodd, sheet, duvet, duvetd, pil = c((140, 96, 58)), c((104, 70, 40)), c((240, 240, 236)), c((70, 110, 170)), c((54, 88, 140)), c((250, 250, 248))
        box(m, X0, X1, 6, 26, Z0 + 2, Z1, wood)                                                                         # base
        box(m, X0 + 2, X1 - 2, 0, 6, Z0 + 4, Z1 - 2, woodd)                                                             # plinth
        box(m, X0, X1, 0, 78, Z0, Z0 + 5, woodd); box(m, X0 + 4, X1 - 4, 30, 72, Z0 + 5, Z0 + 6, wood)                 # panelled headboard
        box(m, X0 - 1, X1 + 1, 78, 82, Z0 - 1, Z0 + 6, wood)                                                            # headboard cap
        box(m, X0, X1, 0, 40, Z1 - 4, Z1, woodd)                                                                        # footboard
        box(m, X0 + 2, X1 - 2, 26, 44, Z0 + 5, Z1 - 4, sheet)                                                           # thick mattress
        box(m, X0 + 1, X1 - 1, 36, 47, Z0 + 5 + d * 0.32, Z1 - 4, duvet)                                                # duvet, hanging over the sides
        box(m, X0 + 1, X1 - 1, 47, 48, Z0 + 5 + d * 0.32, Z1 - 4, duvetd)
        box(m, X0 + 1, X1 - 1, 44, 49, Z0 + 5 + d * 0.27, Z0 + 5 + d * 0.33, sheet)                                     # sheet turned down over it
        for k in (0, 1):
            px0 = X0 + 5 + (wd - 10) * k / 2 + 1; px1 = X0 + 5 + (wd - 10) * (k + 1) / 2 - 1
            box(m, px0, px1, 44, 52, Z0 + 7, Z0 + 7 + d * 0.17, pil); box(m, px0 + 2, px1 - 2, 52, 54, Z0 + 9, Z0 + 5 + d * 0.17, pil)   # two plump pillows
    return m
def b6_stool(w, h, img, metal):
    """a round stool: a turned seat on three splayed legs with a foot ring"""
    m = Mesh(); c = swatch
    if metal: seat, leg = c((96, 100, 108)), c((150, 154, 160))
    else: seat, leg = c((168, 118, 70)), c((128, 86, 50))
    lathe(m, [(0, 42), (13, 42), (14, 43.5), (14, 46), (13, 47), (0, 47)], 18, seat)
    for k in range(3):
        a = math.radians(90 + 120 * k); tx, tz = math.cos(a) * 8, math.sin(a) * 8; bx, bz = math.cos(a) * 14, math.sin(a) * 14
        n = 4
        for i in range(n):
            y0 = 42 * i / n; y1 = 42 * (i + 1) / n; f0 = 1 - i / n; f1 = 1 - (i + 1) / n
            x0 = tx + (bx - tx) * f0; z0 = tz + (bz - tz) * f0; x1 = tx + (bx - tx) * f1; z1 = tz + (bz - tz) * f1
            box(m, min(x0, x1) - 1.6, max(x0, x1) + 1.6, y0, y1, min(z0, z1) - 1.6, max(z0, z1) + 1.6, leg)
    lathe(m, [(11.5, 14), (12.5, 14), (12.5, 16), (11.5, 16)], 16, leg)                     # foot ring
    m.v = [(x * 1.3, y * 1.3, z * 1.3) for (x, y, z) in m.v]                              # a bit bigger
    return m
def rot180(m):
    """turn a model half way round (front +z becomes -z); keeps the winding"""
    m.v = [(-x, y, -z) for (x, y, z) in m.v]; m.vn = [(-x, y, -z) for (x, y, z) in m.vn]; return m
def _legs4(m, X0, X1, Z0, Z1, top, th, col, inset=3):
    for sx in (X0 + inset, X1 - inset - th):
        for sz in (Z0 + inset, Z1 - inset - th): box(m, sx, sx + th, 0, top, sz, sz + th, col)
def b6_oak_table(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    oak, oakd, oakl, runner, runnerd = c((150, 102, 60)), c((104, 68, 38)), c((172, 124, 78)), c((150, 30, 34)), c((110, 20, 24))
    top = 72
    box(m, X0, X1, top - 5, top, Z0, Z1, oak); box(m, X0 - 1, X1 + 1, top - 6, top - 4, Z0 - 1, Z1 + 1, oakd)          # top with a moulded edge
    for k in range(1, 6): x = X0 + (X1 - X0) * k / 6; box(m, x - 0.3, x + 0.3, top, top + 0.2, Z0 + 1, Z1 - 1, oakd)   # plank joints
    box(m, X0 + 5, X1 - 5, top - 14, top - 6, Z0 + 5, Z0 + 7, oakd); box(m, X0 + 5, X1 - 5, top - 14, top - 6, Z1 - 7, Z1 - 5, oakd)   # aprons
    box(m, X0 + 5, X0 + 7, top - 14, top - 6, Z0 + 5, Z1 - 5, oakd); box(m, X1 - 7, X1 - 5, top - 14, top - 6, Z0 + 5, Z1 - 5, oakd)
    for sx in (X0 + 3, X1 - 10):                                                                                         # turned legs
        for sz in (Z0 + 3, Z1 - 10):
            lathe(m, [(3, 0), (3.4, 4), (2.4, 10), (3.6, 30), (2.6, 50), (3.6, top - 14), (3.6, top - 6)], 8, oakd, cx=sx + 3.5, cz=sz + 3.5)
    zc = (Z0 + Z1) / 2; rw = (Z1 - Z0) * 0.18
    box(m, X0 - 2, X1 + 2, top, top + 0.8, zc - rw, zc + rw, runner); box(m, X0 - 2, X0 - 1, top - 8, top + 0.8, zc - rw, zc + rw, runner); box(m, X1 + 1, X1 + 2, top - 8, top + 0.8, zc - rw, zc + rw, runner)   # runner hanging over the ends
    box(m, X0 + 2, X1 - 2, top + 0.8, top + 1, zc - rw + 1.5, zc - rw + 2.5, runnerd); box(m, X0 + 2, X1 - 2, top + 0.8, top + 1, zc + rw - 2.5, zc + rw - 1.5, runnerd)
    lathe(m, [(0, top + 0.8), (6, top + 0.8), (7, top + 2), (4, top + 4), (5, top + 10), (0, top + 10)], 12, c((210, 214, 220)), cx=(X0 + X1) / 2, cz=zc)   # a vase in the middle
    return m
def b6_chess_table(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    wood, woodd, lite, dark, wp, bp = c((126, 82, 46)), c((88, 56, 30)), c((232, 222, 196)), c((46, 40, 36)), c((236, 230, 214)), c((36, 32, 30))
    s = min(X1 - X0, Z1 - Z0) * 0.62; cx = (X0 + X1) / 2; cz = Z0 + (Z1 - Z0) * 0.42; top = 66
    box(m, cx - s / 2 - 6, cx + s / 2 + 6, top - 5, top, cz - s / 2 - 6, cz + s / 2 + 6, wood)
    box(m, cx - s / 2 - 7, cx + s / 2 + 7, top - 7, top - 4, cz - s / 2 - 7, cz + s / 2 + 7, woodd)
    _legs4(m, cx - s / 2 - 6, cx + s / 2 + 6, cz - s / 2 - 6, cz + s / 2 + 6, top - 5, 5, woodd, 2)
    q = s / 8
    for i in range(8):                                                                                                   # the board: 64 inlaid squares
        for j in range(8):
            box(m, cx - s / 2 + i * q, cx - s / 2 + (i + 1) * q, top, top + 0.6, cz - s / 2 + j * q, cz - s / 2 + (j + 1) * q, lite if (i + j) % 2 == 0 else dark)
    import random as _r; rr = _r.Random(7)
    for k in range(14):                                                                                                  # a game in progress
        i, j = rr.randrange(8), rr.randrange(8); col = wp if k % 2 == 0 else bp; px_, pz_ = cx - s / 2 + (i + 0.5) * q, cz - s / 2 + (j + 0.5) * q
        hgt = 5 if k % 3 else 8
        lathe(m, [(0, top + 0.6), (q * 0.32, top + 0.6), (q * 0.22, top + 2), (q * 0.14, top + hgt - 1.5), (q * 0.2, top + hgt), (0, top + hgt)], 7, col, cx=px_, cz=pz_)
    sz_ = Z1 - (Z1 - Z0) * 0.16                                                                                          # the stool on the near side
    lathe(m, [(0, 40), (10, 40), (10.5, 44), (0, 44)], 14, wood, cx=cx, cz=sz_); lathe(m, [(2.5, 0), (2.5, 40)], 8, woodd, cx=cx, cz=sz_)
    lathe(m, [(0, 0), (8, 0), (8, 2), (0, 2)], 12, woodd, cx=cx, cz=sz_)
    return m
def b6_sorting_desk(w, h, img):
    """a mail room desk, untidy: a few trays with uneven piles, loose letters, bundles, parcels of different sizes on and under it, a sack on the floor"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    import random as _r; rr = _r.Random(23)
    wood, woodd, tray = c((186, 140, 86)), c((132, 96, 56)), c((80, 84, 92))
    papers = [c((240, 238, 230)), c((226, 210, 170)), c((214, 222, 236)), c((236, 220, 200))]
    cards = [c((176, 140, 96)), c((150, 116, 76)), c((196, 162, 112)), c((128, 98, 64))]
    top = 70; wd = X1 - X0; dp = Z1 - Z0
    box(m, X0, X1, top - 4, top, Z0, Z1, wood); box(m, X0 + 2, X1 - 2, 0, top - 4, Z0 + 2, Z0 + 5, woodd)
    box(m, X0 + 2, X0 + 6, 0, top - 4, Z0 + 2, Z1 - 2, woodd); box(m, X1 - 6, X1 - 2, 0, top - 4, Z0 + 2, Z1 - 2, woodd)
    box(m, X0 + 6, X1 - 6, 10, 13, Z0 + 5, Z1 - 4, woodd)                                                           # lower shelf
    x = X0 + 8
    while x < X1 - 30:                                                                                              # parcels under the desk, all sizes
        pw = rr.uniform(14, 30); ph = rr.uniform(8, 22); pd = rr.uniform(dp * 0.35, dp * 0.7); z0 = Z0 + 6 + rr.uniform(0, dp - pd - 12)
        box(m, x, x + pw, 13, 13 + ph, z0, z0 + pd, cards[rr.randrange(4)]); box(m, x + pw * 0.45, x + pw * 0.55, 13 + ph, 13 + ph + 0.3, z0, z0 + pd, c((200, 190, 160)))   # tape
        x += pw + rr.uniform(2, 10)
    n = max(2, int(wd // 60)); slots = sorted(rr.sample(range(max(n + 1, int(wd // 34))), n))
    for k in slots:                                                                                                  # a few trays, each with its own pile
        x0 = X0 + 6 + k * 34; x1 = x0 + 28
        if x1 > X1 - 6: continue
        z0 = Z0 + 5 + rr.uniform(0, 6)
        box(m, x0, x1, top, top + 1, z0, z0 + 20, tray); box(m, x0, x0 + 1, top, top + 5, z0, z0 + 20, tray); box(m, x1 - 1, x1, top, top + 5, z0, z0 + 20, tray); box(m, x0, x1, top, top + 5, z0, z0 + 1, tray)
        for j in range(rr.randrange(1, 7)):
            ox = rr.uniform(-1.5, 1.5); oz = rr.uniform(-1.5, 1.5)
            box(m, x0 + 2 + ox, x1 - 2 + ox, top + 1 + j * 1.1, top + 2 + j * 1.1, z0 + 3 + oz, z0 + 18 + oz, papers[rr.randrange(4)])
    for k in range(rr.randrange(5, 9)):                                                                              # loose letters scattered on the desk
        lx = rr.uniform(X0 + 8, X1 - 24); lz = rr.uniform(Z0 + dp * 0.45, Z1 - 16)
        box(m, lx, lx + rr.uniform(10, 16), top, top + 0.5 + k * 0.05, lz, lz + rr.uniform(6, 10), papers[rr.randrange(4)])
    for k in range(2):                                                                                               # bundles tied with string
        bx = X0 + wd * (0.25 + 0.4 * k); bz = Z1 - 18
        box(m, bx, bx + 16, top, top + 6, bz, bz + 10, papers[1]); box(m, bx + 7, bx + 9, top, top + 6.3, bz - 0.2, bz + 10.2, c((150, 40, 40)))
    box(m, X1 - 40, X1 - 22, top, top + 5, Z1 - 24, Z1 - 12, c((170, 40, 40))); box(m, X1 - 18, X1 - 4, top, top + 9, Z1 - 26, Z1 - 12, c((40, 120, 70)))   # the parcels from the sprite
    box(m, X0 + 8, X0 + 26, top, top + 2, Z1 - 22, Z1 - 8, c((160, 164, 170))); box(m, X0 + 10, X0 + 24, top + 2, top + 3, Z1 - 20, Z1 - 10, c((60, 62, 66)))   # letter scales
    lathe(m, [(0, 0), (12, 0), (14, 10), (12, 26), (6, 30), (3, 32), (0, 32)], 10, c((150, 140, 110)), cx=X1 + 4, cz=Z1 - 10)   # mail sack on the floor
    return m
def b6_arcade(w, h, img):
    """a slim upright: stepped side profile, screen leaning back, marquee overhanging, control deck in front"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    body, side, trim, marq, scr, scr2, panel, stick, btn, coin = c((40, 60, 140)), c((30, 46, 110)), c((240, 200, 40)), c((250, 230, 120)), c((10, 14, 24)), c((60, 200, 220)), c((22, 22, 28)), c((200, 30, 30)), c((240, 220, 60)), c((200, 160, 40))
    cx = (X0 + X1) / 2; hw = min(24.0, (X1 - X0) / 2 - 2); cz = (Z0 + Z1) / 2; D = min(46.0, Z1 - Z0 - 4)
    zb = cz - D / 2; zf = cz + D / 2; zt = zb + D * 0.55                                       # zt: front of the upper cabinet
    box(m, cx - hw, cx + hw, 4, 70, zb, zf - 2, body)                                           # lower cabinet
    box(m, cx - hw, cx + hw, 0, 4, zb + 2, zf - 4, panel)                                       # kick plate
    box(m, cx - hw, cx + hw, 70, 80, zb, zf, panel)                                             # control deck
    box(m, cx - hw, cx + hw, 80, 150, zb, zt - 6, body)                                         # upper cabinet behind the screen
    n = 6                                                                                       # the screen bezel leans back: stepped slabs
    for k in range(n):
        y0 = 82 + k * 11; z1 = zt - k * 1.2
        box(m, cx - hw, cx + hw, y0, y0 + 11, zt - 7, z1, body)
        box(m, cx - hw + 4, cx + hw - 4, y0 + (1 if k == 0 else 0), y0 + 11, z1, z1 + 0.5, scr)
        if 0 < k < n - 1: box(m, cx - hw + 7, cx + hw - 7, y0, y0 + 11, z1 + 0.5, z1 + 0.8, scr2)
    for k in range(5): box(m, cx - 12 + k * 5, cx - 9 + k * 5, 96 + (k * 9) % 30, 99 + (k * 9) % 30, zt - 2, zt + 0.5, trim)   # sprites on the screen
    box(m, cx - hw, cx + hw, 148, 166, zb, zt + 4, side); box(m, cx - hw + 2, cx + hw - 2, 150, 164, zt + 4, zt + 4.6, marq)      # marquee, overhanging
    for sx in (cx - hw - 1.2, cx + hw):                                                         # side panels with a stepped profile and art stripe
        box(m, sx, sx + 1.2, 0, 72, zb, zf, side); box(m, sx, sx + 1.2, 72, 82, zb, zf + 1, side); box(m, sx, sx + 1.2, 82, 166, zb, zt + 4, side)
        box(m, sx - 0.2, sx + 1.4, 96, 140, zb + 4, zb + 12, trim)
    lathe(m, [(0, 80), (1.2, 80), (1.2, 88), (0, 88)], 6, panel, cx=cx - hw * 0.45, cz=zf - 9); lathe(m, [(0, 88), (2.6, 88), (2.6, 92), (0, 92)], 8, stick, cx=cx - hw * 0.45, cz=zf - 9)
    for k in range(3): lathe(m, [(0, 80), (2.2, 80), (2.2, 82), (0, 82)], 8, btn if k != 1 else stick, cx=cx + hw * (0.05 + 0.25 * k), cz=zf - 9)
    box(m, cx - 7, cx + 7, 36, 52, zf - 2, zf - 1, panel); box(m, cx - 5, cx - 1, 42, 48, zf - 1, zf - 0.6, coin); box(m, cx + 1, cx + 5, 42, 48, zf - 1, zf - 0.6, coin)   # coin door
    return m
def b6_swivel_chair(w, h, img, col=(52, 72, 62)):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    up, upd, frame_, metal = c(col), c(tuple(int(v * 0.75) for v in col)), c((40, 40, 44)), c((150, 152, 156))
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; r = min(X1 - X0, Z1 - Z0) / 2
    for k in range(5):
        a = math.radians(90 + 72 * k); ex, ez = cx + math.cos(a) * r * 0.85, cz + math.sin(a) * r * 0.85
        box(m, min(cx, ex) - 1.8, max(cx, ex) + 1.8, 4, 7, min(cz, ez) - 1.8, max(cz, ez) + 1.8, frame_)
        lathe(m, [(0, 0), (2.6, 0), (2.6, 4), (0, 4)], 8, frame_, cx=ex, cz=ez)
    lathe(m, [(3, 7), (3, 38)], 10, metal, cx=cx, cz=cz)
    box(m, cx - r * 0.8, cx + r * 0.8, 38, 44, cz - r * 0.75, cz + r * 0.8, upd); box(m, cx - r * 0.76, cx + r * 0.76, 44, 47, cz - r * 0.7, cz + r * 0.76, up)    # padded seat
    box(m, cx - r * 0.7, cx + r * 0.7, 52, 98, cz - r * 0.9, cz - r * 0.72, up); box(m, cx - r * 0.74, cx + r * 0.74, 50, 100, cz - r * 0.98, cz - r * 0.9, upd)   # back (at -z: it faces +z)
    box(m, cx - 3, cx + 3, 42, 54, cz - r * 0.95, cz - r * 0.85, frame_)
    for sx in (-1, 1):
        box(m, cx + sx * r * 0.82 - 2, cx + sx * r * 0.82 + 2, 44, 62, cz - r * 0.2, cz - r * 0.12, frame_)
        box(m, cx + sx * r * 0.82 - 3, cx + sx * r * 0.82 + 3, 62, 65, cz - r * 0.5, cz + r * 0.35, frame_)           # arm rests
    return m
def b6_fancy_desk(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    wood, woodd, leather, brass, paper, ink = c((92, 60, 34)), c((62, 40, 22)), c((40, 84, 52)), c((200, 168, 80)), c((242, 240, 232)), c((30, 30, 40))
    top = 74; wd = X1 - X0; ped = min(30.0, wd * 0.24)
    box(m, X0, X1, top - 5, top, Z0, Z1, wood); box(m, X0 - 1, X1 + 1, top - 7, top - 5, Z0 - 1, Z1 + 1, woodd)
    box(m, X0 + wd * 0.18, X1 - wd * 0.18, top, top + 0.4, Z0 + 6, Z1 - 6, leather)                                   # leather writing inset
    for sx0 in (X0 + 1, X1 - 1 - ped):                                                                                    # two drawer pedestals, drawers facing +z
        box(m, sx0, sx0 + ped, 0, top - 7, Z0 + 2, Z1 - 2, woodd)
        for k in range(3):
            y0 = 4 + k * 21; box(m, sx0 + 2, sx0 + ped - 2, y0, y0 + 18, Z1 - 2, Z1 - 1, wood)
            box(m, sx0 + ped / 2 - 4, sx0 + ped / 2 + 4, y0 + 8, y0 + 10, Z1 - 1, Z1 + 0.6, brass)
    box(m, X0 + ped + 1, X1 - ped - 1, top - 16, top - 7, Z1 - 3, Z1 - 1, wood); box(m, (X0 + X1) / 2 - 5, (X0 + X1) / 2 + 5, top - 12, top - 10, Z1 - 1, Z1 + 0.6, brass)   # centre drawer
    box(m, X0 + ped + 1, X1 - ped - 1, top - 40, top - 7, Z0 + 2, Z0 + 4, woodd)                                     # modesty panel
    cx = (X0 + X1) / 2
    box(m, cx - 6, cx + 10, top + 0.4, top + 0.7, (Z0 + Z1) / 2 - 10, (Z0 + Z1) / 2 + 10, paper); box(m, cx - 2, cx + 14, top + 0.7, top + 1, (Z0 + Z1) / 2 - 8, (Z0 + Z1) / 2 + 12, paper)   # papers
    lathe(m, [(0, top), (4, top), (4, top + 3), (2, top + 4), (0, top + 4)], 10, ink, cx=X1 - wd * 0.12, cz=Z0 + 10)      # inkwell
    box(m, X1 - wd * 0.12 - 0.5, X1 - wd * 0.12 + 0.5, top + 4, top + 14, Z0 + 10, Z0 + 11, c((240, 240, 240)))       # quill
    lathe(m, [(0, top), (6, top), (6, top + 2), (1.2, top + 3), (1.2, top + 26), (0, top + 26)], 10, brass, cx=X0 + wd * 0.1, cz=Z0 + 10)   # banker's lamp
    box(m, X0 + wd * 0.1 - 10, X0 + wd * 0.1 + 10, top + 24, top + 30, Z0 + 4, Z0 + 16, leather)
    return m
def b6_chest_of_drawers(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    wood, woodd, woodl, knob = c((190, 140, 74)), c((140, 100, 52)), c((206, 158, 92)), c((220, 220, 214))
    hgt = 84
    box(m, X0 + 1, X1 - 1, 4, hgt, Z0 + 1, Z1 - 3, wood); box(m, X0, X1, hgt, hgt + 4, Z0, Z1 - 1, woodl)               # carcass and top
    box(m, X0 + 2, X1 - 2, 0, 4, Z0 + 2, Z1 - 4, woodd)                                                                 # plinth
    rows = 4; cols = 2 if X1 - X0 > 70 else 1
    for r in range(rows):
        y0 = 7 + r * (hgt - 10) / rows; y1 = y0 + (hgt - 10) / rows - 2
        for k in range(cols):
            x0 = X0 + 4 + k * (X1 - X0 - 8) / cols; x1 = x0 + (X1 - X0 - 8) / cols - 2
            box(m, x0, x1, y0, y1, Z1 - 3, Z1 - 1.5, woodl)
            for kx in ((x0 + x1) / 2,) if x1 - x0 < 40 else (x0 + (x1 - x0) * 0.25, x0 + (x1 - x0) * 0.75):
                box(m, kx - 1.6, kx + 1.6, (y0 + y1) / 2 - 1.6, (y0 + y1) / 2 + 1.6, Z1 - 1.5, Z1 + 0.5, knob)
    return m
def b6_treadmill(w, h, img):
    """long along z, console and uprights at the -z end (the runner faces -z)"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    frame_, belt, beltl, side, metal, cons, scr, red = c((40, 44, 50)), c((30, 30, 34)), c((52, 52, 58)), c((60, 140, 170)), c((170, 174, 180)), c((50, 54, 62)), c((90, 200, 220)), c((210, 40, 40))
    cx = (X0 + X1) / 2; hw = (X1 - X0) / 2 - 2
    box(m, cx - hw, cx + hw, 2, 10, Z0 + 10, Z1 - 2, frame_)                                                  # deck
    box(m, cx - hw + 7, cx + hw - 7, 10, 12, Z0 + 14, Z1 - 6, belt)                                         # running belt
    for k in range(6): z = Z0 + 18 + k * (Z1 - Z0 - 28) / 6; box(m, cx - hw + 7, cx + hw - 7, 12, 12.2, z, z + 1.2, beltl)
    for sx in (-1, 1):
        box(m, cx + sx * hw - (7 if sx > 0 else 0), cx + sx * hw + (0 if sx > 0 else 7), 10, 14, Z0 + 10, Z1 - 2, side)   # side rails
        box(m, cx + sx * (hw - 3) - 2.5, cx + sx * (hw - 3) + 2.5, 10, 110, Z0 + 10, Z0 + 15, metal)           # uprights
        box(m, cx + sx * (hw - 3) - 2, cx + sx * (hw - 3) + 2, 92, 96, Z0 + 12, Z0 + 52, metal)                # hand rails
    lathe(m, [(0, 0), (4, 0), (4, 2), (0, 2)], 8, frame_, cx=cx - hw + 4, cz=Z1 - 6); lathe(m, [(0, 0), (4, 0), (4, 2), (0, 2)], 8, frame_, cx=cx + hw - 4, cz=Z1 - 6)   # feet
    box(m, cx - hw + 2, cx + hw - 2, 104, 120, Z0 + 6, Z0 + 18, cons)                                     # console, angled towards the runner
    box(m, cx - hw + 8, cx + hw - 8, 108, 117, Z0 + 18, Z0 + 18.6, scr); box(m, cx + hw - 8, cx + hw - 4, 106, 110, Z0 + 18, Z0 + 18.8, red)
    box(m, cx - hw + 2, cx + hw - 2, 0, 10, Z0 + 4, Z0 + 12, frame_)                                        # motor cover
    return m
def b6_water_cooler(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    body, bodyd, tap_r, tap_b, water, cap, cup = c((226, 228, 230)), c((150, 154, 160)), c((210, 50, 50)), c((50, 110, 210)), c((120, 180, 235)), c((40, 80, 170)), c((246, 246, 242))
    cx = (X0 + X1) / 2; cz = (Z0 + Z1) / 2; hw = min(16.0, (X1 - X0) / 2 - 1); hd = min(16.0, (Z1 - Z0) / 2 - 1)
    box(m, cx - hw, cx + hw, 0, 94, cz - hd, cz + hd, body); box(m, cx - hw, cx + hw, 0, 4, cz - hd, cz + hd, bodyd)
    box(m, cx - hw + 3, cx + hw - 3, 50, 70, cz + hd, cz + hd + 0.6, bodyd)                                  # recess for the taps
    for sx, col in ((-5, tap_b), (5, tap_r)): box(m, cx + sx - 2, cx + sx + 2, 62, 66, cz + hd, cz + hd + 5, col)
    box(m, cx - hw + 4, cx + hw - 4, 46, 48, cz + hd - 2, cz + hd + 7, bodyd)                               # drip tray
    lathe(m, [(0, 94), (13, 94), (14, 98), (14, 122), (12, 128), (5, 132), (5, 136), (0, 136)], 18, water, cx=cx, cz=cz)   # the water bottle
    lathe(m, [(0, 136), (5.4, 136), (5.4, 139), (0, 139)], 10, cap, cx=cx, cz=cz)
    lathe(m, [(3, 40), (3.4, 40), (3.4, 80), (3, 80)], 8, cup, cx=cx + hw + 3.5, cz=cz + hd - 6)            # cup tube on the side
    return m
def b6_snack_machine(w, h, img):
    """about the size of the game's drink machine; the window is a real recess so the snacks show"""
    X0, X1, Z0, Z1 = frame_of(w, h, img, 0.78); m = Mesh(); c = swatch
    body, bodyd, glass, shelf, panel, slot, red, back = c((206, 208, 202)), c((60, 62, 66)), c((190, 220, 235)), c((110, 112, 118)), c((40, 42, 48)), c((24, 24, 28)), c((200, 40, 40)), c((34, 36, 42))
    snacks = [c((220, 60, 50)), c((240, 200, 50)), c((60, 140, 220)), c((90, 180, 90)), c((200, 90, 200)), c((240, 140, 40))]
    zb, zf = Z0 + 2, Z1 - 2; H = 128; rz = zf - 14                                  # rz: depth of the window recess
    gx0, gx1 = X0 + 5, X1 - 5 - (X1 - X0) * 0.26; gy0, gy1 = 30, H - 14
    box(m, X0 + 1, X1 - 1, 4, H, zb, rz, body)                                      # main body behind the recess
    box(m, X0 + 1, X1 - 1, 0, 4, zb, zf - 3, bodyd); box(m, X0, X1, H, H + 3, zb - 1, zf, body)
    box(m, X0 + 1, gx0, 4, H, rz, zf, body); box(m, gx1, X1 - 1, 4, H, rz, zf, body)                   # front columns either side of the window
    box(m, gx0, gx1, 4, gy0, rz, zf, body); box(m, gx0, gx1, gy1, H, rz, zf, body)                       # below and above it
    box(m, gx0, gx1, gy0, gy1, rz - 0.5, rz, back)                                  # dark back of the recess
    rows = 4
    for r in range(rows):
        y = gy0 + 4 + r * (gy1 - gy0 - 6) / rows; box(m, gx0, gx1, y, y + 1.2, rz, zf - 2, shelf)
        n = 4
        for k in range(n):
            x0 = gx0 + 1.5 + k * (gx1 - gx0 - 3) / n; box(m, x0, x0 + (gx1 - gx0 - 3) / n - 2.5, y + 1.2, y + 11, rz + 2, zf - 4, snacks[(r * 2 + k) % len(snacks)])
    for (a0, a1, b0, b1) in ((gx0 - 1.5, gx0, gy0, gy1), (gx1, gx1 + 1.5, gy0, gy1), (gx0, gx1, gy0 - 1.5, gy0), (gx0, gx1, gy1, gy1 + 1.5)):
        box(m, a0, a1, b0, b1, zf - 0.5, zf + 0.8, bodyd)                           # window frame
    box(m, gx0 + 2, gx0 + 4, gy0 + 4, gy1 - 4, zf - 0.6, zf - 0.3, glass)          # a glint on the glass
    px0 = gx1 + 3; box(m, px0, X1 - 3, 64, 104, zf - 1, zf + 1, panel)              # keypad panel
    for r in range(4):
        for k in range(3): box(m, px0 + 2 + k * 4, px0 + 4.8 + k * 4, 70 + r * 5, 73 + r * 5, zf + 1, zf + 1.5, body)
    box(m, px0 + 2, X1 - 5, 94, 100, zf + 1, zf + 1.5, c((60, 200, 120)))
    box(m, px0 + 3, px0 + 8, 50, 58, zf - 1, zf + 1.2, slot)
    box(m, gx0, gx1, 10, 22, zf - 1, zf + 0.5, slot)                                # collection flap
    box(m, X0 + 1, X1 - 1, H - 7, H, zf - 1, zf + 0.6, red)
    return m
def b6_recycling_bin(w, h, img):
    m = Mesh(); c = swatch
    body, rim, lid, label = c((46, 120, 70)), c((36, 96, 56)), c((40, 44, 48)), c((240, 240, 236))
    lathe(m, [(0, 0), (17, 0), (19, 50), (20, 54), (20, 56), (0, 56)], 16, body)
    lathe(m, [(20.2, 50), (21, 50), (21, 56), (20.2, 56)], 16, rim)
    lathe(m, [(0, 56), (21, 56), (21, 59), (16, 62), (0, 62)], 16, lid)
    box(m, -9, 9, 62, 63, -2, 2, c((20, 20, 22)))                                                             # slot in the lid
    for k in range(3):                                                                                        # recycling arrows on the front
        a = math.radians(90 + 120 * k); x, y = math.cos(a) * 6, 30 + math.sin(a) * 6
        box(m, x - 3, x + 3, y - 1.2, y + 1.2, 19.4, 20.4, label)
    return m
def b6_dumbbell_rack(w, h, img):
    """a two-tier steel rack along the long side, with pairs of dumbbells getting heavier along it"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    steel, steeld, iron, grip, label = c((70, 74, 82)), c((44, 46, 52)), c((40, 40, 44)), c((150, 154, 160)), c((200, 40, 40))
    along_x = (X1 - X0) >= (Z1 - Z0)
    L0, L1 = (X0, X1) if along_x else (Z0, Z1); D0, D1 = (Z0, Z1) if along_x else (X0, X1)
    dm = (D0 + D1) / 2; dd = min(26.0, (D1 - D0) / 2)
    def B(a0, a1, y0, y1, b0, b1, col):          # a = along the rack, b = across it
        if along_x: box(m, a0, a1, y0, y1, b0, b1, col)
        else: box(m, b0, b1, y0, y1, a0, a1, col)
    nlegs = max(2, int(round((L1 - L0) / 80)) + 1)
    for k in range(nlegs):                                                          # A-frame end and middle supports
        a = L0 + 3 + (L1 - L0 - 8) * k / (nlegs - 1)
        B(a, a + 3, 0, 4, dm - dd, dm + dd, steeld)
        B(a, a + 3, 4, 56, dm - dd + 2, dm - dd + 5, steel); B(a, a + 3, 4, 40, dm + dd - 5, dm + dd - 2, steel)
    chrome, rubber, ring = c((206, 210, 216)), c((30, 30, 34)), c((180, 40, 36))
    for (y, b0, b1) in ((54, dm - dd + 2, dm), (38, dm, dm + dd - 2)):          # two tiers: a tray with a raised lip on both edges (cradles the bells)
        B(L0 + 2, L1 - 2, y - 2.5, y, b0, b1, steel); B(L0 + 2, L1 - 2, y, y + 2.5, b0, b0 + 1.4, steeld); B(L0 + 2, L1 - 2, y, y + 2.5, b1 - 1.4, b1, steeld)
    for a in (L0 + 2, L1 - 5): B(a, a + 3, 4, 8, dm - dd + 2, dm + dd - 2, steeld)      # end rails
    n = max(4, int((L1 - L0 - 10) // 20))
    def bell(a, y, bc, r, half):      # a round dumbbell lying across the rack: chrome handle, two rubber heads with a coloured weight band
        def cyl(b0, b1, rad, col):
            seg = 10
            for i in range(seg):
                t0, t1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
                a0, y0, a1, y1 = a + rad * math.cos(t0), y + r + rad * math.sin(t0), a + rad * math.cos(t1), y + r + rad * math.sin(t1)
                nm_ = (math.cos((t0 + t1) / 2), math.sin((t0 + t1) / 2))
                P = [(a0, y0, b0), (a1, y1, b0), (a1, y1, b1), (a0, y0, b1)] if along_x else [(b0, y0, a0), (b0, y1, a1), (b1, y1, a1), (b1, y0, a0)]
                N = (nm_[0], nm_[1], 0) if along_x else (0, nm_[1], nm_[0])
                _quad_out(m, P, N, col)
            for bb, sgn in ((b0, -1), (b1, 1)):      # end caps as a fan of quads
                for i in range(seg):
                    t0, t1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
                    pa = (a + rad * math.cos(t0), y + r + rad * math.sin(t0)); pb = (a + rad * math.cos(t1), y + r + rad * math.sin(t1))
                    P = [(a, y + r, bb), (pa[0], pa[1], bb), (pb[0], pb[1], bb), (a, y + r, bb)] if along_x else [(bb, y + r, a), (bb, pa[1], pa[0]), (bb, pb[1], pb[0]), (bb, y + r, a)]
                    _quad_out(m, P, (0, 0, sgn) if along_x else (sgn, 0, 0), col)
        cyl(bc - half, bc + half, 1.1, chrome)
        for e in (-1, 1):
            h0, h1 = sorted((bc + e * half, bc + e * (half - 4.2)))
            cyl(h0, h1, r, rubber); m0 = (h0 + h1) / 2; cyl(m0 - 0.6, m0 + 0.6, r + 0.25, ring)
    for tier, (y, bc) in enumerate(((54, dm - dd / 2 + 1), (38, dm + dd / 2 - 1))):
        for k in range(n):
            a = L0 + 8 + (L1 - L0 - 16) * (k + 0.5) / n; r = 2.8 + 2.4 * (k / max(1, n - 1)) + tier * 0.6      # heavier along the rack
            bell(a, y, bc, r, min(9.5, dd / 2 - 1))
    return m
def _quad_out(m, P, N, col):
    """a quad (or a triangle given as a quad with a repeated corner) whose winding faces N"""
    ux, uy, uz = P[1][0] - P[0][0], P[1][1] - P[0][1], P[1][2] - P[0][2]; vx, vy, vz = P[2][0] - P[0][0], P[2][1] - P[0][1], P[2][2] - P[0][2]
    cx_, cy_, cz_ = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    if cx_ * N[0] + cy_ * N[1] + cz_ * N[2] < 0: P = P[::-1]
    if P[3] == P[0] or P[0] == P[1]:      # a triangle: one face only (no zero-area half)
        q = [P[0], P[1], P[2]] if P[3] == P[0] else [P[1], P[2], P[3]]
        bv = len(m.v); bt = len(m.vt); m.v += q; m.vt += [col] * 3; m.vn.append(N); ni = len(m.vn)
        m.f.append(((bv + 1, bt + 1, ni), (bv + 2, bt + 2, ni), (bv + 3, bt + 3, ni))); return
    m.quad(P, N, [col] * 4)
def b6_dog_bowl(w, h, img):
    """a low teal stand holding a food bowl and a water bowl"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    stand, standd, steel, food, water = c((40, 150, 150)), c((28, 110, 110)), c((196, 200, 206)), c((128, 74, 36)), c((150, 196, 230))
    box(m, X0 + 2, X1 - 2, 0, 8, Z0 + 2, Z1 - 2, stand); box(m, X0 + 2, X1 - 2, 8, 9, Z0 + 2, Z1 - 2, standd)
    along_x = (X1 - X0) >= (Z1 - Z0); cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; L = max(X1 - X0, Z1 - Z0); r = min(min(X1 - X0, Z1 - Z0) * 0.36, L * 0.2)
    for k, fill in ((0, food), (1, water)):
        off = (k - 0.5) * L * 0.46; bx, bz = (cx + off, cz) if along_x else (cx, cz + off)
        lathe(m, [(r * 0.7, 6), (r, 9), (r, 12), (r * 0.86, 12), (r * 0.62, 7)], 18, steel, cx=bx, cz=bz)        # steel bowl
        if fill is food:
            lathe(m, [(0, 9), (r * 0.8, 9), (r * 0.7, 11), (0, 12.5)], 14, fill, cx=bx, cz=bz)                  # a heap of food
            for j in range(5): a = j * 1.3; box(m, bx + math.cos(a) * r * 0.4 - 1, bx + math.cos(a) * r * 0.4 + 1, 11, 13, bz + math.sin(a) * r * 0.4 - 1, bz + math.sin(a) * r * 0.4 + 1, c((150, 90, 44)))
        else:
            lathe(m, [(0, 10.5), (r * 0.8, 10.5)], 14, fill, cx=bx, cz=bz)                                    # water surface
    return m
def b6_bare_tree(w, h, img):
    """a leafless tree: a tapering trunk splitting into branches and twigs"""
    m = Mesh(); c = swatch; bark, barkd = c((96, 76, 58)), c((70, 54, 40))
    import random as _r; rr = _r.Random(11)
    lathe(m, [(9, 0), (7, 8), (6, 40), (5, 70)], 10, bark)                               # trunk with a flared foot
    def branch(x, y, z, dx, dy, dz, L, r, depth):
        n = 4
        for i in range(n):
            x0, y0, z0 = x + dx * L * i / n, y + dy * L * i / n, z + dz * L * i / n; x1, y1, z1 = x + dx * L * (i + 1) / n, y + dy * L * (i + 1) / n, z + dz * L * (i + 1) / n
            rr_ = r * (1 - 0.5 * i / n)
            box(m, min(x0, x1) - rr_, max(x0, x1) + rr_, min(y0, y1) - rr_ * 0.5, max(y0, y1) + rr_ * 0.5, min(z0, z1) - rr_, max(z0, z1) + rr_, bark if depth == 0 else barkd)
        if depth < 2:
            ex, ey, ez = x + dx * L, y + dy * L, z + dz * L
            for k in range(2 if depth else 3):
                a = rr.uniform(0, 2 * math.pi); s = rr.uniform(0.5, 0.9)
                ndx, ndy, ndz = dx * 0.5 + math.cos(a) * s, 0.7 + rr.uniform(0, 0.4), dz * 0.5 + math.sin(a) * s
                nl = math.sqrt(ndx * ndx + ndy * ndy + ndz * ndz)
                branch(ex, ey, ez, ndx / nl, ndy / nl, ndz / nl, L * 0.62, r * 0.55, depth + 1)
    for k in range(4):
        a = k * math.pi / 2 + 0.4; branch(0, 62, 0, math.cos(a) * 0.6, 0.8, math.sin(a) * 0.6, 46, 3.6, 0)
    lathe(m, [(5, 70), (3.5, 110)], 8, bark)                                             # leader
    return m
def b6_floor_lamp(w, h, img):
    """a standing lamp: weighted base, thin pole, pleated fabric shade with a warm glow inside"""
    m = Mesh(); c = swatch; base, pole, shade, shaded, glow = c((60, 50, 40)), c((176, 150, 100)), c((236, 220, 180)), c((210, 190, 150)), c((255, 236, 170))
    lathe(m, [(0, 0), (14, 0), (14, 2), (10, 5), (2, 6), (0, 6)], 18, base)
    lathe(m, [(1.6, 6), (1.6, 130)], 8, pole)
    lathe(m, [(20, 120), (21, 121), (12, 150), (11, 151)], 18, shade)                   # shade
    lathe(m, [(11, 151), (11, 152), (19.5, 121.5), (20, 120)], 18, shaded)              # inside of the shade
    lathe(m, [(0, 132), (5, 134), (4, 140), (0, 141)], 10, glow)                        # bulb
    return m
def b6_flood_light(w, h, img):
    """a floodlight mast: base plate, tall steel pole, an arm reaching out and a lamp head tilted down towards the front"""
    m = Mesh(); c = swatch; steel, steeld, head, lens = c((110, 116, 128)), c((70, 74, 84)), c((84, 88, 100)), c((255, 246, 210))
    box(m, -12, 12, 0, 3, -12, 12, steeld)
    for sx in (-8, 8):
        for sz in (-8, 8): box(m, sx - 1.5, sx + 1.5, 3, 5, sz - 1.5, sz + 1.5, steel)            # bolts
    lathe(m, [(6, 3), (5, 12), (3.4, 20), (3, 230)], 10, steel)                                    # mast
    for i in range(4):                                                                             # the arm curving out to the front
        a0, a1 = i / 4 * math.pi / 2, (i + 1) / 4 * math.pi / 2
        y0, z0 = 230 + math.sin(a0) * 18, 18 - math.cos(a0) * 18; y1, z1 = 230 + math.sin(a1) * 18, 18 - math.cos(a1) * 18
        box(m, -2.4, 2.4, min(y0, y1) - 2.4, max(y0, y1) + 2.4, min(z0, z1) - 2.4, max(z0, z1) + 2.4, steel)
    box(m, -2.4, 2.4, 246, 251, 18, 34, steel)
    box(m, -16, 16, 232, 252, 30, 48, head)                                                        # lamp head
    for k in range(5): box(m, -15 + k * 7, -13 + k * 7, 252, 256, 32, 46, steeld)                  # cooling fins on top
    box(m, -14, 14, 231, 232, 32, 46, lens)                                                        # lens facing down and forward
    box(m, -14, 14, 234, 250, 48, 49, lens)
    return m
def b6_ox_statue(w, h, img):
    """a lucky ox on a red lacquered plinth with gold trim"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    red, redd, gold, ox, oxd, horn = c((176, 30, 30)), c((120, 18, 20)), c((230, 186, 70)), c((196, 50, 44)), c((150, 34, 30)), c((240, 214, 120))
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; hw, hd = min(40.0, (X1 - X0) / 2 - 2), min(26.0, (Z1 - Z0) / 2 - 2)
    box(m, cx - hw, cx + hw, 0, 6, cz - hd, cz + hd, redd); box(m, cx - hw + 3, cx + hw - 3, 6, 30, cz - hd + 3, cz + hd - 3, red)   # plinth
    box(m, cx - hw + 1, cx + hw - 1, 30, 34, cz - hd + 1, cz + hd - 1, gold); box(m, cx - hw + 2, cx + hw - 2, 4, 6, cz - hd + 1, cz + hd - 1, gold)
    box(m, cx - 12, cx + 12, 12, 24, cz + hd - 3, cz + hd - 2, gold)                                                  # plaque
    _ball(m, cx, 52, cz, 26, 16, 14, (196, 50, 44), 12, 7)                                                                        # body
    for lx in (cx - 16, cx + 14):
        for lz in (cz - 8, cz + 6): box(m, lx, lx + 5, 34, 46, lz, lz + 5, oxd)                                        # legs
    _ball(m, cx + 28, 60, cz, 10, 9, 9, (196, 50, 44), 10, 6)                                                                     # head
    _ball(m, cx + 36, 56, cz, 5, 5, 6, (230, 186, 70), 8, 5)                                                                     # gold muzzle
    for s in (-1, 1):
        box(m, cx + 27, cx + 30, 66, 70, cz + s * 6, cz + s * 13, horn); box(m, cx + 27, cx + 30, 70, 77, cz + s * 12, cz + s * 15 if s > 0 else cz + s * 12 + 0.01, horn)   # horns
        _ball(m, cx + 27, 72, cz + s * 13.5, 1.8, 4, 1.8, (240, 214, 120), 6, 4)
    box(m, cx - 30, cx - 26, 52, 54, cz - 1, cz + 1, oxd); box(m, cx - 33, cx - 30, 40, 54, cz - 1, cz + 1, oxd)    # tail
    _ball(m, cx + 10, 66, cz, 6, 4, 6, (230, 186, 70), 8, 5)                                                                     # gold coin saddle
    return m
def b6_snowy_conifer(w, h, img, scale=1.0):
    """a tall slim fir: a visible trunk, six drooping tiers that step in towards the top (low-poly, seven sides), patchy snow on the upper faces"""
    m = Mesh(); c = swatch; trunk, green, greend, snow = c((84, 60, 40)), c((40, 74, 46)), c((28, 54, 34)), c((236, 240, 246))
    import random as _r; rr = _r.Random(int(scale * 100))
    s = scale
    lathe(m, [(5 * s, 0), (4.4 * s, 34 * s)], 7, trunk)
    tiers = [(34, 26, 62), (30, 52, 86), (25, 76, 108), (20, 98, 128), (15, 118, 148), (10, 138, 168)]
    for i, (r, y0, y1) in enumerate(tiers):
        r *= s; y0 *= s; y1 *= s; ox = rr.uniform(-2, 2) * s; oz = rr.uniform(-2, 2) * s
        lathe(m, [(r * 0.35, y0 + 6 * s), (r, y0), (r * 0.05, y1)], 7, green, cx=ox, cz=oz)                        # drooping tier
        lathe(m, [(r * 0.98, y0 + 0.5 * s), (r * 0.3, y0 + 7 * s)], 7, greend, cx=ox, cz=oz)                       # its dark underside
        lathe(m, [(r * 0.66, y0 + (y1 - y0) * 0.33), (r * 0.72, y0 + (y1 - y0) * 0.35), (r * 0.38, y0 + (y1 - y0) * 0.66), (r * 0.33, y0 + (y1 - y0) * 0.65)], 7, snow, cx=ox + r * 0.05, cz=oz)   # a band of snow, not the whole tier
    lathe(m, [(3 * s, 166 * s), (0.01, 182 * s)], 7, green)
    lathe(m, [(2.2 * s, 172 * s), (0.01, 183 * s)], 7, snow)
    return m
def b6_capacitor(w, h, img):
    """a capacitor bank: a steel skid carrying four big cylindrical cans with insulated terminals, bus bars and a warning plate"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    skid, can, cand, cap, term, bus, warn, blk = c((70, 74, 80)), c((200, 204, 210)), c((150, 156, 164)), c((60, 64, 72)), c((190, 120, 60)), c((210, 160, 70)), c((240, 200, 40)), c((30, 30, 34))
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; hw, hd = (X1 - X0) / 2 - 3, (Z1 - Z0) / 2 - 3
    box(m, cx - hw, cx + hw, 0, 6, cz - hd, cz + hd, skid)
    for sx in (-1, 1): box(m, cx + sx * hw - (4 if sx > 0 else 0), cx + sx * hw + (0 if sx < 0 else 0) + (4 if sx < 0 else 0), 0, 8, cz - hd, cz + hd, cand)
    r = min(hw, hd) * 0.42
    for ox in (-1, 1):
        for oz in (-1, 1):
            px_, pz_ = cx + ox * hw * 0.5, cz + oz * hd * 0.5
            lathe(m, [(r, 6), (r, 64), (r * 0.94, 66), (0, 66)], 14, can, cx=px_, cz=pz_)                 # the can
            lathe(m, [(r * 1.02, 18), (r * 1.02, 22)], 14, cand, cx=px_, cz=pz_); lathe(m, [(r * 1.02, 50), (r * 1.02, 54)], 14, cand, cx=px_, cz=pz_)   # bands
            for t in (-1, 1):
                tx = px_ + t * r * 0.4
                lathe(m, [(2.6, 66), (2.6, 70), (3.4, 71), (2.2, 73), (3.4, 74), (2.2, 76), (1.2, 78)], 8, term, cx=tx, cz=pz_)   # ribbed insulator
    for t in (-1, 1): box(m, cx - hw * 0.5 - r * 0.4 * (-t) - 1.5 + t * 0, cx + hw * 0.5 + r * 0.4 * t + 1.5, 77, 79, cz - 1.5 + t * 0, cz + 1.5, bus) if False else None
    box(m, cx - hw * 0.7, cx + hw * 0.7, 77, 79, cz - hd * 0.5 - 1.5, cz - hd * 0.5 + 1.5, bus); box(m, cx - hw * 0.7, cx + hw * 0.7, 77, 79, cz + hd * 0.5 - 1.5, cz + hd * 0.5 + 1.5, bus)   # bus bars
    box(m, cx - 1.5, cx + 1.5, 77, 79, cz - hd * 0.5, cz + hd * 0.5, bus)
    box(m, cx - 9, cx + 9, 26, 40, cz + hd, cz + hd + 1, warn); box(m, cx - 2, cx + 2, 29, 37, cz + hd + 1, cz + hd + 1.4, blk)   # warning plate
    return m
def b6_food_tray(w, h, img, dirty):
    """a canteen tray at its real size: moulded compartments with food, or the scraps and smears left on a dirty one"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2; hw, hd = min(30.0, (X1 - X0) / 2), min(21.0, (Z1 - Z0) / 2)
    tray, rim, well = c((150, 160, 170)), c((126, 136, 146)), c((176, 184, 192))
    box(m, cx - hw, cx + hw, 0, 2, cz - hd, cz + hd, tray)
    box(m, cx - hw, cx + hw, 2, 4, cz - hd, cz - hd + 2, rim); box(m, cx - hw, cx + hw, 2, 4, cz + hd - 2, cz + hd, rim)
    box(m, cx - hw, cx - hw + 2, 2, 4, cz - hd, cz + hd, rim); box(m, cx + hw - 2, cx + hw, 2, 4, cz - hd, cz + hd, rim)
    cells = [(-0.62, -0.45, 0.32, 0.4), (0.0, -0.45, 0.26, 0.4), (0.55, -0.45, 0.36, 0.4), (-0.35, 0.42, 0.6, 0.42), (0.55, 0.42, 0.36, 0.42)]
    foods = [(236, 226, 190), (90, 150, 60), (150, 80, 40), (200, 140, 60), (220, 60, 50)]
    for k, (u, v, su, sv) in enumerate(cells):
        x0, x1 = cx + (u - su) * hw * 0.95, cx + (u + su) * hw * 0.95; z0, z1 = cz + (v - sv) * hd * 0.95, cz + (v + sv) * hd * 0.95
        box(m, x0 + 1, x1 - 1, 2, 2.6, z0 + 1, z1 - 1, well)
        if not dirty: box(m, x0 + 2, x1 - 2, 2.6, 6 + (k % 3), z0 + 2, z1 - 2, c(foods[k]))                    # a portion in each compartment
        elif k in (0, 2, 3):
            fc = tuple(int(v_ * 0.8) for v_ in foods[k]); box(m, x0 + 2, x0 + (x1 - x0) * 0.5, 2.6, 3.4, z0 + 2, z0 + (z1 - z0) * 0.6, c(fc))   # leftovers
    if dirty:
        for k in range(6): a = k * 1.1; box(m, cx + math.cos(a) * hw * 0.5 - 1.5, cx + math.cos(a) * hw * 0.5 + 1.5, 2.6, 3.2, cz + math.sin(a) * hd * 0.5 - 1, cz + math.sin(a) * hd * 0.5 + 1, c((120, 96, 70)))   # crumbs
        box(m, cx + hw * 0.2, cx + hw * 0.8, 3, 4, cz + hd * 0.1, cz + hd * 0.25, c((200, 200, 204)))         # a plastic spoon
    return m
def b6_serving_food(w, h, img):
    """the cooked food on the serving table: open steel pans of mash, peas, stew and beans, a ladle in one"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    steel, steeld = c((196, 200, 206)), c((150, 156, 164)); foods = [c((236, 226, 190)), c((90, 160, 60)), c((150, 80, 40)), c((200, 110, 50))]
    n = max(2, int(round((X1 - X0) / 34))); pw = (X1 - X0 - 4) / n
    for k in range(n):
        x0 = X0 + 2 + k * pw; x1 = x0 + pw - 2
        box(m, x0, x1, 0, 10, Z0 + 3, Z1 - 3, steel); box(m, x0 - 1, x1 + 1, 10, 11, Z0 + 2, Z1 - 2, steeld)          # pan with a lip
        box(m, x0 + 2, x1 - 2, 10, 10.6, Z0 + 5, Z1 - 5, foods[k % len(foods)])                                       # food level with the top
        for j in range(3): box(m, x0 + 4 + j * (x1 - x0 - 10) / 3, x0 + 8 + j * (x1 - x0 - 10) / 3, 10.6, 12, Z0 + 8 + j * 4, Z0 + 12 + j * 4, foods[k % len(foods)])   # lumps
    box(m, X0 + pw * 0.5 - 1, X0 + pw * 0.5 + 1, 10, 26, Z0 + 10, Z0 + 12, steeld); lathe(m, [(0, 9), (4, 9), (4, 12), (0, 12)], 8, steeld, cx=X0 + pw * 0.5, cz=Z0 + 14)   # ladle
    return m
def b6_bookcase(w, h, img):
    """a double-sided library bookcase: wooden carcass, three shelves a side, upright books of mixed heights with their spines out, a few leaning"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    wood, woodd, woodl = c((140, 96, 58)), c((100, 66, 38)), c((168, 122, 78))
    cols = [(124, 36, 36), (36, 54, 96), (40, 86, 56), (110, 76, 44), (176, 140, 60), (40, 40, 44), (220, 210, 186), (96, 40, 70), (60, 100, 120)]
    import random as _r; rr = _r.Random(5)
    zc = (Z0 + Z1) / 2; dz = min(22.0, (Z1 - Z0) / 2 - 1); H = 118; shelves = [6, 42, 78]
    box(m, X0, X0 + 4, 0, H, zc - dz, zc + dz, woodd); box(m, X1 - 4, X1, 0, H, zc - dz, zc + dz, woodd)             # ends
    box(m, X0, X1, H, H + 4, zc - dz - 1, zc + dz + 1, woodl)                                                         # top
    box(m, X0 + 4, X1 - 4, 0, H, zc - 1, zc + 1, woodd)                                                               # centre panel
    for y in shelves: box(m, X0 + 4, X1 - 4, y - 3, y, zc - dz, zc + dz, wood)                                       # shelves
    box(m, X0 + 4, X1 - 4, 0, 3, zc - dz, zc + dz, woodd)
    for side in (-1, 1):
        for y in shelves:
            x = X0 + 5
            while x < X1 - 8:
                bw = rr.uniform(4.0, 8.0); bh = rr.uniform(20, 30); col = cols[rr.randrange(len(cols))]
                if rr.random() < 0.06: x += rr.uniform(3, 7); continue                                                 # a gap
                if x + bw > X1 - 5: break
                d0, d1 = (zc + 2, zc + dz - 1) if side > 0 else (zc - dz + 1, zc - 2)
                box(m, x, x + bw, y, y + bh, d0, d1, c(col))
                sz = d1 if side > 0 else d0; sd = 0.4 * side
                if rr.random() < 0.25: box(m, x + 0.4, x + bw - 0.4, y + bh - 6, y + bh - 4.5, min(sz, sz + sd), max(sz, sz + sd), c(tuple(min(255, int(v * 1.4) + 30) for v in col)))   # spine band
                x += bw + 0.3
            if rr.random() < 0.5:                                                                                       # a couple of books lying flat at the end
                d0, d1 = (zc + 3, zc + dz - 2) if side > 0 else (zc - dz + 2, zc - 3)
                box(m, X1 - 14, X1 - 5, y, y + 3, d0, d1, c(cols[rr.randrange(len(cols))])); box(m, X1 - 13, X1 - 6, y + 3, y + 6, d0, d1, c(cols[rr.randrange(len(cols))]))
    return m
def b6_sofa(w, h, img, col):
    """a sofa: plinth, deep seat cushions, back cushions and rolled arms; back at -z"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    base, cush, cushl, foot = c(tuple(int(v * 0.7) for v in col)), c(col), c(tuple(min(255, int(v * 1.15)) for v in col)), c((40, 30, 24))
    wd, dp = X1 - X0, Z1 - Z0; arm = min(12.0, wd * 0.1)
    for fx in (X0 + 4, X1 - 8):
        for fz in (Z0 + 4, Z1 - 8): box(m, fx, fx + 4, 0, 6, fz, fz + 4, foot)
    box(m, X0 + 1, X1 - 1, 6, 22, Z0 + 1, Z1 - 1, base)                                     # plinth
    n = 2 if wd < 140 else 3; sw = (wd - 2 * arm) / n
    for k in range(n):                                                                       # seat and back cushions
        x0 = X0 + arm + k * sw
        box(m, x0 + 0.6, x0 + sw - 0.6, 22, 33, Z0 + dp * 0.3, Z1 - 2, cush); box(m, x0 + 1.5, x0 + sw - 1.5, 33, 34, Z0 + dp * 0.32, Z1 - 3, cushl)
        box(m, x0 + 0.6, x0 + sw - 0.6, 22, 64, Z0 + 3, Z0 + dp * 0.3, cush); box(m, x0 + 1.5, x0 + sw - 1.5, 64, 66, Z0 + 4, Z0 + dp * 0.28, cushl)
    box(m, X0 + 1, X1 - 1, 22, 60, Z0 + 1, Z0 + 4, base)                                     # back frame
    for ax in (X0 + 1, X1 - 1 - arm):                                                        # arms with a rolled top
        box(m, ax, ax + arm, 22, 44, Z0 + 1, Z1 - 1, cush); box(m, ax - 1, ax + arm + 1, 44, 48, Z0 + 1, Z1, cushl)
    return m
def b6_foam_mattress(w, h, img):
    """a foam mattress on a low wooden pallet: thick pale foam, a crumpled sheet and a flat pillow"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    pal, pald, foam, sheet, pil = c((150, 116, 76)), c((110, 84, 52)), c((226, 214, 170)), c((200, 196, 186)), c((236, 234, 226))
    box(m, X0 + 1, X1 - 1, 0, 6, Z0 + 1, Z1 - 1, pald)
    for k in range(5): z = Z0 + 2 + k * (Z1 - Z0 - 4) / 5; box(m, X0 + 1, X1 - 1, 6, 9, z, z + (Z1 - Z0 - 4) / 5 - 2, pal)      # pallet slats
    box(m, X0 + 3, X1 - 3, 9, 25, Z0 + 3, Z1 - 3, foam)                                                                        # foam block
    box(m, X0 + 2.5, X1 - 2.5, 20, 26, Z0 + (Z1 - Z0) * 0.32, Z1 - 4, sheet)                                                    # sheet over the lower part
    for k in range(3): z = Z0 + (Z1 - Z0) * (0.45 + k * 0.15); box(m, X0 + 4, X1 - 4, 26, 27.5, z, z + 4, c((186, 182, 172)))  # creases
    box(m, X0 + (X1 - X0) * 0.2, X1 - (X1 - X0) * 0.2, 25, 30, Z0 + 5, Z0 + 5 + (Z1 - Z0) * 0.18, pil)
    return m
def b6_blackboard(w, h, img):
    """a classroom blackboard on a wheeled A-frame stand, chalk writing on the board, a ledge with chalk"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    frame_, board, chalk, wheel = c((120, 84, 48)), c((34, 70, 50)), c((236, 236, 228), ), c((40, 40, 44))
    cz = (Z0 + Z1) / 2; wd = X1 - X0
    for fx in (X0 + 2, X1 - 6):
        box(m, fx, fx + 4, 6, 150, cz - 2, cz + 2, frame_)                                        # uprights
        box(m, fx - 1, fx + 5, 4, 8, cz - 14, cz + 14, frame_)                                    # feet
        for fz in (cz - 12, cz + 12): lathe(m, [(0, 0), (3, 0), (3, 4), (0, 4)], 8, wheel, cx=fx + 2, cz=fz)
    box(m, X0 + 6, X1 - 6, 60, 140, cz - 1.5, cz + 1.5, frame_)                                   # board frame
    box(m, X0 + 8, X1 - 8, 62, 138, cz + 1.5, cz + 2, board); box(m, X0 + 8, X1 - 8, 62, 138, cz - 2, cz - 1.5, board)   # the board, both faces
    import random as _r; rr = _r.Random(9)
    for row in range(5):                                                                          # chalk writing
        y = 128 - row * 14; x = X0 + 14
        while x < X1 - 20:
            lw = rr.uniform(6, 18); box(m, x, x + lw, y, y + 1.6, cz + 2, cz + 2.3, chalk); x += lw + rr.uniform(3, 8)
    box(m, X0 + 8, X1 - 8, 58, 61, cz + 1.5, cz + 6, frame_)                                      # chalk ledge
    for k in range(3): box(m, X0 + 20 + k * 9, X0 + 26 + k * 9, 61, 62.6, cz + 3, cz + 4.6, chalk)
    box(m, X1 - 30, X1 - 18, 61, 64, cz + 2, cz + 5.5, c((150, 110, 70)))                         # board rubber
    return m
def b6_small_table(w, h, img):
    """a small canteen table: white laminate top with a steel edge, four square steel legs"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    top, edge, leg = c((232, 232, 228)), c((150, 154, 160)), c((176, 180, 186))
    box(m, X0, X1, 68, 72, Z0, Z1, edge); box(m, X0 + 1, X1 - 1, 72, 73, Z0 + 1, Z1 - 1, top)
    _legs4(m, X0, X1, Z0, Z1, 68, 5, leg, 3)
    box(m, X0 + 5, X1 - 5, 58, 61, Z0 + 4, Z0 + 6, edge); box(m, X0 + 5, X1 - 5, 58, 61, Z1 - 6, Z1 - 4, edge)
    return m
def b6_tyre_run(w, h, img):
    """a tyre run: two rows of tyres laid flat and staggered, so a runner steps from one row to the other, in a low timber frame"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    rub, rubd, rim, wood = c((36, 36, 40)), c((24, 24, 28)), c((70, 72, 78)), c((120, 90, 60))
    along_x = (X1 - X0) >= (Z1 - Z0); L0, L1 = (X0, X1) if along_x else (Z0, Z1); D0, D1 = (Z0, Z1) if along_x else (X0, X1)
    dm = (D0 + D1) / 2; r = min(18.0, (D1 - D0) / 4 - 1)
    def B(a0, a1, y0, y1, b0, b1, col):
        if along_x: box(m, a0, a1, y0, y1, b0, b1, col)
        else: box(m, b0, b1, y0, y1, a0, a1, col)
    B(L0, L1, 0, 4, D0, D0 + 3, wood); B(L0, L1, 0, 4, D1 - 3, D1, wood); B(L0, L0 + 3, 0, 4, D0, D1, wood); B(L1 - 3, L1, 0, 4, D0, D1, wood)
    n = max(3, int((L1 - L0 - 10) // (2 * r)))
    for row, off in ((0, 0.0), (1, r)):                                                               # two rows, the second shifted half a tyre
        bc = dm - r - 1 if row == 0 else dm + r + 1
        for k in range(n):
            a = L0 + 6 + r + k * 2 * r + off
            if a + r > L1 - 4: break
            px_, pz_ = (a, bc) if along_x else (bc, a)
            lathe(m, [(r * 0.55, 0), (r, 1), (r, 9), (r * 0.55, 10), (r * 0.5, 5)], 12, rub, cx=px_, cz=pz_)      # tyre lying flat
            lathe(m, [(r * 0.52, 2), (r * 0.56, 2), (r * 0.56, 8), (r * 0.52, 8)], 12, rim, cx=px_, cz=pz_)
    return m
def b6_mail_bag(w, h, img):
    """the prisoners' letters: a canvas mail sack tied at the neck, with a few envelopes poking out and a label"""
    m = Mesh(); c = swatch; sack, sackd, rope, env, env2, tag = c((176, 160, 120)), c((140, 126, 92)), c((110, 80, 50)), c((240, 238, 230)), c((226, 210, 170)), c((200, 60, 50))
    lathe(m, [(0, 0), (12, 0), (16, 6), (17, 18), (14, 30), (8, 37), (5, 40)], 12, sack)          # the sack
    lathe(m, [(5.5, 38), (6, 38), (6, 41), (5.5, 41)], 10, rope)                                    # tie
    lathe(m, [(5, 41), (7, 44), (9, 47), (5, 47)], 10, sackd)                                       # gathered neck
    box(m, -3, 4, 44, 54, -1, 0, env); box(m, 1, 7, 43, 51, 1, 2, env2); box(m, -6, -1, 42, 49, 0, 1, env)   # envelopes sticking out
    box(m, 10, 17, 16, 24, 13, 14, tag); box(m, 12, 13, 24, 30, 13.2, 13.8, rope)                   # label on a string
    box(m, -9, 9, 14, 18, 15.5, 16.5, sackd)                                                         # stencilled band
    return m
def b6_ingredient_crate(w, h, img, kind):
    """a cardboard crate of ingredients, open at the top: vegetables, bread or meat piled inside"""
    m = Mesh(); c = swatch; card, cardd, flap = c((176, 140, 96)), c((140, 108, 70)), c((196, 162, 112))
    box(m, -18, 18, 0, 22, -14, 14, card); box(m, -17, 17, 1, 21.5, -13, 13, cardd)            # crate (its inside darker)
    for s_ in (-1, 1): box(m, -18, 18, 21, 22, s_ * 14 - (2 if s_ > 0 else 0) + (0 if s_ > 0 else 0), s_ * 14 + (0 if s_ > 0 else 2), flap)
    import random as _r; rr = _r.Random(len(kind) * 7)
    if kind == 'veg': cols = [(90, 160, 60), (120, 180, 70), (230, 140, 40)]
    elif kind == 'bread': cols = [(206, 150, 80), (186, 130, 64), (226, 176, 104)]
    else: cols = [(200, 80, 70), (220, 120, 110), (180, 60, 56)]
    for k in range(9):
        x = rr.uniform(-12, 12); z = rr.uniform(-8, 8); col = cols[rr.randrange(3)]
        if kind == 'bread': box(m, x - 5, x + 5, 18, 24, z - 3, z + 3, c(col))
        else: _ball(m, x, 21, z, 5, 4, 5, col, 8, 5)
    return m
_b6 = {'ingredient1': lambda w, h, i: b6_ingredient_crate(w, h, i, 'veg'), 'ingredient2': lambda w, h, i: b6_ingredient_crate(w, h, i, 'bread'), 'ingredient3': lambda w, h, i: b6_ingredient_crate(w, h, i, 'meat'), 'd_d11ingredient01': lambda w, h, i: b6_ingredient_crate(w, h, i, 'veg'), 'prisonerbus': b6_mail_bag, 'd_smalltable': b6_small_table, 'd_tyreapparatus': b6_tyre_run, 'd_doublebrownsofa': lambda w, h, i: b6_sofa(w, h, i, (110, 62, 46)), 'd_leathersofa': lambda w, h, i: b6_sofa(w, h, i, (60, 40, 32)), 'd_foammattress': b6_foam_mattress, 'd_blackboard': b6_blackboard, 'librarybookshelf': b6_bookcase, 'foodtray': lambda w, h, i: b6_food_tray(w, h, i, False), 'foodtraydirty1': lambda w, h, i: b6_food_tray(w, h, i, True), 'd_d11foodtray': lambda w, h, i: b6_food_tray(w, h, i, False), 'd_d11foodtraydirty': lambda w, h, i: b6_food_tray(w, h, i, True), 'servingtablelid': b6_serving_food, 'd_capacitormax': b6_capacitor, 'd_oxstatue': b6_ox_statue, 'd_treesnowyconifer': b6_snowy_conifer, 'd_treeyoungsnowyconifer': lambda w, h, i: b6_snowy_conifer(w, h, i, 0.6), 'd_floodlight': b6_flood_light, 'd_baretree': b6_bare_tree, 'd_lamp': b6_floor_lamp, 'd_dogbowl': b6_dog_bowl, 'd_dumbbellrack': b6_dumbbell_rack, 'd_treadmill': b6_treadmill, 'd_watercooler': b6_water_cooler, 'd_snackmachine': b6_snack_machine, 'd_recyclingbin': b6_recycling_bin, 'd_oaktable': b6_oak_table, 'd_chesstable': b6_chess_table, 'sortingtable': b6_sorting_desk, 'arcadecabinet': b6_arcade, 'd_swivelchair': b6_swivel_chair, 'oakdesk': b6_fancy_desk, 'd_chestofdrawers': b6_chest_of_drawers, 'd_woodenstool': lambda w, h, i: b6_stool(w, h, i, False), 'd_metalstool': lambda w, h, i: b6_stool(w, h, i, True), 'd_oldbed': lambda w, h, i: b6_bed(w, h, i, 'old'), 'superiorbed': lambda w, h, i: b6_bed(w, h, i, 'comfy'), 'd_computerstation': b6_computer, 'd_fan': lambda w, h, i: rot180(b6_fan(w, h, i)), 'electricchair': b6_electric_chair, 'tvlarge': b6_large_tv,
       'd_woodentable': lambda w, h, i: round_table(w, h, i, plates=False), 'd_ovalstafftable': lambda w, h, i: round_table(w, h, i, plates=False)}
for _k, _fn in _b6.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch6'))
        except Exception as _e: print('batch6 failed', _k, _e)
# ---------------------------------------------------------------- batch 5: school desk size, dirty / clean food trays
def _scaled_xyz(src, sx, sy, sz, dst):
    out = []
    for l in open(src, encoding='latin1'):
        if l.startswith('v '):
            p = l.split(); l = 'v %.4f %.4f %.4f' % (float(p[1]) * sx, float(p[2]) * sy, float(p[3]) * sz) + chr(10)
        out.append(l)
    open(dst, 'w', encoding='latin1').writelines(out)
def laundry_machine_body():
    """front-loading washer in the stock model's own frame (x 0..100, front at z 100) so the game's separate door lines up"""
    m = Mesh(); c = swatch
    white, whited, panel, chrome, glass, cloth, blk = c((236, 238, 240)), c((196, 200, 204)), c((70, 76, 86)), c((190, 194, 200)), c((34, 40, 50)), c((90, 130, 190)), c((30, 30, 34))
    box(m, 2, 98, 4, 96, 2, 98, white); box(m, 6, 94, 0, 4, 6, 94, panel)                                 # cabinet and plinth
    box(m, 1, 99, 96, 99.5, 1, 99, whited)                                                                  # worktop
    box(m, 2, 98, 80, 96, 98, 100, white); box(m, 8, 34, 84, 92, 100, 101, whited)                          # control strip, detergent drawer
    box(m, 10, 30, 87, 88, 101, 101.6, panel)
    for k, x in enumerate((50, 64, 78)): box(m, x - 3, x + 3, 85, 91, 100, 102.5, chrome if k else panel)   # dials
    box(m, 86, 92, 86, 90, 100, 100.6, c((60, 200, 90)))
    box(m, 2, 98, 4, 80, 98, 99, white)                                                                     # front panel
    box(m, 18, 82, 18, 82, 99, 100, chrome)                                                                 # chrome bezel round the door opening
    box(m, 22, 78, 22, 78, 99.2, 100.2, glass)                                                              # dark drum behind the glass
    box(m, 30, 70, 26, 44, 99.4, 100.3, cloth); box(m, 36, 62, 44, 54, 99.4, 100.3, c((200, 80, 80)))       # washing tumbling inside
    box(m, 2, 98, 4, 8, 98, 100, whited)                                                                    # kick plate
    return m
_lm = os.path.join(OUT, 'data', 'shapes', 'laundrymachine.obj'); laundry_machine_body().write(_lm, 'laundryMachine')
_ib = os.path.join(shapes_dir, 'ironingBoard.obj')
if os.path.exists(_ib) and 'd_ironingboardshort' in sprites:
    _scaled_xyz(_ib, 0.55, 1.0, 1.0, os.path.join(OUT, 'data', 'shapes', 'd_ironingboardshort.obj'))      # the small board is the game's own board, shorter
_pt = os.path.join(shapes_dir, 'PoolTable.obj')
if os.path.exists(_pt):      # the game's pool table model runs across its 2D sprite: turn it a quarter round so it lies the same way as the sprite
    _o = []
    for _l in open(_pt, encoding='latin1'):
        if _l.startswith('v ') or _l.startswith('vn '):
            _p = _l.split(); _x, _y, _z = float(_p[1]), float(_p[2]), float(_p[3]); _l = '%s %.4f %.4f %.4f' % (_p[0], _z, _y, -_x) + chr(10)
        _o.append(_l)
    open(os.path.join(OUT, 'data', 'shapes', 'pooltable.obj'), 'w', encoding='latin1').writelines(_o)
_gd = os.path.join(shapes_dir, 'guardDog.obj')
if os.path.exists(_gd) and not os.path.exists(os.path.join(OUT, 'data', 'shapes', 'dog.obj')): shutil.copyfile(_gd, os.path.join(OUT, 'data', 'shapes', 'dog.obj'))
def _recolour_obj(src, dst, test, rgb):
    """copy a stock model, moving every vertex whose atlas colour passes test() onto a swatch of colour rgb"""
    uvn = swatch(rgb); out = []
    for _l in open(src, encoding='latin1'):
        if _l.startswith('vt '):
            _p = _l.split(); u, v = float(_p[1]), float(_p[2]); x = min(AW - 1, max(0, int(u * AW))); y = min(AH - 1, max(0, int((1 - v) * AH)))
            r_, g_, b_ = px[x, y]
            if test(r_, g_, b_): _l = 'vt %.5f %.5f 0.0000' % (uvn[0], uvn[1]) + chr(10)
        out.append(_l)
    open(dst, 'w', encoding='latin1').writelines(out)
_sd2 = os.path.join(shapes_dir, 'staffDoor.obj')
if os.path.exists(_sd2) and 'd_securedoor' in sprites:      # the secure door is grey steel, not the staff door's green
    _recolour_obj(_sd2, os.path.join(OUT, 'data', 'shapes', 'd_securedoor.obj'), lambda r_, g_, b_: g_ > r_ + 20 and g_ > b_ + 10, (118, 124, 132))
# doors in the colours of their 2D icons: the game's jail door has a dark brown frame, the staff door a green leaf, the plain door is grey
_jail = lambda r_, g_, b_: r_ < 100 and g_ < 72 and b_ < 72 and r_ >= g_ and r_ + g_ + b_ > 60
_green = lambda r_, g_, b_: g_ > r_ + 20 and g_ > b_ + 10
_grey = lambda r_, g_, b_: abs(r_ - g_) < 12 and abs(g_ - b_) < 12 and 70 < r_ < 125
_DOOR_COLOURS = [
    ('jailDoor', _jail, {'d_jaildoorgrey': (128, 130, 136), 'd_jaildoororange': (220, 120, 40), 'd_jaildoorred': (176, 48, 40), 'd_jaildooryellow': (222, 190, 50), 'd_jaildoorwhite': (232, 232, 228)}),
    ('jailDoorLarge', _jail, {'d_jaildoorlargegrey': (128, 130, 136), 'd_jaildoorlargeorange': (220, 120, 40), 'd_jaildoorlargered': (176, 48, 40), 'd_jaildoorlargeyellow': (222, 190, 50), 'd_jaildoorlargewhite': (232, 232, 228)}),
    ('staffDoor', _green, {'d_staffdoorblue': (50, 100, 190), 'd_staffdoororange': (230, 140, 40), 'd_remotedoororange': (230, 140, 40), 'd_remotedoorred': (190, 50, 44),
                           'd_remotedooryellow': (226, 196, 60), 'd_remotedoorwhite': (232, 232, 228), 'd_remotedoormaroon': (128, 36, 56), 'd_remotedoorblack': (44, 44, 48), 'remotedoor': (100, 102, 108)}),
    ('door', lambda r_, g_, b_: r_ > b_ + 25 and r_ >= g_ and r_ < 200, {'d_visitordoor': (140, 40, 62), 'door': (206, 152, 64)}),
]
for _base, _test, _targets in _DOOR_COLOURS:
    _bp = os.path.join(shapes_dir, _base + '.obj')
    if not os.path.exists(_bp): continue
    for _k, _rgb in _targets.items():
        if _k in sprites or _k == 'door':
            _recolour_obj(_bp, os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _test, _rgb)
# ---------------------------------------------------------------- batch 7: everyday prison furniture that was still a raised sprite
def _fr(w, h, img): X0, X1, Z0, Z1 = frame_of(w, h, img); return X0, X1, Z0, Z1, (X0 + X1) / 2, (Z0 + Z1) / 2
def b7_dining_chair(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wd_, wdd = c((150, 104, 60)), c((110, 74, 40)); r = min(X1 - X0, Z1 - Z0) / 2 * 0.8
    for sx in (-1, 1):
        for sz in (-1, 1): box(m, cx + sx * r - 2 if sx > 0 else cx - r, cx + sx * r if sx > 0 else cx - r + 2, 0, 42, cz + sz * r - 2 if sz > 0 else cz - r, cz + sz * r if sz > 0 else cz - r + 2, wdd)
    box(m, cx - r, cx + r, 42, 46, cz - r, cz + r, wd_)
    box(m, cx - r, cx - r + 2, 46, 88, cz - r, cz - r + 2, wdd); box(m, cx + r - 2, cx + r, 46, 88, cz - r, cz - r + 2, wdd)
    for y in (60, 72, 84): box(m, cx - r, cx + r, y, y + 4, cz - r, cz - r + 2, wd_)
    return m
def b7_bench(w, h, img, col=(150, 104, 60), park=False, legcol=None):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wd_, leg = c(col), c(legcol or ((60, 62, 68) if park else tuple(int(v * 0.7) for v in col)))
    along = (X1 - X0) >= (Z1 - Z0); L0, L1 = (X0, X1) if along else (Z0, Z1); D0, D1 = (Z0, Z1) if along else (X0, X1); dm = (D0 + D1) / 2; dd = min(18.0, (D1 - D0) / 2)
    def B(a0, a1, y0, y1, b0, b1, col_):
        if along: box(m, a0, a1, y0, y1, b0, b1, col_)
        else: box(m, b0, b1, y0, y1, a0, a1, col_)
    for k in range(3): z0 = dm - dd + k * (2 * dd / 3); B(L0 + 2, L1 - 2, 40, 44, z0 + 0.6, z0 + 2 * dd / 3 - 0.6, wd_)      # slats
    for a in (L0 + 8, L1 - 12): B(a, a + 4, 0, 40, dm - dd + 2, dm + dd - 2, leg)
    if park:
        for k in range(2): z0 = dm - dd - 2; B(L0 + 2, L1 - 2, 52 + k * 12, 60 + k * 12, z0, z0 + 2, wd_)                    # backrest
        for a in (L0 + 8, L1 - 12): B(a, a + 4, 40, 74, dm - dd - 3, dm - dd, leg)
    return m
def b7_dining_table(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; top, edge, leg = c((226, 222, 210)), c((150, 154, 160)), c((120, 124, 130))
    box(m, X0, X1, 66, 70, Z0, Z1, edge); box(m, X0 + 1, X1 - 1, 70, 71, Z0 + 1, Z1 - 1, top); _legs4(m, X0, X1, Z0, Z1, 66, 5, leg, 4)
    return m
def b7_cot(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; fr_, can, pil = c((90, 96, 70)), c((120, 130, 90)), c((220, 218, 206))
    for sx in (X0 + 2, X1 - 5):
        box(m, sx, sx + 3, 22, 26, Z0 + 2, Z1 - 2, fr_)
        for sz in (Z0 + 4, Z1 - 7): box(m, sx, sx + 3, 0, 24, sz, sz + 3, fr_)
    box(m, X0 + 4, X1 - 4, 24, 27, Z0 + 3, Z1 - 3, can); box(m, X0 + (X1 - X0) * 0.2, X1 - (X1 - X0) * 0.2, 27, 31, Z0 + 5, Z0 + 5 + (Z1 - Z0) * 0.16, pil)
    return m
def b7_radio(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; body, grill, dial, blk = c((150, 110, 70)), c((70, 60, 50)), c((220, 200, 150)), c((40, 40, 44))
    box(m, cx - 14, cx + 14, 0, 18, cz - 6, cz + 6, body); box(m, cx - 12, cx + 2, 3, 15, cz + 6, cz + 6.6, grill)
    box(m, cx + 4, cx + 12, 8, 13, cz + 6, cz + 6.6, dial); lathe(m, [(0, 0), (1.6, 0), (1.6, 1.4), (0, 1.4)], 8, blk, cx=cx + 8, cz=cz + 6.6)
    box(m, cx + 10, cx + 11, 18, 40, cz - 1, cz, c((170, 172, 176)))                                                        # aerial
    return m
def b7_boombox(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; body, spk, chrome = c((40, 42, 48)), c((24, 24, 28)), c((180, 184, 190))
    box(m, cx - 18, cx + 18, 0, 20, cz - 6, cz + 6, body)
    for sx in (-10, 10): zcyl(m, cx + sx, 10, cz + 6, cz + 7, 6, spk, 12)                                                   # speakers facing the front
    box(m, cx - 4, cx + 4, 8, 14, cz + 6, cz + 7, chrome); box(m, cx - 14, cx + 14, 20, 22, cz - 1, cz + 1, chrome); box(m, cx - 14, cx - 12, 20, 28, cz - 1, cz + 1, chrome); box(m, cx + 12, cx + 14, 20, 28, cz - 1, cz + 1, chrome); box(m, cx - 14, cx + 14, 26, 28, cz - 1, cz + 1, chrome)
    return m
def b7_gun_rack(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wd_, gun, stock = c((110, 76, 44)), c((40, 40, 44)), c((90, 60, 36))
    zb = Z0 + 2; box(m, X0 + 2, X1 - 2, 0, 6, zb, zb + 16, wd_); box(m, X0 + 2, X1 - 2, 0, 120, zb, zb + 4, wd_); box(m, X0 + 2, X1 - 2, 70, 74, zb, zb + 14, wd_)
    n = max(3, int((X1 - X0 - 10) // 14))
    for k in range(n):
        x = X0 + 8 + k * (X1 - X0 - 16) / max(1, n - 1)
        box(m, x - 2, x + 2, 6, 30, zb + 6, zb + 12, stock); box(m, x - 1, x + 1, 30, 100, zb + 7, zb + 10, gun)
    return m
def b7_tall_fridge(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wh_, seam, hnd = c((232, 234, 236)), c((180, 184, 188)), c((150, 154, 160))
    box(m, X0 + 2, X1 - 2, 4, 180, Z0 + 2, Z1 - 3, wh_); box(m, X0 + 4, X1 - 4, 0, 4, Z0 + 4, Z1 - 5, c((60, 62, 66)))
    box(m, X0 + 2, X1 - 2, 118, 120, Z1 - 3, Z1 - 2, seam); box(m, X0 + 2, X1 - 2, 4, 180, Z1 - 3, Z1 - 2.4, wh_)
    box(m, X1 - 10, X1 - 7, 128, 160, Z1 - 2.4, Z1 + 1, hnd); box(m, X1 - 10, X1 - 7, 70, 108, Z1 - 2.4, Z1 + 1, hnd)
    return m
def b7_storage_shelf(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; steel, box_c = c((120, 126, 134)), [c((176, 140, 96)), c((150, 116, 76)), c((196, 162, 112))]
    import random as _r; rr = _r.Random(3)
    for x in (X0 + 2, X1 - 5):
        for z in (Z0 + 2, Z1 - 5): box(m, x, x + 3, 0, 150, z, z + 3, steel)
    for y in (10, 56, 102, 146):
        box(m, X0 + 2, X1 - 2, y, y + 3, Z0 + 2, Z1 - 2, steel)
        if y < 140:
            x = X0 + 6
            while x < X1 - 14:
                bw = rr.uniform(10, 22); bh = rr.uniform(12, 34); box(m, x, min(x + bw, X1 - 6), y + 3, y + 3 + bh, Z0 + 5, Z1 - 5, box_c[rr.randrange(3)]); x += bw + rr.uniform(2, 6)
    return m
def b7_pullup_bars(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; steel, pad = c((150, 156, 164)), c((40, 40, 44))
    for x in (X0 + 4, X1 - 8):
        box(m, x, x + 4, 0, 200, cz - 2, cz + 2, steel); box(m, x - 4, x + 8, 0, 3, cz - 10, cz + 10, steel)
    box(m, X0 + 4, X1 - 4, 194, 198, cz - 1.5, cz + 1.5, steel); box(m, X0 + 4, X1 - 4, 150, 154, cz - 1.5, cz + 1.5, steel)
    for x in (cx - 16, cx + 12): box(m, x, x + 4, 193, 199, cz - 2, cz + 2, pad)
    return m
def b7_barrel(w, h, img, col):
    m = Mesh(); c = swatch; b, bd = c(col), c(tuple(int(v * 0.7) for v in col))
    lathe(m, [(0, 0), (14, 0), (16, 10), (16.5, 25), (16, 40), (14, 48), (0, 48)], 14, b)
    for y in (8, 24, 40): lathe(m, [(16.4, y), (17, y), (17, y + 2), (16.4, y + 2)], 14, bd)
    return m
def b7_fountain(w, h, img):
    m = Mesh(); c = swatch; steel, bowl, water = c((180, 184, 190)), c((150, 156, 164)), c((120, 180, 230))
    box(m, -8, 8, 0, 70, -6, 4, steel); lathe(m, [(0, 70), (14, 72), (15, 78), (11, 76), (0, 74)], 14, bowl, cz=4); lathe(m, [(0, 75), (10, 75)], 12, water, cz=4)
    box(m, -1, 1, 76, 82, 0, 2, steel); box(m, 5, 9, 60, 64, 4, 6, steel)
    return m
def b7_first_aid(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wh_, red = c((236, 236, 232)), c((200, 40, 40))
    box(m, cx - 20, cx + 20, 100, 150, Z0 + 2, Z0 + 14, wh_); box(m, cx - 3, cx + 3, 112, 138, Z0 + 14, Z0 + 15, red); box(m, cx - 13, cx + 13, 122, 128, Z0 + 14, Z0 + 15, red)
    box(m, cx - 20, cx + 20, 0, 70, Z0 + 2, Z0 + 30, wh_); box(m, cx - 18, cx + 18, 4, 66, Z0 + 30, Z0 + 31, c((210, 212, 214))); box(m, cx - 2, cx + 2, 40, 50, Z0 + 31, Z0 + 33, c((150, 156, 164)))
    return m
def b7_hand_dryer(w, h, img):
    """a wall-mounted hand dryer: a rounded steel box at chest height against the wall, nozzle pointing down"""
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; body, bodyd, nz, lt = c((214, 218, 222)), c((170, 176, 182)), c((60, 62, 66)), c((80, 200, 120))
    zb = Z0 + 1
    box(m, cx - 11, cx + 11, 96, 128, zb, zb + 12, body); box(m, cx - 10, cx + 10, 128, 130, zb, zb + 11, bodyd)
    box(m, cx - 10, cx + 10, 94, 96, zb + 1, zb + 11, bodyd); lathe(m, [(0, 88), (3, 88), (4, 94), (0, 94)], 10, nz, cx=cx, cz=zb + 8)
    box(m, cx + 5, cx + 8, 118, 121, zb + 12, zb + 12.6, lt); box(m, cx - 8, cx + 2, 104, 112, zb + 12, zb + 12.4, bodyd)
    for _y in (0.0,): m.quad([(cx, _y, zb), (cx, _y, zb), (cx, _y, zb), (cx, _y, zb)], (0, 1, 0), [body] * 4)       # anchor at the floor so the game keeps its height
    return m
_b7 = {'d_handdryer': b7_hand_dryer, 'diningchair': b7_dining_chair, 'd_diningchair': b7_dining_chair, 'bench': lambda w, h, i: b7_bench(w, h, i, (240, 240, 236), legcol=(150, 154, 160)), 'd_oakbench': lambda w, h, i: b7_bench(w, h, i, (130, 86, 48)),
       'd_parkbench': lambda w, h, i: b7_bench(w, h, i, (140, 100, 60), True), 'd_junglebench': lambda w, h, i: b7_bench(w, h, i, (110, 90, 50)),
       'd_diningtablelarge': b7_dining_table, 'd_diningtablesmall': b7_dining_table, 'cot': b7_cot, 'radio': b7_radio, 'boombox': b7_boombox, 'gunrack': b7_gun_rack,
       'd_tallfridge': b7_tall_fridge, 'd_storageshelf': b7_storage_shelf, 'd_pullupbars': b7_pullup_bars, 'd_metalbarrel': lambda w, h, i: b7_barrel(w, h, i, (60, 90, 140)),
       'd_woodenbarrel': lambda w, h, i: b7_barrel(w, h, i, (130, 90, 50)), 'd_drinkingfountain': b7_fountain, 'd_firstaidstation': b7_first_aid}
for _k, _fn in _b7.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch7'))
        except Exception as _e: print('batch7 failed', _k, _e)
def b8_table_tennis(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; top, line, leg, net = c((30, 100, 60)), c((240, 240, 236)), c((50, 52, 58)), c((230, 230, 226))
    box(m, X0 + 2, X1 - 2, 66, 70, Z0 + 2, Z1 - 2, top); _legs4(m, X0 + 2, X1 - 2, Z0 + 2, Z1 - 2, 66, 4, leg, 6)
    box(m, X0 + 2, X1 - 2, 70, 70.4, Z0 + 2, Z0 + 3, line); box(m, X0 + 2, X1 - 2, 70, 70.4, Z1 - 3, Z1 - 2, line); box(m, X0 + 2, X0 + 3, 70, 70.4, Z0 + 2, Z1 - 2, line); box(m, X1 - 3, X1 - 2, 70, 70.4, Z0 + 2, Z1 - 2, line)
    along = (X1 - X0) >= (Z1 - Z0)
    if along: box(m, cx - 0.6, cx + 0.6, 70, 80, Z0, Z1, net); box(m, X0 + 2, X1 - 2, 70, 70.4, cz - 0.5, cz + 0.5, line)
    else: box(m, X0, X1, 70, 80, cz - 0.6, cz + 0.6, net); box(m, cx - 0.5, cx + 0.5, 70, 70.4, Z0 + 2, Z1 - 2, line)
    return m
def b8_table_football(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wood, pitch, rod, red, blue = c((130, 90, 50)), c((40, 130, 60)), c((190, 194, 200)), c((200, 50, 50)), c((50, 90, 200))
    box(m, X0 + 4, X1 - 4, 50, 62, Z0 + 4, Z1 - 4, wood); box(m, X0 + 7, X1 - 7, 62, 63, Z0 + 7, Z1 - 7, pitch)
    for (a0, a1, b0, b1) in ((X0 + 4, X1 - 4, Z0 + 4, Z0 + 7), (X0 + 4, X1 - 4, Z1 - 7, Z1 - 4), (X0 + 4, X0 + 7, Z0 + 4, Z1 - 4), (X1 - 7, X1 - 4, Z0 + 4, Z1 - 4)): box(m, a0, a1, 62, 76, b0, b1, wood)      # open-topped box: the players show
    _legs4(m, X0 + 4, X1 - 4, Z0 + 4, Z1 - 4, 50, 5, wood, 2)
    along = (X1 - X0) >= (Z1 - Z0); L0, L1 = (X0, X1) if along else (Z0, Z1); D0, D1 = (Z0, Z1) if along else (X0, X1)
    for k in range(6):
        a = L0 + 14 + k * (L1 - L0 - 28) / 5; col = red if k % 2 == 0 else blue
        if along: box(m, a - 0.8, a + 0.8, 74, 75.6, D0 - 6, D1 + 6, rod)
        else: box(m, D0 - 6, D1 + 6, 74, 75.6, a - 0.8, a + 0.8, rod)
        for j in range(3):
            b = D0 + (D1 - D0) * (j + 1) / 4
            if along: box(m, a - 1.5, a + 1.5, 63, 77, b - 1.5, b + 1.5, col)
            else: box(m, b - 1.5, b + 1.5, 63, 77, a - 1.5, a + 1.5, col)
    return m
def b8_filing_cabinet(w, h, img, col=(110, 116, 124), depth=None):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; b, bd, hd = c(col), c(tuple(int(v * 0.8) for v in col)), c((200, 204, 210))
    if depth: Z0 = max(Z0, Z1 - depth)      # backed against the wall, front stays where it was
    box(m, X0 + 3, X1 - 3, 0, 130, Z0 + 3, Z1 - 3, b)
    for k in range(4):
        y0 = 4 + k * 31; box(m, X0 + 5, X1 - 5, y0, y0 + 28, Z1 - 3, Z1 - 2, bd); box(m, cx - 6, cx + 6, y0 + 20, y0 + 23, Z1 - 2, Z1, hd); box(m, cx - 4, cx + 4, y0 + 10, y0 + 15, Z1 - 2, Z1 - 1.6, c((236, 232, 214)))
    return m
def b8_flipboard(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; leg, board, paper, ink = c((70, 72, 78)), c((230, 230, 226)), c((248, 248, 244)), c((40, 70, 150))
    for sx in (-1, 1): box(m, cx + sx * 16 - 1.5, cx + sx * 16 + 1.5, 0, 150, cz - 1.5, cz + 1.5, leg)
    box(m, cx - 1.5, cx + 1.5, 0, 140, cz - 14, cz - 11, leg)
    box(m, cx - 20, cx + 20, 80, 146, cz + 1.5, cz + 3, board); box(m, cx - 19, cx + 19, 82, 144, cz + 3, cz + 3.4, paper)
    for k in range(4): box(m, cx - 14, cx - 14 + 8 + (k * 7) % 20, 130 - k * 10, 132 - k * 10, cz + 3.4, cz + 3.7, ink)
    return m
def b8_bleachers(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; seat, frame = c((60, 100, 170)), c((150, 154, 160))
    n = 4; d = (Z1 - Z0) / n
    for k in range(n):
        z0 = Z0 + k * d; y = 12 + (n - 1 - k) * 24
        box(m, X0 + 2, X1 - 2, y, y + 5, z0, z0 + d, seat); box(m, X0 + 2, X1 - 2, 0, y, z0 + d - 3, z0 + d, frame)
    for x in (X0 + 2, X1 - 5): box(m, x, x + 3, 0, 90, Z0, Z0 + 3, frame)
    return m
def b8_coffee_machine(w, h, img):
    # as the 2D art: a round wooden side table on three legs; cream filter machine at the back, glass jug of coffee in front, white mug on the left
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    wood, woodd, cream, creamd, dark, glass, coffee, cup, red = c((214, 150, 70)), c((140, 88, 40)), c((236, 230, 200)), c((200, 194, 160)), c((50, 46, 46)), c((200, 214, 220)), c((110, 64, 34)), c((240, 240, 236)), c((200, 50, 40))
    R = max(20.0, min(X1 - X0, Z1 - Z0) / 2 - 2); TOP = 48
    lathe(m, [(0, TOP - 4), (R, TOP - 4), (R, TOP), (0, TOP)], 22, wood)      # table top
    for k in range(3):
        a = math.radians(90 + k * 120); lx, lz = cx + math.cos(a) * R * 0.6, cz + math.sin(a) * R * 0.6
        box(m, lx - 2.5, lx + 2.5, 0, TOP - 4, lz - 2.5, lz + 2.5, woodd)
    mz = cz - R * 0.35      # coffee maker, back half of the table
    box(m, cx - 11, cx + 11, TOP, TOP + 4, mz - 9, mz + 13, creamd)            # base plate
    box(m, cx - 10, cx + 10, TOP + 4, TOP + 34, mz - 9, mz - 1, cream)         # tower
    box(m, cx - 11, cx + 11, TOP + 34, TOP + 44, mz - 9, mz + 9, cream)        # head with the filter
    box(m, cx - 6, cx + 6, TOP + 22, TOP + 28, mz - 1, mz - 0.4, dark); box(m, cx - 4.5, cx - 1.5, TOP + 23.5, TOP + 26.5, mz - 0.4, mz - 0.1, red)
    jz = mz + 6      # glass jug on the hot plate, coffee inside, black lid and handle
    lathe(m, [(0, TOP + 4), (6.5, TOP + 4), (8, TOP + 12), (6, TOP + 22), (0, TOP + 22)], 14, glass, cx=cx, cz=jz)
    lathe(m, [(0, TOP + 4.2), (6.7, TOP + 4.2), (8.2, TOP + 11), (0, TOP + 11)], 14, coffee, cx=cx, cz=jz)
    lathe(m, [(0, TOP + 22), (6.2, TOP + 22), (5.6, TOP + 25), (0, TOP + 25)], 12, dark, cx=cx, cz=jz)
    box(m, cx + 7.5, cx + 13, TOP + 10, TOP + 12, jz - 1, jz + 1, dark); box(m, cx + 11, cx + 13, TOP + 10, TOP + 21, jz - 1, jz + 1, dark)
    ux, uz = cx - R * 0.62, cz - R * 0.05      # mug with coffee and a handle
    lathe(m, [(0, TOP), (4.5, TOP), (5, TOP + 9), (0, TOP + 9)], 12, cup, cx=ux, cz=uz)
    lathe(m, [(0, TOP + 9.05), (4.4, TOP + 9.05), (4.4, TOP + 9.3), (0, TOP + 9.3)], 12, coffee, cx=ux, cz=uz)
    box(m, ux - 8.5, ux - 4.5, TOP + 2, TOP + 3.5, uz - 1, uz + 1, cup); box(m, ux - 8.5, ux - 7, TOP + 2, TOP + 7.5, uz - 1, uz + 1, cup); box(m, ux - 8.5, ux - 4.5, TOP + 6, TOP + 7.5, uz - 1, uz + 1, cup)
    return m
def b8_oven(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; steel, dark, glass, knob = c((196, 200, 206)), c((60, 62, 66)), c((40, 30, 24)), c((30, 30, 34))
    box(m, X0 + 2, X1 - 2, 0, 90, Z0 + 2, Z1 - 3, steel); box(m, X0 + 6, X1 - 6, 14, 70, Z1 - 3, Z1 - 2, dark); box(m, X0 + 12, X1 - 12, 24, 60, Z1 - 2, Z1 - 1.6, glass)
    box(m, X0 + 10, X1 - 10, 64, 67, Z1 - 2, Z1 + 1, steel); box(m, X0 + 6, X1 - 6, 76, 86, Z1 - 3, Z1 - 2, dark)
    for k in range(4): lathe(m, [(0, 0), (2, 0), (2, 2), (0, 2)], 8, knob, cx=X0 + 14 + k * (X1 - X0 - 28) / 3, cz=Z1 - 1)
    return m
def b8_generator(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; body, dark, warn, pipe = c((200, 170, 40)), c((50, 52, 56)), c((30, 30, 30)), c((120, 124, 130))
    box(m, X0 + 2, X1 - 2, 0, 8, Z0 + 2, Z1 - 2, dark); box(m, X0 + 4, X1 - 4, 8, 70, Z0 + 4, Z1 - 4, body)
    for k in range(6): box(m, X0 + 10, X1 - 10, 20 + k * 7, 22 + k * 7, Z1 - 4, Z1 - 3, dark)
    lathe(m, [(4, 70), (4, 96), (5, 98), (0, 98)], 10, pipe, cx=X1 - 16, cz=Z0 + 14); box(m, cx - 8, cx + 8, 52, 62, Z1 - 4, Z1 - 3.4, warn)
    return m
def b8_small_pile(w, h, img, kind):
    """small loose things: a stack of books, a folded uniform, a newspaper, a letter, a plate"""
    m = Mesh(); c = swatch; import random as _r; rr = _r.Random(len(kind))
    if kind == 'books':
        y = 0
        for k in range(5): cl = [(124, 36, 36), (36, 54, 96), (40, 86, 56), (176, 140, 60), (96, 40, 70)][k]; bw = rr.uniform(14, 20); bd = rr.uniform(10, 14); a = rr.uniform(-3, 3); box(m, -bw / 2 + a, bw / 2 + a, y, y + 3.5, -bd / 2, bd / 2, c(cl)); y += 3.5
    elif kind in ('uniform', 'uniformdirty'):
        col = (226, 128, 36) if kind == 'uniform' else (170, 110, 60)
        for k in range(3): box(m, -11 + k * 0.6, 11 + k * 0.6, k * 3, k * 3 + 3, -8, 8, c(col)); box(m, -11, 11, 9, 9.5, -3, 3, c(tuple(int(v * 0.8) for v in col)))
    elif kind == 'newspaper': box(m, -12, 12, 0, 1.2, -8, 8, c((226, 222, 210))); box(m, -10, 0, 1.2, 1.4, -6, -2, c((90, 90, 90))); box(m, 2, 10, 1.2, 1.4, -6, 6, c((140, 140, 140)))
    elif kind == 'mail': box(m, -7, 7, 0, 1, -4.5, 4.5, c((240, 238, 230))); box(m, 3, 6, 1, 1.2, -3.5, -1.5, c((200, 60, 50)))
    else: lathe(m, [(0, 0), (7, 0), (9, 2), (0, 2)], 14, c((236, 236, 232))); _ball(m, 0, 2, 0, 5, 2, 5, (200, 150, 70), 8, 4)
    return m
_b8 = {'d_tabletennis': b8_table_tennis, 'd_tablefootball': b8_table_football, 'd_filingcabinetfancy': lambda w, h, i: b8_filing_cabinet(w, h, i, (120, 84, 48), depth=52),
       'd_medicinecabinetpsych': lambda w, h, i: b8_filing_cabinet(w, h, i, (226, 228, 230)), 'd_flipboard': b8_flipboard, 'd_bleachers': b8_bleachers,
       'd_coffeemachine': b8_coffee_machine, 'd_breadoven': b8_oven, 'd_backupgenerator': b8_generator,
       'bookssorted': lambda w, h, i: b8_small_pile(w, h, i, 'books'), 'booksunsorted': lambda w, h, i: b8_small_pile(w, h, i, 'books'),
       'prisoneruniform': lambda w, h, i: b8_small_pile(w, h, i, 'uniform'), 'prisonuniformdirty': lambda w, h, i: b8_small_pile(w, h, i, 'uniformdirty'),
       'newspaper': lambda w, h, i: b8_small_pile(w, h, i, 'newspaper'), 'mailunopened1': lambda w, h, i: b8_small_pile(w, h, i, 'mail'), 'mailopened1': lambda w, h, i: b8_small_pile(w, h, i, 'mail'),
       'plate': lambda w, h, i: b8_small_pile(w, h, i, 'plate'), 'food2': lambda w, h, i: b8_small_pile(w, h, i, 'plate')}
for _k, _fn in _b8.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch8'))
        except Exception as _e: print('batch8 failed', _k, _e)
def b8_hay(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; hay, hayd, tw = c((214, 186, 100)), c((180, 152, 76)), c((150, 110, 60))
    box(m, X0 + 3, X1 - 3, 0, 40, Z0 + 6, Z1 - 6, hay)
    for k in range(6): box(m, X0 + 3, X1 - 3, 3 + k * 6, 4 + k * 6, Z1 - 6, Z1 - 5.6, hayd)
    for x in (cx - (X1 - X0) * 0.2, cx + (X1 - X0) * 0.2): box(m, x - 1, x + 1, 0, 40.4, Z0 + 5.6, Z1 - 5.6, tw)
    return m
def b8_gravestone(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; st, std, grass = c((150, 150, 154)), c((110, 110, 114)), c((70, 110, 50))
    box(m, cx - 14, cx + 14, 0, 4, cz - 6, cz + 6, std); box(m, cx - 11, cx + 11, 4, 44, cz - 3, cz + 3, st); lathe(m, [(0, 44), (11, 44), (8, 51), (0, 54)], 10, st, cx=cx, cz=cz, sz=0.27)
    box(m, cx - 6, cx + 6, 30, 31, cz + 3, cz + 3.4, std); box(m, cx - 8, cx + 8, 22, 23, cz + 3, cz + 3.4, std)
    return m
def b8_battery(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; body, top, red, blk = c((60, 64, 70)), c((40, 42, 46)), c((200, 50, 40)), c((30, 30, 32))
    box(m, X0 + 4, X1 - 4, 0, 60, Z0 + 4, Z1 - 4, body); box(m, X0 + 4, X1 - 4, 60, 64, Z0 + 4, Z1 - 4, top)
    for k, col in ((0, red), (1, blk)): lathe(m, [(0, 64), (3, 64), (3, 70), (0, 70)], 8, col, cx=X0 + 14 + k * (X1 - X0 - 28), cz=cz)
    box(m, cx - 10, cx + 10, 30, 42, Z1 - 4, Z1 - 3.4, c((220, 190, 40)))
    return m
def b8_dough_mixer(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; body, bowl, dough = c((220, 222, 226)), c((180, 186, 192)), c((240, 226, 190))
    box(m, cx - 14, cx + 14, 0, 10, cz - 14, cz + 14, body); box(m, cx - 6, cx + 6, 10, 80, cz - 14, cz - 4, body); box(m, cx - 8, cx + 8, 72, 88, cz - 14, cz + 6, body)
    lathe(m, [(0, 30), (8, 30), (14, 40), (15, 56), (14, 56), (0, 50)], 14, bowl, cx=cx, cz=cz + 4); _ball(m, cx, 52, cz + 4, 11, 5, 11, (240, 226, 190), 10, 5)
    box(m, cx - 1, cx + 1, 56, 72, cz + 3, cz + 5, bowl)
    return m
_b8.update({'d_haybale': b8_hay, 'd_gravestone': b8_gravestone, 'd_battery': b8_battery, 'd_doughmixer': b8_dough_mixer})
for _k in ('d_haybale', 'd_gravestone', 'd_battery', 'd_doughmixer'):
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _b8[_k](_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n)
        except Exception as _e: print('batch8b failed', _k, _e)
# ---------------------------------------------------------------- batch 9: security, medical and outdoor kit
def b9_cctv(w, h, img):
    m = Mesh(); c = swatch; body, dark, lens = c((226, 228, 232)), c((60, 62, 68)), c((30, 40, 60))
    box(m, -4, 4, 150, 166, -44, -40, dark); box(m, -1.5, 1.5, 152, 156, -40, -30, dark)                      # wall bracket at the back
    box(m, -6, 6, 146, 158, -32, -8, body); box(m, -7, 7, 157, 159, -33, -6, body)                          # camera body with a hood
    zcyl(m, 0, 151, -8, -6, 4, lens, 10)
    m.quad([(0, 0.0, 0), (0, 0.0, 0), (0, 0.0, 0), (0, 0.0, 0)], (0, 1, 0), [body] * 4)                     # anchor at the floor
    return m
def b9_camera_pillar(w, h, img):
    m = Mesh(); c = swatch; pole, body, lens = c((90, 94, 100)), c((226, 228, 232)), c((30, 40, 60))
    box(m, -10, 10, 0, 3, -10, 10, pole); lathe(m, [(3, 3), (2.6, 190)], 8, pole)
    box(m, -6, 6, 184, 196, -4, 20, body); zcyl(m, 0, 189, 20, 22, 4, lens, 10)
    return m
def b9_fire_alarm(w, h, img):
    m = Mesh(); c = swatch; red, wh = c((200, 40, 36)), c((236, 236, 232))
    box(m, -7, 7, 120, 136, -46, -42, red); box(m, -4, 4, 124, 132, -42, -41.4, wh); box(m, -2, 2, 126, 130, -41.4, -40.6, red)
    lathe(m, [(0, 142), (6, 142), (7, 146), (0, 150)], 10, red, cz=-42)
    m.quad([(0, 0.0, 0), (0, 0.0, 0), (0, 0.0, 0), (0, 0.0, 0)], (0, 1, 0), [red] * 4)
    return m
def b9_health_monitor(w, h, img):
    m = Mesh(); c = swatch; pole, body, scr, wave = c((180, 184, 190)), c((220, 222, 226)), c((20, 30, 30)), c((80, 220, 120))
    for k in range(5): a = math.radians(90 + 72 * k); box(m, min(0, math.cos(a) * 14) - 1.5, max(0, math.cos(a) * 14) + 1.5, 0, 3, min(0, math.sin(a) * 14) - 1.5, max(0, math.sin(a) * 14) + 1.5, pole)
    lathe(m, [(1.6, 3), (1.6, 120)], 8, pole); box(m, -14, 14, 110, 132, -6, 4, body); box(m, -12, 12, 113, 129, 4, 4.6, scr)
    for k in range(6): box(m, -10 + k * 4, -8 + k * 4, 118 + (3 if k % 2 else -2), 120 + (3 if k % 2 else -2), 4.6, 5, wave)
    return m
def b9_desk_monitor(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; desk, deskd, mon, scr, map_ = c((150, 154, 160)), c((100, 104, 110)), c((40, 42, 46)), c((30, 60, 90)), c((90, 200, 140))
    box(m, X0 + 2, X1 - 2, 66, 70, Z0 + 2, Z1 - 2, desk); _legs4(m, X0 + 2, X1 - 2, Z0 + 2, Z1 - 2, 66, 4, deskd, 2)
    nm = max(1, int((X1 - X0) / 45))
    for k in range(nm):
        mx = X0 + (X1 - X0) * (k + 0.5) / nm; box(m, mx - 18, mx + 18, 74, 100, Z0 + 6, Z0 + 10, mon); box(m, mx - 16, mx + 16, 76, 98, Z0 + 10, Z0 + 10.5, scr)
        box(m, mx - 2, mx + 2, 70, 74, Z0 + 6, Z0 + 10, mon); box(m, mx - 10, mx + 6, 82, 92, Z0 + 10.5, Z0 + 10.8, map_)
    return m
def b9_router(w, h, img):
    m = Mesh(); c = swatch; body, ant, led = c((40, 42, 48)), c((30, 30, 34), ), c((80, 220, 120))
    box(m, -12, 12, 0, 5, -8, 8, body)
    for x in (-9, 9): box(m, x - 1, x + 1, 5, 22, -7, -5, ant)
    for k in range(4): box(m, -8 + k * 4, -6 + k * 4, 3, 4, 8, 8.4, led)
    return m
def b9_scanner(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; body, belt, tunnel, flap = c((210, 212, 216)), c((40, 40, 44)), c((70, 74, 80)), c((60, 60, 64))
    along = (X1 - X0) >= (Z1 - Z0)
    def B(a0, a1, y0, y1, b0, b1, col_):
        if along: box(m, a0, a1, y0, y1, b0, b1, col_)
        else: box(m, b0, b1, y0, y1, a0, a1, col_)
    L0, L1 = (X0, X1) if along else (Z0, Z1); D0, D1 = (Z0, Z1) if along else (X0, X1); dm = (D0 + D1) / 2
    B(L0 + 2, L1 - 2, 0, 66, dm - 16, dm + 16, tunnel); B(L0 + 2, L1 - 2, 66, 70, dm - 14, dm + 14, belt)
    lm = (L0 + L1) / 2; B(lm - 30, lm + 30, 66, 120, dm - 22, dm + 22, body); B(lm - 31, lm - 30, 70, 104, dm - 14, dm + 14, flap); B(lm + 30, lm + 31, 70, 104, dm - 14, dm + 14, flap)
    return m
def b9_solar(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; cell, frame_, leg = c((30, 50, 100)), c((200, 204, 210)), c((120, 124, 130))
    for x in (X0 + 6, X1 - 10):
        box(m, x, x + 4, 0, 30, Z1 - 12, Z1 - 8, leg); box(m, x, x + 4, 0, 60, Z0 + 8, Z0 + 12, leg)
    n = 6
    for k in range(n):                                                                       # tilted panel in steps
        z0 = Z0 + 4 + k * (Z1 - Z0 - 8) / n; y = 60 - k * 30 / n
        box(m, X0 + 2, X1 - 2, y, y + 3, z0, z0 + (Z1 - Z0 - 8) / n, frame_); box(m, X0 + 4, X1 - 4, y + 3, y + 3.4, z0 + 1, z0 + (Z1 - Z0 - 8) / n - 1, cell)
    return m
def b9_shed(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wood, woodd, roof, door = c((150, 110, 70)), c((110, 80, 50)), c((80, 60, 50)), c((120, 86, 54))
    box(m, X0 + 2, X1 - 2, 0, 140, Z0 + 2, Z1 - 2, wood)
    for k in range(10): box(m, X0 + 2, X1 - 2, 6 + k * 13, 7 + k * 13, Z1 - 2, Z1 - 1.6, woodd)
    box(m, cx - 18, cx + 18, 0, 110, Z1 - 2, Z1 - 1, door); box(m, cx + 10, cx + 13, 50, 56, Z1 - 1, Z1 + 0.6, c((60, 60, 64)))
    n = 6
    for k in range(n): z0 = Z0 + k * (Z1 - Z0) / n; y = 140 + (n / 2 - abs(k + 0.5 - n / 2)) * 10; box(m, X0, X1, 140, y + 4, z0, z0 + (Z1 - Z0) / n, roof)
    return m
_b9 = {'cctv': b9_cctv, 'd_camerapillar': b9_camera_pillar, 'd_firealarm': b9_fire_alarm, 'd_healthmonitor': b9_health_monitor, 'd_trackingmonitor': b9_desk_monitor,
       'd_router': b9_router, 'd_scannermachine': b9_scanner, 'd_solarpanels': b9_solar, 'd_practicesolarpanel': b9_solar, 'd_shed': b9_shed}
for _k, _fn in _b9.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch9'))
        except Exception as _e: print('batch9 failed', _k, _e)
# ---------------------------------------------------------------- batch 10: seating, bathroom, wall decor and garden pieces
def _anchor(m, col): m.quad([(0, 0.0, 0), (0, 0.0, 0), (0, 0.0, 0), (0, 0.0, 0)], (0, 1, 0), [col] * 4)      # keeps wall-mounted things at their height
def b10_armchair(w, h, img, col=(110, 62, 40)):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; up, upd, foot = c(col), c(tuple(int(v * 0.75) for v in col)), c((40, 30, 24))
    r = min(X1 - X0, Z1 - Z0) / 2 * 0.85
    for sx in (-1, 1):
        for sz in (-1, 1): box(m, cx + sx * (r - 3) - 2, cx + sx * (r - 3) + 2, 0, 6, cz + sz * (r - 3) - 2, cz + sz * (r - 3) + 2, foot)
    box(m, cx - r, cx + r, 6, 24, cz - r, cz + r, upd); box(m, cx - r + 7, cx + r - 7, 24, 34, cz - r + 9, cz + r - 1, up)
    box(m, cx - r, cx + r, 24, 72, cz - r, cz - r + 9, up); box(m, cx - r + 1, cx + r - 1, 72, 76, cz - r, cz - r + 9, upd)
    for sx in (-1, 1): box(m, cx + sx * r - (7 if sx > 0 else 0), cx + sx * r + (0 if sx > 0 else 7), 24, 48, cz - r, cz + r, up); box(m, cx + sx * r - (8 if sx > 0 else -1), cx + sx * r + (1 if sx > 0 else 8), 48, 51, cz - r, cz + r, upd)
    return m
def b10_deckchair(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; frame_, cloth, cl2 = c((150, 110, 70)), c((220, 70, 60)), c((240, 236, 226))
    hw = min(X1 - X0, Z1 - Z0) / 2 * 0.7
    for sx in (-1, 1):
        box(m, cx + sx * hw - 1.5, cx + sx * hw + 1.5, 0, 22, cz + 16, cz + 19, frame_)            # front legs
        for k in range(5): z = cz + 14 - k * 7; box(m, cx + sx * hw - 1.5, cx + sx * hw + 1.5, 10 + k * 13, 14 + k * 13, z - 4, z, frame_)   # raking back frame
    pts = []
    for k in range(11):                                                                          # sling: one continuous strip, dipping at the seat and rising up the back
        f = k / 10.0; pts.append((cz + 16 - f * 34, 20 - 6 * math.sin(min(f, 0.5) * math.pi * 1.6) + max(0, f - 0.45) * 100))
    for k in range(10):
        (z0, y0), (z1, y1) = pts[k], pts[k + 1]; col = cloth if (k // 2) % 2 == 0 else cl2
        m.quad([(cx - hw + 1.5, y0, z0), (cx + hw - 1.5, y0, z0), (cx + hw - 1.5, y1, z1), (cx - hw + 1.5, y1, z1)], (0, 1, 0), [col] * 4)
    return m
def b10_bathroom_sink(w, h, img):
    m = Mesh(); c = swatch; por, tap, mir, frame_ = c((236, 238, 238)), c((180, 186, 192)), c((180, 210, 225)), c((150, 156, 164))
    lathe(m, [(6, 0), (5, 40), (4, 60)], 10, por, cz=-30); box(m, -16, 16, 60, 72, -42, -18, por); box(m, -13, 13, 70, 72.4, -39, -21, c((150, 170, 180)))
    box(m, -1, 1, 72, 80, -40, -38, tap); box(m, -1, 1, 78, 80, -40, -32, tap)
    return m
def b10_sink_mirror(w, h, img):
    m = Mesh(); c = swatch; mir, frame_ = c((180, 210, 225)), c((150, 156, 164))
    box(m, -14, 14, 100, 140, -46, -44, frame_); box(m, -12, 12, 102, 138, -44, -43.6, mir); box(m, -10, -2, 120, 134, -43.6, -43.4, c((220, 236, 244)))
    _anchor(m, frame_); return m
def b10_shower_pillar(w, h, img):
    m = Mesh(); c = swatch; tile, steel = c((226, 230, 232)), c((180, 186, 192))
    box(m, -10, 10, 0, 180, -10, 10, tile)
    for k in range(4):
        a = math.radians(90 * k); dx, dz = math.cos(a), math.sin(a)
        box(m, dx * 10 - 1 + min(0, dx * 8), dx * 10 + 1 + max(0, dx * 8), 150, 152, dz * 10 - 1 + min(0, dz * 8), dz * 10 + 1 + max(0, dz * 8), steel)
        lathe(m, [(0, 144), (4, 146), (2, 150), (0, 150)], 8, steel, cx=dx * 19, cz=dz * 19)
        box(m, dx * 10.4 - 1.5, dx * 10.4 + 1.5, 90, 96, dz * 10.4 - 1.5, dz * 10.4 + 1.5, steel)
    return m
def b10_wall_light(w, h, img):
    m = Mesh(); c = swatch; brass, glow = c((180, 150, 80)), c((255, 236, 170))
    box(m, -5, 5, 128, 140, -46, -44, brass); box(m, -1.5, 1.5, 132, 135, -44, -36, brass); lathe(m, [(2, 128), (6, 132), (7, 142), (3, 144)], 10, glow, cz=-36)
    _anchor(m, brass); return m
def b10_painting(w, h, img, art):
    m = Mesh(); c = swatch; frame_ = c((150, 110, 50))
    box(m, -24, 24, 96, 140, -46, -44, frame_)
    for k, col in enumerate(art):
        box(m, -21 + k * 42 / len(art), -21 + (k + 1) * 42 / len(art), 99, 137, -44, -43.6, c(col))
    box(m, -21, 21, 99, 112, -43.6, -43.4, c((90, 120, 70)))
    _anchor(m, frame_); return m
def b10_moose(w, h, img):
    m = Mesh(); c = swatch; plaque, fur, ant = c((120, 80, 44)), (120, 84, 54), c((214, 196, 160))
    box(m, -12, 12, 110, 140, -46, -43, plaque); _ball(m, 0, 124, -36, 8, 9, 9, fur, 10, 6); _ball(m, 0, 116, -28, 5, 5, 8, fur, 8, 5)
    for s in (-1, 1): box(m, s * 6 - 1, s * 6 + 1, 132, 140, -38, -36, ant); box(m, min(s * 6, s * 18) - 1, max(s * 6, s * 18) + 1, 140, 143, -38, -36, ant); box(m, s * 14 - 1, s * 14 + 1, 143, 150, -38, -36, ant); box(m, s * 18 - 1, s * 18 + 1, 143, 152, -38, -36, ant)
    _anchor(m, plaque); return m
def b10_coffin(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wood, woodd, brass = c((100, 60, 34)), c((70, 40, 22)), c((200, 170, 80))
    box(m, cx - 14, cx + 14, 0, 26, Z0 + 4, Z1 - 4, wood); box(m, cx - 15, cx + 15, 26, 30, Z0 + 3, Z1 - 3, woodd)
    for z in (Z0 + 20, Z1 - 20): box(m, cx - 16, cx + 16, 12, 15, z - 3, z + 3, brass)
    return m
def b10_fire_pit(w, h, img):
    m = Mesh(); c = swatch; stone, log, ember, flame = (130, 126, 120), c((90, 60, 36)), c((200, 70, 30)), c((250, 170, 60))
    for k in range(10): a = 2 * math.pi * k / 10; _ball(m, math.cos(a) * 22, 4, math.sin(a) * 22, 6, 5, 6, stone, 8, 4)
    for k in range(3): a = math.radians(60 * k); box(m, -14, 14, 2 + k * 3, 6 + k * 3, -2, 2, log) if k == 0 else box(m, -2, 2, 2 + k * 3, 6 + k * 3, -14, 14, log)
    lathe(m, [(0, 2), (10, 2), (8, 4), (0, 6)], 10, ember); lathe(m, [(0, 6), (6, 6), (3, 16), (0, 24)], 8, flame)
    return m
def b10_fountain(w, h, img):
    m = Mesh(); c = swatch; stone, water = c((190, 186, 176)), c((120, 180, 230))
    lathe(m, [(0, 0), (40, 0), (42, 14), (38, 16), (0, 10)], 18, stone); lathe(m, [(0, 12), (38, 12)], 18, water)
    lathe(m, [(5, 10), (4, 50)], 10, stone); lathe(m, [(0, 46), (16, 50), (18, 56), (14, 56), (0, 52)], 14, stone); lathe(m, [(0, 54), (14, 54)], 12, water)
    lathe(m, [(2, 56), (2, 72), (0, 76)], 8, c((150, 200, 240)))
    return m
def b10_lantern(w, h, img, paper):
    m = Mesh(); c = swatch
    if paper: lathe(m, [(0, 100), (8, 104), (11, 114), (8, 124), (0, 128)], 12, c((220, 50, 40))); lathe(m, [(0.5, 128), (0.5, 160)], 4, c((40, 40, 40)))
    else:
        box(m, -1.5, 1.5, 0, 120, -1.5, 1.5, c((40, 40, 44))); box(m, -7, 7, 120, 136, -7, 7, c((255, 220, 140))); box(m, -9, 9, 136, 140, -9, 9, c((40, 40, 44)))
    _anchor(m, c((40, 40, 44))); return m
_b10 = {'leatherchair': b10_armchair, 'd_leatherchair': lambda w, h, i: b10_armchair(w, h, i, (60, 40, 32)), 'd_officeleatherchair': lambda w, h, i: b6_swivel_chair(w, h, i, (60, 40, 32)),
        'd_deckchair': b10_deckchair, 'd_bathroomsink': b10_bathroom_sink, 'd_sinkmirror': b10_sink_mirror, 'd_showerpillar': b10_shower_pillar, 'd_walllight': b10_wall_light,
        'd_paintingfull1': lambda w, h, i: b10_painting(w, h, i, [(110, 150, 200), (200, 180, 120)]), 'd_paintingfull2': lambda w, h, i: b10_painting(w, h, i, [(200, 120, 80), (90, 60, 120), (220, 200, 160)]),
        'd_moosehead': b10_moose, 'd_coffinbed': b10_coffin, 'd_campbed': b7_cot, 'd_firepit': b10_fire_pit, 'd_fountain': b10_fountain,
        'd_paperlantern': lambda w, h, i: b10_lantern(w, h, i, True), 'd_ornatelantern': lambda w, h, i: b10_lantern(w, h, i, False)}
for _k, _fn in _b10.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch10'))
        except Exception as _e: print('batch10 failed', _k, _e)
# ---------------------------------------------------------------- batch 11: counters, booths, vehicles, wall panels and small props
def b11_counter(w, h, img, top=(200, 196, 186), body=(130, 92, 56), glass=False):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; tp, bd, gl = c(top), c(body), c((170, 210, 225))
    box(m, X0 + 1, X1 - 1, 0, 70, Z0 + 3, Z1 - 3, bd); box(m, X0, X1, 70, 74, Z0 + 2, Z1 - 1, tp)
    for k in range(3): x = X0 + 6 + k * (X1 - X0 - 12) / 3; box(m, x + 2, x + (X1 - X0 - 12) / 3 - 2, 8, 62, Z1 - 3, Z1 - 2, c(tuple(int(v * 0.85) for v in body)))
    if glass:
        box(m, X0 + 2, X1 - 2, 74, 104, Z1 - 6, Z1 - 5, gl); box(m, X0 + 2, X1 - 2, 104, 106, Z0 + 4, Z1 - 5, gl)
        for k in range(6): x = X0 + 6 + k * (X1 - X0 - 12) / 6; box(m, x, x + 8, 74, 80, cz - 6, cz + 4, c([(200, 120, 60), (230, 200, 120), (180, 60, 60)][k % 3]))
    return m
def b11_booth(w, h, img, col=(150, 50, 50)):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; up, wood, tb = c(col), c((110, 74, 44)), c((200, 196, 186))
    along = (X1 - X0) >= (Z1 - Z0)
    for side in (-1, 1):
        if along:
            x0, x1 = (X0, X0 + 18) if side < 0 else (X1 - 18, X1); box(m, x0, x1, 0, 40, Z0 + 2, Z1 - 2, up); xb = X0 if side < 0 else X1 - 6; box(m, xb, xb + 6, 0, 100, Z0 + 2, Z1 - 2, up)
        else:
            z0, z1 = (Z0, Z0 + 18) if side < 0 else (Z1 - 18, Z1); box(m, X0 + 2, X1 - 2, 0, 40, z0, z1, up); zb = Z0 if side < 0 else Z1 - 6; box(m, X0 + 2, X1 - 2, 0, 100, zb, zb + 6, up)
    box(m, cx - 4, cx + 4, 0, 66, cz - 4, cz + 4, wood); box(m, X0 + 22 if along else X0 + 6, X1 - 22 if along else X1 - 6, 66, 70, Z0 + 6 if along else Z0 + 22, Z1 - 6 if along else Z1 - 22, tb)
    return m
def b11_video_booth(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wall, scr, desk, seat = c((200, 204, 210)), c((40, 70, 110)), c((150, 154, 160)), c((50, 80, 140))
    box(m, X0 + 2, X0 + 5, 0, 150, Z0 + 2, Z1 - 2, wall); box(m, X1 - 5, X1 - 2, 0, 150, Z0 + 2, Z1 - 2, wall); box(m, X0 + 2, X1 - 2, 0, 150, Z0 + 2, Z0 + 5, wall)
    box(m, X0 + 5, X1 - 5, 66, 70, Z0 + 5, Z0 + 24, desk); box(m, cx - 16, cx + 16, 86, 112, Z0 + 5, Z0 + 7, scr); lathe(m, [(0, 40), (10, 40), (10, 44), (0, 44)], 12, seat, cx=cx, cz=Z1 - 16); lathe(m, [(2, 0), (2, 40)], 8, desk, cx=cx, cz=Z1 - 16)
    return m
def b11_slab(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; steel, sheet = c((180, 186, 192)), c((236, 238, 240))
    box(m, X0 + 2, X1 - 2, 70, 76, Z0 + 2, Z1 - 2, steel); _legs4(m, X0 + 2, X1 - 2, Z0 + 2, Z1 - 2, 70, 4, steel, 3); box(m, X0 + 4, X1 - 4, 76, 77, Z0 + 4, Z1 - 4, sheet)
    return m
def b11_box_prop(w, h, img, col, size=(20, 14, 16), band=None):
    m = Mesh(); c = swatch; sx, sy, sz = size; box(m, -sx / 2, sx / 2, 0, sy, -sz / 2, sz / 2, c(col))
    if band: box(m, -sx / 2 - 0.2, sx / 2 + 0.2, 0, sy + 0.2, -2, 2, c(band)); box(m, -2, 2, 0, sy + 0.2, -sz / 2 - 0.2, sz / 2 + 0.2, c(band))
    return m
def b11_van(w, h, img, col):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; body, glass, tyre, light = c(col), c((40, 60, 80)), c((28, 28, 30)), c((250, 230, 150))
    along_z = (Z1 - Z0) >= (X1 - X0)
    def B(a0, a1, y0, y1, b0, b1, cl):
        if along_z: box(m, a0, a1, y0, y1, b0, b1, cl)
        else: box(m, b0, b1, y0, y1, a0, a1, cl)
    A0, A1 = (X0, X1) if along_z else (Z0, Z1); L0, L1 = (Z0, Z1) if along_z else (X0, X1); ac = (A0 + A1) / 2; hw = min(45.0, (A1 - A0) / 2)
    B(ac - hw, ac + hw, 14, 110, L0 + 30, L1 - 2, body); B(ac - hw, ac + hw, 14, 80, L0 + 2, L0 + 30, body); B(ac - hw + 4, ac + hw - 4, 80, 106, L0 + 26, L0 + 32, glass)
    B(ac - hw + 6, ac + hw - 6, 60, 74, L0 + 1, L0 + 2, glass); B(ac - hw + 6, ac - hw + 14, 34, 40, L0 + 1, L0 + 2, light); B(ac + hw - 14, ac + hw - 6, 34, 40, L0 + 1, L0 + 2, light)
    for l in (L0 + 24, L1 - 26):
        for s in (ac - hw - 2, ac + hw - 2): B(s, s + 4, 0, 28, l - 13, l + 13, tyre)
    return m
def b11_wall_panel(w, h, img, col, corner=False):
    m = Mesh(); c = swatch; p, pd = c(col), c(tuple(int(v * 0.8) for v in col))
    box(m, -50, 50, 0, 196, -46, -42, p)
    for k in range(4): box(m, -48, 48, 20 + k * 44, 22 + k * 44, -42, -41.6, pd)
    if corner: box(m, -50, -46, 0, 196, -46, 50, p)
    return m
def b11_bollard(w, h, img):
    m = Mesh(); c = swatch; lathe(m, [(0, 0), (8, 0), (8, 30), (10, 34), (6, 38), (0, 38)], 12, c((50, 52, 56))); return m
def b11_meter(w, h, img):
    m = Mesh(); c = swatch; body, dial, glass = c((200, 200, 196)), c((40, 40, 44)), c((200, 220, 230))
    box(m, -12, 12, 90, 130, -46, -38, body); box(m, -9, 9, 108, 126, -38, -37.6, glass); box(m, -6, 6, 96, 102, -38, -37.6, dial); _anchor(m, body); return m
_b11 = {'d_displaycounter': lambda w, h, i: b11_counter(w, h, i, glass=True), 'd_shopcounter': b11_counter, 'd_smallservingtable': lambda w, h, i: b11_counter(w, h, i, (196, 200, 206), (160, 164, 170)),
        'd_jungleservingtable': lambda w, h, i: b11_counter(w, h, i, (150, 110, 60), (110, 80, 44)), 'd_diningbooth': b11_booth, 'd_videocallbooth': b11_video_booth, 'morgueslab': b11_slab,
        'd_medicalbox': lambda w, h, i: b11_box_prop(w, h, i, (236, 236, 232), (22, 14, 14), (200, 40, 40)), 'd_birthdaypresentclosed': lambda w, h, i: b11_box_prop(w, h, i, (60, 110, 200), (18, 16, 18), (230, 200, 60)),
        'd_softpillow': lambda w, h, i: b11_box_prop(w, h, i, (240, 238, 232), (26, 6, 16)), 'd_flour': lambda w, h, i: b11_box_prop(w, h, i, (236, 230, 214), (18, 22, 12), (120, 160, 200)),
        'd_rattrap': lambda w, h, i: b11_box_prop(w, h, i, (150, 110, 70), (12, 2, 6), (160, 164, 170)), 'd_sonicdeterrent': lambda w, h, i: b11_box_prop(w, h, i, (60, 62, 68), (10, 16, 8)),
        'trooptruck': lambda w, h, i: b11_van(w, h, i, (80, 90, 60)), 'd_constructionvan': lambda w, h, i: b11_van(w, h, i, (226, 180, 40)), 'd_pestcontrolvan': lambda w, h, i: b11_van(w, h, i, (90, 150, 80)),
        'd_roadmaintenancetruck': lambda w, h, i: b11_van(w, h, i, (230, 130, 40)), 'd_mooringbollard': b11_bollard, 'd_powerexportmeter': b11_meter,
        'd_wallpanelred': lambda w, h, i: b11_wall_panel(w, h, i, (150, 50, 50)), 'd_wallpanelred_cornera': lambda w, h, i: b11_wall_panel(w, h, i, (150, 50, 50), True), 'd_wallpanelred_cornerb': lambda w, h, i: b11_wall_panel(w, h, i, (150, 50, 50), True),
        'd_wallpanelstone': lambda w, h, i: b11_wall_panel(w, h, i, (160, 156, 148)), 'd_wallpanelstone_cornera': lambda w, h, i: b11_wall_panel(w, h, i, (160, 156, 148), True), 'd_wallpanelstone_cornerb': lambda w, h, i: b11_wall_panel(w, h, i, (160, 156, 148), True),
        'd_wallpanelwood': lambda w, h, i: b11_wall_panel(w, h, i, (140, 96, 58)), 'd_wallpanelwood_cornera': lambda w, h, i: b11_wall_panel(w, h, i, (140, 96, 58), True), 'd_wallpanelwood_cornerb': lambda w, h, i: b11_wall_panel(w, h, i, (140, 96, 58), True)}
for _k, _fn in _b11.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch11'))
        except Exception as _e: print('batch11 failed', _k, _e)
# ---------------------------------------------------------------- batch 12: gates, towers, outdoor and loose materials
def b12_fence_gate(w, h, img, band):
    """a chain-link gate leaf in the game's gate frame (x -50..50, 128 tall): steel frame, diagonal wire, a coloured band for the gate's type"""
    m = Mesh(); c = swatch; steel, wire, bd = c((150, 156, 164)), c((120, 126, 134)), c(band)
    for x0 in (-50, 46): box(m, x0, x0 + 4, 0, 128, -2, 2, steel)
    for y0 in (0, 62, 124): box(m, -50, 50, y0, y0 + 4, -2, 2, steel)
    box(m, -46, 46, 112, 118, -2.4, 2.4, bd)
    for k in range(1, 10): x = -46 + k * 9.2; box(m, x - 0.4, x + 0.4, 4, 112, -0.4, 0.4, wire)      # an open square wire mesh
    for k in range(1, 12): y = 4 + k * 9; box(m, -46, 46, y - 0.4, y + 0.4, -0.4, 0.4, wire)
    return m
def b12_bar_gate(w, h, img, col, pickets=False):
    m = Mesh(); c = swatch; cl = c(col)
    if pickets:
        for k in range(9): x = -48 + k * 11.5; box(m, x, x + 7, 6, 100, -1.5, 1.5, cl); lathe(m, [(0, 100), (3.5, 100), (0, 106)], 4, cl, cx=x + 3.5, cz=0)
        for y in (24, 76): box(m, -50, 50, y, y + 5, -3, -1.5, cl)
    else:
        for x0 in (-50, 46): box(m, x0, x0 + 4, 0, 128, -2, 2, cl)
        for y0 in (0, 124): box(m, -50, 50, y0, y0 + 4, -2, 2, cl)
        for k in range(1, 12): x = -50 + k * 100 / 12; box(m, x - 1, x + 1, 4, 124, -1, 1, cl)
    return m
def b12_tower(w, h, img, bamboo=False):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    leg, floor_, wall, roof = (c((140, 120, 70)), c((160, 140, 90)), c((180, 160, 100)), c((120, 100, 50))) if bamboo else (c((120, 124, 130)), c((150, 154, 160)), c((200, 196, 186)), c((50, 110, 130)))
    hw = min(X1 - X0, Z1 - Z0) / 2 - 4
    for sx in (-1, 1):
        for sz in (-1, 1): box(m, cx + sx * hw - 3, cx + sx * hw + 3, 0, 220, cz + sz * hw - 3, cz + sz * hw + 3, leg)
    box(m, cx - hw - 4, cx + hw + 4, 150, 156, cz - hw - 4, cz + hw + 4, floor_)
    for sx in (-1, 1): box(m, cx + sx * hw - 2, cx + sx * hw + 2, 156, 186, cz - hw, cz + hw, wall)
    box(m, cx - hw, cx + hw, 156, 186, cz - hw - 2, cz - hw + 2, wall)
    for sx in (-1, 1):
        for sz in (-1, 1): box(m, cx + sx * hw - 2, cx + sx * hw + 2, 186, 214, cz + sz * hw - 2, cz + sz * hw + 2, leg)
    lathe(m, [(0, 238), (hw * 1.25, 216), (hw * 1.25, 212), (0, 212)], 4, roof, cx=cx, cz=cz)
    return m
def b12_fight_ring(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; base, mat, post, rope = c((40, 60, 120)), c((230, 230, 226)), c((40, 40, 44)), c((200, 40, 40))
    box(m, X0 + 2, X1 - 2, 0, 24, Z0 + 2, Z1 - 2, base); box(m, X0 + 4, X1 - 4, 24, 26, Z0 + 4, Z1 - 4, mat)
    for x in (X0 + 4, X1 - 8):
        for z in (Z0 + 4, Z1 - 8): box(m, x, x + 4, 24, 80, z, z + 4, post)
    for y in (42, 56, 70):
        box(m, X0 + 6, X1 - 6, y, y + 1.5, Z0 + 5, Z0 + 6.5, rope); box(m, X0 + 6, X1 - 6, y, y + 1.5, Z1 - 6.5, Z1 - 5, rope); box(m, X0 + 5, X0 + 6.5, y, y + 1.5, Z0 + 6, Z1 - 6, rope); box(m, X1 - 6.5, X1 - 5, y, y + 1.5, Z0 + 6, Z1 - 6, rope)
    return m
def b12_helipad(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; pad, line = c((70, 72, 76)), c((236, 236, 232))
    box(m, X0, X1, 0, 2, Z0, Z1, pad); lathe(m, [(min(X1 - X0, Z1 - Z0) * 0.38, 2), (min(X1 - X0, Z1 - Z0) * 0.42, 2)], 24, line, cx=cx, cz=cz)
    box(m, cx - 14, cx - 8, 2, 2.4, cz - 22, cz + 22, line); box(m, cx + 8, cx + 14, 2, 2.4, cz - 22, cz + 22, line); box(m, cx - 8, cx + 8, 2, 2.4, cz - 3, cz + 3, line)
    return m
def b12_tyres(w, h, img):
    m = Mesh(); c = swatch; rub, rim = c((34, 34, 38)), c((80, 82, 88))
    for k in range(4): lathe(m, [(9, k * 10), (16, k * 10), (17, k * 10 + 5), (16, k * 10 + 10), (9, k * 10 + 10)], 12, rub, cx=(k % 2) * 1.5)
    lathe(m, [(0.01, 1), (9.2, 1)], 12, c((22, 22, 24)))      # dark floor of the stack's hole: no speckle seen through it
    return m
def b12_stones(w, h, img, col=(130, 126, 120), n=3):
    m = Mesh(); import random as _r; rr = _r.Random(n * 3)
    for k in range(n): _ball(m, rr.uniform(-14, 14), 4, rr.uniform(-14, 14), rr.uniform(7, 11), rr.uniform(4, 7), rr.uniform(7, 11), col, 8, 4)
    return m
def b12_logs(w, h, img, col=(90, 60, 36)):
    m = Mesh(); c = swatch; bark, end = c(col), c((190, 150, 100))
    for k, (x, y) in enumerate(((-9, 0), (9, 0), (0, 15))): zcyl(m, x, y + 8, -24, 24, 8, bark, 10); zcyl(m, x, y + 8, 24, 24.4, 7, end, 10, False)
    return m
def b12_sheets(w, h, img):
    m = Mesh(); c = swatch; s1, s2 = c((170, 176, 184)), c((140, 146, 154))
    for k in range(5): box(m, -24 + k * 0.6, 24 + k * 0.6, k * 2, k * 2 + 1.6, -16, 16, s1 if k % 2 else s2)
    return m
def b12_footbridge(w, h, img, steel=False):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; deck, rail = (c((150, 154, 160)), c((100, 104, 110))) if steel else (c((150, 110, 70)), c((110, 80, 50)))
    box(m, X0, X1, 10, 14, Z0 + 4, Z1 - 4, deck)
    for z in (Z0 + 4, Z1 - 8):
        box(m, X0, X1, 50, 54, z, z + 4, rail)
        for k in range(5): x = X0 + 2 + k * (X1 - X0 - 6) / 4; box(m, x, x + 4, 14, 50, z, z + 4, rail)
    return m
def b12_cake(w, h, img, raw=False):
    m = Mesh(); c = swatch; sponge, icing, cherry = c((236, 200, 140) if raw else (200, 140, 80)), c((250, 236, 230)), c((200, 30, 40))
    lathe(m, [(0, 0), (14, 0), (14, 10), (0, 10)], 14, sponge)
    if not raw: lathe(m, [(0, 10), (14.4, 10), (14.4, 13), (0, 13)], 14, icing); _ball(m, 0, 15, 0, 2.4, 2.4, 2.4, (200, 30, 40), 8, 4)
    return m
_b12 = {'d_fencegatestandard': lambda w, h, i: b12_fence_gate(w, h, i, (150, 156, 164)), 'd_fencegatestaff': lambda w, h, i: b12_fence_gate(w, h, i, (60, 150, 70)),
        'd_fencegatevisitor': lambda w, h, i: b12_fence_gate(w, h, i, (170, 40, 60)), 'd_fencegateguard': lambda w, h, i: b12_fence_gate(w, h, i, (50, 80, 150)),
        'd_gate': lambda w, h, i: b12_bar_gate(w, h, i, (90, 94, 100)), 'd_cagegate': lambda w, h, i: b12_bar_gate(w, h, i, (130, 134, 140)), 'd_whitepicketgate': lambda w, h, i: b12_bar_gate(w, h, i, (236, 236, 230), True),
        'd_guardpavilion': b12_tower, 'd_bambooguardtower': lambda w, h, i: b12_tower(w, h, i, True), 'd_fightring': b12_fight_ring, 'd_helipad': b12_helipad, 'd_tyrestack': b12_tyres,
        'd_pushupstones': lambda w, h, i: b12_stones(w, h, i, (130, 126, 120), 2), 'd_steppingstoneiceblock': lambda w, h, i: b12_stones(w, h, i, (200, 230, 245), 1), 'd_steppingstonepallet': lambda w, h, i: b11_box_prop(w, h, i, (170, 130, 80), (40, 6, 40)),
        'd_ebonywood': lambda w, h, i: b12_logs(w, h, i, (40, 30, 26)), 'd_rawebonylog': lambda w, h, i: b12_logs(w, h, i, (60, 44, 34)), 'wood': b12_logs, 'sheetmetal': b12_sheets,
        'd_footbridge': b12_footbridge, 'd_footbridgesteel': lambda w, h, i: b12_footbridge(w, h, i, True), 'd_grassbridge1': b12_footbridge,
        'd_bakedbakeryproduct_00_cake01': b12_cake, 'd_rawbakeryproduct_00_cake01': lambda w, h, i: b12_cake(w, h, i, True)}
for _k, _fn in _b12.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch12'))
        except Exception as _e: print('batch12 failed', _k, _e)
# ---------------------------------------------------------------- batch 13: sprite variants the game draws under another name (from NOMODEL lines)
def b13_saloon(w, h, img, double=False):
    W = w * 25.0; m = Mesh(); c = swatch; wd_, wdd, brass = (150, 100, 56), (110, 70, 38), c((196, 168, 84))
    for sx in (-1, 1):
        x0, x1 = (2, W - 2) if sx > 0 else (-W + 2, -2)
        box(m, x0, x1, 46, 132, -2.5, 2.5, c(wd_))                                                       # swinging leaf, chest height, gap below and above
        box(m, x0, x1, 120, 132, -3.2, 3.2, c(wdd)); box(m, x0, x1, 46, 54, -3.2, 3.2, c(wdd))           # top and bottom rails
        for k in range(7): yy = 60 + k * 8.4; box(m, x0 + 3, x1 - 3, yy, yy + 4.5, -3.0, 3.0, c(shade(wd_, 0.85)))   # louvres
        hx = W - 2 if sx > 0 else -W + 2; box(m, hx - 1.5, hx + 1.5, 60, 66, -3.6, 3.6, brass); box(m, hx - 1.5, hx + 1.5, 112, 118, -3.6, 3.6, brass)   # hinges
    for sx in (-1, 1): box(m, sx * W - 4, sx * W + 4, 0, 160, -6, 6, c(wdd))                            # posts
    return m
_b13 = {'d_tyrestack1': b12_tyres, 'd_tyrestack2': b12_tyres, 'd_tyrestack3': b12_tyres,
        'd_painting1': lambda w, h, i: b10_painting(w, h, i, [(70, 110, 150), (200, 190, 150)]), 'd_painting2': lambda w, h, i: b10_painting(w, h, i, [(140, 100, 60), (60, 90, 60), (210, 180, 120)]),
        'd_smallbench': bench_small,
        'd_doublestaffdoorblue': lambda w, h, i: arch_door_double(w, h, i, (50, 100, 190)), 'd_doublestaffdoororange': lambda w, h, i: arch_door_double(w, h, i, (230, 140, 40)),
        'd_doublestaffdoorred': lambda w, h, i: arch_door_double(w, h, i, (190, 50, 44)),
        'd_saloondoor': b13_saloon, 'd_saloondoubledoor': b13_saloon}
for _k, _fn in _b13.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch13'))
        except Exception as _e: print('batch13 failed', _k, _e)
    else: print('batch13: no sprite', _k)
# ---------------------------------------------------------------- batch 14: letter / number wall signs (the glyph in raised pixels), bamboo and double visitor doors
def b14_sign_glyph(w, h, img, grid=14):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh()
    a, b = X0 + 3, X1 - 3; Y0, Y1 = 90, 128; bg = body_colour(img)
    _plate(m, a, b, Y0, Y1, Z0, 3, (60, 62, 68))
    _plate(m, a + 1, b - 1, Y0 + 1, Y1 - 1, Z0 + 2.2, 0.9, bg)      # the sign face in its own colour
    im = img.convert('RGBA'); W_, H_ = im.size; P = im.load()
    def glyph(x, y):
        r_, g_, b_, al = P[x, y]; return al >= 128 and abs(r_ - bg[0]) + abs(g_ - bg[1]) + abs(b_ - bg[2]) >= 90
    xs = [x for x in range(W_) for y in range(H_) if glyph(x, y)]; ys = [y for x in range(W_) for y in range(H_) if glyph(x, y)]
    if xs:
        # the sprite's dark outline also differs from the face: drop pixels touching the sprite's outer border rows / columns
        u0, v0, u1, v1 = solid_bbox(img); bx0, by0, bx1, by1 = int(u0 * W_) + 2, int(v0 * H_) + 2, int(u1 * W_) - 2, int(v1 * H_) - 2
        pts = [(x, y) for x in range(max(0, bx0), min(W_, bx1)) for y in range(max(0, by0), min(H_, by1)) if glyph(x, y)]
        if pts:
            gx0, gx1 = min(p[0] for p in pts), max(p[0] for p in pts) + 1; gy0, gy1 = min(p[1] for p in pts), max(p[1] for p in pts) + 1
            S = (Y1 - Y0) - 8; side = max(gx1 - gx0, gy1 - gy0); cell = S / side
            cx, cy = (a + b) / 2, (Y0 + Y1) / 2; ox, oy = cx - (gx1 - gx0) * cell / 2, cy + (gy1 - gy0) * cell / 2
            ink = {}
            for (x, y) in pts:
                r_, g_, b_, al = P[x, y]; ink.setdefault((r_ // 48, g_ // 48, b_ // 48), []).append((x, y))
            # keep only the main ink colours (anti-aliased edge shades join the nearest one) and merge pixels into rectangles:
            # one box per pixel made each sign 8-10k faces and added ~12 s to every model load
            main = [k for k in sorted(ink, key=lambda k: -len(ink[k])) if len(ink[k]) >= 0.15 * len(pts)][:2] or [max(ink, key=lambda k: len(ink[k]))]
            merged = {k: set() for k in main}
            for k, cells in ink.items():
                tgt = min(main, key=lambda q: sum((q[i] - k[i]) ** 2 for i in range(3))); merged[tgt].update(cells)
            for key, cells in merged.items():
                col = tuple(min(255, k * 48 + 24) for k in key); sw = swatch(col); left = set(cells)
                for y in range(gy0, gy1):      # greedy: take a row run, grow it down while the rows below hold the same run
                    x = gx0
                    while x < gx1:
                        if (x, y) not in left: x += 1; continue
                        x2 = x
                        while (x2 + 1, y) in left: x2 += 1
                        y2 = y
                        while all((xx, y2 + 1) in left for xx in range(x, x2 + 1)): y2 += 1
                        for yy in range(y, y2 + 1):
                            for xx in range(x, x2 + 1): left.discard((xx, yy))
                        box(m, ox + (x - gx0) * cell, ox + (x2 + 1 - gx0) * cell, oy - (y2 + 1 - gy0) * cell, oy - (y - gy0) * cell, Z0 + 3, Z0 + 4.4, sw)
                        x = x2 + 1
    _anchor(m, swatch((60, 62, 68))); return m
_b14 = {'d_sign' + k: b14_sign_glyph for k in ['number%d' % n for n in range(10)] + ['letter' + chr(65 + n).lower() for n in range(26)]}
_b14.update({'d_bamboogate': lambda w, h, i: arch_door_double(w, h, i, (176, 156, 92)), 'd_visitordoubledoor': lambda w, h, i: arch_door_double(w, h, i, (140, 40, 62))})
for _k, _fn in _b14.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch14'))
        except Exception as _e: print('batch14 failed', _k, _e)
    else: print('batch14: no sprite', _k)
# ---------------------------------------------------------------- batch 15: the last plain sprite slabs (vehicles, small items, props)
def _bc(img, dflt=(150, 150, 150)):
    try: return body_colour(img)
    except Exception: return dflt
def b15_heli(w, h, img, col=None):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; col = col or _bc(img); dk = c(shade(col, 0.6)); gl = c((120, 170, 200)); mt = c((70, 72, 78))
    L = (Z1 - Z0) * 0.9; R = min(X1 - X0, L) * 0.16
    _ball(m, cx, 60, cz + L * 0.12, R * 1.2, R * 1.1, L * 0.26, col, 14, 8)                                  # cabin
    _ball(m, cx, 66, cz + L * 0.3, R * 0.9, R * 0.7, L * 0.1, (120, 170, 200), 10, 6)                       # windscreen
    box(m, cx - R * 0.25, cx + R * 0.25, 58, 70, cz - L * 0.48, cz - L * 0.05, c(col))                         # tail boom
    box(m, cx - 2, cx + 2, 62, 84, cz - L * 0.5, cz - L * 0.44, dk)                                            # fin
    for s in (-1, 1): box(m, cx + s * R * 1.1 - 2, cx + s * R * 1.1 + 2, 30, 34, cz - L * 0.12, cz + L * 0.3, mt); box(m, cx + s * R * 0.7 - 1.5, cx + s * R * 0.7 + 1.5, 34, 50, cz, cz + 3, mt)   # skids
    lathe(m, [(0, 88), (4, 88), (4, 92), (0, 92)], 8, mt, cx=cx, cz=cz + L * 0.1)                              # hub
    for a in (0.4, 2.0): dx, dz = math.cos(a) * L * 0.5, math.sin(a) * L * 0.5; box(m, cx - abs(dx), cx + abs(dx), 90, 91.5, cz + L * 0.1 - 3, cz + L * 0.1 + 3, mt) if a < 1 else box(m, cx - 3, cx + 3, 90, 91.5, cz + L * 0.1 - abs(dz), cz + L * 0.1 + abs(dz), mt)   # rotor
    return m
def b15_boat(w, h, img, col=None):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; col = col or _bc(img, (220, 220, 220))
    W = (X1 - X0) * 0.42; L = (Z1 - Z0) * 0.46
    lathe(m, [(W * 0.7, 0), (W, 22), (W * 1.02, 27)], 12, c(shade(col, 0.8)), cx=cx, cz=cz, sx=1.0, sz=L / W)  # hull
    lathe(m, [(W * 1.0, 27.5), (0, 27.5)], 12, c((170, 130, 90)), cx=cx, cz=cz, sx=1.0, sz=L / W)               # deck, just above the hull rim
    box(m, cx - W * 0.5, cx + W * 0.5, 30, 62, cz - L * 0.45, cz + L * 0.1, c(col))                           # cabin
    box(m, cx - W * 0.52, cx + W * 0.52, 48, 56, cz + L * 0.1, cz + L * 0.12, c((120, 170, 200)))             # cabin windows
    hs = max(1.0, max(L, W) / 110.0); m.v = [(x, y * hs, z) for (x, y, z) in m.v]      # big vessels stand taller, not like rafts
    return m
def b15_trike(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; col = _bc(img, (120, 40, 40)); tyre = (30, 30, 34)
    L = min(40.0, max(X1 - X0, Z1 - Z0) * 0.45)
    for (x, z) in ((cx - 18, cz - L * 0.7), (cx + 18, cz - L * 0.7), (cx, cz + L * 0.8)): _ball(m, x, 12, z, 5, 12, 12, tyre, 10, 6)
    box(m, cx - 14, cx + 14, 18, 34, cz - L * 0.8, cz - L * 0.1, c(col)); box(m, cx - 4, cx + 4, 20, 30, cz - L * 0.1, cz + L * 0.75, c(col))
    box(m, cx - 9, cx + 9, 34, 40, cz - L * 0.6, cz - L * 0.1, c((30, 30, 30))); box(m, cx - 12, cx + 12, 42, 45, cz + L * 0.55, cz + L * 0.6, c((140, 140, 150)))
    return m
def b15_sack(w, h, img, col=(150, 120, 80), hgt=26, r=13):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh()
    _ball(m, cx, hgt * 0.45, cz, r, hgt * 0.5, r * 0.85, col, 12, 7); _ball(m, cx, hgt * 0.95, cz, r * 0.35, hgt * 0.15, r * 0.3, shade(col, 0.85), 8, 4); return m
def b15_letters(w, h, img, n=3, col=(236, 232, 220), opened=False):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    for k in range(n):
        jx, jz = ((k * 7) % 5) - 2, ((k * 3) % 5) - 2; y = k * 1.2
        box(m, cx - 9 + jx, cx + 9 + jx, y, y + 1.0, cz - 6 + jz, cz + 6 + jz, c(col)); box(m, cx - 3 + jx, cx + 3 + jx, y + 1.0, y + 1.2, cz - 1 + jz, cz + 1 + jz, c((190, 60, 50)))
    if opened: box(m, cx - 8, cx + 8, n * 1.2, n * 1.2 + 0.6, cz + 5, cz + 11, c(shade(col, 0.95)))
    return m
def b15_plate(w, h, img, food=None):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    lathe(m, [(0, 0), (9, 0), (12, 2.5), (11, 2.8), (8, 1), (0, 1)], 14, c((236, 236, 232)), cx=cx, cz=cz)
    if food: _ball(m, cx, 2, cz, 6, 3, 6, food, 10, 5)
    return m
def b15_box(w, h, img, col, size=(16, 12, 12), lid=None):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; a, b_, d = size
    box(m, cx - a / 2, cx + a / 2, 0, b_, cz - d / 2, cz + d / 2, c(col))
    if lid: box(m, cx - a / 2 - 0.5, cx + a / 2 + 0.5, b_, b_ + 1.5, cz - d / 2 - 0.5, cz + d / 2 + 0.5, c(lid))
    return m
def b15_hammer(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    box(m, cx - 12, cx + 6, 0, 2.5, cz - 1.5, cz + 1.5, c((150, 104, 60))); box(m, cx + 5, cx + 9, 0, 3.5, cz - 5, cz + 5, c((90, 92, 98))); return m
def b15_easel(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wd = c((150, 104, 60)); art = _bc(img, (200, 180, 140))
    for s in (-1, 1): box(m, cx + s * 14 - 1.5, cx + s * 14 + 1.5, 0, 120, cz - 4, cz - 1, wd)
    box(m, cx - 1.5, cx + 1.5, 0, 110, cz - 18, cz - 15, wd); box(m, cx - 16, cx + 16, 52, 55, cz - 3, cz + 3, wd)
    box(m, cx - 18, cx + 18, 55, 100, cz, cz + 2, c((240, 236, 226))); box(m, cx - 14, cx + 14, 60, 95, cz + 2, cz + 2.4, c(art)); return m
def b15_projector(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    for s in (-1, 1):
        for t_ in (-1, 1): box(m, cx + s * 10 - 1, cx + s * 10 + 1, 0, 70, cz + t_ * 10 - 1, cz + t_ * 10 + 1, c((60, 60, 66)))
    box(m, cx - 14, cx + 14, 70, 90, cz - 16, cz + 16, c((50, 52, 58)))
    for zz in (-6, 6): zcyl(m, cx + zz * 1.4, 98, cz - 6, cz + 6, 8, c((40, 40, 44)), 12)
    zcyl(m, cx, 80, cz + 16, cz + 22, 4, c((160, 170, 180)), 10); return m
def b15_screen(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    box(m, X0, X1, 20, 180, Z0, Z0 + 3, c((236, 236, 240))); box(m, X0 - 3, X1 + 3, 176, 186, Z0 - 2, Z0 + 5, c((40, 40, 44)))
    for x in (X0 - 3, X1): box(m, x, x + 3, 0, 186, Z0 - 2, Z0 + 5, c((40, 40, 44)))
    return m
def b15_ironing(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; L = max(X1 - X0, 60) * 0.48
    lathe(m, [(0, 82), (11, 82), (11, 86), (0, 86)], 12, c((90, 130, 190)), cx=cx, cz=cz, sx=L / 11, sz=1.0)        # board, cloth cover
    lathe(m, [(0, 86), (10, 86), (10, 86.6), (0, 86.6)], 12, c((210, 220, 236)), cx=cx, cz=cz, sx=L / 12, sz=0.9)
    for xx in (cx - L * 0.35, cx + L * 0.25):      # two A-frame leg pairs with floor feet
        for s in (-1, 1): box(m, xx - 1.2, xx + 1.2, 0, 82, cz + s * 6 - 1.2, cz + s * 6 + 1.2, c((150, 156, 164)))
        box(m, xx - 1.2, xx + 1.2, 0, 2, cz - 9, cz + 9, c((60, 62, 68)))
    box(m, cx + L * 0.4, cx + L * 0.75, 86.6, 90, cz - 4, cz + 4, c((200, 200, 205))); box(m, cx + L * 0.45, cx + L * 0.7, 90, 96, cz - 1, cz + 1, c((40, 40, 44)))   # iron
    return m
def b15_pumpkin(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    _ball(m, cx, 12, cz, 14, 12, 14, (226, 120, 30), 14, 8); box(m, cx - 1.5, cx + 1.5, 23, 29, cz - 1.5, cz + 1.5, c((80, 110, 50)))
    for s in (-1, 1): box(m, cx + s * 6 - 2, cx + s * 6 + 2, 14, 17, cz + 12.5, cz + 13.6, c((255, 220, 90)))
    box(m, cx - 6, cx + 6, 6, 9, cz + 12.8, cz + 13.8, c((255, 220, 90))); return m
def b15_gnome(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    lathe(m, [(0, 0), (7, 0), (8, 14), (6, 20), (0, 21)], 10, c((60, 100, 170)), cx=cx, cz=cz)
    _ball(m, cx, 25, cz, 5, 5, 5, (240, 200, 160), 10, 6); _ball(m, cx, 21, cz + 3, 4, 4, 2, (240, 240, 240), 8, 4)
    lathe(m, [(5.5, 28), (0, 40)], 10, c((200, 40, 40)), cx=cx, cz=cz); return m
def b15_statue(w, h, img, col=(170, 168, 160), hgt=150):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    box(m, cx - 22, cx + 22, 0, 30, cz - 22, cz + 22, c(shade(col, 0.85))); box(m, cx - 25, cx + 25, 28, 32, cz - 25, cz + 25, c(shade(col, 0.95)))
    lathe(m, [(0, 32), (10, 32), (12, 70), (13, 100), (9, 112), (0, 114)], 12, c(col), cx=cx, cz=cz)
    _ball(m, cx, 124, cz, 9, 11, 9, col, 12, 7); _ball(m, cx, 130, cz + 1, 10, 3, 10, shade(col, 0.9), 12, 3)
    for s in (-1, 1): box(m, cx + s * 12 - 3, cx + s * 12 + 3, 70, 104, cz - 3, cz + 3, c(col))
    return m
def b15_gravestone(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; st = (150, 150, 146)
    box(m, cx - 20, cx + 20, 0, 6, cz - 12, cz + 12, c(shade(st, 0.85))); box(m, cx - 14, cx + 14, 6, 50, cz - 5, cz + 5, c(st))
    zcyl(m, cx, 50, cz - 5, cz + 5, 14, c(st), 16); box(m, cx - 2, cx + 2, 60, 80, cz - 2, cz + 2, c(shade(st, 1.1))); box(m, cx - 8, cx + 8, 70, 74, cz - 2, cz + 2, c(shade(st, 1.1)))
    return m
def b15_pole(w, h, img, top='sock'):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; mt = c((150, 156, 164))
    box(m, cx - 2, cx + 2, 0, 200, cz - 2, cz + 2, mt); box(m, cx - 8, cx + 8, 0, 3, cz - 8, cz + 8, c((90, 92, 98)))
    if top == 'sock':      # striped sock, narrowing away from the pole
        for k in range(5): r = 7 - k; xx = cx + 3 + k * 9; box(m, xx, xx + 9, 190 - r, 190 + r, cz - r, cz + r, c((230, 110, 30) if k % 2 == 0 else (240, 240, 240)))
    else:
        box(m, cx - 14, cx + 14, 196, 198, cz - 1, cz + 1, mt); box(m, cx - 1, cx + 1, 196, 198, cz - 14, cz + 14, mt)
        for (dx, dz) in ((14, 0), (-14, 0), (0, 14)): lathe(m, [(0, 196), (3, 199), (0, 202)], 8, c((60, 60, 66)), cx=cx + dx, cz=cz + dz)
        box(m, cx - 6, cx + 6, 150, 166, cz - 6, cz + 6, c((236, 236, 236)))
    return m
def b15_lifering(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    box(m, cx - 2, cx + 2, 0, 90, cz - 2, cz + 2, c((150, 104, 60)))
    for k in range(12):
        a0, a1 = k * math.pi / 6, (k + 1) * math.pi / 6; col = (230, 60, 50) if k % 3 else (240, 240, 240)
        box(m, cx + math.cos(a0) * 14 - 3, cx + math.cos(a0) * 14 + 3, 70 + math.sin(a0) * 14 - 3, 70 + math.sin(a0) * 14 + 3, cz + 2, cz + 6, c(col))
    return m
def b15_buoy(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    lathe(m, [(0, 0), (16, 2), (18, 10), (12, 16), (6, 40), (0, 42)], 12, c((230, 70, 40)), cx=cx, cz=cz); lathe(m, [(6, 28), (7, 32), (6.2, 34)], 12, c((240, 240, 240)), cx=cx, cz=cz)
    _ball(m, cx, 46, cz, 3, 3, 3, (255, 230, 120), 8, 4); return m
def b15_kneeraise(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; mt, pad = c((60, 62, 68)), c((40, 40, 46))
    for s in (-1, 1): box(m, cx + s * 18 - 2, cx + s * 18 + 2, 0, 150, Z0 + 6, Z0 + 10, mt); box(m, cx + s * 18 - 3, cx + s * 18 + 3, 0, 3, Z0 + 4, Z1 - 4, mt); box(m, cx + s * 14 - 4, cx + s * 14 + 4, 110, 116, Z0 + 10, Z0 + 34, pad)
    box(m, cx - 12, cx + 12, 60, 110, Z0 + 4, Z0 + 10, pad); box(m, cx - 20, cx + 20, 146, 150, Z0 + 6, Z0 + 10, mt); return m
def b15_cake_stand(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    lathe(m, [(0, 0), (8, 0), (2, 3), (1.5, 12), (14, 13), (14, 14), (0, 14)], 14, c((236, 236, 240)), cx=cx, cz=cz)
    lathe(m, [(0, 14), (13, 14), (12, 22), (0, 23)], 14, c((200, 220, 230)), cx=cx, cz=cz); return m
def b15_dog(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; wh, bl = (240, 240, 236), (30, 30, 32)
    _ball(m, cx, 22, cz, 9, 9, 18, wh, 12, 7); _ball(m, cx, 34, cz + 18, 7, 7, 8, wh, 10, 6); _ball(m, cx, 32, cz + 25, 3, 3, 3, bl, 6, 4)
    for (dx, dz) in ((-5, -10), (5, -10), (-5, 10), (5, 10)): box(m, cx + dx - 2, cx + dx + 2, 0, 16, cz + dz - 2, cz + dz + 2, c(wh))
    for (dx, dz) in ((4, 2), (-5, -6), (3, -12)): _ball(m, cx + dx, 29, cz + dz, 3, 2, 3, bl, 6, 3)
    box(m, cx - 1, cx + 1, 24, 30, cz - 22, cz - 16, c(wh)); return m
def b15_spout(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; st = (150, 150, 146)
    lathe(m, [(0, 0), (30, 0), (30, 14), (26, 14), (26, 4), (0, 4)], 16, c(st), cx=cx, cz=cz); lathe(m, [(0, 4.5), (26, 4.5), (26, 10), (0, 10)], 16, c((80, 140, 180)), cx=cx, cz=cz)
    lathe(m, [(0, 10), (6, 10), (5, 60), (8, 66), (0, 70)], 10, c(st), cx=cx, cz=cz); return m
def b15_harvester(w, h, img, col=(200, 160, 40)):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    box(m, X0 + 6, X1 - 6, 14, 50, cz - (Z1 - Z0) * 0.3, cz + (Z1 - Z0) * 0.2, c(col)); box(m, X0 + 2, X1 - 2, 8, 20, cz + (Z1 - Z0) * 0.2, cz + (Z1 - Z0) * 0.42, c((120, 124, 130)))
    for s in (-1, 1):
        for zz in (-0.25, 0.1): zcyl(m, 0, 0, 0, 0, 0, c(col), 3, False) if False else _ball(m, cx + s * ((X1 - X0) / 2 - 8), 12, cz + (Z1 - Z0) * zz, 4, 12, 12, (30, 30, 34), 10, 6)
    box(m, cx - 10, cx + 10, 50, 70, cz - (Z1 - Z0) * 0.25, cz - (Z1 - Z0) * 0.05, c((110, 160, 200))); return m
def b15_rorschach(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    box(m, cx - 2, cx + 2, 0, 80, cz - 2, cz + 2, c((90, 92, 98))); box(m, cx - 14, cx + 14, 0, 2, cz - 10, cz + 10, c((90, 92, 98)))
    box(m, cx - 18, cx + 18, 80, 120, cz, cz + 2, c((240, 238, 232)))
    for (dx, dy, r) in ((-6, 100, 6), (6, 100, 6), (-9, 92, 4), (9, 92, 4), (0, 110, 4)): box(m, cx + dx - r, cx + dx + r, dy - r, dy + r, cz + 2, cz + 2.4, c((30, 30, 34)))
    return m
def b15_pa(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    box(m, cx - 10, cx + 10, 0, 40, cz - 8, cz + 8, c((70, 72, 78))); box(m, cx - 8, cx + 8, 40, 42, cz - 6, cz + 6, c((40, 40, 44)))
    lathe(m, [(0, 42), (1, 42), (1, 70), (0, 70)], 6, c((150, 156, 164)), cx=cx + 5, cz=cz); _ball(m, cx + 5, 72, cz, 2.5, 3, 2.5, (40, 40, 44), 8, 4)
    for k in range(3): box(m, cx - 7 + k * 5, cx - 4 + k * 5, 30, 33, cz + 8, cz + 9, c((60, 180, 80)))
    return m
def b15_tattoo(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch
    box(m, X0 + 4, X1 - 4, 0, 46, Z0 + 4, Z1 - 4, c((236, 236, 240))); box(m, X0 + 2, X1 - 2, 46, 52, Z0 + 2, Z1 - 2, c((80, 160, 200)))
    lathe(m, [(0, 52), (2, 52), (2, 110), (0, 110)], 6, c((150, 156, 164)), cx=X1 - 10, cz=Z0 + 10); box(m, X1 - 30, X1 - 8, 106, 112, Z0 + 8, Z0 + 12, c((150, 156, 164)))
    box(m, X1 - 34, X1 - 24, 98, 108, Z0 + 6, Z0 + 14, c((60, 62, 68))); return m
def b15_container(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; col = _bc(img, (60, 110, 160))
    box(m, X0 + 4, X1 - 4, 0, 40, Z0 + 4, Z1 - 4, c(col)); box(m, X0 + 3, X1 - 3, 40, 44, Z0 + 3, Z1 - 3, c(shade(col, 0.8)))
    for k in range(5): xx = X0 + 8 + k * (X1 - X0 - 16) / 4; box(m, xx - 1, xx + 1, 8, 36, Z1 - 4, Z1 - 3, c((200, 200, 205)))
    return m
def b15_anatomy(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; bone = (236, 230, 210)
    box(m, cx - 10, cx + 10, 0, 3, cz - 10, cz + 10, c((60, 62, 68))); box(m, cx - 1, cx + 1, 3, 90, cz - 6, cz - 4, c((150, 156, 164)))
    _ball(m, cx, 150, cz, 7, 8, 7, bone, 10, 6); lathe(m, [(1.5, 90), (1.5, 140)], 6, c(bone), cx=cx, cz=cz)
    for k in range(5): y = 118 + k * 5; box(m, cx - 10 + k, cx + 10 - k, y, y + 1.5, cz - 6, cz + 6, c(bone))
    for s in (-1, 1): box(m, cx + s * 4 - 1.5, cx + s * 4 + 1.5, 50, 92, cz - 1.5, cz + 1.5, c(bone)); box(m, cx + s * 12 - 1.2, cx + s * 12 + 1.2, 98, 138, cz - 1.2, cz + 1.2, c(bone))
    return m
def b15_bars(w, h, img):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; mt = c((84, 88, 96))
    a, b_ = min(X0, -48), max(X1, 48)
    for y in (0, 96, 192): box(m, a, b_, y, y + 6, cz - 2.5, cz + 2.5, mt)                 # rails
    n = 9
    for k in range(n): x = a + (b_ - a) * (k + 0.5) / n; box(m, x - 1.6, x + 1.6, 0, 198, cz - 1.6, cz + 1.6, mt)   # bars
    return m
_b15 = {'jailbars': b15_bars,'d_eliteopsheli': lambda w, h, i: b15_heli(w, h, i, (50, 54, 60)), 'd_fireheli': lambda w, h, i: b15_heli(w, h, i, (200, 40, 36)),
        'd_paramedicsheli': lambda w, h, i: b15_heli(w, h, i, (236, 236, 236)), 'd_prisonheli': lambda w, h, i: b15_heli(w, h, i, (90, 96, 104)),
        'd_transportheli': lambda w, h, i: b15_heli(w, h, i, (70, 90, 60)),
        'd_ferryboat': lambda w, h, i: b15_boat(w, h, i, (236, 236, 236)), 'd_prisonboat': lambda w, h, i: b15_boat(w, h, i, (90, 96, 104)),
        'd_supplyboat': lambda w, h, i: b15_boat(w, h, i, (200, 120, 40)), 'd_visitorboat': lambda w, h, i: b15_boat(w, h, i, (60, 110, 170)),
        'd_bountyhunter_trike': b15_trike,
        'mailsatchel': lambda w, h, i: b15_sack(w, h, i, (110, 80, 50), 22, 11), 'd_prisonerletter': lambda w, h, i: b15_sack(w, h, i, (170, 150, 110), 30, 15),
        'mailsorted': lambda w, h, i: b15_letters(w, h, i, 5), 'mailunsorted': lambda w, h, i: b15_letters(w, h, i, 8, (226, 220, 200)),
        'd_loveletter': lambda w, h, i: b15_letters(w, h, i, 1, (250, 200, 210)), 'd_loveletteropened': lambda w, h, i: b15_letters(w, h, i, 1, (250, 200, 210), True),
        'plateblank': b15_plate, 'd_restaurantplateclean': b15_plate, 'd_restaurantmealcooking': lambda w, h, i: b15_plate(w, h, i, (190, 120, 60)),
        'garbage': lambda w, h, i: b15_sack(w, h, i, (40, 40, 44), 24, 12), 'd_recyclingbag': lambda w, h, i: b15_sack(w, h, i, (60, 120, 200), 24, 12),
        'foodtrash1': lambda w, h, i: b15_sack(w, h, i, (110, 100, 70), 8, 9), 'd_zombiechum': lambda w, h, i: b15_sack(w, h, i, (120, 40, 40), 10, 10),
        'shopgoods1': lambda w, h, i: b15_box(w, h, i, (196, 160, 110), (20, 14, 14), (176, 140, 92)), 'stafffood1': lambda w, h, i: b15_box(w, h, i, (236, 236, 230), (14, 5, 10), (200, 60, 50)),
        'stafffoodstack1': lambda w, h, i: b15_box(w, h, i, (236, 236, 230), (16, 18, 12), (200, 60, 50)), 'd_d11stackmeals01': lambda w, h, i: tray_stack(6, False),
        'hammer': b15_hammer, 'rawwood': b12_logs, 'd_canvas': b15_easel, 'd_cinemaprojector': b15_projector, 'd_cinemascreenbottom': b15_screen,
        'd_ironingboardshort': b15_ironing, 'd_jackolantern1': b15_pumpkin, 'd_gardengnome': b15_gnome, 'd_wardenstatue': b15_statue,
        'd_ornamental_gravestone': b15_gravestone, 'd_windsock': b15_pole, 'd_weatherantenna': lambda w, h, i: b15_pole(w, h, i, 'antenna'),
        'd_lifering': b15_lifering, 'd_buoy': b15_buoy, 'd_kneeraise': b15_kneeraise, 'd_cakestandempty': b15_cake_stand, 'd_therapydalmation': b15_dog,
        'd_stonewaterspout': b15_spout, 'd_autoharvester': b15_harvester, 'd_autoscythe': lambda w, h, i: b15_harvester(w, h, i, (60, 140, 60)),
        'd_basicautogatherer': lambda w, h, i: b15_harvester(w, h, i, (200, 100, 40)), 'd_advancedautogatherer': lambda w, h, i: b15_harvester(w, h, i, (60, 100, 180)),
        'd_rorschachtest': b15_rorschach, 'd_pasystem': b15_pa, 'd_tattooremovalmachine': b15_tattoo, 'd_animalcontainer': b15_container, 'd_anatomymodel': b15_anatomy}
def b16_serving_table(w, h, img):
    """the canteen serving table with its four pans OPEN and full of food, as the 2D table shows (the stock model has four closed lids:
    the canteen looked empty). Laid out on the stock model's footprint: x -250..250, pans x -144..245, table top y 55."""
    m = Mesh(); c = swatch; steel, steeld, rail = c((196, 200, 206)), c((150, 154, 160)), c((120, 124, 130))
    box(m, -250, 250, 50, 55, -50, 50, steel); box(m, -250, 250, 46, 50, -50, 50, steeld)              # top
    box(m, -250, 250, 50, 53, 50, 72, rail)                                                              # tray slide along the front
    for x in (-244, -2, 238):
        for z in (-44, 40): box(m, x - 3, x + 3, 0, 46, z - 3, z + 3, steeld)                              # legs
    box(m, -246, 246, 12, 15, -44, 44, steeld)                                                           # shelf rail
    foods = [((236, 226, 196), (220, 206, 170)), ((96, 156, 58), (70, 128, 44)), ((150, 86, 52), (118, 64, 38)), ((206, 112, 50), (180, 90, 40))]
    import random as _r; rr = _r.Random(16)
    for k, (x0, x1) in enumerate(((-144, -52), (-48, 48), (52, 148), (152, 245))):
        box(m, x0, x1, 55, 62, -48, 46, steel)                                                           # pan body
        for (a0, a1, b0, b1) in ((x0, x1, -48, -45), (x0, x1, 43, 46), (x0, x0 + 3, -48, 46), (x1 - 3, x1, -48, 46)):
            box(m, a0, a1, 62, 69, b0, b1, steel)                                                        # open rim
        box(m, x0 + 3, x1 - 3, 62, 67, -45, 43, c(foods[k][0]))                                          # the food, nearly to the brim
        for j in range(12):      # lumps on top
            fx, fz = rr.uniform(x0 + 12, x1 - 12), rr.uniform(-36, 34)
            _ball(m, fx, 67, fz, 8, 4, 8, foods[k][1] if j % 2 else foods[k][0], 8, 4)
        box(m, x0 + 6, x0 + 9, 66, 80, 30, 33, rail)      # serving spoon handle
    return m
_b15['servingtable'] = b16_serving_table
for _k, _fn in _b15.items():
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        try: _fn(_w, _h, _img).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch15'))
        except Exception as _e: print('batch15 failed', _k, _e)
    else: print('batch15: no sprite', _k)
_bn = os.path.join(shapes_dir, 'bin.obj')
if os.path.exists(_bn): _scaled_xyz(_bn, 0.5, 0.5, 0.5, os.path.join(OUT, 'data', 'shapes', 'bin.obj'))      # the wheelie bin at half its size (it was bigger than a person)
_sd = os.path.join(shapes_dir, 'schoolDesk.obj')
if os.path.exists(_sd):      # the game's school desk is 1.6 x 1.3 x 1.4 times the size of its 2D footprint: bring it down to the sprite's size
    _scaled_xyz(_sd, 0.62, 0.68, 0.76, os.path.join(OUT, 'data', 'shapes', 'schooldesk.obj'))
def tray_item(w, h, img, dirty):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    hw, hd = min(30.0, (X1 - X0) / 2), min(18.0, (Z1 - Z0) / 2)
    col = (170, 158, 126) if dirty else (206, 210, 216)
    box(m, cx - hw, cx + hw, 0, 2.5, cz - hd, cz + hd, c(col))
    for (a, b, e, f) in ((cx - hw, cx + hw, cz - hd, cz - hd + 2.5), (cx - hw, cx + hw, cz + hd - 2.5, cz + hd), (cx - hw, cx - hw + 2.5, cz - hd, cz + hd), (cx + hw - 2.5, cx + hw, cz - hd, cz + hd)):
        box(m, a, b, 2.5, 6, e, f, c(shade(col, 0.85)))
    if dirty:
        box(m, cx - hw * 0.7, cx - hw * 0.1, 2.5, 4, cz - hd * 0.6, cz + hd * 0.1, c((120, 92, 60)))
        box(m, cx + hw * 0.1, cx + hw * 0.6, 2.5, 3.5, cz - hd * 0.2, cz + hd * 0.5, c((96, 110, 70)))
    else:
        box(m, cx - hw * 0.7, cx - hw * 0.1, 2.5, 4, cz - hd * 0.6, cz + hd * 0.6, c((170, 120, 70)))
        box(m, cx + hw * 0.1, cx + hw * 0.7, 2.5, 4, cz - hd * 0.6, cz + hd * 0.6, c((110, 170, 90)))
    return m
for _k, _dirty in (('d_d11foodtraydirty', True), ('d_d11foodtray', False), ('foodtraydirty1', True), ('restaurantplatedirty', True)):
    if _k in sprites:
        _n, _w, _h, _img = sprites[_k]
        tray_item(_w, _h, _img, _dirty).write(os.path.join(OUT, 'data', 'shapes', _k + '.obj'), _n); report['made'].append((_k, _k, 'batch5'))
# sprites the game draws as part of another object (no materials.txt entry of their own)
if 'fridgelid' in sprites and 'fridgelid' in ARCH:
    _n, _w, _h, _img = sprites['fridgelid']
    _m = arch_fridge_lid(_w, _h, _img)
    for _nm in ('fridgelid', 'fridge_lid'):
        _m.write(os.path.join(OUT, 'data', 'shapes', _nm + '.obj'), _n)

# Final size fixes, after every batch has written its files (so none of them can undo these): name -> (across, up) factor
FINAL_SCALE = {
    'ingredient1': (2.2, 1.6), 'ingredient2': (2.2, 1.6), 'ingredient3': (2.2, 1.6), 'ingredient4': (2.2, 1.6), 'd_d11ingredient01': (2.2, 1.6),   # the 2D crates are about a tile wide (the 3D ones were specks)
    'bin': (1.25, 1.25),      # user 6 Nov: bin 25 % bigger
    # the DLC trees (SnowyConiferTree, SpookyTree, BareTree) have SpriteScale 0.5: the game draws them at half size, the 3D model too
    'd_treesnowyconifer': (2.0, 2.0), 'd_treespooky': (2.0, 2.0), 'd_treespookyalt': (2.0, 2.0), 'd_baretree': (2.0, 2.0),      # (young conifer: no object uses it at half size)
}
# Bare trees are drawn with the spooky-tree sprite slot (the DLL redirects them: their own sprite has no 3D slot), and spooky trees are
# leafless too: both get the bare tree (it was a purple round canopy, so bare trees would have looked like purple blobs)
_bt = os.path.join(OUT, 'data', 'shapes', 'd_baretree.obj')
if os.path.exists(_bt):
    for _n in ('d_treespooky', 'd_treespookyalt'):
        shutil.copyfile(_bt, os.path.join(OUT, 'data', 'shapes', _n + '.obj')); report['made'].append((_n, _n, 'bare-tree copy'))
for _n, (_sx, _sy) in FINAL_SCALE.items():
    _p = os.path.join(OUT, 'data', 'shapes', _n + '.obj')
    _src = _p if os.path.exists(_p) else (os.path.join(shapes_dir, _stock_files[_n + '.obj']) if _n + '.obj' in _stock_files else None)
    if not _src: continue
    _lines = []
    for _l in open(_src, encoding='latin1'):
        if _l.startswith('v '):
            _q = _l.split(); _l = 'v %.4f %.4f %.4f\n' % (float(_q[1]) * _sx, float(_q[2]) * _sy, float(_q[3]) * _sx)
        _lines.append(_l)
    open(_p, 'w', encoding='latin1').writelines(_lines); report['made'].append((_n, _n, 'final-scale'))
atlas.save(os.path.join(OUT, 'data', 'shapes', 'objects-maps.png'))
report['cookies_final'] = sorted(k for k in COOKIES if k in MAINW and WRITERS.get(k) == MAINW[k] )
json.dump(report, open(os.path.join(OUT, 'report.json'), 'w'), indent=1)
print('made', len(report['made']), 'skipped', len(report['skipped']))
