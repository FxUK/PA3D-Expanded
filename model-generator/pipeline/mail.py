r"""Stacks of mail (user 9 Nov: "low qual" - the mail room's sorted / unsorted mail were flat cut-outs: the game has the sprites StackMailSorted /
StackMailUnsorted but no shape files). Sorted = tidy bundles of envelopes tied with a red band; unsorted = a loose heap. b / c = smaller stacks.
usage: python -I mail.py SHAPES [OUT]      (SHAPES has objects-maps.png)"""
import sys, os, random, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from objlib import Model
SH = sys.argv[1]; OUT = sys.argv[2] if len(sys.argv) > 2 else SH
PAPER = [(240, 236, 222), (226, 214, 180), (210, 180, 120), (236, 226, 200), (200, 214, 230)]
BAND = (190, 40, 40)
def build(name, bundles, loose, seed):
    random.seed(seed)
    m = Model(SH)
    env = lambda cx, cy, cz, ang, c: m.box(-12, 12, 0, 1.6, -6.5, 6.5, c, ang=ang, at=(cx, cy, cz))      # one envelope
    for (px, pz, n) in bundles:      # a bundle: n envelopes squared up, a band round the middle
        y = 0.0; ang = random.uniform(-0.15, 0.15)
        for i in range(n):
            env(px + random.uniform(-0.6, 0.6), y, pz + random.uniform(-0.6, 0.6), ang + random.uniform(-0.03, 0.03), random.choice(PAPER)); y += 1.6
        m.box(-1.5, 1.5, -0.1, y + 0.3, -6.8, 6.8, BAND, ang=ang, at=(px, 0.0, pz))
    for k in range(loose):           # loose envelopes heaped up
        r = random.uniform(0, 22); a = random.uniform(0, 6.283)
        y = max(0.0, 14 - r * 0.6) * random.uniform(0.3, 1.0)
        env(math.cos(a) * r, y, math.sin(a) * r, random.uniform(0, 3.14), random.choice(PAPER))
    print('%s: %d triangles' % (name, m.write(os.path.join(OUT, name + '.obj'), name, 'mail.py')))
build('stackmailsorted', [(-14, -9, 9), (14, -9, 8), (-14, 9, 8), (14, 9, 9), (0, 0, 5)], 0, 1)
build('stackmailsortedb', [(-14, -6, 7), (14, -6, 8), (0, 9, 6)], 0, 2)
build('stackmailsortedc', [(-8, 0, 5), (10, 2, 4)], 0, 3)
build('stackmailunsorted', [], 34, 4)
build('stackmailunsortedb', [], 20, 5)
build('stackmailunsortedc', [], 10, 6)
