"""Convert Drillimpact's pinned CC0 ZIP into typed, immutable NCMM tables.

Offline only. Requires Pillow for the existing RGBA8 texture API. No Blender, FBX,
downloaded scripts, runtime glTF parser or dominant-weight approximation.
"""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import stat
import struct
import zipfile

ARCHIVE_SHA256 = '13689348acce167599db2b7190214aa4dbe914f5547f74e1877ff61361fc8cb8'
SOURCE_URL = 'https://drillimpact.itch.io/psx-first-person-arms-free'
FORMATS = {5120:'b', 5121:'B', 5122:'h', 5123:'H', 5125:'I', 5126:'f'}
COMPONENTS = {'SCALAR':1, 'VEC2':2, 'VEC3':3, 'VEC4':4, 'MAT4':16}

def require(condition, message):
    if not condition:
        raise ValueError(message)

def finite(values):
    return all(isinstance(v, (int, float)) and math.isfinite(v) for v in values)

def multiply(a, b):
    return tuple(sum(a[k*4+r]*b[c*4+k] for k in range(4)) for c in range(4) for r in range(4))

def trs(t, q, s):
    x,y,z,w = q
    require(finite((*t,*q,*s)) and abs(sum(v*v for v in q)-1) < 1e-4, 'Invalid node TRS')
    return ((1-2*(y*y+z*z))*s[0], 2*(x*y+z*w)*s[0], 2*(x*z-y*w)*s[0], 0,
            2*(x*y-z*w)*s[1], (1-2*(x*x+z*z))*s[1], 2*(y*z+x*w)*s[1], 0,
            2*(x*z+y*w)*s[2], 2*(y*z-x*w)*s[2], (1-2*(x*x+y*y))*s[2], 0,
            *t, 1)

class GLB:
    def __init__(self, content):
        require(len(content) >= 20 and len(content) <= 32*1024*1024, 'Invalid GLB size')
        magic, version, size = struct.unpack_from('<4sII', content)
        require(magic == b'glTF' and version == 2 and size == len(content), 'Invalid GLB header')
        chunks = {}; at = 12
        while at < len(content):
            require(at+8 <= len(content), 'Truncated GLB chunk header')
            length, kind = struct.unpack_from('<II', content, at); at += 8
            require(length%4 == 0 and at+length <= len(content) and kind not in chunks, 'Invalid GLB chunk')
            chunks[kind] = content[at:at+length]; at += length
        require(set(chunks) == {0x4E4F534A,0x004E4942}, 'Unknown or absent GLB chunks')
        self.document = json.loads(chunks[0x4E4F534A])
        self.binary = chunks[0x004E4942]
        d = self.document
        require(d.get('asset',{}).get('version') == '2.0' and not d.get('extensionsRequired'), 'Unsupported GLB version/extension')
        require(len(d['buffers']) == 1 and 'uri' not in d['buffers'][0] and
                d['buffers'][0]['byteLength'] <= len(self.binary) <= d['buffers'][0]['byteLength']+3,
                'External or truncated GLB buffer')

    def accessor(self, index, kind=None):
        d = self.document
        require(isinstance(index,int) and 0 <= index < len(d['accessors']), 'Accessor index out of range')
        a = d['accessors'][index]
        require('sparse' not in a and a['type'] in COMPONENTS and a['componentType'] in FORMATS and
                (kind is None or a['type'] == kind) and 0 < a['count'] <= 200000, 'Unsupported accessor')
        view = d['bufferViews'][a['bufferView']]
        require(view['buffer'] == 0, 'External buffer view')
        layout = struct.Struct('<'+FORMATS[a['componentType']]*COMPONENTS[a['type']])
        stride = view.get('byteStride', layout.size)
        offset = a.get('byteOffset', 0); start = view.get('byteOffset',0)
        require(stride >= layout.size and stride%struct.calcsize(FORMATS[a['componentType']]) == 0 and
                offset >= 0 and start >= 0 and start+view['byteLength'] <= len(self.binary) and
                offset+(a['count']-1)*stride+layout.size <= view['byteLength'], 'Accessor exceeds its own buffer view')
        result = [layout.unpack_from(self.binary,start+offset+i*stride) for i in range(a['count'])]
        if a.get('normalized'):
            divisors = {5120:127,5121:255,5122:32767,5123:65535}
            require(a['componentType'] in divisors, 'Invalid normalized component type')
            result = [tuple(max(-1.0,v/divisors[a['componentType']]) for v in row) for row in result]
        require(all(finite(row) for row in result), 'Accessor contains NaN/Inf')
        return result

def hierarchy(nodes):
    require(0 < len(nodes) <= 128, 'Unexpected skeleton size')
    parents = [-1]*len(nodes)
    for parent,node in enumerate(nodes):
        require('matrix' not in node, 'This importer expects authored TRS nodes')
        for child in node.get('children',[]):
            require(isinstance(child,int) and 0 <= child < len(nodes) and parents[child] == -1, 'Invalid or multiply parented bone')
            parents[child] = parent
    order=[]; active=set(); visited=set()
    def visit(index):
        require(index not in active, 'Skeleton hierarchy contains a cycle')
        if index in visited:
            return
        active.add(index)
        if parents[index] >= 0:
            visit(parents[index])
        active.remove(index); visited.add(index); order.append(index)
    for index in range(len(nodes)):
        visit(index)
    return parents,order

def ordered_triangles(vertices, indices, palettes):
    # Opaque triangle order can improve the existing 128-palette chunks.
    # A dominant joint is only an ordering key: no vertex influence changes.
    def dominant(index):
        joints,weights=palettes[vertices[index][8]]
        return joints[max(range(4),key=lambda i:weights[i])]
    triangles=[indices[i:i+3] for i in range(0,len(indices),3)]
    triangles.sort(key=lambda t:(vertices[t[0]][9],vertices[t[0]][10],
                                 min(dominant(i) for i in t),max(dominant(i) for i in t)))
    return [index for triangle in triangles for index in triangle]

def import_model(content):
    glb = GLB(content); d = glb.document
    require(len(d['meshes']) == len(d['skins']) == 1 and len(d['meshes'][0]['primitives']) == 1, 'Unexpected asset topology')
    primitive = d['meshes'][0]['primitives'][0]; attributes = primitive['attributes']
    material=d['materials'][primitive['material']] if 'material' in primitive else {}
    require(material.get('alphaMode','OPAQUE')=='OPAQUE', 'Palette ordering requires an opaque source material')
    require(primitive.get('mode',4) == 4 and set(attributes) == {'POSITION','NORMAL','TEXCOORD_0','JOINTS_0','WEIGHTS_0'},
            'Unsupported primitive or additional weight set')
    positions=glb.accessor(attributes['POSITION'],'VEC3'); normals=glb.accessor(attributes['NORMAL'],'VEC3')
    uv=glb.accessor(attributes['TEXCOORD_0'],'VEC2'); joints=glb.accessor(attributes['JOINTS_0'],'VEC4')
    weights=glb.accessor(attributes['WEIGHTS_0'],'VEC4'); indices=[v[0] for v in glb.accessor(primitive['indices'],'SCALAR')]
    require(0 < len(positions) <= 10000 and len(positions) == len(normals) == len(uv) == len(joints) == len(weights), 'Invalid vertex attributes')
    require(len(indices)%3 == 0 and all(isinstance(i,int) and 0 <= i < len(positions) for i in indices), 'Invalid triangle indices')
    require(all(all(0 <= x <= 1 for x in row) for row in uv), 'UV outside the pinned texture')
    require(all(abs(sum(x*x for x in row)-1) < 2e-3 for row in normals), 'Invalid vertex normal')
    skin=d['skins'][0]; nodes=d['nodes']; parents,order=hierarchy(nodes)
    require(len(set(skin['joints'])) == len(skin['joints']) and all(0 <= j < len(nodes) for j in skin['joints']), 'Invalid skin joints')
    inverse=glb.accessor(skin['inverseBindMatrices'],'MAT4')
    require(len(inverse) == len(skin['joints']), 'Missing inverse bind matrices')
    global_bind={}
    for i in order:
        n=nodes[i]; m=trs(n.get('translation',(0,0,0)),n.get('rotation',(0,0,0,1)),n.get('scale',(1,1,1)))
        global_bind[i]=multiply(global_bind[parents[i]],m) if parents[i]>=0 else m
    identity=tuple(1 if i%5==0 else 0 for i in range(16))
    bind_error=max(abs(a-b) for joint,ibm in zip(skin['joints'],inverse) for a,b in zip(multiply(global_bind[joint],ibm),identity))
    require(bind_error < 2e-4, 'Authored bind pose does not match inverse bind matrices')
    names={node['name']:i for i,node in enumerate(nodes)}
    for name in ('hand.R','hand.L','forearm.R','forearm.L'):
        require(name in names, 'Expected hand/forearm anchor missing')
    def side(index):
        while index >= 0:
            name=nodes[index].get('name','')
            if name.endswith('.R'): return 0
            if name.endswith('.L'): return 1
            index=parents[index]
        return -1
    def is_hand(index):
        while index >= 0:
            if index in (names['hand.R'],names['hand.L']): return True
            index=parents[index]
        return False
    palettes=[]; palette_ids={}; original=[]
    for i,(joint_row,weight_row) in enumerate(zip(joints,weights)):
        require(all(isinstance(j,int) and 0 <= j < len(skin['joints']) for j in joint_row) and
                all(0 <= w <= 1 for w in weight_row) and abs(sum(weight_row)-1) < 2e-5, 'Invalid joint weights')
        sides={side(skin['joints'][j]) for j,w in zip(joint_row,weight_row) if w>1e-7}
        sides.discard(-1)
        require(len(sides)==1, 'A vertex influences both arms or lacks a side')
        palette=(joint_row,weight_row)
        if palette not in palette_ids:
            palette_ids[palette]=len(palettes); palettes.append(palette)
        hand=sum(w for j,w in zip(joint_row,weight_row) if is_hand(skin['joints'][j]))
        original.append((*positions[i],*normals[i],*uv[i],palette_ids[palette],next(iter(sides)),int(hand>=.5)))
    # Duplicate only a material boundary, never alter positions, normals or weights.
    vertices=[]; vertex_ids={}; output_indices=[]
    for at in range(0,len(indices),3):
        face=indices[at:at+3]
        require(len({original[i][-2] for i in face})==1, 'A triangle crosses both arms')
        region=int(sum(original[i][-1] for i in face)>=2)
        for index in face:
            key=(index,region)
            if key not in vertex_ids:
                vertex_ids[key]=len(vertices); vertices.append((*original[index][:-1],region))
            output_indices.append(vertex_ids[key])
    output_indices=ordered_triangles(vertices,output_indices,palettes)
    keys=[]; tracks=[]; clips=[]; sequences={}
    properties={'translation':0,'rotation':1,'scale':2}
    for animation in d['animations']:
        require(animation.get('name') and animation['channels'], 'Unnamed/empty animation')
        source_times=[glb.accessor(s['input'],'SCALAR') for s in animation['samplers']]
        start=min(row[0] for values in source_times for row in values)
        end=max(row[0] for values in source_times for row in values)
        first=len(tracks); seen=set()
        for channel in animation['channels']:
            target=channel['target']; sampler=animation['samplers'][channel['sampler']]
            require(target['path'] in properties and 0 <= target['node'] < len(nodes) and
                    (target['node'],target['path']) not in seen, 'Unknown/duplicate animation target')
            seen.add((target['node'],target['path']))
            mode=sampler.get('interpolation','LINEAR')
            require(mode in ('LINEAR','STEP'), 'Unsupported interpolation; never silently resample')
            times=[v[0]-start for v in source_times[channel['sampler']]]
            values=glb.accessor(sampler['output'],'VEC4' if target['path']=='rotation' else 'VEC3')
            require(len(times)==len(values) and all(b>a for a,b in zip(times,times[1:])), 'Invalid key times')
            if target['path']=='rotation':
                require(all(abs(sum(v*v for v in row)-1)<2e-4 for row in values), 'Invalid animation quaternion')
            sequence=tuple((t,tuple(v) if len(v)==4 else (*v,0.0)) for t,v in zip(times,values))
            if all(v==sequence[0][1] for _,v in sequence):
                sequence=((0.0,sequence[0][1]),)
            if sequence not in sequences:
                sequences[sequence]=len(keys); keys.extend(sequence)
            tracks.append((target['node'],properties[target['path']],int(mode=='STEP'),sequences[sequence],len(sequence)))
        clips.append((animation['name'],end-start,first,len(tracks)-first))
    require(len({c[0] for c in clips})==len(clips) and len(palettes)<=1024, 'Unexpected clip/palette budget')
    return {'vertices':vertices,'indices':output_indices,'nodes':nodes,'parents':parents,'order':order,
            'joints':skin['joints'],'inverse_bind':inverse,'palettes':palettes,'keys':keys,'tracks':tracks,'clips':clips,
            'bind_max_error':bind_error,'original_vertices':len(positions)}

def number(value):
    text=format(value,'.9g')
    return text+('' if '.' in text or 'e' in text.lower() else '.0')+'f'

def emit(model, output):
    lines=['// Generated by tools/import_psx_arms.py; do not edit.', '#include "viewmodel_asset.hpp"',
           'namespace first_person::viewmodel {', 'namespace {']
    def array(kind,name,rows):
        lines.append('const '+kind+' '+name+'[]={')
        lines.extend('    '+row+',' for row in rows)
        lines.append('};')
    f=lambda row: ','.join(number(v) for v in row)
    array('vertex','vertices',('{'+ '{'+f(v[:3])+'},{'+f(v[3:6])+'},'+f(v[6:8])+','+','.join(str(x) for x in v[8:])+'}' for v in model['vertices']))
    array('uint32_t','indices',(','.join(str(v) for v in model['indices'][i:i+24]) for i in range(0,len(model['indices']),24)))
    array('node','nodes',('{'+json.dumps(n.get('name',''))+','+str(model['parents'][i])+',{{'+f(n.get('translation',(0,0,0)))+'},{'+f(n.get('rotation',(0,0,0,1)))+'},{'+f(n.get('scale',(1,1,1)))+'}}}' for i,n in enumerate(model['nodes'])))
    for name in ('order','joints'):
        array('uint16_t',name,(','.join(str(v) for v in model[name]),))
    array('matrix','inverse_bind',('{{'+f(row)+'}}' for row in model['inverse_bind']))
    array('weights','palettes',('{{'+','.join(str(v) for v in j)+'},{'+f(w)+'}}' for j,w in model['palettes']))
    array('keyframe','keys',('{'+number(t)+',{'+f(v)+'}}' for t,v in model['keys']))
    array('track','tracks',('{'+str(n)+',channel::'+('translation','rotation','scale')[p]+',interpolation::'+('linear','step')[m]+','+str(first)+','+str(count)+'}' for n,p,m,first,count in model['tracks']))
    array('clip','clips',('{'+json.dumps(name)+','+number(duration)+','+str(first)+','+str(count)+'}' for name,duration,first,count in model['clips']))
    lines.extend(['}', 'const asset &imported_asset() noexcept {', '    static const asset result{',
                  '        vertices,std::size(vertices),indices,std::size(indices),',
                  '        nodes,order,std::size(nodes),joints,inverse_bind,std::size(joints),',
                  '        palettes,std::size(palettes),keys,std::size(keys),tracks,std::size(tracks),',
                  '        clips,std::size(clips),psx_bare_pixels,psx_glove_pixels,512,512};',
                  '    return result;', '}', '}'])
    (output/'model.generated.cpp').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')

def convert(archive_path, output):
    from PIL import Image, __version__ as pillow_version
    content=archive_path.read_bytes()
    require(hashlib.sha256(content).hexdigest()==ARCHIVE_SHA256, 'Author ZIP SHA256 differs from the pinned version')
    with zipfile.ZipFile(io.BytesIO(content)) as archive:
        require(archive.testzip() is None, 'Author archive CRC failed')
        seen=set()
        for entry in archive.infolist():
            path=PurePosixPath(entry.filename)
            require(not path.is_absolute() and '..' not in path.parts and ':' not in entry.filename and
                    '\\' not in entry.filename and not stat.S_ISLNK(entry.external_attr>>16) and
                    entry.filename.casefold() not in seen and entry.file_size<=32*1024*1024, 'Unsafe ZIP entry')
            seen.add(entry.filename.casefold())
        glb=archive.read('arms_rig.glb'); model=import_model(glb)
        prepared=[]
        for source,target in (('arms_01.png','bare.rgba'),('arms_gloves_01.png','gloves.rgba')):
            raw=archive.read(source); image=Image.open(io.BytesIO(raw)); image.load()
            require(image.size==(512,512), 'Author texture dimensions changed')
            rgba=image.convert('RGBA').tobytes()
            prepared.append((source,target,raw,rgba))
        output.mkdir(parents=True,exist_ok=True)
        emit(model,output)
        for _,target,_,rgba in prepared:
            (output/target).write_bytes(rgba)
    manifest={'schema':1,'name':'PSX First Person Arms','author':'Drillimpact','version':'1.1.0',
              'source_url':SOURCE_URL,'license':'CC0-1.0','license_url':'https://creativecommons.org/publicdomain/zero/1.0/',
              'license_evidence':'Author download page and First Person Arms Big Update devlog; no separate license file in ZIP',
              'archive_sha256':ARCHIVE_SHA256,'glb_sha256':hashlib.sha256(glb).hexdigest(),
              'original_vertices':model['original_vertices'],'material_split_vertices':len(model['vertices']),
              'triangles':len(model['indices'])//3,'nodes':len(model['nodes']),'joints':len(model['joints']),
              'weighted_palette_entries':len(model['palettes']),'all_four_weights_preserved':True,
              'palette_method':'Exact weighted affine matrices per distinct source joint/weight tuple; no dominant-joint approximation',
              'triangle_ordering':'Opaque triangles ordered offline by side, material region and bone locality; original triangle winding, attributes and all weights preserved',
              'bind_max_error':model['bind_max_error'],'clips':[c[0] for c in model['clips']],
              'stored_keyframes':len(model['keys']),'tracks':len(model['tracks']),
              'pillow_version':pillow_version,
              'texture_conversion':'Source RGBA16 PNG to existing API RGBA8; original 512x512 dimensions retained, no filtering or resampling',
              'textures':[{'source':source,'source_sha256':hashlib.sha256(raw).hexdigest(),'file':target,
                           'source_png_bit_depth':raw[24],'size':[512,512],'rgba8_sha256':hashlib.sha256(rgba).hexdigest()} for source,target,raw,rgba in prepared],
              'generated_cpp_sha256':hashlib.sha256((output/'model.generated.cpp').read_bytes()).hexdigest(),
              'runtime_network_or_gltf_parser':False,'blender_scripts_executed':False}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    return manifest

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    result=convert(args.archive,args.output)
    print(json.dumps({key:result[key] for key in ('original_vertices','material_split_vertices','triangles','joints','weighted_palette_entries','bind_max_error','stored_keyframes')}))
