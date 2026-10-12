"""Compile pinned SDL_GPU SPIR-V/DXIL assets into an owned C++ header.

Uses the existing glslang / SDL_shadercross build dependencies. No runtime
compiler, shell interpolation or downloaded shader program is required.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

p=argparse.ArgumentParser()
p.add_argument('--glslang',required=True,type=Path)
p.add_argument('--shadercross',type=Path)
p.add_argument('--output',required=True,type=Path)
p.add_argument('--source',type=Path,default=Path(__file__).parent/'shaders')
a=p.parse_args()
lines=['#pragma once','#include <cstddef>','namespace ncmm_gpu::shaders {']
source_hash=hashlib.sha256()
for source in sorted(a.source.glob('*')):
    if source.suffix in ('.vert','.frag','.comp','.glsl'):
        source_hash.update(source.name.encode());source_hash.update(source.read_bytes())
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
