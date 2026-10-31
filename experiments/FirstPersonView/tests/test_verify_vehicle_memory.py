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


if __name__=='__main__':unittest.main()
