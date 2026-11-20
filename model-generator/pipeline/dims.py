import sys, os
for f in sys.argv[2:]:
    p = os.path.join(sys.argv[1], f + '.obj')
    if not os.path.exists(p): print(f, 'missing'); continue
    V = [list(map(float, l.split()[1:4])) for l in open(p, encoding='latin1') if l.startswith('v ')]
    print(f, 'x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f' % (min(v[0] for v in V), max(v[0] for v in V), min(v[1] for v in V), max(v[1] for v in V), min(v[2] for v in V), max(v[2] for v in V)))
