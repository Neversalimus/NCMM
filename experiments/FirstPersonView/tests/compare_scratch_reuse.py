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
    # Undo only the pass-local binding reuse; retain every earlier inverse
    # and the unchanged original frozen backend digest.
    if "#define NCMM_GPU_FRAGMENT_STATE_CACHE 1\n" in reference:
        fragment_inverse=[['#include "ncmm_gpu_constant_shadow.hpp"\n#include "ncmm_gpu_fragment_state.hpp"\n#define NCMM_GPU_FRAGMENT_STATE_CACHE 1\n', '#include "ncmm_gpu_constant_shadow.hpp"\n'], ['    uint64_t shadow_dispatches=0,constant_shadow_reuses=0;\n    uint64_t material_uniform_pushes=0,material_uniform_reuses=0,sampler_binds=0,sampler_binding_reuses=0,fragment_state_invalidations=0;\n', '    uint64_t shadow_dispatches=0,constant_shadow_reuses=0;\n'], ['    bool rigid48_vertices_=true;\n    bool reuse_fragment_state_=true;\n', '    bool rigid48_vertices_=true;\n'], ['        bool deferred_alpha=false;\n        fragment_state_cache fragment_state;\n', '        bool deferred_alpha=false;\n'], ['                    SDL_BindGPUGraphicsPipeline(pass,programs_.geometry(policy.index,rigid));\n                    fragment_state.invalidate();++recording_.fragment_state_invalidations;\n', '                    SDL_BindGPUGraphicsPipeline(pass,programs_.geometry(policy.index,rigid));\n'], ['                static_assert(sizeof(uniform)==48,"original fragment material word layout");\n                if(!reuse_fragment_state_||fragment_state.uniform_changed(&uniform,sizeof(uniform))){\n                    SDL_PushGPUFragmentUniformData(commands,1,&uniform,sizeof(uniform));++recording_.material_uniform_pushes;\n                }else ++recording_.material_uniform_reuses;\n', '                SDL_PushGPUFragmentUniformData(commands,1,&uniform,sizeof(uniform));\n'], ['                const std::array<fragment_binding_identity,3> bindings{{{textures[0].texture,textures[0].sampler},{textures[1].texture,textures[1].sampler},{textures[2].texture,textures[2].sampler}}};\n                if(!reuse_fragment_state_||fragment_state.samplers_changed(bindings)){\n                    SDL_BindGPUFragmentSamplers(pass,0,textures,3);++recording_.sampler_binds;\n                }else ++recording_.sampler_binding_reuses;\n                SDL_DrawGPUIndexedPrimitives(pass,range.count,b.count,range.first,0,0);', '                SDL_BindGPUFragmentSamplers(pass,0,textures,3);SDL_DrawGPUIndexedPrimitives(pass,range.count,b.count,range.first,0,0);'], ['    unsigned rigid48_program_count()const{return programs_.rigid_geometry_count();}\n    void reuse_fragment_state(bool enabled=true){reuse_fragment_state_=enabled;}\n', '    unsigned rigid48_program_count()const{return programs_.rigid_geometry_count();}\n']]
        for before,after in fragment_inverse:
            assert reference.count(before)==1,"Unreviewed fragment state reconstruction"
            reference=reference.replace(before,after,1)
        assert hashlib.sha256(reference.encode()).hexdigest()=='b557164a89f67619488652fd5ebd9c6fcc17217039ebeb41bb618edd38a434fa',"Fragment state inverse changed source72"
    # Strip only the reviewed private rigid48 fetch path; all previous
    # reconstruction recipes and the original frozen digest stay unchanged.
    if "#define NCMM_GPU_RIGID48_VERTEX_LAYOUT 1\n" in reference:
        rigid_inverse=[['        // A pipeline allocation can fail. Finish the complete format/state\n        // preflight before mapping staging or acquiring any frame commands;\n        // draw_batches consequently never discovers a missing format midpass.\n        if(!prepare_geometry_programs(batches)){const std::string error=context_.reason();invalidate();reason(error);return false;}\n        pick_zone.stop();\n', '        pick_zone.stop();\n'], ['if(!resources_.use_rigid48_vertices(compact_opaque_vertices_&&rigid48_vertices_)){reason(resources_.reason());return false;}context_.describe(metrics_);return true;}\n    bool available()', 'context_.describe(metrics_);return true;}\n    bool available()'], ['    resource_cache &resources(){return resources_;}\n    bool use_rigid48_vertices(bool enabled=true){\n        if(!resources_.use_rigid48_vertices(compact_opaque_vertices_&&enabled)){reason(resources_.reason());return false;}\n        rigid48_vertices_=enabled;return true;\n    }\n    unsigned rigid48_program_count()const{return programs_.rigid_geometry_count();}\n', '    resource_cache &resources(){return resources_;}\n'], ['                const bool rigid=mesh->vertex_stride==sizeof(rigid48::vertex);\n                const auto policy=range_policy(b,range,m,rigid,alpha_only);\n                const bool winding=policy.winding,component=policy.component,reflective=policy.reflective,glass=policy.glass,plain=policy.plain,bounded=policy.bounded;\n                if(policy.key!=opaque_policy){\n                    SDL_BindGPUGraphicsPipeline(pass,programs_.geometry(policy.index,rigid));\n                    opaque_policy=policy.key;if(rigid)++recording_.rigid48_pipeline_binds;\n                }\n                if(rigid)++recording_.rigid48_draws;else ++recording_.vertex72_draws;\n', '                const bool winding=selected==NCMM_GPU_OPAQUE&&b.winding_safe&&range.winding_safe;\n                const bool component=selected==NCMM_GPU_OPAQUE&&b.component_safe&&range.component_surface;\n                const bool reflective=component&&reflective_component_finish(m.finish);\n                const bool glass=selected==NCMM_GPU_TRANSPARENT&&b.component_safe&&range.glass_surface;\n                const bool plain=component&&plain_component_finish(m.finish);\n                const bool bounded=component&&b.depth_bound;\n                const unsigned policy=unsigned(winding)|(unsigned(component)<<1)|(unsigned(reflective)<<2)|(unsigned(glass)<<3)|(unsigned(plain)<<4)|(unsigned(bounded)<<5);\n                if(policy!=opaque_policy){\n                    if(selected==NCMM_GPU_TRANSPARENT)SDL_BindGPUGraphicsPipeline(pass,glass?programs_.transparent_glass:programs_.transparent);\n                    else SDL_BindGPUGraphicsPipeline(pass,bounded?\n                        (plain?(winding?programs_.opaque_plain_bound_culled:programs_.opaque_plain_bound):reflective?\n                         (winding?programs_.opaque_reflective_bound_culled:programs_.opaque_reflective_bound):\n                         (winding?programs_.opaque_component_bound_culled:programs_.opaque_component_bound)):plain?\n                        (winding?programs_.opaque_plain_culled:programs_.opaque_plain):reflective?\n                        (winding?programs_.opaque_reflective_culled:programs_.opaque_reflective):component?\n                        (winding?programs_.opaque_component_culled:programs_.opaque_component):(winding?programs_.opaque_culled:programs_.opaque));\n                    opaque_policy=policy;}\n'], ['    struct range_program {bool winding,component,reflective,glass,plain,bounded;unsigned key,index;};\n    static range_program range_policy(const batch &b,const mesh_range &range,const ncmm_gpu_material_v1 &m,bool rigid,bool alpha_only=false){\n        const uint32_t selected=b.pass;\n        const bool winding=selected==NCMM_GPU_OPAQUE&&b.winding_safe&&range.winding_safe;\n        const bool component=selected==NCMM_GPU_OPAQUE&&b.component_safe&&range.component_surface;\n        const bool reflective=component&&reflective_component_finish(m.finish);\n        const bool glass=selected==NCMM_GPU_TRANSPARENT&&b.component_safe&&range.glass_surface;\n        const bool plain=component&&plain_component_finish(m.finish);\n        const bool bounded=component&&b.depth_bound;\n        const unsigned key=unsigned(winding)|(unsigned(component)<<1)|(unsigned(reflective)<<2)|(unsigned(glass)<<3)|(unsigned(plain)<<4)|(unsigned(bounded)<<5)|(unsigned(rigid)<<6);\n        return {winding,component,reflective,glass,plain,bounded,key,programs::geometry_index(selected,alpha_only,winding,component,reflective,glass,plain,bounded)};\n    }\n    bool prepare_geometry_programs(const std::vector<batch> &batches){\n        for(const auto &b:batches){const auto *mesh=resources_.mesh(b.key);if(!mesh||mesh->vertex_stride!=sizeof(rigid48::vertex))continue;\n            for(const auto &range:mesh->ranges){const auto &material=mesh->materials[range.material];\n                const auto policy=range_policy(b,range,material,true,b.pass==NCMM_GPU_HELD&&material.color[3]<.999f);\n                if(!programs_.prepare_rigid(context_,policy.index))return false;\n            }\n        }\n        recording_.rigid48_cached_programs=programs_.rigid_geometry_count();return true;\n    }\n    bool draw_batches(SDL_GPUCommandBuffer *commands,', '    bool draw_batches(SDL_GPUCommandBuffer *commands,'], ['    bool compact_opaque_vertices_=true;\n    bool rigid48_vertices_=true;\n', '    bool compact_opaque_vertices_=true;\n'], ['    uint64_t shadow_dispatches=0,constant_shadow_reuses=0;\n    uint64_t rigid48_draws=0,vertex72_draws=0,rigid48_pipeline_binds=0,rigid48_cached_programs=0;\n', '    uint64_t shadow_dispatches=0,constant_shadow_reuses=0;\n'], ['#define NCMM_GPU_CONSTANT_SHADOW_CACHE 1\n#define NCMM_GPU_RIGID48_VERTEX_LAYOUT 1\n', '#define NCMM_GPU_CONSTANT_SHADOW_CACHE 1\n']]
        for before,after in rigid_inverse:
            assert reference.count(before)==1,"Unreviewed rigid48 reconstruction"
            reference=reference.replace(before,after,1)
        assert hashlib.sha256(reference.encode()).hexdigest()=='8c3fbf48ef85c348879cea40c8a4349371a8e516f6a1c66212f0732b20d27eae',"Rigid48 inverse changed the source71 backend"
    # Remove only the reviewed constant-one storage cache before reconstructing
    # the unchanged frozen backend; every inverse and final digest stays strict.
    if "#define NCMM_GPU_CONSTANT_SHADOW_CACHE 1\n" in reference:
        shadow_inverse=[['    void profile(bool enabled=true){profiling_=enabled;}\n    void reuse_constant_shadows(bool enabled=true){if(reuse_constant_shadows_!=enabled){reuse_constant_shadows_=enabled;constant_shadows_.invalidate();}}\n    // Private backend diagnostics for actual CI storage readback/lifetime controls.\n    SDL_GPUBuffer *constant_shadow_buffer()const{return shadows_;}\n    uint32_t constant_shadow_prefix()const{return constant_shadows_.initialized_prefix();}\n', '    void profile(bool enabled=true){profiling_=enabled;}\n'], ['        if(!reuse_shadows&&reuse_constant_shadows_&&scene.camera.grid[3]>.5f)constant_shadows_.submitted_constant(scene.cell_count);\n        white_ready_=cells_ready_=true;\n', '        white_ready_=cells_ready_=true;\n'], ['        // XYZ lighting is already propagated by the Host. Its shadow program\n        // writes exactly1.0 for every cell; retain only a successfully submitted\n        // initialized prefix. Fragment programs still read this owned buffer.\n        const bool reuse_shadows=constant_shadows_.reusable(reuse_constant_shadows_,scene.camera.grid[3],scene.cell_count);\n        SDL_GPUComputePass *compute=nullptr;\n        if(reuse_shadows){++recording_.constant_shadow_reuses;}else{\n            constant_shadows_.invalidate();\n            SDL_GPUStorageBufferReadWriteBinding shadow_binding{};shadow_binding.buffer=shadows_;shadow_binding.cycle=cycle_private_scratch();\n            compute=SDL_BeginGPUComputePass(commands,nullptr,0,&shadow_binding,1);if(!compute)return stop("begin shadow compute pass");SDL_BindGPUComputePipeline(compute,programs_.shadow);\n            SDL_BindGPUComputeStorageBuffers(compute,0,&cells_,1);SDL_PushGPUComputeUniformData(commands,0,&scene.camera,sizeof(scene.camera));SDL_DispatchGPUCompute(compute,(scene.cell_count+63)/64,1,1);SDL_EndGPUComputePass(compute);\n            ++recording_.shadow_dispatches;\n        }', '        SDL_GPUStorageBufferReadWriteBinding shadow_binding{};shadow_binding.buffer=shadows_;shadow_binding.cycle=cycle_private_scratch();\n        auto *compute=SDL_BeginGPUComputePass(commands,nullptr,0,&shadow_binding,1);if(!compute)return stop("begin shadow compute pass");SDL_BindGPUComputePipeline(compute,programs_.shadow);\n        SDL_BindGPUComputeStorageBuffers(compute,0,&cells_,1);SDL_PushGPUComputeUniformData(commands,0,&scene.camera,sizeof(scene.camera));SDL_DispatchGPUCompute(compute,(scene.cell_count+63)/64,1,1);SDL_EndGPUComputePass(compute);'], ['        // A newly allocated or cycled backing buffer has no initialized prefix.\n        if(!shadows_||shadow_size_<std::max(16u,scene.cell_count*4))constant_shadows_.invalidate();\n        if(!buffer(instances_,instance_size_,uint32_t(scene.draw_count*sizeof(instance)),SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ)||', '        if(!buffer(instances_,instance_size_,uint32_t(scene.draw_count*sizeof(instance)),SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ)||'], ['    void release_buffers(){constant_shadows_.invalidate();auto *d=context_.device();', '    void release_buffers(){auto *d=context_.device();'], ['    bool compact_opaque_vertices_=true;\n    bool reuse_constant_shadows_=true;\n    constant_shadow_cache constant_shadows_;\n', '    bool compact_opaque_vertices_=true;\n'], ['    double cell_prepare_ms=0;\n    uint64_t shadow_dispatches=0,constant_shadow_reuses=0;\n', '    double cell_prepare_ms=0;\n'], ['#include "ncmm_graphics_trace.hpp"\n#define NCMM_GPU_CONSTANT_SHADOW_CACHE 1\n#include "ncmm_gpu_constant_shadow.hpp"\n', '#include "ncmm_graphics_trace.hpp"\n']]
        for before,after in shadow_inverse:
            assert reference.count(before)==1,"Unreviewed constant-shadow reconstruction"
            reference=reference.replace(before,after,1)
    # Empty visible groups and a zero-bone scene are valid public inputs.
    # The new backend avoids passing null+zero arrays to memcpy; retain the
    # exact historical backend in the frozen reference control nevertheless.
    if 'if(!instances.empty()){' in reference:
        before=('        stage=clock::now();uint32_t offset=0;\n'
            '        if(!instances.empty()){\n'
            '            std::memcpy(mapped+offset,instances.data(),instances.size()*sizeof(instance));\n'
            '        }\n'
            '        offset+=uint32_t(instances.size()*sizeof(instance));\n'
            '        if(scene.bone_count){\n'
            '            std::memcpy(mapped+offset,scene.bones,scene.bone_count*sizeof(ncmm_gpu_matrix_v1));\n'
            '        }\n'
            '        offset+=scene.bone_count*sizeof(ncmm_gpu_matrix_v1);')
        after=('        stage=clock::now();uint32_t offset=0;std::memcpy(mapped+offset,instances.data(),instances.size()*sizeof(instance));offset+=uint32_t(instances.size()*sizeof(instance));\n'
            '        std::memcpy(mapped+offset,scene.bones,scene.bone_count*sizeof(ncmm_gpu_matrix_v1));offset+=scene.bone_count*sizeof(ncmm_gpu_matrix_v1);')
        assert reference.count(before)==1,'Unreviewed empty-upload reconstruction'
        reference=reference.replace(before,after,1)
        assert hashlib.sha256(reference.encode()).hexdigest()=='5ec8692d83d20a584923fa2c32842d3dde5aaa035307764654971419c897df3e','Empty-upload inverse changed source67'
    # Reverse only the three private packing opt-in edits. The reconstructed
    # backend never opts in, so the shared cache retains its original uploads.
    # Require the exact source66 backend before any older frozen recipe runs.
    if '    bool compact_opaque_vertices_=true;\n' in reference:
        for before,after in [
            ('    bool compact_opaque_vertices_=true;\n',''),
            ('    explicit backend(bool compact_opaque_vertices=true):compact_opaque_vertices_(compact_opaque_vertices){metrics_.struct_size=sizeof(metrics_);metrics_.gpu_ms=-1;metrics_.target_cell=-1;}',
             '    backend(){metrics_.struct_size=sizeof(metrics_);metrics_.gpu_ms=-1;metrics_.target_cell=-1;}'),
            ('resources_.bind(context_.device());if(!resources_.use_compact_opaque_vertices(compact_opaque_vertices_)){reason(resources_.reason());return false;}context_.describe(metrics_);return true;',
             'resources_.bind(context_.device());context_.describe(metrics_);return true;')]:
            assert reference.count(before)==1,'Unreviewed private vertex packing reconstruction'
            reference=reference.replace(before,after,1)
        assert hashlib.sha256(reference.encode()).hexdigest()=='c5c6f97068422f45e6527331bb75e946c20ac3893850852180c202592f9400b7','Vertex packing inverse changed the source66 backend'
    # Remove only the reviewed opaque-span addition. Hash each complete new
    # block before removal, then require the exact previous backend bytes.
    # Every older inverse recipe and the original final digest remain below.
    if '#define NCMM_GPU_GROUP_DEPTH_PROGRAM 1\n' in reference:
        for before,after in [
            ('#define NCMM_GPU_GROUP_DEPTH_PROGRAM 1\n',''),
            ('    uint64_t bounded_depth_draws=0,group_depth_draws=0,opaque_depth_spans=0;','    uint64_t bounded_depth_draws=0;'),
            ('    uint64_t conservative_depth_draws_=0,group_depth_draws_=0;\n    bool group_depth_enabled_=true;','    uint64_t conservative_depth_draws_=0;'),
            ('    void group_depth(bool enabled=true){group_depth_enabled_=enabled;}\n    uint64_t group_depth_draws()const{return group_depth_draws_;}\n','')]:
            assert reference.count(before)==1,'Unreviewed opaque-span field reconstruction'
            reference=reference.replace(before,after)
        begin=reference.index('    void prefill_opaque_group(');end=reference.index('    bool draw_batches(',begin)
        assert hashlib.sha256(reference[begin:end].encode()).hexdigest()=='689e63044185f512419eb9de5a2107d56bdcc404e69b9330bdabc28e9c286667','Unreviewed opaque-span draw reconstruction'
        reference=reference[:begin]+reference[end:]
        begin=reference.index('        size_t depth_span_end=0;\n')
        end=reference.index('            auto *mesh=resources_.mesh(b.key);if(!mesh)continue;\n',begin)+len('            auto *mesh=resources_.mesh(b.key);if(!mesh)continue;\n')
        assert hashlib.sha256(reference[begin:end].encode()).hexdigest()=='530201728203b31451fba0b5f49452c90b2fdd94be6c737a76ff38b0097ab989','Unreviewed opaque-span ordering reconstruction'
        reference=reference[:begin]+'        for(const auto &b:batches)if(b.pass==selected){auto *mesh=resources_.mesh(b.key);if(!mesh)continue;\n'+reference[end:]
        assert hashlib.sha256(reference.encode()).hexdigest()=='c5c6f97068422f45e6527331bb75e946c20ac3893850852180c202592f9400b7','Opaque-span inverse changed the previous backend'
    # Reverse only the bounded-depth backend addition before the frozen recipes.
    depth_bound_replacements=[['#include "ncmm_gpu_programs.hpp"', '#include "ncmm_gpu_programs.hpp"\n#include "ncmm_gpu_conservative_depth.hpp"\n#define NCMM_GPU_CONSERVATIVE_DEPTH_PROGRAM 1'], ['    bool policy_counts_=false;', '    bool policy_counts_=false;\n    bool conservative_depth_enabled_=true;\n    uint64_t conservative_depth_draws_=0;'], ['    uint64_t counted_plain_draws=0,counted_glass_draws=0;', '    uint64_t counted_plain_draws=0,counted_glass_draws=0;\n    uint64_t bounded_depth_draws=0;'], ['    struct batch {uint64_t key;uint32_t pass,first,count;bool winding_safe=false,component_safe=false;};', '    struct batch {uint64_t key;uint32_t pass,first,count;bool winding_safe=false,component_safe=false,depth_bound=false;};'], ['    void count_policies(bool enabled=true){policy_counts_=enabled;}', '    void count_policies(bool enabled=true){policy_counts_=enabled;}\n    void conservative_depth(bool enabled=true){conservative_depth_enabled_=enabled;}\n    uint64_t conservative_depth_draws()const{return conservative_depth_draws_;}'], ['                const bool plain=component&&plain_component_finish(m.finish);\n                const unsigned policy=unsigned(winding)|(unsigned(component)<<1)|(unsigned(reflective)<<2)|(unsigned(glass)<<3)|(unsigned(plain)<<4);', '                const bool plain=component&&plain_component_finish(m.finish);\n                const bool bounded=component&&b.depth_bound;\n                const unsigned policy=unsigned(winding)|(unsigned(component)<<1)|(unsigned(reflective)<<2)|(unsigned(glass)<<3)|(unsigned(plain)<<4)|(unsigned(bounded)<<5);'], ['                    else SDL_BindGPUGraphicsPipeline(pass,plain?', '                    else SDL_BindGPUGraphicsPipeline(pass,bounded?\n                        (plain?(winding?programs_.opaque_plain_bound_culled:programs_.opaque_plain_bound):reflective?\n                         (winding?programs_.opaque_reflective_bound_culled:programs_.opaque_reflective_bound):\n                         (winding?programs_.opaque_component_bound_culled:programs_.opaque_component_bound)):plain?'], ['                if(profiling_&&plain)++recording_.plain_draws;', '                if(profiling_&&plain)++recording_.plain_draws;\n                if(bounded){++conservative_depth_draws_;if(profiling_||policy_counts_)++recording_.bounded_depth_draws;}'], ['            batches.push_back({key.second,key.first,uint32_t(instances.size()),uint32_t(list.size()),winding,component});instances.insert(instances.end(),list.begin(),list.end());}', '            auto *mesh=resources_.mesh(key.second);\n            bool bounded=conservative_depth_enabled_&&component&&mesh&&!mesh->dynamic&&mesh->joint_count==1&&mesh->joints.size()==1&&mesh->joints[0].valid()&&\n                !mesh->ranges.empty()&&std::all_of(mesh->ranges.begin(),mesh->ranges.end(),[](const mesh_range &r){return r.component_surface;});\n            if(bounded){depth_values.clear();depth_values.reserve(list.size());for(const auto &i:list){\n                if(i.reserved!=0){bounded=false;break;}\n                const conservative_depth::point eye{{scene.camera.eye[0],scene.camera.eye[1],scene.camera.eye[2]}},forward{{scene.camera.forward[0],scene.camera.forward[1],scene.camera.forward[2]}};\n                const auto value=conservative_depth::lower_bound(mesh->joints[0].lo,mesh->joints[0].hi,i.world,eye,forward,scene.camera.projection[3]);\n                if(!value){bounded=false;break;}depth_values.push_back(*value);\n            }}\n            const auto first=uint32_t(instances.size());\n            batches.push_back({key.second,key.first,first,uint32_t(list.size()),winding,component,bounded});instances.insert(instances.end(),list.begin(),list.end());\n            // Only this all-qualified group uses the alternate vertex program.\n            // Its private instance word carries a strict lower bound; the old\n            // zero-heading value is preserved for every unproved group.\n            if(bounded)for(size_t i=0;i<depth_values.size();++i)std::memcpy(&instances[first+i].reserved,&depth_values[i],sizeof(float));}'], ['        for(const auto &[key,list]:groups){', '        std::vector<float> depth_values;\n        for(const auto &[key,list]:groups){']]
    for before,after in reversed(depth_bound_replacements):
        assert reference.count(after)==1,"Unreviewed conservative depth reconstruction"
        reference=reference.replace(after,before)
    # Remove only these private diagnostic counters before every old recipe.
    # No program, input proof or frozen digest changes with this addition.
    diagnostic_replacements=[('    uint64_t opaque_component_draws=0,opaque_winding_draws=0,opaque_requested_winding_draws=0,opaque_range_rejections=0,opaque_batch_rejections=0,opaque_reflective_draws=0;\n    uint64_t opaque_component_triangles=0,opaque_winding_triangles=0;\n    uint64_t counted_plain_draws=0,counted_glass_draws=0;\n', ''), ('                if(profiling_||policy_counts_){\n                    if(plain){++recording_.counted_plain_draws;}\n                    if(glass){++recording_.counted_glass_draws;}\n                    if(selected==NCMM_GPU_OPAQUE){\n                        recording_.opaque_component_draws+=component;recording_.opaque_winding_draws+=winding;\n                        recording_.opaque_requested_winding_draws+=bool(m.flags&NCMM_GPU_CULL_BACK);\n                        recording_.opaque_range_rejections+=bool(m.flags&NCMM_GPU_CULL_BACK)&&!range.winding_safe;\n                        recording_.opaque_batch_rejections+=range.winding_safe&&!b.winding_safe;\n                        recording_.opaque_reflective_draws+=reflective;\n                        const uint64_t triangles=uint64_t(range.count/3)*b.count;\n                        if(component)recording_.opaque_component_triangles+=triangles;\n                        if(winding)recording_.opaque_winding_triangles+=triangles;\n                    }\n                }\n', ''), ('    bool policy_counts_=false;\n', ''), ('    void count_policies(bool enabled=true){policy_counts_=enabled;}\n', '')]
    for before,after in diagnostic_replacements:
        assert reference.count(before)==1,'Unreviewed policy diagnostics'
        reference=reference.replace(before,after)
    # Reverse only the exact finish-zero addition before the old recipes.
    # The original frozen digest and every previous rejection control remain.
    if '#define NCMM_GPU_COMPONENT_PLAIN_PROGRAM 1\n' in reference:
        plain_replacements=[('                if(profiling_&&glass)++recording_.glass_draws;\n                if(profiling_&&plain)++recording_.plain_draws;', '                if(profiling_&&glass)++recording_.glass_draws;'), ('                    else SDL_BindGPUGraphicsPipeline(pass,plain?\n                        (winding?programs_.opaque_plain_culled:programs_.opaque_plain):reflective?\n                        (winding?programs_.opaque_reflective_culled:programs_.opaque_reflective):component?', '                    else SDL_BindGPUGraphicsPipeline(pass,reflective?\n                        (winding?programs_.opaque_reflective_culled:programs_.opaque_reflective):component?'), ('                const bool glass=selected==NCMM_GPU_TRANSPARENT&&b.component_safe&&range.glass_surface;\n                const bool plain=component&&plain_component_finish(m.finish);\n                const unsigned policy=unsigned(winding)|(unsigned(component)<<1)|(unsigned(reflective)<<2)|(unsigned(glass)<<3)|(unsigned(plain)<<4);', '                const bool glass=selected==NCMM_GPU_TRANSPARENT&&b.component_safe&&range.glass_surface;\n                const unsigned policy=unsigned(winding)|(unsigned(component)<<1)|(unsigned(reflective)<<2)|(unsigned(glass)<<3);'), ('    uint64_t glass_draws=0;\n    uint64_t plain_draws=0;', '    uint64_t glass_draws=0;'), ('#define NCMM_GPU_COMPONENT_GLASS_PROGRAM 1\n#define NCMM_GPU_COMPONENT_PLAIN_PROGRAM 1', '#define NCMM_GPU_COMPONENT_GLASS_PROGRAM 1')]
        for before,after in plain_replacements:
            assert reference.count(before)==1,'Unreviewed plain reconstruction'
            reference=reference.replace(before,after)
    # Remove only the reviewed glass policy before the existing reconstruction.
    # The unchanged digest below still rejects every unrelated modification.
    if '#define NCMM_GPU_COMPONENT_GLASS_PROGRAM 1\n' in reference:
        glass_replacements=[('#define NCMM_GPU_COMPONENT_GLASS_PROGRAM 1\n', ''), ('    uint64_t glass_draws=0;\n', ''), ('                const bool glass=selected==NCMM_GPU_TRANSPARENT&&b.component_safe&&range.glass_surface;\n                const unsigned policy=unsigned(winding)|(unsigned(component)<<1)|(unsigned(reflective)<<2)|(unsigned(glass)<<3);\n                if(policy!=opaque_policy){\n                    if(selected==NCMM_GPU_TRANSPARENT)SDL_BindGPUGraphicsPipeline(pass,glass?programs_.transparent_glass:programs_.transparent);\n                    else SDL_BindGPUGraphicsPipeline(pass,reflective?\n                        (winding?programs_.opaque_reflective_culled:programs_.opaque_reflective):component?\n                        (winding?programs_.opaque_component_culled:programs_.opaque_component):(winding?programs_.opaque_culled:programs_.opaque));\n                    opaque_policy=policy;}\n                if(profiling_&&glass)++recording_.glass_draws;', '                const unsigned policy=unsigned(winding)|(unsigned(component)<<1)|(unsigned(reflective)<<2);\n                if(policy!=opaque_policy){SDL_BindGPUGraphicsPipeline(pass,reflective?\n                    (winding?programs_.opaque_reflective_culled:programs_.opaque_reflective):component?\n                    (winding?programs_.opaque_component_culled:programs_.opaque_component):(winding?programs_.opaque_culled:programs_.opaque));opaque_policy=policy;}'), ('            const bool component=d.cell<scene.cell_count&&d.bone_count==1&&d.bone_offset<scene.bone_count&&\n                winding_camera(scene.camera)&&winding_pose(d.transform)&&winding_bone(scene.bones[d.bone_offset]);\n            uint32_t heading;std::memcpy(&heading,&d.creature_yaw,4);batches.push_back({d.mesh_key,d.pass,uint32_t(instances.size()),1,false,component});instances.push_back({d.transform,d.bone_offset,d.bone_count,d.cell,heading});}', '            uint32_t heading;std::memcpy(&heading,&d.creature_yaw,4);batches.push_back({d.mesh_key,d.pass,uint32_t(instances.size()),1});instances.push_back({d.transform,d.bone_offset,d.bone_count,d.cell,heading});}')]
        for before,after in glass_replacements:
            assert reference.count(before)==1,'Unreviewed glass reconstruction'
            reference=reference.replace(before,after)
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


def diagnostic_no_fixed_cull_programs(programs):
    """One diagnostic pipeline-state ablation, never the ordinary pixel gate."""
    assert hashlib.sha256(programs.encode()).hexdigest()=='46908cc5242e403c60909f1d71f07f9212ab03e52c64524995e299c4309518e0','Unreviewed diagnostic pipeline source'
    before='if(cull){p.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_BACK;'
    after='if(cull){p.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;'
    assert programs.count(before)==1,'Diagnostic must change only the existing fixed-function cull state'
    return programs.replace(before,after)


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
    parser.add_argument('--diagnostic-no-fixed-cull',action='store_true',help='Separate traced ablation of fixed-function cull state; never replaces the primary gate')
    args=parser.parse_args()
    if args.diagnostic_no_fixed_cull and not args.trace_inputs:parser.error('The cull ablation requires separate --trace-inputs diagnostics')
    repo=args.repository.resolve();out=args.output.resolve()
    assert not out.exists(),'Use a fresh directory; preserve earlier output'
    assert not (args.sanitize and os.name=='nt'),'Use the compiler-specific Windows ASan fixture'
    assert args.sdl_dir or args.toolchain_file,'Use the same explicit SDL configuration as the original GPU fixture'
    parent=repo/'experiments/FirstPersonView';backend=repo/'experiments/graphics_bridge/ncmm_gpu_backend.hpp'
    candidate=backend.read_text(encoding='utf-8').replace('\r\n','\n');reference=reference_backend(candidate)
    out.mkdir(parents=True);(out/'reference').mkdir();(out/'reference/ncmm_gpu_backend.hpp').write_text(reference,encoding='utf-8',newline='\n')
    programs=(repo/'experiments/graphics_bridge/ncmm_gpu_programs.hpp').read_text(encoding='utf-8')
    compiled_programs=diagnostic_no_fixed_cull_programs(programs) if args.diagnostic_no_fixed_cull else programs
    if args.diagnostic_no_fixed_cull:
        # Relocate an unchanged candidate backend so its quoted include finds
        # precisely this one-state programs variant. Producer/reference/shaders
        # and every existing scene assertion/readback remain unchanged.
        (out/'candidate').mkdir()
        (out/'candidate/ncmm_gpu_backend.hpp').write_text(candidate,encoding='utf-8',newline='\n')
        (out/'candidate/ncmm_gpu_programs.hpp').write_text(compiled_programs,encoding='utf-8',newline='\n')
    capture = '#include "'+(parent/'tests/gpu_scratch_capture.inc').as_posix()+'"\n'
    capture += '#include "'+(parent/'tests/gpu_fixture_producer.hpp').as_posix()+'"\n#define scene_cache fixture_scene_cache\n'
    (out/'capture.cpp').write_text(capture+'#include "'+(parent/'tests/gpu_scene_test.cpp').as_posix()+'"\n',encoding='utf-8',newline='\n')
    cmake='''cmake_minimum_required(VERSION 3.20)
project(ScratchReusePixelComparison LANGUAGES CXX)
set(FIRST_PERSON_BUILD_GPU_TESTS OFF CACHE BOOL "Only explicit paired probes")
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
add_subdirectory("'''+parent.as_posix()+'''" frozen EXCLUDE_FROM_ALL)
find_package(SDL3 3.4 CONFIG REQUIRED)
# One immutable producer object is linked into both independently compiled
# backend controls. No LTO/IPO can specialize it for either backend.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
add_library(scratch_producer OBJECT "'''+(parent/'tests/gpu_fixture_producer.cpp').as_posix()+'''")
first_person_target(scratch_producer)
foreach(variant reference candidate)
  add_executable(scratch_${variant} capture.cpp $<TARGET_OBJECTS:scratch_producer>)
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
    if args.diagnostic_no_fixed_cull:
        marker='  if(variant STREQUAL "reference")'
        assert cmake.count(marker)==1
        cmake=cmake.replace(marker,'  if(variant STREQUAL "candidate")\n    target_include_directories(scratch_${variant} BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/candidate")\n  endif()\n'+marker)
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
    producer_objects=list((out/'build').rglob('gpu_fixture_producer.cpp.o'))+list((out/'build').rglob('gpu_fixture_producer.obj'))
    assert len(producer_objects)==1,'Both controls require the one original producer object'
    producer_object_sha256=sha(producer_objects[0])
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
                  diagnostic_program_ablation='fixed-cull-none' if args.diagnostic_no_fixed_cull else 'none',
                  shipping_programs_sha256=hashlib.sha256(programs.encode()).hexdigest(),
                  candidate_programs_sha256=hashlib.sha256(compiled_programs.encode()).hexdigest(),
                  existing_scene_test_sha256=sha(parent/'tests/gpu_scene_test.cpp'),
                  semantic_input_fingerprint_sha256=sha(parent/'tests/gpu_input_fingerprint.hpp'),
                  capture_instrumentation_sha256=sha(parent/'tests/gpu_scratch_capture.inc'),
                  shared_producer_object_sha256=producer_object_sha256,
                  shared_producer_source_sha256=sha(parent/'tests/gpu_fixture_producer.cpp'),
                  shared_producer_header_sha256=sha(parent/'tests/gpu_fixture_producer.hpp'),
                  shared_producer_compiled_once=True,shared_producer_IPO=False)
    result=write_comparison(out,runs,metadata)
    assert runs['reference']['images']==runs['candidate']['images'],'Complete pixel mismatch; outputs retained'
    if args.trace_inputs:assert result['semantic_submission_inputs_equal'],'Successful submission inputs differ; producer fingerprints retained'
    assert all(runs['reference']['report'][k]==runs['candidate']['report'][k] for k in ('backend','device','software_device'))
    print(json.dumps({k:result[k] for k in ('success','unit_gpu_fixture','compared_images','compared_bytes','full_normalized_readbacks_equal','performance_accepted')}))


if __name__=='__main__':main()
