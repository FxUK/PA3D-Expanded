# List entries of a RAR4 archive (e.g. Prison Architect main.dat) by walking its headers.
import struct,sys
def ls(p):
    data=open(p,'rb').read(); pos=7; out=[]
    while pos+7<=len(data):
        crc,typ,flags,size=struct.unpack_from('<HBHH',data,pos)
        if typ==0x74:
            pack,unp,host,fcrc,ftime,ver,meth,nlen,attr=struct.unpack_from('<IIBIIBBHI',data,pos+7)
            off=pos+32
            if flags&0x100: hp,hu=struct.unpack_from('<II',data,off); off+=8; pack|=hp<<32; unp|=hu<<32
            name=data[off:off+nlen].split(b'\0')[0].decode('latin1')
            out.append((name,unp,meth)); pos+=size+pack
        elif typ==0x7b: break
        else:
            add=struct.unpack_from('<I',data,pos+7)[0] if flags&0x8000 else 0
            pos+=size+add
    return out
if __name__=='__main__':
    for n,u,m in ls(sys.argv[1]): print(u,n)
