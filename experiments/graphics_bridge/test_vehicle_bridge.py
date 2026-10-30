import unittest
from vehicle_bridge import patch_vehicle_header


class VehicleHeaderTests(unittest.TestCase):
    source='struct vehicle_part {\n};\nclass vehicle {\n        vehicle( const vehicle & ) = delete;\n};\n'
    def test_stable_individual_identity_and_actual_current_pivot(self):
        prepared=patch_vehicle_header(self.source)
        self.assertEqual(prepared.count('ncmm_graphics_part_lifetime_;'),1)
        self.assertEqual(prepared.count('ncmm_graphics_vehicle_lifetime_;'),1)
        self.assertIn('return pivot_anchor[0];',prepared)
        self.assertIn('NOLINT(cata-serialize)',prepared)

    def test_reapplication_and_changed_upstream_anchors_fail_closed(self):
        with self.assertRaises(ValueError):patch_vehicle_header(patch_vehicle_header(self.source))
        with self.assertRaises(ValueError):patch_vehicle_header(self.source.replace('struct vehicle_part {','struct changed_part {'))
        with self.assertRaises(ValueError):patch_vehicle_header(self.source+self.source)


if __name__=='__main__':unittest.main()
