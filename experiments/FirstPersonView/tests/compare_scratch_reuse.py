"""Compare all existing normalized GPU scene readbacks with the exact pre-change hints.

This is a GPU unit fixture. It does not measure Native game performance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,struct,subprocess,sys

REFERENCE_SHA256='a0dd1aa83c05e1a51ac33854dedb54696e85616857d9da1f68bb30ee7fe9662a'


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def reference_backend(candidate):
    """Pure reconstruction, always checked against the unchanged frozen digest."""
    reference=candidate
    # Candidate-only fixed-function face rejection must not also optimize the
    # frozen baseline. Its original backend digest stays exactly unchanged.
    winding_replacements=[
        ('    struct batch {uint64_t key;uint32_t pass,first,count;bool winding_safe=false,component_safe=false;};',
         '    struct batch {uint64_t key;uint32_t pass,first,count;};'),
        ('        unsigned opaque_policy=0;\n',''),
        ('''                // Component flags/coverage and every real cell are proved.
                // Its original detail/reflection/depth expressions remain;
                // every unproved material, pose or cell uses the old program.
                const bool winding=selected==NCMM_GPU_OPAQUE&&b.winding_safe&&range.winding_safe;
                const bool component=selected==NCMM_GPU_OPAQUE&&b.component_safe&&range.component_surface;
                const bool reflective=component&&reflective_component_finish(m.finish);
                const unsigned policy=unsigned(winding)|(unsigned(component)<<1)|(unsigned(reflective)<<2);
                if(policy!=opaque_policy){SDL_BindGPUGraphicsPipeline(pass,reflective?
                    (winding?programs_.opaque_reflective_culled:programs_.opaque_reflective):component?
                    (winding?programs_.opaque_component_culled:programs_.opaque_component):(winding?programs_.opaque_culled:programs_.opaque));opaque_policy=policy;}
''',''),
        ('''        for(const auto &[key,list]:groups){
            const bool winding=key.first==NCMM_GPU_OPAQUE&&winding_camera(scene.camera)&&std::all_of(list.begin(),list.end(),[&](const instance &i){
                return i.bone_count==1&&i.bone_offset<scene.bone_count&&winding_bone(scene.bones[i.bone_offset])&&winding_pose(i.world);});
            const bool component=winding&&std::all_of(list.begin(),list.end(),[&](const instance &i){return i.cell<scene.cell_count;});
            batches.push_back({key.second,key.first,uint32_t(instances.size()),uint32_t(list.size()),winding,component});instances.insert(instances.end(),list.begin(),list.end());}
''','        for(const auto &[key,list]:groups){batches.push_back({key.second,key.first,uint32_t(instances.size()),uint32_t(list.size())});instances.insert(instances.end(),list.begin(),list.end());}\n')]
    for before,after in winding_replacements:
        assert reference.count(before)==1,'Unreviewed winding reconstruction'
        reference=reference.replace(before,after)
    policy='    // Scratch reuse is currently pixel-qualified on Direct3D 12.\n    bool cycle_private_scratch()const{\n        auto *device=context_.device();\n        const char *driver=device?SDL_GetGPUDeviceDriver(device):nullptr;\n        return !driver||std::strcmp(driver,"direct3d12")!=0;\n    }\n'
    assert reference.count(policy)==1
    reference=reference.replace(policy,'')
    replacements=[('c.cycle=clear&&cycle_private_scratch();', 'c.cycle=clear;'), ('depth.cycle=(clear||held)&&cycle_private_scratch();', 'depth.cycle=clear||held;'), ('shadow_binding.cycle=cycle_private_scratch();', 'shadow_binding.cycle=true;'), ('e.cycle=cycle_private_scratch();', 'e.cycle=true;')]
    for before,after in replacements:
        assert reference.count(before)==1,before;reference=reference.replace(before,after)
    # Reconstruct the same frozen baseline, including its original lifecycle
    # disposal. The candidate now drains SDL's deferred child releases after
    # clear(); that change must preserve these pixels too. Do not advance the
    # reference hash to the candidate or share its new disposal with both runs.
    disposal='''        auto *device=context_.device();
        if(device){
            if(context_.renderer())SDL_FlushRenderer(context_.renderer());
            SDL_WaitForGPUIdle(context_.device());
        }
        resources_.clear();programs_.clear();release_targets();release_buffers();context_.clear();recording_={};retirement_failed_=false;metrics_.active=0;++resource_epoch_;
        // SDL queues Release* calls in a pending-destroy list. The preceding
        // drain protects queued draws, but cannot retire objects released
        // afterwards. Finish disposal at this lifecycle boundary, including
        // when the next mode is CPU and no further 3D submission will occur.
        if(device)SDL_WaitForGPUIdle(device);
'''
    original_disposal='''        if(context_.device()){
            if(context_.renderer())SDL_FlushRenderer(context_.renderer());
            SDL_WaitForGPUIdle(context_.device());
        }
        resources_.clear();programs_.clear();release_targets();release_buffers();context_.clear();recording_={};retirement_failed_=false;metrics_.active=0;++resource_epoch_;
'''
    assert reference.count(disposal)==1,'Unreviewed lifecycle reconstruction'
    reference=reference.replace(disposal,original_disposal)
    assert hashlib.sha256(reference.encode()).hexdigest()==REFERENCE_SHA256,'Unreviewed reference reconstruction'
    return reference


def image_difference(left, right):
    result = dict(reference_bytes=len(left), candidate_bytes=len(right))
    shapes = [list(struct.unpack('<III', data[:12])) if len(data) >= 12 else None
              for data in (left, right)]
    result['reference_shape'], result['candidate_shape'] = shapes
    if shapes[0] is None or shapes[0] != shapes[1]:
        result['comparable_pixels'] = False
        return result
    width, height, _ = shapes[0]
    expected = 12 + width * height * 4
    if not width or not height or len(left) != expected or len(right) != expected:
        result['comparable_pixels'] = False
        return result
    result['comparable_pixels'] = True
    changed = alpha = maximum = 0
    storage_byte_changes = [0,0,0,0]
    samples = []
    minimum_x, minimum_y, maximum_x, maximum_y = width, height, -1, -1
    for offset in range(12, expected, 4):
        a, b = left[offset:offset+4], right[offset:offset+4]
        if a == b:
            continue
        changed += 1
        # Captures request SDL_PIXELFORMAT_RGBA8888 on little-endian CI;
        # storage is A,B,G,R, so byte three is red rather than alpha.
        alpha += a[0] != b[0]
        maximum = max(maximum, *(abs(x-y) for x, y in zip(a, b)))
        pixel = (offset-12)//4
        x, y = pixel % width, pixel // width
        for channel in range(4):storage_byte_changes[channel] += a[channel] != b[channel]
        if len(samples)<16:samples.append(dict(x=x,y=y,reference_storage_bytes=list(a),candidate_storage_bytes=list(b)))
        minimum_x, minimum_y = min(minimum_x, x), min(minimum_y, y)
        maximum_x, maximum_y = max(maximum_x, x), max(maximum_y, y)
    result.update(different_pixels=changed, different_alpha_pixels=alpha,
                  max_channel_delta=maximum,
                  storage_byte_differences=storage_byte_changes,
                  first_pixel_differences=samples,
                  storage_order='A,B,G,R (little-endian SDL_PIXELFORMAT_RGBA8888)',
                  changed_bounds=[minimum_x, minimum_y, maximum_x, maximum_y] if changed else None)
    return result


def write_comparison(out, runs, metadata):
    reference, candidate = runs['reference']['images'], runs['candidate']['images']
    equal = reference == candidate
    same_device = all(runs['reference']['report'][key] == runs['candidate']['report'][key]
                      for key in ('backend', 'device', 'software_device'))
    differences = []
    for name in sorted(reference.keys() | candidate.keys()):
        a, b = reference.get(name), candidate.get(name)
        if a == b:
            continue
        difference = dict(file=name, reference=a, candidate=b)
        if a is not None and b is not None:
            # Only files already captured by the existing fixture. No extra
            # GPU readback or transfer is needed to retain numeric diagnostics.
            left = (out/'reference/run'/name).read_bytes()
            right = (out/'candidate/run'/name).read_bytes()
            difference.update(image_difference(left, right))
        differences.append(difference)
    result = dict(metadata)
    result.update(success=equal and same_device, unit_gpu_fixture=True,
                  native_game_qualified=False, performance_accepted=False,
                  compared_images=len(reference),
                  compared_bytes=sum(row['bytes'] for row in reference.values()),
                  full_normalized_readbacks_equal=equal, no_extra_gpu_readbacks=True,
                  device_descriptors_equal=same_device,
                  reference_images=len(reference), candidate_images=len(candidate),
                  image_differences=differences, runs=runs)
    # Input equality supplements the exact existing pixel gate. It distinguishes
    # producer math/data differences from GPU state; it never excuses pixels.
    if all('submission_inputs' in runs[k] for k in ('reference','candidate')):
        left,right=(runs[k]['submission_inputs'] for k in ('reference','candidate'))
        mismatches=[]
        for index in range(max(len(left),len(right))):
            a=left[index] if index<len(left) else None;b=right[index] if index<len(right) else None
            if a!=b:mismatches.append(dict(submission=index,reference=a,candidate=b))
        result.update(semantic_submission_inputs_equal=left==right,submission_input_mismatches=mismatches)
        result['success'] = result['success'] and left==right
    # Preserve hashes and numeric mismatch details even when the exact gate
    # below fails. A diagnostic manifest is never an acceptance override.
    (out/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result


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
    parser.add_argument('--trace-inputs',action='store_true',help='Separate diagnostic experiment; never replaces the ordinary pixel gate')
    args=parser.parse_args();repo=args.repository.resolve();out=args.output.resolve()
    assert not out.exists(),'Use a fresh directory; preserve earlier output'
    assert not (args.sanitize and os.name=='nt'),'Use the compiler-specific Windows ASan fixture'
    assert args.sdl_dir or args.toolchain_file,'Use the same explicit SDL configuration as the original GPU fixture'
    parent=repo/'experiments/FirstPersonView';backend=repo/'experiments/graphics_bridge/ncmm_gpu_backend.hpp'
    candidate=backend.read_text(encoding='utf-8').replace('\r\n','\n');reference=reference_backend(candidate)
    out.mkdir(parents=True);(out/'reference').mkdir();(out/'reference/ncmm_gpu_backend.hpp').write_text(reference,encoding='utf-8',newline='\n')
    capture = '#include "'+(parent/'tests/gpu_scratch_capture.inc').as_posix()+'"\n'
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
    if args.trace_inputs:
        cmake=cmake.replace('  first_person_target(scratch_${variant})','  first_person_target(scratch_${variant})\n  target_compile_definitions(scratch_${variant} PRIVATE NCMM_GPU_CAPTURE_INPUTS=1)')
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
        if args.trace_inputs:
            inputs=directory/'submission-inputs.jsonl'
            assert inputs.is_file() and inputs.stat().st_size<=4*1024*1024,'Missing/bounded semantic input trace'
            runs[variant]['submission_inputs']=[json.loads(line) for line in inputs.read_text(encoding='utf-8').splitlines()]
    metadata=dict(runtime_source=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip(),
                  runtime_tree=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD^{tree}'],text=True).strip(),
                  reference_backend_sha256=REFERENCE_SHA256,
                  candidate_backend_sha256=hashlib.sha256(candidate.encode()).hexdigest(),
                  compiled_with_sanitizers=args.sanitize,
                  semantic_input_trace=args.trace_inputs,
                  existing_scene_test_sha256=sha(parent/'tests/gpu_scene_test.cpp'),
                  semantic_input_fingerprint_sha256=sha(parent/'tests/gpu_input_fingerprint.hpp'),
                  capture_instrumentation_sha256=sha(parent/'tests/gpu_scratch_capture.inc'))
    result=write_comparison(out,runs,metadata)
    assert runs['reference']['images']==runs['candidate']['images'],'Complete pixel mismatch; outputs retained'
    if args.trace_inputs:assert result['semantic_submission_inputs_equal'],'Successful submission inputs differ; producer fingerprints retained'
    assert all(runs['reference']['report'][k]==runs['candidate']['report'][k] for k in ('backend','device','software_device'))
    print(json.dumps({k:result[k] for k in ('success','unit_gpu_fixture','compared_images','compared_bytes','full_normalized_readbacks_equal','performance_accepted')}))


if __name__=='__main__':main()
