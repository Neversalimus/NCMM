"""Bounded offline conversion of the pinned CC0 world props; no runtime importer."""
import argparse,hashlib,json,math,re
from pathlib import Path
from import_held_items import Inputs,read_glb,require,hierarchy,trs,multiply,transform
from import_office_collada import read_collada

class WorldInputs(Inputs):
    def __init__(self,root,selection):
        self.root=root.resolve();self.files=selection['files'];self.used=set();self.textures={}
        require(selection['schema']==1 and selection['license']=='CC0-1.0','Unsupported prop selection')
        require(0<len(selection['models'])<=100 and len(self.files)<=256,'Unbounded prop inventory')
        for name,digest in self.files.items():
            require(hashlib.sha256(self.read(name)).hexdigest()==digest,'Pinned prop source mismatch: '+name)

def convert(root):
    selection=json.loads((root/'selection.json').read_text(encoding='utf-8'))
    inputs=WorldInputs(root,selection);models=[]
    for entry in selection['models']:
        name=entry['id'];require(re.fullmatch('[A-Za-z][A-Za-z0-9]{0,40}',name),'Invalid prop identifier')
        require(name not in [m['id'] for m in models],'Duplicate prop identifier')
        if entry['source'].endswith('.DAE'):
            triangles,bounds=read_collada(inputs,entry)
            models.append(dict(id=name,triangles=triangles,bounds=bounds));continue
        source=read_glb(inputs,entry['source']);require(len(source['indices'])<=24000,'Prop triangle budget exceeded')
        require(all(not m['texture'] for m in source['materials']),'Selected props require flat source materials')
        parents,order=hierarchy(source['nodes']);globals_=[None]*len(parents)
        for i in order:
            node=source['nodes'][i]
            require('matrix' not in node,'Selected prop needs a baked node matrix')
            local=trs(node.get('translation',[0,0,0]),node.get('rotation',[0,0,0,1]),node.get('scale',[1,1,1]))
            globals_[i]=multiply(globals_[parents[i]],local) if parents[i]>=0 else local
        skins=[multiply(globals_[j],m) for j,m in zip(source['joints'],source['inverse_bind'])]
        points=[]
        for vertex in source['vertices']:
            joints,weights=source['palettes'][vertex[3]]
            points.append(tuple(sum(transform(skins[j],vertex[0])[axis]*w for j,w in zip(joints,weights)) for axis in range(3)))
        low=[min(p[i] for p in points) for i in range(3)];high=[max(p[i] for p in points) for i in range(3)]
        require(all(math.isfinite(x) and abs(x)<100 for x in low+high),'Unbounded prop coordinates')
        require(high[1]-low[1]>.005,'Prop has no height')
        centre=[(low[i]+high[i])*.5 for i in range(3)]
        # GLB Y-up -> right-handed world Z-up. Foot stays on the physical tile.
        def point(v):return (v[0]-centre[0],-(v[2]-centre[2]),v[1]-low[1])
        triangles=[]
        for at in range(0,len(source['indices']),3):
            indices=source['indices'][at:at+3];vertices=[source['vertices'][i] for i in indices]
            colour=source['materials'][vertices[0][4]]['color']
            triangles.append(([point(points[i]) for i in indices],colour))
        models.append(dict(id=name,triangles=triangles,bounds=[high[0]-low[0],high[2]-low[2],high[1]-low[1]]))
    names=', '.join(m['id'] for m in models)
    header='#pragma once\n#include <cstddef>\nnamespace first_person::props {\n'
    header+='enum class id { '+names+', count };\n'
    header+='struct point {float x,y,z;};\nstruct triangle {point vertices[3];float color[4];};\n'
    header+='struct model {const triangle *triangles;size_t count;point dimensions;};\nconst model &get(id value) noexcept;\n}\n'
    def number(v):
        text=format(v,'.8g')
        return text+('f' if '.' in text or 'e' in text else '.0f')
    def numbers(v):return ','.join(number(x) for x in v)
    cpp='#include "world_props.generated.hpp"\nnamespace first_person::props {\n'
    for model in models:
        cpp+='static const triangle '+model['id']+'_triangles[]={\n'
        for points,colour in model['triangles']:
            cpp+='{{'+','.join('{'+numbers(p)+'}' for p in points)+'},{'+numbers(colour)+'}},\n'
        cpp+='};\n'
    cpp+='static const model models[]={\n'
    for m in models:cpp+='{'+m['id']+'_triangles,'+str(len(m['triangles']))+',{'+numbers(m['bounds'])+'}},\n'
    cpp+='};\nconst model &get(id value) noexcept {return models[static_cast<size_t>(value)<static_cast<size_t>(id::count)?static_cast<size_t>(value):0];}\n}\n'
    files={'world_props.generated.hpp':header.encode(),'world_props.generated.cpp':cpp.encode()}
    manifest=dict(schema=1,source_selection_sha256=hashlib.sha256((root/'selection.json').read_bytes()).hexdigest(),
        files={k:hashlib.sha256(v).hexdigest() for k,v in files.items()},
        models=[dict(id=m['id'],triangles=len(m['triangles']),dimensions=m['bounds']) for m in models])
    files['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    return files

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    files=convert(args.root);args.output.mkdir(parents=True,exist_ok=True)
    for name,data in files.items():(args.output/name).write_bytes(data)
    print('Converted pinned world props:',len(json.loads(files['manifest.json'])['models']))
