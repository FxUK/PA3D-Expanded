import sys
old=sys.argv[1]; out=sys.argv[2]
src=open(old+r"\full\data\shapes\WaterBoiler.obj",encoding="latin1").read().split("\n")
def write(name):
    o=[]
    for l in src:
        if l.startswith("v ") or l.startswith("vn "):
            p=l.split(); x,y,z=map(float,p[1:4])
            if p[0]=="v": x,y,z=(z+30.0)*0.8,y*0.8,-x*0.8     # long axis onto x (the 2x1 footprint), centred, scaled to fit
            else: x,y,z=z,y,-x
            l="%s %.5f %.5f %.5f"%(p[0],x,y,z)
        o.append(l.replace("g WaterBoiler","g "+name))
    open(out+r"\data\shapes\%s.obj"%name,"w",encoding="latin1").write("\n".join(o))
write("d_waterboiler"); write("d_waterboiler_lagged")
print("boiler written")
