import copy
import unittest
from compare_fullsize_modes import COMPONENTS,SCENES,compare,native_window,stats


def fixture():
    runs=[]
    for direction,modes in [('forward',COMPONENTS),('reverse',reversed(COMPONENTS))]:
        for mode in modes:
            runs.append(dict(direction=direction,mode=mode,udp_inventory_sha256='a'*64,
                sdl_runtime_versions=[3004010],fov_z_range=None,
                native_windows={s:(2560,1369,1.0) for s in SCENES},
                loaded_world_bounds={} if mode=='baseline' else {s:(-1,1,3) for s in SCENES},
                scenes={s:{k:stats([10.0]*64) for k in ['first_render','native_action_to_ready','warm_rotation']} for s in SCENES}))
    return runs


class ComparisonTests(unittest.TestCase):
    def test_both_directions_and_all_scenes_are_compared(self):
        result=compare(fixture())
        self.assertTrue(result['performance_accepted'])
        self.assertEqual(len(result['comparisons']),3*len(SCENES)*3*4)

    def test_repeated_complete_frame_regression_blocks(self):
        runs=fixture()
        for run in runs:
            if run['mode']=='combined':run['scenes']['dense_city']['first_render']['p99_ms']=11
        result=compare(runs)
        self.assertFalse(result['performance_accepted'])
        self.assertEqual(len(result['blocking_reproducible_regressions']),1)

    def test_fast_scope_percentage_alone_is_not_whole_frame_regression(self):
        runs=fixture()
        for run in runs:
            for scene in run['scenes'].values():scene['warm_rotation']=stats([.01]*64)
            if run['mode']=='optimization':run['scenes']['street']['warm_rotation']=stats([.02]*64)
        self.assertTrue(compare(runs)['performance_accepted'])

    def test_one_tail_is_retained_without_claiming_repeated_regression(self):
        runs=fixture();runs[3]['scenes']['smoke_rain']['warm_rotation']['maximum_ms']=30
        result=compare(runs)
        affected=[v for v in result['comparisons'] if v['mode']=='combined' and v['scene']=='smoke_rain' and v['metric']=='maximum_ms' and v['group']=='warm_rotation']
        self.assertEqual(affected[0]['repeats'][0]['after_ms'],30)
        self.assertEqual(affected[0]['repeats'][1]['after_ms'],10)

    def test_duplicate_direction_or_wrong_order_is_rejected(self):
        runs=fixture();runs[-1]['direction']='forward'
        with self.assertRaisesRegex(ValueError,'Run order'):compare(runs)

    def test_udp_mismatch_is_rejected(self):
        runs=fixture();runs[-1]['udp_inventory_sha256']='b'*64
        with self.assertRaisesRegex(ValueError,'UDP'):compare(runs)

    def test_sdl_mismatch_is_rejected(self):
        runs=fixture();runs[-1]['sdl_runtime_versions']=[3004000]
        with self.assertRaisesRegex(ValueError,'SDL'):compare(runs)

    def test_native_window_or_dpi_mismatch_is_rejected(self):
        for value in [(1920,1080,1.0),(2560,1369,1.5)]:
            runs=fixture();runs[-1]['native_windows']['street']=value
            with self.assertRaisesRegex(ValueError,'window size or display scale'):compare(runs)

    def test_reduced_loaded_z_is_rejected(self):
        runs=fixture();runs[3]['loaded_world_bounds']['multi_level_horde']=(0,0,1)
        with self.assertRaisesRegex(ValueError,'Loaded Native Z'):compare(runs)

    def test_unknown_window_size_cannot_masquerade_as_viewport(self):
        record=dict(native_window_size_available=True,native_window_pixel_width=2560,
                    native_window_pixel_height=1369,window_display_scale=1.0)
        self.assertEqual(native_window(record),(2560,1369,1.0))
        for key,value in [('native_window_size_available',False),('native_window_pixel_height',0),('window_display_scale',float('nan'))]:
            invalid=copy.deepcopy(record);invalid[key]=value
            with self.assertRaises(ValueError):native_window(invalid)


if __name__=='__main__':unittest.main()
