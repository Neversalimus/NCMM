"""Retain only exact normal atlas pixels at the existing pinned Native upload."""

def once(text,old,new):
    if text.count(old)!=1:
        raise ValueError('Native atlas anchor differs: '+repr(old[:100]))
    return text.replace(old,new,1)

def prepare(header,source):
    header=once(header,
        '                                      const atlas_replay_quarantine::gate &abandon_gate );\n        void create_textures_from_tile_atlas',
        '                                      const atlas_replay_quarantine::gate &abandon_gate,\n                                      bool ncmm_retain_pixels = false );\n        void create_textures_from_tile_atlas')
    source=once(source,'#include "tileset_loader.h"','#include "tileset_loader.h"\n#include "ncmm_native_atlas_pixels.hpp"')
    source=once(source,
        '        const std::vector<SDL_Rect> &opaque_bounds,\n        const atlas_replay_quarantine::gate &abandon_gate )',
        '        const std::vector<SDL_Rect> &opaque_bounds,\n        const atlas_replay_quarantine::gate &abandon_gate, const bool ncmm_retain_pixels )')
    source=once(source,
        '    cata_assert( texture_ptr );\n    ++texture_count;',
        '    cata_assert( texture_ptr );\n    if( ncmm_retain_pixels ) ncmm_native_atlas::retain( texture_ptr.get(), surf.get() );\n    ++texture_count;')
    source=once(source,
        '            copy_surface_to_texture( scan_surf, offset, *tile_values, opaque_bounds, abandon_gate );',
        '            copy_surface_to_texture( scan_surf, offset, *tile_values, opaque_bounds, abandon_gate, tile_values == targets.normal );')
    return header,source
