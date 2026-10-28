"""Strict controlled Native terrain scene profile; ordinary-game FPS is excluded."""
import argparse
import json
import math
import statistics
from pathlib import Path

from verify_gpu_profile import verify as verify_phases

SCENES = ('terrain-grass', 'terrain-road', 'terrain-mixed')
MATERIALS = ('t_pavement', 't_gravel', 't_dirt', 't_grass')


def layout_digest(scene):
    assert scene in SCENES
    result = 1469598103934665603
    for y in range(-48, 49):
        for x in range(-48, 49):
            if scene == 'terrain-grass':
                kind = 3
            elif scene == 'terrain-road':
                kind = 0 if x <= 0 else 3
            else:
                kind = ((x + 48) // 2 + (y + 48) // 2) % 4
            for value in MATERIALS[kind].encode('ascii') + b'\0':
                result = ((result ^ value) * 1099511628211) & ((1 << 64) - 1)
    return result


def verify(report):
    assert report['schema'] == 2 and report['terrain_profile_only'] is True
    assert report['ordinary_performance_measurement'] is False
    assert report['terrain_grid_side'] == 97
    assert report['stable_cpu_binary'] is False
    assert 'cpu_algorithm_commit' not in report and 'stable_main_commit' not in report
    result = verify_phases(report, expected_scenes=SCENES)
    for record in report['records']:
        assert record['terrain_layout_digest'] == layout_digest(record['scene'])
        assert record['terrain_capture_seconds'] == 1000.5
        assert record['creatures'] == 0
        for phase in ('native_tiles_wall', 'perspective_wall', 'actual_present_wall'):
            assert abs(statistics.median(record[phase + '_samples']) - record[phase + '_median_ms']) <= .002
        assert 1 <= record['warmup_frames'] <= 180
        for name in ('warmup_frame_wall_samples', 'warmup_native_tiles_wall_samples',
                     'warmup_perspective_wall_samples', 'warmup_actual_present_wall_samples',
                     'warmup_cpu_geometry_samples', 'warmup_gpu_geometry_prepare_samples'):
            values = record[name]
            assert len(values) == record['warmup_frames']
            assert all(math.isfinite(v) and v >= 0 for v in values)
        for frame, native, perspective, present in zip(*(record[name] for name in (
                'warmup_frame_wall_samples', 'warmup_native_tiles_wall_samples',
                'warmup_perspective_wall_samples', 'warmup_actual_present_wall_samples'))):
            assert frame + .002 >= native + perspective + present
        assert abs(max(record['warmup_frame_wall_samples']) - record['warmup_frame_max_ms']) <= .002
        assert record['mode_first_ready_ms'] + .002 >= sum(record['warmup_frame_wall_samples'])
        inactive = ('warmup_gpu_geometry_prepare_samples' if record['renderer'] == 'cpu'
                    else 'warmup_cpu_geometry_samples')
        assert all(value == 0 for value in record[inactive])
        for name in ('mode_first_ready_ms', 'warmup_frame_max_ms', 'steady_frame_max_ms'):
            assert math.isfinite(record[name]) and record[name] >= 0
    result.update(terrain_profile_only=True, ordinary_performance_measurement=False,
                  physical_layout_verified=True, source_identity_verified=False,
                  limits='Program profile only; caller must bind Host/module/source/run and clean exit. '
                         'CPU preparation/recording and Present wall are not GPU execution or scanout.')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(json.loads(args.input.read_text(encoding='utf-8-sig')))
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Controlled Native terrain profiles and exact physical layouts PASS')
