import ctypes,os,sys
la=ctypes.CDLL('libarchive.so.13')
la.archive_read_new.restype=ctypes.c_void_p
la.archive_entry_pathname.restype=ctypes.c_char_p
la.archive_entry_pathname.argtypes=[ctypes.c_void_p]
for f in ['archive_read_support_format_all','archive_read_support_filter_all']: getattr(la,f).argtypes=[ctypes.c_void_p]
la.archive_read_open_filename.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_size_t]
la.archive_read_next_header.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
la.archive_read_data.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t]; la.archive_read_data.restype=ctypes.c_ssize_t
la.archive_error_string.restype=ctypes.c_char_p; la.archive_error_string.argtypes=[ctypes.c_void_p]
src,dst=sys.argv[1],sys.argv[2]
a=la.archive_read_new(); la.archive_read_support_format_all(a); la.archive_read_support_filter_all(a)
assert la.archive_read_open_filename(a,src.encode(),1<<20)==0, la.archive_error_string(a)
e=ctypes.c_void_p(); buf=ctypes.create_string_buffer(1<<20); n=0
while True:
    r=la.archive_read_next_header(a,ctypes.byref(e))
    if r==1: break
    if r<0: print('ERR',la.archive_error_string(a)); break
    p=la.archive_entry_pathname(e).decode('latin1').replace('\\','/')
    out=os.path.join(dst,p); os.makedirs(os.path.dirname(out),exist_ok=True)
    with open(out,'wb') as fo:
        while True:
            k=la.archive_read_data(a,buf,len(buf))
            if k<0: print('DERR',p,la.archive_error_string(a)); break
            if k==0: break
            fo.write(buf.raw[:k])
    n+=1
print('extracted',n)
