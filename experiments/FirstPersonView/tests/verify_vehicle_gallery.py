"""Verify actual Native records and create bounded lossless review sheets.

This establishes provenance and comparable captures, not aesthetic acceptance.
The full unscaled PNGs remain in CI; only named sheets enter the review archive.
"""
import argparse
from collections import defaultdict
import hashlib
import json
import math
import re
from pathlib import Path

PROTOTYPES = {'car','suv','cube_van','semi_truck','bus','pickup','motorcycle','bicycle',
              'quad_bike','apc','truck_trailer','flatbed_truck','shopping_cart','canoe'}
VIEWS = {'front','rear','left','right','front-three-quarter','rear-three-quarter','top-oblique'}

def verify_cpu_metrics(r):
    assert (r['cpu_width'],r['cpu_height'])==(r.get('frame_width',640),r.get('frame_height',480)), 'CPU resolution differs from the Native viewport'
    for name in ('cpu_callback_ms','cpu_geometry_ms','cpu_raster_ms','cpu_effects_ms'):
        assert type(r[name]) in (int,float) and math.isfinite(r[name]) and r[name]>=0,name
    assert 0<r['cpu_callback_ms']<=r['full_frame_ms']+.01,'CPU callback must belong to the actual full frame'
    for name in ('cpu_raster_calls','cpu_triangles'):assert type(r[name]) is int and r[name]>=0,name

def verify_mutation_views(groups):
    """Same actual camera across the before/state-change/after sequence."""
    variants={('cpu','legacy'),('cpu','modular'),('gpu','legacy'),('gpu','modular')}
    for sequence in [('mutation-before','open-door','closed-door','damaged','broken-door'),
                     ('wheel-before','broken-wheel','removed-wheel')]:
        for label in sequence:
            assert ('car',label) in groups,('mutation capture missing',label)
            assert set(groups[('car',label)])==variants,('mutation capture matrix incomplete',label)
        for variant in variants:
            before=groups[('car',sequence[0])][variant]
            for label in sequence[1:]:
                after=groups[('car',label)][variant]
                assert all(after[k]==before[k] for k in ('camera_xyz','yaw','pitch','eye_height')),('mutation camera changed',label,variant)



def verify_fixture_camera(row):
    """Published provider camera must match the requested fixture, not just its peer."""
    for key in ('yaw','pitch','requested_yaw','requested_pitch'):
        assert type(row[key]) in (int,float) and math.isfinite(row[key]),('invalid requested/published camera',key)
    assert abs(row['requested_pitch'])<=1.2, 'Fixture requested an unrepresentable provider pitch'
    assert abs(math.remainder(row['yaw']-row['requested_yaw'],math.tau))<=.000001, 'Provider yaw differs from requested fixture'
    assert abs(row['pitch']-row['requested_pitch'])<=.000001, 'Provider pitch differs from requested fixture'
    if row['label'] in {'driver','passenger','driver-controls'}:
        target=-.95 if row['label']=='driver-controls' else -.04
        assert abs(row['requested_pitch']-target)<=.000001, 'Occupied fixture intent is not its prescribed view'


def fixture_camera_self_test():
    import copy
    good=dict(label='passenger',yaw=0.,pitch=-.04,requested_yaw=0.,requested_pitch=-.04)
    verify_fixture_camera(good)
    verify_fixture_camera(dict(good,label='driver-controls',pitch=-.95,requested_pitch=-.95))
    verify_fixture_camera(dict(good,yaw=-math.pi,requested_yaw=math.pi))
    mutations=[lambda r:r.update(pitch=-.95),lambda r:r.update(requested_pitch=-.95,pitch=-.95),
               lambda r:r.update(yaw=.01),lambda r:r.update(requested_yaw=.01),
               lambda r:r.update(pitch=True),lambda r:r.update(requested_pitch=True),
               lambda r:r.update(yaw=float('nan')),lambda r:r.update(requested_yaw=float('inf')),
               lambda r:r.pop('requested_pitch'),lambda r:r.pop('requested_yaw'),
               lambda r:r.update(label='driver-controls'),lambda r:r.update(requested_pitch=1.3,pitch=1.3)]
    for mutate in mutations:
        row=copy.deepcopy(good);mutate(row)
        try:verify_fixture_camera(row)
        except (AssertionError,KeyError):pass
        else:raise AssertionError('Invalid fixture camera admitted')
    return dict(success=True,pure_controls=3+len(mutations),local_GPU_Native_raster_or_game_execution=False)



def verify_native_frame_phases(row):
    """Five disjoint wall intervals must exhaust this same actual Native frame."""
    phases=row['native_frame_phases_ms']
    assert type(phases) is dict and set(phases)=={'prepare','tile_draw','view_draw','flush_present','finish'}, 'Incomplete Native wall phase partition'
    for name,value in phases.items():
        assert type(value) in (int,float) and math.isfinite(value) and value>=0,('Invalid Native wall phase',name)
    total=row['full_frame_ms']
    assert type(total) in (int,float) and math.isfinite(total) and total>0
    assert math.isclose(sum(phases.values()),total,rel_tol=0.000001,abs_tol=0.00001), 'Native wall phase partition omits or double counts complete frame time'


def native_frame_phases_self_test():
    import copy
    good=dict(full_frame_ms=10.,native_frame_phases_ms=dict(prepare=2.,tile_draw=3.,view_draw=4.,flush_present=.5,finish=.5))
    verify_native_frame_phases(good)
    verify_native_frame_phases(dict(full_frame_ms=10.,native_frame_phases_ms=dict(prepare=0.,tile_draw=0.,view_draw=10.,flush_present=0.,finish=0.)))
    mutations=[lambda r:r.pop('native_frame_phases_ms'),lambda r:r['native_frame_phases_ms'].pop('finish'),
               lambda r:r['native_frame_phases_ms'].update(extra=0.),lambda r:r['native_frame_phases_ms'].update(view_draw=5.),
               lambda r:r['native_frame_phases_ms'].update(view_draw=3.),lambda r:r['native_frame_phases_ms'].update(prepare=-1.),
               lambda r:r['native_frame_phases_ms'].update(prepare=True),lambda r:r['native_frame_phases_ms'].update(prepare=float('nan')),
               lambda r:r['native_frame_phases_ms'].update(finish=float('inf')),lambda r:r.update(full_frame_ms=0.),
               lambda r:r.update(full_frame_ms=True),lambda r:r.update(full_frame_ms=float('nan'))]
    for mutate in mutations:
        row=copy.deepcopy(good);mutate(row)
        try:verify_native_frame_phases(row)
        except (AssertionError,KeyError):pass
        else:raise AssertionError('Incomplete or double-counted Native wall phases admitted')
    return dict(success=True,pure_controls=2+len(mutations),local_GPU_Native_raster_or_game_execution=False)



def verify_aisle_floor_views(groups):
    """The extra floor view uses the actual adjacent aisle and existing seat."""
    variants={(b,m) for b in ['cpu','gpu'] for m in ['legacy','modular']}
    assert ('bus','aisle-floor') in groups, 'Actual Native aisle view is missing'
    rows=groups['bus','aisle-floor'];seats=groups['bus','passenger']
    assert set(rows)==variants and set(seats)==variants, 'Incomplete actual aisle/seat matrix'
    identities=set()
    for variant,r in rows.items():
        seat=seats[variant]
        assert r['prototype']=='bus' and r['label']=='aisle-floor' and (r['backend'],r['renderer'])==variant
        assert all(r[k]==seat[k] for k in ('camera_xyz','eye_height')), 'Aisle view moved the real occupied eye'
        target=r['aisle_native_target']
        assert type(target['part_id']) is int and target['part_id']>0
        mount,delta=target['mount'],target['delta_xy']
        assert type(mount) is list and len(mount)==3 and all(type(x) is int for x in mount) and mount[2]==0
        assert type(delta) is list and len(delta)==2 and all(type(x) is int for x in delta) and sum(abs(x) for x in delta)==1
        assert abs(math.remainder(r['requested_yaw']-math.atan2(delta[1],delta[0]),math.tau))<=.000001
        assert abs(r['requested_pitch']+.75)<=.000001
        verify_fixture_camera(r)
        identities.add((target['part_id'],tuple(mount),tuple(delta)))
        if variant[1]=='modular':
            assert r['occupied_vehicle'] and r['occupied_part'] and r['occupied_eye_valid']
            assert all(r[k]==seat[k] for k in ('occupied_vehicle','occupied_part','occupied_eye_valid','occupied_eye_xy'))
            published=target['published']
            assert published['part_id']==target['part_id'] and published['mount']==mount and published['id']=='aisle'
            assert type(published['features']) is int and published['features']==(16384|524288)
            assert type(published['state']) is int and published['state']&1 and not published['state']&128
            assert type(published['cell_index']) is int and published['cell_index']>=0
    assert len(identities)==1, 'Aisle part identity or real world direction changed between renderers'


def aisle_floor_view_self_test():
    import copy
    rows={};seats={}
    for b in ['cpu','gpu']:
        for m in ['legacy','modular']:
            seat=dict(camera_xyz=[10,20,0],eye_height=1.4548,occupied_vehicle=7,occupied_part=9,occupied_eye_valid=True,occupied_eye_xy=[1.,2.])
            r=dict(seat,prototype='bus',label='aisle-floor',backend=b,renderer=m,yaw=-math.pi/2,pitch=-.75,requested_yaw=-math.pi/2,requested_pitch=-.75,
                   aisle_native_target=dict(part_id=14,mount=[-1,1,0],delta_xy=[0,-1]))
            if m=='modular':r['aisle_native_target']['published']=dict(part_id=14,mount=[-1,1,0],id='aisle',features=16384|524288,state=33,cell_index=4)
            rows[b,m]=r;seats[b,m]=seat
    good={('bus','aisle-floor'):rows,('bus','passenger'):seats};verify_aisle_floor_views(good)
    def current(g):return g['bus','aisle-floor']['gpu','modular']
    mutations=[lambda g:g.pop(('bus','aisle-floor')),lambda g:g['bus','aisle-floor'].pop(('cpu','legacy')),
               lambda g:current(g).update(camera_xyz=[11,20,0]),lambda g:current(g).update(eye_height=1.55),
               lambda g:current(g).update(requested_pitch=-.95,pitch=-.95),lambda g:current(g).update(requested_yaw=0.,yaw=0.),
               lambda g:current(g)['aisle_native_target'].update(part_id=True),lambda g:current(g)['aisle_native_target'].update(delta_xy=[0,0]),
               lambda g:current(g)['aisle_native_target'].update(delta_xy=[0,-2]),lambda g:current(g)['aisle_native_target'].update(mount=[-1,1,1]),
               lambda g:current(g)['aisle_native_target']['published'].update(part_id=15),lambda g:current(g)['aisle_native_target']['published'].update(mount=[0,1,0]),
               lambda g:current(g)['aisle_native_target']['published'].update(id='seat'),lambda g:current(g)['aisle_native_target']['published'].update(features=16384),
               lambda g:current(g)['aisle_native_target']['published'].update(state=129),lambda g:current(g)['aisle_native_target']['published'].update(state=0),
               lambda g:current(g)['aisle_native_target']['published'].update(cell_index=-1),lambda g:current(g)['aisle_native_target']['published'].update(cell_index=True),
               lambda g:current(g).update(occupied_part=10),lambda g:current(g).update(occupied_eye_valid=False),
               lambda g:current(g).update(occupied_eye_xy=[0.,0.]),lambda g:current(g).pop('aisle_native_target')]
    for mutate in mutations:
        bad=copy.deepcopy(good);mutate(bad)
        try:verify_aisle_floor_views(bad)
        except (AssertionError,KeyError):pass
        else:raise AssertionError('Invented, hidden or incorrectly aimed aisle floor view admitted')
    return dict(success=True,pure_controls=1+len(mutations),local_GPU_Native_raster_or_game_execution=False)


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



def verify_depth_controls(report):
    rows=[r for r in report['frames'] if r.get('label','').startswith('depth-control-')]
    parity=[r for r in report['frames'] if r.get('label','').startswith('depth-parity-')]
    assert len(rows)==196 and len(parity)==14,'Both Native scenes require seven complete alternating depth-control pairs'
    groups={};cameras=defaultdict(set)
    for row in rows:
        match=re.fullmatch(r'depth-control-(parking|large)-([0-6])-(on|off)-([0-6])',row['label']);assert match,'Unknown depth-control label'
        scene,pair,enabled,sample=match[1],int(match[2]),match[3]=='on',int(match[4]);key=scene,pair,enabled,sample
        assert key not in groups and row['prototype']==scene and type(row['pair']) is int and row['pair']==pair and type(row['sample']) is int and row['sample']==sample
        assert type(row['depth_enabled']) is bool and row['depth_enabled']==enabled and row['actual_gpu'] is True
        assert (row['backend'],row['renderer'],row['frame_width'],row['frame_height'])==('gpu','modular',1280,800)
        for field in ['full_frame_ms','synchronized_frame_ms']:
            assert type(row[field]) in (int,float) and math.isfinite(row[field]) and row[field]>0
        assert row['synchronized_frame_ms']>=row['full_frame_ms'],'Explicit completion cannot precede its complete Native frame'
        assert type(row['bounded_depth_draws']) is int and ((row['bounded_depth_draws']>0) if enabled else row['bounded_depth_draws']==0),'The requested shader policy was not actually used'
        assert type(row['gpu_frame_serial']) is int and row['gpu_frame_serial']>0
        assert row['snapshot_vehicles']>0 and row['snapshot_parts']>0
        cameras[scene,pair].add((tuple(row['camera_xyz']),row['yaw'],row['pitch'],row['eye_height'],row['snapshot_vehicles'],row['snapshot_parts']))
        groups[key]=row
    assert len(cameras)==14 and all(len(v)==1 for v in cameras.values()),'Depth control changed the actual Native camera or snapshot counts'
    verified=set()
    for row in parity:
        match=re.fullmatch(r'depth-parity-(parking|large)-([0-6])',row['label']);assert match
        key=match[1],int(match[2]);assert key not in verified and row['prototype']==key[0] and type(row['pair']) is int and row['pair']==key[1];verified.add(key)
        assert row['pixels_equal'] is True and type(row['different_pixels']) is int and row['different_pixels']==0
        assert type(row['compared_pixels']) is int and row['compared_pixels']==1280*800 and type(row['compared_bytes']) is int and row['compared_bytes']==1280*800*4
    assert verified==set(cameras)
    controls=[]
    for scene in ['parking','large']:
        pairs=[]
        for pair in range(7):
            value={}
            for enabled in [False,True]:
                samples=[groups[scene,pair,enabled,n] for n in range(7)]
                assert len({r['gpu_frame_serial'] for r in samples})==7,'A depth control reused old GPU metrics instead of new submissions'
                value['enabled' if enabled else 'disabled']={field:sorted(r[field] for r in samples)[3] for field in ['full_frame_ms','synchronized_frame_ms']}
            pairs.append(dict(pair=pair,**value,ordinary_enabled_disabled_ratio=value['enabled']['full_frame_ms']/value['disabled']['full_frame_ms'],synchronized_enabled_disabled_ratio=value['enabled']['synchronized_frame_ms']/value['disabled']['synchronized_frame_ms']))
        controls.append(dict(scene=scene,pairs=pairs))
    return dict(success=True,actual_Native_scenes=2,paired_controls=14,timed_frames=196,full_normalized_pixel_pairs=14,compared_pixels=14*1280*800,scope='Separate same-process Native shader-policy control. Ordinary full-frame timing and frame plus explicit shared-device completion are reported separately; this is not hardware FPS and does not replace any original1120benchmark sample or900sresource qualification.',performance_accepted=False,hardware_FPS=False,controls=controls)


def ordinary_vehicle_frames(report):
    # These supplemental records have their own mandatory complete validation.
    # Preserve every original frame and its original checks without treating a
    # whole-image parity record as a submitted render or benchmark sample.
    depth_control=verify_depth_controls(report)
    return [r for r in report['frames'] if not r.get('label','').startswith(('depth-control-','depth-parity-'))],depth_control


def depth_control_self_test():
    import copy
    report={'frames':[]}
    for scene in ['parking','large']:
        for pair in range(7):
            for enabled in [False,True]:
                for sample in range(7):
                    report['frames'].append(dict(label=f'depth-control-{scene}-{pair}-'+('on' if enabled else 'off')+f'-{sample}',prototype=scene,pair=pair,sample=sample,depth_enabled=enabled,actual_gpu=True,backend='gpu',renderer='modular',frame_width=1280,frame_height=800,full_frame_ms=1.,synchronized_frame_ms=2.,bounded_depth_draws=5 if enabled else 0,gpu_frame_serial=len(report['frames'])+1,camera_xyz=[0,0,0],yaw=0.,pitch=0.,eye_height=1.55,snapshot_vehicles=1,snapshot_parts=230))
            report['frames'].append(dict(label=f'depth-parity-{scene}-{pair}',prototype=scene,pair=pair,pixels_equal=True,different_pixels=0,compared_pixels=1280*800,compared_bytes=1280*800*4))
    assert verify_depth_controls(report)['timed_frames']==196
    mutations=[lambda f:f.pop(0),lambda f:f.append(copy.deepcopy(f[0])),lambda f:f[0].update(pair=True),lambda f:f[0].update(sample=7),lambda f:f[0].update(depth_enabled='off'),lambda f:f[7].update(bounded_depth_draws=0),lambda f:f[0].update(bounded_depth_draws=1),lambda f:f[0].update(actual_gpu=False),lambda f:f[0].update(frame_width=640),lambda f:f[0].update(synchronized_frame_ms=.5),lambda f:f[0].update(full_frame_ms=float('nan')),lambda f:f.pop(14),lambda f:f[14].update(pixels_equal=False),lambda f:f[14].update(compared_pixels=1),lambda f:f[0].update(renderer='legacy'),lambda f:f[0].update(backend='cpu'),lambda f:f[0].update(yaw=.1),lambda f:f[1].update(gpu_frame_serial=f[0]['gpu_frame_serial'])]
    for mutate in mutations:
        changed=copy.deepcopy(report);mutate(changed['frames'])
        try:verify_depth_controls(changed)
        except (AssertionError,KeyError):pass
        else:raise AssertionError('Invalid Native depth control was admitted')
    ordinary={'label':'bench-original','full_frame_ms':3.};soak={'label':'soak-summary','elapsed_seconds':900.}
    mixed={'frames':[ordinary]+report['frames']+[soak]}
    frames,control=ordinary_vehicle_frames(mixed)
    assert len(frames)==2 and frames[0] is ordinary and frames[1] is soak and control['timed_frames']==196
    changed=copy.deepcopy(mixed);changed['frames'][1]['depth_enabled']='off'
    try:ordinary_vehicle_frames(changed)
    except (AssertionError,KeyError):pass
    else:raise AssertionError('Routing skipped validation of supplemental records')
    return dict(success=True,pure_controls=3+len(mutations),local_GPU_Native_raster_or_game_execution=False)


def verify(root, output, source, engine, module=None):
    from PIL import Image, ImageDraw
    root, output = Path(root), Path(output)
    report = json.loads((root/'first-person-vehicles.json').read_text(encoding='utf-8-sig'))
    assert report['schema']==1 and report['success'] and report['actual_cdda'] and report['native_parts_verified']
    assert report['native_driving_verified']
    assert report['intact_baseline_doors_closed']
    assert report['native_reload_lifecycle']=='quit_cleanup_setup_load'
    assert not report['manual_playthrough'] and not report['hardware_performance_qualified']
    assert (report['width'],report['height'],report['horizon_radius'])==(640,480,48)
    assert len(source)==40 and len(engine)==40
    assert module is not None and len(module)==64 and all(c in '0123456789abcdef' for c in module)
    memory=verify_memory(root/'first-person-process-memory.json',source,engine,module)
    frame_rows,depth_control=ordinary_vehicle_frames(report)
    groups=defaultdict(dict)
    captures=[];benchmarks=defaultdict(list);soak=None;records=set()
    for r in frame_rows:
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
        verify_fixture_camera(r)
        verify_native_frame_phases(r)
        assert r['full_frame_ms']>0
        if r['backend']=='cpu':verify_cpu_metrics(r)
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
        if prototype not in {'shopping_cart','truck_trailer'}:assert (prototype,'driver-controls') in groups,(prototype,'missing actual occupied controls detail')
    assert {('car',v) for v in ['driver','passenger','open-door','damaged','broken-door','broken-wheel','removed-wheel',
                               'installed-armor','night','rain','fog','paint-red','paint-blue']} <= groups.keys()
    verify_mutation_views(groups)
    verify_aisle_floor_views(groups)
    assert {('mixed','parking'),('mixed','after-motion'),('mixed','save-reload')} <= groups.keys()
    assert {('car','driving-'+str(step)) for step in range(8)} <= groups.keys()
    driving=[]
    for step in range(8):
        for (backend,mode),record in groups[('car','driving-'+str(step))].items():
            # Legacy deliberately does not request/copy the optional transport
            # snapshot. Only the selected new renderer can report its anchor.
            if mode=='modular':assert record['occupied_vehicle'] and record['occupied_part'] and record['occupied_eye_valid']
        driving.append(groups[('car','driving-'+str(step))][('gpu','modular')])
    assert len({tuple(r['camera_xyz']) for r in driving})>1,'Native driving never moved the actual passenger'
    assert len({r['yaw'] for r in driving})>1,'Native driving never changed the actual heading'
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
    sheet('vehicle-interiors.png',sorted(k for k in groups if k[1] in {'driver','passenger','driver-controls','aisle-floor'}))
    sheet('vehicle-aisle.png',[('bus','aisle-floor')])
    car_keys=sorted(k for k in groups if k[0]=='car' and not k[1].startswith(('moving-','activity-')))
    for offset in range(0,len(car_keys),12):sheet('vehicle-details-'+str(offset//12)+'.png',car_keys[offset:offset+12])
    sheet('vehicle-parking.png',[k for k in sorted(groups) if k[0]=='mixed' and not k[1].startswith('moving-')])
    if benchmarks:sheet('vehicle-custom.png',[k for k in sorted(groups) if k[0]=='custom'])
    sheet('vehicle-driving.png',[('car','driving-0'),('car','driving-7')])
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
                native_driving_verified=True,
                cpu_internal_dimensions_verified=True,
                capture_inventory=captures,review_inventory=inventory,
                performance=performance,depth_control=depth_control,soak=soak,process_memory=memory,cold_definition='Native cache refresh and first frame after switching the vehicle renderer; device resources may be retained',
                limitations=['Automated Native capture integrity does not establish visual acceptance.',
                             '640x480 visual captures and 1280x800 full-frame timings on CI adapters do not establish hardware FPS.',
                             'Interior comparisons pin the same actual Native seat eye across both renderers.'])
    (output/'vehicle-review.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result


if __name__=='__main__':
    import sys
    if sys.argv[1:]==['--self-test-aisle-floor-view']:
        print(json.dumps(aisle_floor_view_self_test()));raise SystemExit(0)
    if sys.argv[1:]==['--self-test-native-frame-phases']:
        print(json.dumps(native_frame_phases_self_test()));raise SystemExit(0)
    if sys.argv[1:]==['--self-test-fixture-camera']:
        print(json.dumps(fixture_camera_self_test()));raise SystemExit(0)
    if sys.argv[1:]==['--self-test-depth-controls']:
        depth=depth_control_self_test();depth['fixture_camera_controls']=fixture_camera_self_test();depth['native_frame_phase_controls']=native_frame_phases_self_test();depth['aisle_floor_view_controls']=aisle_floor_view_self_test();print(json.dumps(depth));raise SystemExit(0)
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path);p.add_argument('--source',required=True);p.add_argument('--engine',required=True);p.add_argument('--module-sha256')
    args=p.parse_args();result=verify(args.root,args.output,args.source,args.engine,args.module_sha256)
    print(json.dumps({k:v for k,v in result.items() if k not in {'capture_inventory','review_inventory'}}))
