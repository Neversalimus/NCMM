"""Verify a diagnostic Native stair experiment; never qualify gameplay or FPS."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from PIL import Image

BODY = 16
NAV = 256 | 1024 | 2048 | 16384 | 32768 | 131072
THRESHOLDS = dict(interaction_triangles=24, interaction_pixels=8, rgb_interaction=4)
PHASES = ('normal_before', 'debug', 'normal_after')
MATRIX = ('matrix11', 'matrix10', 'matrix01', 'matrix00')
ROI = [240, 160, 160, 280]
CAMERA = [-math.pi / 2, -.1]
MAX_LEDGER_CELLS = 97 * 97 * 32


def integer(value, low=0, high=(1 << 64) - 1):
    assert type(value) is int and low <= value <= high, 'Invalid integer evidence'
    return value


def number(value):
    assert type(value) in (int, float) and math.isfinite(value), 'Nonfinite scalar evidence'
    return value


def digest(value):
    assert type(value) is str and value.isascii() and value.isdecimal(), 'Invalid Native digest'
    assert 0 <= int(value) < 1 << 64, 'Oversized Native digest'
    return value


def xyz(value):
    assert type(value) is list and len(value) == 3
    for coordinate in value:
        integer(coordinate, -(1 << 31), (1 << 31) - 1)
    assert -128 <= value[2] <= 128
    return value


def bounds(value, maximum):
    assert type(value) is list and len(value) == 2
    lower, upper = map(xyz, value)
    assert all(a <= b for a, b in zip(lower, upper)), 'Reversed ledger bounds'
    count = math.prod(b - a + 1 for a, b in zip(lower, upper))
    assert count <= maximum, 'Unbounded diagnostic ledger'
    return count


def component(record):
    integer(record['count'], 0, 1_000_000)
    digest(record['hash'])


def ledger(record):
    physical = record['physical']
    for name in ('terrain', 'furniture', 'fields', 'map_items', 'avatar_items', 'creatures', 'vehicles', 'light'):
        component(physical[name])
    state = physical['scalars']
    xyz(state['avatar']); xyz(state['origin'])
    integer(state['avatar_moves'], -(1 << 31), (1 << 31) - 1)
    for name in ('avatar_hp', 'avatar_hp_max', 'turn'):
        integer(state[name])
    assert state['avatar_hp'] <= state['avatar_hp_max']
    assert type(state['weather']) is str and state['weather']
    for name in ('winddirection', 'windspeed', 'eye_height', 'animation_seconds'):
        number(state[name])
    assert type(state['lightning']) is bool
    assert type(state['camera']) is list and len(state['camera']) == 2
    for angle in state['camera']:
        number(angle)
    assert 0 < state['eye_height'] < 5
    component(record['memory']); component(record['overmap'])
    assert type(record['memory']['valid']) is bool
    assert type(record['memory']['show']) is bool
    integer(record['overmap']['resident_tiles'], 0, record['overmap']['count'])
    return record


def conserved(record):
    # A transient raw-light-cache mismatch makes the experiment inconclusive.
    # Actual terrain/items/actors/moves/discovery changes invalidate it.
    return dict(physical={key: value for key, value in record['physical'].items() if key != 'light'},
                memory=record['memory'], overmap=record['overmap'])


def control(record):
    for name in ('present', 'raw_terrain_up', 'raw_furniture_up', 'native_sees', 'focus_attempted'):
        assert type(record[name]) is bool, 'Missing control observation'
    integer(record['index'], -1, 65535)
    assert record['present'] == (record['index'] >= 0)
    integer(record['flags'], 0, (1 << 32) - 1)
    integer(record['direction'], 0, 3)
    for name in ('terrain', 'furniture', 'item', 'creature', 'field', 'raw_terrain', 'raw_furniture'):
        assert type(record[name]) is str and len(record[name]) <= 256
    number(record['ambient_light'])
    for key in ('light', 'published_light'):
        assert type(record[key]) is list and len(record[key]) == 3
        for channel in record[key]:
            assert number(channel) >= 0
    for name in ('focus_result', 'target_result'):
        integer(record[name], 0, 1)
    return record


def pixels(root, record, scene, name):
    assert record['file'] == f'first-person-static-stair-{scene}-{name}.bmp', 'Wrong capture identity'
    path = (root / record['file']).resolve()
    assert path.parent == root.resolve(), 'Capture outside evidence directory'
    assert 0 < path.stat().st_size <= 2 * 1024 * 1024, 'Invalid capture size'
    with Image.open(path) as source:
        assert source.format == 'BMP' and source.size == (640, 480), 'Wrong actual raster dimensions'
        data = source.convert('RGB').tobytes()
    integer(record['triangles'], 1, 10_000_000)
    integer(record['raster_calls'], 1, 1000)
    assert record['raster_width'] == 640 and record['raster_height'] == 480
    for key in ('world_digest', 'protected_snapshot_digest', 'pixel_fingerprint'):
        digest(record[key])
    return data


def interaction(images, roi):
    """Signed factorial interaction, with every subtraction done before abs."""
    assert len(images) == 4 and all(len(image) == 640 * 480 * 3 for image in images)
    x, y, width, height = roi
    changed = 0
    for row in range(y, y + height):
        for column in range(x, x + width):
            at = (row * 640 + column) * 3
            strength = max(abs(images[0][at + c] - images[1][at + c] - images[2][at + c] + images[3][at + c]) for c in range(3))
            changed += strength >= THRESHOLDS['rgb_interaction']
    return changed


def verify(root):
    root = Path(root)
    report = json.loads((root / 'first-person-static-stair-audit.json').read_text(encoding='utf-8-sig'))
    assert integer(report['schema'], 1, 1) == 1 and report['success'] is True
    assert report['diagnostic_only'] is True and report['actual_cdda_sdl'] is True
    assert integer(report['preflight_frames'], 2, 2) == 2, 'Missing fixed preflight declaration'
    assert report['renderer'] == 'cpu' and report['width'] == 640 and report['height'] == 480
    assert type(report['fixture_camera']) is list and len(report['fixture_camera']) == 2
    assert all(abs(number(a) - b) < 1e-5 for a, b in zip(report['fixture_camera'], CAMERA)), 'Fixture camera changed after declaration'
    assert report['thresholds'] == THRESHOLDS, 'Causal thresholds changed after observation'
    cases = report['cases']
    assert len(cases) == 3 and [case['name'] for case in cases] == ['day', 'dusk', 'night']
    results = []
    for case in cases:
        scene = case['name']
        assert case['roi'] == ROI, 'Stair ROI changed after observation'
        locations = [xyz(case[key]) for key in ('target', 'furniture_control', 'hidden_control')]
        assert len(set(map(tuple, locations))) == 3, 'Terrain and furniture controls overlap'
        expected_counts = {key: bounds(case[key], maximum) for key, maximum in (
            ('physical_bounds', MAX_LEDGER_CELLS), ('memory_bounds', MAX_LEDGER_CELLS), ('omt_bounds', 100_000))}
        for point in locations:
            assert all(a <= value <= b for value, a, b in zip(point, *case['physical_bounds'])), 'Control outside physical ledger'
        phases = case['phases']
        assert [phase['name'] for phase in phases] == list(PHASES), 'Incomplete normal/debug/normal observation'
        normal = ledger(phases[0]['ledger_before'])
        state = normal['physical']['scalars']
        assert all(abs(a - b) < 1e-5 for a, b in zip(state['camera'], CAMERA)), 'Native camera differs from the fixed fixture'
        delta = [a - b for a, b in zip(case['target'], state['avatar'])]
        assert delta[2] == 0 and 2 <= max(abs(delta[0]), abs(delta[1])) <= 4, 'Target uses forced avatar BODY'
        assert normal['physical']['terrain']['count'] == expected_counts['physical_bounds']
        assert normal['physical']['furniture']['count'] == expected_counts['physical_bounds']
        assert normal['physical']['map_items']['count'] > 0 and normal['physical']['creatures']['count'] > 0, 'Vacuous hidden-content controls'
        assert normal['memory']['count'] == expected_counts['memory_bounds']
        assert normal['overmap']['count'] == expected_counts['omt_bounds']
        rendered = []
        raw_light_stable = True
        for phase in phases:
            assert phase['debug_nightvision'] is (phase['name'] == 'debug')
            for key in ('ledger_before', 'ledger_after'):
                observed = ledger(phase[key])
                assert conserved(observed) == conserved(normal), 'Native state/discovery changed'
                raw_light_stable &= observed['physical']['light'] == normal['physical']['light']
            target = control(phase['target'])
            furniture = control(phase['furniture_control'])
            hidden = control(phase['hidden_control'])
            assert target['raw_terrain_up'] and not target['raw_furniture_up']
            assert not furniture['raw_terrain_up'] and furniture['raw_furniture_up'], 'Furniture GOES_UP became terrain evidence'
            assert hidden['raw_furniture'] not in ('', 'f_null'), 'Missing physical hidden furniture control'
            assert not (hidden['flags'] & BODY), 'Hidden control became body-visible'
            assert all(not hidden[name] for name in ('item', 'furniture', 'creature', 'field'))
            assert hidden['focus_result'] == hidden['target_result'] == 0, 'Hidden contents became an interaction target'
            rendered.append(pixels(root, phase, scene, phase['name']))
        first, debug, last = phases
        repeated = rendered[0] == rendered[2]
        assert type(case['normal_repeat_pixels_equal']) is bool and case['normal_repeat_pixels_equal'] == repeated, 'Actual normal pixels contradict the reported repeat flag'
        source_repeated = (first['target'] == last['target'] and first['world_digest'] == last['world_digest']
                           and first['protected_snapshot_digest'] == last['protected_snapshot_digest'])
        target, changed_target = last['target'], debug['target']
        missing = changed_target['flags'] & NAV & ~target['flags']
        eligible = (raw_light_stable and normal['memory']['valid'] and normal['memory']['show'] and repeated and source_repeated
                    and target['present'] and changed_target['present'] and not (target['flags'] & BODY)
                    and (changed_target['flags'] & (BODY | 256 | 16384)) == (BODY | 256 | 16384) and bool(missing)
                    and target['ambient_light'] == changed_target['ambient_light'] and target['light'] == changed_target['light'])
        assert type(case['matrix_eligible']) is bool and case['matrix_eligible'] == bool(eligible), 'Causal eligibility differs from raw observations'
        triangle_interaction = pixel_interaction = None
        if eligible:
            assert case['inconclusive_reason'] == ''
            matrix = case['matrix']
            assert [frame['name'] for frame in matrix] == list(MATRIX), 'Incomplete or reordered factorial controls'
            matrix_pixels = []
            for frame in matrix:
                body, semantics = map(int, frame['name'][-2:])
                assert type(frame['body']) is int and type(frame['semantics']) is int
                assert (frame['body'], frame['semantics']) == (body, semantics)
                expected = (target['flags'] | missing if semantics else target['flags'])
                expected = expected | BODY if body else expected & ~BODY
                assert frame['target_flags'] == expected, 'Matrix changed unrelated terrain flags'
                assert frame['target_direction'] == (changed_target['direction'] if semantics else target['direction'])
                assert frame['published_light'] == target['published_light'], 'Matrix changed normal published illumination'
                assert frame['protected_snapshot_digest'] == last['protected_snapshot_digest'], 'Owned snapshot contents were altered'
                assert ledger(frame['ledger_after']) == normal, 'Matrix changed Native state/discovery'
                matrix_pixels.append(pixels(root, frame, scene, frame['name']))
            assert len({frame['world_digest'] for frame in matrix}) == 4, 'Matrix did not exercise four distinct inputs'
            assert matrix[-1]['world_digest'] == last['world_digest'], 'Matrix00 is not the untouched normal snapshot'
            assert case['matrix00_normal_pixels_equal'] is True and matrix_pixels[-1] == rendered[2], 'Actual matrix00 pixels differ from normal'
            triangle_interaction = sum(sign * frame['triangles'] for sign, frame in zip((1, -1, -1, 1), matrix))
            pixel_interaction = interaction(matrix_pixels, ROI)
            confirmed = triangle_interaction >= THRESHOLDS['interaction_triangles'] and pixel_interaction >= THRESHOLDS['interaction_pixels']
            reason = '' if confirmed else 'Insufficient terrain-only geometry/pixel interaction at the fixed thresholds'
        else:
            assert case['matrix'] == [] and case['matrix00_normal_pixels_equal'] is False
            assert type(case['inconclusive_reason']) is str and case['inconclusive_reason']
            confirmed, reason = False, case['inconclusive_reason']
        results.append(dict(name=scene, status='confirmed' if confirmed else 'inconclusive', reason=reason,
                            interaction_triangles=triangle_interaction, interaction_pixels=pixel_interaction,
                            debug_published_light_changed=target['published_light'] != changed_target['published_light'],
                            normal_rgb_sha256=hashlib.sha256(rendered[2]).hexdigest()))
    return dict(success=True, diagnostic_only=True, actual_cdda_sdl=True, renderer='cpu',
                ordinary_performance_measurement=False, manual_playthrough=False,
                status='confirmed' if any(case['status'] == 'confirmed' for case in results) else 'inconclusive',
                limitations=['Debug observation may publish different illumination; its pixels are not a causal comparison',
                             'Every factorial control retains the same normal appearance and published illumination',
                             'Two declared preflight frames precede observations; this is not a first-frame performance probe',
                             'This diagnostic does not qualify a production visibility fix or ordinary game frame time'],
                thresholds=THRESHOLDS, cases=results)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.root)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))
