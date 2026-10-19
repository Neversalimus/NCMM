"""Verify complete real Native Before captures on the matched old renderer."""
import argparse
from collections import Counter
import hashlib,json,math,struct
from pathlib import Path

BODY=dict(pistol='glock_17',revolver='sw_619',smg='hk_mp5',assault='scar_l',battle='fn_fal_semi',
    pump='remington_870',auto_shotgun='saiga_12',knife='knife_combat',machete='machete',bat='bat',
    fire_axe='fire_ax',sledge='hammer_sledge',crowbar='crowbar',hatchet='hatchet',hammer='hammer',
    shovel='shovel',pickaxe='pickaxe',bottle='bottle_plastic',saw='saw',wrench='wrench',
    screwdriver='screwdriver',pliers='pliers',gas_can='jerrycan',medical='1st_aid_box',spear='spear_steel',flashlight='flashlight')
ACCESSORIES={'red_dot_sight','suppressor','grip','mounted_flashlight'}

def verify(root,expected_device=None):
    root=Path(root);report=json.loads((root/'first-person-held-before-gallery.json').read_text(encoding='utf-8-sig'))
    assert report['success'] and report['actual_cdda_sdl'] and not report['manual_playthrough']
    assert report['production_renderer_source']=='8d24cc2b8df4ccd6574e1f2f61108baf24f4bcf9'
    phases=set(BODY)|ACCESSORIES|{'pump-loaded','pump-empty'};records=report['records']
    assert Counter((r['renderer'],r['name']) for r in records)==Counter({(m,p):1 for m in ['cpu','gpu'] for p in phases})
    images=[]
    for record in records:
        mode,phase=record['renderer'],record['name']
        native=BODY[phase] if phase in BODY else 'remington_870' if phase.startswith('pump-') else 'scar_l'
        assert record['canonical']==native and record['gpu']==(mode=='gpu')
        assert record['attachments']==int(phase in ACCESSORIES)
        assert record['equip_settle_minimum_ms']==280 and math.isfinite(record['capture_after_equip_ms']) and record['capture_after_equip_ms']>=280
        assert math.isfinite(record['capture_presentation_seconds']) and record['capture_presentation_seconds']>=0
        if phase=='pump-loaded':assert record['native_ammo_remaining']>0
        if phase=='pump-empty':assert record['native_ammo_remaining']==0
        if mode=='gpu':
            assert record['backend']=='direct3d12' and record['device']
            if expected_device:assert record['device']==expected_device and not record['software_device']
        name=f'first-person-held-before-{phase}-{mode}.bmp';assert record['capture']==name
        assert (record['width'],record['height'])==(1280,800)
        data=(root/name).read_bytes();assert data[:2]==b'BM' and len(data)>54
        assert struct.unpack_from('<I',data,2)[0]==len(data)
        width,height=struct.unpack_from('<ii',data,18);bits=struct.unpack_from('<H',data,28)[0]
        compression,offset=struct.unpack_from('<I',data,30)[0],struct.unpack_from('<I',data,10)[0]
        assert width==1280 and abs(height)==800 and bits in (24,32) and compression in (0,3)
        assert 54<=offset and offset+800*(((width*bits+31)//32)*4)<=len(data)
        images.append(dict(file=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
    return dict(success=True,native_cases=64,actual_cdda_sdl=True,manual_playthrough=False,
        production_renderer_source=report['production_renderer_source'],images=images,
        limitations=['Matched baseline Host/module and diagnostic harness; does not qualify a new installer or performance',
                     'All capture integrity must be followed by visual Before/After review'])

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--expected-device')
    args=parser.parse_args();result=verify(args.root,args.expected_device)
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='images'}))
