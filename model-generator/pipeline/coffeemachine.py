r"""Coffee machine (review 10 Nov: the 2D sprite was smeared over a round table): a small round wooden table carrying a cream filter coffee
machine with a glass jug of coffee, and two mugs. Footprint as before (about 0.9 tile across).
usage: python -I coffeemachine.py SHAPES [OUT]      (SHAPES has objects-maps.png; writes OUT\d_coffeemachine.obj)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from objlib import Model
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
m = Model(SH)
WOOD, WOOD_DK, CREAM, CREAM_DK, BLACK, GLASS, COFFEE, MUG_R, MUG_W, STEEL = (
    (150, 96, 50), (96, 60, 30), (226, 220, 196), (180, 172, 150), (40, 40, 44), (170, 196, 206), (60, 34, 20), (176, 40, 40), (236, 236, 232), (170, 172, 178))
TOP = 62.0
m.ring(44, 44, TOP - 4, TOP, WOOD, 24)                                   # round table top
m.ring(41, 41, TOP - 8, TOP - 4, WOOD_DK, 24, cap_top=False, cap_bottom=True)
for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):                      # four legs
    m.box(sx * 26 - 2.5, sx * 26 + 2.5, 0, TOP - 8, sz * 26 - 2.5, sz * 26 + 2.5, WOOD_DK)
# the machine, at the back: base plate, water tower, head over the jug
m.box(-14, 14, TOP, TOP + 3, -26, 6, CREAM_DK)
m.box(-14, 14, TOP + 3, TOP + 30, -26, -12, CREAM)
m.box(-14, 14, TOP + 24, TOP + 30, -12, 4, CREAM)
m.box(-6, 6, TOP + 22, TOP + 24, -6, 2, BLACK)                           # the filter outlet
m.box(8, 12, TOP + 18, TOP + 22, -11.5, -11, (200, 40, 40))              # a red power light
m.cyl(0, -2, TOP + 3, TOP + 4, 7.5, BLACK, 12)                           # hot plate
m.cyl(0, -2, TOP + 4, TOP + 11, 6.5, COFFEE, 12)                         # the jug: coffee in glass
m.cyl(0, -2, TOP + 11, TOP + 17, 6.5, GLASS, 12)
m.box(6, 11, TOP + 8, TOP + 15, -3.5, -0.5, BLACK)                       # its handle
for (cx, cz, c) in ((-26, 18, MUG_R), (22, 20, MUG_W)):                  # two mugs
    m.cyl(cx, cz, TOP, TOP + 7, 3.6, c, 10)
    m.box(cx + 3.4, cx + 5.4, TOP + 2, TOP + 5.5, cz - 0.8, cz + 0.8, c)
m.box(16, 28, TOP, TOP + 0.4, 4, 12, STEEL)                              # a spoon on the table
print('d_coffeemachine: %d triangles' % m.write(os.path.join(OUT, 'd_coffeemachine.obj'), 'd_CoffeeMachine', 'coffeemachine.py: round table, filter machine, jug, mugs'))
