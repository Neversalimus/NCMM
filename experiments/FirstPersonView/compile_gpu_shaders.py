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
# The additional ordinary-opaque program must retain the complete established
# colour, lighting and material body. Only hardware-interpolated depth differs.
ordinary=(a.source/'geometry.frag').read_text(encoding='utf-8')
depth_write=';gl_FragDepth=depth_z(z);'
if ordinary.count(depth_write)!=1:p.error('Unreviewed ordinary opaque depth specialization')
if (a.source/'geometry_opaque.frag').read_text(encoding='utf-8')!=ordinary.replace(depth_write,';'):
    p.error('Opaque specialization changed colour, material or lighting code')
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
