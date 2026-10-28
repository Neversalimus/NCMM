"""Validate per-turn inclusive wall intervals; these are not pure AI/GPU times."""

GROUPS = ((1, 6), (5, 6), (20, 6), (0, 8))
DRAW_SCOPE = 'union of outermost graphics_draw callbacks inside do_turn'
OTHER_SCOPE = 'other Native work, UI, waits and scheduling; not pure AI'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, maximum, name):
    require(type(value) is int and 0 <= value <= maximum, f'invalid {name}')
    return value


def verify(report):
    require(report.get('actual_cdda_sdl') is True and report.get('manual_playthrough') is False,
            'controlled real Native report required')
    data = report.get('native_turn_diagnostics')
    require(type(data) is dict and type(data.get('schema')) is int and data['schema'] == 1,
            'native turn attribution schema required')
    require(data.get('controlled_fixture_without_interactive_main_loop') is True and
            data.get('ordinary_performance_measurement') is False and
            data.get('draw_wall_scope') == DRAW_SCOPE and data.get('remainder_scope') == OTHER_SCOPE,
            'timing scope or controlled fixture descriptor differs')
    samples = data.get('samples')
    require(type(samples) is list and len(samples) == 26, 'all26 real turns required')
    groups = []
    index = 0
    for enemies, count in GROUPS:
        totals = dict(initial_enemies=enemies, turns=count, wall_ns=0, draw_wall_ns=0,
                      other_wall_ns=0, draw_calls=0, outer_draws=0, cpu_draws=0,
                      gpu_draws=0, legacy_draws=0, skipped_draws=0)
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
            for key in totals.keys() - {'initial_enemies', 'turns'}:
                totals[key] += sample[key]
        groups.append(totals)
    return dict(success=True, actual_cdda_sdl=True, controlled_native_turns=26,
                ordinary_performance_measurement=False, gpu_execution_measured=False,
                first_correct_present_measured=False, pure_ai_time_measured=False, groups=groups)
