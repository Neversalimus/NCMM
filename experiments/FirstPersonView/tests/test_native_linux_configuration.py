import json
import hashlib
from pathlib import Path
import tempfile
import unittest
from run_native_linux import seed_linux_configuration,install_linux_fixtures,process_memory
from seed_deferred_options import seed


class LinuxConfigurationTests(unittest.TestCase):
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
