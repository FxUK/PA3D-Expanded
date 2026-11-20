r"""Oval staff table (user 9 Nov: "bad texture" - the sprite smeared over it): a clean oval wooden top on two pedestals, 1 x 2 tiles.
usage: python -I ovaltable.py SHAPES [OUT]      (SHAPES has objects-maps.png; writes OUT\d_ovalstafftable.obj)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from objlib import Model
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
m = Model(SH)
WOOD, WOOD_DK, WOOD_LT = (176, 104, 56), (110, 62, 30), (196, 124, 70)
TOP = 76.0
m.ring(47, 95, TOP - 5, TOP, WOOD)                               # the oval top
m.ring(44, 92, TOP, TOP + 0.4, WOOD_LT)                          # a lighter inlay band inside the rim
m.ring(40, 88, TOP + 0.4, TOP + 0.6, WOOD)                       # the field
m.ring(43, 91, TOP - 8, TOP - 5, WOOD_DK, cap_top=False, cap_bottom=True)   # apron under the edge
for cz in (-50, 50):                                             # two pedestals with round feet
    m.ring(5, 5, 6, TOP - 8, WOOD_DK, 10, cz=cz)
    m.ring(22, 22, 0, 4, WOOD_DK, 16, cz=cz)
    m.ring(12, 12, 4, 6, WOOD_DK, 12, cz=cz)
print('d_ovalstafftable: %d triangles' % m.write(os.path.join(OUT, 'd_ovalstafftable.obj'), 'd_OvalStaffTable', 'ovaltable.py: oval top, two pedestals'))
