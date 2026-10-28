"""Validate real focused SDL samples, including exclusive frame phase accounting."""
import argparse
import json
import math
from pathlib import Path


def verify(report, *, expected_scenes=('smoke-gas', 'creatures-1')):
    assert report['actual_cdda_sdl'] and report['focused_reproduction']
    assert report['phase_samples_available'] and not report['player_fps']
    assert not report['gpu_timestamps_available']
    records = report['records']
    assert len(records) == len(expected_scenes) * 2
    assert {(r['scene'], r['renderer']) for r in records} == {
        (scene, renderer) for scene in expected_scenes for renderer in ['cpu', 'gpu']
    }
    fields = ['frame_wall_samples', 'native_tiles_wall_samples', 'perspective_wall_samples',
              'actual_present_wall_samples', 'unattributed_wall_samples']
    for record in records:
        assert (record['width'], record['height']) == (640, 480)
        assert (record['internal_width'], record['internal_height']) == (640, 480)
        assert record['frames'] == 96
        assert len(record['frame_renderers_gpu']) == 96
        assert all(tag == (1 if record['renderer'] == 'gpu' else 0)
                   for tag in record['frame_renderers_gpu'])
        for name in fields:
            values = record[name]
            assert len(values) == 96
            assert all(math.isfinite(v) and v >= 0 for v in values)
        for full, tiles, perspective, present, other in zip(*(record[f] for f in fields)):
            assert abs(full - tiles - perspective - present - other) <= .002
        ordered = sorted(record['frame_wall_samples'])
        for name, q in [('median_ms', .5), ('p95_ms', .95), ('p99_ms', .99)]:
            at = q * (len(ordered) - 1)
            lo, hi = math.floor(at), math.ceil(at)
            expected = ordered[lo] + (ordered[hi] - ordered[lo]) * (at - lo)
            assert abs(record[name] - expected) <= .002
        if record['renderer'] == 'gpu':
            assert record['backend'] and record['device']
            assert record['gpu_3d_draw_calls'] > 0
        assert record['fallback_creatures'] == 0
    columns = ['scene', 'renderer', 'median_ms', 'p95_ms', 'p99_ms',
               'native_tiles_wall_median_ms', 'perspective_wall_median_ms',
               'actual_present_wall_median_ms', 'extract_median_ms', 'callback_median_ms',
               'gpu_geometry_prepare_median_ms',
               'gpu_resource_update_median_ms', 'cpu_cache_bytes',
               'producer_cpu_cache_bytes_peak', 'host_cpu_cache_bytes_peak',
               'gpu_3d_draw_calls', 'mesh_triangles_submitted', 'upload_bytes_peak',
               'backend', 'device', 'software_device']
    print(json.dumps({'phase_summary': [{k: record[k] for k in columns} for record in records]}))
    return {'success': True, 'actual_cdda_sdl': True, 'manual_playthrough': False,
            'gpu_execution_timestamps_available': False, 'cases': len(records), 'frames_per_case': 96,
            'exclusive_frame_phases_verified': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(json.loads(args.input.read_text(encoding='utf-8-sig')))
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('Focused native CPU/GPU frame phases and renderer identities PASS')
