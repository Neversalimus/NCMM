"""Surface contacts are reproducible and malformed calibration cannot pass."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from calibrate_grips import calibrate,closest
import test_import_held_items as fixtures


class GripCalibrationTests(unittest.TestCase):
    def test_triangle_interior_edge_corner_and_degenerate(self):
        triangle=((0,0,0),(1,0,0),(0,1,0))
        for point,expected in [((.2,.3,2),(.2,.3,0)),((2,0,1),(1,0,0)),((-.2,.3,0),(0,.3,0))]:
            for actual,wanted in zip(closest(point,*triangle),expected):self.assertAlmostEqual(actual,wanted)
        self.assertEqual(closest((.4,1,0),(0,0,0),(1,0,0),(1,0,0)),(.4,0,0))

    def test_calibration_is_on_source_and_idempotent(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=fixtures.glb_fixture(trigger=True);(root/'mesh.glb').write_bytes(source)
            profile=fixtures.HeldImportTests.profile(self);selection=dict(schema=1,files={'mesh.glb':hashlib.sha256(source).hexdigest()},models=[profile])
            path=root/'selection.json';path.write_text(json.dumps(selection));self.assertEqual(calibrate(path),1)
            first=path.read_bytes();self.assertEqual(calibrate(path,True),1);calibrate(path);self.assertEqual(first,path.read_bytes())
            valid=json.loads(path.read_text());self.assertEqual(valid['models'][0]['contacts']['trigger']['node'],'Trigger')
            for bad in [[],[0,0],[0,0,0,0],[float('nan'),0,0]]:
                changed=copy.deepcopy(valid);changed['models'][0]['contacts']['trigger']['world']=bad;path.write_text(json.dumps(changed))
                with self.assertRaises((ValueError,TypeError)):calibrate(path,True)


if __name__=='__main__':unittest.main()
