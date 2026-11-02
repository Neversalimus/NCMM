"""Strict numeric admission of actual full-image material-palette controls.

Synthetic self-tests validate this parser and are never GPU or game evidence.
"""
import argparse
import copy
import json


def validate(d):
    root = {'full_viewport', 'original501_and930_controls_unchanged',
            'exact_float_material_palette_only', 'ordinary_fallback_after_palette', 'pairs', 'pair_count',
            'active_pairs', 'fallback_pairs', 'saved_draw_calls',
            'different_pixels', 'success'}
    row = {'variant', 'view', 'saved_draw_calls', 'source_draws_equal',
           'palette_draws', 'original_material_ranges',
           'rendered_triangles_equal', 'actual_parts', 'pixels_equal',
           'different_pixels', 'focus_equal', 'target_equal'}
    booleans = {'source_draws_equal', 'rendered_triangles_equal',
                'pixels_equal', 'focus_equal', 'target_equal'}
    assert type(d) is dict and set(d) == root
    assert d['full_viewport'] == [1280, 800]
    assert d['original501_and930_controls_unchanged'] is True
    assert d['exact_float_material_palette_only'] is True
    assert d['ordinary_fallback_after_palette'] is True
    for key in ['pair_count', 'active_pairs', 'fallback_pairs',
                'saved_draw_calls', 'different_pixels']:
        assert type(d[key]) is int
    assert d['pair_count'] == 60 and 0 < d['active_pairs'] < 60
    assert 0 < d['fallback_pairs'] < 60
    assert d['active_pairs'] + d['fallback_pairs'] == 60
    assert type(d['pairs']) is list and len(d['pairs']) == 60
    assert [(r.get('variant'), r.get('view')) for r in d['pairs']
            if type(r) is dict] == [(v, n) for v in range(6) for n in range(10)]
    saved = different = active = fallback = calls = ranges = 0
    equal = True
    for r in d['pairs']:
        assert type(r) is dict and set(r) == row
        for key in booleans:
            assert type(r[key]) is bool
        for key in row - booleans:
            assert type(r[key]) is int
        assert 0 <= r['variant'] < 6 and 0 <= r['view'] < 10
        assert 0 < r['actual_parts'] <= 231
        if r['variant'] == 0:
            assert r['actual_parts'] == 231
        assert r['source_draws_equal'] is True
        assert r['rendered_triangles_equal'] is True
        assert 0 <= r['palette_draws'] <= r['actual_parts']
        assert 8 * r['palette_draws'] <= r['original_material_ranges']
        assert r['original_material_ranges'] <= 128 * r['palette_draws']
        assert r['saved_draw_calls'] == r['original_material_ranges'] - r['palette_draws']
        assert 0 <= r['different_pixels'] <= 1280 * 800
        assert r['pixels_equal'] == (r['different_pixels'] == 0)
        if r['variant'] == r['view'] == 0:
            assert r['saved_draw_calls'] > 0
        saved += r['saved_draw_calls']
        calls += r['palette_draws']
        ranges += r['original_material_ranges']
        different += r['different_pixels']
        active += int(r['saved_draw_calls'] > 0)
        fallback += int(r['saved_draw_calls'] == 0)
        equal &= r['pixels_equal'] and r['focus_equal'] and r['target_equal']
    assert (saved, different, active, fallback) == (
        d['saved_draw_calls'], d['different_pixels'],
        d['active_pairs'], d['fallback_pairs'])
    assert saved > 0 and type(d['success']) is bool and d['success'] == equal
    return dict(success=d['success'], pair_count=60, compared_pixels=61440000,
                different_pixels=different, active_pairs=active,
                fallback_pairs=fallback, saved_draw_calls_fixture_sum=saved,
                palette_draw_calls_fixture_sum=calls,
                original_material_ranges_fixture_sum=ranges,
                all_original_Native_parts_geometry_and_indices_retained=True,
                all_focus_targets_equal=all(r['focus_equal'] and r['target_equal']
                                            for r in d['pairs']),
                not_Native_game_or_hardware_FPS=True,
                no_original501_930_or_shader_or_full_qualification_override=True)


def synthetic():
    rows = []
    for v in range(6):
        for n in range(10):
            enabled = n == 0
            rows.append(dict(variant=v, view=n, saved_draw_calls=7 if enabled else 0,
                             source_draws_equal=True, palette_draws=int(enabled),
                             original_material_ranges=8 if enabled else 0,
                             rendered_triangles_equal=True, actual_parts=231,
                             pixels_equal=True, different_pixels=0,
                             focus_equal=True, target_equal=True))
    return dict(full_viewport=[1280, 800], original501_and930_controls_unchanged=True,
                exact_float_material_palette_only=True, ordinary_fallback_after_palette=True, pairs=rows, pair_count=60,
                active_pairs=6, fallback_pairs=54, saved_draw_calls=42,
                different_pixels=0, success=True)


def self_test():
    base = synthetic()
    assert validate(base)['success'] is True
    failed_image = copy.deepcopy(base)
    failed_image['pairs'][0].update(pixels_equal=False, different_pixels=1)
    failed_image.update(different_pixels=1, success=False)
    assert validate(failed_image)['success'] is False
    mutations = [
        lambda d: d.update(full_viewport=[640, 400]),
        lambda d: d.update(original501_and930_controls_unchanged=False),
        lambda d: d.update(exact_float_material_palette_only=False),
        lambda d: d.update(ordinary_fallback_after_palette=False),
        lambda d: d.update(pair_count=59),
        lambda d: d.update(active_pairs=0),
        lambda d: d.update(fallback_pairs=0),
        lambda d: d.update(saved_draw_calls=41),
        lambda d: d.update(different_pixels=1),
        lambda d: d.update(success=False),
        lambda d: d.update(success=1),
        lambda d: d.update(extra=True),
        lambda d: d['pairs'].pop(),
        lambda d: d['pairs'].__setitem__(1, copy.deepcopy(d['pairs'][0])),
        lambda d: d['pairs'].reverse(),
        lambda d: d['pairs'][0].update(variant=True),
        lambda d: d['pairs'][0].update(view=10),
        lambda d: d['pairs'][0].update(actual_parts=230),
        lambda d: d['pairs'][10].update(actual_parts=232),
        lambda d: d['pairs'][0].update(source_draws_equal=False),
        lambda d: d['pairs'][0].update(rendered_triangles_equal=False),
        lambda d: d['pairs'][0].update(palette_draws=True),
        lambda d: d['pairs'][0].update(palette_draws=-1),
        lambda d: d['pairs'][0].update(original_material_ranges=129),
        lambda d: d['pairs'][0].update(original_material_ranges=7),
        lambda d: d['pairs'][0].update(saved_draw_calls=-1),
        lambda d: d['pairs'][0].update(saved_draw_calls=0),
        lambda d: d['pairs'][0].update(different_pixels=1),
        lambda d: d['pairs'][0].update(pixels_equal=False),
        lambda d: d['pairs'][0].update(focus_equal=False),
        lambda d: d['pairs'][0].update(target_equal=False),
        lambda d: d['pairs'][0].update(pixels_equal=1),
        lambda d: d['pairs'][0].update(extra=False),
    ]
    for mutate in mutations:
        d = copy.deepcopy(base)
        mutate(d)
        try:
            validate(d)
        except (AssertionError, KeyError, TypeError, ValueError):
            continue
        raise AssertionError('Corrupt palette receipt was admitted')
    return len(mutations) + 2


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--report')
    args = parser.parse_args()
    if args.self_test:
        print(json.dumps(dict(parser_checks=self_test(), synthetic_only=True)))
    if args.report:
        from pathlib import Path
        raw = Path(args.report).read_bytes()
        assert len(raw) <= 65536
        result = validate(json.loads(raw))
        print(json.dumps(result))
        assert result['success'], 'Actual palette pixels or target queries differ'
    assert args.self_test or args.report
