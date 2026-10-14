"""Verify genuine paired game evidence; image quality still requires review."""
import argparse,hashlib,json
from pathlib import Path
from PIL import Image
import numpy as np
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
reports=[json.loads((a.root/f'first-person-multilevel-{mode}.json').read_text()) for mode in ['before','after']]
scenes={'house_one','house_two','house_three','high_building','basement','stairs','stairs_down','stairwell','ramp','shaft','broken_floor','rooftop','bridge','lab','cave','enemy_above','enemy_below','flying','light_other_z','smoke','unknown_mod'}
key=lambda r:(r['gallery'],r['name'],r['constructed_levels'],r['theme'],r['width'],r['height'],r['renderer'])
matrices=[{key(r):r for r in report['frames']} for report in reports]
assert all(r['success'] and r['actual_cdda_sdl'] and not r['gpu_timing_is_execution'] for r in reports)
assert not reports[0]['world_api'] and reports[1]['world_api'], 'reference/current callback identities are wrong'
assert len(reports[1]['transitions'])==2 and all(t['camera_continuous'] and t['renderer_preserved_moves'] for t in reports[1]['transitions'])
for mode in ['before','after']:
 save=json.loads((a.root/f'first-person-multilevel-save-{mode}.json').read_text())
 assert save['success'] and save['actual_cdda_save_load'] and [r['z'] for r in save['levels']]==[-1,0,1]
 assert all(r['restored_xyz'] and r['restored_moves'] and r['restored_turn'] and r['rendered'] and r['f6_restored_tiles'] for r in save['levels'])
assert matrices[0].keys()==matrices[1].keys(), 'paired scenes or dimensions differ'
assert len(matrices[1])==266, '21 galleries or 224 load records incomplete'
assert {r['name'] for r in matrices[1].values() if r['gallery']}==scenes
assert {r['name'] for r in matrices[1].values() if not r['gallery']}=={'outdoor','house','urban','lab','high_building','shaft','creatures'}, 'the required load scenarios differ'
expected={(n,t,w,h,mode) for n in [1,3,5,9] for t in range(7) for w,h in [(640,480),(1280,800),(1920,1080),(2560,1080)] for mode in ['cpu','gpu']}
assert {(r['constructed_levels'],r['theme'],r['width'],r['height'],r['renderer']) for r in matrices[1].values() if not r['gallery']}==expected
records=[]
for case,current in matrices[1].items():
 before=matrices[0][case]
 for r in [before,current]:
  assert r['simulation_unchanged'] and r['discovery_unchanged']
  assert 0<=r['median_ms']<=r['p95_ms']<=r['p99_ms'] and r['first_ready_ms']>=0
  assert r['actual_render_width']>0 and r['actual_render_height']>0
  if r['renderer']=='gpu':assert r['cpu_raster_calls']==0 and r['draw_calls']>0
 assert current['available_levels']>=current['constructed_levels'] and current['submitted_cells']<=65536
 assert current['source_cache_bytes']<8*1024*1024 and current['snapshot_bytes']<80*1024*1024
 assert current['budget_rejected']==0, 'normal tested scenes exceeded the sparse publication budget'
 assert current['processed_cells']>=current['submitted_cells']
 assert all(r[field]>=0 for r in [before,current] for field in ['cpu_frame_cache_bytes','gpu_host_cpu_cache_bytes','gpu_resource_bytes','gpu_geometry_cache_bytes','sprite_cache_bytes','creature_cache_bytes'])
 record={'case':case,'median_ms_before':before['median_ms'],'median_ms_after':current['median_ms'],'p95_ms_after':current['p95_ms'],'software_device':current['software_device']}
 if current['gallery']:
  if current['name']=='smoke':assert current['neutral_floor_tops']>0, 'dense smoke exposed sky in place of structural floor'
  images=[]
  for r in [before,current]:
   path=a.root/r['file'];image=np.array(Image.open(path).convert('RGB'))
   assert image.shape==(r['height'],r['width'],3)
   assert float(image.std())>1, 'uniform/empty gallery image'
   images.append(image.astype(np.float32));record['sha256_'+('before' if r is before else 'after')]=hashlib.sha256(path.read_bytes()).hexdigest()
  record['rgb_mae']=float(np.abs(images[0]-images[1]).mean()/255)
  assert [v['name'] for v in before['views']]==[v['name'] for v in current['views']]==['up','down']
  record['views']=[]
  for old_view,new_view in zip(before['views'],current['views']):
   assert abs(old_view['pitch']-new_view['pitch'])<.001
   assert (new_view['pitch']>.8 if new_view['name']=='up' else new_view['pitch']<-.8)
   compared=[];view_record={'name':new_view['name'],'pitch':new_view['pitch']}
   for phase,view in [('before',old_view),('after',new_view)]:
    path=a.root/view['file'];pixels=np.array(Image.open(path).convert('RGB'))
    assert pixels.shape==(current['height'],current['width'],3)
    assert path.name not in [before['file'],current['file']]
    view_record['sha256_'+phase]=hashlib.sha256(path.read_bytes()).hexdigest();compared.append(pixels.astype(np.float32))
   view_record['rgb_mae']=float(np.abs(compared[0]-compared[1]).mean()/255);record['views'].append(view_record)
 records.append(record)
assert any(r['floor_undersides']>0 for r in matrices[1].values() if r['gallery']), 'actual structural ceilings are absent'
assert any(len(r['submitted_levels'])>=3 for r in matrices[1].values()), 'rendering never published three real Z levels'
result={'schema':1,'success':True,'actual_cdda_sdl':True,'paired_records':len(records),'gallery_scenes':len(scenes),'gallery_views':len(scenes)*2*3,'baseline':'main 0.11 module 9ee19bdd68e40a1677694187f85aa65bd4b52407 on the same pinned engine Host','gpu_timing_is_execution':False,'records':records}
a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print('Paired real CDDA multilevel evidence: 21 scenes, 224 load records PASS')
