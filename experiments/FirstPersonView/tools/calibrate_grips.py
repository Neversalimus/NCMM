"""Offline contact on the real authored trigger surface, not its hinge pivot.

The original weighted mesh and animation nodes stay intact. World-space points
are converted back to the original source frame, so this check is idempotent
when the separately calibrated hand/weapon origin changes.
"""
import argparse
import json
import math
from pathlib import Path
from import_held_items import Inputs, inverse, multiply, prepare, transform, trs


def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))


def closest(p,a,b,c):
    ab,ac,ap=sub(b,a),sub(c,a),sub(p,a);d1,d2=dot(ab,ap),dot(ac,ap)
    normal=(ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0])
    if dot(normal,normal)<1e-20:
        def segment(x,y):
            v=sub(y,x);return add(x,mul(v,max(0,min(1,dot(sub(p,x),v)/max(1e-20,dot(v,v))))))
        return min((segment(a,b),segment(b,c),segment(c,a)),key=lambda q:dot(sub(q,p),sub(q,p)))
    if d1<=0 and d2<=0:return a
    bp=sub(p,b);d3,d4=dot(ab,bp),dot(ac,bp)
    if d3>=0 and d4<=d3:return b
    vc=d1*d4-d3*d2
    if vc<=0 and d1>=0 and d3<=0:return add(a,mul(ab,d1/(d1-d3)))
    cp=sub(p,c);d5,d6=dot(ab,cp),dot(ac,cp)
    if d6>=0 and d5<=d6:return c
    vb=d5*d2-d1*d6
    if vb<=0 and d2>=0 and d6<=0:return add(a,mul(ac,d2/(d2-d6)))
    va=d3*d6-d5*d4
    if va<=0 and d4-d3>=0 and d5-d6>=0:return add(b,mul(sub(c,b),(d4-d3)/((d4-d3)+(d5-d6))))
    denominator=va+vb+vc
    if abs(denominator)<1e-20:return min((a,b,c),key=lambda q:dot(sub(q,p),sub(q,p)))
    return add(a,add(mul(ab,vb/denominator),mul(ac,vc/denominator)))


def trigger_contact(data):
    node=data['trigger_node']
    if node<0:return None
    globals_=[None]*len(data['nodes'])
    for i in data['order']:
        n=data['nodes'][i];local=trs(n.get('translation',[0,0,0]),n.get('rotation',[0,0,0,1]),n.get('scale',[1,1,1]))
        globals_[i]=multiply(globals_[n['parent']],local) if n['parent']>=0 else local
    skin=[multiply(globals_[j],ib) for j,ib in zip(data['joints'],data['inverse_bind'])]
    points=[];weighted=[]
    for p,n,uv,palette,material in data['vertices']:
        js,ws=data['palettes'][palette]
        points.append(tuple(sum(transform(skin[j],p)[k]*w for j,w in zip(js,ws)) for k in range(3)))
        weighted.append(any(data['joints'][j]==node and w>.5 for j,w in zip(js,ws)))
    triangles=[tuple(points[i] for i in data['indices'][at:at+3]) for at in range(0,len(data['indices']),3)
               if all(weighted[i] for i in data['indices'][at:at+3])]
    if not triangles:raise ValueError('Authored trigger has no independently weighted surface')
    surface=[p for t in triangles for p in t]
    target=(max(p[0] for p in surface),max(p[1] for p in surface)-.016,max(p[2] for p in surface)-.002)
    candidates=[closest(target,*t) for t in triangles]
    point=min(candidates,key=lambda p:dot(sub(p,target),sub(p,target)))
    return dict(node=data['nodes'][node]['name'],world=list(transform(inverse(data['root_matrix']),point)))


def calibrate(path,check=False):
    selection=json.loads(path.read_text(encoding='utf-8'));inputs=Inputs(path.parent,selection);count=0
    for profile in selection['models']:
        if profile['family'] not in ['pistol','smg','assault','battle','pump','auto_shotgun']:continue
        expected=trigger_contact(prepare(inputs,profile))
        if expected is None:continue
        if check:
            actual=profile['contacts']['trigger']
            if not isinstance(actual,dict) or actual.get('node')!=expected['node'] or not isinstance(actual.get('world'),list) or len(actual['world'])!=3 or any(
                    not math.isfinite(a) or abs(a-b)>1e-7 for a,b in zip(actual['world'],expected['world'])):
                raise ValueError('Trigger surface calibration differs: '+profile['id'])
        else:profile['contacts']['trigger']=expected
        count+=1
    if not check:path.write_bytes((json.dumps(selection,indent=2,ensure_ascii=False)+'\n').encode('utf-8'))
    return count


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--selection',type=Path,required=True);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    print(json.dumps(dict(success=True,authored_trigger_surfaces=calibrate(args.selection,args.check),check=args.check)))
