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
    def test_reconstructed_reference_keeps_frozen_bytes(self):
        reference=comparison.reference_backend(self.backend())
        self.assertEqual(hashlib.sha256(reference.encode()).hexdigest(),'a0dd1aa83c05e1a51ac33854dedb54696e85616857d9da1f68bb30ee7fe9662a')
        self.assertNotIn('opaque_color_replacement(',reference)
        self.assertNotIn('bound_pipeline',reference)
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
