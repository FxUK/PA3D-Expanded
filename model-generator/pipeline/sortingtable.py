r"""Mail sorting table (user 9 Nov: the 2D sprite's lamp / books / stamp were smeared over one end): 3 x 1 tiles of wood, a lower shelf with
parcels, and at one end the sprite's things in 3D - a green banker's lamp, a stack of books, a letter tray, a stamp and ink pad.
usage: python -I sortingtable.py SHAPES [OUT]      (SHAPES has objects-maps.png; writes OUT\sortingtable.obj)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from objlib import Model
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
m = Model(SH)
WOOD, WOOD_DK = (214, 158, 86), (150, 100, 50)
CARD, CARD_DK, GREEN, BRASS = (196, 160, 110), (160, 124, 80), (30, 120, 70), (200, 170, 70)
X, Z, TOP = 155.0, 50.0, 76.0
for sx in (-1, 1):                                    # legs
    for sz in (-1, 1): m.box(sx * (X - 8) - 4, sx * (X - 8) + 4, 0, TOP - 5, sz * (Z - 8) - 4, sz * (Z - 8) + 4, WOOD_DK)
m.box(-X, X, TOP - 5, TOP, -Z, Z, WOOD)                  # top
m.box(-X + 2, X - 2, TOP - 5.5, TOP - 5, -Z + 2, Z - 2, WOOD_DK)
m.box(-X + 6, X - 6, 14, 18, -Z + 6, Z - 6, WOOD_DK)      # lower shelf
for i, (x0, w, h) in enumerate([(-140, 34, 22), (-100, 28, 18), (-66, 36, 26), (-24, 30, 20), (12, 34, 24), (52, 28, 16)]):   # parcels on it
    m.box(x0, x0 + w, 18, 18 + h, -Z + 12 + (i % 2) * 8, Z - 14 - (i % 3) * 6, CARD if i % 2 else CARD_DK)
# at the +x end, as in the sprite: lamp and books at the back, tray and stamp in front
lx, lz = X - 30, -Z + 18
m.box(lx - 9, lx + 9, TOP, TOP + 2.5, lz - 7, lz + 7, BRASS)                     # lamp base
m.cyl(lx, lz, TOP + 2.5, TOP + 26, 1.6, BRASS, 6)                                # stem
m.box(lx - 14, lx + 14, TOP + 24, TOP + 32, lz - 5, lz + 9, GREEN)               # green shade
m.box(lx - 12, lx + 12, TOP + 22, TOP + 24, lz - 3, lz + 7, (240, 230, 160))     # lit underside
bx, bz, y0 = X - 30, -Z + 40, TOP
for k, (c, h) in enumerate([((120, 30, 30), 6), ((50, 56, 62), 7), ((120, 30, 30), 5)]):   # books, a page edge on each
    m.box(bx - 15 + k, bx + 15 - k, y0, y0 + h, bz - 10 + k, bz + 10 - k, c)
    m.box(bx - 14 + k, bx + 14 - k, y0 + 0.8, y0 + h - 0.8, bz + 10 - k, bz + 10.5 - k, (236, 230, 210))
    y0 += h
m.box(X - 70, X - 40, TOP, TOP + 4, Z - 34, Z - 12, (120, 120, 126))             # letter tray
m.box(X - 68, X - 42, TOP + 4, TOP + 5, Z - 32, Z - 14, (238, 234, 220))
m.box(X - 30, X - 14, TOP, TOP + 2, Z - 30, Z - 16, (60, 60, 66))                # ink pad
m.box(X - 29, X - 15, TOP + 2, TOP + 2.4, Z - 29, Z - 17, (170, 30, 30))
m.cyl(X - 20, Z - 40, TOP, TOP + 3, 4, (60, 60, 66), 8)                          # stamp: base, handle, red knob
m.cyl(X - 20, Z - 40, TOP + 3, TOP + 12, 1.6, (120, 80, 40), 6)
m.cyl(X - 20, Z - 40, TOP + 12, TOP + 15, 3, (170, 30, 30), 8)
print('sortingtable: %d triangles' % m.write(os.path.join(OUT, 'sortingtable.obj'), 'SortingTable', 'sortingtable.py: table, parcels, lamp, books, tray, stamp'))
