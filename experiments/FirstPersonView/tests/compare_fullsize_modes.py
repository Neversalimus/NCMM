"""Validate four ordinary Native hardware modes and their reverse-order repeat.

Input paths refer to preserved local evidence, never CI timing or profiler spans.
The original baseline may use a diagnostic-only harness on unchanged production
sources; that equivalence is verified against Git, not inferred from a label.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import subprocess
from compare_movement_runs import execution_scopes
from verify_horizon_release import frame_phases, percentile, require

SCENES = {'street','city_blocks','dense_city','forest','multi_level_horde',
          'creatures_20','creatures_100','smoke_rain','armed','tool','empty_attacks'}
COMPONENTS = {'baseline':(False,False,False), 'assets_hud':(False,True,True),
              'optimization':(True,False,False), 'combined':(True,True,True)}
BASELINE_FILES = {'experiments/graphics_bridge/ncmm_graphics_movement_benchmark.inc',
                  'experiments/FirstPersonView/Run-FirstPersonBenchmark.ps1',
                  'experiments/FirstPersonView/reports/baseline-8d24-matrix.md'}
BASELINE_ORIGINAL_HARNESS='2bb3ec5a10ce2e22b11b537ec2fd5782518ee9f2'
BASELINE_SDK_REFERENCE='13f3e3e3f8ceaa777b8ea325605ba22a5bfe5a72'
SDK_IDENTITY_FIELDS=('sdl_version','private_port_version','registry_commit','source_archive_sha512',
                     'port_manifest_sha256','patch_sha256','runtime_vendor')
PINS = {'1040':'3f7fb352bf492ba521bd9408a0c9f6ce239e8d83',
        '1807':'074aa98bd5be3de4c35f154082db32a0e63bb0f1'}

def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()

def stats(values):
    require(values and all(type(v) in (int,float) and math.isfinite(v) and v>=0 for v in values),'Invalid timing samples')
    return dict(samples=len(values),median_ms=percentile(values,.5),p95_ms=percentile(values,.95),
                p99_ms=percentile(values,.99),maximum_ms=max(values))

def counts(values):
    require(values and all(type(v) is int and v>=0 for v in values),'Invalid count samples')
    return dict(samples=len(values),median=percentile(values,.5),p95=percentile(values,.95),
                p99=percentile(values,.99),maximum=max(values))

def native_window(record):
    require(record['native_window_size_available'] is True,'Actual Native window size unavailable')
    width=record['native_window_pixel_width'];height=record['native_window_pixel_height']
    scale=record['window_display_scale']
    require(type(width) is int and type(height) is int and width>0 and height>0,'Invalid Native window dimensions')
    require(type(scale) in (int,float) and math.isfinite(scale) and scale>0,'Invalid Native display scale')
    return width,height,scale

def baseline_equivalence(repo, production, harness):
    repo=Path(repo).resolve()
    command=['git','-c','safe.directory='+repo.as_posix(),'-C',str(repo)]
    changed=set(subprocess.check_output(command+['diff','--name-only',production,harness],text=True).splitlines())
    if changed-BASELINE_FILES:
        # The independently pinned reference is the old harness plus only the
        # same SDK correctness fix. It is not a blanket infrastructure exemption.
        sdk_files=set(subprocess.check_output(command+['diff','--name-only',BASELINE_ORIGINAL_HARNESS,BASELINE_SDK_REFERENCE],text=True).splitlines())
        require(changed<=BASELINE_FILES|sdk_files,'Baseline changes production sources outside its diagnostic harness/shared SDK')
        for name in sdk_files:
            expected=subprocess.check_output(command+['show',BASELINE_SDK_REFERENCE+':'+name])
            actual=subprocess.check_output(command+['show',harness+':'+name])
            require(actual==expected,'Baseline SDK recipe differs from its pinned reference: '+name)
    require(production=='8d24cc2b8df4ccd6574e1f2f61108baf24f4bcf9','Unexpected production baseline')


def sdk_identity(native):
    dependency=native.get('windows_sdl',{})
    require(dependency.get('sdl_version')=='3.4.10' and dependency.get('private_port_version')==2 and
            dependency.get('windows_build_only') is True and dependency.get('global_vcpkg_unchanged') is True,
            'Shared corrected SDL dependency is missing')
    require(all(isinstance(dependency.get(key),str) and len(dependency[key])==64 for key in ['port_manifest_sha256','patch_sha256']),
            'Invalid shared SDK digest identity')
    return {key:dependency[key] for key in SDK_IDENTITY_FIELDS}

def inspect_run(entry, bundle, pin, expected_vsync, *, baseline=False, plan_sha256):
    root=Path(entry['root']).resolve();evidence=root/'evidence'
    receipt=load(Path(bundle)/'candidate-download-verified.json')
    manifest=load(Path(bundle)/'payload/first-person-bundle.json')
    require(receipt['success'] and receipt['source']==manifest['ncmm_source_commit'],'Unverified candidate source')
    require(not manifest.get('diagnostic_mixed_components') and not manifest.get('diagnostic_module_source'),'Mixed components are not final four-mode evidence')
    identity={'ncmm_commit':receipt['source'],'host_sha256':receipt['host_sha256'],'module_sha256':receipt['module_sha256']}
    require(manifest['source_commit']==PINS[pin],'Candidate engine pin differs')
    shared_sdk=sdk_identity(load(Path(bundle)/'payload/host/first-person-source.json'))
    provenance=load(root/'benchmark-provenance.json');process=load(evidence/'first-person-process-result.json')
    require(provenance['success'] and provenance['movement_benchmark'] and provenance['extended_matrix'] and
            provenance['full_size_gpu_matrix'] and provenance['udp_requested'] and not provenance['manual_playthrough'],'Incomplete full-size hardware provenance')
    require(process['exit_signed']==0 and process['crash_dumps']==0 and process['debugger_error'] is None and
            process['wrapper_failure'] is None and process['last_progress']=='movement_complete','Unclean Native process')
    require(process.get('debugger_requested') is False,'Ordinary timing must not attach the diagnostic debugger')
    for source in [provenance,process]:
        require(all(source[k]==v for k,v in identity.items()),'Timed binary identity differs')
    require(process['engine_commit']==provenance['source_commit']==PINS[pin],'Timed Native pin differs')
    require(sha(root/'game/cataclysm-tiles.ncmm.exe')==identity['host_sha256'] and
            sha(root/'game/code_mods/FirstPersonView/ncmm_mod.dll')==identity['module_sha256'],'Installed timed bytes changed')
    launch=load(root/'ordinary-launch.json')
    require(launch['profiler_attached'] is False and launch['gpu_debug'] is False and
            launch['vsync_environment_hint'] is None and launch['components']==list(COMPONENTS[entry['mode']]),'Unexpected launch conditions')
    require(launch['wrapper_success'] is True and launch['mode']==entry['mode'] and launch['direction']==entry['direction'] and
            launch['plan_sha256']==plan_sha256,'Launch differs from the predeclared plan')
    require(launch['runner_sha256']==sha(Path(bundle)/'Run-FirstPersonBenchmark.ps1'),'Packaged benchmark wrapper differs')
    require(launch['controller_sha256']==sha(Path(__file__).with_name('Run-FullsizeModes.ps1')),'Four-mode controller source differs')
    if not baseline:
        for name,expected in zip(['camera_snapshot_enabled','held_assets_enabled','target_hud_enabled'],COMPONENTS[entry['mode']]):
            require(type(provenance[name]) is bool and provenance[name]==expected,'Incorrect component mode: '+name)
        require(provenance['gpu_debug_requested'] is False and provenance['trace_markers_enabled'] is False,'Instrumented launch is not ordinary timing')
    report=load(evidence/'first-person-movement-benchmark.json')
    require(report['success'] and report['actual_cdda_sdl'] and not report['manual_playthrough'] and
            report['simulation_turns_advanced']==0 and report['full_size_gpu_matrix'],'Invalid Native movement matrix')
    if not baseline:
        require(report['profiler_markers_enabled'] is False and report['gpu_debug_requested'] is False and
                report['distance_radius']==48 and report['native_pass_retained'] is True,'Ordinary rendering conditions changed')
    require(report['geometry_regressions']['success'] and report['geometry_regressions']['captures']==8 and
            report['facade_regression']['normal_vision'] and report['facade_regression']['slabs_connected'],'Native geometry regression failed')
    records=report['records'];require(len(records)==len(SCENES) and {r['scene'] for r in records}==SCENES,'Incomplete or duplicate scene matrix')
    inventory=load(evidence/'udp-asset-sha256.json');require(inventory,'UDP source inventory missing')
    for item in inventory:
        relative=PurePosixPath(item['file'])
        require(not relative.is_absolute() and '..' not in relative.parts,'Unsafe UDP inventory path')
        require(sha(root/'game/gfx/UndeadPeople_0J_Hybrid_v3'/Path(relative))==item['sha256'],'Actually used UDP file changed')
    udp_digest=hashlib.sha256(json.dumps(sorted((v['file'],v['sha256']) for v in inventory),separators=(',',':')).encode()).hexdigest()
    summaries={};windows={};bounds={}
    for record in records:
        windows[record['scene']]=native_window(record)
        if not baseline:
            value=tuple(record[k] for k in ['loaded_min_z','loaded_max_z','available_z_slices'])
            require(all(type(v) is int for v in value) and value[0]<=value[1] and value[2]>0,'Invalid loaded Native Z bounds')
            bounds[record['scene']]=value
        require(record['renderer']=='gpu' and record['device']=='NVIDIA GeForce RTX 4090' and
                record['backend']=='direct3d12' and record['software_device'] is False,'Hidden CPU/software fallback')
        require((record['width'],record['height'])==(1768,1084) and
                (record['internal_width'],record['internal_height'])==(1761,1080),'Resolution/quality changed')
        require(record['native_renderer_vsync_available'] and record['native_renderer_vsync']==expected_vsync,'Actual SDL VSync differs')
        require(record['tileset']=='UndeadPeople_0J_Hybrid_v3' and record['native_art'] and record['native_steps']==64,'Native fixture differs')
        require(record['native_confirmed_attacks']==(16 if record['scene']=='empty_attacks' else 0),'Confirmed Native attack count differs')
        require(record['native_weather_id']==('rain' if record['scene']=='smoke_rain' else 'sunny'),'Native weather differs')
        wall=record['frame_samples_ms'];require(len(record['frame_renderers_gpu'])==len(wall) and all(v==1 for v in record['frame_renderers_gpu']),'Measured frame fell back')
        require(len(record['first_ready_samples_ms'])==len(record['first_frame_after_step_samples_ms'])==64 and
                len(record['warm_rotation_samples_ms'])==len(record['cold_rotation_samples_ms'])==96,'Incomplete samples')
        require(all(a+.003>=b for a,b in zip(record['first_ready_samples_ms'],record['first_frame_after_step_samples_ms'])),'Action-ready precedes rendering')
        phases=frame_phases(record,extended=True);scopes=execution_scopes(record);require(scopes is not None,'Missing CPU/upload diagnostics')
        for name in ['gpu_static_upload_bytes','gpu_upload_bytes','gpu_draw_calls','gpu_resource_bytes','host_cpu_cache_bytes','process_rss_bytes']:
            require(len(record[name])==len(wall) and all(type(v) is int and v>=0 for v in record[name]),'Invalid resource stream')
        if not baseline:
            enabled=COMPONENTS[entry['mode']][0]
            hits=sum(record['camera_snapshot_hits'][96:192])
            require(hits>1000 if enabled else hits==0,'Actual Native snapshot reuse differs from requested mode')
            if enabled:require(len(set(record['camera_snapshot_generations'][96:192]))==1,'Camera-only Native cache generation changed')
        summaries[record['scene']]=dict(first_render=stats(record['first_frame_after_step_samples_ms']),
            native_action_to_ready=stats(record['first_ready_samples_ms']),warm_rotation=stats(record['warm_rotation_samples_ms']),
            cold_rotation=stats(record['cold_rotation_samples_ms']),phases=phases,execution_scopes=scopes,
            initial_render_ms=record['initial_render_ready_ms'],gpu_execution_ms=None,
            resource_peaks={k:max(record[k]) for k in ['gpu_resource_bytes','host_cpu_cache_bytes','process_rss_bytes']},
            warm_static_upload_bytes=sum(record['gpu_static_upload_bytes'][96:192]),
            warm_upload_bytes=sum(record['gpu_upload_bytes'][96:192]),warm_draw_calls=counts(record['gpu_draw_calls'][96:192]))
    return dict(mode=entry['mode'],direction=entry['direction'],root=str(root),identity=identity,udp_inventory_sha256=udp_digest,
                report_sha256=sha(evidence/'first-person-movement-benchmark.json'),scenes=summaries,
                fov_z_range=report.get('fov_z_range_configured'),native_windows=windows,loaded_world_bounds=bounds,
                sdl_runtime_versions=sorted({r['sdl_runtime_version'] for r in records}),shared_sdk=shared_sdk)

def compare(runs):
    require(len(runs)==8,'Both directions of all four modes are required')
    require([(r['direction'],r['mode']) for r in runs]==[
        *[('forward',m) for m in COMPONENTS],*[('reverse',m) for m in reversed(COMPONENTS)]],'Run order differs from predeclared forward/reverse plan')
    require(len({r['udp_inventory_sha256'] for r in runs})==1,'Actually used UDP files differ')
    require(len({tuple(r['sdl_runtime_versions']) for r in runs})==1,'SDL versions differ')
    require(all(isinstance(r.get('shared_sdk'),dict) and set(r['shared_sdk'])==set(SDK_IDENTITY_FIELDS) for r in runs),
            'Shared SDK source identity missing')
    require(len({json.dumps(r['shared_sdk'],sort_keys=True) for r in runs})==1,'Shared SDK differs between ordinary modes')
    require(len({r['fov_z_range'] for r in runs if r['mode']!='baseline'})==1,'Z range differs between component modes')
    require(len({json.dumps(r['native_windows'],sort_keys=True) for r in runs})==1,'Actual Native window size or display scale differs')
    require(len({json.dumps(r['loaded_world_bounds'],sort_keys=True) for r in runs if r['mode']!='baseline'})==1,'Loaded Native Z bounds differ')
    blocking=[];differences=[];lookup={(r['direction'],r['mode']):r for r in runs}
    for mode in list(COMPONENTS)[1:]:
        for scene in sorted(SCENES):
            for group in ['first_render','native_action_to_ready','warm_rotation']:
                for metric in ['median_ms','p95_ms','p99_ms','maximum_ms']:
                    pairs=[]
                    for direction in ['forward','reverse']:
                        before=lookup[direction,'baseline']['scenes'][scene][group][metric]
                        after=lookup[direction,mode]['scenes'][scene][group][metric]
                        require(before>0,'Invalid zero baseline')
                        pairs.append(dict(direction=direction,before_ms=before,after_ms=after,delta_ms=after-before,delta_percent=(after/before-1)*100))
                    item=dict(mode=mode,scene=scene,group=group,metric=metric,repeats=pairs)
                    differences.append(item)
                    if all(p['delta_percent']>5 and p['delta_ms']>.1 for p in pairs):blocking.append(item)
    noise={scene:{group:{metric:abs(lookup['forward','baseline']['scenes'][scene][group][metric]-lookup['reverse','baseline']['scenes'][scene][group][metric])
            for metric in ['median_ms','p95_ms','p99_ms','maximum_ms']} for group in ['first_render','warm_rotation']} for scene in sorted(SCENES)}
    return dict(performance_accepted=not blocking,blocking_reproducible_regressions=blocking,comparisons=differences,
                observed_baseline_repeat_spread_ms=noise)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--plan',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();plan=load(args.plan)
    require(plan['schema']==1 and plan['pin'] in PINS and plan['expected_vsync']==1,'Ordinary comparison plan required')
    baseline_equivalence(plan['baseline_repo'],plan['baseline_production_source'],plan['baseline_harness_source'])
    require(load(Path(plan['baseline_bundle'])/'candidate-download-verified.json')['source']==plan['baseline_harness_source'],
            'Baseline package does not contain the verified production-equivalent harness')
    require(load(Path(plan['combined_bundle'])/'candidate-download-verified.json')['source']==plan['combined_source'],
            'Combined package source differs from the declared plan')
    runs=[inspect_run(e,plan['baseline_bundle'] if e['mode']=='baseline' else plan['combined_bundle'],plan['pin'],1,
          baseline=e['mode']=='baseline',plan_sha256=sha(args.plan)) for e in plan['runs']]
    result=dict(schema=1,success=True,ordinary_hardware_measurements=True,manual_playthrough=False,
                first_frame_observations=64*len(SCENES)*len(runs),camera_observations=192*len(SCENES)*len(runs),
                runs=runs,**compare(runs),limitations=['Controlled Native movement without ordinary AI turns; action-to-ready excludes OS input dispatch.',
                    'No GPU execution duration is inferred from CPU submit timers. Profiler captures are separate evidence.',
                    'The timed perspective canvas and internal resolution are recorded separately from the actual Native window and display scale.',
                    '64 first frames per scene/run give limited p99/max precision; both repeats and baseline spread are retained.',
                    'Target 15 ms is reported by scene; absence of reproducible regressions does not promise 15 ms everywhere.'])
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k not in ['runs','comparisons','observed_baseline_repeat_spread_ms','blocking_reproducible_regressions']}))
    return 0 if result['performance_accepted'] else 2

if __name__=='__main__':raise SystemExit(main())
