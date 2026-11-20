"""Model generators retired from make_models.py on 5 Nov 2026 (kept for reference, not imported).
Each was either never called, or every file it made was overwritten by a later batch (pipeline/liveness.py found them;
the full pipeline output stayed byte-identical without them)."""

def _place_person_art(img):
    """paste a character's sprite (cropped to its opaque box, at most 56 px) into the atlas; returns (ox, oy, w, h) or None"""
    a = img.split()[3].point(lambda v: 255 if v > 110 else 0); bb = a.getbbox()
    if not bb: return None
    sc = min(1.0, 56.0 / max(bb[2] - bb[0], bb[3] - bb[1])); crop = img.crop(bb)
    tw, th = max(2, int(crop.width * sc)), max(2, int(crop.height * sc))
    spot = place((tw + 2 * BLK - 1) // BLK + 1, (th + 2 * BLK - 1) // BLK + 1)
    if spot is None: return None
    ox, oy = spot[0] + 4, spot[1] + 4
    art = crop.resize((tw, th), Image.LANCZOS)
    bg = Image.new('RGB', art.size, (0, 0, 0)); bg.paste(art.convert('RGB').filter(ImageFilter.MaxFilter(5)), (0, 0)); bg.paste(art.convert('RGB'), (0, 0), art)
    atlas.paste(bg, (ox, oy)); return ox, oy, tw, th

def bust_art(name, img):
    """a character wearing its own vanilla artwork: the sprite (outfit, tie, apron, stripes, hat, face) is projected from the front onto a 3D capsule body,
    head and arms, so each person keeps the look the game gave them"""
    art = _place_person_art(img)
    if art is None: return None
    ox, oy, tw, th = art
    body, skin, top, face_only = _people_colours(img)
    n = name.lower(); sd = sum(ord(ch) for ch in n); k = 0.94 + 0.12 * ((sd % 7) / 6.0)
    def amap(x, y, z):
        fx = min(1.0, max(0.0, 0.5 + x / (64.0 * k))); yy = y / k
        fy = (127.0 - yy) / 43.0 * 0.42 if yy >= 84.0 else 0.42 + (84.0 - yy) / 84.0 * 0.58
        fy = min(1.0, max(0.0, fy)); fx = 0.12 + 0.76 * fx; fy = 0.07 + 0.86 * fy          # stay inside the sprite's black outline
        return uv(ox + fx * (tw - 1), oy + fy * (th - 1))
    m = Mesh(); S = 9; hy = 106 * k
    lathe(m, [(0, 0), (25 * k, 0), (28 * k, 4), (30 * k, 12), (29 * k, 36), (27 * k, 52), (24 * k, 63), (19 * k, 71), (12 * k, 78), (7 * k, 82)], S, None, mapuv=amap)   # body
    lathe(m, [(7 * k, 80 * k), (6.6 * k, 93 * k)], 10, None, mapuv=amap)                                                         # neck
    _blob(m, 0, hy, 0, 20.5 * k, skin, 1.08, S, mapuv=amap)                                                                      # head with the painted face and hair
    for sgn in (-1, 1):
        _blob(m, sgn * 20.2 * k, hy - 1, 0, 4.4, skin, 1.0, 8)                                                                   # ears
        _blob(m, sgn * 26 * k, 70 * k, 0, 9.5, shade(body, 0.95), 0.95, 10)                                                      # shoulders (plain sleeve colour: the sprite edge is outline)
        lathe(m, [(0, 40 * k), (5.2, 40 * k), (7.0, 46 * k), (7.6, 60 * k), (6.6, 70 * k), (0, 72 * k)], 10, swatch(shade(body, 0.95)), cx=sgn * 31 * k, cz=2)   # sleeves
        _blob(m, sgn * 31 * k, 36 * k, 3, 6.0, skin, 0.95, 8)                                                                    # hands
    _blob(m, 0, hy - 1, -7 * k, 21.4 * k, top, 1.0, S)                                                                           # back of the head: hair colour
    if re.search(r'female|daughter|wife|kerry|sarah|nurse|girl|lady', n): _blob(m, 0, hy - 16, -12 * k, 20, top, 1.45, 12)       # long hair down the back
    return m

def _accent_colour(img, body):
    """a tie / scarf / sash colour down the middle of the chest that differs clearly from the coat, or None"""
    W, H = img.size; sm = img.resize((48, 48), Image.BOX); px = sm.load()
    pts = [(x, y) for y in range(48) for x in range(48) if px[x, y][3] > 150]
    if not pts: return None
    x0 = min(p[0] for p in pts); x1 = max(p[0] for p in pts); y0 = min(p[1] for p in pts); y1 = max(p[1] for p in pts)
    cx0 = x0 + (x1 - x0) * 0.44; cx1 = x0 + (x1 - x0) * 0.56; cy0 = y0 + (y1 - y0) * 0.55; cy1 = y0 + (y1 - y0) * 0.9
    cs = [px[x, y][:3] for (x, y) in pts if cx0 <= x <= cx1 and cy0 <= y <= cy1 and sum(px[x, y][:3]) > 150]
    if len(cs) < 3: return None
    cs.sort(key=lambda c: c[0] + c[1] + c[2]); c = cs[len(cs) // 2]
    return c if sum(abs(c[i] - body[i]) for i in range(3)) > 110 else None

def arch_toilet(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = lambda r: swatch(r)
    d = Z1 - Z0; wd = X1 - X0; cx = (X0 + X1) / 2
    tz = Z0 + d * 0.34                                    # cistern occupies the top third
    box(m, X0 + wd * 0.18, X1 - wd * 0.18, 0, 34, tz, Z1 - d * 0.08, c(PORC))            # pedestal
    box(m, X0 + wd * 0.04, X1 - wd * 0.04, 34, 50, tz, Z1, c(WHITE))                     # bowl
    box(m, X0 + wd * 0.18, X1 - wd * 0.18, 50, 54, tz + d * 0.06, Z1 - d * 0.06, c((246, 246, 242)))  # seat
    box(m, X0 + wd * 0.30, X1 - wd * 0.30, 54, 55, tz + d * 0.16, Z1 - d * 0.16, c(WATER))           # water
    box(m, X0, X1, 0, 92, Z0, tz, c(WHITE))                                              # cistern
    box(m, X0 - 2, X1 + 2, 92, 98, Z0 - 2, tz + 2, c((250, 250, 248)))                   # lid
    box(m, cx - 4, cx + 4, 98, 103, Z0 + d * 0.08, Z0 + d * 0.08 + 8, c(STEEL))          # flush
    return m

def arch_chair(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img, 0.9)
    m = Mesh(); c = lambda r: swatch(r)
    d = Z1 - Z0; wd = X1 - X0; cx = (X0 + X1) / 2; cz = (Z0 + Z1) / 2
    col = region_colour(img, 0.1, 0.1, 0.9, 0.9, (30, 60, 120))
    box(m, cx - 5, cx + 5, 0, 30, cz - 5, cz + 5, c(DARK))                                  # column
    box(m, X0, X1, 0, 5, cz - 5, cz + 5, c(DARK)); box(m, cx - 5, cx + 5, 0, 5, Z0, Z1, c(DARK))   # base cross
    box(m, X0, X1, 30, 40, Z0 + d * 0.22, Z1, c(col))                                       # seat
    box(m, X0 + wd * 0.05, X1 - wd * 0.05, 40, 78, Z0, Z0 + d * 0.2, c(shade(col, 0.9)))   # back
    return m

def arch_shower(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch
    cx = (X0 + X1) / 2; cz = (Z0 + Z1) / 2; r = min(X1 - X0, Z1 - Z0) * 0.42
    lathe(m, [(r, 0), (r, 3), (r * 0.9, 3.5)], 10, c((60, 64, 72)), cx=cx, cz=cz)           # drain tray
    lathe(m, [(r * 0.3, 3.5), (r * 0.3, 4)], 8, c((20, 22, 26)), cx=cx, cz=cz)              # drain
    pz = Z0 + (Z1 - Z0) * 0.18
    box(m, cx - 3, cx + 3, 0, 165, pz - 3, pz + 3, c(CHROME))                                # standpipe
    box(m, cx - 3, cx + 3, 160, 166, pz - 3, cz, c(CHROME))                                  # arm
    lathe(m, [(0, 156), (15, 158), (15, 163), (0, 166)], 10, c((220, 224, 230)), cx=cx, cz=cz)  # rose
    box(m, cx - 6, cx + 6, 60, 68, pz, pz + 8, c((200, 40, 40)))                             # tap
    return m

def arch_desk(w, h, img, H=78.0):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch
    wd = X1 - X0; d = Z1 - Z0
    wood = (226, 172, 98); dark = (150, 100, 56); top = H - 8
    box(m, X0, X1, top, H, Z0, Z1, c(wood))                                                   # top
    box(m, X0 + 2, X0 + wd * 0.40, 0, top, Z0 + d * 0.1, Z1 - 2, c(dark))                    # left pedestal
    box(m, X1 - wd * 0.2, X1 - 2, 0, top, Z0 + d * 0.1, Z1 - 2, c(dark))                     # right pedestal
    box(m, X0 + wd * 0.40, X1 - wd * 0.2, 14, top, Z0 + d * 0.1, Z0 + d * 0.1 + 3, c(shade(dark, 0.8)))   # modesty panel
    for i in range(4):                                                                         # drawers
        y0 = 6 + i * (top - 10) / 4
        box(m, X0 + 5, X0 + wd * 0.40 - 3, y0 + 2, y0 + (top - 10) / 4 - 2, Z1 - 2, Z1 - 0.5 + 1.0, c(shade(dark, 1.15)))
        box(m, X0 + wd * 0.17, X0 + wd * 0.23, y0 + 8, y0 + 11, Z1, Z1 + 2, c((236, 200, 90)))
    box(m, X1 - wd * 0.16, X1 - wd * 0.04, H, H + 14, Z0 + d * 0.15, Z0 + d * 0.15 + 12, c((38, 124, 72)))   # lamp
    box(m, X1 - wd * 0.30, X1 - wd * 0.22, H, H + 6, Z0 + d * 0.18, Z0 + d * 0.18 + 14, c((200, 50, 50)))    # books
    return m

def bench_park(w, h, img): return _bench(w, h, img)

def bench_oak(w, h, img): return _bench(w, h, img)

def bench_jungle(w, h, img): return _bench(w, h, img)

def arch_bottle(w, h, img, count=1):
    """bleach and similar: round bottle with shoulder, neck, cap and a label band"""
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); bc = body_of(img, (230, 230, 240))
    cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    r = min(X1 - X0, Z1 - Z0) / 2 * 0.92
    cap = region_colour(img, 0.3, 0.0, 0.7, 0.3, (200, 40, 40))
    lathe(m, [(r * 0.9, 0), (r, 3), (r, 30), (r * 0.7, 38), (r * 0.36, 42), (r * 0.36, 48)], 10, swatch(bc), cx=cx, cz=cz)
    lathe(m, [(r * 1.02, 11), (r * 1.02, 24)], 10, swatch((40, 90, 190)), cx=cx, cz=cz)      # label band
    lathe(m, [(r * 0.42, 48), (r * 0.42, 56), (r * 0.3, 57)], 8, swatch(cap), cx=cx, cz=cz)   # cap
    return m

def arch_solarpanels(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch
    prism(m, [(Z0 + 4, 30), (Z1 - 4, 78), (Z1 - 4, 83), (Z0 + 4, 35)], X0 + 3, X1 - 3, 'x', c((34, 62, 130)))
    for lx in (X0 + 10, X1 - 14):
        box(m, lx, lx + 4, 0, 34, Z0 + 6, Z0 + 10, c(GREY)); box(m, lx, lx + 4, 0, 82, Z1 - 10, Z1 - 6, c(GREY))
    return m

def arch_dumbbells(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; d = Z1 - Z0
    for sx in (X0 + 4, X1 - 8): box(m, sx, sx + 4, 0, 52, Z0 + 4, Z1 - 4, c(DARK))
    box(m, X0 + 4, X1 - 4, 24, 28, Z0 + 4, Z1 - 4, c(STEEL)); box(m, X0 + 4, X1 - 4, 46, 50, Z0 + 4, Z1 - 4, c(STEEL))
    n = max(3, min(5, int((X1 - X0) // 30)))
    for i in range(n):
        bx = X0 + 12 + (X1 - X0 - 24) * i / max(1, n - 1)
        for lv, y in ((0, 28), (1, 50)):
            for sz in (Z0 + d * 0.28, Z0 + d * 0.72):
                box(m, bx - 4, bx + 4, y, y + 8, sz - 4, sz + 4, c((60, 62, 68)))
    return m

def arch_pullup(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; cz = (Z0 + Z1) / 2
    for sx in (X0 + 4, X1 - 10): box(m, sx, sx + 6, 0, 210, cz - 3, cz + 3, c(STEEL))
    box(m, X0 + 4, X1 - 4, 204, 210, cz - 3, cz + 3, c(STEEL))
    for sx in (X0 + 2, X1 - 12): box(m, sx, sx + 10, 0, 4, cz - 14, cz + 14, c(DARK))
    return m

def arch_pingpong(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; zc = (Z0 + Z1) / 2; xc = (X0 + X1) / 2
    box(m, X0, X1, 68, 76, Z0, Z1, c((30, 100, 70)))
    box(m, xc - 1, xc + 1, 76, 92, Z0, Z1, c((236, 236, 236)))
    for sx in (X0 + 8, X1 - 12):
        for sz in (Z0 + 6, Z1 - 10): box(m, sx, sx + 4, 0, 68, sz, sz + 4, c(DARK))
    return m

def arch_shopshelf(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img)
    m = Mesh(); c = swatch; fr = (150, 108, 64)
    d = Z1 - Z0; wd = X1 - X0; H = 150.0
    box(m, X0, X1, 0, H, Z1 - 4, Z1, c(shade(fr, 0.7)))                                       # back panel
    box(m, X0, X0 + 4, 0, H, Z0, Z1, c(fr)); box(m, X1 - 4, X1, 0, H, Z0, Z1, c(fr))
    cols = [(210, 60, 60), (60, 140, 210), (240, 190, 50), (90, 180, 90), (180, 90, 200), (240, 130, 60)]
    for lv in range(4):
        y = 8 + lv * 40
        box(m, X0, X1, y, y + 4, Z0, Z1, c(fr))                                               # shelf board
        n = max(3, int(wd // 22)); k = 0
        for i in range(n):
            bx = X0 + 8 + (wd - 24) * i / max(1, n - 1)
            box(m, bx, bx + 14, y + 4, y + 4 + 14 + (i * 7 + lv * 3) % 10, Z0 + d * 0.25, Z1 - 8, c(cols[(i + lv) % 6])); k += 1
    return m

def arch_arcade(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    cx = (X0 + X1) / 2; wd = X1 - X0; d = Z1 - Z0; body = (40, 70, 150)
    box(m, cx - wd * 0.42, cx + wd * 0.42, 0, 150, Z0 + d * 0.12, Z1 - d * 0.1, c(body))
    box(m, cx - wd * 0.46, cx + wd * 0.46, 138, 160, Z0 + d * 0.1, Z1 - d * 0.08, c((230, 190, 40)))   # marquee
    box(m, cx - wd * 0.32, cx + wd * 0.32, 82, 128, Z1 - d * 0.1, Z1 - d * 0.1 + 3, c((20, 24, 40)))     # screen
    box(m, cx - wd * 0.32, cx + wd * 0.32, 60, 76, Z1 - d * 0.1, Z1 - d * 0.1 + 14, c((30, 30, 40)))     # control deck
    box(m, cx - 3, cx + 3, 76, 90, Z1 - d * 0.1 + 6, Z1 - d * 0.1 + 10, c((200, 40, 40)))               # joystick
    return m

def arch_moose(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx = (X0 + X1) / 2; zc = (Z0 + Z1) / 2
    box(m, cx - 30, cx + 30, 70, 150, zc - 4, zc + 4, c((110, 70, 40)))
    box(m, cx - 14, cx + 14, 86, 120, zc + 4, zc + 26, c((140, 92, 56))); box(m, cx - 8, cx + 8, 80, 90, zc + 20, zc + 40, c((120, 80, 48)))
    for s in (-1, 1):
        box(m, cx + s * 14 - 3, cx + s * 14 + 3, 118, 140, zc + 8, zc + 14, c((220, 200, 150))); box(m, cx + s * 14 - 3 + s * 16, cx + s * 14 + 3 + s * 16, 128, 150, zc + 8, zc + 14, c((220, 200, 150)))
        box(m, min(cx + s * 14, cx + s * 30), max(cx + s * 14, cx + s * 30), 134, 140, zc + 8, zc + 14, c((220, 200, 150)))
    return m

def arch_tyre(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; zc = (Z0 + Z1) / 2; wd = X1 - X0
    box(m, X0, X1, 0, 6, zc - 28, zc + 28, c((70, 74, 82)))
    n = max(2, int(wd // 48))
    for i in range(n):
        cxx = X0 + 28 + (wd - 56) * i / max(1, n - 1)
        lathe(m, [(22, 6), (26, 14), (26, 26), (22, 34), (10, 34), (10, 6)], 12, c((36, 38, 46)), cx=cxx, cz=zc)
    return m

def arch_chair2(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img, 0.9); m = Mesh(); c = swatch; cx = (X0 + X1) / 2; cz = (Z0 + Z1) / 2; col = (120, 40, 30)
    lathe(m, [(5, 0), (5, 28)], 8, c((50, 50, 56)), cx=cx, cz=cz)
    box(m, X0, X1, 0, 5, cz - 5, cz + 5, c((50, 50, 56))); box(m, cx - 5, cx + 5, 0, 5, Z0, Z1, c((50, 50, 56)))
    box(m, X0 + 4, X1 - 4, 28, 40, Z0 + (Z1 - Z0) * 0.2, Z1 - 4, c(col)); box(m, X0 + 6, X1 - 6, 40, 82, Z0 + 2, Z0 + (Z1 - Z0) * 0.2, c(shade(col, 0.9)))
    return m

def arch_tray(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    wd = X1 - X0; d = Z1 - Z0; col = (200, 204, 210)
    box(m, X0, X1, 0, 3, Z0, Z1, c(col))
    for (a, b, e, f) in ((X0, X1, Z0, Z0 + 3), (X0, X1, Z1 - 3, Z1), (X0, X0 + 3, Z0, Z1), (X1 - 3, X1, Z0, Z1)):
        box(m, a, b, 3, 8, e, f, c(shade(col, 0.85)))                                  # raised rim
    box(m, X0 + wd * 0.5 - 1.5, X0 + wd * 0.5 + 1.5, 3, 6, Z0 + 3, Z1 - 3, c(shade(col, 0.8)))   # divider
    box(m, X0 + 6, X0 + wd * 0.46, 3, 5, Z0 + 6, Z1 - 6, c((170, 120, 70)))            # food
    box(m, X0 + wd * 0.54, X1 - 6, 3, 5, Z0 + 6, Z0 + d * 0.5 - 2, c((110, 170, 90)))
    box(m, X0 + wd * 0.54, X1 - 6, 3, 5, Z0 + d * 0.5 + 2, Z1 - 6, c((200, 90, 70)))
    return m

def arch_coffee(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; wd = X1 - X0
    box(m, X0 + 4, X1 - 4, 0, 6, Z0 + 4, Z1 - 4, c((40, 42, 48))); box(m, X0 + 6, X1 - 6, 6, 78, Z0 + 6, Z1 - 6, c((70, 74, 84)))
    box(m, X0 + 4, X1 - 4, 78, 86, Z0 + 4, Z1 - 4, c((200, 204, 210))); box(m, X0 + 10, X1 - 10, 40, 62, Z1 - 6, Z1 - 4, c((24, 26, 30)))    # drip bay
    box(m, X0 + wd * 0.4, X0 + wd * 0.6, 58, 66, Z1 - 12, Z1 - 5, c((190, 194, 200))); box(m, X0 + wd * 0.42, X0 + wd * 0.58, 20, 36, Z1 - 12, Z1 - 5, c((240, 240, 236)))   # spout + cup
    box(m, X0 + 12, X0 + 20, 66, 74, Z1 - 5, Z1 - 3, c((200, 50, 50))); box(m, X1 - 20, X1 - 12, 66, 74, Z1 - 5, Z1 - 3, c((60, 200, 90)))
    return m

def b3_rubble(w, h, img):
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch; cx, cz = (X0 + X1) / 2, (Z0 + Z1) / 2
    for k, (dx, dz, sx, sz, hh) in enumerate(((-18, -8, 16, 12, 12), (10, -14, 14, 16, 9), (14, 12, 18, 12, 14), (-8, 14, 12, 14, 8), (0, 0, 14, 14, 18), (-24, 10, 10, 10, 6))):
        box(m, cx + dx - sx / 2, cx + dx + sx / 2, 0, hh, cz + dz - sz / 2, cz + dz + sz / 2, c(shade((140, 134, 126), 0.8 + 0.07 * (k % 4))))
    return m

def b6_prisoner_bus(w, h, img):
    """a long prison bus: barred windows, a darker skirt, a roof stripe and wheels; it runs along the footprint's long side"""
    X0, X1, Z0, Z1 = frame_of(w, h, img); m = Mesh(); c = swatch
    body, skirt, roof, win, bar, tyre, hub, light = c((48, 92, 64)), c((30, 54, 38)), c((220, 222, 214)), c((40, 50, 58)), c((150, 156, 160)), c((28, 28, 30)), c((170, 172, 176)), c((250, 230, 150))
    along_z = (Z1 - Z0) >= (X1 - X0)
    def B(a0, a1, y0, y1, b0, b1, col):        # a = across the bus, b = along it
        if along_z: box(m, a0, a1, y0, y1, b0, b1, col)
        else: box(m, b0, b1, y0, y1, a0, a1, col)
    A0, A1 = (X0, X1) if along_z else (Z0, Z1); L0, L1 = (Z0, Z1) if along_z else (X0, X1)
    wdh = min(A1 - A0, 90.0); ac = (A0 + A1) / 2; a0, a1 = ac - wdh / 2, ac + wdh / 2
    B(a0, a1, 14, 34, L0 + 2, L1 - 2, skirt)                                            # chassis skirt
    B(a0, a1, 34, 122, L0 + 4, L1 - 2, body)                                            # body
    B(a0 + 2, a1 - 2, 122, 128, L0 + 8, L1 - 6, roof)                                   # roof
    n = max(4, int((L1 - L0 - 60) // 34))
    for k in range(n):                                                                  # barred windows both sides
        b0 = L0 + 40 + k * (L1 - L0 - 60) / n; b1 = b0 + (L1 - L0 - 60) / n - 6
        for side in (a0 - 0.6, a1):
            B(side, side + 0.6, 80, 110, b0, b1, win)
            for j in range(4): bb = b0 + (b1 - b0) * (j + 0.5) / 4; B(side - 0.4, side + 1.0, 80, 110, bb - 0.8, bb + 0.8, bar)
    B(a0 + 6, a1 - 6, 70, 112, L0 + 3, L0 + 4, win)                                    # windscreen at the front (-)
    for ox in (a0 + 10, a1 - 18): B(ox, ox + 8, 44, 52, L0 + 3, L0 + 4, light)         # headlights
    for b in (L0 + 30, L1 - 34):                                                        # wheels
        for side in (a0 - 3, a1 - 1):
            if along_z: zcyl_x = None
            for sgn in (0,):
                pass
            B(side, side + 4, 0, 30, b - 15, b + 15, tyre); B(side - 0.6 if side < ac else side + 4, side if side < ac else side + 4.6, 8, 22, b - 7, b + 7, hub)
    return m

def b15_machine(w, h, img, hgt=110, col=None, screen=True):
    X0, X1, Z0, Z1, cx, cz = _fr(w, h, img); m = Mesh(); c = swatch; col = col or _bc(img, (190, 194, 200))
    box(m, X0 + 3, X1 - 3, 0, hgt * 0.75, Z0 + 3, Z1 - 3, c(col)); box(m, X0 + 1, X1 - 1, hgt * 0.75, hgt * 0.8, Z0 + 1, Z1 - 1, c(shade(col, 0.8)))
    if screen: box(m, cx - (X1 - X0) * 0.25, cx + (X1 - X0) * 0.25, hgt * 0.8, hgt, Z0 + 6, Z0 + 9, c((40, 44, 52))); box(m, cx - (X1 - X0) * 0.22, cx + (X1 - X0) * 0.22, hgt * 0.83, hgt * 0.97, Z0 + 9, Z0 + 9.4, c((90, 170, 210)))
    for k in range(3): box(m, X0 + 8 + k * 7, X0 + 12 + k * 7, hgt * 0.55, hgt * 0.6, Z1 - 3, Z1 - 2, c((200, 60, 50) if k == 0 else (60, 180, 80)))
    return m
