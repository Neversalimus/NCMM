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

PROTOTYPES = {'car','suv','cube_van','semi_truck','bus','pickup','motorcycle','bicycle',
              'quad_bike','apc','truck_trailer','flatbed_truck','shopping_cart','canoe'}
VIEWS = {'front','rear','left','right','front-three-quarter','rear-three-quarter','top-oblique'}


def verify_memory(path, source, engine, module):
    from statistics import median
    report=json.loads(Path(path).read_text(encoding='utf-8-sig'))
    assert report['schema']==1 and report['actual_native']
    assert (report['source_commit'],report['engine_commit'],report['module_sha256'])==(source,engine,module)
    samples=report['samples'];assert len(samples)>=15 and samples[-1]['elapsed_seconds']>=900
    windows='working_set_bytes' in samples[0]
    rss_name,peak_name=('working_set_bytes','peak_working_set_bytes') if windows else ('rss_bytes','peak_rss_bytes')
    rss=[];peaks=[];previous=-1
    for sample in samples:
        at=sample['elapsed_seconds'];assert math.isfinite(at) and at>previous;previous=at
        value,peak=sample[rss_name],sample[peak_name]
        assert type(value) is int and type(peak) is int and 0<value<=peak
        rss.append(value);peaks.append(peak)
    span=max(1,len(samples)//3);baseline=median(rss[:span]);tail=median(rss[-span:])
    # Allow bounded allocator/driver warmup, while rejecting multi-GiB growth
    # hidden by the much smaller module/GPU resource counters.
    tail_limit=baseline+max(256*1024**2,baseline*.5)
    peak_limit=max(baseline*2,baseline+1024**3)
    assert tail<=tail_limit,('Native process resident memory grows after warmup',baseline,tail,tail_limit)
    assert max(peaks)<=peak_limit,('Native process transient memory exceeds the warmed budget',max(peaks),peak_limit)
    return dict(samples=len(samples),baseline_bytes=baseline,tail_bytes=tail,peak_bytes=max(peaks),tail_limit_bytes=tail_limit,peak_limit_bytes=peak_limit)


def verify(root, output, source, engine, module=None):
    from PIL import Image, ImageDraw
    root, output = Path(root), Path(output)
    report = json.loads((root/'first-person-vehicles.json').read_text(encoding='utf-8-sig'))
    assert report['schema']==1 and report['success'] and report['actual_cdda'] and report['native_parts_verified']
    assert not report['manual_playthrough'] and not report['hardware_performance_qualified']
    assert (report['width'],report['height'],report['horizon_radius'])==(640,480,48)
    assert len(source)==40 and len(engine)==40
    assert module is not None and len(module)==64 and all(c in '0123456789abcdef' for c in module)
    memory=verify_memory(root/'first-person-process-memory.json',source,engine,module)
    groups=defaultdict(dict)
    captures=[];benchmarks=defaultdict(list);soak=None;records=set()
    for r in report['frames']:
        if r['label']=='soak-summary':
            assert soak is None;soak=r
            assert 900<=r['requested_seconds']<=1800 and r['elapsed_seconds']>=r['requested_seconds']
            assert r['iterations']>=512 and r['saves']==r['loads']>=2 and r['mutations']>=4
            continue
        identity=r['prototype'],r['label'],r['backend'],r['renderer']
        assert identity not in records,('duplicate vehicle record',identity)
        records.add(identity)
        assert r['backend'] in {'cpu','gpu'} and r['renderer'] in {'legacy','modular'}
        assert r['actual_gpu']==(r['backend']=='gpu')
        for name in ['yaw','pitch','eye_height','full_frame_ms']:
            assert isinstance(r[name],(int,float)) and math.isfinite(r[name])
        assert r['full_frame_ms']>0
        if r['renderer']=='modular' and r['snapshot_parts']:
            assert 0<r['vehicle_submitted_parts']<=r['snapshot_parts'],(r['label'],'Native rows were not rendered')
        if r['label'].startswith('bench-'):
            assert (r['frame_width'],r['frame_height'])==(1280,800)
            scene=r['prototype'];assert scene in {'empty','one','multiple','parking','large'}
            benchmarks[(scene,r['backend'],r['renderer'],'cold' if r['label'].endswith('-cold') else 'warm')].append(r['full_frame_ms'])
        if 'file' not in r:continue
        name=r['file'];assert Path(name).name==name and name.startswith('vehicle-') and name.endswith('.bmp')
        key=r['prototype'],r['label'];variant=r['backend'],r['renderer']
        assert variant not in groups[key]
        groups[key][variant]=r
        path=root/name
        with Image.open(path) as original:
            assert original.size==(r.get('frame_width',640),r.get('frame_height',480)) and original.format=='BMP'
            assert original.size in {(640,480),(1280,800)}
            image=original.convert('RGB');assert image.getextrema()!=((0,0),(0,0),(0,0)),name
            captures.append(dict(file=name,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    for prototype in PROTOTYPES:
        assert {(prototype,v) for v in VIEWS} <= groups.keys(),prototype
        if prototype not in {'shopping_cart','truck_trailer'}:assert (prototype,'driver') in groups,(prototype,'missing Native occupied view')
    assert {('car',v) for v in ['driver','passenger','open-door','damaged','broken-door','broken-wheel','removed-wheel',
                               'installed-armor','night','rain','fog','paint-red','paint-blue']} <= groups.keys()
    assert {('mixed','parking'),('mixed','after-motion'),('mixed','save-reload')} <= groups.keys()
    assert benchmarks and soak is not None,'Full-frame series and completed 900-second Native soak are mandatory'
    if benchmarks:
        for scene in ['empty','one','multiple','parking','large']:
            for backend in ['cpu','gpu']:
                for mode in ['legacy','modular']:
                    assert len(benchmarks[(scene,backend,mode,'cold')])==7
                    assert len(benchmarks[(scene,backend,mode,'warm')])==49
        assert ('custom','asymmetric-extension') in groups and ('custom','turned') in groups
    for (prototype,label),variants in groups.items():
        if label.startswith('moving-') or label.startswith('activity-'):continue
        assert set(variants)=={(b,m) for b in ['cpu','gpu'] for m in ['legacy','modular']}
        inputs={(tuple(r['camera_xyz']),r['yaw'],r['pitch'],r['eye_height']) for r in variants.values()}
        assert len(inputs)==1,(prototype,label,'comparison camera differs')
        for r in variants.values():
            if r['renderer']=='modular' and label not in {'night','night-headlights'}:assert r['snapshot_vehicles'] and r['snapshot_parts']
    output.mkdir(parents=True,exist_ok=False)
    inventory={}
    def sheet(name,keys,w=240,h=180):
        row_h=h+34
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
    angle_keys=[(p,v) for p in sorted(PROTOTYPES) for v in sorted(VIEWS)]
    for offset in range(0,len(angle_keys),12):sheet('vehicle-angles-'+str(offset//12)+'.png',angle_keys[offset:offset+12],200,150)
    sheet('vehicle-interiors.png',sorted(k for k in groups if k[1] in {'driver','passenger'}))
    car_keys=sorted(k for k in groups if k[0]=='car' and not k[1].startswith(('moving-','activity-')))
    for offset in range(0,len(car_keys),12):sheet('vehicle-details-'+str(offset//12)+'.png',car_keys[offset:offset+12])
    sheet('vehicle-parking.png',[k for k in sorted(groups) if k[0]=='mixed' and not k[1].startswith('moving-')])
    if benchmarks:sheet('vehicle-custom.png',[k for k in sorted(groups) if k[0]=='custom'])
    def percentile(values,fraction):
        values=sorted(values);return values[round((len(values)-1)*fraction)]
    performance=[]
    for key,values in sorted(benchmarks.items()):
        scene,backend,mode,phase=key
        performance.append(dict(scene=scene,backend=backend,renderer=mode,phase=phase,samples=len(values),
                                median_ms=percentile(values,.5),p95_ms=percentile(values,.95),p99_ms=percentile(values,.99),max_ms=max(values)))
    result=dict(schema=1,success=True,source=source,engine_source=engine,module_sha256=module,
                actual_cdda=True,manual_playthrough=False,visual_acceptance=False,hardware_performance_qualified=False,
                comparable_configurations=len(groups),captured_frames=len(captures),
                capture_inventory=captures,review_inventory=inventory,
                performance=performance,soak=soak,process_memory=memory,cold_definition='Native cache refresh and first frame after switching the vehicle renderer; device resources may be retained',
                limitations=['Automated Native capture integrity does not establish visual acceptance.',
                             '640x480 visual captures and 1280x800 full-frame timings on CI adapters do not establish hardware FPS.',
                             'Interior comparisons pin the same actual Native seat eye across both renderers.'])
    (output/'vehicle-review.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path);p.add_argument('--source',required=True);p.add_argument('--engine',required=True);p.add_argument('--module-sha256')
    args=p.parse_args();result=verify(args.root,args.output,args.source,args.engine,args.module_sha256)
    print(json.dumps({k:v for k,v in result.items() if k not in {'capture_inventory','review_inventory'}}))
