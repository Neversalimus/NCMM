"""Verify actual Native records and create bounded lossless review sheets.

This establishes provenance and comparable captures, not aesthetic acceptance.
The full unscaled PNGs remain in CI; only named sheets enter the review archive.
"""
import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw

PROTOTYPES = {'car','suv','cube_van','semi_truck','bus','pickup','motorcycle','bicycle',
              'quad_bike','apc','truck_trailer','flatbed_truck','shopping_cart','canoe'}
VIEWS = {'front','rear','left','right','front-three-quarter','rear-three-quarter','top-oblique'}


def verify(root, output, source, engine, module=None):
    root, output = Path(root), Path(output)
    report = json.loads((root/'first-person-vehicles.json').read_text(encoding='utf-8-sig'))
    assert report['schema']==1 and report['success'] and report['actual_cdda'] and report['native_parts_verified']
    assert not report['manual_playthrough'] and not report['hardware_performance_qualified']
    assert (report['width'],report['height'],report['horizon_radius'])==(640,480,48)
    assert len(source)==40 and len(engine)==40
    groups=defaultdict(dict)
    captures=[]
    for r in report['frames']:
        assert r['backend'] in {'cpu','gpu'} and r['renderer'] in {'legacy','modular'}
        assert r['actual_gpu']==(r['backend']=='gpu')
        for name in ['yaw','pitch','eye_height','full_frame_ms']:
            assert isinstance(r[name],(int,float)) and math.isfinite(r[name])
        assert r['full_frame_ms']>0
        if 'file' not in r:continue
        name=r['file'];assert Path(name).name==name and name.startswith('vehicle-') and name.endswith('.bmp')
        key=r['prototype'],r['label'];variant=r['backend'],r['renderer']
        assert variant not in groups[key]
        groups[key][variant]=r
        path=root/name
        with Image.open(path) as original:
            assert original.size==(640,480) and original.format=='BMP'
            image=original.convert('RGB');assert image.getextrema()!=((0,0),(0,0),(0,0)),name
            captures.append(dict(file=name,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    for prototype in PROTOTYPES:
        assert {(prototype,v) for v in VIEWS} <= groups.keys(),prototype
    assert {('car',v) for v in ['driver','passenger','open-door','damaged','broken-door','removed-wheel',
                               'installed-armor','night','rain','fog']} <= groups.keys()
    assert {('mixed','parking'),('mixed','after-motion'),('mixed','save-reload')} <= groups.keys()
    for (prototype,label),variants in groups.items():
        if label.startswith('moving-'):continue
        assert set(variants)=={(b,m) for b in ['cpu','gpu'] for m in ['legacy','modular']}
        inputs={(tuple(r['camera_xyz']),r['yaw'],r['pitch']) for r in variants.values()}
        assert len(inputs)==1,(prototype,label,'comparison camera differs')
        for r in variants.values():
            if r['renderer']=='modular':assert r['snapshot_vehicles'] and r['snapshot_parts']
    output.mkdir(parents=True,exist_ok=False)
    inventory={}
    def sheet(name,keys):
        w,h=240,180;row_h=h+34
        page=Image.new('RGB',(w*4,row_h*len(keys)),(22,24,28));draw=ImageDraw.Draw(page)
        for row,key in enumerate(keys):
            for column,variant in enumerate((('cpu','legacy'),('cpu','modular'),('gpu','legacy'),('gpu','modular'))):
                r=groups[key][variant]
                with Image.open(root/r['file']) as original:
                    thumbnail=original.convert('RGB').resize((w,h),Image.Resampling.LANCZOS)
                page.paste(thumbnail,(column*w,row*row_h+34));draw.text((column*w+3,row*row_h+2),key[0]+' / '+key[1],fill='white')
                draw.text((column*w+3,row*row_h+17),' / '.join(variant),fill=(166,207,226))
        path=output/name;page.save(path,optimize=True)
        assert path.stat().st_size<=4*1024*1024
        inventory[name]=dict(bytes=path.stat().st_size,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),width=page.width,height=page.height)
    sheet('vehicle-exteriors.png',[(p,'front-three-quarter') for p in sorted(PROTOTYPES)])
    car_keys=sorted(k for k in groups if k[0]=='car' and not k[1].startswith('moving-'))
    for offset in range(0,len(car_keys),12):sheet('vehicle-details-'+str(offset//12)+'.png',car_keys[offset:offset+12])
    sheet('vehicle-parking.png',[k for k in sorted(groups) if k[0]=='mixed' and not k[1].startswith('moving-')])
    result=dict(schema=1,success=True,source=source,engine_source=engine,module_sha256=module,
                actual_cdda=True,manual_playthrough=False,visual_acceptance=False,hardware_performance_qualified=False,
                comparable_configurations=len(groups),captured_frames=len(captures),
                capture_inventory=captures,review_inventory=inventory,
                limitations=['Automated Native capture integrity does not establish visual acceptance.',
                             '640x480 on CI adapters is a functional comparison, not hardware FPS.',
                             'Interior camera intentionally changes from Legacy clamp to actual seat.'])
    (output/'vehicle-review.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path);p.add_argument('--source',required=True);p.add_argument('--engine',required=True)
    args=p.parse_args();result=verify(args.root,args.output,args.source,args.engine)
    print(json.dumps({k:v for k,v in result.items() if k not in {'capture_inventory','review_inventory'}}))
