import copy
import math
import unittest
from verify_horizon_release import frame_phases

class FramePhaseTests(unittest.TestCase):
    def valid_record(self):
        r={'exclusive_phases_available': True, 'frame_samples_ms': [4.0]*108,
           'first_frame_indices': list(range(96,108)), 'first_frame_after_step_samples_ms': [4.0]*12}
        for name in ('native_tiles','perspective','present','unattributed'):
            r[name+'_samples_ms']=[1.0]*108
        for name in ('extract','callback','gpu_geometry','gpu_resource','gpu_upload','gpu_record','gpu_submit'):
            r[name+'_samples_ms']=[0.0]*108
        return r
    def test_complete_exclusive_phases(self):
        result=frame_phases(self.valid_record())
        self.assertEqual(result['first_after_step']['perspective_samples_ms']['median_ms'],1.0)
    def test_extended_warm_slice_and_step_indices(self):
        r=self.valid_record()
        for name,values in r.items():
            if isinstance(values,list):r[name]=[values[0]]*256
        r['first_frame_indices']=list(range(192,256))
        r['first_frame_after_step_samples_ms']=[4.0]*64
        r['present_samples_ms'][96:192]=[2.0]*96
        r['native_tiles_samples_ms'][96:192]=[0.0]*96
        result=frame_phases(r,extended=True)
        self.assertEqual(result['warm_rotation']['present_samples_ms']['median_ms'],2.0)
        self.assertEqual(result['first_after_step']['present_samples_ms']['median_ms'],1.0)
        with self.assertRaises(ValueError):frame_phases(r)
    def test_overlap_and_missing_work_rejected(self):
        for value in (0.0, 2.0):
            r=self.valid_record();r['present_samples_ms'][99]=value
            with self.assertRaisesRegex(ValueError,'overlap or omit'):frame_phases(r)
    def test_step_indices_must_match_frame_stream(self):
        for indices in ([96]*12, list(range(97,109))):
            r=self.valid_record();r['first_frame_indices']=indices
            with self.assertRaises(ValueError):frame_phases(r)
        r=self.valid_record();r['first_frame_after_step_samples_ms'][0]=3.0
        with self.assertRaisesRegex(ValueError,'differ'):frame_phases(r)
    def test_invalid_nested_samples_rejected(self):
        for values in ([0.0]*107,[float('nan')]*108,[-1.0]*108):
            r=self.valid_record();r['gpu_resource_samples_ms']=values
            with self.assertRaisesRegex(ValueError,'Invalid movement phase'):frame_phases(r)
    def test_no_claim_without_exclusive_measurement(self):
        r=self.valid_record();r['exclusive_phases_available']=False
        with self.assertRaisesRegex(ValueError,'missing'):frame_phases(r)

if __name__=='__main__':unittest.main()
