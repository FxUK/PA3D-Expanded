import sys, math, numpy as np
from PIL import Image, ImageDraw
def load(path):
    V,T,N,F=[],[],[],[]
    for l in open(path,encoding="latin1"):
        p=l.split()
        if not p: continue
        if p[0]=="v": V.append(list(map(float,p[1:4])))
        elif p[0]=="vt": T.append(list(map(float,p[1:3])))
        elif p[0]=="vn": N.append(list(map(float,p[1:4])))
        elif p[0]=="f":
            idx=[]
            for a in p[1:]:
                q=a.split("/"); idx.append((int(q[0])-1,int(q[1])-1 if len(q)>1 and q[1] else 0,int(q[2])-1 if len(q)>2 and q[2] else -1))
            for i in range(1,len(idx)-1): F.append((idx[0],idx[i],idx[i+1]))
    return np.array(V),np.array(T) if T else np.zeros((1,2)),np.array(N) if N else None,F
def render(path,atlas,size=170,yaw=45,pitch=35.264):
    V,T,N,F=load(path)
    y,p=math.radians(yaw),math.radians(pitch)
    Ry=np.array([[math.cos(y),0,math.sin(y)],[0,1,0],[-math.sin(y),0,math.cos(y)]]); Rx=np.array([[1,0,0],[0,math.cos(p),-math.sin(p)],[0,math.sin(p),math.cos(p)]])
    P=V@Ry.T@Rx.T; ctr=(P[:,:2].min(0)+P[:,:2].max(0))/2; span=max(1.0,float((P[:,:2].max(0)-P[:,:2].min(0)).max()))*1.15
    S=(P[:,:2]-ctr)/span*size+size/2; S[:,1]=size-S[:,1]
    img=np.full((size,size,3),48,np.float32); zb=np.full((size,size),-1e9)
    A=np.asarray(atlas,np.float32); AH,AW=A.shape[:2]; L=np.array([0.4,0.8,0.45]); L/=np.linalg.norm(L)
    for (a,ta,na),(b,tb,nb),(c,tc,nc) in F:
        pa,pb,pc=S[a],S[b],S[c]
        x0,x1=int(max(0,min(pa[0],pb[0],pc[0]))),int(min(size-1,max(pa[0],pb[0],pc[0]))+1); y0,y1=int(max(0,min(pa[1],pb[1],pc[1]))),int(min(size-1,max(pa[1],pb[1],pc[1]))+1)
        if x1<=x0 or y1<=y0: continue
        xs,ys=np.meshgrid(np.arange(x0,x1)+0.5,np.arange(y0,y1)+0.5)
        d=(pb[0]-pa[0])*(pc[1]-pa[1])-(pc[0]-pa[0])*(pb[1]-pa[1])
        if abs(d)<1e-9: continue
        w1=((xs-pa[0])*(pc[1]-pa[1])-(pc[0]-pa[0])*(ys-pa[1]))/d; w2=((pb[0]-pa[0])*(ys-pa[1])-(xs-pa[0])*(pb[1]-pa[1]))/d; w0=1-w1-w2
        m=(w0>=-1e-4)&(w1>=-1e-4)&(w2>=-1e-4)
        if not m.any(): continue
        z=w0*P[a,2]+w1*P[b,2]+w2*P[c,2]; uv=w0[...,None]*T[ta]+w1[...,None]*T[tb]+w2[...,None]*T[tc]
        if N is not None and na>=0:
            n=w0[...,None]*N[na]+w1[...,None]*N[nb]+w2[...,None]*N[nc]; n=n/np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-9); shade=0.5+0.5*np.clip(n@L,0,1)
        else:
            n=np.cross(V[b]-V[a],V[c]-V[a]); n=n/(np.linalg.norm(n)+1e-9); shade=0.5+0.5*max(0.0,float(n@L))
        tx=np.clip((uv[...,0]*AW).astype(int),0,AW-1); ty=np.clip(((1-uv[...,1])*AH).astype(int),0,AH-1)
        col=A[ty,tx]*(shade[...,None] if np.ndim(shade) else shade)
        sub=zb[y0:y1,x0:x1]; upd=m&(z>sub); sub[upd]=z[upd]; img[y0:y1,x0:x1][upd]=col[upd]
    return Image.fromarray(np.clip(img,0,255).astype(np.uint8))
shapes=sys.argv[1]; atlas=Image.open(shapes+r"\objects-maps.png").convert("RGB"); views=[(0,89)]; names=sys.argv[3:]; sz=200; cols=1
rows=1; sheet=Image.new("RGB",(len(names)*sz,sz),(30,30,30)); dr=ImageDraw.Draw(sheet)
import os
for i,n in enumerate(names):
    p=os.path.join(shapes,n+".obj")
    if os.path.exists(p):
        sheet.paste(render(p,atlas,sz,0,89),(i*sz,0)); dr.text((i*sz+4,sz-13),n,fill=(255,255,120))
sheet.save(sys.argv[2])
