"""Hash the code and data sections of a PE file (ignores the timestamp and checksum), to prove a refactor did not change the build.
usage: python -I pehash.py file.dll [file2.dll]"""
import sys, struct, hashlib
def sections(path):
    b = open(path, 'rb').read(); pe = struct.unpack_from('<I', b, 0x3c)[0]; n = struct.unpack_from('<H', b, pe + 6)[0]; opt = struct.unpack_from('<H', b, pe + 20)[0]
    out = {}
    for i in range(n):
        s = pe + 24 + opt + i * 40; name = b[s:s + 8].rstrip(b'\0').decode(); vsz, va, rsz, raw = struct.unpack_from('<IIII', b, s + 8)
        if name in ('.text', '.rdata', '.data'): out[name] = hashlib.sha256(b[raw:raw + min(vsz, rsz)]).hexdigest()[:16]
    return out
a = sections(sys.argv[1]); print(sys.argv[1], a)
if len(sys.argv) > 2:
    c = sections(sys.argv[2]); print(sys.argv[2], c); print('IDENTICAL' if a == c else 'DIFFERENT: ' + ', '.join(k for k in a if a.get(k) != c.get(k)))
