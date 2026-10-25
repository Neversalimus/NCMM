"""Synthetic verifier fixtures, never Native evidence."""
import json,tempfile,unittest
from pathlib import Path
from verify_fps_audit import verify,COUNTS,ACTIONS,FREE

class FpsEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.report=dict(schema=1,success=True,actual_cdda_sdl=True,manual_playthrough=False,
                         physical_two_monitor_session=False,samples=[])
        for mode in ('keychar','keycode'):
            for renderer in ('cpu','gpu'):
                for case,count in COUNTS.items():
                    for i in range(count):
                        self.report['samples'].append(dict(scenario=case,keyboard_mode=mode,renderer=renderer,
                            native_actions=ACTIONS.get(case,1),native_move_cost=0 if case in FREE else 100,direction=i))
    def run_report(self):
        (self.root/'first-person-fps-input.json').write_text(json.dumps(self.report))
        return verify(self.root)
    def test_complete_fixture(self):self.assertEqual(self.run_report()['samples'],132)
    def test_missing_case_rejected(self):
        self.report['samples'].pop()
        with self.assertRaises(AssertionError):self.run_report()
    def test_captured_outside_click_case_required(self):
        for sample in self.report['samples']:
            if sample['scenario']=='lmb_native_reach':sample['direction']=0
        with self.assertRaises(AssertionError):self.run_report()
    def test_one_layout_cannot_stand_for_both(self):
        for sample in self.report['samples']:sample['keyboard_mode']='keycode'
        with self.assertRaises(AssertionError):self.run_report()
    def test_repeated_yaw_cannot_stand_for_eight(self):
        self.report['samples'][1]['direction']=0
        with self.assertRaises(AssertionError):self.run_report()
    def test_uncosted_movement_rejected(self):
        self.report['samples'][0]['native_move_cost']=0
        with self.assertRaises(AssertionError):self.run_report()
    def test_accidental_ui_turn_rejected(self):
        next(s for s in self.report['samples'] if s['scenario'] in FREE)['native_move_cost']=1
        with self.assertRaises(AssertionError):self.run_report()
    def test_held_queue_rejected(self):
        next(s for s in self.report['samples'] if s['scenario']=='held_w_three_native_boundaries')['native_actions']=100
        with self.assertRaises(AssertionError):self.run_report()
    def test_missing_native_reload_cost_rejected(self):
        next(s for s in self.report['samples'] if s['scenario']=='r_native_reload')['native_move_cost']=0
        with self.assertRaises(AssertionError):self.run_report()
    def test_native_tool_may_be_free(self):
        for sample in self.report['samples']:
            if sample['scenario']=='u_native_tool':sample['native_move_cost']=0
        self.assertTrue(self.run_report()['success'])
    def test_cancelled_interaction_cannot_spend_moves(self):
        next(s for s in self.report['samples'] if s['scenario']=='e_npc_cancel')['native_move_cost']=100
        with self.assertRaises(AssertionError):self.run_report()

if __name__=='__main__':unittest.main()
