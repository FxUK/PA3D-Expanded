r"""Oak desk (user 9 Nov: "desk could be improved with depth"): 2 x 1 tiles of dark oak - two drawer pedestals, a recessed kneehole with a
modesty panel, a moulded top - and the sprite's things in 3D: green leather blotter, papers, a green banker's lamp, an inkwell and pen.
usage: python -I oakdesk.py SHAPES [OUT]      (SHAPES has objects-maps.png; writes OUT\oakdesk.obj)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from objlib import Model
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
m = Model(SH)
OAK, OAK_DK, OAK_LT, BRASS = (104, 60, 30), (64, 36, 18), (132, 80, 42), (200, 170, 70)
LEATHER, PAPER, GREEN, INK = (34, 84, 46), (236, 232, 218), (30, 120, 70), (30, 30, 36)
X, Z, TOP = 98.0, 48.0, 78.0          # half width / depth, top height; the front (drawers, kneehole) faces +z
PW = 34.0                              # pedestal width
for sx in (-1, 1):                     # two pedestals with three drawers each, a plinth under them
    x0, x1 = (-X + 2, -X + 2 + PW) if sx < 0 else (X - 2 - PW, X - 2)
    m.box(x0 - 1, x1 + 1, 0, 6, -Z + 1, Z - 1, OAK_DK)
    m.box(x0, x1, 6, TOP - 6, -Z + 2, Z - 2, OAK)
    for k in range(3):
        y0 = 10 + k * 20; y1 = y0 + 17
        m.box(x0 + 3, x1 - 3, y0, y1, Z - 2, Z, OAK_LT)                                          # drawer front, proud of the carcass
        mx = (x0 + x1) / 2
        m.box(mx - 5, mx + 5, (y0 + y1) / 2 - 1.2, (y0 + y1) / 2 + 1.2, Z, Z + 2, BRASS)          # handle
m.box(-X + PW + 2, X - PW - 2, TOP - 18, TOP - 6, Z - 6, Z - 2, OAK)                               # centre drawer over the kneehole
m.box(-8, 8, TOP - 13, TOP - 11, Z - 2, Z, BRASS)
m.box(-X + PW + 2, X - PW - 2, 10, TOP - 18, -Z + 2, -Z + 6, OAK_DK)                               # modesty panel at the back of the kneehole
m.box(-X, X, TOP - 6, TOP - 2, -Z, Z, OAK_DK)                                                      # top: a darker apron band, then the top
m.box(-X - 2, X + 2, TOP - 2, TOP, -Z - 2, Z + 2, OAK)
m.box(-X + 30, X - 30, TOP, TOP + 0.6, -Z + 8, Z - 10, LEATHER)                                    # leather blotter
for dx, dz, a in ((-10, -2, 0.08), (-6, 1, -0.05), (-8, -1, 0.0)):                                 # a few loose sheets on it
    m.box(-12, 12, 0, 0.5, -16, 16, PAPER, ang=a, at=(dx, TOP + 0.6, dz))
lx, lz = X - 22, -Z + 16                                                                           # banker's lamp at the back corner
m.box(lx - 9, lx + 9, TOP, TOP + 2, lz - 6, lz + 6, BRASS)
m.cyl(lx, lz, TOP + 2, TOP + 24, 1.5, BRASS, 6)
m.box(lx - 15, lx + 15, TOP + 22, TOP + 29, lz - 4, lz + 9, GREEN)
m.box(lx - 13, lx + 13, TOP + 20, TOP + 22, lz - 2, lz + 7, (240, 230, 160))
for k, (c, h) in enumerate([((120, 30, 30), 5), ((50, 56, 62), 6)]):                               # two books by the lamp
    y0 = TOP + sum(hh for _, hh in [((0,), 5), ((0,), 6)][:k])
    m.box(lx - 40 + k, lx - 18 - k, y0, y0 + h, lz - 9 + k, lz + 9 - k, c)
m.cyl(-X + 24, -Z + 18, TOP, TOP + 6, 4.5, INK, 8)                                                 # inkwell and a pen
m.cyl(-X + 24, -Z + 18, TOP + 6, TOP + 7.5, 2.5, BRASS, 8)
m.box(-10, 10, 0, 1, -0.6, 0.6, INK, ang=0.4, at=(-X + 38, TOP + 0.3, -Z + 24))
print('oakdesk: %d triangles' % m.write(os.path.join(OUT, 'oakdesk.obj'), 'OakDesk', 'oakdesk.py: pedestals, drawers, kneehole, blotter, lamp, books, inkwell'))
