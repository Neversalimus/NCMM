"""Verify Native gallery records, exact compiled model IDs and lossless captures.

Capture integrity and recorded Native state do not replace visual inspection.
This validator does not qualify an installer or measure ordinary frame times.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct

BODY = dict(pistol='glock_17', revolver='sw_619', smg='hk_mp5', assault='scar_l',
    battle='fn_fal_semi', pump='remington_870', auto_shotgun='saiga_12', knife='knife_combat',
    machete='machete', bat='bat', fire_axe='fire_ax', sledge='hammer_sledge', crowbar='crowbar',
    hatchet='hatchet', hammer='hammer', shovel='shovel', pickaxe='pickaxe', bottle='bottle_plastic',
    saw='saw', wrench='wrench', screwdriver='screwdriver', pliers='pliers', gas_can='jerrycan',
    medical='1st_aid_box', spear='spear_steel', flashlight='flashlight')
ACCESSORIES = dict(red_dot_sight='red_dot', suppressor='suppressor', grip='foregrip', mounted_flashlight='gun_light')
PHASES = set(BODY) | set(ACCESSORIES) | {'pump-loaded', 'pump-empty'}
DEFAULT_MANIFEST = Path(__file__).resolve().parents[1]/'assets/held_items/generated/manifest.json'


def verify(root, manifest=DEFAULT_MANIFEST, expected_device=None, expected_backend='direct3d12', *, require_timing=True):
    root = Path(root).resolve()
    imported = json.loads(Path(manifest).read_text(encoding='utf-8-sig'))
    if 'imported' in imported:
        imported = imported['imported']
    identities = {m['id']:m['identity'] for m in imported['models']}
    assert len(identities) == 30 and len(set(identities.values())) == 30
    report = json.loads((root/'first-person-held-gallery.json').read_text(encoding='utf-8-sig'))
    assert report['schema'] == 1 and report['success'] and report['actual_cdda_sdl'] and not report['manual_playthrough']
    records = report['records']
    assert Counter((r['renderer'],r['name']) for r in records) == Counter({(m,p):1 for m in ['cpu','gpu'] for p in PHASES})
    images = []; selected_models = set(); devices = set(); software_devices = set(); peak = 0
    for r in records:
        if require_timing:
            first=r['first_render_after_equip_ms'];settled=r['settled_render_samples_ms']
            assert type(first) in (int,float) and math.isfinite(first) and first>0
            assert len(settled)==2 and all(type(v) in (int,float) and math.isfinite(v) and v>0 for v in settled)
            assert r['equip_settle_minimum_ms']==280
            assert all(type(r[k]) in (int,float) and math.isfinite(r[k]) for k in ['capture_after_equip_ms','capture_presentation_seconds'])
            assert r['capture_after_equip_ms']+.01>=first+280+sum(settled)
            assert math.isfinite(r['capture_presentation_seconds']) and r['capture_presentation_seconds']>=0
            assert all(type(r[k]) is int and r[k]>=0 for k in ['first_static_builds','first_static_reuses',
                'first_gpu_upload_bytes','first_gpu_static_upload_bytes','first_gpu_resource_bytes'])
        phase, mode = r['name'], r['renderer']
        if require_timing:assert r['first_gpu_frame']==(mode=='gpu')
        model = phase if phase in BODY else 'pump' if phase.startswith('pump-') else 'assault'
        native = BODY[model]
        assert r['canonical'] == native and r['model'] == model and r['identity'] == identities[model]
        assert r['matched'] == 1 and r['fallback_reason'] == 0 and r['gpu'] == (mode == 'gpu')
        attachments = int(phase in ACCESSORIES)
        assert r['attachments'] == attachments and r['draws'] >= 1+attachments and 0 < r['pose_matrices'] <= 4096
        assert r['ammo_loaded'] == bool(r['native_is_gun'] and r['native_ammo_remaining'] > 0)
        expected_accessories = [ACCESSORIES[phase]] if attachments else []
        assert r['accessory_models'] == expected_accessories
        assert r['accessory_identities'] == [identities[m] for m in expected_accessories]
        selected_models.update([model]+expected_accessories)
        if mode == 'gpu':
            assert r['static_builds'] == 0 and r['static_reuses'] >= attachments+1
            assert r['static_mesh_upload_bytes'] == r['dynamic_geometry_bytes'] == 0
            assert r['gpu_draw_calls'] > 0 and r['backend'] == expected_backend and r['device']
            if expected_device:
                assert not r['software_device'] and r['device'] == expected_device
            devices.add(r['device']);software_devices.add(r['software_device']);peak=max(peak,r['gpu_resource_bytes'])
        name = f'first-person-held-{phase}-{mode}.bmp'
        assert r['capture'] == name and r['width'] == 1280 and r['height'] == 800
        path = root/name
        assert path.resolve().parent == root
        data = path.read_bytes()
        assert data[:2] == b'BM' and len(data) >= 54
        assert struct.unpack_from('<I',data,2)[0] == len(data)
        width,height = struct.unpack_from('<ii',data,18)
        bits = struct.unpack_from('<H',data,28)[0]
        compression,offset = struct.unpack_from('<I',data,30)[0],struct.unpack_from('<I',data,10)[0]
        assert width == 1280 and abs(height) == 800 and bits in (24,32) and compression in (0,3)
        assert 54 <= offset and offset+abs(height)*(((width*bits+31)//32)*4) <= len(data)
        images.append(dict(file=name,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)))
    assert selected_models == set(identities)
    for mode in ['cpu','gpu']:
        by_name = {r['name']:r for r in records if r['renderer'] == mode}
        loaded,empty = by_name['pump-loaded'],by_name['pump-empty']
        assert loaded['ammo_loaded'] and loaded['native_ammo_remaining'] > 0
        assert not empty['ammo_loaded'] and empty['native_ammo_remaining'] == 0
        # CPU records one model submission and skips hidden triangles within
        # that submission. GPU records the separate authored part batches.
        if mode == 'gpu':
            assert empty['draws'] < loaded['draws'], 'Empty Native ammunition must hide actual authored round geometry'
    return dict(success=True,actual_cdda_sdl=True,manual_playthrough=False,ordinary_performance_measurement=False,
        native_cases=len(records),compiled_models=len(selected_models),cpu_cases=32,gpu_cases=32,
        devices=sorted(devices),software_devices=sorted(software_devices),gpu_resource_peak_bytes=peak,
        manifest_sha256=hashlib.sha256(Path(manifest).read_bytes()).hexdigest(),images=images,
        equip_timing_verified=require_timing,
        limitations=['Controlled Native gallery, not a manual equip/combat session',
            'Capture bytes and recorded state require separate visual review',
            'Does not establish installer provenance or final unchanged-binary qualification'])


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--manifest',type=Path,default=DEFAULT_MANIFEST)
    parser.add_argument('--expected-device')
    parser.add_argument('--expected-backend',choices=['direct3d12','vulkan'],default='direct3d12')
    parser.add_argument('--allow-historical-without-timing',action='store_true')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    result=verify(args.root,args.manifest,args.expected_device,args.expected_backend,require_timing=not args.allow_historical_without_timing)
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='images'}))
