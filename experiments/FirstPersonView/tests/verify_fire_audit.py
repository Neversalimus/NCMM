"""Verify real native command/UI attempts and every successful presented PNG."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import zlib


def verify_png(path, width, height):
    data = path.read_bytes()
    assert len(data) <= 32 * 1024 * 1024 and data[:8] == b'\x89PNG\r\n\x1a\n'
    offset, packed, header, ended = 8, bytearray(), None, False
    while offset + 12 <= len(data):
        length = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset + 4:offset + 8]
        end = offset + 8 + length
        assert end + 4 <= len(data)
        payload = data[offset + 8:end]
        assert zlib.crc32(kind + payload) & 0xffffffff == struct.unpack_from('>I', data, end)[0]
        if kind == b'IHDR':
            assert header is None and length == 13
            header = struct.unpack('>IIBBBBB', payload)
        elif kind == b'IDAT':
            packed.extend(payload)
        elif kind == b'IEND':
            assert length == 0 and end + 4 == len(data)
            ended = True
        offset = end + 4
    assert ended and header is not None and packed
    w, h, depth, color, compression, filtering, interlace = header
    assert (w, h) == (width, height) and 0 < w <= 8192 and 0 < h <= 8192
    assert depth == 8 and color in (2, 6) and (compression, filtering, interlace) == (0, 0, 0)
    expected = h * (1 + w * (3 if color == 2 else 4))
    decoder = zlib.decompressobj()
    pixels = decoder.decompress(packed, expected + 1)
    assert len(pixels) == expected and decoder.eof and not decoder.unconsumed_tail
    return hashlib.sha256(data).hexdigest()


def verify(root):
    root = Path(root).resolve()
    report = json.loads((root / 'first-person-fire-audit.json').read_text(encoding='utf-8-sig'))
    assert report['schema'] == 1 and report['success'] and report['actual_cdda_sdl']
    assert not report['manual_playthrough'] and not report['synthetic_attack_notifications']
    assert report['normal_game_command_dispatch'] and report['native_activity_and_target_ui']
    assert not report['ordinary_ai_turns']
    trials, frames = report['trials'], report['frames']
    controls = [t for t in trials if t['legacy_control']]
    measured = [t for t in trials if not t['legacy_control']]
    common = {'keyboard_single', 'lmb_single', 'cancel', 'empty', 'no_target', 'window'}
    extra = {'blocked_wall', 'vertical_cancel', 'burst'}
    expected = Counter({(mode, name): 10 for mode in ['cpu', 'gpu'] for name in common})
    expected.update({(mode, name): 1 for mode in ['cpu', 'gpu'] for name in extra})
    assert Counter((t['renderer'], t['scenario']) for t in measured) == expected
    assert len(measured) >= 100 and len(controls) == 1 and controls[0]['scenario'] == 'cancel'
    assert frames and all(f['captured'] and f['presented'] for f in frames)
    manifest = []
    for i, frame in enumerate(frames):
        assert frame['file'] == f'first-person-fire-frame-{i}.png'
        path = root / frame['file']
        assert path.resolve().parent == root
        digest = verify_png(path, frame['width'], frame['height'])
        manifest.append({'file': frame['file'], 'sha256': digest, 'bytes': path.stat().st_size})
    cursor, shots = 0, 0
    for trial in trials:
        first, last = trial['first_frame'], trial['last_frame_exclusive']
        assert first == cursor and first < last <= len(frames)
        cursor = last
        selected = frames[first:last]
        if trial['legacy_control']:
            assert any(not f['perspective'] for f in selected), 'Original regression was not reproduced'
            continue
        assert trial['enabled_before'] and trial['enabled_after']
        assert trial['yaw_before'] == trial['yaw_after'] and trial['pitch_before'] == trial['pitch_after']
        assert all(f['enabled'] and f['perspective'] and f['gpu'] == (trial['renderer'] == 'gpu') for f in selected)
        delta = trial['attack_serial_after'] - trial['attack_serial_before']
        spent = trial['ammo_before'] - trial['ammo_after']
        if trial['scenario'] in {'cancel', 'empty', 'no_target', 'blocked_wall', 'vertical_cancel'}:
            assert delta == spent == 0
        else:
            assert delta > 0 and spent > 0 and trial['moves_after'] < trial['moves_before']
            assert not trial['escape_recovery'], 'Required firing path needed escape watchdog'
            shots += delta
    assert cursor == len(frames) and shots >= 60
    return {'success': True, 'actual_cdda_sdl': True, 'manual_playthrough': False,
            'normal_native_ui_attempts': len(measured), 'confirmed_shots': shots,
            'original_context_regression_reproduced': True, 'presented_images': len(frames),
            'images': manifest, 'ordinary_ai_turns': False,
            'limitations': ['LMB fixture holds RMB look; pointer-only LMB needs a separate case',
                            'Vertical case verifies cancellation, not a successful vertical shot',
                            'Controlled automation is not a manual playthrough']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.root)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'images'}))
