import sys, numpy as np
from PIL import Image
old=sys.argv[1]; out=sys.argv[2]
A=np.asarray(Image.open(out+r"\data\shapes\objects-maps.png").convert("RGB")).astype(int); H,W=A.shape[:2]
src=open(old+r"\full\data\shapes\warden.obj",encoding="latin1").read().split("\n")
remap={(115,99,87):(104,36,58),(66,57,50):(58,22,34),(80,88,98):(38,34,34),(77,19,0):(224,196,70)}   # maroon jacket, black hair, gold tie
def find(c):
    d=((A[:2048,:2048]-np.array(c))**2).sum(axis=2); y,x=np.unravel_index(np.argmin(d),d.shape); return (x+0.5)/W, 1-(y+0.5)/H
tg={k:find(v) for k,v in remap.items()}
o=[]
for l in src:
    if l.startswith("vt "):
        u,v=map(float,l.split()[1:3]); x=min(W-1,int(u*W)); y=min(H-1,int((1-v)*H)); c=tuple(int(q) for q in A[y,x])
        if c in tg: u,v=tg[c]; l="vt %.5f %.5f 0.0000"%(u,v)
    o.append(l.replace("g warden","g actor"))
open(out+r"\data\shapes\actor.obj","w",encoding="latin1").write("\n".join(o))
print("actor written")
