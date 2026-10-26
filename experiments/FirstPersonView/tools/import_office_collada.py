"""Bounded offline import of explicitly selected CC0 3TD visual nodes.

Never follows the exporter's file:// paths. Diffuse images are explicitly
remapped to pinned files and baked into face colours; collision meshes, source
lights and reflective environment maps are not game geometry/materials.
"""
import io,math,xml.etree.ElementTree as ET
from import_held_items import IDENTITY,multiply,transform,trs,require

NS={'c':'http://www.collada.org/2005/11/COLLADASchema'}

def read_collada(inputs,entry):
    data=inputs.read(entry['source'])
    require(len(data)<=2_000_000 and b'<!DOCTYPE' not in data and b'<!ENTITY' not in data,'Unsupported Collada XML')
    root=ET.fromstring(data)
    require(root.tag=='{'+NS['c']+'}COLLADA','Unsupported Collada namespace')
    require(not root.findall('c:library_animations/*',NS),'Collada animations must not be discarded')
    asset=root.find('c:asset',NS);require(asset is not None,'Missing Collada asset')
    require(asset.findtext('c:up_axis',namespaces=NS)=='Z_UP','Selected office source must be Z-up')
    meter=float(asset.find('c:unit',NS).get('meter','1'));require(math.isfinite(meter) and 0<meter<10,'Invalid Collada units')
    def floats(text,count=None):
        values=[float(v) for v in (text or '').split()]
        require(len(values)<=100_000 and (count is None or len(values)==count) and all(math.isfinite(v) and abs(v)<1e6 for v in values),'Invalid Collada values')
        return values
    effects={};images={}
    for name,path in entry.get('textures',{}).items():
        from PIL import Image
        with Image.open(io.BytesIO(inputs.read(path))) as image:
            require(image.format in ['JPEG','PNG'] and 0<image.width<=1024 and 0<image.height<=1024,'Unbounded office diffuse image')
            image.load();images[name]=image.convert('RGB')
    for effect in root.findall('c:library_effects/c:effect',NS):
        profile=effect.find('c:profile_COMMON',NS);require(profile is not None,'Unsupported Collada effect')
        diffuse=profile.find('c:technique/*/c:diffuse',NS);require(diffuse is not None,'Missing office diffuse')
        texture=diffuse.find('c:texture',NS)
        if texture is not None:
            sampler=next((p for p in profile.findall('c:newparam',NS) if p.get('sid')==texture.get('texture')),None)
            require(sampler is not None,'Missing office sampler')
            surface_name=sampler.findtext('c:sampler2D/c:source',namespaces=NS)
            surface=next((p for p in profile.findall('c:newparam',NS) if p.get('sid')==surface_name),None)
            require(surface is not None,'Missing office surface')
            image_name=surface.findtext('c:surface/c:init_from',namespaces=NS)
            require(image_name in images,'Unmapped office diffuse image')
            effects[effect.get('id')]=images[image_name]
        else:
            color=floats(diffuse.findtext('c:color',namespaces=NS),4)
            require(all(0<=v<=1 for v in color),'Invalid office colour')
            effects[effect.get('id')]=tuple(color)
    materials={}
    for material in root.findall('c:library_materials/c:material',NS):
        effect=material.find('c:instance_effect',NS);require(effect is not None,'Missing office material effect')
        materials[material.get('id')]=effect.get('url','').removeprefix('#')
    geometries={g.get('id'):g for g in root.findall('c:library_geometries/c:geometry',NS)}
    require(0<len(geometries)<=32,'Unbounded office geometry inventory')
    output=[];selected=[];node_count=0
    def visit(node,parent,active=False):
        nonlocal node_count
        node_count+=1;require(node_count<=128,'Unbounded office scene')
        here=node.get('name')==entry['visual_node']
        if here:selected.append(node)
        active=active or here;matrix=parent
        for child in node:
            tag=child.tag.rsplit('}',1)[-1];local=None
            if tag=='translate':local=trs(floats(child.text,3),[0,0,0,1],[1,1,1])
            elif tag=='scale':
                scale=floats(child.text,3);require(all(.0001<v<100 for v in scale),'Invalid office scale')
                local=trs([0,0,0],[0,0,0,1],scale)
            elif tag=='rotate':
                x,y,z,angle=floats(child.text,4);length=math.sqrt(x*x+y*y+z*z)
                require(length>1e-8,'Invalid office axis');s=math.sin(math.radians(angle)*.5)/length
                local=trs([0,0,0],[x*s,y*s,z*s,math.cos(math.radians(angle)*.5)],[1,1,1])
            elif tag=='matrix':
                value=floats(child.text,16);require(value[12:]==[0,0,0,1],'Projective office transform')
                local=tuple(value[r*4+c] for c in range(4) for r in range(4))
            if local is not None:matrix=multiply(matrix,local)
        if active:
            for instance in node.findall('c:instance_geometry',NS):
                key=instance.get('url','').removeprefix('#');require(key in geometries,'Unknown office geometry')
                geometry=geometries[key].find('c:mesh',NS);require(geometry is not None,'Missing office mesh')
                sources={}
                for source in geometry.findall('c:source',NS):
                    array=source.find('c:float_array',NS);accessor=source.find('c:technique_common/c:accessor',NS)
                    require(array is not None and accessor is not None,'Unsupported office source')
                    values=floats(array.text);stride=int(accessor.get('stride','1'));count=int(accessor.get('count','0'))
                    require(0<stride<=4 and 0<count<=16384 and len(values)==stride*count,'Invalid office accessor bounds')
                    sources[source.get('id')]=[values[i:i+stride] for i in range(0,len(values),stride)]
                vertices={v.get('id'):v.find("c:input[@semantic='POSITION']",NS).get('source','').removeprefix('#') for v in geometry.findall('c:vertices',NS)}
                bindings={b.get('symbol'):b.get('target','').removeprefix('#') for b in instance.findall('c:bind_material/c:technique_common/c:instance_material',NS)}
                require(not geometry.findall('c:polylist',NS) and not geometry.findall('c:polygons',NS),'Selected office source must be triangulated')
                for batch in geometry.findall('c:triangles',NS):
                    attributes=batch.findall('c:input',NS);require(0<len(attributes)<=8,'Invalid office attribute count')
                    stride=max(int(a.get('offset','0')) for a in attributes)+1;require(stride<=8,'Invalid office corner stride')
                    position=next((a for a in attributes if a.get('semantic')=='VERTEX'),None)
                    uv=next((a for a in attributes if a.get('semantic')=='TEXCOORD'),None);require(position is not None,'Missing office positions')
                    positions=sources[vertices[position.get('source','').removeprefix('#')]]
                    uvs=sources[uv.get('source','').removeprefix('#')] if uv is not None else None
                    indices=[int(v) for v in batch.findtext('c:p',default='',namespaces=NS).split()]
                    count=int(batch.get('count','0'));require(0<count<=8000 and len(indices)==count*3*stride,'Invalid office triangle bounds')
                    symbol=batch.get('material');require(symbol in bindings and bindings[symbol] in materials,'Missing office material binding')
                    shade=effects[materials[bindings[symbol]]]
                    for at in range(0,len(indices),stride*3):
                        points=[];coords=[]
                        for k in range(3):
                            index=indices[at+k*stride+int(position.get('offset','0'))];require(0<=index<len(positions),'Office position outside source')
                            p=transform(matrix,positions[index]);points.append(tuple(v*meter for v in p))
                            if uvs is not None:
                                index=indices[at+k*stride+int(uv.get('offset','0'))];require(0<=index<len(uvs),'Office UV outside source')
                                coords.append(uvs[index])
                        color=shade
                        if not isinstance(shade,tuple):
                            require(len(coords)==3,'Textured office surface has no UV')
                            samples=[]
                            for weights in [(1/3,1/3,1/3),(.6,.2,.2),(.2,.6,.2),(.2,.2,.6)]:
                                u,v=(sum(coords[k][j]*weights[k] for k in range(3)) for j in range(2))
                                samples.append(shade.getpixel((min(shade.width-1,int((u%1)*shade.width)),min(shade.height-1,int((1-v%1)*shade.height)))))
                            color=tuple(sum(p[c] for p in samples)/(255*len(samples)) for c in range(3))+(1,)
                        output.append((points,color));require(len(output)<=8000,'Office triangle budget exceeded')
        for child in node.findall('c:node',NS):visit(child,matrix,active)
    for node in root.findall('c:library_visual_scenes/c:visual_scene/c:node',NS):visit(node,IDENTITY)
    require(len(selected)==1 and output,'Selected office visual node missing or ambiguous')
    points=[p for triangle,_ in output for p in triangle]
    low=[min(p[c] for p in points) for c in range(3)];high=[max(p[c] for p in points) for c in range(3)]
    require(all(math.isfinite(v) and abs(v)<100 for v in low+high),'Unbounded office coordinates')
    center=[(a+b)*.5 for a,b in zip(low,high)]
    turns=entry.get('source_turns',0);require(type(turns) is int and 0<=turns<4,'Invalid office front calibration')
    def point(p):
        x,y,z=p[0]-center[0],p[1]-center[1],p[2]-low[2]
        for _ in range(turns):x,y=-y,x
        return (x,y,z)
    bounds=[high[c]-low[c] for c in range(3)]
    if turns%2:bounds[0],bounds[1]=bounds[1],bounds[0]
    require(all(v>.005 for v in bounds),'Office model has empty bounds')
    # Match the generated float table precision. Platform libm differences in
    # source rotations must not leak sub-ULP double tails into the manifest.
    bounds=[float(format(v,'.8g')) for v in bounds]
    return [([point(p) for p in triangle],color) for triangle,color in output],bounds
