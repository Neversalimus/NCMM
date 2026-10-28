"""Offline iron-sight calibration from actual pinned mesh apertures, not scope sockets."""
import argparse,json
from pathlib import Path
from import_held_items import Inputs,prepare,trs,multiply,transform,inverse,require

FAMILIES={'pistol','revolver','smg','assault','battle','pump','auto_shotgun'}

def measure(inputs,profile):
    data=prepare(inputs,profile);globals_=[None]*len(data['nodes'])
    for i in data['order']:
        node=data['nodes'][i];local=trs(node.get('translation',[0,0,0]),node.get('rotation',[0,0,0,1]),node.get('scale',[1,1,1]))
        globals_[i]=multiply(globals_[node['parent']],local) if node['parent']>=0 else local
    skin=[multiply(globals_[j],m) for j,m in zip(data['joints'],data['inverse_bind'])];points=[]
    for vertex in data['vertices']:
        joints,weights=data['palettes'][vertex[3]]
        points.append(tuple(sum(transform(skin[j],vertex[0])[axis]*weight for j,weight in zip(joints,weights)) for axis in range(3)))
    node,point=data['contacts']['sight'];old=transform(globals_[node],point);x=old[0];intervals=[]
    # This is the existing .67 m gun grip, -.24 m ADS offset and .08 m
    # clipping plane expressed in the normalized author model's coordinates.
    near=-.35
    for at in range(0,len(data['indices']),3):
        polygon=[points[i] for i in data['indices'][at:at+3]];clipped=[]
        for a,b in zip(polygon,polygon[1:]+polygon[:1]):
            if a[2]>=near:clipped.append(a)
            if (a[2]>=near)!=(b[2]>=near):
                t=(near-a[2])/(b[2]-a[2]);clipped.append(tuple(a[k]+t*(b[k]-a[k]) for k in range(3)))
        if len(clipped)<3:continue
        a,b,c=clipped[:3]
        if abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))<1e-12:continue
        ys=[]
        for a,b in zip(clipped,clipped[1:]+clipped[:1]):
            if min(a[0],b[0])<=x<=max(a[0],b[0]) and abs(b[0]-a[0])>1e-10:
                t=(x-a[0])/(b[0]-a[0]);ys.append(a[1]+t*(b[1]-a[1]))
        if len(ys)>=2:intervals.append((min(ys),max(ys)))
    merged=[]
    for lo,hi in sorted(intervals):
        if merged and lo<=merged[-1][1]+1e-6:merged[-1]=(merged[-1][0],max(hi,merged[-1][1]))
        else:merged.append((lo,hi))
    require(merged,'No solid sight geometry')
    windows=[(a[1],b[0]) for a,b in zip(merged,merged[1:]) if b[0]-a[1]>.001 and a[1]>.02]
    # Closed hoods: centre the actual free ray band above the front post.
    # Open sights/beads: the reticle sits 1 mm above the post, never in the receiver.
    height=sum(windows[-1])*.5 if windows else merged[-1][1]+.001
    world=transform(inverse(data['root_matrix']),(old[0],height,old[2]))
    return dict(id=profile['id'],height=height,previous_height=old[1],world=world,windows=windows)

def calibrate(path,write=False):
    selection=json.loads(path.read_text(encoding='utf-8'));inputs=Inputs(path.parent,selection);report=[]
    for profile in selection['models']:
        if profile['family'] not in FAMILIES:continue
        result=measure(inputs,profile);report.append(result)
        if write:profile['contacts']['sight']=list(result['world'])
        else:require(abs(result['height']-result['previous_height'])<.0001,'Iron sight does not follow mesh aperture: '+profile['id'])
    if write:path.write_text(json.dumps(selection,indent=2)+'\n',encoding='utf-8',newline='\n')
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--selection',type=Path,required=True)
    parser.add_argument('--write',action='store_true');args=parser.parse_args()
    print(json.dumps(calibrate(args.selection,args.write),indent=2))
