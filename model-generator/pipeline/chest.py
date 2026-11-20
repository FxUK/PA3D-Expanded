r"""Chest of drawers (user 9 Nov: "on 1 sqr and tall"): one tile wide, under half a tile deep, tall; plain wood with four rows of two drawers.
usage: python -I chest.py SHAPES [OUT]      (SHAPES has objects-maps.png; writes OUT\d_chestofdrawers.obj)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from objlib import Model
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
m = Model(SH)
WOOD, WOOD_DK, WOOD_LT, METAL = (196, 140, 72), (120, 78, 38), (222, 170, 100), (190, 192, 196)
X, Z0, Z1, TOP = 46.0, -22.0, 18.0, 132.0
m.box(-X + 3, X - 3, 0, 8, Z0 + 2, Z1 - 2, WOOD_DK)                 # plinth
m.box(-X, X, 8, TOP - 5, Z0, Z1, WOOD)                               # carcass
m.box(-X - 2, X + 2, TOP - 5, TOP, Z0 - 1, Z1 + 3, WOOD_LT)           # top, overhanging the front
rows, gap, h0, h1 = 4, 3.0, 12.0, TOP - 9.0
rh = (h1 - h0) / rows
for r in range(rows):
    y0 = h0 + r * rh + gap / 2; y1 = h0 + (r + 1) * rh - gap / 2
    for c in (-1, 1):
        x0, x1 = (-X + 4, -gap / 2) if c < 0 else (gap / 2, X - 4)
        m.box(x0, x1, y0, y1, Z1, Z1 + 2.5, WOOD_LT)                   # drawer front
        mx, my = (x0 + x1) / 2, (y0 + y1) / 2
        m.box(mx - 6, mx + 6, my - 1.5, my + 1.5, Z1 + 2.5, Z1 + 5, METAL)   # handle
print('d_chestofdrawers: %d triangles' % m.write(os.path.join(OUT, 'd_chestofdrawers.obj'), 'd_ChestOfDrawers', 'chest.py: tall, four rows of two drawers'))
