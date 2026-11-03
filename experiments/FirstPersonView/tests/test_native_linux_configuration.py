import json
import hashlib
from pathlib import Path
import tempfile
import unittest
import subprocess
from run_native_linux import seed_linux_configuration,install_linux_fixtures,process_memory,observe_process_memory
from seed_deferred_options import seed


class LinuxConfigurationTests(unittest.TestCase):
    class Child:
        def __init__(self,code=None):self.code=code;self.waits=[]
        def wait(self,timeout=None):
            self.waits.append(timeout)
            if self.code is None:raise subprocess.TimeoutExpired('owned-fixture',timeout)
            return self.code

    def test_valid_live_memory_does_not_wait_or_change_units(self):
        child=self.Child()
        self.assertEqual(observe_process_memory(child,lambda:'VmRSS: 1200 kB\nVmHWM: 1400 kB\n'),
                         dict(rss_bytes=1200*1024,peak_rss_bytes=1400*1024))
        self.assertEqual(child.waits,[])

    def test_missing_terminal_fields_require_proven_exit_without_inventing_zero_memory(self):
        child=self.Child(0)
        self.assertIsNone(observe_process_memory(child,lambda:'Name: ncmm\nState: R (running)\n'))
        self.assertEqual(child.waits,[5])

    def test_live_malformed_memory_still_fails_closed(self):
        child=self.Child()
        with self.assertRaisesRegex(ValueError,'VmHWM'):
            observe_process_memory(child,lambda:'VmRSS: 4 kB\n')
        self.assertEqual(child.waits,[5])

    def test_missing_proc_file_is_terminal_only_after_a_confirmed_exit(self):
        def missing():raise FileNotFoundError('owned-proc-status')
        child=self.Child(0);self.assertIsNone(observe_process_memory(child,missing));self.assertEqual(child.waits,[5])
        with self.assertRaises(FileNotFoundError):observe_process_memory(self.Child(),missing)

    def test_failed_child_exit_is_retained_for_the_original_process_gate(self):
        child=self.Child(9)
        self.assertIsNone(observe_process_memory(child,lambda:'Name: ncmm\n'))
        self.assertEqual(child.wait(),9)

    def test_permission_fault_is_never_treated_as_process_exit(self):
        def denied():raise PermissionError('owned-proc-status')
        child=self.Child(0)
        with self.assertRaises(PermissionError):observe_process_memory(child,denied)
        self.assertEqual(child.waits,[])

    def test_process_memory_keeps_kernel_units_and_peak_separate(self):
        value=process_memory('Name:\tncmm\nVmHWM:\t 1400 kB\nVmRSS:\t 1200 kB\n')
        self.assertEqual(value,dict(rss_bytes=1200*1024,peak_rss_bytes=1400*1024))
        with self.assertRaises(ValueError):process_memory('VmRSS: 4 kB\n')

    def test_fresh_fixture_selects_a_real_language_before_startup(self):
        with tempfile.TemporaryDirectory() as folder:
            game=Path(folder);seed_linux_configuration(game)
            self.assertEqual(json.loads((game/'config/options.json').read_text()),[{'name':'USE_LANG','value':'en'}])

    def test_existing_configuration_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as folder:
            game=Path(folder);path=game/'config/options.json';path.parent.mkdir();path.write_text('[{"owned":true}]')
            with self.assertRaisesRegex(ValueError,'fresh owned'):seed_linux_configuration(game)
            self.assertEqual(path.read_text(),'[{"owned":true}]')

    def test_all_four_serialized_typed_values_preserve_startup_language(self):
        with tempfile.TemporaryDirectory() as folder:
            game=Path(folder);seed_linux_configuration(game);seed(game)
            values={v['name']:v['value'] for v in json.loads((game/'config/options.json').read_text())}
            fixture=json.loads(Path(__file__).with_name('deferred_options_fixture.json').read_text())
            self.assertEqual(values['USE_LANG'],'en')
            self.assertEqual(len(values),5)
            self.assertTrue(all(values[v['name']]==v['value'] for v in fixture))

    def test_isolated_data_includes_the_actual_unknown_mod_appearance_definition(self):
        with tempfile.TemporaryDirectory() as folder:
            game=Path(folder);inventory=install_linux_fixtures(game)
            self.assertEqual(len(inventory),2)
            for relative,expected in inventory.items():
                self.assertEqual(hashlib.sha256((game/relative).read_bytes()).hexdigest(),expected)
            definitions=json.loads((game/'data/json/monsters/ncmm_appearance_qualification.json').read_text())
            self.assertTrue(any(item.get('id')=='mon_ncmm_appearance_unknown' for item in definitions))

    def test_missing_required_fixture_fails_before_copying_any_data(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=root/'sources';source.mkdir();game=root/'game';game.mkdir()
            (source/'multilevel_fixture.json').write_text('[]')
            with self.assertRaises(FileNotFoundError):install_linux_fixtures(game,source)
            self.assertEqual(list(game.rglob('*')),[])

    def test_existing_fixture_is_never_replaced_or_partly_overwritten(self):
        with tempfile.TemporaryDirectory() as folder:
            game=Path(folder);path=game/'data/json/monsters/ncmm_appearance_qualification.json'
            path.parent.mkdir(parents=True);path.write_text('[{"owned":true}]')
            with self.assertRaisesRegex(ValueError,'fresh owned'):install_linux_fixtures(game)
            self.assertEqual(path.read_text(),'[{"owned":true}]')
            self.assertFalse((game/'data/json/furniture_and_terrain/ncmm_multilevel_qualification.json').exists())

if __name__=='__main__':unittest.main()
