"""Validate a same-run Native backend ablation; never convert CI wall time to FPS."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
from statistics import median

from verify_turn_attribution import require, verify as verify_turns, verify_stair_transitions

REFERENCE_SOURCE = '75850a31d244ebdca085ab09fa328ec5fa6d526e'
CANDIDATE_SOURCE = '099101a259eb9af90c64caff214c0083de7af21b'
REFERENCE_SHA = '2c6a31754b82c6b1bec8b03efaffc97603132107d16b604b0ad60265bcc4f6d4'
CANDIDATE_SHA = 'd8d349dd27588bdc3e1d0f0d466ac8777fd74d925dfc49d7a991025a49c97489'
PARSER_SHA = '6e70d6e921b2edac5bda144d875673011cebf158221a03f23c2fa20f1e9ca5db'
PAIR_ACTIONS = {'native_walk_and_visual_continuity', 'native_door_open_close_and_furniture',
                'native_pickup_inventory_cancel_and_item_use', 'native_melee_aim_fire_and_reload_completion',
                'native_stairs_and_basement', 'native_vehicle_board_controls_displacement_and_exit',
                'rapid_tall_avatar_stairs_respect_scene_bounds', 'native_combat_1_5_20_ai_and_render_state_guards',
                'native_turn_processing', 'native_f6_and_f12_dispatch'}
PAIR_SCENES = {'city', 'dense_smoke', 'multi_level', 'creatures_1', 'creatures_5', 'creatures_20',
               'creatures_50', 'creatures_100', 'large_and_flying', 'lod_distances', 'unknown_mod_objects', 'rotation'}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def relative(root, name):
    require(type(name) is str and re.fullmatch(r'pair-0[1-7]-(reference|candidate)/(process|gameplay)\.json', name),
            'Unexpected trial evidence path')
    return root / name


def stair_metrics(report):
    require(report.get('success') is True and report.get('schema') == 1,
            'Native gameplay recorder did not complete')
    require(report.get('retirement_pair_only') is True and report.get('full_gameplay_coverage') is False,
            'Focused pair scope missing or relabelled')
    actions = report.get('actions')
    require(type(actions) is list and len(actions) == len(PAIR_ACTIONS)
            and {row.get('name') for row in actions} == PAIR_ACTIONS
            and all(row.get('success') is True for row in actions), 'Focused Native action coverage incomplete')
    require(report.get('width') == 640 and report.get('height') == 480, 'Stair viewport changed')
    tilesets = report.get('tilesets')
    require(type(tilesets) is list and len(tilesets) == 1 and tilesets[0].get('id') == 'UltimateCataclysm'
            and tilesets[0].get('actual_loaded_id') == 'UltimateCataclysm', 'Official pair tileset differs')
    cases = [case for case in tilesets[0]['cases'] if case['renderer'] == 'gpu']
    require(len(cases) == 12 and {case.get('name') for case in cases} == PAIR_SCENES,
            'Incomplete GPU scene descriptors')
    soak = report.get('soak', {})
    require(soak.get('automated') is True and type(soak.get('requested_seconds')) is int
            and soak['requested_seconds'] == 60 and type(soak.get('actual_wall_seconds')) in (int, float)
            and math.isfinite(soak['actual_wall_seconds']) and soak['actual_wall_seconds'] >= 60
            and type(soak.get('frames')) is int and soak['frames'] > 0,
            'Focused pair must retain its original sixty-second soak')
    descriptors = []
    for case in cases:
        require(all(type(case.get(key)) is str and case[key] for key in ('device', 'backend'))
                and type(case.get('software_device')) is bool, 'GPU device descriptor missing')
        descriptors.append((case['device'], case['backend'], case['software_device']))
    require(len(set(descriptors)) == 1, 'GPU device changed within a child')
    verify_turns(report)  # All original 26-turn/callback gates remain in force.
    verified = verify_stair_transitions(report)  # All schema5/resource/first-present gates remain in force.
    result = []
    for index, row in enumerate(verified['samples']):
        require(row['first_frame']['wall_ns'] > 0, 'First frame has no wall interval')
        result.append({'index': index, 'renderer': 'cpu' if index < 4 else 'gpu',
                       'from_z': row['from_z'], 'to_z': row['to_z'], 'delta': row['delta'],
                       'first_frame_wall_ms': row['first_frame']['wall_ns'] / 1e6,
                       'native_action_wall_ms': row['action']['wall_ns'] / 1e6,
                       'resource_counts': row['gpu_resource_counts']})
    ready = report['native_action_stages']['native_stair_first_ready_wall_ms']
    require(type(ready) is dict and ready.get('samples') == 8, 'Native first-ready samples missing')
    values = [ready.get(name) for name in ('median', 'p95', 'p99', 'max')]
    require(all(type(v) in (int, float) and math.isfinite(v) and v > 0 for v in values)
            and values == sorted(values), 'Invalid whole Native first-ready summary')
    # This existing summary covers all eight CPU/GPU transitions. The sum of
    # the two separate action/frame captures is deliberately not called ready.
    return result, {name: ready[name] for name in ('median', 'p95', 'p99', 'max')}, descriptors[0]


def verify(root):
    root = Path(root)
    pair = load(root / 'pair.json')
    require(pair.get('schema') == 1 and pair.get('pairs') == 7 and pair.get('phase') == 'complete',
            'Seven complete alternating pairs required')
    require(pair.get('diagnostic_only') is True and pair.get('ordinary_game_performance') is False
            and pair.get('whole_historical_parent_measured') is False
            and pair.get('fixture_mode') == 'focused_turns_stairs',
            'Backend ablation scope was relabelled')
    require(pair.get('reference_backend_source') == REFERENCE_SOURCE
            and pair.get('candidate_backend_source') == CANDIDATE_SOURCE
            and pair.get('reference_backend_sha256') == REFERENCE_SHA
            and pair.get('candidate_backend_sha256') == CANDIDATE_SHA,
            'Unexpected source-bound backend pair')
    for key in ('source_commit', 'source_tree'):
        require(type(pair.get(key)) is str and re.fullmatch('[0-9a-f]{40}', pair[key]), 'Invalid source identity')
    require(pair.get('engine_commit') in ('3f7fb352bf492ba521bd9408a0c9f6ce239e8d83',
                                        '074aa98bd5be3de4c35f154082db32a0e63bb0f1'), 'Unexpected engine pin')
    require(pair.get('turn_parser_sha256') == PARSER_SHA, 'Original stair/turn parser changed')
    for key in ('host_restored', 'source_restored', 'fixture_restored'):
        require(pair.get(key) is True, 'Restoration incomplete: ' + key)
    hashes = pair['host_sha256']
    require(set(hashes) == {'reference', 'candidate'} and len(set(hashes.values())) == 2,
            'Reference and candidate Host are indistinguishable')
    for value in [*hashes.values(), pair['module_sha256'], pair['fixture_digest']]:
        require(type(value) is str and re.fullmatch('[0-9a-f]{64}', value), 'Invalid binary/fixture digest')
    require(sha(root / 'fixture-files.json') == pair['fixture_digest'], 'Fixture manifest changed')
    inputs = load(root / 'native-inputs.json')
    require(sha(root / 'native-inputs.json') == pair['native_inputs_sha256'], 'Native input receipt changed')
    require(inputs['candidate'].keys() == inputs['reference'].keys() and len(inputs['candidate']) > 100,
            'Native input set incomplete')
    require(all(type(value) is str and re.fullmatch('[0-9a-f]{64}', value)
                for kind in inputs.values() for value in kind.values()), 'Invalid Native input digest')
    changes = {name for name in inputs['candidate'] if inputs['candidate'][name] != inputs['reference'][name]}
    require(changes == {'ncmm_gpu_backend.hpp'}
            and inputs['candidate']['ncmm_gpu_backend.hpp'] == CANDIDATE_SHA
            and inputs['reference']['ncmm_gpu_backend.hpp'] == REFERENCE_SHA,
            'More than the reviewed backend changed between builds')
    trials = pair['trials']
    require(type(trials) is list and len(trials) == 14, 'Missing or duplicate trial')
    collected = {}
    for ordinal, row in enumerate(trials):
        pair_index, order = divmod(ordinal, 2)
        kind = ('reference', 'candidate')[(pair_index + order) % 2]
        directory = f'pair-{pair_index + 1:02d}-{kind}'
        require(type(row.get('pair')) is int and type(row.get('order')) is int
                and row.get('pair') == pair_index and row.get('order') == order and row.get('kind') == kind
                and row.get('directory') == directory, 'Trial order changed or repeated')
        process_path, report_path = (relative(root, directory + '/' + name) for name in ('process.json', 'gameplay.json'))
        require(sha(process_path) == row['process_sha256'] and sha(report_path) == row['report_sha256'],
                'Trial evidence changed')
        process = load(process_path)
        require(process.get('state') == 'exited' and type(process.get('exit_code')) is int and process.get('exit_code') == 0
                and process.get('timeout') is False and process.get('error_type') is None
                and type(process.get('pid')) is int and process['pid'] > 0
                and process.get('ended_ns', 0) > process.get('started_ns', 0), 'Native child did not exit cleanly')
        require(process.get('source_commit') == pair['source_commit']
                and process.get('engine_commit') == pair['engine_commit']
                and process.get('host_sha256') == hashes[kind]
                and process.get('module_sha256') == pair['module_sha256']
                and process.get('fixture_digest') == pair['fixture_digest'], 'Timed child identity differs')
        collected[pair_index, kind] = stair_metrics(load(report_path))
    comparisons = []
    device = collected[0, 'reference'][2]
    for pair_index in range(7):
        reference, reference_ready, reference_device = collected[pair_index, 'reference']
        candidate, candidate_ready, candidate_device = collected[pair_index, 'candidate']
        require(reference_device == candidate_device == device, 'GPU device changed between children')
        for a, b in zip(reference, candidate):
            require(all(a[k] == b[k] for k in ('index', 'renderer', 'from_z', 'to_z', 'delta', 'resource_counts')),
                    'Paired stair geometry or resource work differs')
        comparisons.append({'pair': pair_index,
                            'gpu_first_frame_wall_ms': {kind: [row['first_frame_wall_ms'] for row in rows[4:]]
                                                        for kind, rows in (('reference', reference), ('candidate', candidate))},
                            'cpu_first_frame_wall_ms': {kind: [row['first_frame_wall_ms'] for row in rows[:4]]
                                                        for kind, rows in (('reference', reference), ('candidate', candidate))},
                            'native_first_ready_all_eight_summary_ms': {'reference': reference_ready, 'candidate': candidate_ready}})
    ratios = [median(row['gpu_first_frame_wall_ms']['candidate']) / median(row['gpu_first_frame_wall_ms']['reference'])
              for row in comparisons]
    return {'success': True, 'source': pair['source_commit'], 'engine': pair['engine_commit'],
            'pairs': 7, 'clean_children': 14, 'native_stair_transitions': 112, 'gpu_stair_transitions': 56,
            'turn_parser_sha256': PARSER_SHA, 'comparisons': comparisons,
            'device': dict(zip(('name', 'backend', 'software'), device)),
            'per_pair_gpu_first_frame_median_ratio': ratios,
            'performance_accepted': False, 'whole_candidate_qualified': False,
            'ordinary_RTX_FPS': False, 'gpu_execution_measured': False,
            'scope': 'Source-bound backend ablation on one CI runner. Ratios describe complete first-frame wall intervals; all-eight Native first-ready summaries include CPU controls. No resource-child-only gain, historical full-binary equivalence, hardware FPS, statistical admission or freeze-fix claim.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.root)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Seven alternating Native backend pairs, original turn/stair gates and restoration PASS; performance admission is separate.')
