r"""Serving table food: move the triangles of the food in the pans to the END of servingtable.obj and note how many there are, so the
DLL can leave them out (draw the first count - 3 * N vertices) when the table holds no food in the game.
usage: python -I foodtail.py SHAPES_DIR      (servingtable.obj and objects-maps.png in it; rewritten in place, the old file kept as .bak)
The food is found by colour (the swatches b16_serving_table in make_models.py paints it with)."""
import sys, os, shutil
from PIL import Image
SH = sys.argv[1]; fn = os.path.join(SH, 'servingtable.obj')
FOOD = [(236, 226, 196), (220, 206, 170), (96, 156, 58), (70, 128, 44), (150, 86, 52), (118, 64, 38), (206, 112, 50), (180, 90, 40)]
A = Image.open(os.path.join(SH, 'objects-maps.png')).convert('RGB'); AW, AH = A.size; P = A.load()
lines = open(fn, encoding='latin1').read().splitlines()
lines = [l for l in lines if not l.startswith('# PA3aiD food tail')]
T = [tuple(map(float, l.split()[1:3])) for l in lines if l.startswith('vt ')]
def is_food(fl):
    t = int(fl.split()[1].split('/')[1]) - 1
    u, v = T[t]; c = P[min(AW - 1, int((u % 1) * AW)), min(AH - 1, int(((1 - v) % 1) * AH))]
    return any(sum(abs(a - b) for a, b in zip(c, f)) < 24 for f in FOOD)
faces = [l for l in lines if l.startswith('f ')]
if any(len(f.split()) != 4 for f in faces): sys.exit('not all triangles')
rest = [l for l in lines if not l.startswith('f ')]
food = [f for f in faces if is_food(f)]; other = [f for f in faces if not is_food(f)]
shutil.copy(fn, fn + '.bak')
open(fn, 'w', encoding='latin1').write('\n'.join(['# PA3aiD food tail %d' % len(food)] + rest + other + food) + '\n')
print('servingtable: %d triangles, %d of them food (now last)' % (len(faces), len(food)))
