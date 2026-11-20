import sys, os, re, json, shutil, numpy as np
from PIL import Image
old, out = sys.argv[1], sys.argv[2]
STOCK = os.path.join(old, "shapes"); SH = os.path.join(out, "data", "shapes")
A = np.asarray(Image.open(os.path.join(SH, "objects-maps.png")).convert("RGB")).astype(int); H, W = A.shape[:2]
pc = json.load(open(os.path.join(out, "people_colours.json")))
have = {f[:-4].lower(): f for f in os.listdir(STOCK) if f.endswith(".obj")}
def tpl(n): return os.path.join(STOCK, have[n.lower()]) if n.lower() in have else None
def readobj(p):
    L = open(p, encoding="latin1").read().split("\n"); T = []; V = []
    for l in L:
        q = l.split()
        if not q: continue
        if q[0] == "vt": T.append((float(q[1]), float(q[2])))
        elif q[0] == "v": V.append(float(q[2]))
    ty = {}
    for l in L:
        q = l.split()
        if q and q[0] == "f":
            for a in q[1:]:
                pp = a.split("/")
                if len(pp) > 1 and pp[1]: ty.setdefault(int(pp[1]) - 1, []).append(V[int(pp[0]) - 1])
    return L, T, ty
def tcol(T, i):
    u, v = T[i]; x = min(W - 1, int(u * W)); y = min(H - 1, int((1 - v) * H)); return tuple(int(c) for c in A[y, x])
# Exact colours: paint an 8 x 8 swatch for each colour these repaints need, in an all-black block that no model uses (stock or generated).
# (It used to take the nearest pixel already in the atlas - sometimes a pixel of sprite art - so people's colours drifted whenever the atlas layout changed.)
BLK = 8; _used = set()
for _d in (STOCK, SH):      # every block a model's textured triangles cover (their whole uv footprint), wrapped like the GPU
    for _f in os.listdir(_d):
        if not _f.lower().endswith('.obj'): continue
        _T = []
        for _l in open(os.path.join(_d, _f), encoding='latin1'):
            _q = _l.split()
            if not _q: continue
            if _q[0] == 'vt': _T.append((float(_q[1]), float(_q[2])))
            elif _q[0] == 'f':
                _uv = [_T[int(a.split('/')[1]) - 1] for a in _q[1:] if len(a.split('/')) > 1 and a.split('/')[1]]
                if not _uv: continue
                _xs = [u * W / BLK for u, v in _uv]; _ys = [(1 - v) * H / BLK for u, v in _uv]
                for _by in range(int(np.floor(min(_ys))), int(np.floor(max(_ys))) + 1):
                    for _bx in range(int(np.floor(min(_xs))), int(np.floor(max(_xs))) + 1): _used.add((_by % (H // BLK), _bx % (W // BLK)))
_swatches = {}; _scan = [0]; _dirty = [False]
def _free_block():
    gw, gh = W // BLK, min(H, 2048) // BLK
    while _scan[0] < gw * gh:
        by, bx = divmod(_scan[0], gw); _scan[0] += 1
        if any((by + dy, bx + dx) in _used for dy in (-1, 0, 1) for dx in (-1, 0, 1)): continue
        blk = A[max(0, by * BLK - BLK):by * BLK + 2 * BLK, max(0, bx * BLK - BLK):bx * BLK + 2 * BLK]
        if blk.sum(axis=2).max() <= 24: return by, bx
    raise SystemExit('ppl3: no free atlas block for a colour swatch')
def near(c):
    c = tuple(int(v) for v in c)
    if c not in _swatches:
        by, bx = _free_block(); A[by * BLK:(by + 1) * BLK, bx * BLK:(bx + 1) * BLK] = c; _used.add((by, bx)); _dirty[0] = True
        _swatches[c] = ((bx * BLK + BLK / 2) / W, 1 - (by * BLK + BLK / 2) / H)
    return _swatches[c]
def sh(c, f): return tuple(max(0, min(255, int(v * f))) for v in c)
def luma(c): return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
def is_skin(c): return c[0] > 190 and c[1] > 140 and c[2] > 90 and c[0] >= c[1] >= c[2] and 30 < c[0] - c[2] < 150
def recolour(src, key, name, skin, top, body, naked=False, uw=None, plain=False):
    L, T, ty = readobj(src); info = {}
    for ti, ys in ty.items(): info.setdefault(tcol(T, ti), []).extend(ys)
    cols = sorted(info, key=lambda c: -len(info[c]))
    skinc = [c for c in cols if is_skin(c)]
    whites = [c for c in cols if not is_skin(c) and luma(c) > 38 and min(c) > 190]
    rest = [c for c in cols if not is_skin(c) and luma(c) > 38 and not (min(c) > 190)]
    head = [c for c in rest if np.mean(info[c]) > 86]; bodyc = [c for c in rest if np.mean(info[c]) <= 86]
    mp = {}
    for c in skinc: mp[c] = skin
    for i, c in enumerate(head[:3]): mp[c] = top if i == 0 else sh(top, 0.8)
    for i, c in enumerate(bodyc):
        mp[c] = (skin if naked else body) if i == 0 else (sh(skin, 0.9) if naked else sh(body, 0.62 if luma(c) < luma(bodyc[0]) else 1.2))
    if plain:
        for c in whites: mp[c] = sh(body, 1.15)
    tg = {c: near(v) for c, v in mp.items()}; o = []; first = [True]
    # recolour whole triangles: one whose corners all show the same colour moves, all corners together, to that colour's new swatch.
    # (Moving single texture coordinates stretched mixed triangles - textured faces - across the atlas: garbage art on faces.)
    nvt = len(T); newvt = {}; extra = []
    def swv(c):
        if c not in newvt: newvt[c] = nvt + len(extra) + 1; extra.append("vt %.5f %.5f 0.0000" % tg[c])
        return newvt[c]
    for l in L:
        if l.startswith("f "):
            toks = l.split()[1:]; tis = [int(t.split("/")[1]) - 1 if len(t.split("/")) > 1 and t.split("/")[1] else None for t in toks]
            cs = {tcol(T, ti) for ti in tis if ti is not None}
            if None not in tis and len(cs) == 1 and next(iter(cs)) in tg:
                k = swv(next(iter(cs))); toks = ["/".join([t.split("/")[0], str(k)] + t.split("/")[2:]) for t in toks]; l = "f " + " ".join(toks)
        elif l.startswith("g ") and first[0]: l = "g " + name; first[0] = False
        o.append(l)
    lastvt = max(i for i, l in enumerate(o) if l.startswith("vt "))      # the swatch uvs go with the others, before any face uses them
    o = o[:lastvt + 1] + extra + o[lastvt + 1:]
    if naked and uw:
        V = [float(l.split()[2]) for l in o if l.startswith("v ")]; nvt = sum(1 for l in o if l.startswith("vt "))
        u1, v1 = near(uw); extra = ["vt %.5f %.5f 0.0000" % (u1, v1)]; o2 = []
        for l in o:
            if l.startswith("f "):
                toks = l.split()[1:]; ys = [V[int(t.split("/")[0]) - 1] for t in toks]
                if sum(ys) / len(ys) < 24: toks = ["/".join([t.split("/")[0], str(nvt + 1)] + t.split("/")[2:]) for t in toks]; l = "f " + " ".join(toks)
            o2.append(l)
        lastvt = max(i for i, l in enumerate(o2) if l.startswith("vt "))      # the underwear uv goes with the others, before any face uses it
        o = o2[:lastvt + 1] + extra + o2[lastvt + 1:]
    open(os.path.join(SH, key + ".obj"), "w", encoding="latin1").write("\n".join(o))
SKIN = [(250, 205, 160), (232, 180, 135), (206, 150, 108), (170, 118, 82), (126, 86, 60), (244, 214, 190)]
HAIR = [(40, 28, 20), (74, 50, 34), (110, 70, 40), (150, 110, 60), (206, 176, 110), (30, 28, 30), (172, 172, 176), (140, 66, 38)]
COAT = [(58, 74, 106), (96, 66, 70), (150, 150, 152), (120, 104, 76), (70, 70, 84), (60, 62, 68), (118, 84, 62), (86, 100, 116), (104, 92, 84), (168, 156, 140), (140, 120, 112), (78, 92, 130), (46, 52, 70), (130, 96, 80)]
def pick(lst, sd):
    ok = [n for n in lst if tpl(n)]
    return ok[sd % len(ok)] if ok else None
DOC = ["doctor", "doctor2", "doctor3", "doctor4"]
SUIT = [(46, 52, 70), (60, 62, 68), (40, 40, 46), (70, 60, 52), (52, 64, 90)]
done = 0
for key, d in pc.items():
    n = d["name"].lower(); sd = sum(ord(ch) * (i + 3) for i, ch in enumerate(n))
    skin = SKIN[sd % 6]; top = HAIR[(sd // 4) % 8]; body = COAT[(sd // 9) % len(COAT)]; naked = False; plain = True; copy_only = False
    if re.match(r"prisonernaked", n): t = pick(["prisoner", "prisoner3"], sd); copy_only = True      # the game uses these while its 2D view still shows the uniform: keep the uniform so the security colour shows
    elif re.search(r"prisoner|zombie|insane", n): t = pick(["prisoner", "prisoner2", "prisoner3", "prisoner4"], sd); t = t; copy_only = True      # the game tints prisoner uniforms by security level: keep the stock model's own (pale) uniform
    elif re.search(r"armour|armed|security|handler|guard", n): t = pick(["guard", "guard2", "guard3", "guard4"], sd); copy_only = True
    elif re.search(r"paramedic", n): t = pick(["paramedic"], sd); plain = False
    elif re.search(r"fire", n): t = pick(["fireman"], sd); plain = False
    elif re.search(r"truck|gritter|repair|water|pest|worker|workman|builder", n): t = pick(["workman", "workman2", "workman3", "workman4"], sd); plain = False
    elif re.search(r"lawyer|ceo|detective|negotiator|paroleboard|warden|teacher|reporter|cameraman|accountant|benedict|edward|morgan|thedon|heller|hurst|keane", n):
        t = pick(["warden"], sd); plain = False; body = SUIT[sd % len(SUIT)]      # office people wear suits: the doctor body put a stethoscope on the lawyer
    elif re.search(r"shrink|psychiatrist|orderly|doctor", n): t = pick(DOC, sd)
    else: t = pick(DOC, sd)
    uw = [(240, 240, 236), (96, 130, 190), (190, 70, 70)][sd % 3] if naked else None
    if not t: continue
    if copy_only: shutil.copyfile(tpl(t), os.path.join(SH, key + ".obj"))
    else: recolour(tpl(t), key, d["name"], skin, top, body, naked, uw, plain)
    done += 1
print("repainted stock models:", done)
# Prisoner bodies: each 3D body file gets the stock 3D body whose shape matches its 2D sprite (user 7 Nov: wrong bodies). The game's own
# files have Prisoner and Prisoner3 swapped: 2D Prisoner is a slim pill but prisoner.obj is the big dome, 2D Prisoner3 is the big dome but
# prisoner3.obj is the slim pill. 2D shapes (people.png): Prisoner 20 px wide pill, Prisoner2 54 wide bowl, Prisoner3 60 big dome,
# Prisoner4 41 medium; FemaleBody 15 pill, FemaleBody2 21 small bowl, FemaleBody3 30 dome, FemaleBody4 22 slim.
BODY_MATCH = {'prisoner': 'prisoner3', 'prisoner2': 'prisoner2', 'prisoner3': 'prisoner', 'prisoner4': 'prisoner4',
              'prisonernaked': 'prisoner3', 'prisonernaked2': 'prisoner2', 'prisonernaked3': 'prisoner', 'prisonernaked4': 'prisoner4',
              'prisonerfemale': 'prisoner3', 'prisonerfemale2': 'prisoner4', 'prisonerfemale3': 'prisoner', 'prisonerfemale4': 'prisoner3',
              'prisonernakedfemale': 'prisoner3', 'prisonernakedfemale2': 'prisoner4', 'prisonernakedfemale3': 'prisoner', 'prisonernakedfemale4': 'prisoner3'}
for _k, _t in BODY_MATCH.items():
    if tpl(_t): shutil.copyfile(tpl(_t), os.path.join(SH, _k + ".obj"))
print("prisoner bodies matched to their 2D shapes:", len(BODY_MATCH))
if _dirty[0]:      # keep the atlas's alpha channel as it was; only the new swatch blocks change
    _im = Image.open(os.path.join(SH, "objects-maps.png")); _mode = _im.mode; _rgba = np.asarray(_im.convert("RGBA")).copy()
    _rgba[:, :, :3] = A.astype(np.uint8)
    for (_c, (_u, _v)) in _swatches.items():
        _x, _y = int(_u * W - BLK / 2), int((1 - _v) * H - BLK / 2); _rgba[_y:_y + BLK, _x:_x + BLK, 3] = 255
    Image.fromarray(_rgba, "RGBA").convert(_mode).save(os.path.join(SH, "objects-maps.png"))
    print("ppl3: %d exact colour swatches added to the atlas" % len(_swatches))
