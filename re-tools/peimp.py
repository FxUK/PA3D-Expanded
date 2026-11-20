import struct,sys
d=open(sys.argv[1],'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]; osz=struct.unpack_from('<H',d,pe+20)[0]
opt=pe+24; base=struct.unpack_from('<Q',d,opt+24)[0]
secs=[struct.unpack_from('<8sIIII',d,opt+osz+i*40) for i in range(nsec)]
def off(rva):
    for n,vs,va,rs,ro in secs:
        if va<=rva<va+max(vs,rs): return rva-va+ro
imp=struct.unpack_from('<I',d,opt+112+8)[0]
p=off(imp)
while True:
    olt,ts,fc,name,ft=struct.unpack_from('<IIIII',d,p); p+=20
    if not name: break
    dll=d[off(name):].split(b'\0')[0].decode()
    i=0
    while True:
        t=struct.unpack_from('<Q',d,off(olt or ft)+i*8)[0]
        if not t: break
        fn='#%d'%(t&0xffff) if t>>63 else d[off(t&0x7fffffff)+2:].split(b'\0')[0].decode()
        print(hex(base+ft+i*8),dll,fn); i+=1
