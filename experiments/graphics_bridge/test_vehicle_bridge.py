import unittest
from vehicle_bridge import patch_vehicle_header
from vehicle_interactions import patch_actors,patch_definitions

# Independent minimal upstream anchors; no fixture is derived from patched text.
ACTOR_HEADER='        int opened_vehicle_part = -1;\n        int closed_vehicle_part = -1;\n'
ACTOR_SOURCE='''#include "activity_actor_definitions.h"
        opened_vehicle_part = veh->next_part_to_open( vp->part_index() );
            if( !outside ) {
                open_success = open_tile_result::OPEN_VEHICLE_SINGLE;
        const std::string part_name = veh->part( opened_vehicle_part ).name();

        switch( open_success )
        closed_vehicle_part = veh->next_part_to_close( vpart,
                              veh_pointer_or_null( here.veh_at( who.pos_bub() ) ) != veh );
        optional_vpart_position vp = here.veh_at( closep );
        // There is a vehicle part here;
        who.add_msg_if_player( _( "You close the %1$s's %2$s." ), veh->name,
    jsout.member( "opened_vehicle_part", opened_vehicle_part );
    data.read( "opened_vehicle_part", actor.opened_vehicle_part );
    jsout.member( "closed_vehicle_part", closed_vehicle_part );
    data.read( "closed_vehicle_part", actor.closed_vehicle_part );
'''


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

    def test_actual_activities_retain_native_behavior_and_cancel_stale_parts(self):
        out=patch_actors(ACTOR_SOURCE)
        self.assertIn('graphics_vehicle_activity_validate',out)
        self.assertIn('ncmm_exact_vehicle',out)
        self.assertIn('!veh->can_close',out)
        self.assertIn('!outside || ncmm_exact_vehicle',out)
        self.assertNotIn('mod_moves',out)
        self.assertNotIn('actor.ncmm_vehicle_identity',out)
        with self.assertRaises(ValueError):patch_actors(out)
        definitions=patch_definitions(ACTOR_HEADER)
        with self.assertRaises(ValueError):patch_definitions(definitions)

    def test_changed_native_activities_reject_partial_patches(self):
        with self.assertRaises(ValueError):patch_actors(ACTOR_SOURCE.replace('switch( open_success )','switch( changed )'))


if __name__=='__main__':unittest.main()
