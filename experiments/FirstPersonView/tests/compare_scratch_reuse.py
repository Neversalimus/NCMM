"""Compare all existing normalized GPU scene readbacks with the exact pre-change hints.

This is a GPU unit fixture. It does not measure Native game performance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys

REFERENCE_SHA256='a0dd1aa83c05e1a51ac33854dedb54696e85616857d9da1f68bb30ee7fe9662a'


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--sdl-dir',type=Path)
    parser.add_argument('--toolchain-file',type=Path)
    parser.add_argument('--vcpkg-triplet')
    parser.add_argument('--vcpkg-installed-dir',type=Path)
    parser.add_argument('--shader-header',type=Path)
    parser.add_argument('--sanitize',action='store_true',help='Clang ASan/UBSan on Linux')
    args=parser.parse_args();repo=args.repository.resolve();out=args.output.resolve()
    assert not out.exists(),'Use a fresh directory; preserve earlier output'
    assert not (args.sanitize and os.name=='nt'),'Use the compiler-specific Windows ASan fixture'
    assert args.sdl_dir or args.toolchain_file,'Use the same explicit SDL configuration as the original GPU fixture'
    parent=repo/'experiments/FirstPersonView';backend=repo/'experiments/graphics_bridge/ncmm_gpu_backend.hpp'
    candidate=backend.read_text(encoding='utf-8').replace('\r\n','\n');reference=candidate
    policy='    // Scratch reuse is currently pixel-qualified on Direct3D 12.\n    bool cycle_private_scratch()const{\n        auto *device=context_.device();\n        const char *driver=device?SDL_GetGPUDeviceDriver(device):nullptr;\n        return !driver||std::strcmp(driver,"direct3d12")!=0;\n    }\n'
    assert reference.count(policy)==1
    reference=reference.replace(policy,'')
    replacements=[('c.cycle=clear&&cycle_private_scratch();', 'c.cycle=clear;'), ('depth.cycle=(clear||held)&&cycle_private_scratch();', 'depth.cycle=clear||held;'), ('shadow_binding.cycle=cycle_private_scratch();', 'shadow_binding.cycle=true;'), ('e.cycle=cycle_private_scratch();', 'e.cycle=true;')]
    for before,after in replacements:
        assert reference.count(before)==1,before;reference=reference.replace(before,after)
    assert hashlib.sha256(reference.encode()).hexdigest()==REFERENCE_SHA256,'Unreviewed reference reconstruction'
    out.mkdir(parents=True);(out/'reference').mkdir();(out/'reference/ncmm_gpu_backend.hpp').write_text(reference,encoding='utf-8',newline='\n')
    capture='''#include "ncmm_graphics_performance.hpp"
#include "ncmm_gpu_backend.hpp"
#include "gpu_scene.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
namespace {
unsigned observed_image_count=0;
SDL_Surface *observed_convert_surface(SDL_Surface *source,SDL_PixelFormat format){
    auto *result=SDL_ConvertSurface(source,format);
    if(result&&format==SDL_PIXELFORMAT_RGBA8888){
        std::ofstream stream("readback-"+std::to_string(observed_image_count++)+".rgba",std::ios::binary);
        const uint32_t shape[]={uint32_t(result->w),uint32_t(result->h),uint32_t(format)};
        stream.write(reinterpret_cast<const char *>(shape),sizeof(shape));
        for(int y=0;y<result->h;++y)stream.write(static_cast<const char *>(result->pixels)+y*result->pitch,result->w*4);
        if(!stream){std::fprintf(stderr,"Could not preserve complete GPU readback\\n");std::exit(3);}
    }
    return result;
}
}
#define SDL_ConvertSurface observed_convert_surface
'''
    (out/'capture.cpp').write_text(capture+'#include "'+(parent/'tests/gpu_scene_test.cpp').as_posix()+'"\n',encoding='utf-8',newline='\n')
    cmake='''cmake_minimum_required(VERSION 3.20)
project(ScratchReusePixelComparison LANGUAGES CXX)
set(FIRST_PERSON_BUILD_GPU_TESTS OFF CACHE BOOL "Only explicit paired probes")
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
add_subdirectory("'''+parent.as_posix()+'''" frozen EXCLUDE_FROM_ALL)
find_package(SDL3 3.4 CONFIG REQUIRED)
foreach(variant reference candidate)
  add_executable(scratch_${variant} capture.cpp)
  first_person_target(scratch_${variant})
  target_link_libraries(scratch_${variant} PRIVATE SDL3::SDL3)
  if(variant STREQUAL "reference")
    target_include_directories(scratch_${variant} BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/reference")
  endif()
  if(WIN32)
    add_custom_command(TARGET scratch_${variant} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:scratch_${variant}>")
  endif()
endforeach()
'''
    (out/'CMakeLists.txt').write_text(cmake,encoding='utf-8',newline='\n')
    env=os.environ.copy();env['MSBUILDDISABLENODEREUSE']='1'
    cmake_binary=shutil.which('cmake');assert cmake_binary,'CMake is required'
    configure=[cmake_binary,'-S',str(out),'-B',str(out/'build'),'-DCMAKE_BUILD_TYPE=RelWithDebInfo',
               '-DFIRST_PERSON_SANITIZE='+('ON' if args.sanitize else 'OFF')]
    if args.sdl_dir:configure.append('-DSDL3_DIR='+str(args.sdl_dir.resolve()))
    if args.toolchain_file:configure.append('-DCMAKE_TOOLCHAIN_FILE='+str(args.toolchain_file.resolve()))
    if args.vcpkg_triplet:configure.append('-DVCPKG_TARGET_TRIPLET='+args.vcpkg_triplet)
    if args.vcpkg_installed_dir:configure.append('-DVCPKG_INSTALLED_DIR='+str(args.vcpkg_installed_dir.resolve()))
    if args.shader_header:configure.append('-DFIRST_PERSON_GPU_SHADER_HEADER='+str(args.shader_header.resolve()))
    if args.sanitize:configure.append('-DCMAKE_CXX_COMPILER='+os.environ.get('CXX','clang++'))
    with (out/'build.log').open('wb') as log:
        subprocess.run(configure,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        subprocess.run([cmake_binary,'--build',str(out/'build'),'--config','RelWithDebInfo','--parallel','2','--target','scratch_reference','scratch_candidate'],
                       env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
    runs={}
    for variant in ('reference','candidate'):
        directory=out/variant/'run';directory.mkdir(parents=True)
        executable=out/'build'/('RelWithDebInfo' if os.name=='nt' else '')/('scratch_'+variant+('.exe' if os.name=='nt' else ''))
        with (directory/'run.log').open('wb') as log:
            result=subprocess.run([str(executable),'gpu-scene'],cwd=directory,env=env,stdout=log,stderr=subprocess.STDOUT)
        if result.returncode:
            sys.stderr.write((directory/'run.log').read_text(encoding='utf-8',errors='replace'))
            raise AssertionError(variant+' GPU scene fixture failed; all output retained')
        report=json.loads((directory/'gpu-scene.json').read_text(encoding='utf-8'))
        assert report['success'] and report['full_gpu_pipeline'] and report['cpu_raster_calls']==0
        assert report['backend'] in ('direct3d12','vulkan')
        images={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in directory.glob('readback-*.rgba')}
        assert images,'No complete existing readbacks captured'
        runs[variant]=dict(images=images,report=report,log_sha256=sha(directory/'run.log'),executable_sha256=sha(executable))
    assert runs['reference']['images']==runs['candidate']['images'],'Complete pixel mismatch; outputs retained'
    assert all(runs['reference']['report'][k]==runs['candidate']['report'][k] for k in ('backend','device','software_device'))
    result=dict(success=True,unit_gpu_fixture=True,native_game_qualified=False,performance_accepted=False,
                reference_backend_sha256=REFERENCE_SHA256,candidate_backend_sha256=hashlib.sha256(candidate.encode()).hexdigest(),
                compiled_with_sanitizers=args.sanitize,existing_scene_test_sha256=sha(parent/'tests/gpu_scene_test.cpp'),
                compared_images=len(runs['reference']['images']),compared_bytes=sum(r['bytes'] for r in runs['reference']['images'].values()),
                full_normalized_readbacks_equal=True,no_extra_gpu_readbacks=True,runs=runs)
    (out/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('success','unit_gpu_fixture','compared_images','compared_bytes','full_normalized_readbacks_equal','performance_accepted')}))


if __name__=='__main__':main()
