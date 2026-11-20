r"""Boxing ring (user 9 Nov: "not good texturing" - the sprite stretched over the top looked blurred): built from clean parts in plain colours.
usage: python -I ring.py SHAPES [OUT]      (SHAPES has objects-maps.png; writes OUT\d_boxingring.obj, same footprint as before: x +-207, z +-242)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from objlib import Model
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
m = Model(SH)
SKIRT, SKIRT_DK, CANVAS, CANVAS_LT, LINE = (40, 70, 150), (22, 36, 80), (92, 140, 210), (120, 165, 228), (235, 238, 242)
POST, PAD_R, PAD_B, STEEL = (70, 72, 78), (190, 40, 40), (40, 60, 170), (170, 174, 182)
ROPE = [(200, 36, 36), (240, 240, 240), (40, 60, 180)]
X, Z, P = 207.0, 242.0, 34.0      # half sizes; platform height
# platform: a dark base, the blue apron skirt, a white edge band, then the canvas with a lighter square
m.box(-X, X, 0, 4, -Z, Z, SKIRT_DK)
m.box(-X + 2, X - 2, 4, P - 4, -Z + 2, Z - 2, SKIRT)
m.box(-X, X, P - 4, P, -Z, Z, LINE)
m.box(-X + 8, X - 8, P, P + 1.5, -Z + 8, Z - 8, CANVAS)
m.box(-X + 40, X - 40, P + 1.5, P + 2, -Z + 40, Z - 40, CANVAS_LT)
# corner posts with padded turnbuckle covers (red / blue corners as in the sprite), steel caps
TOP = 130.0; pr = 7.0
for i, (sx, sz) in enumerate([(-1, -1), (1, -1), (1, 1), (-1, 1)]):
    cx, cz = sx * (X - 12), sz * (Z - 12)
    m.box(cx - pr, cx + pr, P, TOP, cz - pr, cz + pr, POST)
    m.box(cx - pr - 2, cx + pr + 2, TOP, TOP + 4, cz - pr - 2, cz + pr + 2, STEEL)
    m.box(cx - pr - 4, cx + pr + 4, P + 22, TOP - 6, cz - pr - 4, cz + pr + 4, PAD_R if i % 2 == 0 else PAD_B)
# three ropes on each side, between the posts
for k, h in enumerate([P + 30, P + 58, P + 86]):
    r = 2.6
    for sz in (-1, 1): m.box(-X + 12, X - 12, h - r, h + r, sz * (Z - 12) - r, sz * (Z - 12) + r, ROPE[k], True)
    for sx in (-1, 1): m.box(sx * (X - 12) - r, sx * (X - 12) + r, h - r, h + r, -Z + 12, Z - 12, ROPE[k], True)
print('d_boxingring: %d triangles' % m.write(os.path.join(OUT, 'd_boxingring.obj'), 'd_BoxingRing', 'ring.py: platform, canvas, padded posts, three ropes a side'))
