#!/usr/bin/env python3
"""Exercise Item Glyphs patch contracts on a clean CDDA 0546 checkout.

Run: python3 ci/test_item_glyphs_source.py --source-root /path/to/cdda --pwsh pwsh
All patching happens in a temporary copy. No item JSON or caller checkout is changed.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--pwsh', default='pwsh')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    source = args.source_root.resolve() / 'src'
    require(digest(source / 'wcwidth.cpp') ==
            '859d21c69ad9ce1c33e7549beead830c7859089d1cee998eaaafc5020068c12a',
            'Expected original CDDA 0546 wcwidth.cpp')
    old_prefix = '''        if( get_option<bool>( "ITEM_SYMBOLS" ) ) {
            item_name = string_format( "%s %s", it.symbol(), item_name );
        }'''
    new_prefix = '''        if( ncmm::inventory_symbols_enabled( get_option<bool>( "ITEM_SYMBOLS" ) ) ) {
            item_name = string_format( "%s %s", ncmm::inventory_item_symbol( it ), item_name );
        }'''
    gate = 'ncmm::inventory_symbols_enabled( get_option<bool>( "ITEM_SYMBOLS" ) )'
    with tempfile.TemporaryDirectory(prefix='ncmm-item-glyphs-') as temp:
        temp = Path(temp)
        root = temp / 'upstream'
        shutil.copytree(source, root / 'src')
        env = os.environ.copy()
        for kind in ('CACHE', 'CONFIG', 'DATA'):
            env[f'XDG_{kind}_HOME'] = str(temp / kind.lower())
        env['TEMP'] = str(temp)

        def patch(success=True):
            result = subprocess.run([args.pwsh, '-NoProfile', '-File',
                                     str(repo / 'host_patch/Apply-NCMMHostPatch.ps1'),
                                     '-SourceRoot', str(root)], env=env, capture_output=True, text=True)
            require((result.returncode == 0) == success, result.stdout + result.stderr)
            return result.stdout + result.stderr

        def snapshot():
            return {p.name: digest(p) for p in (root / 'src').iterdir() if p.is_file()}

        patch()
        advanced = (root / 'src/advanced_inv.cpp').read_text()
        original_advanced = (source / 'advanced_inv.cpp').read_text()
        expected = original_advanced.replace('#include "advanced_inv.h"',
                                             '#include "advanced_inv.h"\n#include "ncmm_loader.h"')
        require(expected.count(old_prefix) == 1, 'AIM anchor must occur exactly once')
        expected = expected.replace(old_prefix, new_prefix)
        require(advanced == expected, 'AIM changed beyond include and existing symbol prefix')
        inventory = (root / 'src/inventory_ui.cpp').read_text()
        require(inventory.count(gate) == 2, 'Inventory indent and draw must share the enable gate')
        require('mvwputch( win, point( xx, yy ), color, ncmm::inventory_item_symbol( *entry.any_item() ) );'
                in inventory, 'Inventory must retain item color in existing symbol slot')
        require('const nc_color color = entry.any_item()->color();' in inventory,
                'Inventory item color changed')
        require('res += 2;' in inventory and 'xx += 2;' in inventory, 'Two-cell slot missing')
        require('class trade_selector : public inventory_drop_selector' in
                (source / 'trade_ui.h').read_text(), 'Trade must inherit inventory selector')
        require((root / 'src/trade_ui.h').read_bytes() == (source / 'trade_ui.h').read_bytes(),
                'Trade-specific patch is forbidden')
        require('ncmm_item_glyphs.h' in snapshot(), 'Host classifier was not copied')
        print('PASS clean patch; Inventory/Trade slot wiring; exact AIM diff preserves marker/trim/sort')
        first = snapshot()
        patch()
        require(snapshot() == first, 'Existing-marker path must preserve source bytes')
        print('PASS existing marker and idempotent source bytes')
        aim = root / 'src/advanced_inv.cpp'
        aim.write_text(advanced.replace(gate, 'get_option<bool>( "ITEM_SYMBOLS" )'))
        before = snapshot()
        require('Existing NCMM marker' in patch(False), 'Corrupt marker contract must be rejected')
        require(snapshot() == before, 'Rejected marker modified source')
        print('PASS corrupted marker fails closed')
        (root / '.ncmm_host_v1_patched').unlink()
        # Restore pristine files, then duplicate an exact anchor. No source write
        # is permitted when Replace-ExactlyOnce detects ambiguity.
        shutil.copytree(source, root / 'src', dirs_exist_ok=True)
        aim.write_text(original_advanced + '\n' + old_prefix + '\n')
        before = snapshot()
        require('advanced-inventory.item-glyphs-prefix' in patch(False),
                'Duplicated AIM anchor must fail its exact replacement contract')
        require(snapshot() == before, 'Ambiguous anchor modified source')
        require(not (root / '.ncmm_host_v1_patched').exists(), 'Failed patch wrote marker')
        print('PASS duplicate anchor fails closed before writing source')
        aim.unlink()
        require('Required source file missing' in patch(False), 'AIM must be a required file')
        print('PASS Advanced Inventory required-file guard')


if __name__ == '__main__':
    main()
