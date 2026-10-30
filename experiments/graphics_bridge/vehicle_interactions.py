"""Keep the real door activities/costs while selecting the displayed part."""


def once(text, before, after):
    if text.count(before) != 1:
        raise ValueError('Native vehicle activity anchor differs: ' + before[:80])
    return text.replace(before, after, 1)


def patch_definitions(text):
    if 'ncmm_exact_vehicle' in text:
        raise ValueError('Native vehicle activity identity is already prepared')
    for state in ('opened', 'closed'):
        anchor='        int '+state+'_vehicle_part = -1;\n'
        text=once(text,anchor,anchor+'        bool ncmm_exact_vehicle = false;\n'
                  '        uint64_t ncmm_vehicle_identity = 0, ncmm_part_identity = 0;\n')
    return text


def patch_actors(text):
    if '#include "ncmm_loader.h"' not in text:
        text=once(text,'#include "activity_actor_definitions.h"',
                  '#include "activity_actor_definitions.h"\n#include "ncmm_loader.h"')
    anchor='        opened_vehicle_part = veh->next_part_to_open( vp->part_index() );'
    text=once(text,anchor,
        '        const auto ncmm_selected = ncmm::graphics_vehicle_activity_part( *veh, openp, true, ncmm_vehicle_identity, ncmm_part_identity );\n'
        '        ncmm_exact_vehicle = ncmm_selected.has_value();\n'
        '        opened_vehicle_part = ncmm_selected ? *ncmm_selected : veh->next_part_to_open( vp->part_index() );')
    text=once(text,'            if( !outside ) {\n                open_success = open_tile_result::OPEN_VEHICLE_SINGLE;',
                  '            if( !outside || ncmm_exact_vehicle ) {\n                open_success = open_tile_result::OPEN_VEHICLE_SINGLE;')
    text=once(text,'        const std::string part_name = veh->part( opened_vehicle_part ).name();\n\n        switch( open_success )',
        '        if( ncmm_exact_vehicle ) {\n'
        '            opened_vehicle_part = ncmm::graphics_vehicle_activity_validate( *veh, ncmm_vehicle_identity, ncmm_part_identity, true );\n'
        '            if( opened_vehicle_part < 0 ) { act.set_to_null(); return; }\n'
        '        }\n        const std::string part_name = veh->part( opened_vehicle_part ).name();\n\n        switch( open_success )')
    text=once(text,'        closed_vehicle_part = veh->next_part_to_close( vpart,\n                              veh_pointer_or_null( here.veh_at( who.pos_bub() ) ) != veh );',
        '        const auto ncmm_selected = ncmm::graphics_vehicle_activity_part( *veh, closep, false, ncmm_vehicle_identity, ncmm_part_identity );\n'
        '        ncmm_exact_vehicle = ncmm_selected.has_value();\n'
        '        closed_vehicle_part = ncmm_selected ? *ncmm_selected : veh->next_part_to_close( vpart,\n'
        '                              veh_pointer_or_null( here.veh_at( who.pos_bub() ) ) != veh );')
    text=once(text,'        optional_vpart_position vp = here.veh_at( closep );\n        // There is a vehicle part here;',
        '        optional_vpart_position vp = here.veh_at( closep );\n'
        '        if( ncmm_exact_vehicle && !vp ) { act.set_to_null(); return; }\n        // There is a vehicle part here;')
    text=once(text,'        who.add_msg_if_player( _( "You close the %1$s\'s %2$s." ), veh->name,',
        '        if( ncmm_exact_vehicle ) {\n'
        '            closed_vehicle_part = ncmm::graphics_vehicle_activity_validate( *veh, ncmm_vehicle_identity, ncmm_part_identity, false );\n'
        '            if( closed_vehicle_part < 0 || !veh->can_close( closed_vehicle_part, who ) ) { act.set_to_null(); return; }\n'
        '        }\n        who.add_msg_if_player( _( "You close the %1$s\'s %2$s." ), veh->name,')
    for state in ('opened', 'closed'):
        anchor='    jsout.member( "'+state+'_vehicle_part", '+state+'_vehicle_part );'
        text=once(text,anchor,anchor+'\n    jsout.member( "ncmm_exact_vehicle", ncmm_exact_vehicle );')
        anchor='    data.read( "'+state+'_vehicle_part", actor.'+state+'_vehicle_part );'
        text=once(text,anchor,anchor+'\n    data.read( "ncmm_exact_vehicle", actor.ncmm_exact_vehicle );\n'
                  '    // Runtime part identities never survive a save/restart. An exact\n'
                  '    // pending activity cancels instead of targeting a recycled index.\n')
    return text


def prepare(root, updates):
    for name, patch in [('activity_actor_definitions.h',patch_definitions),('activity_actor.cpp',patch_actors)]:
        updates[name]=patch(updates.get(name) or (root/'src'/name).read_text(encoding='utf-8'))
    updates['ncmm_loader.h']=once(updates['ncmm_loader.h'],'class input_context;', 'class vehicle;\nclass input_context;')
    updates['ncmm_loader.h']=once(updates['ncmm_loader.h'],'void initialize();',
        'std::optional<int> graphics_vehicle_activity_part( vehicle &, const tripoint_bub_ms &, bool, uint64_t &, uint64_t & );\n'
        'int graphics_vehicle_activity_validate( vehicle &, uint64_t, uint64_t, bool );\nvoid initialize();')
    updates['ncmm_loader.cpp']=once(updates['ncmm_loader.cpp'],'} // namespace ncmm\n',
        '''std::optional<int> graphics_vehicle_activity_part( vehicle &v, const tripoint_bub_ms &at, bool opening, uint64_t &entity, uint64_t &part )
{
#if defined(TILES)
    return graphics_vehicle_activity_part_impl(v,at,opening,entity,part);
#else
    (void)v;(void)at;(void)opening;(void)entity;(void)part;return std::nullopt;
#endif
}
int graphics_vehicle_activity_validate( vehicle &v, uint64_t entity, uint64_t part, bool opening )
{
#if defined(TILES)
    return graphics_vehicle_activity_validate_impl(v,entity,part,opening);
#else
    (void)v;(void)entity;(void)part;(void)opening;return -1;
#endif
}
} // namespace ncmm
''')
