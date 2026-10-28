"""Verify native automation coverage without claiming a manual session or GPU timings."""
import argparse
import json
import math
from pathlib import Path
from verify_fire_audit import verify as verify_fire_audit
from verify_held_gallery import verify as verify_held_gallery
from verify_target_audit import verify as verify_target_audit
from verify_turn_attribution import verify as verify_turn_attribution, verify_stair_transitions

p=argparse.ArgumentParser()
p.add_argument('--root',type=Path,required=True)
p.add_argument('--minimum-soak-seconds',type=int,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
r=json.loads((a.root/'first-person-gameplay-audit.json').read_text(encoding='utf-8-sig'))
assert r['success'] and r['actual_cdda_sdl'] and not r['manual_playthrough']
assert not r['gpu_execution_timestamps_available']
actions={'native_held_assets_and_shared_psx_contacts','native_target_health_identity_visibility_and_font','native_firing_ui_context_and_presented_frames','native_fps_input_and_actions','native_walk_and_visual_continuity','native_door_open_close_and_furniture',
         'native_pickup_inventory_cancel_and_item_use','native_melee_aim_fire_and_reload_completion',
         'native_stairs_and_basement','native_vehicle_board_controls_displacement_and_exit',
         'rapid_tall_avatar_stairs_respect_scene_bounds',
         'native_combat_1_5_20_ai_and_render_state_guards',
         'native_turn_processing','native_f6_and_f12_dispatch'}
assert {x['name'] for x in r['actions']}==actions and all(x['success'] for x in r['actions'])
fire_audit = verify_fire_audit(a.root)
held_gallery = verify_held_gallery(a.root)
target_audit = verify_target_audit(a.root)
turn_attribution = verify_turn_attribution(r)
stair_attribution = verify_stair_transitions(r)
assert r['native_action_stages']['native_game_turn_wall_ms']['samples']==8
assert r['native_action_stages']['native_stair_first_ready_wall_ms']['samples']==4
assert [t['spawned'] for t in r['combat_trials']]==[1,5,20]
assert all(t['native_turns']==6 and t['observed_enemy_attacks']>0 and t['avatar_debug_protection']
           and t['avatar_stamina_reset_each_turn'] and t['avatar_combat_state_reset_each_turn'] for t in r['combat_trials'])
assert len(r['tilesets'])==2 and r['tilesets'][1]['id']=='UndeadPeople_0J_Hybrid_v3'
scenes={'city','dense_smoke','multi_level','creatures_1','creatures_5','creatures_20',
        'creatures_50','creatures_100','large_and_flying','lod_distances','unknown_mod_objects','rotation'}
gpu_stages={'gpu_geometry_prepare_cpu_ms','gpu_resource_update_cpu_ms','gpu_upload_record_cpu_ms',
            'gpu_commands_record_cpu_ms','gpu_submit_cpu_ms','gpu_world_draw_commands_cpu_ms',
            'gpu_creature_draw_commands_cpu_ms','gpu_volume_commands_cpu_ms','gpu_compose_commands_cpu_ms'}
for tileset in r['tilesets']:
    assert tileset['id']==tileset['actual_loaded_id']
    assert {(x['name'],x['renderer']) for x in tileset['cases']}=={(n,m) for n in scenes for m in ['cpu','gpu']}
    for case in tileset['cases']:
        assert case['model_fallback']==0 and len(case['lod'])==4
        assert case['sprite_cache_bytes']<=64*1024*1024 and case['creature_cache_bytes']>=0
        assert case['stages']['full_frame_wall_ms']['samples']==24
        assert math.isfinite(case['first_ready_wall_ms']) and case['first_ready_wall_ms']>=0
        if case['renderer']=='gpu':
            assert case['device'] and case['backend'] and gpu_stages<=case['stages'].keys()
        for stats in case['stages'].values():
            assert stats['samples']==24 and all(math.isfinite(stats[k]) for k in ['median','p95','p99','max'])
            assert 0<=stats['median']<=stats['p95']<=stats['p99']<=stats['max']
        if case['name']=='rotation':
            stress=case['continuous_rotation']
            assert stress['requested_renderer']==case['renderer'] and stress['angle_step_radians']==.12
            assert len(stress['frame_wall_samples'])==len(stress['frame_renderers_gpu'])==24
            assert stress['gpu_frames']==sum(stress['frame_renderers_gpu']) and stress['cpu_frames']==24-stress['gpu_frames']
            assert set(stress['frame_renderers_gpu'])<={0,1}
            assert all(math.isfinite(x) and x>=0 for x in stress['frame_wall_samples'])
            assert math.isfinite(stress['recovery_first_ready_wall_ms']) and stress['recovery_first_ready_wall_ms']>=0
            assert stress['stages']['full_frame_wall_ms']['samples']==24
            assert case['stages']['rotation_pose_ready_wall_ms']['samples']==24
            if case['renderer']=='cpu':assert stress['gpu_frames']==0
    assert all(x['visible_creatures']>0 for x in tileset['cases'] if x['name'] in {'creatures_1','large_and_flying','lod_distances'})
s=r['soak']
assert s['automated'] and s['actual_wall_seconds']>=a.minimum_soak_seconds and s['frames']>0
assert s['f6_cycles']>0 and s['f12_switches']>0
assert s['gpu_resource_peak']<=256*1024*1024 and s['sprite_cache_peak']<=64*1024*1024 and s['world_cache_peak']<8*1024*1024
assert s['rss_samples'] and max(s['rss_samples'])==s['process_rss_peak']
assert len(s['frame_wall_samples'])==len(s['loop_wall_samples'])==len(s['frame_renderers_gpu'])==s['frames']
assert all(math.isfinite(x) and x>=0 for k in ['frame_wall_samples','loop_wall_samples'] for x in s[k])
assert set(s['frame_renderers_gpu'])<={0,1}
save=json.loads((a.root/'first-person-multilevel-save-after.json').read_text(encoding='utf-8-sig'))
assert save['success'] and save['actual_cdda_save_load'] and [x['z'] for x in save['levels']]==[-1,0,1]
assert all(x['restored_xyz'] and x['restored_moves'] and x['restored_turn'] and x['f6_restored_tiles'] for x in save['levels'])
result={'success':True,'actual_cdda_sdl':True,'manual_playthrough':False,'native_action_groups':len(actions),
        'tilesets':[x['id'] for x in r['tilesets']],'paired_cpu_gpu_cases':len(scenes)*2,
        'automated_soak_seconds':s['actual_wall_seconds'],'gpu_execution_timestamps_available':False,
        'native_turn_attribution':turn_attribution,'stair_transition_attribution':stair_attribution}
a.output.write_text(json.dumps(result,indent=2)+'\n')
print('Native action audit, actual UDP, CPU/GPU phases, bounded soak and saves PASS')
