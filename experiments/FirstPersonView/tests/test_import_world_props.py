"""Offline source provenance, deterministic tables and hostile bounded inputs."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from import_world_props import WorldInputs,convert
from test_import_held_items import glb_fixture


class WorldPropImportTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)

    def fixture(self,data=None):
        data=glb_fixture() if data is None else data
        (self.root/'prop.glb').write_bytes(data)
        selection=dict(schema=1,license='CC0-1.0',files={'prop.glb':hashlib.sha256(data).hexdigest()},models=[dict(id='Prop',source='prop.glb')])
        self.write(selection);return selection

    def write(self,selection):
        (self.root/'selection.json').write_text(json.dumps(selection),encoding='utf-8')

    def test_all_43_shipped_sources_reproduce_exact_tables(self):
        root=Path(__file__).resolve().parents[1]/'assets/world_props'
        result=convert(root);manifest=json.loads(result['manifest.json'])
        self.assertEqual(len(manifest['models']),43)
        for name,data in result.items():self.assertEqual(data,(root/'generated'/name).read_bytes(),name)
        selection=json.loads((root/'selection.json').read_text())
        self.assertEqual(selection['license'],'CC0-1.0')
        license_name='source/kenney/License.txt'
        self.assertIn(license_name,selection['files'])
        self.assertIn('Creative Commons Zero',(root/license_name).read_text())

    def test_small_weighted_fixture_bakes_to_ground(self):
        self.fixture();first=convert(self.root);self.assertEqual(first,convert(self.root))
        model=json.loads(first['manifest.json'])['models'][0]
        self.assertEqual(model['id'],'Prop');self.assertEqual(model['triangles'],1)
        self.assertGreater(model['dimensions'][2],.5)

    def test_modified_source_fails_hash_gate(self):
        self.fixture();(self.root/'prop.glb').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'mismatch'):convert(self.root)

    def test_escaping_and_unlisted_sources_rejected(self):
        selection=self.fixture()
        for path in ('../outside.glb','/outside.glb','C:/outside.glb','source\\bad.glb','unlisted.glb'):
            with self.subTest(path=path):
                selection['models'][0]['source']=path;self.write(selection)
                with self.assertRaises(ValueError):convert(self.root)

    def test_duplicate_or_cpp_identifier_injection_rejected(self):
        for name in ('Prop;int x','../Prop','-Name','0Thing','a'*42):
            selection=self.fixture();selection['models'][0]['id']=name;self.write(selection)
            with self.assertRaisesRegex(ValueError,'identifier'):convert(self.root)
        selection=self.fixture();selection['models']*=2;self.write(selection)
        with self.assertRaisesRegex(ValueError,'Duplicate'):convert(self.root)

    def test_animations_never_silently_dropped(self):
        self.fixture(glb_fixture(clips=True))
        with self.assertRaisesRegex(ValueError,'animation|clip|Animation'):convert(self.root)

    def test_selection_and_inventory_bounded(self):
        selection=self.fixture();selection['models']*=101
        with self.assertRaises(ValueError):WorldInputs(self.root,selection)
        selection=self.fixture();selection['license']='unknown'
        with self.assertRaises(ValueError):WorldInputs(self.root,selection)


if __name__=='__main__':unittest.main()
