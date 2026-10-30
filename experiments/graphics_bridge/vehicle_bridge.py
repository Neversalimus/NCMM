"""Prepare optional vehicle snapshots at the two pinned Native versions."""
from pathlib import Path
from vehicle_interactions import prepare as prepare_interactions


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise ValueError('Native modular vehicle anchor differs: ' + before[:90])
    return text.replace(before, after, 1)


def patch_vehicle_header(text):
    if 'ncmm_graphics_vehicle_lifetime_' in text or 'ncmm_graphics_part_lifetime_' in text:
        raise ValueError('Native modular vehicle identity is already prepared')
    text = replace_once(text, 'struct vehicle_part {\n',
        '#include "ncmm_graphics_vehicle_identity.hpp"\n\nstruct vehicle_part {\n'
        '        ncmm_graphics_identity::vehicle_part_lifetime ncmm_graphics_part_lifetime_; // NOLINT(cata-serialize)\n'
        '        uint64_t ncmm_graphics_part_id() const noexcept { return ncmm_graphics_part_lifetime_.value(); }\n')
    return replace_once(text, '        vehicle( const vehicle & ) = delete;\n',
        '        vehicle( const vehicle & ) = delete;\n'
        '        uint64_t ncmm_graphics_vehicle_id() const noexcept { return ncmm_graphics_vehicle_lifetime_.value(); }\n'
        '        point_rel_ms ncmm_graphics_vehicle_pivot() const { return pivot_anchor[0]; }\n'
        '    private:\n'
        '        ncmm_graphics_identity::lifetime ncmm_graphics_vehicle_lifetime_; // NOLINT(cata-serialize)\n'
        '    public:\n')


def prepare(root, updates, bridge):
    root, bridge = Path(root), Path(bridge)
    header = updates.get('vehicle.h')
    if header is None:
        header = (root / 'src/vehicle.h').read_text(encoding='utf-8')
    updates['vehicle.h'] = patch_vehicle_header(header)
    loader = updates['ncmm_loader.cpp']
    loader = replace_once(loader, '#include "ncmm_graphics_presentation_policy.hpp"',
        '#include "ncmm_graphics_presentation_policy.hpp"\n#include "ncmm_graphics_vehicles.h"\n#if defined(TILES)\n#include "sdl_utils.h"\n#include "activity_actor_definitions.h"\n#endif')
    loader = replace_once(loader, '    "graphics.presentation.v1",',
        '    "graphics.presentation.v1",\n    "graphics.vehicles.v1",')
    loader = replace_once(loader, '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_PRESENTATION_ID ) {',
        '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_VEHICLES_ID ) {\n'
        '        if( min_major > 1 || min_minor != 0 ) return nullptr;\n'
        '        graphics_vehicles_requested = true; return &graphics_vehicles_api;\n'
        '    }\n    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_PRESENTATION_ID ) {')
    updates['ncmm_loader.cpp'] = loader
    prepare_interactions(root,updates)
    for name in ('ncmm_graphics_vehicles.h','ncmm_graphics_vehicle_identity.hpp','ncmm_graphics_vehicles.inc','ncmm_graphics_vehicle_policy.hpp','ncmm_graphics_vehicle_smoke.inc','ncmm_graphics_vehicle_qualification.inc'):
        if (root / 'src' / name).exists():
            raise ValueError('Experimental vehicle source already exists: ' + name)
        updates[name] = (bridge / name).read_text(encoding='utf-8')
