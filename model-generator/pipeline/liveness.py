"""Which make_models.py functions actually shape the output? Runs make_models under a profiler and reports, for each function:
called or not, and for mesh generators whether any file they made survived (later batches overwrite earlier ones).
usage: python -I liveness.py DATA OUT  -> OUT/liveness.json + a summary"""
import sys, os, json, runpy, collections
MM = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'make_models.py'); MM = os.path.normcase(os.path.abspath(MM))
called = collections.Counter(); edges = set(); creators = {}; writes = []      # writes: (file, creators of the mesh)
def mm_frames(f):
    out = []
    while f is not None:
        if os.path.normcase(f.f_code.co_filename) == MM and f.f_code.co_name not in ('<module>', '__init__', 'quad', 'write'): out.append(f.f_code.co_name)
        f = f.f_back
    return out
def prof(frame, event, arg):
    if event != 'call': return
    co = frame.f_code
    if os.path.normcase(co.co_filename) != MM: return
    called[co.co_name] += 1
    b = frame.f_back
    if b is not None and os.path.normcase(b.f_code.co_filename) == MM: edges.add((b.f_code.co_name, co.co_name))
    if co.co_name == '__init__' and 's' in frame.f_locals: creators[id(frame.f_locals['s'])] = mm_frames(frame.f_back)
    elif co.co_name == 'write' and 'path' in frame.f_locals and 's' in frame.f_locals:
        writes.append((os.path.basename(frame.f_locals['path']).lower(), creators.get(id(frame.f_locals['s']), [])))
sys.argv = [MM] + sys.argv[1:3]; out = sys.argv[2]
sys.setprofile(prof)
try: runpy.run_path(MM, run_name='__main__')
finally: sys.setprofile(None)
src = open(MM, encoding='utf-8').read().split('\n'); defs = [l[4:].split('(')[0] for l in src if l.startswith('def ')]
final = {}
for f, cr in writes: final[f] = cr
live_gen = set(); all_gen = set()
for f, cr in writes: all_gen.update(cr)
for f, cr in final.items(): live_gen.update(cr)
# functions reachable from live generators or from the module body (callers not in defs = top level)
reach = set(live_gen)      # everything a surviving generator uses
changed = True
while changed:
    changed = False
    for a, c in edges:
        if a in reach and c not in reach: reach.add(c); changed = True
never = [d for d in defs if called[d] == 0]
overwritten = sorted(g for g in all_gen - live_gen if g in defs and g not in reach)
json.dump({'writes': writes, 'edges': sorted(edges), 'never_called': never, 'only_overwritten': overwritten, 'files': len(final)}, open(os.path.join(out, 'liveness.json'), 'w'), indent=1)
print('defs %d, never called %d, generators whose every file was overwritten %d' % (len(defs), len(never), len(overwritten)))
print('never:', ', '.join(never)); print('overwritten:', ', '.join(overwritten))
