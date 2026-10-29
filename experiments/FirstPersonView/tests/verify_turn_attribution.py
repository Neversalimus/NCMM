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



STAIR_SCOPE = 'CPU then GPU real vertical_move and first successful render/present in adjacent separate captures; fixture reset, warm-up and easing settle excluded'


STAIR_COMPONENT_SCOPE = 'fresh stages appended by this first-frame render; components can overlap and are not additive; CPU recording/submission, not GPU execution'
STAIR_COMMON_STAGES = ('full_frame_wall_ms', 'native_cache_cpu_ms', 'native_tiles_cpu_ms', 'perspective_callback_wall_ms', 'actual_present_wall_ms', 'world_extract_cpu_ms', 'world_visibility_cpu_ms', 'ui_composition_record_cpu_ms')
STAIR_CPU_STAGES = ('cpu_geometry_ms', 'cpu_raster_ms', 'cpu_effects_ms')
STAIR_GPU_STAGES = ('gpu_geometry_prepare_cpu_ms', 'gpu_resource_update_cpu_ms', 'gpu_upload_record_cpu_ms', 'gpu_commands_record_cpu_ms', 'gpu_submit_cpu_ms', 'gpu_batching_pick_cpu_ms', 'gpu_shadow_commands_cpu_ms', 'gpu_world_draw_commands_cpu_ms', 'gpu_creature_draw_commands_cpu_ms', 'gpu_held_draw_commands_cpu_ms', 'gpu_volume_commands_cpu_ms', 'gpu_compose_commands_cpu_ms')

STAIR_GPU_RESOURCE_KEYS = ('static_vertex_buffer_creations', 'static_vertex_buffer_reuses', 'static_vertex_pool_bytes', 'static_index_buffer_creations', 'static_index_buffer_reuses', 'static_index_pool_bytes', 'static_upload_bytes', 'frame_upload_bytes', 'gpu_resource_bytes', 'host_cpu_cache_bytes', 'frame_upload_capacity_bytes', 'frame_upload_creations_total', 'cell_upload_bytes', 'cell_cache_bytes', 'dynamic_geometry_bytes', 'mesh_triangles')
STAIR_RESOURCE_SCOPE = 'fresh counters from this first successful GPU frame; buffer creations/reuses/uploads are per submit, memory/pool bytes are current, frame_upload_creations_total is cumulative within backend lifetime; logical counts, not GPU execution or VRAM'

def verify_stair_transitions(report):
    require(report.get('actual_cdda_sdl') is True and report.get('manual_playthrough') is False,
            'controlled real Native stair report required')
    data = report.get('stair_transition_diagnostics')
    require(type(data) is dict and type(data.get('schema')) is int and data['schema'] == 4 and
            data.get('scope') == STAIR_SCOPE and data.get('ordinary_performance_measurement') is False and
            data.get('gpu_execution_measured') is False, 'stair timing scope differs')
    require(data.get('component_scope') == STAIR_COMPONENT_SCOPE, 'stair component timing scope differs')
    require(data.get('resource_counter_scope') == STAIR_RESOURCE_SCOPE, 'stair resource counter scope differs')
    rows = data.get('samples')
    require(type(rows) is list and len(rows) == 8, 'four CPU and four GPU stair transitions required')
    totals = []
    for index, (row, direction) in enumerate(zip(rows, (1, -1, -1, 1)*2)):
        require(type(row) is dict and row.get('successful_present') is True and
                row.get('requested_renderer') == ('cpu' if index < 4 else 'gpu'), 'missing successful stair frame')
        for name in ('from_z', 'to_z', 'delta'):
            require(type(row.get(name)) is int and -128 <= row[name] <= 128, 'invalid stair '+name)
        require(row['delta'] == direction and row['to_z'] == row['from_z'] + direction,
                'missing, duplicate or reordered stair transition')
        if index:
            require(row['from_z'] == rows[index-1]['to_z'], 'nonadjacent stair source positions')
        detail = {}
        for name in ('action', 'first_frame'):
            sample = row.get(name)
            require(type(sample) is dict and all(sample.get(k) is True for k in ('valid', 'claimed', 'returned', 'closed')),
                    'partial stair capture')
            require(integer(sample.get('errors'), 2**32-1, 'errors') == 0, 'stair capture fault')
            for key in ('draw_calls','outer_draws','max_depth','cpu_draws','gpu_draws','legacy_draws','skipped_draws'):
                integer(sample.get(key), 2**32-1, key)
            for key in ('wall_ns','draw_wall_ns','other_wall_ns'):
                integer(sample.get(key), 2**63-1, key)
            calls,outer,depth = sample['draw_calls'],sample['outer_draws'],sample['max_depth']
            require(sum(sample[k] for k in ('cpu_draws','gpu_draws','legacy_draws','skipped_draws')) == calls and
                    ((1 <= outer <= calls and 1 <= depth <= calls-outer+1) if calls else outer == depth == 0) and
                    sample['draw_wall_ns'] + sample['other_wall_ns'] == sample['wall_ns'] and
                    (calls != 0 or sample['draw_wall_ns'] == 0), 'stair wall partition inconsistent')
            detail[name] = dict(wall_ns=sample['wall_ns'], callbacks=verify_callbacks(sample))
        frame = row['first_frame']
        require(frame['outer_draws'] > 0 and frame['cpu_draws'] + frame['gpu_draws'] > 0 and
                detail['first_frame']['callbacks']['world_callbacks'] > 0, 'no perspective in first stair frame')
        expected_mode = row['requested_renderer']
        require(frame[expected_mode+'_draws'] > 0 and frame[('gpu' if expected_mode == 'cpu' else 'cpu')+'_draws'] == 0,
                'first stair frame did not use the requested backend')
        stages = row.get('frame_stages')
        expected = set(STAIR_COMMON_STAGES + (STAIR_GPU_STAGES if expected_mode == 'gpu' else STAIR_CPU_STAGES))
        require(type(stages) is dict and stages.keys() == expected, 'fresh stair frame stages missing')
        for key, value in stages.items():
            require(type(value) in (int,float) and math.isfinite(value) and 0 <= value <= frame['wall_ns']/1e6 + .003,
                    'invalid stair '+key)
        available = row.get('gpu_resources_available')
        require(type(available) is bool and available == (expected_mode == 'gpu'), 'stair GPU resource availability differs')
        before = integer(row.get('gpu_reports_before'), 2**64-1, 'GPU reports before')
        after = integer(row.get('gpu_reports_after'), 2**64-1, 'GPU reports after')
        require(after == before + (1 if available else 0), 'stale or duplicate stair GPU resource report')
        counts = row.get('gpu_resource_counts')
        require(type(counts) is dict and counts.keys() == (set(STAIR_GPU_RESOURCE_KEYS) if available else set()), 'stair GPU resource counters missing or unexpected')
        for key,value in counts.items():
            integer(value, 2**64-1, key)
        if available:
            require(counts['mesh_triangles'] > 0 and counts['gpu_resource_bytes'] > 0 and counts['frame_upload_capacity_bytes'] > 0, 'empty first-frame GPU resource snapshot')
        detail.update(from_z=row['from_z'],to_z=row['to_z'],delta=row['delta'],frame_stages=stages,gpu_resources_available=available,gpu_resource_counts=counts,gpu_reports_before=before,gpu_reports_after=after)
        totals.append(detail)
    require(rows[-1]['to_z'] == rows[0]['from_z'], 'stair sequence does not return to its source')
    return dict(success=True,transitions=8,renderers=['cpu','gpu'],ordinary_performance_measurement=False,gpu_execution_measured=False,component_scope=STAIR_COMPONENT_SCOPE,resource_counter_scope=STAIR_RESOURCE_SCOPE,stage_components_adjacent=False,samples=totals)
