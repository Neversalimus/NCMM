"""Offline source provenance, deterministic tables and hostile bounded inputs."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
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

    def test_all_shipped_sources_reproduce_exact_tables(self):
        root=Path(__file__).resolve().parents[1]/'assets/world_props'
        result=convert(root);manifest=json.loads(result['manifest.json'])
        self.assertEqual(len(manifest['models']),44)
        for name,data in result.items():self.assertEqual(data,(root/'generated'/name).read_bytes(),name)
        selection=json.loads((root/'selection.json').read_text())
        self.assertEqual(selection['license'],'CC0-1.0')
        license_name='source/kenney/License.txt'
        self.assertIn(license_name,selection['files'])
        self.assertIn('Creative Commons Zero',(root/license_name).read_text())
        filing=next(m for m in manifest['models'] if m['id']=='filingCabinet')
        self.assertEqual(filing['triangles'],388)
        self.assertIn('3TD Studios',(root/'source/3td/License.txt').read_text(encoding='utf-8'))

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

    def office_fixture(self,mutate=None):
        shipped=Path(__file__).resolve().parents[1]/'assets/world_props'
        selection=json.loads((shipped/'selection.json').read_text())
        entry=next(m for m in selection['models'] if m['id']=='filingCabinet')
        names=[entry['source'],*entry['textures'].values()];files={}
        for name in names:
            data=(shipped/name).read_bytes()
            if name==entry['source'] and mutate:data=mutate(data)
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
            files[name]=hashlib.sha256(data).hexdigest()
        result=dict(schema=1,license='CC0-1.0',files=files,models=[entry]);self.write(result);return result

    def test_office_original_visual_node_excludes_exporter_collision_and_lights(self):
        self.office_fixture();result=convert(self.root);model=json.loads(result['manifest.json'])['models'][0]
        self.assertEqual(model['triangles'],388)
        self.assertGreater(model['dimensions'][2],1)
        self.assertEqual(result,convert(self.root))

    def test_office_exporter_absolute_texture_urls_are_never_followed(self):
        selection=self.office_fixture();selection['models'][0]['textures']={};self.write(selection)
        with self.assertRaisesRegex(ValueError,'Unmapped'):convert(self.root)
        selection=self.office_fixture();selection['models'][0]['textures']['_TD_DividerPaint_Dark_01_jpg']='../outside.jpg';self.write(selection)
        with self.assertRaisesRegex(ValueError,'escaping|Unlisted'):convert(self.root)

    def test_office_rejects_xml_entities_and_animation(self):
        self.office_fixture(lambda d:b'<!DOCTYPE COLLADA [<!ENTITY secret SYSTEM "file:///private">]>'+d)
        with self.assertRaisesRegex(ValueError,'XML'):convert(self.root)
        def animated(data):
            root=ET.fromstring(data);namespace='{http://www.collada.org/2005/11/COLLADASchema}'
            ET.SubElement(ET.SubElement(root,namespace+'library_animations'),namespace+'animation')
            return ET.tostring(root)
        self.office_fixture(animated)
        with self.assertRaisesRegex(ValueError,'animation'):convert(self.root)

    def test_office_rejects_unbounded_or_negative_indices_and_nonfinite_values(self):
        ns={'c':'http://www.collada.org/2005/11/COLLADASchema'}
        for mode in ['index','count','nan']:
            def mutate(data):
                root=ET.fromstring(data)
                if mode=='index':
                    indices=root.find('.//c:triangles/c:p',ns);indices.text='-1 '+' '.join(indices.text.split()[1:])
                elif mode=='count':root.find('.//c:triangles',ns).set('count','999999999')
                else:
                    array=root.find('.//c:float_array',ns);array.text='nan '+' '.join(array.text.split()[1:])
                return ET.tostring(root)
            with self.subTest(mode=mode):
                self.office_fixture(mutate)
                with self.assertRaises(ValueError):convert(self.root)


if __name__=='__main__':unittest.main()
