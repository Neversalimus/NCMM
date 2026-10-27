import hashlib
import json
import re
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import apply_graphics_bridge as bridge
from native_compiler_bridge import BEFORE as NATIVE_TOGGLE

REPO = Path(__file__).resolve().parents[2]
SDL_ANCHOR = """        tilecontext->draw(
            point( win->pos.x * fontwidth, win->pos.y * fontheight ),
            g->ter_view_p,
            TERRAIN_WINDOW_TERM_WIDTH * font->width,
            TERRAIN_WINDOW_TERM_HEIGHT * font->height,
            overlay_strings,
            color_blocks );"""

APPARENT_LIGHT_DECLARATIONS = """        struct apparent_light_info {
            bool obstructed;
            bool abs_obstructed;
            float apparent_light;
        };
        static apparent_light_info apparent_light_helper( const level_cache &map_cache,
                const tripoint_bub_ms &p );
        lit_level apparent_light_at( const tripoint_bub_ms &p,
                                    const visibility_variables &cache ) const;
"""


def apparent_light_original():
    # The primary-source oracle stays frozen when the extracted implementation
    # changes. Do not derive an upstream patch fixture from the changed inc.
    frozen = (REPO / "experiments/FirstPersonView/tests/multilevel_cache_test.cpp").read_text(encoding="utf-8")
    begin = "// BEGIN FROZEN NATIVE APPARENT LIGHT\n"
    end = "// END FROZEN NATIVE APPARENT LIGHT\n"
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native apparent-light fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_update_map_original():
    frozen = (REPO / "experiments/FirstPersonView/tests/trace_platform_test.cpp").read_text(encoding="utf-8")
    begin, end = "// BEGIN FROZEN NATIVE UPDATE MAP\n", "// END FROZEN NATIVE UPDATE MAP\n"
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native update_map fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_map_shift_original(pin):
    labels = dict(zip(bridge.SOURCES, ("1040", "1807")))
    frozen = (REPO / "experiments/FirstPersonView/tests/native_map_shift_fixture.cpp").read_text(encoding="utf-8")
    begin, end = (f"// {kind} FROZEN NATIVE MAP SHIFT {labels[pin]}\n" for kind in ("BEGIN", "END"))
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native map-shift fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_map_cache_original():
    frozen=(REPO/"experiments/FirstPersonView/tests/native_map_cache_primary.inc").read_text(encoding="utf-8")
    return frozen[frozen.index("void map::build_map_cache"):]


def native_phase_source(name):
    frozen = (REPO / "experiments/FirstPersonView/tests/native_phase_scan_fixture.cpp").read_text(encoding="utf-8")
    begin, end = (f"// {kind} FROZEN NATIVE PHASE SOURCE {name}\n" for kind in ("BEGIN", "END"))
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native phase source differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_phase_query():
    frozen = (REPO / "experiments/FirstPersonView/tests/native_phase_scan_fixture.cpp").read_text(encoding="utf-8")
    return frozen.split("// BEGIN FROZEN NATIVE ORIGINAL TERRAIN QUERY\n", 1)[1].split("// END FROZEN NATIVE ORIGINAL TERRAIN QUERY\n", 1)[0]


def native_actualize_original():
    frozen = (REPO / "experiments/FirstPersonView/tests/native_actualize_primary.inc").read_text(encoding="utf-8")
    begin, end = "// BEGIN FROZEN NATIVE ACTUALIZE\n", "// END FROZEN NATIVE ACTUALIZE\n"
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native actualize fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_actualize_prefixes():
    frozen = (REPO / "experiments/FirstPersonView/tests/native_actualize_primary.inc").read_text(encoding="utf-8")
    result = {}
    for name in bridge.NATIVE_ACTUALIZE_PREFIXES:
        begin = f"// BEGIN FROZEN NATIVE ACTUALIZE PREFIX {name}\n"
        end = f"// END FROZEN NATIVE ACTUALIZE PREFIX {name}\n"
        block = frozen.split(begin, 1)[1].split(end, 1)[0]
        result[name] = block.split('R"NCMM(', 1)[1].split(')NCMM";', 1)[0]
    return result


def native_floor_cache_original():
    frozen = (REPO / "experiments/FirstPersonView/tests/native_floor_cache_fixture.cpp").read_text(encoding="utf-8")
    begin, end = "// BEGIN FROZEN NATIVE FLOOR CACHE\n", "// END FROZEN NATIVE FLOOR CACHE\n"
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native floor-cache fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_vision_transparency_original():
    frozen = (REPO / "experiments/FirstPersonView/tests/native_vision_transparency_fixture.cpp").read_text(encoding="utf-8")
    begin, end = "// BEGIN FROZEN NATIVE VISION TRANSPARENCY\n", "// END FROZEN NATIVE VISION TRANSPARENCY\n"
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native vision-transparency fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_outside_cache_original():
    frozen = (REPO / "experiments/FirstPersonView/tests/native_outside_cache_fixture.cpp").read_text(encoding="utf-8")
    begin, end = "// BEGIN FROZEN NATIVE OUTSIDE CACHE\n", "// END FROZEN NATIVE OUTSIDE CACHE\n"
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native outside-cache fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_item_reconciliation_original(pin):
    labels = dict(zip(bridge.SOURCES, ("1040", "1807")))
    frozen = (REPO / "experiments/FirstPersonView/tests/native_item_reconciliation_fixture.cpp").read_text(encoding="utf-8")
    begin, end = (f"// {kind} FROZEN NATIVE RECONCILE {labels[pin]}\n" for kind in ("BEGIN", "END"))
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native item-reconciliation fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_shadowcast_originals():
    directory = REPO / "experiments/FirstPersonView/tests"
    body = (directory / "native_shadowcasting_primary.inc").read_text(encoding="utf-8").split("// BEGIN FROZEN NATIVE SHADOWCAST BODY\n", 1)[1]
    adapter = ('#if !defined(NCMM_SHADOWCAST_REFERENCE)\n'
        '    ncmm_graphics_shadowcasting::calc_cache<T, calc, ncmm_shadow_fixture::counted_exp> ncmm_calc;\n#endif\n')
    original = '                    last_intensity = calc( numerator, this_span->cumulative_value, dist );\n'
    call = '#if defined(NCMM_SHADOWCAST_REFERENCE)\n' + original + '#else\n                    last_intensity = ncmm_calc( numerator, this_span->cumulative_value, dist );\n#endif\n'
    if body.count(adapter) != 2 or body.count(call) != 2:
        raise ValueError("Frozen Native shadowcast adapters differ")
    prefix = ('#include "shadowcasting.h"\n\n#include <cstdint>\n#include <cstdlib>\n#include <iterator>\n\n'
        '#include "coordinates.h"\n#include "cuboid_rectangle.h"\n#include "fragment_cloud.h" // IWYU pragma: keep\n'
        '#include "line.h"\n#include "list.h"\n#include "point.h"\n\n')
    cpp = prefix + body.replace(adapter, '').replace(call, original)
    header = (directory / "native_shadowcasting_math.inc").read_text(encoding="utf-8").split("// BEGIN FROZEN NATIVE SHADOWCAST HEADER\n", 1)[1]
    includes = '#include "coords_fwd.h"\n#include "lightmap.h"\n#include "map_scale_constants.h"\n#include "mdarray.h"\n'
    header = header.replace('#if !defined(NCMM_SHADOWCAST_MODEL)\n' + includes + '#endif\n', includes, 1)
    header = header.replace('ncmm_shadow_fixture::evaluate( transparency * distance )', 'std::exp( transparency * distance )', 1)
    return cpp, header


class ApplyTests(unittest.TestCase):
    def test_shadowcast_complete_primary_and_exact_square_dispatch(self):
        cpp, header = native_shadowcast_originals()
        # Independent pinned digests, not computed from the changed helper.
        self.assertEqual(hashlib.sha256(cpp.encode()).hexdigest(), 'fbf4e65de6c85af60267e5b73a0d2d4d933a0dd24fe4752a9685281d32fa59c9')
        self.assertEqual(hashlib.sha256(header.encode()).hexdigest(), '9f5eea643f64c436fcbb202e8b966d53bc54a9ce45392447ceebf17cc1ad0af8')
        for pin in bridge.SOURCES:
            actual = bridge.patch_native_shadowcasting(cpp, header, pin)
            self.assertNotIn('ncmm_calc', actual)
            restored = actual.replace('#include <type_traits>\n', '', 1)
            self.assertTrue(restored.endswith('\n#include "ncmm_graphics_sight_reference.inc"\n'))
            restored = restored.removesuffix('\n#include "ncmm_graphics_sight_reference.inc"\n')
            self.assertEqual(restored.count('template<bool NCMM_SQUARE, int '), 2)
            restored = restored.replace('template<bool NCMM_SQUARE, int ', 'template<int ')
            restored = restored.replace('template<bool NCMM_SQUARE, typename T,', 'template<typename T,', 1)
            restored = restored.replace('void ncmm_cast_zlight_implementation(\n', 'void cast_zlight(\n', 1)
            self.assertEqual(restored.count('< NCMM_SQUARE, '), 24)
            restored = restored.replace('< NCMM_SQUARE, ', '< ')
            distance='( NCMM_SQUARE ? distance : rl_dist( tripoint_rel_ms::zero, delta ) )'
            self.assertEqual(restored.count(distance), 2)
            restored = restored.replace(distance, 'rl_dist( tripoint_rel_ms::zero, delta )')
            restored = restored.replace('#include "ncmm_graphics_sight_dispatch.inc"\n\n', '', 1)
            self.assertEqual(restored, cpp)
        with self.assertRaisesRegex(ValueError, 'shadowcasting'):
            bridge.patch_native_shadowcasting(cpp, header, 'unknown')

    def test_distance_model_compiles_the_actual_emitted_native_body(self):
        cpp, header = native_shadowcast_originals()
        directory=REPO/'experiments/FirstPersonView/tests'
        frozen=(directory/'native_shadowcasting_distance.inc').read_text(encoding='utf-8')
        frozen=frozen.split('// BEGIN EMITTED NATIVE DISTANCE BODY\n',1)[1]
        for pin in bridge.SOURCES:
            actual=bridge.patch_native_shadowcasting(cpp,header,pin)
            actual=actual[actual.index('// historically 8 bits is enough'):]
            actual=actual.removesuffix('\n#include "ncmm_graphics_sight_reference.inc"\n')
            self.assertEqual(actual.count('( NCMM_SQUARE ? distance : rl_dist('),2)
            actual=actual.replace('( NCMM_SQUARE ? distance : rl_dist(',
                '( NCMM_SQUARE ? ncmm_test_row_distance( distance ) : rl_dist(')
            self.assertEqual(actual,frozen)
        numerical=(directory/'native_distance_primary.inc').read_text(encoding='utf-8')
        numerical=numerical[numerical.index('inline float trig_dist'):]
        self.assertEqual(hashlib.sha256(numerical.encode()).hexdigest(),
                         '914d0b9f8f3c979ada713e8a603fededf54cf83dff86d87402bf0007fa50864c')

    def test_shadowcast_real_container_and_fragment_primary_identity(self):
        directory = REPO / 'experiments/FirstPersonView/tests'
        for name, digest in (
                ('third_party/native_plf_list.h', 'af8a455aea61d8f4c20395ae378679bf4d21ee5f745c76aae97dd31b6f08e4c2'),
                ('third_party/native_plf_list.LICENSE', 'a33cffd26f76937bb96370c2c97f188fdca3aad933e1032f399f96644769a63c')):
            self.assertEqual(hashlib.sha256((directory/name).read_bytes()).hexdigest(), digest)
        fragment = (directory/'native_shadowcasting_fragment.hpp').read_text(encoding='utf-8')
        fragment = fragment[fragment.index('#pragma once'):]
        self.assertEqual(hashlib.sha256(fragment.encode()).hexdigest(), '36e3758aa305c5df73edd3db83d92c796d0b59cbe6cee4545c6af74a9ad60074')

    def test_shadowcast_both_pins_copy_receipt_and_unchanged_header(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp); self.fixture(root,pin)
                header=(root/'src/shadowcasting.h').read_bytes()
                with patch.object(bridge.subprocess,'check_output',return_value=pin): bridge.apply(root)
                self.assertEqual((root/'src/shadowcasting.h').read_bytes(),header)
                receipt=json.loads((root/'first-person-source.json').read_text(encoding='utf-8'))
                for name in ('ncmm_graphics_sight_dispatch.inc','ncmm_graphics_sight_reference.hpp',
                             'ncmm_graphics_sight_reference.inc','ncmm_graphics_sight_comparison.inc'):
                    data=Path(bridge.__file__).with_name(name).read_bytes()
                    self.assertEqual((root/'src'/name).read_bytes(),data)
                    self.assertEqual(receipt['inputs'][name],hashlib.sha256(data).hexdigest())
                self.assertEqual(receipt['inputs']['shadowcasting.cpp'],hashlib.sha256((root/'src/shadowcasting.cpp').read_bytes()).hexdigest())

    def test_shadowcast_changed_missing_or_duplicate_sources_are_atomic(self):
        for pin in bridge.SOURCES:
            for name in ('shadowcasting.cpp','shadowcasting.h'):
                for kind in ('changed','missing','duplicate'):
                    with self.subTest(pin=pin,name=name,kind=kind), tempfile.TemporaryDirectory() as tmp:
                        root=Path(tmp); self.fixture(root,pin); path=root/'src'/name
                        data=path.read_text(encoding='utf-8')
                        path.write_text({'changed':data+'// drift\n','missing':'','duplicate':data+data}[kind],encoding='utf-8')
                        before=self.fingerprint(root)
                        with patch.object(bridge.subprocess,'check_output',return_value=pin):
                            with self.assertRaisesRegex(ValueError,'shadowcasting'): bridge.apply(root)
                        self.assertEqual(before,self.fingerprint(root))

    def test_sight_comparison_missing_or_existing_helpers_are_atomic(self):
        for name in ('ncmm_graphics_sight_reference.hpp','ncmm_graphics_sight_reference.inc',
                     'ncmm_graphics_sight_comparison.inc'):
            for pin in bridge.SOURCES:
                for existing in (False,True):
                    with self.subTest(name=name,pin=pin,existing=existing), tempfile.TemporaryDirectory() as tmp:
                        root=Path(tmp); self.fixture(root,pin)
                        if existing: (root/'src'/name).write_text('existing helper\n',encoding='utf-8')
                        before=self.fingerprint(root); original_read=Path.read_text
                        def read(path,*args,**kwargs):
                            if not existing and path==Path(bridge.__file__).with_name(name):
                                raise FileNotFoundError('missing comparison include')
                            return original_read(path,*args,**kwargs)
                        with patch.object(bridge.subprocess,'check_output',return_value=pin), patch.object(Path,'read_text',read):
                            with self.assertRaises((ValueError,FileNotFoundError)): bridge.apply(root)
                        self.assertEqual(before,self.fingerprint(root))

    def test_shadowcast_missing_or_existing_include_is_atomic(self):
        name='ncmm_graphics_sight_dispatch.inc'
        for pin in bridge.SOURCES:
            for existing in (False,True):
                with self.subTest(pin=pin,existing=existing), tempfile.TemporaryDirectory() as tmp:
                    root=Path(tmp); self.fixture(root,pin)
                    if existing: (root/'src'/name).write_text('existing include\n',encoding='utf-8')
                    before=self.fingerprint(root); original_read=Path.read_text
                    def read(path,*args,**kwargs):
                        if not existing and path==Path(bridge.__file__).with_name(name): raise FileNotFoundError('missing sight cache include')
                        return original_read(path,*args,**kwargs)
                    with patch.object(bridge.subprocess,'check_output',return_value=pin), patch.object(Path,'read_text',read):
                        with self.assertRaises((ValueError,FileNotFoundError)): bridge.apply(root)
                    self.assertEqual(before,self.fingerprint(root))

    def test_map_cache_primary_and_adjacent_probe_positions(self):
        original=native_map_cache_original()
        self.assertEqual((len(original.encode()),hashlib.sha256(original.encode()).hexdigest()),bridge.NATIVE_MAP_CACHE_ORIGINAL)
        source=Path(bridge.__file__).with_name("ncmm_graphics_map_cache.inc").read_text(encoding="utf-8")
        source=source[source.index("void map::build_map_cache"):]
        restored=''.join(line for line in source.splitlines(keepends=True) if 'ncmm_cache_scope' not in line)
        self.assertEqual(restored,original)
        self.assertTrue(source.startswith('void map::build_map_cache( const int zlev, bool skip_lightmap )\n{\n    ncmm_graphics_move_diagnostics::cache_scope'))
        self.assertIn('    bool camera_cache_dirty = false;\n    if( ncmm_cache_scope ) ncmm_cache_scope.bounds( minz, maxz );\n',source)
        self.assertIn('cache_boundary::vision );\n\n    if( seen_cache_dirty ) {\n        skew_vision_cache.clear();',source)
        self.assertIn('    seen_cache_dirty |= player_prev_pos != p || sr != player_prev_range || camera_cache_dirty;\n    if( ncmm_cache_scope ) ncmm_cache_scope.mark( ncmm_graphics_move_diagnostics::cache_boundary::moncam_state );\n',source)
        before={
            'structure':'    // needs a separate pass',
            'vehicles':'    for( int z = minz; z <= maxz; z++ ) {\n        seen_cache_dirty |= build_vision_transparency_cache',
            'skew':'    avatar &u = get_avatar();',
            'seen':'    if( camera_cache_dirty ) {\n        u.moncam_cache = mcache;',
            'moncam_seen':'    // Detect character/NPC light changes reactively.',
            'character_lights':'    if( !skip_lightmap ) {',
        }
        for name,anchor in before.items():
            mark=f'    if( ncmm_cache_scope ) ncmm_cache_scope.mark( ncmm_graphics_move_diagnostics::cache_boundary::{name} );\n'
            self.assertIn(mark+anchor,source)
        self.assertIn('    }\n    if( ncmm_cache_scope ) ncmm_cache_scope.mark( ncmm_graphics_move_diagnostics::cache_boundary::lightmap );\n}',source)

    def test_map_cache_patch_preserves_surrounding_native(self):
        for pin in bridge.SOURCES:
            source='// prefix\n'+native_map_cache_original()+'// suffix\n'
            self.assertEqual(bridge.patch_native_map_cache(source,pin),'// prefix\n#include "ncmm_graphics_map_cache.inc"\n// suffix\n')

    def test_map_cache_both_pins_copy_and_receipt(self):
        name='ncmm_graphics_map_cache.inc'
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin),tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);self.fixture(root,pin)
                with patch.object(bridge.subprocess,'check_output',return_value=pin): bridge.apply(root)
                self.assertEqual((root/'src/map.cpp').read_text(encoding='utf-8').count('#include "ncmm_graphics_map_cache.inc"'),1)
                data=Path(bridge.__file__).with_name(name).read_bytes()
                self.assertEqual((root/'src'/name).read_bytes(),data)
                receipt=json.loads((root/'first-person-source.json').read_text(encoding='utf-8'))
                self.assertEqual(receipt['inputs'][name],hashlib.sha256(data).hexdigest())

    def test_map_cache_changed_missing_and_duplicate_body_are_atomic(self):
        original=native_map_cache_original()
        for pin in bridge.SOURCES:
            for kind in ('changed','missing','duplicate'):
                with self.subTest(pin=pin,kind=kind),tempfile.TemporaryDirectory() as tmp:
                    root=Path(tmp);self.fixture(root,pin);path=root/'src/map.cpp'
                    data=path.read_text(encoding='utf-8')
                    replacement={'changed':original.replace('generate_lightmap( zlev );','different_lightmap( zlev );'),'missing':'','duplicate':original+original}[kind]
                    path.write_text(data.replace(original,replacement,1),encoding='utf-8')
                    before=self.fingerprint(root)
                    with patch.object(bridge.subprocess,'check_output',return_value=pin):
                        with self.assertRaisesRegex(ValueError,'map-cache'):bridge.apply(root)
                    self.assertEqual(before,self.fingerprint(root))

    def test_map_cache_missing_or_existing_include_is_atomic(self):
        name='ncmm_graphics_map_cache.inc'
        for pin in bridge.SOURCES:
            for existing in (False,True):
                with self.subTest(pin=pin,existing=existing),tempfile.TemporaryDirectory() as tmp:
                    root=Path(tmp);self.fixture(root,pin)
                    if existing:(root/'src'/name).write_text('existing include\n',encoding='utf-8')
                    before=self.fingerprint(root);original_read=Path.read_text
                    def read(path,*args,**kwargs):
                        if not existing and path==Path(bridge.__file__).with_name(name):raise FileNotFoundError('missing include')
                        return original_read(path,*args,**kwargs)
                    with patch.object(bridge.subprocess,'check_output',return_value=pin),patch.object(Path,'read_text',read):
                        with self.assertRaises((ValueError,FileNotFoundError)):bridge.apply(root)
                    self.assertEqual(before,self.fingerprint(root))

    def test_phase_primary_and_source_preservation(self):
        for name, expected in (("loadn", bridge.NATIVE_PHASE_LOADN_ORIGINAL),
                               ("phase_function", bridge.NATIVE_TEMPERATURE_PHASE_ORIGINAL)):
            data = native_phase_source(name)
            self.assertEqual((len(data.encode()), hashlib.sha256(data.encode()).hexdigest()), expected)
        anchor = native_phase_source("submap_anchor")
        self.assertEqual(anchor, bridge.NATIVE_SUBMAP_PHASE_ANCHOR)
        self.assertEqual((len(anchor.encode()), hashlib.sha256(anchor.encode()).hexdigest()),
                         (394, "f4b432b46faa03b2e3026798f518a0a9c27ffe28891737a4fc48fbeefab53247"))
        query = native_phase_query()
        self.assertEqual((len(query.encode()), hashlib.sha256(query.encode()).hexdigest()), bridge.NATIVE_SUBMAP_ORIGINAL_QUERY)
        source = Path(bridge.__file__).with_name("ncmm_graphics_phase_loop.inc").read_text(encoding="utf-8")
        begin, end = "        // BEGIN PROVEN UNIFORM PHASE NOOP\n", "        // END PROVEN UNIFORM PHASE NOOP\n"
        prefix, rest = source.split(begin, 1)
        self.assertTrue(prefix.startswith("// Adapted from Cataclysm:"))
        self.assertEqual(rest.split(end, 1)[1], bridge.NATIVE_PHASE_LOOP)
        frozen = (REPO / "experiments/FirstPersonView/tests/native_phase_scan_fixture.cpp").read_text(encoding="utf-8")
        loop = frozen.split("// BEGIN FROZEN NATIVE PHASE LOOP\n",1)[1].split("// END FROZEN NATIVE PHASE LOOP\n",1)[0]
        self.assertEqual(loop, bridge.NATIVE_PHASE_LOOP)
        compiled = frozen.split("// BEGIN COMPILED NATIVE PHASE PREFIX\n",1)[1].split("// END COMPILED NATIVE PHASE PREFIX\n",1)[0]
        compiled = compiled.replace("map::phase_inner(", "map::temp_based_phase_change_at(", 1)
        self.assertEqual(compiled, native_phase_source("phase_function").split("    units::temperature standard = 0_K;",1)[0])

    def test_phase_patch_retains_complete_native_load_and_metadata(self):
        original = native_phase_source("loadn")
        header = "// prefix\n" + native_phase_source("submap_anchor") + "// suffix\n"
        for pin in bridge.SOURCES:
            changed = bridge.patch_native_phase_scan("// prefix\n"+original+"// suffix\n", native_phase_source("phase_function"), pin)
            self.assertEqual(changed.replace('#include "ncmm_graphics_phase_loop.inc"\n', bridge.NATIVE_PHASE_LOOP, 1),
                             "// prefix\n"+original+"// suffix\n")
            updated = bridge.patch_native_submap_phase(header,native_phase_query(),pin)
            self.assertEqual(updated.replace('#include "ncmm_graphics_submap_phase.inc"\n','',1),header)

    def test_phase_both_pins_copy_and_receipt(self):
        names = ("ncmm_graphics_phase_loop.inc", "ncmm_graphics_submap_phase.inc")
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp); self.fixture(root,pin)
                with patch.object(bridge.subprocess,"check_output",return_value=pin): bridge.apply(root)
                self.assertEqual((root/"src/map.cpp").read_text(encoding="utf-8").count('#include "ncmm_graphics_phase_loop.inc"'),1)
                self.assertEqual((root/"src/submap.h").read_text(encoding="utf-8").count('#include "ncmm_graphics_submap_phase.inc"'),1)
                receipt=json.loads((root/"first-person-source.json").read_text(encoding="utf-8"))
                for name in names:
                    data=Path(bridge.__file__).with_name(name).read_bytes()
                    self.assertEqual((root/"src"/name).read_bytes(),data)
                    self.assertEqual(receipt["inputs"][name],hashlib.sha256(data).hexdigest())
                header=(root/"src/submap.h").read_bytes()
                self.assertEqual(receipt["inputs"]["submap.h"],hashlib.sha256(header).hexdigest())

    def test_phase_changed_missing_and_duplicate_sources_are_atomic(self):
        specs=(("map.cpp",native_phase_source("loadn")),("mapgen.cpp",native_phase_source("phase_function")),
               ("submap.h",native_phase_source("submap_anchor")),("submap.cpp",native_phase_query()))
        for pin in bridge.SOURCES:
            for name, original in specs:
                for kind in ("changed","missing","duplicate"):
                    with self.subTest(pin=pin,name=name,kind=kind), tempfile.TemporaryDirectory() as tmp:
                        root=Path(tmp); self.fixture(root,pin); path=root/"src"/name
                        data=path.read_text(encoding="utf-8")
                        changed=original.replace("original_terrain", "different_terrain",1) if "original_terrain" in original else original.replace("bool map_incomplete = false;", "bool map_incomplete = true;",1)
                        replacement={"changed":changed,"missing":"","duplicate":original+original}[kind]
                        self.assertNotEqual(replacement,original)
                        path.write_text(data.replace(original,replacement,1),encoding="utf-8")
                        before=self.fingerprint(root)
                        with patch.object(bridge.subprocess,"check_output",return_value=pin):
                            with self.assertRaisesRegex(ValueError,"phase-scan|cache reset"): bridge.apply(root)
                        self.assertEqual(before,self.fingerprint(root))

    def test_phase_metadata_name_collision_is_atomic(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);self.fixture(root,pin);path=root/"src/submap.h"
                with path.open('a',encoding='utf-8') as f: f.write('\nvoid ncmm_graphics_has_original_terrain();\n')
                before=self.fingerprint(root)
                with patch.object(bridge.subprocess,"check_output",return_value=pin):
                    with self.assertRaisesRegex(ValueError,"phase-scan metadata"): bridge.apply(root)
                self.assertEqual(before,self.fingerprint(root))

    def test_phase_missing_or_existing_includes_are_atomic(self):
        for pin in bridge.SOURCES:
            for name in ("ncmm_graphics_phase_loop.inc","ncmm_graphics_submap_phase.inc"):
                for existing in (False,True):
                    with self.subTest(pin=pin,name=name,existing=existing), tempfile.TemporaryDirectory() as tmp:
                        root=Path(tmp);self.fixture(root,pin)
                        if existing: (root/"src"/name).write_text("existing include\n",encoding="utf-8")
                        before=self.fingerprint(root);original_read=Path.read_text
                        def read(path,*args,**kwargs):
                            if not existing and path==Path(bridge.__file__).with_name(name): raise FileNotFoundError("missing include")
                            return original_read(path,*args,**kwargs)
                        with patch.object(bridge.subprocess,"check_output",return_value=pin),patch.object(Path,"read_text",read):
                            with self.assertRaises((ValueError,FileNotFoundError)): bridge.apply(root)
                        self.assertEqual(before,self.fingerprint(root))

    def test_actualize_primary_and_source_preserving_fast_path(self):
        original = native_actualize_original()
        self.assertEqual((len(original.encode()), hashlib.sha256(original.encode()).hexdigest()),
                         bridge.NATIVE_ACTUALIZE_ORIGINAL)
        source = Path(bridge.__file__).with_name("ncmm_graphics_actualize.inc").read_text(encoding="utf-8")
        source = source[source.index("void map::actualize"):]
        begin, end = "    // BEGIN PROVEN UNIFORM UNDERGROUND NOOP\n", "    // END PROVEN UNIFORM UNDERGROUND NOOP\n"
        self.assertEqual(source.count(begin), 1); self.assertEqual(source.count(end), 1)
        before, rest = source.split(begin, 1)
        self.assertEqual(before + rest.split(end, 1)[1], original)
        for name, prefix in native_actualize_prefixes().items():
            self.assertEqual((len(prefix.encode()), hashlib.sha256(prefix.encode()).hexdigest()),
                             bridge.NATIVE_ACTUALIZE_PREFIXES[name])

    def test_actualize_patch_retains_surrounding_source_and_helpers(self):
        original = native_actualize_original()
        helpers = ''.join(native_actualize_prefixes().values())
        for pin in bridge.SOURCES:
            source = "// prefix\n" + helpers + original + "// suffix\n"
            self.assertEqual(bridge.patch_native_actualize(source, pin),
                             "// prefix\n" + helpers + '#include "ncmm_graphics_actualize.inc"\n// suffix\n')
        with self.assertRaisesRegex(ValueError, "actualize"):
            bridge.patch_native_actualize(source, "unknown")

    def test_actualize_both_pins_copy_and_receipt(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_actualize.inc")
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, "check_output", return_value=pin): bridge.apply(root)
                source = (root / "src/map.cpp").read_text(encoding="utf-8")
                self.assertEqual(source.count('#include "ncmm_graphics_actualize.inc"'), 1)
                self.assertNotIn("void map::actualize(", source)
                self.assertEqual((root / "src" / helper.name).read_bytes(), helper.read_bytes())
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                self.assertEqual(receipt["inputs"][helper.name], hashlib.sha256(helper.read_bytes()).hexdigest())

    def test_actualize_changed_missing_and_duplicate_primary_are_atomic(self):
        original = native_actualize_original()
        for pin in bridge.SOURCES:
            for kind in ("changed", "missing", "duplicate"):
                with self.subTest(pin=pin, kind=kind), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin); path = root / "src/map.cpp"
                    source = path.read_text(encoding="utf-8")
                    replacement = {"changed": original.replace("grid.z() >= 0", "grid.z() > 0"),
                                   "missing": "", "duplicate": original + original}[kind]
                    path.write_text(source.replace(original, replacement, 1), encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "actualize"): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_actualize_changed_missing_and_duplicate_helper_prefixes_are_atomic(self):
        for pin in bridge.SOURCES:
            for name, prefix in native_actualize_prefixes().items():
                for kind in ("changed", "missing", "duplicate"):
                    with self.subTest(pin=pin, name=name, kind=kind), tempfile.TemporaryDirectory() as tmp:
                        root = Path(tmp); self.fixture(root, pin); path = root / "src/map.cpp"
                        source = path.read_text(encoding="utf-8")
                        replacement = {"changed": prefix.replace("\n{\n", "\n{\n    native_effect();\n", 1),
                                       "missing": "", "duplicate": prefix + prefix}[kind]
                        path.write_text(source.replace(prefix, replacement, 1), encoding="utf-8")
                        before = self.fingerprint(root)
                        with patch.object(bridge.subprocess, "check_output", return_value=pin):
                            with self.assertRaisesRegex(ValueError, "actualize helper"): bridge.apply(root)
                        self.assertEqual(before, self.fingerprint(root))

    def test_actualize_missing_or_existing_include_is_atomic(self):
        name = "ncmm_graphics_actualize.inc"
        for pin in bridge.SOURCES:
            for existing in (False, True):
                with self.subTest(pin=pin, existing=existing), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin)
                    if existing: (root / "src" / name).write_text("existing include\n", encoding="utf-8")
                    before = self.fingerprint(root); original_read = Path.read_text
                    def read(path, *args, **kwargs):
                        if not existing and path == Path(bridge.__file__).with_name(name):
                            raise FileNotFoundError("missing actualize include")
                        return original_read(path, *args, **kwargs)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                        with self.assertRaises((ValueError, FileNotFoundError)): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_map_shift_full_primary_and_source_preserving_probes(self):
        source = Path(bridge.__file__).with_name("ncmm_graphics_map_shift.inc").read_text(encoding="utf-8")
        source = source[source.index("void map::shift"):]
        hooks = [line for line in source.splitlines(keepends=True)
                 if line.startswith("    ncmm_graphics_move_diagnostics::shift_scope ")
                 or line.startswith("    if( ncmm_shift_scope ) ")]
        self.assertEqual(len(hooks), 12)
        for line in hooks:
            self.assertEqual(source.count(line), 1)
            source = source.replace(line, "", 1)
        begin = "        // BEGIN REUSED NATIVE SUPPORT NODES\n"
        end = "        // END REUSED NATIVE SUPPORT NODES\n"
        self.assertEqual(source.count(begin), 1); self.assertEqual(source.count(end), 1)
        old = ("        std::set<tripoint_bub_ms> old_cache = std::move( support_cache_dirty );\n"
               "        support_cache_dirty.clear();\n"
               "        for( const tripoint_bub_ms &pt : old_cache ) {\n"
               "            support_cache_dirty.insert( pt + point( -sp.x() * SEEX, -sp.y() * SEEY ) );\n"
               "        }\n")
        prefix, rest = source.split(begin, 1)
        source = prefix + old + rest.split(end, 1)[1]
        binding = "            NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP(cache)\n"
        self.assertEqual(source.count(binding), 1)
        for pin, (length, digest, sweep) in bridge.NATIVE_MAP_SHIFT_ORIGINALS.items():
            original = native_map_shift_original(pin)
            self.assertEqual(len(original.encode("utf-8")), length)
            self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), digest)
            restored = source.replace(binding, "            cache->map_memory_sweep_pending = true;\n" if sweep else "", 1)
            self.assertEqual(restored, original)

    def test_map_shift_patch_retains_surrounding_native_source_and_pin(self):
        for pin, (_, _, sweep) in bridge.NATIVE_MAP_SHIFT_ORIGINALS.items():
            original = native_map_shift_original(pin)
            definition = "#define NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP(c)" + (" " + sweep if sweep else "")
            expected = ("// prefix\n" + definition + "\n"
                        '#include "ncmm_graphics_map_shift.inc"\n#undef NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP\n// suffix\n')
            self.assertEqual(bridge.patch_native_map_shift("// prefix\n"+original+"// suffix\n", pin), expected)
        with self.assertRaisesRegex(ValueError, "map-shift"):
            bridge.patch_native_map_shift(original, "unknown")

    def test_map_shift_both_pins_receipt_and_global_tls(self):
        name = "ncmm_graphics_map_shift.inc"
        helper = Path(bridge.__file__).with_name(name)
        for pin, (_, _, sweep) in bridge.NATIVE_MAP_SHIFT_ORIGINALS.items():
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, "check_output", return_value=pin): bridge.apply(root)
                source = (root / "src/map.cpp").read_text(encoding="utf-8")
                definition = "#define NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP(c)" + (" " + sweep if sweep else "")
                binding = (definition + "\n"
                           '#include "ncmm_graphics_map_shift.inc"\n#undef NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP\n')
                self.assertEqual(source.count(binding), 1)
                self.assertNotIn("void map::shift", source)
                self.assertEqual(source.count('#include "ncmm_graphics_move_diagnostics.hpp"'), 1)
                self.assertLess(source.index('#include "ncmm_graphics_move_diagnostics.hpp"'), source.index(binding))
                namespace = re.search(r"^namespace\b", source, flags=re.M)
                if namespace: self.assertLess(source.index('#include "ncmm_graphics_move_diagnostics.hpp"'), namespace.start())
                self.assertEqual((root / "src" / name).read_bytes(), helper.read_bytes())
                support_name = "ncmm_graphics_support_rebase.hpp"
                self.assertEqual(source.count(f'#include "{support_name}"'), 1)
                support = Path(bridge.__file__).with_name(support_name)
                self.assertEqual((root / "src" / support_name).read_bytes(), support.read_bytes())
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                self.assertEqual(receipt["inputs"][name], hashlib.sha256(helper.read_bytes()).hexdigest())
                self.assertEqual(receipt["inputs"][support_name], hashlib.sha256(support.read_bytes()).hexdigest())

    def test_map_shift_changed_missing_and_duplicate_primary_are_atomic(self):
        for pin in bridge.SOURCES:
            for kind in ("changed", "missing", "duplicate"):
                with self.subTest(pin=pin, kind=kind), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin); path = root / "src/map.cpp"
                    original = native_map_shift_original(pin)
                    replacement = {"changed": original.replace("clear_vehicle_level_caches();", "different_vehicle_clear();"),
                                   "missing": "", "duplicate": original + original}[kind]
                    path.write_text(path.read_text(encoding="utf-8").replace(original, replacement, 1), encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "map-shift"): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_map_shift_missing_or_existing_include_is_atomic(self):
        for pin, name in ((p, n) for p in bridge.SOURCES for n in ("ncmm_graphics_map_shift.inc", "ncmm_graphics_support_rebase.hpp")):
            for existing in (False, True):
                with self.subTest(pin=pin, existing=existing), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin)
                    if existing: (root / "src" / name).write_text("existing shift include\n", encoding="utf-8")
                    before = self.fingerprint(root); original_read = Path.read_text
                    def read(path, *args, **kwargs):
                        if not existing and path == Path(bridge.__file__).with_name(name):
                            raise FileNotFoundError("missing shift include")
                        return original_read(path, *args, **kwargs)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                        with self.assertRaises((ValueError, FileNotFoundError)): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_map_shift_private_macro_collision_is_atomic(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin); path = root / "src/map.cpp"
                path.write_text(path.read_text(encoding="utf-8") + "\n#define NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP(c) collision\n", encoding="utf-8")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    with self.assertRaisesRegex(ValueError, "map-shift"): bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))

    def test_outside_cache_frozen_body_and_source_preservation(self):
        original = native_outside_cache_original()
        self.assertEqual(len(original.encode("utf-8")), 2217)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(),
                         "99181cb92ff370313fa5870684f513ee0dad010dcc0798f6aa9587b085a66abc")
        source = Path(bridge.__file__).with_name("ncmm_graphics_outside_cache.inc").read_text(encoding="utf-8")
        source = source[source.index("void map::build_outside_cache"):]
        begin, end = "            // BEGIN UNIFORM NATIVE OUTSIDE CACHE\n", "            // END UNIFORM NATIVE OUTSIDE CACHE\n"
        self.assertEqual(source.count(begin), 1); self.assertEqual(source.count(end), 1)
        prefix, rest = source.split(begin, 1)
        self.assertEqual(prefix + rest.split(end, 1)[1], original)

    def test_outside_cache_patch_retains_surrounding_native_source(self):
        original = native_outside_cache_original()
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin):
                source = "// prefix\n" + original + "// suffix\n"
                self.assertEqual(bridge.patch_native_outside_cache(source, pin),
                                 '// prefix\n#include "ncmm_graphics_outside_cache.inc"\n// suffix\n')

    def test_outside_cache_both_pins_source_copy_and_receipt(self):
        name = "ncmm_graphics_outside_cache.inc"
        helper = Path(bridge.__file__).with_name(name)
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, "check_output", return_value=pin): bridge.apply(root)
                source = (root / "src/map.cpp").read_text(encoding="utf-8")
                for companion in (name, "ncmm_graphics_floor_cache.inc", "ncmm_graphics_reconcile_items.inc"):
                    self.assertEqual(source.count(f'#include "{companion}"'), 1)
                self.assertNotIn("void map::build_outside_cache", source)
                self.assertIn("void map::set_outside_cache_dirty", source)
                self.assertIn("void map::on_field_modified", source)
                self.assertEqual((root / "src" / name).read_bytes(), helper.read_bytes())
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                self.assertEqual(receipt["inputs"][name], hashlib.sha256(helper.read_bytes()).hexdigest())

    def test_outside_cache_changed_missing_and_duplicate_primary_are_atomic(self):
        for pin in bridge.SOURCES:
            for kind in ("changed", "missing", "duplicate"):
                with self.subTest(pin=pin, kind=kind), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin); path = root / "src/map.cpp"
                    source = path.read_text(encoding="utf-8"); original = native_outside_cache_original()
                    replacement = {"changed": original.replace("ch.outside_cache_dirty = false;", "ch.outside_cache_dirty = true;"),
                                   "missing": "", "duplicate": original + original}[kind]
                    path.write_text(source.replace(original, replacement, 1), encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "outside-cache"): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_outside_cache_missing_or_existing_include_is_atomic(self):
        name = "ncmm_graphics_outside_cache.inc"
        for pin in bridge.SOURCES:
            for existing in (False, True):
                with self.subTest(pin=pin, existing=existing), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin)
                    if existing: (root / "src" / name).write_text("existing outside-cache include\n", encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root); original_read = Path.read_text
                    def read(path, *args, **kwargs):
                        if not existing and path == Path(bridge.__file__).with_name(name):
                            raise FileNotFoundError("missing outside-cache include")
                        return original_read(path, *args, **kwargs)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                        with self.assertRaises((ValueError, FileNotFoundError)): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_vision_transparency_frozen_body_and_source_preservation(self):
        original = native_vision_transparency_original()
        self.assertEqual(len(original.encode("utf-8")), 3826)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(),
                         "5dec4f06915166dad4638e9656dbd9f672acb20bc82a8b74ebe84a8bea229d5d")
        source = Path(bridge.__file__).with_name("ncmm_graphics_vision_transparency.inc").read_text(encoding="utf-8")
        source = source[source.index("bool map::build_vision_transparency_cache"):]
        begin, end = "            // BEGIN UNIFORM NATIVE VISION TRANSPARENCY\n", "            // END UNIFORM NATIVE VISION TRANSPARENCY\n"
        self.assertEqual(source.count(begin), 1); self.assertEqual(source.count(end), 1)
        prefix, rest = source.split(begin, 1)
        self.assertEqual(prefix + rest.split(end, 1)[1], original)

    def test_vision_transparency_patch_retains_surrounding_native_source(self):
        original = native_vision_transparency_original()
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin):
                source = "// prefix\n" + original + "// suffix\n"
                self.assertEqual(bridge.patch_native_vision_transparency(source, pin),
                                 '// prefix\n#include "ncmm_graphics_vision_transparency.inc"\n// suffix\n')

    def test_vision_transparency_both_pins_source_copy_and_receipt(self):
        name = "ncmm_graphics_vision_transparency.inc"
        helper = Path(bridge.__file__).with_name(name)
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, "check_output", return_value=pin): bridge.apply(root)
                source = (root / "src/lightmap.cpp").read_text(encoding="utf-8")
                self.assertEqual(source.count(f'#include "{name}"'), 1)
                self.assertEqual(source.count('#include "ncmm_graphics_apparent_light.inc"'), 1)
                self.assertNotIn("bool map::build_vision_transparency_cache", source)
                self.assertIn("// Native continuation retained.", source)
                self.assertEqual((root / "src" / name).read_bytes(), helper.read_bytes())
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                self.assertEqual(receipt["inputs"][name], hashlib.sha256(helper.read_bytes()).hexdigest())

    def test_vision_transparency_changed_missing_and_duplicate_primary_are_atomic(self):
        for pin in bridge.SOURCES:
            for kind in ("changed", "missing", "duplicate"):
                with self.subTest(pin=pin, kind=kind), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin); path = root / "src/lightmap.cpp"
                    source = path.read_text(encoding="utf-8"); original = native_vision_transparency_original()
                    replacement = {"changed": original.replace("map_cache.transparency_cache_dirty.reset();", "map_cache.transparency_cache_dirty.set();"),
                                   "missing": "", "duplicate": original + original}[kind]
                    path.write_text(source.replace(original, replacement, 1), encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "vision-transparency"): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_vision_transparency_missing_or_existing_include_is_atomic(self):
        name = "ncmm_graphics_vision_transparency.inc"
        for pin in bridge.SOURCES:
            for existing in (False, True):
                with self.subTest(pin=pin, existing=existing), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin)
                    if existing: (root / "src" / name).write_text("existing vision-transparency include\n", encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root); original_read = Path.read_text
                    def read(path, *args, **kwargs):
                        if not existing and path == Path(bridge.__file__).with_name(name):
                            raise FileNotFoundError("missing vision-transparency include")
                        return original_read(path, *args, **kwargs)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                        with self.assertRaises((ValueError, FileNotFoundError)): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_item_reconciliation_frozen_bodies_and_source_preservation(self):
        expected = {
            "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83": (4665, "43e9d5dff43780bc726edd5ce0798ecd7161dc6a8978072349f3d4b4088c2f82", "map_cursor( get_abs( p ) )"),
            "074aa98bd5be3de4c35f154082db32a0e63bb0f1": (4660, "ac9217d4344b3f4363009e3549e542a37fff80e894e0eb3df910b910cc6cc4d9", "map_cursor( this, p )"),
        }
        source = Path(bridge.__file__).with_name("ncmm_graphics_reconcile_items.inc").read_text(encoding="utf-8")
        source = source[source.index("void map::reconcile_loaded_items"):]
        begin, end = "                // BEGIN EMPTY UNIFORM NATIVE ITEMS\n", "                // END EMPTY UNIFORM NATIVE ITEMS\n"
        self.assertEqual(source.count(begin), 1); self.assertEqual(source.count(end), 1)
        prefix, rest = source.split(begin, 1)
        source = prefix + rest.split(end, 1)[1]
        self.assertEqual(source.count("NCMM_GRAPHICS_RECONCILE_MAP_CURSOR( p )"), 1)
        for pin, (length, digest, cursor) in expected.items():
            with self.subTest(pin=pin):
                original = native_item_reconciliation_original(pin)
                self.assertEqual(len(original.encode("utf-8")), length)
                self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), digest)
                self.assertEqual(source.replace("NCMM_GRAPHICS_RECONCILE_MAP_CURSOR( p )", cursor, 1), original)

    def test_item_reconciliation_patch_preserves_native_cursor_and_surroundings(self):
        cursor_macro = "NCMM_GRAPHICS_RECONCILE_MAP_CURSOR"
        for pin, (_, _, cursor) in bridge.NATIVE_RECONCILE_ORIGINALS.items():
            with self.subTest(pin=pin):
                source = "// prefix\n" + native_item_reconciliation_original(pin) + "// suffix\n"
                self.assertEqual(bridge.patch_native_item_reconciliation(source, pin),
                                 f'// prefix\n#define {cursor_macro}(p) {cursor}\n#include "ncmm_graphics_reconcile_items.inc"\n#undef {cursor_macro}\n// suffix\n')
                other = next(value for value in bridge.SOURCES if value != pin)
                with self.assertRaisesRegex(ValueError, "item-reconciliation"):
                    bridge.patch_native_item_reconciliation(source, other)

    def test_item_reconciliation_both_pins_source_copy_and_receipt(self):
        name = "ncmm_graphics_reconcile_items.inc"
        helper = Path(bridge.__file__).with_name(name)
        for pin, (_, _, cursor) in bridge.NATIVE_RECONCILE_ORIGINALS.items():
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, "check_output", return_value=pin): bridge.apply(root)
                source = (root / "src/map.cpp").read_text(encoding="utf-8")
                self.assertEqual(source.count(f'#include "{name}"'), 1)
                self.assertIn(f'#define NCMM_GRAPHICS_RECONCILE_MAP_CURSOR(p) {cursor}\n#include "{name}"\n#undef NCMM_GRAPHICS_RECONCILE_MAP_CURSOR\n', source)
                self.assertNotIn("void map::reconcile_loaded_items", source)
                self.assertIn("void map::set_floor_cache_dirty", source)
                self.assertIn("void map::on_field_modified", source)
                self.assertEqual((root / "src" / name).read_bytes(), helper.read_bytes())
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                self.assertEqual(receipt["inputs"][name], hashlib.sha256(helper.read_bytes()).hexdigest())

    def test_item_reconciliation_anchor_and_macro_failures_are_atomic(self):
        for pin in bridge.SOURCES:
            for kind in ("changed", "missing", "duplicate", "other_pin", "macro_collision"):
                with self.subTest(pin=pin, kind=kind), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin); path = root / "src/map.cpp"
                    source = path.read_text(encoding="utf-8"); original = native_item_reconciliation_original(pin)
                    other = next(value for value in bridge.SOURCES if value != pin)
                    replacement = {"changed": original.replace("index.clear();", "index.changed_clear();"),
                                   "missing": "", "duplicate": original + original,
                                   "other_pin": native_item_reconciliation_original(other),
                                   "macro_collision": "#define NCMM_GRAPHICS_RECONCILE_MAP_CURSOR(p) existing(p)\n" + original}[kind]
                    path.write_text(source.replace(original, replacement, 1), encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "item-reconciliation"): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_item_reconciliation_missing_or_existing_include_is_atomic(self):
        name = "ncmm_graphics_reconcile_items.inc"
        for pin in bridge.SOURCES:
            for existing in (False, True):
                with self.subTest(pin=pin, existing=existing), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin)
                    if existing: (root / "src" / name).write_text("existing item-reconciliation include\n", encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root); original_read = Path.read_text
                    def read(path, *args, **kwargs):
                        if not existing and path == Path(bridge.__file__).with_name(name):
                            raise FileNotFoundError("missing item-reconciliation include")
                        return original_read(path, *args, **kwargs)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                        with self.assertRaises((ValueError, FileNotFoundError)): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_floor_cache_primary_and_source_preserving_fast_path(self):
        original = native_floor_cache_original()
        self.assertEqual(len(original.encode("utf-8")), 2349)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(),
                         "80c7cfd987d5550275fd2eabf182d2027591c99d22fa8ca58024da46c2495eb6")
        source = Path(bridge.__file__).with_name("ncmm_graphics_floor_cache.inc").read_text(encoding="utf-8")
        source = source[source.index("bool map::build_floor_cache"):]
        begin, end = "            // BEGIN UNIFORM NATIVE FLOOR CACHE\n", "            // END UNIFORM NATIVE FLOOR CACHE\n"
        self.assertEqual(source.count(begin), 1)
        self.assertEqual(source.count(end), 1)
        before, rest = source.split(begin, 1)
        self.assertEqual(before + rest.split(end, 1)[1], original)

    def test_floor_cache_patch_retains_surrounding_native_source_both_pins(self):
        original = native_floor_cache_original()
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin):
                source = "// Native prefix.\n" + original + "// Native suffix.\n"
                self.assertEqual(bridge.patch_native_floor_cache(source, pin),
                                 '// Native prefix.\n#include "ncmm_graphics_floor_cache.inc"\n// Native suffix.\n')

    def test_floor_cache_both_pins_source_copy_and_receipt(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_floor_cache.inc")
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, "check_output", return_value=pin): bridge.apply(root)
                source = (root / "src/map.cpp").read_text(encoding="utf-8")
                self.assertEqual(source.count('#include "ncmm_graphics_floor_cache.inc"'), 1)
                self.assertNotIn("bool map::build_floor_cache", source)
                self.assertIn("void map::set_floor_cache_dirty", source)
                self.assertIn("void map::on_field_modified", source)
                self.assertEqual((root / "src" / helper.name).read_bytes(), helper.read_bytes())
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                self.assertEqual(receipt["inputs"][helper.name], hashlib.sha256(helper.read_bytes()).hexdigest())

    def test_floor_cache_changed_missing_and_duplicate_primary_are_atomic(self):
        for pin in bridge.SOURCES:
            for kind in ("changed", "missing", "duplicate"):
                with self.subTest(pin=pin, kind=kind), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin); path = root / "src/map.cpp"
                    source = path.read_text(encoding="utf-8")
                    original = native_floor_cache_original()
                    replacement = {"changed": original.replace("return zlevels;", "return true;"),
                                   "missing": "", "duplicate": original + original}[kind]
                    path.write_text(source.replace(original, replacement, 1), encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "floor-cache"): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_floor_cache_missing_or_existing_include_is_atomic(self):
        name = "ncmm_graphics_floor_cache.inc"
        for pin in bridge.SOURCES:
            for existing in (False, True):
                with self.subTest(pin=pin, existing=existing), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin)
                    if existing: (root / "src" / name).write_text("existing floor-cache include\n", encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root); original_read = Path.read_text
                    def read(path, *args, **kwargs):
                        if not existing and path == Path(bridge.__file__).with_name(name):
                            raise FileNotFoundError("missing floor-cache include")
                        return original_read(path, *args, **kwargs)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                        with self.assertRaises((ValueError, FileNotFoundError)): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_update_map_primary_and_source_preserving_markers(self):
        original = native_update_map_original()
        self.assertEqual(len(original.encode()), 3205)
        self.assertEqual(hashlib.sha256(original.encode()).hexdigest(),
                         'bfb01cf4e07c99f1b41ebd460c81531fb8ff33e0c9800e58cf13e30201253a4b')
        source = Path(bridge.__file__).with_name('ncmm_graphics_update_map.inc').read_text(encoding='utf-8')
        source = source[source.index('point_rel_sm game::update_map'):]
        stripped = ''.join(line for line in source.splitlines(keepends=True) if 'ncmm_move_scope' not in line)
        self.assertEqual(stripped.replace(bridge.NATIVE_UPDATE_MAP_RESET_SCOPE, '', 1), original)
        self.assertEqual(stripped.count(bridge.NATIVE_UPDATE_MAP_RESET_SCOPE), 1)

    def test_update_map_both_pins_receipts_and_global_tls_include(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, 'check_output', return_value=pin): bridge.apply(root)
                receipt = json.loads((root/'first-person-source.json').read_text(encoding='utf-8'))
                for name in ('ncmm_graphics_move_diagnostics.hpp', 'ncmm_graphics_update_map.inc'):
                    content = Path(bridge.__file__).with_name(name).read_bytes()
                    self.assertEqual((root/'src'/name).read_bytes(), content)
                    self.assertEqual(receipt['inputs'][name], hashlib.sha256(content).hexdigest())
                for name, anchor in (('game.cpp', '#include "game.h"'), ('ncmm_loader.cpp', '#include "ncmm_loader.h"')):
                    source = (root/'src'/name).read_text(encoding='utf-8')
                    self.assertEqual(source.count('#include "ncmm_graphics_move_diagnostics.hpp"'), 1)
                    self.assertLess(source.index(anchor), source.index('#include "ncmm_graphics_move_diagnostics.hpp"'))
                    namespace = re.search(r'namespace\s*\{', source)
                    if namespace: self.assertLess(source.index('#include "ncmm_graphics_move_diagnostics.hpp"'), namespace.start())
                source = (root/'src/game.cpp').read_text(encoding='utf-8')
                self.assertEqual(source.count('#include "ncmm_graphics_update_map.inc"'), 1)
                self.assertIn('// Native continuation retained.', source)

    def test_update_map_changed_and_duplicate_primary_are_atomic(self):
        for pin in bridge.SOURCES:
            for duplicate in (False, True):
                with self.subTest(pin=pin, duplicate=duplicate), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin); game = root/'src/game.cpp'
                    text = game.read_text(encoding='utf-8')
                    game.write_text(text+native_update_map_original() if duplicate else text.replace('load_npcs();', 'different_load_npcs();'), encoding='utf-8')
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, 'check_output', return_value=pin):
                        with self.assertRaisesRegex(ValueError, 'update_map|cache.reset'): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_update_map_missing_or_existing_include_is_atomic(self):
        for pin in bridge.SOURCES:
            for name in ('ncmm_graphics_move_diagnostics.hpp', 'ncmm_graphics_update_map.inc'):
                for existing in (False, True):
                    with self.subTest(pin=pin, name=name, existing=existing), tempfile.TemporaryDirectory() as tmp:
                        root = Path(tmp); self.fixture(root, pin)
                        if existing: (root/'src'/name).write_text('existing file\n')
                        before = self.fingerprint(root); original_read = Path.read_text
                        def read(path, *args, **kwargs):
                            if not existing and path == Path(bridge.__file__).with_name(name): raise FileNotFoundError('missing diagnostic include')
                            return original_read(path, *args, **kwargs)
                        with patch.object(bridge.subprocess, 'check_output', return_value=pin), patch.object(Path, 'read_text', read):
                            with self.assertRaises((ValueError, FileNotFoundError)): bridge.apply(root)
                        self.assertEqual(before, self.fingerprint(root))

    def fixture(self, root, pin=next(iter(bridge.SOURCES))):
        (root / "src").mkdir()
        cpp, header = native_shadowcast_originals()
        (root/'src/shadowcasting.cpp').write_text(cpp,encoding='utf-8')
        (root/'src/shadowcasting.h').write_text(header,encoding='utf-8')
        for name in ("ncmm_loader.cpp", "ncmm_loader.h"):
            (root / "src" / name).write_bytes((REPO / "host_patch" / name).read_bytes())
        (root / "src/game.h").write_text("        bool save();\n")
        (root / "src/game.cpp").write_text('#include "game.h"\n'+native_update_map_original()+'\n// Native continuation retained.\n')
        (root / "src/vehicle_use.cpp").write_text(NATIVE_TOGGLE)
        (root / "src/debug.cpp").write_text('#include "debug.h"\nstd::string ncmm_debug_report_text( const std::string &text )\n{ return text; }\nvoid native_debug_window() {\n    for( bool stop = false; !stop; ) {\n    }\n}\n')
        options='static void ncmm_apply_deferred_option_value( const std::string &, options_manager::cOpt & );\n// NCMM v8.7.6.6 stale world-option repair:\n'
        for kind in ['bool','int','float','enum']:
            options+='bool options_manager::ncmm_register_world_'+kind+'()\n{\n    if( missing ) {\n        return true;\n    }\n    cOpt &opt = it->second;\n    if( world_options.has_value() ) { }\n    return true;\n}\n'
        options+='\nvoid options_manager::update_global_locale() { }\n'
        (root/'src/options.cpp').write_text(options)
        (root / "src/creature.h").write_text('#include <array>\nclass Creature {\n    public:\n        ~Creature() override;\n};\n')
        (root / "src/overmapbuffer.h").write_text("        bool has( const point_abs_om &p );\n")
        (root / "src/map.h").write_text("        int supports_zlevels() const {\n            return zlevels;\n        }\n" + APPARENT_LIGHT_DECLARATIONS)
        (root / "src/lightmap.cpp").write_text('#include "map.h"\n' + apparent_light_original() + '\n' + native_vision_transparency_original() + '\n// Native continuation retained.\n')
        map_cpp='#include "map.h"\n'
        map_cpp += native_map_cache_original()
        map_cpp += native_phase_source("loadn")
        (root/"src/mapgen.cpp").write_text(native_phase_source("phase_function"),encoding="utf-8")
        (root/"src/submap.h").write_text(native_phase_source("submap_anchor"),encoding="utf-8")
        (root/"src/submap.cpp").write_text(native_phase_query(),encoding="utf-8")
        map_cpp += native_actualize_original()
        for prefix in native_actualize_prefixes().values():
            map_cpp += prefix + '}\n' * (prefix.count('{') - prefix.count('}'))
        for signature in ("void map::set_transparency_cache_dirty( const int zlev )",
                          "void map::set_transparency_cache_dirty( const tripoint_bub_ms &p, bool field )",
                          "void map::set_outside_cache_dirty( const int zlev )",
                          "void map::set_floor_cache_dirty( const int zlev )"):
            map_cpp+=signature+"\n{\n}\n"
        (root / "src/map.cpp").write_text(map_cpp+native_item_reconciliation_original(pin)+native_map_shift_original(pin)+native_outside_cache_original()+native_floor_cache_original()+'\n    current_submap->set_ter( l, new_terrain );\n    current_submap->set_furn( l, new_target_furniture );\n'+'void map::on_field_modified( const tripoint_bub_ms &p, const field_type &fd_type )\n{\n}\n')
        (root / "src/input_context.cpp").write_text('#include "input_context.h"\nconst std::string &input_context::handle_input( const int timeout )\n{\n        next_action = inp_mngr.get_input_event( preferred_keyboard_mode );\n        if( next_action.type == input_event_t::timeout ) {\n        }\n    inp_mngr.set_timeout( old_timeout );\n    return *result;\n}\n')
        (root / "src/input.cpp").write_text('#include "input.h"\nvoid input_manager::wait_for_any_key()\n{\n}\n')
        (root / "src/sdltiles.cpp").write_text(SDL_ANCHOR+'\nstatic void CheckMessages()\n{\n        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );\n    if( needupdate ) {\n        try_sdl_update();\n    }\n}\n')
        (root / "src/sdl_wrappers.cpp").write_text('#include "sdl_wrappers.h"\nvoid RenderPresent() {\n    printErrorIf( !SDL_RenderPresent( renderer.get() ), "SDL_RenderPresent failed" );\n}\n')
        with (root / "src/sdltiles.cpp").open('a') as f:
            f.write('void refresh_mouse_config() {\n    ImGuiIO &io = ImGui::GetIO();\n    if( !mouse.enabled ) {}\n}\n')
            f.write('\n'.join('void teardown_'+str(i)+'() {\n    renderer.reset();\n}' for i in range(4))+'\n')
            f.write('    } else if( ui_adaptor::has_imgui() ) {\n        try_sdl_update();\n    }\n')
        (root / "src/main_menu.cpp").write_text("        std::exit( ncmm::run_gameplay_smoke() );")
        (root / "src/sdltiles.h").write_text("SDL_Window *get_sdl_window();")
        (root / "src/handle_action.cpp").write_text("    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {\n        player_character.clear_destination();\n        destination_preview.clear();\n        return false;\n    }")
        with (root / "src/handle_action.cpp").open('a') as f: f.write('\n    if( act == ACTION_NULL ) {\n        act = look_up_action( action );\n    }')
        with (root / "src/handle_action.cpp").open('a') as f: f.write('\n        if( act == ACTION_SELECT || act == ACTION_SEC_SELECT ) {}\ninput_context game::get_player_input( std::string &action )\n{\n    return ctxt;\n}\n\nstatic void rcdrive( ) {}')
        with (root / "src/handle_action.cpp").open('a') as f:f.write('\n        if( act == ACTION_INTERACT ) {}\nplayer_character.assign_activity( open_tile_activity_actor( std::nullopt ) );\n')
        with (root / "src/handle_action.cpp").open('a') as f:f.write('\nstatic void reach_attack( avatar &you )\n{\n}\n')
        (root / "src/melee.cpp").write_text('    const bool hits = hit_spread >= 0;')
        (root / "src/animation.cpp").write_text('#include "animation.h"\nclass basic_animation {\n        void draw() const {\n            static_popup popup;\n        }\n};\nclass bullet_animation {\n        void progress() const {\n            draw();\n\n            // Skip artificial animation delay\n        }\n};\n')
        (root / "src/avatar_action.cpp").write_text('#include "avatar_action.h"\nvoid avatar_action::fire_wielded_weapon( avatar &you )\n{\n    const item_location weapon = ncmm::select_ranged_weapon( you, ncmm::ranged_weapon_action::fire, "select", "select" );\n    if( weapon == you.get_wielded_item() ) {\n        you.assign_activity( aim_activity_actor::use_wielded() );\n    } else {\n        you.assign_activity( aim_activity_actor::use_item_location( weapon ) );\n    }\n}\n')
        (root / "src/ranged.cpp").write_text('target_handler::trajectory target_ui::run()\n{\n}\n        curshot++;\n\n        if( !gun.is_gun() ) {}\n    bool skip_redraw = false;')
        (root / "src/creature.cpp").write_text("int Creature::deal_melee_attack( Creature *source, int hitroll )\n{\n}\n    apply_damage( source, bp, total_damage );")
        (root / "src/ballistics.cpp").write_text('#include "ballistics.h"\n    const bool do_animation = get_option<bool>( "ANIMATION_PROJECTILES" );\n        // Done with the trajectory!')
        (root / "src/monster.cpp").write_text("    // We were carrying a creature, deposit the rider")
        (root / "src/npc.cpp").write_text("    prevent_death_reminder = false;\n    dialogue d( get_talker_for( this ),")
        (root / "src/cata_tiles.h").write_text("        std::optional<texture_draw_data> get_texture_draw_data( const std::string &id,\n                TILE_CATEGORY category, const tripoint_bub_ms &p );")
        (root / "src/tileset_loader.h").write_text("                                      const atlas_replay_quarantine::gate &abandon_gate );\n        void create_textures_from_tile_atlas")
        (root / "src/tileset_loader.cpp").write_text('#include "tileset_loader.h"\n'
            '        const std::vector<SDL_Rect> &opaque_bounds,\n        const atlas_replay_quarantine::gate &abandon_gate )\n'
            '    cata_assert( texture_ptr );\n    ++texture_count;\n'
            '            copy_surface_to_texture( scan_surf, offset, *tile_values, opaque_bounds, abandon_gate );\n')
        (root / "src/cata_tiles.cpp").write_text("""void cata_tiles::load_tileset( const std::string &tileset_id, const bool precheck,
                               const bool force, const bool pump_events, const bool terrain )
{
}
std::optional<texture_draw_data> cata_tiles::get_texture_draw_data( const std::string &id,
        TILE_CATEGORY category, const tripoint_bub_ms &p )
{
    const std::vector<int> *indices = ttype.fg.pick( seed );
}
std::unordered_set<std::string> cata_tiles::get_all_portrait_tile_ids() {}
bool cata_tiles::draw_sprite_at() {
    const texture *sprite_tex = tileset_ptr->get_tile( sprite_index );
    height_3d += tile.height_3d;
    return true;
}
""")
    def fingerprint(self, root):
        return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in root.rglob("*") if p.is_file()}
    def test_unknown_identity_no_writes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root); before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value="a" * 40):
                with self.assertRaisesRegex(ValueError, "Unqualified"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_static_detail_frozen_oracle_is_independent_and_unchanged(self):
        frozen = (REPO / "experiments/FirstPersonView/tests/multilevel_cache_test.cpp").read_text(encoding="utf-8")
        begin, end = "// BEGIN FROZEN STATIC DETAIL\n", "// END FROZEN STATIC DETAIL\n"
        self.assertEqual(frozen.count(begin), 1)
        self.assertEqual(frozen.count(end), 1)
        original = frozen.split(begin, 1)[1].split(end, 1)[0]
        self.assertEqual(len(original.encode("utf-8")), 1759)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), "f4013a7f84a0163079fb9fd73881459707da474e82587b732ede4f8440a0317d")
        self.assertNotIn("graphics_populate_static_detail", original)
    def test_static_detail_both_pins_and_source_receipt(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_static_detail.inc")
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    bridge.apply(root)
                self.assertEqual((root / "src" / helper.name).read_bytes(), helper.read_bytes())
                hybrid = (root / "src/ncmm_graphics_hybrid.inc").read_text(encoding="utf-8")
                self.assertEqual(hybrid.count('#include "ncmm_graphics_static_detail.inc"'), 1)
                call = "if(graphics_populate_static_detail(here,p,d,world,facade_lights,i,terrain_shape,copy_id))continue;"
                self.assertEqual(hybrid.count(call), 1)
                self.assertIn(call + "\n        const ter_t &ter = here.ter( p ).obj(); const furn_t &furn = here.furn( p ).obj();", hybrid)
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                for name in (helper.name, "ncmm_graphics_hybrid.inc"):
                    self.assertEqual(receipt["inputs"][name], hashlib.sha256((root / "src" / name).read_bytes()).hexdigest())
    def test_static_detail_existing_include_failure_is_atomic(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                (root / "src/ncmm_graphics_static_detail.inc").write_text("existing extraction\n", encoding="utf-8")
                (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    with self.assertRaisesRegex(ValueError, "Experimental source already exists: ncmm_graphics_static_detail.inc"):
                        bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_static_detail_missing_helper_failure_is_atomic(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_static_detail.inc")
        original_read = Path.read_text
        def read(path, *args, **kwargs):
            if path == helper:
                raise FileNotFoundError("missing static-detail source")
            return original_read(path, *args, **kwargs)
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                    with self.assertRaisesRegex(FileNotFoundError, "missing static-detail source"):
                        bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_terrain_stair_sampler_frozen_oracle_is_independent_and_unchanged(self):
        frozen = (REPO / "experiments/FirstPersonView/tests/multilevel_render_test.cpp").read_text(encoding="utf-8")
        begin, end = "// BEGIN FROZEN TERRAIN STAIR SAMPLER\n", "// END FROZEN TERRAIN STAIR SAMPLER\n"
        self.assertEqual(frozen.count(begin), 1)
        self.assertEqual(frozen.count(end), 1)
        original = frozen.split(begin, 1)[1].split(end, 1)[0]
        self.assertEqual(len(original.encode("utf-8")), 1682)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), "8678d4be60bcb162394c0353dff16fc90949d7e9c7a37b6bca6d2fc2a3322b3a")
        self.assertNotIn("graphics_world_terrain_stair_flags", original)
    def test_multilevel_observer_sources_both_pins_and_receipt(self):
        names = ("ncmm_graphics_terrain_stair.inc", "ncmm_graphics_multilevel_observer.inc")
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    bridge.apply(root)
                for name in names:
                    self.assertEqual((root / "src" / name).read_bytes(), Path(bridge.__file__).with_name(name).read_bytes())
                world = (root / "src/ncmm_graphics_world.inc").read_text(encoding="utf-8")
                observer = (root / "src/ncmm_graphics_multilevel_smoke.inc").read_text(encoding="utf-8")
                self.assertEqual(world.count('#include "ncmm_graphics_terrain_stair.inc"'), 1)
                self.assertEqual(observer.count('#include "ncmm_graphics_multilevel_observer.inc"'), 1)
                self.assertIn("graphics_world_terrain_stair_flags(here,ax,ay,z,ox,oy,side,side,loaded.front(),loaded.back())", world)
                self.assertIn("world.loaded_min_z,world.loaded_max_z", observer)
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                for name in names + ("ncmm_graphics_world.inc", "ncmm_graphics_multilevel_smoke.inc"):
                    self.assertEqual(receipt["inputs"][name], hashlib.sha256((root / "src" / name).read_bytes()).hexdigest())
    def test_multilevel_observer_existing_source_failure_is_atomic(self):
        for pin in bridge.SOURCES:
            for name in ("ncmm_graphics_terrain_stair.inc", "ncmm_graphics_multilevel_observer.inc"):
                with self.subTest(pin=pin, source=name), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin)
                    (root / "src" / name).write_text("existing observer source\n", encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "Experimental source already exists: " + re.escape(name)):
                            bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))
    def test_multilevel_observer_missing_source_failure_is_atomic(self):
        original_read = Path.read_text
        for pin in bridge.SOURCES:
            for name in ("ncmm_graphics_terrain_stair.inc", "ncmm_graphics_multilevel_observer.inc"):
                helper = Path(bridge.__file__).with_name(name)
                def read(path, *args, **kwargs):
                    if path == helper:
                        raise FileNotFoundError("missing observer source")
                    return original_read(path, *args, **kwargs)
                with self.subTest(pin=pin, source=name), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin)
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                        with self.assertRaisesRegex(FileNotFoundError, "missing observer source"):
                            bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))
    def test_candidate_range_frozen_oracle_is_independent_and_unchanged(self):
        frozen = (REPO / "experiments/FirstPersonView/tests/multilevel_cache_test.cpp").read_text(encoding="utf-8")
        begin, end = "// BEGIN FROZEN CANDIDATE RANGE\n", "// END FROZEN CANDIDATE RANGE\n"
        self.assertEqual(frozen.count(begin), 1)
        self.assertEqual(frozen.count(end), 1)
        original = frozen.split(begin, 1)[1].split(end, 1)[0]
        self.assertEqual(len(original.encode("utf-8")), 839)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), "9f5d494805a4f67005db9636c92ee80cbfe677a6ad492a9497349738936b37b6")
        self.assertIn("lower_in_range=in_visual_range(below);", original)
    def test_candidate_range_both_pins_and_source_receipt(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_candidate_range.inc")
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    bridge.apply(root)
                self.assertEqual((root / "src" / helper.name).read_bytes(), helper.read_bytes())
                world = (root / "src/ncmm_graphics_world.inc").read_text(encoding="utf-8")
                self.assertEqual(world.count('#include "ncmm_graphics_candidate_range.inc"'), 1)
                self.assertIn("if(!static_surface&&!current_in_range&&!lower_in_range&&p!=you.pos_bub())", world)
                self.assertIn("if(!structure_range&&!lower_in_range)permission&=~NCMM_WORLD_FLOOR_BOTTOM;", world)
                self.assertIn("if(permission&NCMM_WORLD_FLOOR_BOTTOM){const auto rgb=graphics_world_rgb(below);", world)
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                for name in (helper.name, "ncmm_graphics_world.inc"):
                    self.assertEqual(receipt["inputs"][name], hashlib.sha256((root / "src" / name).read_bytes()).hexdigest())
    def test_candidate_range_existing_include_failure_is_atomic(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                (root / "src/ncmm_graphics_candidate_range.inc").write_text("existing candidate evaluation\n", encoding="utf-8")
                (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    with self.assertRaisesRegex(ValueError, "Experimental source already exists: ncmm_graphics_candidate_range.inc"):
                        bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_candidate_range_missing_helper_failure_is_atomic(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_candidate_range.inc")
        original_read = Path.read_text
        def read(path, *args, **kwargs):
            if path == helper:
                raise FileNotFoundError("missing candidate-range source")
            return original_read(path, *args, **kwargs)
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                    with self.assertRaisesRegex(FileNotFoundError, "missing candidate-range source"):
                        bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_native_apparent_light_adaptation_retains_both_primary_pin_bodies(self):
        original = apparent_light_original()
        digest = "7e570456c43c6dcf6b02d29bd4ef8a25a63a0889006855f28483870dd6a70a14"
        self.assertEqual(len(original.encode("utf-8")), 5280)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), digest)
        self.assertEqual(set(bridge.APPARENT_LIGHT_ORIGINALS), set(bridge.SOURCES))
        extracted = Path(bridge.__file__).with_name("ncmm_graphics_apparent_light.inc").read_text(encoding="utf-8")
        signature = "map::apparent_light_info map::apparent_light_helper( const level_cache &map_cache,\n        const tripoint_bub_ms &p )\n"
        implementation = "static map::apparent_light_info ncmm_apparent_light_impl( const level_cache &map_cache,\n        const tripoint_bub_ms &p, const avatar &u, const int dist )\n"
        native_locals = "    avatar const &u = get_avatar();\n    const int dist = rl_dist( u.pos_bub(), p );\n"
        helper, outer = original.split("\nlit_level map::apparent_light_at(", 1)
        self.assertEqual(helper.count(native_locals), 1)
        adapted_helper = helper.replace(signature, implementation, 1).replace(native_locals, "", 1)
        wrapper = signature + "{\n" + native_locals + "    return ncmm_apparent_light_impl( map_cache, p, u, dist );\n}\n"
        adapted_outer = ("lit_level map::apparent_light_at(" + outer).replace(
            "    const apparent_light_info a = apparent_light_helper( map_cache, p );",
            "    const apparent_light_info a = ncmm_apparent_light_impl( map_cache, p, get_avatar(), dist );", 1)
        # All remaining Native body text, including guards and float order, is
        # exact. The public wrapper retains its original getter/distance pair.
        self.assertEqual(extracted[extracted.index(implementation):], adapted_helper + "\n" + wrapper + "\n" + adapted_outer)
        self.assertTrue(extracted.startswith("// Adapted from Cataclysm: Dark Days Ahead"))
        self.assertIn("https://creativecommons.org/licenses/by-sa/3.0/", extracted)
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin):
                self.assertEqual(bridge.APPARENT_LIGHT_ORIGINALS[pin], (5280, digest))
                self.assertIn("/blob/" + pin + "/src/lightmap.cpp", extracted)
    def test_native_apparent_light_both_pins_and_source_receipt(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root, pin)
                original = (root / "src/lightmap.cpp").read_text(encoding="utf-8")
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    bridge.apply(root)
                actual = (root / "src/lightmap.cpp").read_text(encoding="utf-8")
                expected = original.replace(apparent_light_original(), '#include "ncmm_graphics_apparent_light.inc"\n', 1)
                expected = expected.replace(native_vision_transparency_original(), '#include "ncmm_graphics_vision_transparency.inc"\n', 1)
                self.assertEqual(actual, expected)
                self.assertEqual((root / "src/ncmm_graphics_apparent_light.inc").read_bytes(), Path(bridge.__file__).with_name("ncmm_graphics_apparent_light.inc").read_bytes())
                self.assertIn(APPARENT_LIGHT_DECLARATIONS, (root / "src/map.h").read_text(encoding="utf-8"))
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                self.assertIn("lightmap.cpp", receipt["inputs"])
                self.assertIn("ncmm_graphics_apparent_light.inc", receipt["inputs"])
                for name, digest in receipt["inputs"].items():
                    self.assertEqual(hashlib.sha256((root / "src" / name).read_bytes()).hexdigest(), digest)
    def test_native_apparent_light_anchor_failure_is_atomic_including_receipt(self):
        original = apparent_light_original()
        for pin in bridge.SOURCES:
            for changed in (original.replace("avatar const &u = get_avatar();", "avatar const &u = changed_avatar();"),
                            original.replace("a.apparent_light >= cache.vision_threshold", "a.apparent_light > cache.vision_threshold"),
                            original + original, "changed Native lightmap"):
                with self.subTest(pin=pin, changed=changed[:80]), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root, pin)
                    (root / "src/lightmap.cpp").write_text(changed, encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "Native apparent-light source differs"):
                            bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))
    def test_native_apparent_light_existing_include_failure_is_atomic(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/ncmm_graphics_apparent_light.inc").write_text("existing Native include\n", encoding="utf-8")
            (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "Experimental source already exists: ncmm_graphics_apparent_light.inc"):
                    bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_native_apparent_light_missing_source_is_atomic(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/lightmap.cpp").unlink()
            (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaises(FileNotFoundError):
                    bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_multilevel_minor_two_dispatch_preserves_legacy_and_rejects_newer(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                bridge.apply(root)
            loader = (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8")
            # Inspect the actual generated dispatch, not a second negotiation
            # helper which could pass while the inserted Native branch drifted.
            limits = re.search(r'if\( interface_id && std::string\( interface_id \) == NCMM_GRAPHICS_MULTILEVEL_ID \) \{\s*'
                               r'return min_major <= (\d+) && min_minor <= (\d+) \? &graphics_world_api : nullptr;', loader)
            self.assertIsNotNone(limits)
            major, minor = map(int, limits.groups())
            for requested_major, requested_minor, accepted in ((1,0,True),(1,1,True),(1,2,True),(1,3,False),(2,0,False)):
                with self.subTest(major=requested_major, minor=requested_minor):
                    self.assertEqual(requested_major <= major and requested_minor <= minor, accepted)
            published = (root / "src/ncmm_graphics_world.inc").read_text(encoding="utf-8")
            self.assertIn('graphics_world_api={1,sizeof(ncmm_host_graphics_multilevel_v1),1,2,', published)
            self.assertIn('graphics_get_world_metrics,graphics_get_lod,graphics_get_skyline}', published)
    def test_changed_submap_cache_reset_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            target=root/'src/map.cpp';target.write_text(target.read_text().replace('set_pathfinding_cache_dirty( z );','changed_native_cache_reset();'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_native_firing_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            target=root/'src/avatar_action.cpp'
            target.write_text(target.read_text().replace('void avatar_action::fire_wielded_weapon( avatar &you )', 'void changed_native_firing( avatar &you )'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_native_animation_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            target=root/'src/animation.cpp';target.write_text(target.read_text().replace('static_popup popup;', 'changed_native_animation();'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_rebase_cache_reset_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            (root/'src/game.cpp').write_text('#include "game.h"\nchanged_native_cache_reset();')
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_missing_field_notification_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            target=root/'src/map.cpp';target.write_text(target.read_text().replace('void map::on_field_modified(', 'void changed_field_notification('))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_missing_late_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/sdltiles.cpp").write_text("changed renderer")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_changed_input_return_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            path=root/'src/input_context.cpp'
            path.write_text(path.read_text().replace('return *result;', 'return modified_result;'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_success_and_reapply_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                bridge.apply(root)
                self.assertTrue((root / "first-person-source.json").is_file())
                firing=(root/'src/avatar_action.cpp').read_text(encoding='utf-8')
                self.assertLess(firing.index('graphics_take_fps_gunshot'),firing.index('ncmm::select_ranged_weapon'))
                self.assertIn('aim_activity_actor::use_item_location( weapon )',firing)
                animation=(root/'src/animation.cpp').read_text(encoding='utf-8')
                self.assertLess(animation.index('ncmm::graphics_cursor_captured()'),animation.index('static_popup popup;'))
                self.assertIn('ui_manager::redraw_invalidated();',animation)
                self.assertIn("draw_graphics_view", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_requires_terrain_pass", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn("ncmm_native_pixels_ready = true;", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn("&& !ncmm_native_pixels_ready", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn(SDL_ANCHOR, (root / "src/sdltiles.cpp").read_text(encoding="utf-8")) # Full native fallback remains available.
                self.assertIn('if( !printErrorIf( !SDL_RenderPresent( renderer.get() ), "SDL_RenderPresent failed" ) )', (root / "src/sdl_wrappers.cpp").read_text(encoding="utf-8"))
                self.assertEqual((root / "src/sdl_wrappers.cpp").read_text(encoding="utf-8").count("ncmm::graphics_frame_presented();"), 1)
                self.assertIn("graphics_erase( module_id )", (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_context.note( category );", (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_targeting_scope ncmm_aiming", (root / "src/ranged.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_input_wait( category, input_manager::actual_keyboard_mode( preferred_keyboard_mode ) == keyboard_mode::keychar )", (root / "src/input_context.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_input_wait( const std::string &, bool text )", (root / "src/ncmm_loader.h").read_text(encoding="utf-8"))
                self.assertTrue((root / "src/ncmm_graphics_context.hpp").is_file())
                self.assertNotIn('const std::string aimed = ncmm::graphics_click_action();',
                                 (root / 'src/handle_action.cpp').read_text())
                self.assertTrue((root / "src/ncmm_graphics_snapshot_cache.hpp").is_file())
                self.assertIn('ncmm_wait.completed( *result );', (root / "src/input_context.cpp").read_text())
                self.assertIn('graphics_snapshot_external_callback( module_id );', (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn('#include "ncmm_graphics_scratch.hpp"', (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertEqual((root / "src/ncmm_graphics_scratch.hpp").read_bytes(), Path(bridge.__file__).with_name("ncmm_graphics_scratch.hpp").read_bytes())
                self.assertEqual((root / "src/ncmm_native_atlas_pixels.hpp").read_bytes(), Path(bridge.__file__).with_name("ncmm_native_atlas_pixels.hpp").read_bytes())
                self.assertIn("tile_values == targets.normal", (root / "src/tileset_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics.creatures.v1", (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_creature( this, 4 )", (root / "src/monster.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_creature( origin, 2", (root / "src/ballistics.cpp").read_text(encoding="utf-8"))
                tiles = (root / "src/cata_tiles.cpp").read_text(encoding="utf-8")
                self.assertEqual(tiles.count("if( stable && ttype.animated ) seed = 0;"), 1)
                self.assertIn("cata_tiles::get_texture_draw_data", tiles)
                self.assertNotIn("ncmm_perspective_prepass", tiles)
                before = self.fingerprint(root)
                with self.assertRaises(ValueError): bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_changed_native_atlas_upload_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            (root/'src/tileset_loader.cpp').write_text('changed Native upload')
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'Native atlas anchor differs'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))

    def test_ambiguous_native_atlas_upload_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            p=root/'src/tileset_loader.cpp';p.write_text(p.read_text()*2)
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'Native atlas anchor differs'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))

    def test_changed_native_compiler_predicate_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            (root/'src/vehicle_use.cpp').write_text('changed upstream predicate')
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'electronic-toggle predicate changed'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))

    def test_missing_shutdown_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/main_menu.cpp").write_text("changed smoke entry")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_changed_native_animation_input_loop_no_partial_patch(self):
        for anchor in ['input_context game::get_player_input(', 'static void rcdrive(']:
            with self.subTest(anchor=anchor), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);self.fixture(root);path=root/'src/handle_action.cpp'
                path.write_text(path.read_text().replace(anchor,'changed_native_input_loop('))
                before=self.fingerprint(root)
                with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                    with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
                self.assertEqual(before,self.fingerprint(root))
    def test_changed_native_debug_helper_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);(root/'src/debug.cpp').write_text('changed Native report helper')
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_blocking_native_wait_has_presentation_deadline(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);sdl=root/'src/sdltiles.cpp'
            sdl.write_text(sdl.read_text()+'\nstatic int input_wait_ms( const std::optional<uint32_t> &input_ms ) {\n    return input_wait_timeout_ms( s );\n}\n')
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):bridge.apply(root)
            self.assertIn('return ncmm::graphics_input_wait_timeout( input_wait_timeout_ms( s ) );',sdl.read_text())
            ballistic=(root/'src/ballistics.cpp').read_text()
            self.assertIn('&& !ncmm::graphics_projectile_presentation()',ballistic)
            self.assertIn('graphics_note_projectile(origin,*here,proj_arg,source,tp,t_copy)',ballistic)
            self.assertIn('if(ncmm::graphics_projectile_presentation())return;', (root/'src/animation.cpp').read_text())
            self.assertEqual((root/'src/ncmm_loader.cpp').read_text(encoding='utf-8').count('#include "ncmm_graphics_projectiles.h"'),1)
    def test_changed_blocking_wait_anchor_is_atomic(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);sdl=root/'src/sdltiles.cpp'
            sdl.write_text(sdl.read_text()+'\nstatic int input_wait_ms( int changed ) { return changed; }\n');before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_deferred_setting_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);path=root/'src/options.cpp'
            path.write_text(path.read_text().replace('ncmm_register_world_enum(', 'changed_native_registration('))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'four exact Native'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_creature_hook_no_partial_patch(self):
        for name in ("creature.h", "creature.cpp", "ballistics.cpp", "monster.cpp", "npc.cpp"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                (root / "src" / name).write_text("changed creature event pathway")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                    with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_changed_atlas_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            path = root / "src/cata_tiles.cpp"
            path.write_text(path.read_text().replace("ttype.fg.pick( seed )", "ttype.fg.pick( different_seed )"))
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_changed_renderer_teardown_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);path=root/'src/sdltiles.cpp'
            path.write_text(path.read_text().replace('renderer.reset();','renderer.changed_reset();',1));before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'teardown'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_present_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);path=root/'src/sdl_wrappers.cpp'
            path.write_text(path.read_text().replace('SDL_RenderPresent( renderer.get() )','changed_present( renderer.get() )'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))

    def test_changed_fps_native_input_anchors_no_partial_patch(self):
        for name,anchor in [
            ('input.cpp','void input_manager::wait_for_any_key()\n{'),
            ('input.cpp','#include "input.h"'),
            ('debug.cpp','    for( bool stop = false; !stop; ) {'),
            ('debug.cpp','#include "debug.h"'),
            ('input_context.cpp','        if( next_action.type == input_event_t::timeout ) {'),
            ('handle_action.cpp','        if( act == ACTION_INTERACT ) {'),
            ('handle_action.cpp','static void reach_attack( avatar &you )\n{\n'),
            ('handle_action.cpp','player_character.assign_activity( open_tile_activity_actor( std::nullopt ) );'),
            ('sdltiles.cpp','    } else if( ui_adaptor::has_imgui() ) {\n        try_sdl_update();\n    }'),
            ('sdltiles.cpp','    ImGuiIO &io = ImGui::GetIO();\n    if( !mouse.enabled ) {'),
            ('sdltiles.cpp','    if( needupdate ) {\n        try_sdl_update();')]:
            for ambiguous in [False,True]:
                with self.subTest(name=name,anchor=anchor,ambiguous=ambiguous), tempfile.TemporaryDirectory() as tmp:
                    root=Path(tmp);self.fixture(root);path=root/'src'/name
                    source=path.read_text();self.assertEqual(source.count(anchor),1)
                    path.write_text(source.replace(anchor,anchor*2 if ambiguous else 'changed_fps_anchor'))
                    before=self.fingerprint(root)
                    with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                        with self.assertRaisesRegex(ValueError,'FPS input requires one exact anchor'):bridge.apply(root)
                    self.assertEqual(before,self.fingerprint(root))

    def test_native_sprite_path_is_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);path=root/'src/cata_tiles.cpp'
            original='    const texture *sprite_tex = tileset_ptr->get_tile( sprite_index );'
            self.assertEqual(path.read_text().count(original),1)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                bridge.apply(root)
            self.assertEqual(path.read_text().count(original),1)
            self.assertNotIn('ncmm_perspective_prepass',path.read_text())

if __name__ == "__main__": unittest.main()
