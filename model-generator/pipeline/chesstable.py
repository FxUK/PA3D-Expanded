r"""Chess table (user 9 Nov: "retexture chess" - the sprite smeared over the top): a small wooden table with a real 8 x 8 board and pieces.
usage: python -I chesstable.py SHAPES [OUT]      (SHAPES has objects-maps.png; writes OUT\d_chesstable.obj)"""
import sys, os, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from objlib import Model
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
m = Model(SH)
WOOD, WOOD_DK, SQ_L, SQ_D, PC_W, PC_B, METAL = (176, 120, 64), (98, 62, 30), (232, 220, 190), (70, 52, 36), (240, 238, 230), (34, 32, 34), (150, 152, 158)
S, TOP = 34.0, 72.0
for sx in (-1, 1):                     # legs
    for sz in (-1, 1): m.box(sx * (S - 4) - 2.5, sx * (S - 4) + 2.5, 0, TOP - 4, sz * (S - 4) - 2.5, sz * (S - 4) + 2.5, METAL)
m.box(-S, S, TOP - 4, TOP, -S, S, WOOD_DK)                       # table top
m.box(-S + 3, S - 3, TOP, TOP + 0.8, -S + 3, S - 3, WOOD)        # board frame
B = 26.0; q = 2 * B / 8
for i in range(8):                                               # the squares
    for j in range(8):
        m.box(-B + i * q, -B + (i + 1) * q, TOP + 0.8, TOP + 1.2, -B + j * q, -B + (j + 1) * q, SQ_L if (i + j) % 2 == 0 else SQ_D, top_only=True)
random.seed(11)                                                  # pieces: a game in progress, white near +z, black near -z
def piece(i, j, c, tall):
    cx, cz = -B + (i + 0.5) * q, -B + (j + 0.5) * q; r = q * 0.28
    m.box(cx - r * 1.2, cx + r * 1.2, TOP + 1.2, TOP + 2.4, cz - r * 1.2, cz + r * 1.2, c)        # base
    m.box(cx - r * 0.7, cx + r * 0.7, TOP + 2.4, TOP + 2.4 + tall, cz - r * 0.7, cz + r * 0.7, c)  # body
    m.box(cx - r, cx + r, TOP + 2.4 + tall, TOP + 3.6 + tall, cz - r, cz + r, c)                  # head
used = set()
for c, rows in ((PC_W, (6, 7)), (PC_B, (0, 1))):
    for k in range(9):
        i, j = random.randrange(8), random.choice(rows)
        if random.random() < 0.3: j = random.randrange(2, 6)
        if (i, j) in used: continue
        used.add((i, j)); piece(i, j, c, random.choice((3.0, 4.5, 6.5)))
print('d_chesstable: %d triangles' % m.write(os.path.join(OUT, 'd_chesstable.obj'), 'd_ChessTable', 'chesstable.py: table, 8 x 8 board, pieces'))
