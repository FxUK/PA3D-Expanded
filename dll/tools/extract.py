import re, sys
t = open(sys.argv[1], encoding='utf-8', errors='replace').read()
for nm in ('kFloorVS', 'kFloorFS'):
    m = re.search(nm + r'\s*=\s*R"GLSL\((.*?)\)GLSL"', t, re.S); open(sys.argv[2] + '\\' + nm + '.glsl', 'w').write(m.group(1))
print('extracted')
