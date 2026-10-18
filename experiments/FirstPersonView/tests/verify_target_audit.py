"""Validate bounded native target/HUD captures; visual review remains separate."""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct


PHASES = {'full', 'damaged', 'cyrillic-long', 'disabled-name-only', 'replaced-same-cell',
          'dead', 'opaque-wall', 'window-mullion', 'glass', 'full-height-glass', 'friendly-name-only', 'lost-aim', 'night', 'npc-native-summary', 'npc-damaged'}
HIDDEN = {'dead', 'opaque-wall', 'window-mullion', 'lost-aim'}


def verify(root):
    root = Path(root).resolve()
    report = json.loads((root/'first-person-target-audit.json').read_text(encoding='utf-8-sig'))
    assert report['schema'] == 1 and report['success'] and report['actual_cdda_sdl'] and not report['manual_playthrough']
    cases = report['cases']
    assert Counter((c['renderer'], c['phase']) for c in cases) == Counter({(m, p): 1 for m in ['cpu', 'gpu'] for p in PHASES})
    images = []
    for case in cases:
        phase, mode = case['phase'], case['renderer']
        selected = phase not in HIDDEN
        hostile = selected and phase != 'friendly-name-only'
        assert case['selected'] == selected and case['hostile'] == hostile and case['known'] == hostile
        assert case['gpu'] == (mode == 'gpu'), 'Hidden CPU fallback in GPU HUD evidence'
        assert math.isfinite(case['fraction'])
        if selected:
            assert case['entity'] > 0 and case['world'] > 0 and case['frame'] > 0
        else:
            assert case['entity'] == case['world'] == case['frame'] == 0
        if hostile:
            assert case['maximum'] > 0 and 0 <= case['hp'] <= case['maximum']
            assert abs(case['fraction']-case['hp']/case['maximum']) < .001
        else:
            assert case['fraction'] == 0
        name = f'first-person-audit-target-hud-{phase}-{mode}.bmp'
        assert case['file'] == name
        path = root/name
        assert path.resolve().parent == root
        data = path.read_bytes()
        assert 54 <= len(data) <= 256*1024*1024 and data[:2] == b'BM'
        assert struct.unpack_from('<I', data, 2)[0] == len(data)
        width, height = struct.unpack_from('<ii', data, 18)
        bits = struct.unpack_from('<H', data, 28)[0]
        compression = struct.unpack_from('<I', data, 30)[0]
        offset = struct.unpack_from('<I', data, 10)[0]
        assert 0 < width <= 8192 and 0 < abs(height) <= 8192 and bits in (24, 32) and compression in (0, 3)
        assert 54 <= offset <= len(data) and offset+abs(height)*(((width*bits+31)//32)*4) <= len(data)
        images.append({'file': name, 'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)})
    for mode in ['cpu', 'gpu']:
        by_phase = {c['phase']: c for c in cases if c['renderer'] == mode}
        original = by_phase['full']['entity']
        assert all(by_phase[p]['entity'] == original for p in ['damaged', 'cyrillic-long', 'disabled-name-only'])
        assert by_phase['replaced-same-cell']['entity'] != original
        assert by_phase['full']['fraction'] == 1 and 0 < by_phase['damaged']['fraction'] < 1
    return {'success': True, 'actual_cdda_sdl': True, 'manual_playthrough': False,
            'native_cases': len(cases), 'images': images,
            'limitations': ['Controlled native HP changes and targets; actual firing is qualified separately',
                            'Capture integrity and reported state do not replace visual review']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.root)
    args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'images'}))
