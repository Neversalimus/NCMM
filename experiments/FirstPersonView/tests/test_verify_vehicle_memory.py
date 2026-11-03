"""Negative admission tests for actual Native long-run process memory receipts."""
import json
from pathlib import Path
import tempfile
import unittest
from verify_vehicle_gallery import verify_memory,verify_mutation_views,verify_cpu_metrics


class VehicleMemoryTests(unittest.TestCase):
    source='a'*40;engine='b'*40;module='c'*64

    def receipt(self, windows=True):
        value,peak=('working_set_bytes','peak_working_set_bytes') if windows else ('rss_bytes','peak_rss_bytes')
        return dict(schema=1,actual_native=True,source_commit=self.source,engine_commit=self.engine,module_sha256=self.module,
                    samples=[dict(elapsed_seconds=20+n*20,**{value:1024**3+n*1024**2,peak:1200*1024**2}) for n in range(50)])

    def admit(self, receipt):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'memory.json';path.write_text(json.dumps(receipt))
            return verify_memory(path,self.source,self.engine,self.module)

    def test_windows_and_linux_native_units_share_the_same_budget(self):
        self.assertEqual(self.admit(self.receipt())['samples'],50)
        self.assertEqual(self.admit(self.receipt(False))['samples'],50)

    def test_late_process_growth_cannot_hide_behind_small_gpu_counters(self):
        receipt=self.receipt()
        for sample in receipt['samples'][30:]:sample['working_set_bytes']=sample['peak_working_set_bytes']=12*1024**3
        with self.assertRaises(AssertionError):self.admit(receipt)

    def test_transient_peak_is_rejected_even_after_memory_is_freed(self):
        receipt=self.receipt();receipt['samples'][-1]['peak_working_set_bytes']=13*1024**3
        with self.assertRaises(AssertionError):self.admit(receipt)

    def test_wrong_source_and_incomplete_sampling_cannot_qualify(self):
        receipt=self.receipt();receipt['source_commit']='d'*40
        with self.assertRaises(AssertionError):self.admit(receipt)
        receipt=self.receipt();receipt['samples']=receipt['samples'][:2]
        with self.assertRaises(AssertionError):self.admit(receipt)


class CpuMetricTests(unittest.TestCase):
    def row(self):
        return dict(cpu_width=1280,cpu_height=800,frame_width=1280,frame_height=800,full_frame_ms=10,
            cpu_callback_ms=8,cpu_geometry_ms=2,cpu_raster_ms=5,cpu_effects_ms=.1,cpu_raster_calls=30,cpu_triangles=400)
    def test_actual_full_resolution_callback_and_reused_frame(self):
        verify_cpu_metrics(self.row())
        row=self.row();row.update(cpu_geometry_ms=0,cpu_raster_ms=0,cpu_effects_ms=0,cpu_raster_calls=0,cpu_triangles=0)
        verify_cpu_metrics(row)
    def test_scaled_invalid_or_uncontained_cpu_telemetry_is_rejected(self):
        for name,value in [('cpu_width',960),('cpu_height',600),('cpu_callback_ms',11),('cpu_callback_ms',0),
                           ('cpu_geometry_ms',float('nan')),('cpu_raster_ms',-.1),('cpu_triangles',-1)]:
            row=self.row();row[name]=value
            with self.subTest(name=name,value=value),self.assertRaises(AssertionError):verify_cpu_metrics(row)


class MutationViewTests(unittest.TestCase):
    def groups(self):
        labels=('mutation-before','open-door','closed-door','damaged','broken-door','wheel-before','broken-wheel','removed-wheel')
        return {('car',label):{(backend,mode):dict(camera_xyz=[60,52,0],yaw=1.57,pitch=-.08,eye_height=1.55)
                for backend in ('cpu','gpu') for mode in ('legacy','modular')} for label in labels}

    def test_complete_matched_mutations_are_accepted(self):
        verify_mutation_views(self.groups())

    def test_changed_position_direction_or_eye_is_rejected(self):
        for name,value in [('camera_xyz',[60,68,0]),('yaw',-1.57),('pitch',.2),('eye_height',2)]:
            groups=self.groups();groups['car','broken-door']['gpu','modular'][name]=value
            with self.subTest(name=name),self.assertRaises(AssertionError):verify_mutation_views(groups)

    def test_missing_baseline_or_variant_is_rejected(self):
        groups=self.groups();del groups['car','wheel-before']
        with self.assertRaises(AssertionError):verify_mutation_views(groups)
        groups=self.groups();del groups['car','open-door']['cpu','legacy']
        with self.assertRaises(AssertionError):verify_mutation_views(groups)

    def test_wheel_and_door_can_use_different_actual_sides(self):
        groups=self.groups()
        for label in ('wheel-before','broken-wheel','removed-wheel'):
            for row in groups['car',label].values():row['camera_xyz']=[60,68,0];row['yaw']=-1.57
        verify_mutation_views(groups)


from verify_vehicle_gallery import verify_fragment_controls,ordinary_vehicle_frames


class NativeFragmentControlTests(unittest.TestCase):
    def receipt(self):
        report={'frames':[]};serial=0
        for scene in ['parking','large']:
            for pair in range(7):
                bounds={}
                for slot in range(2):
                    enabled=bool((pair+slot)%2);first=serial+1
                    for sample in range(7):
                        serial+=1
                        report['frames'].append(dict(label=f'fragment-control-{scene}-{pair}-{"on" if enabled else "off"}-{sample}',
                            prototype=scene,backend='gpu',renderer='modular',actual_gpu=True,frame_width=1280,frame_height=800,
                            pair=pair,sample=sample,slot=slot,fragment_reuse=enabled,full_frame_ms=10.,synchronized_frame_ms=11.,
                            gpu_frame_serial=serial,gpu_draw_calls=250,gpu_triangles=26000,
                            uniform_pushes=200 if enabled else 250,uniform_reuses=50 if enabled else 0,
                            sampler_binds=100 if enabled else 250,sampler_reuses=150 if enabled else 0,pipeline_invalidations=70,
                            camera_xyz=[60,68,0],yaw=0.,pitch=-.1,eye_height=1.55,snapshot_vehicles=1,snapshot_parts=231,
                            device='pure fixture, no device execution',software_device=True))
                    bounds[enabled]=[first,serial]
                report['frames'].append(dict(label=f'fragment-parity-{scene}-{pair}',prototype=scene,pair=pair,
                    pixels_equal=True,different_pixels=0,compared_pixels=1280*800,compared_bytes=1280*800*4,
                    disabled_serials=bounds[False],enabled_serials=bounds[True]))
        return report

    def test_complete_alternating_controls_and_exact_work_accounting(self):
        got=verify_fragment_controls(self.receipt())
        self.assertEqual((got['timed_frames'],got['paired_controls'],got['full_normalized_pixel_pairs']),(196,14,14))
        self.assertEqual((got['actual_uniform_uploads_saved'],got['actual_sampler_binds_saved']),(4900,14700))
        self.assertFalse(got['performance_accepted']);self.assertFalse(got['hardware_FPS'])

    def test_incomplete_stale_scaled_or_uncounted_measurements_are_rejected(self):
        mutations=[lambda r:r['frames'].pop(),lambda r:r['frames'].pop(0),lambda r:r['frames'].append(r['frames'][0]),
            lambda r:r['frames'][0].update(label='fragment-control-large-0-off-0'),
            lambda r:r['frames'][0].update(pair=True),lambda r:r['frames'][0].update(sample=True),
            lambda r:r['frames'][0].update(slot=1),lambda r:r['frames'][0].update(fragment_reuse=0),
            lambda r:r['frames'][0].update(actual_gpu=False),lambda r:r['frames'][0].update(backend='cpu'),
            lambda r:r['frames'][0].update(renderer='legacy'),lambda r:r['frames'][0].update(frame_width=960),
            lambda r:r['frames'][0].update(frame_height=600),lambda r:r['frames'][0].update(full_frame_ms=float('nan')),
            lambda r:r['frames'][0].update(full_frame_ms=0),lambda r:r['frames'][0].update(full_frame_ms=True),
            lambda r:r['frames'][0].update(synchronized_frame_ms=9),lambda r:r['frames'][0].update(eye_height=-1),
            lambda r:r['frames'][0].update(camera_xyz=[60,68,True]),lambda r:r['frames'][0].update(device=''),
            lambda r:r['frames'][0].update(software_device=1),lambda r:r['frames'][0].update(snapshot_parts=0),
            lambda r:r['frames'][0].update(snapshot_vehicles=True),lambda r:r['frames'][1].update(gpu_frame_serial=1),
            lambda r:r['frames'][0].update(uniform_reuses=1),lambda r:r['frames'][7].update(uniform_pushes=199),
            lambda r:r['frames'][7].update(sampler_binds=99),lambda r:r['frames'][7].update(pipeline_invalidations=69),
            lambda r:r['frames'][7].update(gpu_triangles=25999),lambda r:r['frames'][7].update(gpu_draw_calls=249),
            lambda r:r['frames'][7].update(camera_xyz=[60,69,0]),lambda r:r['frames'][7].update(sampler_reuses=True),
            lambda r:r['frames'][14].update(pixels_equal=False),lambda r:r['frames'][14].update(different_pixels=True),
            lambda r:r['frames'][14].update(compared_pixels=640*480),lambda r:r['frames'][14].update(compared_bytes=0),
            lambda r:r['frames'][14].update(enabled_serials=[1,7]),lambda r:r['frames'][14].update(disabled_serials=[True,7]),
            lambda r:r['frames'][14].pop('pixels_equal'),lambda r:r['frames'][7].update(extra_unverified_field=0)]
        for index,mutate in enumerate(mutations):
            r=self.receipt();mutate(r)
            with self.subTest(index=index),self.assertRaises((AssertionError,KeyError)):verify_fragment_controls(r)

    def test_policy_that_never_reuses_a_binding_is_rejected(self):
        r=self.receipt()
        for row in r['frames']:
            if row.get('fragment_reuse'):row.update(sampler_binds=250,sampler_reuses=0)
        with self.assertRaises(AssertionError):verify_fragment_controls(r)

    def test_existing_ordinary_frame_router_cannot_skip_malformed_supplement(self):
        # Supply the complete old depth matrix too: failure must come from the
        # new malformed group, not from an unrelated missing original receipt.
        r=self.receipt();depth=[]
        for row in r['frames']:
            item=dict(row);item['label']=item['label'].replace('fragment-control-','depth-control-').replace('fragment-parity-','depth-parity-')
            if 'fragment_reuse' in item:
                item['depth_enabled']=item['fragment_reuse'];item['bounded_depth_draws']=2 if item['fragment_reuse'] else 0
            depth.append(item)
        ordinary={'label':'bench-original'}
        r['frames']+=depth+[ordinary]
        frames,old=ordinary_vehicle_frames(r)
        self.assertEqual(frames,[ordinary]);self.assertEqual(old['timed_frames'],196)
        r['frames'][7]['uniform_pushes']=199
        with self.assertRaises(AssertionError):ordinary_vehicle_frames(r)


if __name__=='__main__':unittest.main()
