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
assert matrices[0].keys()==matrices[1].keys(), 'paired scenes or dimensions differ'
assert len(matrices[1])==266, '21 galleries or 224 load records incomplete'
assert {r['name'] for r in matrices[1].values() if r['gallery']}==scenes
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
 record={'case':case,'median_ms_before':before['median_ms'],'median_ms_after':current['median_ms'],'p95_ms_after':current['p95_ms'],'software_device':current['software_device']}
 if current['gallery']:
  images=[]
  for r in [before,current]:
   path=a.root/r['file'];image=np.array(Image.open(path).convert('RGB'))
   assert image.shape==(r['height'],r['width'],3)
   assert float(image.std())>1, 'uniform/empty gallery image'
   images.append(image.astype(np.float32));record['sha256_'+('before' if r is before else 'after')]=hashlib.sha256(path.read_bytes()).hexdigest()
  record['rgb_mae']=float(np.abs(images[0]-images[1]).mean()/255)
 records.append(record)
assert any(r['floor_undersides']>0 for r in matrices[1].values() if r['gallery']), 'actual structural ceilings are absent'
assert any(len(r['submitted_levels'])>=3 for r in matrices[1].values()), 'rendering never published three real Z levels'
result={'schema':1,'success':True,'actual_cdda_sdl':True,'paired_records':len(records),'gallery_scenes':len(scenes),'baseline':'main 0.11 module 9ee19bdd68e40a1677694187f85aa65bd4b52407 on the same pinned engine Host','gpu_timing_is_execution':False,'records':records}
a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print('Paired real CDDA multilevel evidence: 21 scenes, 224 load records PASS')
