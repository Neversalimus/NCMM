"""Compile pinned SDL_GPU SPIR-V/DXIL assets into an owned C++ header.

Uses the existing glslang / SDL_shadercross build dependencies. No runtime
compiler, shell interpolation or downloaded shader program is required.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import tempfile

p=argparse.ArgumentParser()
p.add_argument('--glslang',type=Path)
p.add_argument('--shadercross',type=Path)
p.add_argument('--output',required=True,type=Path)
p.add_argument('--source',type=Path,default=Path(__file__).parent/'shaders')
p.add_argument('--verify',action='store_true',help='Verify the shipping source identity and embedded SPIR-V/DXIL without invoking a compiler')
a=p.parse_args()
# The optional component program deletes only branches excluded by the Host's
# exact material/vertex/cell proof. Keep all used detail and depth expressions.
ordinary=(a.source/'geometry.frag').read_text(encoding='utf-8')
component=ordinary
distance='    if((flags&8192u)!=0u){float light=(.035+.82*sat(camera.sun.w))*(.72+.20*max(n.z,0.0)+.08*max(0.0,dot(n,camera.sun.xyz)));return vec3(light);}\n'
if component.count(distance)!=1:p.error('Unreviewed component distance branch')
component=component.replace(distance,'')
begin=component.index('    if(cell>=uint(camera.grid.z)){');end=component.index('    Cell c=cells[cell];',begin)
component=component[:begin]+component[end:]
begin=component.index('    if((flags&64u)!=0u){ivec2 size=textureSize(');end=component.index('    color=material_detail(',begin)
removed=component[begin:end]
if not all(term in removed for term in ['flags&1u','flags&32u','material.state.y==16u','if(alpha<.01)discard;']):
    p.error('Unreviewed component coverage branches')
component=component[:begin]+component[end:]
for before,after in [('#include "material.glsl"','#include "material_component.glsl"'),
                     ('uint flags=material.state.x|vertex_flags;','uint flags=(material.state.x&(512u|16384u))|8u;'),
                     ('bool lower=(material.state.x&4096u)!=0u;','bool lower=false;'),
                     ('float z=view_point(p).z,alpha=material.color.a;','float z=view_point(p).z,alpha=1.0;')]:
    if component.count(before)!=1:p.error('Unreviewed component constant proof')
    component=component.replace(before,after)
if (a.source/'geometry_component.frag').read_text(encoding='utf-8')!=component:
    p.error('Component fragment changed an expression outside its proved branches/constants')
reflective=component.replace('material_detail(color,material.state.y,','material_detail(color,7u,')
if reflective==component or (a.source/'geometry_reflective.frag').read_text(encoding='utf-8')!=reflective:
    p.error('Reflective fragment changed an expression outside its exact finish constant')
plain=component.replace('material_detail(color,material.state.y,','material_detail(color,0u,')
if plain==component or (a.source/'geometry_plain.frag').read_text(encoding='utf-8')!=plain:
    p.error('Plain fragment changed an expression outside its exact finish-zero proof')

glass=component.replace('#include "material_component.glsl"','#include "material.glsl"')
old_glass_main='void main(){uint flags=(material.state.x&(512u|16384u))|8u;vec3 p=world_position,n=(flags&8u)!=0u?normalize(world_normal):normalize(flat_normal);\n    if((flags&512u)!=0u&&dot(flat_normal,camera.eye.xyz-p)<=0.0)discard;\n    float z=view_point(p).z,alpha=1.0;vec3 color=material.color.rgb;'
new_glass_main='// The Host proves finish0, smooth untextured corners, a real world cell,\n// unchanged ordinary alpha, and no coverage/depth/lower/held modifiers.\nvoid main(){uint flags=(material.state.x&16384u)|8u;vec3 p=world_position,n=normalize(world_normal);\n    float z=view_point(p).z,alpha=material.color.a;vec3 color=material.color.rgb;\n    if(alpha<.01)discard;'
if glass.count(old_glass_main)!=1:p.error('Unreviewed ordinary glass flag/alpha proof')
glass=glass.replace(old_glass_main,new_glass_main).replace('material_detail(color,material.state.y,','material_detail(color,0u,')
if (a.source/'geometry_glass.frag').read_text(encoding='utf-8')!=glass:
    p.error('Glass fragment changed an expression outside its proved branches and finish constant')

detail=(a.source/'material.glsl').read_text(encoding='utf-8')
if detail.count('finish==7u')!=1 or detail.count('finish==8u')!=1 or 'if(finish==7u||finish==8u)' not in detail:
    p.error('Paint and metal no longer share the same original reflection expressions')
begin=detail.index('    if(finish==12u){float scale=');end=detail.index('    vec2 q=abs(n.z)>.5?',begin)
if (a.source/'material_component.glsl').read_text(encoding='utf-8')!=detail[:begin]+detail[end:]:
    p.error('Component material changed ordinary detail or reflection arithmetic')
lines=['#pragma once','#include <cstddef>','namespace ncmm_gpu::shaders {']
source_hash=hashlib.sha256()
for source in sorted(a.source.glob('*')):
    if source.suffix in ('.vert','.frag','.comp','.glsl'):
        source_hash.update(source.name.encode());source_hash.update(source.read_bytes())
if a.verify:
    header=a.output.read_text(encoding='utf-8')
    identity=re.findall(r'source_sha256\[\]="([0-9a-f]{64})"',header)
    if identity!=[source_hash.hexdigest()]:p.error('Embedded GPU shaders do not match the current source SHA256')
    programs=sorted(source for source in a.source.glob('*') if source.suffix in ('.vert','.frag','.comp'))
    if not programs:p.error('No GPU shader programs to verify')
    for source in programs:
        name=source.stem+'_'+source.suffix[1:]
        for fmt,magic in (('spv',b'\x03\x02\x23\x07'),('dxil',b'DXBC')):
            arrays=re.findall(r'inline constexpr unsigned char '+re.escape(name+'_'+fmt)+r'\[\]=\{([^}]*)\};',header,re.S)
            if len(arrays)!=1:p.error(f'Missing or duplicated embedded shader: {name}_{fmt}')
            try:data=bytes(int(value) for value in arrays[0].split(',') if value.strip())
            except ValueError:p.error(f'Invalid embedded shader bytes: {name}_{fmt}')
            if len(data)<=20 or data[:4]!=magic:p.error(f'Uncompiled or invalid embedded shader: {name}_{fmt}')
    print(f'Verified {len(programs)} shipping GPU programs in SPIR-V/DXIL; source SHA256 {source_hash.hexdigest()}')
    raise SystemExit(0)
if not a.glslang:p.error('--glslang is required for compilation')
with tempfile.TemporaryDirectory(prefix='ncmm-gpu-shaders-') as directory:
    root=Path(directory)
    for source in sorted(a.source.glob('*')):
        if source.suffix not in ('.vert','.frag','.comp'):continue
        stage=source.suffix[1:];name=source.stem+'_'+stage
        spv=root/(source.name+'.spv')
        subprocess.run([str(a.glslang),'--quiet','-V','-I'+str(a.source.resolve()),'-S',stage,'-e','main','-o',str(spv),str(source)],check=True)
        formats={'spv':spv}
        if a.shadercross:
            dxil=root/(source.name+'.dxil')
            subprocess.run([str(a.shadercross),str(spv),'-s','SPIRV','-d','DXIL','-o',str(dxil)],check=True)
            formats['dxil']=dxil
        for fmt,path in formats.items():
            data=path.read_bytes()
            assert len(data)>20 and (fmt!='spv' or data[:4]==b'\x03\x02\x23\x07')
            lines.append(f'inline constexpr unsigned char {name}_{fmt}[]={{')
            lines.extend(','.join(str(b) for b in data[i:i+32])+',' for i in range(0,len(data),32))
            lines.append('};')
        if 'dxil' not in formats:lines.append(f'inline constexpr unsigned char {name}_dxil[]={{0}};')
    lines.append('inline constexpr const char source_sha256[]="'+source_hash.hexdigest()+'";')
lines.append('}')
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(f'Compiled GPU shader header: {a.output.name}; source SHA256 {source_hash.hexdigest()}')
