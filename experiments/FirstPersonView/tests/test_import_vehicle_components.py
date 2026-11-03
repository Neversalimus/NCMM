import importlib.util
import json
from pathlib import Path
import unittest
from PIL import Image

SPEC=importlib.util.spec_from_file_location('vehicle_import',Path(__file__).resolve().parents[1]/'tools/import_vehicle_components.py')
IMPORT=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(IMPORT)


class ComponentImportTests(unittest.TestCase):
    def test_pinned_outputs_are_reproducible_and_contain_only_two_components(self):
        outputs=IMPORT.produce()
        for name,body in outputs.items():self.assertEqual(body,(IMPORT.ROOT/'generated'/name).read_bytes())
        report=json.loads(outputs['manifest.json'])
        self.assertEqual([m['id'] for m in report['models']],['road_wheel','truck_wheel'])
        for model in report['models']:
            self.assertGreater(model['triangles'],100);self.assertLess(model['triangles'],2048)
            self.assertEqual(model['normalized_radius'],1);self.assertEqual(model['normalized_axle_width'],1)
            self.assertGreater(model['palette_colors'],1)

    def test_nonfinite_or_unbounded_coordinates_are_rejected(self):
        for value in ['nan','inf','-inf','1000000']:
            with self.assertRaises(ValueError):IMPORT.parse_obj('v '+value+' 0 0',None)

    def test_external_geometry_and_unowned_indices_are_rejected(self):
        for text in ['call ../other.obj','f 0/0/0 1/1/1 2/2/2','f -1/1/1 -2/2/2 -3/3/3',
                     'v 0 0 0\nvt 0 0\nvn 0 0 1\nf 1/1/1 1/1/1 999/1/1']:
            with self.assertRaises(ValueError):IMPORT.parse_obj(text,None)


if __name__=='__main__':unittest.main()
