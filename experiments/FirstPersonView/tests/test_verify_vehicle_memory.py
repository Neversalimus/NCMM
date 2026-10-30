"""Negative admission tests for actual Native long-run process memory receipts."""
import json
from pathlib import Path
import tempfile
import unittest
from verify_vehicle_gallery import verify_memory


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


if __name__=='__main__':unittest.main()
