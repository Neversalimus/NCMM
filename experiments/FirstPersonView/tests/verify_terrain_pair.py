"""Strict same-Host terrain ablation; CPU wall times never imply GPU execution."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import struct

from verify_terrain_profile import MATERIALS, SCENES, verify as verify_profile

REFERENCE_SHA = '4b9a36a62151216e0b44ac82aaa277c273d7e989591868c9ac4b6902bb0d86ec'
ENGINES = ('3f7fb352bf492ba521bd9408a0c9f6ce239e8d83', '074aa98bd5be3de4c35f154082db32a0e63bb0f1')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def image_check(path):
    data = path.read_bytes()
    assert len(data) >= 54 and data[:2] == b'BM'
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height, planes, bits, compression = struct.unpack_from('<iiHHI', data, 18)
    assert (width, abs(height), planes) == (640, 480, 1)
    assert bits in (24, 32) and compression in (0, 3)
    stride = ((width * bits + 31) // 32) * 4
    assert 54 <= offset <= 256 and offset + abs(height) * stride == len(data)
    # Exclude black/constant placeholder captures; channel order does not affect sum.
    colors = []
    for y in range(0, abs(height), 8):
        for x in range(0, width, 8):
            at = offset + y * stride + x * (bits // 8)
            colors.append(sum(data[at:at + 3]))
    assert statistics.mean(colors) > 9 and max(colors) - min(colors) > 24


def distribution(values):
    values = sorted(values)
    def quantile(q):
        at = (len(values) - 1) * q
        lo, hi = math.floor(at), math.ceil(at)
        return values[lo] + (values[hi] - values[lo]) * (at - lo)
    return dict(samples=len(values), median_ms=statistics.median(values), p95_ms=quantile(.95),
                p99_ms=quantile(.99), max_ms=values[-1])


def verify(root):
    root = Path(root)
    manifest = read(root / 'pair.json')
    assert manifest['schema'] == 2
    assert manifest['terrain_fixture_schema'] == 3
    assert manifest['terrain_material_ids'] == list(MATERIALS)
    assert manifest['ordinary_game_performance'] is False
    assert manifest['gpu_execution_timestamps_available'] is False
    assert manifest['module_restored'] is True and manifest['fixture_restored'] is True
    for key in ('source_commit', 'source_tree', 'engine_commit', 'reference_include_commit'):
        assert re.fullmatch('[0-9a-f]{40}', manifest[key])
    assert manifest['engine_commit'] in ENGINES
    assert re.fullmatch('[0-9]+', manifest['workflow_run'])
    for key in ('host_sha256', 'current_module_sha256', 'reference_module_sha256',
                'current_include_sha256', 'fixture_files_sha256', 'fixture_digest'):
        assert re.fullmatch('[0-9a-f]{64}', manifest[key])
    assert manifest['reference_kind'] == 'current-source-with-pre71-terrain-include'
    assert manifest['reference_include_sha256'] == REFERENCE_SHA
    assert manifest['reference_include_commit'] == 'fdbba45f9082986202bd05db521bc315e9be4744'
    assert manifest['current_include_sha256'] != REFERENCE_SHA
    assert manifest['reference_module_sha256'] != manifest['current_module_sha256']
    assert digest(root / 'fixture-files.json') == manifest['fixture_files_sha256']
    fixture = read(root / 'fixture-files.json')
    assert fixture and all(re.fullmatch('[0-9a-f]{64}', v) for v in fixture.values())
    definitions = manifest['terrain_material_definitions']
    assert set(definitions) == set(MATERIALS)
    for value in definitions.values():
        path = value['path']
        assert re.fullmatch(r'[A-Za-z0-9_./-]+\.json', path)
        assert not path.startswith('/') and '..' not in path.split('/')
        assert fixture['data/json/furniture_and_terrain/' + path] == value['sha256']
    assert hashlib.sha256(json.dumps(fixture, sort_keys=True).encode()).hexdigest() == manifest['fixture_digest']
    bundle = read(root / 'bundle.json')
    assert bundle['schema'] == 1 and bundle['ncmm_source_commit'] == manifest['source_commit']
    assert bundle['source_commit'] == manifest['engine_commit'] and bundle['host_sha256'] == manifest['host_sha256']
    assert bundle['files']['code_mods/FirstPersonView/ncmm_mod.dll'] == manifest['current_module_sha256']
    count = manifest['pairs']
    assert type(count) is int and 3 <= count <= 7
    assert len(manifest['trials']) == count * 2
    keys = {(scene, mode) for scene in SCENES for mode in ('cpu', 'gpu')}
    profiles = {}
    for index, trial in enumerate(manifest['trials']):
        pair, order = divmod(index, 2)
        kind = ('reference', 'current')[(pair + order) % 2]
        assert (trial['pair'], trial['order'], trial['kind']) == (pair, order, kind)
        assert trial['directory'] == f'pair-{pair + 1:02d}-{kind}'
        directory = root / trial['directory']
        assert digest(directory / 'first-person-terrain-benchmark.json') == trial['report_sha256']
        assert digest(directory / 'process.json') == trial['process_sha256']
        process = read(directory / 'process.json')
        assert process['exit_code'] == 0 and process['timeout'] is False and process['pid'] > 0
        assert 0 < process['started_ns'] < process['ended_ns']
        assert (process['ended_ns'] - process['started_ns']) / 1e9 <= 600
        for key in ('source_commit', 'engine_commit', 'host_sha256', 'fixture_digest'):
            assert process[key] == manifest[key]
        assert process['module_sha256'] == manifest[kind + '_module_sha256']
        report = read(directory / 'first-person-terrain-benchmark.json')
        verify_profile(report)
        records = {(r['scene'], r['renderer']): r for r in report['records']}
        assert set(records) == keys
        expected_images = set()
        for (scene, mode), record in records.items():
            name = f'first-person-gpu-{scene}-640x480-{mode}.bmp'
            assert record['capture'] == name
            expected_images.add(name)
            assert digest(directory / name) == trial['images'][name]
            image_check(directory / name)
        assert set(trial['images']) == expected_images
        profiles[pair, kind] = records
    comparisons = []
    metrics = ('mode_first_ready_ms', 'median_ms', 'p95_ms', 'p99_ms',
               'native_tiles_wall_median_ms', 'perspective_wall_median_ms',
               'actual_present_wall_median_ms', 'cpu_geometry_median_ms',
               'gpu_geometry_prepare_median_ms')
    for key in sorted(keys):
        pairs = [(profiles[p, 'reference'][key], profiles[p, 'current'][key]) for p in range(count)]
        for before, after in pairs:
            for field in ('terrain_layout_digest', 'terrain_capture_seconds', 'backend', 'device',
                          'software_device', 'mesh_triangles_submitted', 'gpu_3d_draw_calls',
                          'width', 'height', 'internal_width', 'internal_height', 'frames', 'creatures'):
                assert before[field] == after[field]
        capture = pairs[0][0]['capture']
        # Same pixels for all independent processes, not only a same-pair picture.
        images = [t['images'][capture] for t in manifest['trials']]
        assert len(set(images)) == 1
        row = dict(scene=key[0], renderer=key[1], software_device=pairs[0][0]['software_device'],
                   backend=pairs[0][0]['backend'], device=pairs[0][0]['device'], metrics={})
        for metric in metrics:
            before = [p[0][metric] for p in pairs]
            after = [p[1][metric] for p in pairs]
            assert all(math.isfinite(v) and v >= 0 for v in before + after)
            row['metrics'][metric] = dict(reference=distribution(before), current=distribution(after),
                paired_ratio_median=statistics.median(b / a for a, b in zip(before, after)) if all(before) else None)
        for metric in ('warmup_frame_wall_samples', 'warmup_native_tiles_wall_samples',
                       'warmup_perspective_wall_samples', 'warmup_actual_present_wall_samples',
                       'warmup_cpu_geometry_samples', 'warmup_gpu_geometry_prepare_samples'):
            before, after = [[r[lane][metric][0] for r in pairs] for lane in (0, 1)]
            row['metrics']['initial_' + metric] = dict(reference=distribution(before), current=distribution(after),
                paired_ratio_median=statistics.median(b / a for a, b in zip(before, after)) if all(before) else None)
        comparisons.append(row)
    return dict(success=True, actual_cdda_sdl=True, pairs=count, children=count * 2,
                source_commit=manifest['source_commit'], engine_commit=manifest['engine_commit'],
                workflow_run=manifest['workflow_run'], host_sha256=manifest['host_sha256'],
                current_module_sha256=manifest['current_module_sha256'], reference_module_sha256=manifest['reference_module_sha256'],
                exact_capture_equality=True, physical_layouts_verified=True, same_host_verified=True,
                ordinary_game_performance=False, gpu_execution_timestamps_available=False,
                comparisons=comparisons, limits='Controlled same-Host terrain renderer ablation. First-ready starts after Native map/cache setup; '
                'it is not action-to-first-correct-game frame, scanout, ordinary FPS, or hardware GPU execution time. '
                'Geometry counters are CPU wall intervals and may include nested work. Reference is current source with one old include.')


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    a.output.write_text(json.dumps(verify(a.root), indent=2) + '\n', encoding='utf-8')
    print('Native same-Host terrain pair qualification PASS')
