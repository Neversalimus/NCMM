import json
from pathlib import Path
import tempfile
import unittest
from run_native_linux import seed_linux_configuration
from seed_deferred_options import seed


class LinuxConfigurationTests(unittest.TestCase):
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

if __name__=='__main__':unittest.main()
