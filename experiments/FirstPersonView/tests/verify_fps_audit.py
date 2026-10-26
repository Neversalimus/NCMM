"""Fail closed on missing Native FPS controls evidence, without certifying manual play."""
from collections import Counter
import argparse,json
from pathlib import Path

COUNTS={'physical_w_8_yaws':8,'wd_one_diagonal':1,'held_w_three_native_boundaries':1,
        'wall_blocks_without_turn':1,'continuous_subtile_without_turn':1,'continuous_wall_slide':1,
        'inventory_held_w_cancel':1,'focus_resize_dpi_release':3,
        'focus_walk_recovery':1,'teleport_walk_recovery':1,
        'e_native_open':1,'lmb_native_melee':2,'lmb_native_reach':2,'lmb_reach_out_of_range':2,'native_run_crouch_wait':1,
        'r_native_reload':1,'u_native_tool':1,'g_native_pickup_cancel':1,
        'e_ground_item_cancel':1,'e_container_cancel':1,'e_window_cancel':1,
        'e_npc_cancel':1,'e_terminal_cancel':1,'e_native_stair':1,'native_offset_stair':1,
        'e_vehicle_cancel':1,'e_occluded_refusal':1,'grass_native_presentation':1,
        'lmb_fps_gun':1,'lmb_fps_free_fire':1,'rmb_fps_ads_fire':1,'lmb_fps_empty':1,'lmb_fps_bow':1}
ACTIONS={'held_w_three_native_boundaries':3,'continuous_wall_slide':3,'native_run_crouch_wait':4,
         'wall_blocks_without_turn':0,'continuous_subtile_without_turn':0,
         'focus_resize_dpi_release':0,'e_occluded_refusal':0,'grass_native_presentation':0,
         'lmb_reach_out_of_range':0,'lmb_fps_empty':0}
FREE={'wall_blocks_without_turn','continuous_subtile_without_turn','inventory_held_w_cancel','focus_resize_dpi_release','g_native_pickup_cancel',
      'e_ground_item_cancel','e_container_cancel','e_window_cancel','e_npc_cancel',
      'e_terminal_cancel','e_vehicle_cancel','e_occluded_refusal','grass_native_presentation',
      'lmb_reach_out_of_range','lmb_fps_empty'}
ALLOW_ZERO={'u_native_tool'} # Native flashlight activation may have no move cost.

def verify(root):
    report=json.loads((root/'first-person-fps-input.json').read_text(encoding='utf-8-sig'))
    assert report['schema']==1 and report['success'] is True and report['actual_cdda_sdl'] is True
    assert report['manual_playthrough'] is False and report['physical_two_monitor_session'] is False
    samples=report['samples']
    assert len(samples)==4*sum(COUNTS.values()),'Missing or extra Native FPS cases'
    for mode in ('keychar','keycode'):
        for renderer in ('cpu','gpu'):
            selected=[s for s in samples if s['keyboard_mode']==mode and s['renderer']==renderer]
            assert Counter(s['scenario'] for s in selected)==COUNTS,'Incomplete keyboard/renderer matrix'
            assert sorted(s['direction'] for s in selected if s['scenario']=='physical_w_8_yaws')==list(range(8))
            assert sorted(s['direction'] for s in selected if s['scenario']=='lmb_native_reach')==[0,1],'Missing captured click with virtual cursor outside viewport'
            assert sorted(s['direction'] for s in selected if s['scenario']=='lmb_reach_out_of_range')==[0,1],'Missing axial or diagonal out-of-range refusal'
            for sample in selected:
                case=sample['scenario'];cost=sample['native_move_cost']
                assert type(cost) is int and cost>=0
                assert sample['native_actions']==ACTIONS.get(case,1),'Queued or missing Native action'
                if case in FREE:assert cost==0,'UI/focus event spent Native moves'
                elif case not in ALLOW_ZERO:
                    assert cost>0,'Gameplay action did not spend Native moves'
                if case=='continuous_subtile_without_turn':
                    assert sample['configured_walking_speed']==1,'Sub-tile presentation probe must use the declared F5 minimum'
                    distance=sample['fractional_distance'];frames=sample['presented_frames']
                    assert type(distance) in (int,float) and .001<distance<.49,'Missing fractional movement inside the original tile'
                    assert type(frames) is int and frames>=2,'Fractional movement was not presented by Native'
    return dict(success=True,actual_cdda_sdl=True,samples=len(samples),manual_playthrough=False,
                native_action_costs_verified=True,
                limitations=['Controlled isolated world; ordinary AI turns are covered by the separate gameplay audit',
                             'Synthetic window events; physical DPI/two-monitor and manual sessions remain unverified',
                             'Selector cancellation verifies Native UI ownership, not completion of every menu branch',
                             'The stair case looks down while crouching at the existing pitch limit',
                             'These cases do not alone qualify every interaction category or performance'])

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();result=verify(args.root)
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({key:result[key] for key in ('success','samples','native_action_costs_verified')}))
