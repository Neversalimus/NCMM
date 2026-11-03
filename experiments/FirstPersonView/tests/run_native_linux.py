"""Run the actual pinned Linux CDDA Host in isolated worlds under Xvfb.

Mesa software Vulkan is functional evidence only, never hardware timing evidence.
The data archive is the same official, digest-checked graphics package used by
Windows qualification; no Windows executable is run here.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

PINS = {'1040':'3f7fb352bf492ba521bd9408a0c9f6ce239e8d83',
        '1807':'074aa98bd5be3de4c35f154082db32a0e63bb0f1'}
MODES = {'controls':{}, 'camera':{'NCMM_FP_CAMERA_INPUT':'1'},
         'vehicles':{'NCMM_FP_VEHICLE_ONLY':'1'},
         'fps':{'NCMM_FP_GAMEPLAY_AUDIT':'1','NCMM_FP_FPS_INPUT_ONLY':'1'},
         'held':{'NCMM_FP_HELD_GALLERY_ONLY':'1'},
         'target':{'NCMM_FP_GAMEPLAY_AUDIT':'1','NCMM_FP_TARGET_HUD_ONLY':'1'},
         'gameplay':{'NCMM_FP_GAMEPLAY_AUDIT':'1'}}


def digest(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def process_memory(status):
    result={}
    for key,name in [('VmRSS','rss_bytes'),('VmHWM','peak_rss_bytes')]:
        match=re.search(r'^'+key+r':\s+(\d+) kB$',status,re.MULTILINE)
        if not match:raise ValueError('Native process memory field unavailable: '+key)
        result[name]=int(match[1])*1024
    return result


def observe_process_memory(process, read_status):
    try:return process_memory(read_status())
    except (FileNotFoundError,ProcessLookupError,ValueError) as error:
        # A single poll cannot settle a race with process teardown. Omit a
        # malformed/missing terminal sample only after a bounded wait proves
        # that the process has exited;
        # retain its real exit status and every preceding RSS/HWM sample.
        try:process.wait(timeout=5)
        except subprocess.TimeoutExpired:raise error
        return None


def seed_linux_configuration(game):
    path=Path(game)/'config/options.json'
    if path.exists():raise ValueError('Linux fixture requires a fresh owned configuration')
    path.parent.mkdir(parents=True,exist_ok=True)
    # C.UTF-8 does not match a supported game language. An empty USE_LANG then
    # opens select_initial_language() before the Native smoke dispatcher runs.
    path.write_text(json.dumps([{'name':'USE_LANG','value':'en'}],indent=2)+'\n',encoding='utf-8')


def install_linux_fixtures(game, tests=None):
    tests=Path(tests) if tests is not None else Path(__file__).resolve().parent
    fixtures={
        'data/json/furniture_and_terrain/ncmm_multilevel_qualification.json':'multilevel_fixture.json',
        'data/json/monsters/ncmm_appearance_qualification.json':'creature_appearance_fixture.json',
    }
    planned=[(Path(game)/relative,(tests/name).read_bytes(),relative) for relative,name in fixtures.items()]
    if any(target.exists() for target,_,_ in planned):
        raise ValueError('Linux fixtures require fresh owned test data')
    for target,data,_ in planned:
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    return {relative:hashlib.sha256(data).hexdigest() for _,data,relative in planned}


def run(args):
    assert sys.platform=='linux'
    repo=Path(__file__).resolve().parents[3]
    bridge=repo/'experiments/graphics_bridge'
    # Logical PRs add these source hooks independently. Test every hook that is
    # compiled in this exact source tree; the complete integration has all six
    # modes. Missing data/verifiers for a present hook remain a hard failure.
    features={'held':(bridge/'ncmm_graphics_held_gallery.inc').is_file(),
              'target':(bridge/'ncmm_graphics_target_smoke.inc').is_file()}
    for mode,present in features.items():
        if present:
            verifier='verify_held_gallery.py' if mode=='held' else 'verify_target_audit.py'
            assert Path(__file__).with_name(verifier).is_file(), 'Compiled Native feature lacks its verifier'
    modes={name:flags for name,flags in MODES.items() if name not in features or features[name]}
    modes={name:flags for name,flags in modes.items() if (name=='vehicles' if args.mode=='vehicles' else name!='vehicles')}
    engine=args.engine.resolve(); data=args.data.resolve(); output=args.output.resolve()
    assert not output.exists(), 'Preserve earlier Native evidence'
    output.mkdir(parents=True)
    source=subprocess.check_output(['git','-C',str(engine),'rev-parse','HEAD'],text=True).strip()
    ncmm_source=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
    assert source==PINS[args.build]
    receipt=json.loads((engine/'first-person-source.json').read_text())
    assert receipt['source_commit']==source and receipt['status']=='experimental-not-certified'
    version=re.search(r'commit sha:\s*([0-9a-fA-F]{7,40})', (data/'VERSION.txt').read_text())
    assert version and source.startswith(version[1].lower())
    binary=engine/'build/src/cataclysm-tiles'
    assert binary.is_file()
    assert binary.read_bytes()[:4]==b'\x7fELF'
    modules=json.loads((repo/'components/native-build.json').read_text())['modules']
    module_sources={m['folder']:repo/'mods'/m['folder'] for m in modules}
    module_sources['FirstPersonView']=repo/'experiments/FirstPersonView'
    inventory={}
    for folder,module_source in module_sources.items():
        library=args.modules/folder/'ncmm_mod.so'
        assert library.is_file() and library.read_bytes()[:4]==b'\x7fELF'
        inventory[folder]=digest(library)
    results=[]
    for mode,switches in modes.items():
        game=output/mode;game.mkdir()
        shutil.copy2(binary,game/'cataclysm-tiles.ncmm')
        shutil.copy2(data/'VERSION.txt',game/'VERSION.txt')
        for folder in ['data','gfx','font','sound','lang']:
            if (data/folder).is_dir():
                shutil.copytree(data/folder,game/folder,ignore=shutil.ignore_patterns('cache'))
        # Linux SPIR-V comes from this exact source build, not the Windows data archive.
        shutil.copytree(engine/'data/shaders',game/'data/shaders',dirs_exist_ok=True)
        fixture_inputs=install_linux_fixtures(game)
        for folder,module_source in module_sources.items():
            target=game/'code_mods'/folder;target.mkdir(parents=True)
            shutil.copy2(args.modules/folder/'ncmm_mod.so',target/'ncmm_mod.so')
            shutil.copy2(module_source/'mod.json',target/'mod.json')
            for extra in ['data','persistent_data']:
                if (module_source/extra).is_dir(): shutil.copytree(module_source/extra,target/extra)
        allowed={'PATH','HOME','USER','LOGNAME','LANG','LC_ALL','LC_CTYPE','LD_LIBRARY_PATH',
                 'DISPLAY','XAUTHORITY','XDG_RUNTIME_DIR','XDG_DATA_DIRS','XDG_CONFIG_DIRS',
                 'TMPDIR','TMP','TEMP','VK_DRIVER_FILES'}
        environment={k:v for k,v in os.environ.items() if k in allowed}
        environment.update(NCMM_FP_APPEARANCE_SMOKE='1',SDL_ASSERT='abort',
                           SDL_RENDER_DRIVER='gpu',SDL_GPU_DRIVER='vulkan',SDL_AUDIODRIVER='dummy')
        environment.update(switches)
        if mode=='vehicles':environment['NCMM_FP_VEHICLE_SOAK_SECONDS']='900'
        seed_linux_configuration(game)
        if mode=='controls':
            from seed_deferred_options import seed
            seed(game);environment['NCMM_FP_VERIFY_DEFERRED_OPTIONS']='1'
        initial_options=(game/'config/options.json').read_bytes()
        started=time.time(); timed_out=False;memory=[]
        # A distro-style Linux build defaults to PREFIX/share/cataclysm-dda.
        # The owned fixture uses the portable data/gfx layout in its cwd.
        # An explicit empty base selects that upstream-supported layout;
        # --datadir alone would incorrectly move gfx under data/gfx.
        command=[str(game/'cataclysm-tiles.ncmm'),'--ncmm-gameplay-smoke',
                 '--seed','ncmm-gameplay-0121','--basepath','', '--userdir',str(game)]
        with (game/'stdout.log').open('wb') as stdout,(game/'stderr.log').open('wb') as stderr:
            process=subprocess.Popen(command,cwd=game,env=environment,
                stdout=stdout,stderr=stderr,start_new_session=True)
            try:
                if mode!='vehicles':code=process.wait(timeout=4200 if mode=='gameplay' else 1500)
                else:
                    deadline=time.monotonic()+3000
                    while True:
                        try:code=process.wait(timeout=5);break
                        except subprocess.TimeoutExpired:
                            if time.monotonic()>=deadline:raise
                            sample=observe_process_memory(process,lambda:Path('/proc',str(process.pid),'status').read_text())
                            if sample is not None:memory.append(dict(elapsed_seconds=time.time()-started,**sample))
            except subprocess.TimeoutExpired:
                import signal
                timed_out=True;os.killpg(process.pid,signal.SIGKILL);code=process.wait()
        result=dict(mode=mode,exit_code=code,timed_out=timed_out,elapsed_seconds=time.time()-started,
                    command=command,working_directory=str(game),
                    host_sha256=digest(game/'cataclysm-tiles.ncmm'),module_sha256=inventory,
                    actual_linux_native=True,ordinary_performance_measurement=False,manual_playthrough=False)
        results.append(result)
        (output/'process-results.json').write_text(json.dumps(results,indent=2)+'\n')
        evidence=output/'evidence'/mode;evidence.mkdir(parents=True)
        if mode=='vehicles':
            (evidence/'first-person-process-memory.json').write_text(json.dumps(dict(schema=1,actual_native=True,
                source_commit=ncmm_source,engine_commit=source,module_sha256=inventory['FirstPersonView'],sample_period_seconds=5,samples=memory),indent=2)+'\n')
        (evidence/'fixture-options-before-launch.json').write_bytes(initial_options)
        (evidence/'fixture-inputs-before-launch.json').write_text(json.dumps(fixture_inputs,indent=2)+'\n',encoding='utf-8')
        for file in list((game/'ncmm').glob('*'))+list((game/'config').glob('*.log'))+[game/'stdout.log',game/'stderr.log']:
            if file.is_file():shutil.copy2(file,evidence/file.name)
        print(json.dumps(result),flush=True)
        for file in [game/'stdout.log',game/'stderr.log',game/'ncmm/first-person-audit-progress.txt']:
            if file.exists():print(file.read_text(errors='replace')[-12000:],flush=True)
        if code!=0:
            failure=game/'ncmm/first-person-smoke.json'
            if failure.exists():print(failure.read_text(errors='replace')[:8192],flush=True)
        assert not timed_out and code==0, f'Actual Linux Native failed: {mode}, exit {code}'
        smoke=json.loads((game/'ncmm/first-person-smoke.json').read_text())
        gameplay=json.loads((game/'ncmm/gameplay-smoke.json').read_text())
        assert smoke['success'] and gameplay['success']
        if mode=='vehicles':
            from verify_vehicle_gallery import verify
            verify(evidence,output/'vehicle-review',ncmm_source,source,inventory['FirstPersonView'])
        if mode=='controls':
            assert smoke['sdl_frames']>=70
            restored=json.loads((evidence/'first-person-deferred-options.json').read_text())
            assert restored['success'] and restored['actual_cdda_configuration'] and restored['value_checks']==8
        if mode=='camera':
            camera=json.loads((game/'ncmm/first-person-camera-input.json').read_text())
            assert camera['success'] and camera['actual_cdda_sdl'] and camera['native_unchanged']
            assert not camera['fixture_input_lease'] and camera['gpu_frames']==camera['presented_frames']>=16
            assert camera['backend']=='vulkan' and camera['device']
            assert camera['software_device'], 'This runner deliberately qualifies software Vulkan only'
        if mode=='held':
            from verify_held_gallery import verify
            verified=verify(game/'ncmm',expected_backend='vulkan')
            assert verified['software_devices']==[True]
            (evidence/'gallery-verified.json').write_text(json.dumps(verified,indent=2)+'\n')
        if mode=='target':
            from verify_target_audit import verify
            verified=verify(game/'ncmm',expected_backend='vulkan')
            assert verified['software_devices']==[True]
            (evidence/'target-verified.json').write_text(json.dumps(verified,indent=2)+'\n')
        if mode=='gameplay':
            from verify_fire_pixels import verify
            verified=verify(game/'ncmm')
            (evidence/'fire-pixels-verified.json').write_text(json.dumps(verified,indent=2)+'\n')
        if mode in {'fps','gameplay'}:
            from verify_fps_audit import verify
            verified=verify(game/'ncmm')
            (evidence/'fps-input-verified.json').write_text(json.dumps(verified,indent=2)+'\n')
        # Only fresh, owned copies are removed; all evidence has been preserved.
        for folder in ['data','gfx','font','sound','lang','save','code_mods']:
            target=game/folder
            assert target.resolve().parent==game.resolve() and game.parent==output
            if target.is_dir():shutil.rmtree(target)
    summary=dict(schema=1,success=True,ncmm_source=ncmm_source,engine_source=source,host_sha256=digest(binary),
                 module_sha256=inventory,actual_linux_native=True,software_vulkan_functional_only=True,
                 hardware_performance_qualified=False,manual_playthrough=False,features=features,
                 tested_modes=list(modes),processes=results)
    (output/'linux-native-verified.json').write_text(json.dumps(summary,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine',type=Path,required=True)
    parser.add_argument('--data',type=Path,required=True)
    parser.add_argument('--modules',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--build',choices=PINS,required=True)
    parser.add_argument('--mode',choices=['standard','vehicles'],default='standard')
    run(parser.parse_args())
