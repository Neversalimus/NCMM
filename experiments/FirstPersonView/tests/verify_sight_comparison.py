"""Validate isolated Native caster pairs; never certify FPS or first presentation."""
import argparse
import json
import math
from pathlib import Path
import re
import statistics

SCENES = ('street', 'multi_level_horde', 'city_blocks')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(value):
    return (isinstance(value, str) and re.fullmatch(r'0|[1-9][0-9]{0,19}', value) is not None
            and int(value) < 2**64)


def verify(report, scene):
    require(scene in SCENES, 'Unknown sight comparison scene')
    require(type(report) is dict, 'Missing sight comparison object')
    for key in ('success', 'actual_cdda_sdl', 'same_process', 'origin_preserved',
                'moves_preserved', 'calendar_preserved'):
        require(report.get(key) is True, 'Missing exact true receipt: '+key)
    for key in ('manual_playthrough', 'gpu_execution_measured', 'first_correct_present_measured'):
        require(report.get(key) is False, 'Incorrect qualification scope: '+key)
    for key, value in dict(scene=scene, scope='native_3d_caster_only',
                           timing='steady_clock_wall_batch_ms',
                           world_state='warmed_native_caches_no_simulation_turns',
                           reference='original_sight_formula_generic_forwarding',
                           candidate='retained_exact_one_entry_cache').items():
        require(report.get(key) == value, 'Incorrect comparison identity: '+key)
    for key, value in dict(schema=1, width=132, height=132, layers=21, max_view_distance=60,
                           offset_distance=0, repetitions=8,
                           protected_native_bytes=132*132*21*13).items():
        require(type(report.get(key)) is int and report[key] == value,
                'Incomplete or changed Native grid/work scope: '+key)
    require(type(report.get('numerator')) in (int, float) and report['numerator'] == 1,
            'Changed Native sight numerator')
    origin = report.get('origin')
    require(type(origin) is list and len(origin) == 3 and all(type(v) is int for v in origin),
            'Invalid Native origin')
    require(0 <= origin[0] < 132 and 0 <= origin[1] < 132 and -10 <= origin[2] <= 10,
            'Native origin outside protected grids')
    require(report.get('direction') in ('up', 'both'), 'Changed Native cast directions')
    before_identity = report.get('native_before')
    require(digest(before_identity), 'Invalid Native cache identity')
    samples = report.get('samples')
    require(type(samples) is list and len(samples) == 9, 'Missing or extra same-process pairs')
    reference, cached, ratios = [], [], []
    for index, sample in enumerate(samples):
        require(type(sample) is dict and type(sample.get('sample')) is int and sample['sample'] == index,
                'Missing, duplicate or reordered pair')
        require(sample.get('reference_first') is (index % 2 == 0), 'Nonalternating measurement order')
        for key in ('output_bits_equal', 'protected_native_bytes_equal'):
            require(sample.get(key) is True, 'Failed exact comparison: '+key)
        require(type(sample.get('output_floats_compared')) is int and
                sample['output_floats_compared'] == 132*132*21, 'Incomplete XYZ output comparison')
        require(digest(sample.get('reference_digest')) and
                sample['reference_digest'] == sample.get('cached_digest'), 'Different output identities')
        require(sample.get('native_after') == before_identity, 'Native caches changed between pairs')
        for key, target in (('reference_wall_ms', reference), ('cached_wall_ms', cached)):
            value = sample.get(key)
            require(type(value) in (int, float) and math.isfinite(value) and value > 0,
                    'Missing, zero, nonnumeric or nonfinite timing: '+key)
            target.append(value / report['repetitions'])
        ratio = cached[-1] / reference[-1]
        require(math.isfinite(ratio), 'Nonfinite comparison ratio')
        ratios.append(ratio)
    return dict(scene=scene, pairs=9, repetitions=8, first_reference_ms=reference[0],
                first_cached_ms=cached[0], reference_median_ms=statistics.median(reference),
                cached_median_ms=statistics.median(cached),
                paired_ratio_median=statistics.median(ratios),
                regressions=sum(ratio > 1 for ratio in ratios),
                first_correct_present_qualified=False, ordinary_game_fps_qualified=False)


def verify_directory(directory):
    directory = Path(directory)
    expected = {'first-person-sight-cache-'+scene+'.json' for scene in SCENES}
    require({p.name for p in directory.glob('first-person-sight-cache-*.json')} == expected,
            'Missing, extra or ambiguous Native scene files')
    return [verify(json.loads((directory/('first-person-sight-cache-'+scene+'.json')).read_text(
        encoding='utf-8-sig')), scene) for scene in SCENES]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = dict(success=True, scenes=verify_directory(args.directory),
                  limits='Isolated warmed Native 3D caster wall times only; no GPU, complete seen-cache, ordinary game, cold load or first-correct-present qualification.')
    text = json.dumps(result, indent=2)+'\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    print(text, end='')


if __name__ == '__main__':
    main()
