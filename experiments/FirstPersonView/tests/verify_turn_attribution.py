"""Validate per-turn inclusive wall intervals; these are not pure AI/GPU times."""
import math

GROUPS = ((1, 6), (5, 6), (20, 6), (0, 8))
DRAW_SCOPE = 'union of outermost graphics_draw callbacks inside do_turn'
OTHER_SCOPE = 'other Native work, UI, waits and scheduling; not pure AI'
CALLBACK_SCOPE = 'adjacent outermost callback wall phases; module includes submission/waits, not GPU execution'
CPU_SCOPE = 'fresh validated submit_cpu_metrics event inside this module call; reported components may overlap'
BOUNDARIES = ('entry', 'native_cache_begin', 'native_cache_end', 'visibility_begin',
              'visibility_end', 'module_begin', 'module_end', 'returned')
PHASES = ('dispatch_ns', 'native_cache_ns', 'pre_visibility_ns', 'visibility_ns',
          'post_visibility_ns', 'module_ns', 'presentation_tail_ns')
CPU_TIMES = ('frame_ms', 'geometry_ms', 'raster_ms', 'effects_ms')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, maximum, name):
    require(type(value) is int and 0 <= value <= maximum, f'invalid {name}')
    return value


def verify_callbacks(sample):
    require(sample.get('callback_details') is True, 'callback partition capture required')
    rows = sample.get('callbacks')
    require(type(rows) is list and len(rows) == sample['outer_draws'] and len(rows) <= 32,
            'outermost callback records are missing or exceed capacity')
    result = dict(callback_count=len(rows), world_callbacks=0, other_callbacks_wall_ns=0,
                  **{key: 0 for key in PHASES})
    counts = dict(cpu_draws=0, gpu_draws=0, legacy_draws=0, skipped_draws=0)
    total = 0
    for ordinal, row in enumerate(rows):
        require(type(row) is dict and integer(row.get('ordinal'), 2**32-1, 'ordinal') == ordinal and
                row.get('returned') is True and type(row.get('rendered')) is bool,
                'partial, duplicate or reordered callback')
        mode = row.get('backend')
        require(mode in ('cpu', 'gpu', 'legacy'), 'unknown callback backend')
        for key in ('viewport_width', 'viewport_height'):
            require(type(row.get(key)) is int and -(2**31) <= row[key] < 2**31,
                    f'invalid callback {key}')
        for key in ('image_width', 'image_height'):
            integer(row.get(key), 2**32-1, key)
        integer(row.get('source_cells'), 2**32-1, 'source_cells')
        integer(row.get('source_slices'), 63, 'source_slices')
        points = row.get('boundaries')
        require(type(points) is list and 2 <= len(points) <= len(BOUNDARIES), 'partial callback boundaries')
        expected = BOUNDARIES[:len(points)-1] + ('returned',)
        offsets = []
        for point, tag in zip(points, expected):
            require(type(point) is dict and point.get('tag') == tag, 'missing or reordered callback boundary')
            offsets.append(integer(point.get('offset_ns'), 2**63-1, 'callback offset_ns'))
        require(offsets[0] == 0 and offsets == sorted(offsets), 'overlapping or backwards callback phases')
        total += offsets[-1]
        cpu = row.get('cpu_metrics')
        require(type(cpu) is dict, 'explicit CPU metric availability required')
        reports = integer(cpu.get('reports'), 1, 'CPU reports')
        require(type(cpu.get('available')) is bool and cpu['available'] == (reports == 1),
                'CPU report availability differs')
        for key, limit in (('width', 8192), ('height', 8192), ('raster_calls', 2**32-1),
                           ('triangles', 2**64-1), ('cache_bytes', 2**64-1)):
            integer(cpu.get(key), limit, 'CPU '+key)
        for key in CPU_TIMES:
            require(type(cpu.get(key)) in (int, float) and math.isfinite(cpu[key]) and cpu[key] >= 0,
                    'invalid CPU '+key)
        if reports:
            require(mode == 'cpu' and len(points) == len(BOUNDARIES) and
                    cpu['width'] == row['image_width'] and cpu['height'] == row['image_height'],
                    'CPU metrics differ from their actual module image')
            require(cpu['frame_ms'] <= (offsets[6]-offsets[5])/1e6 + .003,
                    'CPU frame report is outside its captured module interval')
        else:
            require(all(cpu[key] == 0 for key in ('width', 'height', 'raster_calls', 'triangles', 'cache_bytes') + CPU_TIMES),
                    'unavailable CPU metrics contain stale data')
        key = mode + '_draws' if row['rendered'] else 'skipped_draws'
        counts[key] += 1
        if row['rendered'] and mode != 'legacy':
            require(len(points) == len(BOUNDARIES), 'successful world callback is missing phases')
            require(all(1 <= row[key] <= 8192 for key in ('viewport_width', 'viewport_height', 'image_width', 'image_height')),
                    'successful world image has invalid or missing dimensions')
            require(row['source_slices'] >= 1, 'successful world callback is missing source dimensions')
            result['world_callbacks'] += 1
            for key, before, after in zip(PHASES, offsets, offsets[1:]):
                result[key] += after-before
        else:
            result['other_callbacks_wall_ns'] += offsets[-1]
    require(total == sample['draw_wall_ns'], 'callback spans do not equal outermost union')
    require(all(counts[key] <= sample[key] for key in counts), 'root callback backend exceeds total backend count')
    require(sum(result[key] for key in PHASES) + result['other_callbacks_wall_ns'] == total,
            'adjacent callback partition does not conserve wall time')
    return result


def verify(report):
    require(report.get('actual_cdda_sdl') is True and report.get('manual_playthrough') is False,
            'controlled real Native report required')
    data = report.get('native_turn_diagnostics')
    require(type(data) is dict and type(data.get('schema')) is int and data['schema'] == 3,
            'native turn attribution schema required')
    require(data.get('controlled_fixture_without_interactive_main_loop') is True and
            data.get('ordinary_performance_measurement') is False and
            data.get('draw_wall_scope') == DRAW_SCOPE and data.get('remainder_scope') == OTHER_SCOPE and
            data.get('callback_scope') == CALLBACK_SCOPE and data.get('cpu_metric_scope') == CPU_SCOPE,
            'timing scope or controlled fixture descriptor differs')
    samples = data.get('samples')
    require(type(samples) is list and len(samples) == 26, 'all26 real turns required')
    groups = []
    index = 0
    for enemies, count in GROUPS:
        totals = dict(initial_enemies=enemies, turns=count, wall_ns=0, draw_wall_ns=0,
                      other_wall_ns=0, draw_calls=0, outer_draws=0, cpu_draws=0,
                      gpu_draws=0, legacy_draws=0, skipped_draws=0)
        callbacks = dict(callback_count=0, world_callbacks=0, other_callbacks_wall_ns=0,
                         **{key: 0 for key in PHASES})
        for round_number in range(count):
            sample = samples[index]
            index += 1
            require(type(sample) is dict, 'turn record must be an object')
            require(type(sample.get('initial_enemies')) is int and sample['initial_enemies'] == enemies and
                    type(sample.get('round')) is int and sample['round'] == round_number,
                    'missing, duplicated or reordered turn')
            require(sample.get('requested_renderer') == ('gpu' if enemies else 'cpu'),
                    'controlled turn renderer setup differs')
            require(all(sample.get(key) is True for key in ('claimed', 'returned', 'closed', 'valid')),
                    'turn capture is partial or invalid')
            require(integer(sample.get('errors'), 2**32-1, 'errors') == 0, 'turn capture fault')
            for key in ('draw_calls', 'outer_draws', 'max_depth', 'cpu_draws', 'gpu_draws', 'legacy_draws', 'skipped_draws'):
                integer(sample.get(key), 2**32-1, key)
            for key in ('wall_ns', 'draw_wall_ns', 'other_wall_ns'):
                integer(sample.get(key), 2**63-1, key)
            calls, outer, depth = (sample[key] for key in ('draw_calls', 'outer_draws', 'max_depth'))
            require(sum(sample[key] for key in ('cpu_draws', 'gpu_draws', 'legacy_draws', 'skipped_draws')) == calls,
                    'backend counts do not match callback attempts')
            require((1 <= outer <= calls and 1 <= depth <= calls-outer+1) if calls else outer == depth == 0,
                    'invalid outermost callback ancestry')
            require(sample['draw_wall_ns'] + sample['other_wall_ns'] == sample['wall_ns'],
                    'inclusive wall partition is inconsistent')
            require(calls != 0 or sample['draw_wall_ns'] == 0, 'absent callbacks have invented wall time')
            for key, value in verify_callbacks(sample).items():
                callbacks[key] += value
            for key in totals.keys() - {'initial_enemies', 'turns'}:
                totals[key] += sample[key]
        totals['callbacks'] = callbacks
        groups.append(totals)
    return dict(success=True, actual_cdda_sdl=True, controlled_native_turns=26, callback_schema=3,
                cpu_metric_components_adjacent=False,
                ordinary_performance_measurement=False, gpu_execution_measured=False,
                first_correct_present_measured=False, pure_ai_time_measured=False, groups=groups)
