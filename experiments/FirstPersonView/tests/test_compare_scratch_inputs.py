"""Pure numeric diagnostics only: no renderer, SDL, device or Native process."""
import importlib.util
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('scratch_comparison',Path(__file__).with_name('compare_scratch_reuse.py'))
comparison=importlib.util.module_from_spec(spec);spec.loader.exec_module(comparison)

class Diagnostics(unittest.TestCase):
    def backend(self):
        return (Path(__file__).resolve().parents[2]/'graphics_bridge/ncmm_gpu_backend.hpp').read_text(encoding='utf-8')
    def retired_span_backend(self):
        source=(Path(__file__).with_name('reference')/'gpu_backend_opaque_span.hpp').read_text(encoding='utf-8')
        self.assertEqual(hashlib.sha256(source.encode()).hexdigest(),'d502e3356f7ac6fe83c709313a97248af2d779ed40c31f85f59051328e0d93c0')
        return source
    def test_retired_span_reference_keeps_original_frozen_bytes(self):
        reference=comparison.reference_backend(self.retired_span_backend())
        self.assertEqual(hashlib.sha256(reference.encode()).hexdigest(),'a0dd1aa83c05e1a51ac33854dedb54696e85616857d9da1f68bb30ee7fe9662a')
    def test_shipping_backend_excludes_retired_span_pass(self):
        source=self.backend()
        self.assertNotIn('#define NCMM_GPU_GROUP_DEPTH_PROGRAM',source)
        self.assertNotIn('prefill_opaque_group(',source)
        self.assertEqual(hashlib.sha256(source.encode()).hexdigest(),'08bd3d7bffb2cf51c1f4c2325e5942ccabaafa7e02168208dfff28bff4c76434')
    def test_changed_palette_default_cannot_reconstruct(self):
        source=self.backend().replace('bool material_palette_enabled_=true;','bool material_palette_enabled_=false;')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_palette_uniform_size_cannot_reconstruct(self):
        source=self.backend().replace('uniform.data(),uint32_t(sizeof(uniform))','uniform.data(),32')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_palette_caller_proof_cannot_reconstruct(self):
        source=self.backend().replace('batches.back().palette=material_palette::instance_order(source,poses,scene.camera);','batches.back().palette=true;')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_palette_state_reset_cannot_reconstruct(self):
        source=self.backend().replace('fragment_state.invalidate();++recording_.fragment_state_invalidations;opaque_policy=~0u;','opaque_policy=~0u;')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_palette_cannot_leave_ordinary_zero_policy_bound_to_palette(self):
        source=self.backend().replace('fragment_state.invalidate();++recording_.fragment_state_invalidations;opaque_policy=~0u;','fragment_state.invalidate();++recording_.fragment_state_invalidations;opaque_policy=0;')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_fragment_state_default_cannot_reconstruct(self):
        source=self.backend().replace('    bool reuse_fragment_state_=true;','    bool reuse_fragment_state_=false;')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_fragment_state_lifetime_cannot_reconstruct(self):
        source=self.backend().replace('        fragment_state_cache fragment_state;','        static fragment_state_cache fragment_state;')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_fragment_state_pipeline_reset_cannot_reconstruct(self):
        source=self.backend().replace('                    fragment_state.invalidate();++recording_.fragment_state_invalidations;','                    ++recording_.fragment_state_invalidations;')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_fragment_state_uniform_bytes_cannot_reconstruct(self):
        source=self.backend().replace('fragment_state.uniform_changed(&uniform,sizeof(uniform))','fragment_state.uniform_changed(&uniform,32)')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_fragment_state_uniform_guard_cannot_reconstruct(self):
        source=self.backend().replace('if(!reuse_fragment_state_||fragment_state.uniform_changed','if(reuse_fragment_state_||fragment_state.uniform_changed')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_fragment_state_sampler_guard_cannot_reconstruct(self):
        source=self.backend().replace('if(!reuse_fragment_state_||fragment_state.samplers_changed','if(reuse_fragment_state_||fragment_state.samplers_changed')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_fragment_state_sampler_identity_cannot_reconstruct(self):
        source=self.backend().replace('{textures[2].texture,textures[2].sampler}','{textures[1].texture,textures[1].sampler}')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_fragment_state_toggle_cannot_reconstruct(self):
        source=self.backend().replace('reuse_fragment_state_=enabled;','reuse_fragment_state_=!enabled;')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_rigid48_default_cannot_reconstruct(self):
        source=self.backend().replace('    bool rigid48_vertices_=true;','    bool rigid48_vertices_=false;')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_rigid48_layout_rebind_cannot_reconstruct(self):
        source=self.backend().replace('|(unsigned(rigid)<<6)','')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_rigid48_held_alpha_cannot_reconstruct(self):
        source=self.backend().replace('alpha_only?programs_.held_alpha:programs_.held','alpha_only?programs_.held:programs_.held_alpha')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_rigid48_preflight_cannot_reconstruct(self):
        source=self.backend().replace('if(!prepare_geometry_programs(batches))','if(false)')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_rigid48_proof_cannot_reconstruct(self):
        source=self.backend().replace('mesh->vertex_stride!=sizeof(rigid48::vertex)','mesh->vertex_stride==sizeof(rigid48::vertex)')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_rigid48_kind_cannot_reconstruct(self):
        source=self.backend().replace('programs::geometry_index(selected,alpha_only,winding,component,reflective,glass,plain,bounded)','programs::geometry_index(selected,alpha_only,winding,component,reflective,glass,plain,false)')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_diagnostic_rejects_changed_rigid48_pitch(self):
        programs=(Path(__file__).resolve().parents[2]/'graphics_bridge/ncmm_gpu_programs.hpp').read_text(encoding='utf-8')
        changed=programs.replace('layout.pitch=rigid?sizeof(rigid48::vertex):sizeof(ncmm_gpu_vertex_v1);','layout.pitch=sizeof(ncmm_gpu_vertex_v1);')
        self.assertNotEqual(changed,programs)
        with self.assertRaises(AssertionError):comparison.diagnostic_no_fixed_cull_programs(changed)

    def test_diagnostic_rejects_changed_rigid48_offsets(self):
        programs=(Path(__file__).resolve().parents[2]/'graphics_bridge/ncmm_gpu_programs.hpp').read_text(encoding='utf-8')
        changed=programs.replace('rigid?rigid48::offsets[i]:offsets[i]','offsets[i]')
        self.assertNotEqual(changed,programs)
        with self.assertRaises(AssertionError):comparison.diagnostic_no_fixed_cull_programs(changed)

    def test_changed_constant_shadow_default_cannot_reconstruct(self):
        source=self.backend().replace('    bool reuse_constant_shadows_=true;','    bool reuse_constant_shadows_=false;')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_constant_shadow_resize_cannot_reconstruct(self):
        source=self.backend().replace('shadow_size_<std::max(16u,scene.cell_count*4)','shadow_size_>std::max(16u,scene.cell_count*4)')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_constant_shadow_lifetime_cannot_reconstruct(self):
        source=self.backend().replace('release_buffers(){constant_shadows_.invalidate();','release_buffers(){')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_constant_shadow_prefix_cannot_reconstruct(self):
        source=self.backend().replace('constant_shadows_.submitted_constant(scene.cell_count);','constant_shadows_.submitted_constant(scene.cell_count+1);')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_constant_shadow_submit_guard_cannot_reconstruct(self):
        source=self.backend().replace('!reuse_shadows&&reuse_constant_shadows_','reuse_shadows&&reuse_constant_shadows_')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_constant_shadow_dispatch_cannot_reconstruct(self):
        source=self.backend().replace('if(reuse_shadows){++recording_.constant_shadow_reuses;}','if(!reuse_shadows){++recording_.constant_shadow_reuses;}')
        with self.assertRaises(AssertionError): comparison.reference_backend(source)

    def test_changed_empty_instance_copy_guard_cannot_reconstruct(self):
        source=self.backend().replace('if(!instances.empty()){','if(instances.empty()){')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_empty_bone_copy_guard_cannot_reconstruct(self):
        source=self.backend().replace('if(scene.bone_count){','if(!scene.bone_count){')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_original_cache_default_and_current_explicit_packing_are_separate(self):
        resource=(Path(__file__).resolve().parents[2]/'graphics_bridge/ncmm_gpu_resources.hpp').read_text(encoding='utf-8')
        self.assertIn('    bool compact_opaque_vertices_=false;',resource)
        self.assertNotIn('compact_opaque_vertices_',comparison.reference_backend(self.backend()))
        self.assertIn('resources_.use_compact_opaque_vertices(compact_opaque_vertices_)',self.backend())
    def test_changed_packing_default_cannot_reconstruct(self):
        source=self.backend().replace('backend(bool compact_opaque_vertices=true)','backend(bool compact_opaque_vertices=false)')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_packing_failure_guard_cannot_reconstruct(self):
        source=self.backend().replace('if(!resources_.use_compact_opaque_vertices(compact_opaque_vertices_))','if(resources_.use_compact_opaque_vertices(compact_opaque_vertices_))')
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_unrelated_packing_inverse_backend_edit_cannot_reconstruct(self):
        source=self.backend().replace('metrics_.gpu_ms=-1;','metrics_.gpu_ms=0;',1)
        self.assertNotEqual(source,self.backend())
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_diagnostic_fixed_cull_ablation_changes_only_one_pipeline_state(self):
        programs=(Path(__file__).resolve().parents[2]/'graphics_bridge/ncmm_gpu_programs.hpp').read_text(encoding='utf-8')
        changed=comparison.diagnostic_no_fixed_cull_programs(programs)
        self.assertEqual(changed,programs.replace('if(cull){p.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_BACK;','if(cull){p.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;'))
        with self.assertRaises(AssertionError):comparison.diagnostic_no_fixed_cull_programs(changed)
    def test_diagnostic_fixed_cull_rejects_an_unrelated_pipeline_edit(self):
        programs=(Path(__file__).resolve().parents[2]/'graphics_bridge/ncmm_gpu_programs.hpp').read_text(encoding='utf-8')
        changed=programs.replace('SDL_GPU_FRONTFACE_CLOCKWISE','SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE')
        self.assertNotEqual(programs,changed)
        with self.assertRaises(AssertionError):comparison.diagnostic_no_fixed_cull_programs(changed)
    def test_reconstructed_reference_keeps_frozen_bytes(self):
        reference=comparison.reference_backend(self.backend())
        self.assertEqual(hashlib.sha256(reference.encode()).hexdigest(),'a0dd1aa83c05e1a51ac33854dedb54696e85616857d9da1f68bb30ee7fe9662a')
        self.assertNotIn('opaque_color_replacement(',reference)
        self.assertNotIn('bound_pipeline',reference)
    def test_changed_span_alpha_barrier_cannot_reconstruct(self):
        source=self.retired_span_backend().replace('return r.alpha_one;','return true;')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_span_bound_guard_cannot_reconstruct(self):
        source=self.retired_span_backend().replace('if(!b.depth_bound)return;','if(!b.component_safe)return;')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_span_extra_work_accounting_cannot_reconstruct(self):
        source=self.retired_span_backend().replace('++metrics_.draw_calls;metrics_.triangles+=uint64_t(range.count/3)*b.count;','metrics_.triangles+=uint64_t(range.count/3)*b.count;',1)
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_span_policy_rebind_cannot_reconstruct(self):
        source=self.retired_span_backend().replace('opaque_policy=~0u;','opaque_policy=0;')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_span_prefill_colour_binding_cannot_reconstruct(self):
        source=self.retired_span_backend().replace('uniform.state[0]=m.flags;uniform.state[1]=m.finish;','uniform.state[0]=0;uniform.state[1]=m.finish;',1)
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_span_enable_guard_cannot_reconstruct(self):
        source=self.retired_span_backend().replace('selected==NCMM_GPU_OPAQUE&&group_depth_enabled_&&at>=depth_span_end','selected==NCMM_GPU_OPAQUE&&at>=depth_span_end')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_pipeline_policy_cannot_reconstruct(self):
        source=self.backend().replace('selected==NCMM_GPU_OPAQUE?programs_.opaque:','selected==NCMM_GPU_OPAQUE?programs_.transparent:')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_unrelated_backend_edit_cannot_reconstruct(self):
        source=self.backend().replace('metrics_.cpu_raster_calls=0;','metrics_.cpu_raster_calls=1;')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_glass_policy_cannot_reconstruct(self):
        source=self.backend().replace('if(profiling_&&glass)++recording_.glass_draws;','if(profiling_&&glass)recording_.glass_draws+=2;')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_glass_batch_proof_cannot_reconstruct(self):
        source=self.backend().replace('const bool component=d.cell<scene.cell_count&&d.bone_count==1&&','const bool component=d.cell<=scene.cell_count&&d.bone_count==1&&')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_plain_counter_cannot_reconstruct(self):
        source=self.backend().replace('if(profiling_&&plain)++recording_.plain_draws;','if(profiling_&&plain)recording_.plain_draws+=2;')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_plain_finish_proof_cannot_reconstruct(self):
        source=self.backend().replace('const bool plain=component&&plain_component_finish(m.finish);','const bool plain=plain_component_finish(m.finish);')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_policy_diagnostic_cannot_reconstruct(self):
        source=self.backend().replace('recording_.opaque_component_draws+=component;','recording_.opaque_component_draws+=!component;')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def test_changed_policy_guard_cannot_reconstruct(self):
        source=self.backend().replace('if(profiling_||policy_counts_){','if(profiling_&&policy_counts_){')
        with self.assertRaises(AssertionError):comparison.reference_backend(source)
    def image(self,pixel):return struct.pack('<III',1,1,373694468)+bytes(pixel)
    def test_complete_equality(self):
        result=comparison.image_difference(self.image([255,20,30,40]),self.image([255,20,30,40]))
        self.assertEqual(result['different_pixels'],0);self.assertEqual(result['storage_byte_differences'],[0,0,0,0])
    def test_red_is_not_alpha(self):
        result=comparison.image_difference(self.image([255,20,30,40]),self.image([255,20,30,200]))
        self.assertEqual(result['different_pixels'],1);self.assertEqual(result['different_alpha_pixels'],0)
        self.assertEqual(result['storage_byte_differences'],[0,0,0,1]);self.assertEqual(result['max_channel_delta'],160)
        self.assertEqual(result['first_pixel_differences'][0]['candidate_storage_bytes'],[255,20,30,200])
    def test_alpha_storage_byte(self):
        result=comparison.image_difference(self.image([255,20,30,40]),self.image([0,20,30,40]))
        self.assertEqual(result['different_alpha_pixels'],1);self.assertEqual(result['storage_byte_differences'],[1,0,0,0])
    def test_input_mismatch_cannot_pass_even_when_pixels_match(self):
        common=dict(images={'readback-0.rgba':dict(bytes=16,sha256='same')},report=dict(backend='vulkan',device='fixture',software_device=True))
        runs={'reference':dict(common,submission_inputs=[dict(groups=dict(positions='a'))]),
              'candidate':dict(common,submission_inputs=[dict(groups=dict(positions='b'))])}
        with tempfile.TemporaryDirectory() as folder:
            result=comparison.write_comparison(Path(folder),runs,{})
            self.assertTrue(result['full_normalized_readbacks_equal']);self.assertFalse(result['semantic_submission_inputs_equal'])
            self.assertFalse(result['success']);self.assertEqual(len(result['submission_input_mismatches']),1)
            self.assertEqual(json.loads((Path(folder)/'comparison.json').read_text())['success'],False)
    def test_identical_inputs_cannot_excuse_one_pixel(self):
        common=dict(report=dict(backend='vulkan',device='fixture',software_device=True),submission_inputs=[dict(groups=dict(positions='same'))])
        runs={}
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for variant,pixel in [('reference',[255,20,30,40]),('candidate',[255,20,30,41])]:
                path=root/variant/'run';path.mkdir(parents=True);(path/'readback-0.rgba').write_bytes(self.image(pixel))
                runs[variant]=dict(common,images={'readback-0.rgba':dict(bytes=16,sha256=variant)})
            result=comparison.write_comparison(root,runs,{})
            self.assertTrue(result['semantic_submission_inputs_equal']);self.assertFalse(result['success'])
            self.assertEqual(result['image_differences'][0]['different_pixels'],1)
    def test_primary_without_trace_keeps_original_gate(self):
        common=dict(images={'readback-0.rgba':dict(bytes=16,sha256='same')},report=dict(backend='vulkan',device='fixture',software_device=True))
        with tempfile.TemporaryDirectory() as folder:
            result=comparison.write_comparison(Path(folder),dict(reference=common,candidate=common),{})
            self.assertTrue(result['success']);self.assertNotIn('semantic_submission_inputs_equal',result)

if __name__=='__main__':unittest.main()
