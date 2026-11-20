"""pamem - read the running Prison Architect's memory (read-only) to find object layouts without a rebuild / play cycle.

usage (game running, a prison loaded):
  python -I pamem.py prisoners [N]        list N prisoner objects (vtable exe+0xB09478): category, naked, and where their Bio is
  python -I pamem.py bio [N]              dump N prisoners' Bio objects (vtable exe+0xAC0508): names, head, body, scale, skin
  python -I pamem.py find-vt RVA [N]      list objects whose first qword is exe+RVA (hex)
  python -I pamem.py dump ADDR [LEN]      hex dump at an absolute address (hex)
  python -I pamem.py refs ADDR [N]        find qwords in writable memory pointing at ADDR (hex)
  python -I pamem.py regime               game day / time and each prisoner category's 24-hour schedule (1 = Sleep)
  python -I pamem.py rooms                room types in use (index, type number, name)
Only reads memory (PROCESS_VM_READ | PROCESS_QUERY_INFORMATION); never writes."""
import sys, ctypes, ctypes.wintypes as wt, struct
k32 = ctypes.WinDLL('kernel32', use_last_error=True)
PROCESS_QUERY_INFORMATION, PROCESS_VM_READ = 0x0400, 0x0010
MEM_COMMIT, PAGE_GUARD, PAGE_NOACCESS = 0x1000, 0x100, 0x01
WRITABLE = {0x04, 0x08, 0x40, 0x80}      # READWRITE, WRITECOPY, EXECUTE_READWRITE, EXECUTE_WRITECOPY
class MBI(ctypes.Structure):
    _fields_ = [('BaseAddress', ctypes.c_uint64), ('AllocationBase', ctypes.c_uint64), ('AllocationProtect', wt.DWORD), ('PartitionId', wt.WORD),
                ('RegionSize', ctypes.c_uint64), ('State', wt.DWORD), ('Protect', wt.DWORD), ('Type', wt.DWORD)]
class ME32(ctypes.Structure):
    _fields_ = [('dwSize', wt.DWORD), ('th32ModuleID', wt.DWORD), ('th32ProcessID', wt.DWORD), ('GlblcntUsage', wt.DWORD), ('ProccntUsage', wt.DWORD),
                ('modBaseAddr', ctypes.c_void_p), ('modBaseSize', wt.DWORD), ('hModule', wt.HMODULE), ('szModule', ctypes.c_char * 256), ('szExePath', ctypes.c_char * 260)]
class PE32(ctypes.Structure):
    _fields_ = [('dwSize', wt.DWORD), ('cntUsage', wt.DWORD), ('th32ProcessID', wt.DWORD), ('th32DefaultHeapID', ctypes.c_void_p), ('th32ModuleID', wt.DWORD),
                ('cntThreads', wt.DWORD), ('th32ParentProcessID', wt.DWORD), ('pcPriClassBase', ctypes.c_long), ('dwFlags', wt.DWORD), ('szExeFile', ctypes.c_char * 260)]
k32.CreateToolhelp32Snapshot.restype = wt.HANDLE; k32.OpenProcess.restype = wt.HANDLE
k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
k32.VirtualQueryEx.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(MBI), ctypes.c_size_t]
def find_game():
    snap = k32.CreateToolhelp32Snapshot(0x2, 0); pe = PE32(); pe.dwSize = ctypes.sizeof(PE32); pid = None
    ok = k32.Process32First(snap, ctypes.byref(pe))
    while ok:
        if pe.szExeFile.decode(errors='ignore').lower().startswith('prison architect'): pid = pe.th32ProcessID
        ok = k32.Process32Next(snap, ctypes.byref(pe))
    k32.CloseHandle(snap)
    if not pid: sys.exit('Prison Architect is not running')
    snap = k32.CreateToolhelp32Snapshot(0x8 | 0x10, pid); me = ME32(); me.dwSize = ctypes.sizeof(ME32); base = None
    ok = k32.Module32First(snap, ctypes.byref(me))
    while ok:
        if me.szModule.decode(errors='ignore').lower().startswith('prison architect'): base = me.modBaseAddr
        ok = k32.Module32Next(snap, ctypes.byref(me))
    k32.CloseHandle(snap)
    h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
    if not h: sys.exit('cannot open the game process (error %d)' % ctypes.get_last_error())
    return h, base
H, EXE = find_game()
def read(addr, n):
    buf = ctypes.create_string_buffer(n); got = ctypes.c_size_t(0)
    if not k32.ReadProcessMemory(H, addr, buf, n, ctypes.byref(got)) or got.value != n: return None
    return buf.raw
def q(addr): b = read(addr, 8); return struct.unpack('<Q', b)[0] if b else None
def regions(writable_only=True):
    a = 0; mbi = MBI()
    while a < 0x7fffffffffff and k32.VirtualQueryEx(H, a, ctypes.byref(mbi), ctypes.sizeof(mbi)):
        if mbi.State == MEM_COMMIT and not (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) and (not writable_only or (mbi.Protect & 0xff) in WRITABLE):
            yield mbi.BaseAddress, mbi.RegionSize
        a = mbi.BaseAddress + mbi.RegionSize
def scan_qword(value, limit=200, aligned=8):
    pat = struct.pack('<Q', value); out = []
    for base, size in regions():
        for off in range(0, size, 1 << 24):
            n = min(1 << 24, size - off); b = read(base + off, n)
            if not b: continue
            i = b.find(pat)
            while i >= 0:
                if (base + off + i) % aligned == 0:
                    out.append(base + off + i)
                    if len(out) >= limit: return out
                i = b.find(pat, i + 1)
    return out
def mstr(addr):      # MSVC std::string
    b = read(addr, 0x20)
    if not b: return '?'
    n, cap = struct.unpack('<QQ', b[0x10:0x20])
    if n > 64: return '?'
    if cap >= 16: p = struct.unpack('<Q', b[:8])[0]; s = read(p, n) or b''
    else: s = b[:n]
    return s.decode('latin1')
VT_PRISONER, VT_BIO = EXE + 0xB09478, EXE + 0xAC0508
def find_bio(obj):      # the Bio is built inline in the prisoner at +0xCB0 (prisoner constructor: lea rcx,[rsi+0xCB0]; call BioCtor)
    b = obj + 0xcb0; n = read(b + 0x28, 8)
    if not n or not (0 < struct.unpack('<Q', n)[0] <= 40): return None, None
    return 'inline +0xcb0', b
def hexdump(addr, n):
    b = read(addr, n)
    if not b: print('unreadable'); return
    for i in range(0, n, 16):
        row = b[i:i + 16]; print('%016x +%04x  %s  %s' % (addr + i, i, ' '.join('%02x' % c for c in row), ''.join(chr(c) if 32 <= c < 127 else '.' for c in row)))
cmd = sys.argv[1] if len(sys.argv) > 1 else 'prisoners'; arg = sys.argv[2:]
print('game exe at %x' % EXE)
if cmd == 'prisoners':
    for p in scan_qword(VT_PRISONER, int(arg[0]) if arg else 10):
        cat = struct.unpack('<i', read(p + 0xa34, 4))[0]; naked = read(p + 0xa4c, 1)[0]; where, b = find_bio(p)
        print('prisoner %x  category %d naked %d  Bio %s %s' % (p, cat, naked, where, '' if not b else '%x %s %s' % (b, mstr(b + 0x18), mstr(b + 0x38))))
elif cmd == 'bio':
    for p in scan_qword(VT_PRISONER, int(arg[0]) if arg else 6):
        where, b = find_bio(p)
        if not b: print('prisoner %x: no Bio found' % p); continue
        raw = read(b + 0x58, 0x30); head, body = struct.unpack('<ii', raw[0x10:0x18]); scale = struct.unpack('<f', raw[0x18:0x1c])[0]
        print('%s %s  (%s)  head %d body %d scale %.3f  +0x74: %s' % (mstr(b + 0x18), mstr(b + 0x38), where, head, body, scale, raw[0x1c:0x20].hex()))
        hexdump(b + 0x58, 0x50)
elif cmd == 'regime':      # world+0x80 time, world+0x5E8 schedule per prisoner category (1 = Sleep)
    world = q(q(EXE + 0xd57900) + 0x198); tm = struct.unpack('<d', read(world + 0x80, 8))[0]
    print('day %d %02d:%02d' % (tm // 1440 + 1, (tm % 1440) // 60, tm % 60)); tbl = q(world + 0x5e8)
    for cat in range(8):
        s = read(q(tbl + 8 * cat), 96); print('category %d: %s' % (cat, list(struct.unpack('<24i', s)) if s else '-'))
elif cmd == 'rooms':      # world+0x44A0 Room* by index: +0x34 index, +0x38 type, +0x40 name
    world = q(q(EXE + 0xd57900) + 0x198); arr = q(world + 0x44a0); import collections; cnt = collections.Counter()
    for k in range(int(arg[0]) if arg else 600):
        r = q(arr + 8 * k); b = read(r, 0x60) if r else None
        if not b or struct.unpack_from('<i', b, 0x34)[0] != k: continue
        cnt[(struct.unpack_from('<i', b, 0x38)[0], b[0x40:0x58].split(b'\0')[0].decode('latin1'))] += 1
    for (ty, nm), n in sorted(cnt.items()): print('type %2d %-20s x%d' % (ty, nm, n))
elif cmd == 'find-vt':
    for p in scan_qword(EXE + int(arg[0], 16), int(arg[1]) if len(arg) > 1 else 20): print('%x' % p)
elif cmd == 'links':      # objects whose vtable is exe+RVA: which offsets in their first 0x600 bytes point at a prisoner (vtable 0xB09478)?
    import collections; cnt = collections.Counter(); objs = scan_qword(EXE + int(arg[0], 16), int(arg[1]) if len(arg) > 1 else 200)
    for o in objs:
        b = read(o, 0x600)
        if not b: continue
        for off in range(0, 0x600, 8):
            p = struct.unpack_from('<Q', b, off)[0]
            if 0x10000 < p < 0x7fffffffffff and q(p) == VT_PRISONER: cnt[off] += 1
    print('%d objects with that vtable; offsets that point at a prisoner:' % len(objs))
    for off, n in cnt.most_common(10): print('  +0x%x  %d' % (off, n))
    if objs: print('first object %x; its +0x48/+0x4C floats: %s' % (objs[0], struct.unpack('<ff', read(objs[0] + 0x48, 8))))
elif cmd == 'find-pos':      # float pair X Y (an object's position) anywhere in writable memory
    pat = struct.pack('<ff', float(arg[0]), float(arg[1])); n = 0
    for base, size in regions():
        for off in range(0, size, 1 << 24):
            m = min(1 << 24, size - off); b = read(base + off, m)
            if not b: continue
            i = b.find(pat)
            while i >= 0 and n < 40:
                if (base + off + i) % 4 == 0: print('%x' % (base + off + i)); n += 1
                i = b.find(pat, i + 1)
elif cmd == 'dump': hexdump(int(arg[0], 16), int(arg[1], 16) if len(arg) > 1 else 0x100)
elif cmd == 'refs':
    for p in scan_qword(int(arg[0], 16), int(arg[1]) if len(arg) > 1 else 20): print('%x' % p)
else: print(__doc__)
