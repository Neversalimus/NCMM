"""CI-only same-Host ablation. Reuse the existing disposable FPS fixture root.

The reference target uses current sources except the hash-pinned pre-71 terrain
include. It is not the historical complete module or an ordinary-game FPS test.
No local mode, extra checkout, full game copy, mouse helper or driver change.
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
import threading
import time

from verify_terrain_profile import MATERIALS, SCENES, native_material_definitions, verify as verify_profile

REFERENCE_SHA = '4b9a36a62151216e0b44ac82aaa277c273d7e989591868c9ac4b6902bb0d86ec'
REFERENCE_COMMIT = 'fdbba45f9082986202bd05db521bc315e9be4744'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def require_ci(env):
    if not (env.get('GITHUB_ACTIONS') == 'true' and env.get('RUNNER_OS') == 'Windows'
            and env.get('GITHUB_REPOSITORY') == 'Neversalimus/NCMM'
            and re.fullmatch(r'[0-9]+', env.get('GITHUB_RUN_ID', ''))
            and env.get('RUNNER_TEMP') and env.get('GITHUB_WORKSPACE')):
        raise ValueError('Native terrain pair is restricted to an isolated Windows GitHub runner.')


def below(path, parent):
    raw, parent = Path(os.path.abspath(path)), Path(parent).resolve(strict=True)
    for entry in (raw, *raw.parents):
        if entry.exists() and (entry.is_symlink() or getattr(entry, 'is_junction', lambda: False)()):
            raise ValueError('Fixture paths may not contain links or junctions.')
        if entry == parent:
            break
    path = raw.resolve(strict=False)
    if not path.is_relative_to(parent) or path == parent:
        raise ValueError('Fixture/build output must stay below the named CI root.')
    return path


def read_progress_phase(game, started_ns):
    """Return the latest complete Native phase from a fresh, bounded tail."""
    try:
        progress = below(game / 'ncmm/first-person-audit-progress.txt', game)
        with progress.open('rb') as stream:
            if os.fstat(stream.fileno()).st_mtime_ns < started_ns:
                return 'unavailable'
            size = stream.seek(0, os.SEEK_END)
            start = max(0, size - 4096)
            stream.seek(start)
            tail = stream.read(size - start)
        if start:
            # The first line can start outside the bounded read.
            tail = tail.partition(b'\n')[2]
        for line in reversed(tail.split(b'\n')[:-1]):
            line = line.removesuffix(b'\r')
            match = re.fullmatch(
                rb'([0-9][0-9,. \x80-\xff]{0,39}) ([A-Za-z0-9_+.-]{1,128}(?: PASS)?) frames=([0-9][0-9,. \x80-\xff]{0,39})',
                line)
            if not match or len(match[2]) > 128:
                continue
            # Legacy Native iostreams inherit locale integer grouping. These
            # fields identify records; they are never timing measurements.
            valid = True
            for value in (match[1], match[3]):
                for separator in (b'\xc2\xa0', b'\xe2\x80\xaf', b'\xa0', b'.', b' '):
                    value = value.replace(separator, b',')
                if (not re.fullmatch(rb'(?:[0-9]{1,20}|[1-9][0-9]{0,2}(?:,[0-9]{3}){1,6})', value)
                        or len(value.replace(b',', b'')) > 20):
                    valid = False
                    break
            if valid:
                return match[2].replace(b' ', b'_').decode('ascii')
    except (OSError, ValueError):
        pass
    return 'unavailable'


def run_child(command, game, env, directory, identity, timeout=600):
    require_ci(os.environ)
    result = dict(identity, exit_code=None, pid=None, started_ns=time.time_ns(),
                  ended_ns=None, timeout=False, state='launching', error_type=None)
    path = directory / 'process.json'
    save(path, result)
    active = None
    stopped = threading.Event()
    monitor = None
    def progress_log():
        # CI diagnostics stay in the CI log, consumed through Code Review.
        # Do not repackage progress text or child stdout as program JSON evidence.
        while not stopped.wait(30):
            phase = read_progress_phase(game, result['started_ns'])
            elapsed = max(0, (time.time_ns() - result['started_ns']) / 1e9)
            print(f"Terrain owned child pid={result['pid']} elapsed_s={elapsed:.1f} phase={phase}", flush=True)
    try:
        startup = subprocess.STARTUPINFO() if sys.platform == 'win32' else None
        if startup:
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = subprocess.SW_HIDE
        with (directory / 'stdout.log').open('wb') as stdout, (directory / 'stderr.log').open('wb') as stderr:
            active = subprocess.Popen(command, cwd=game, env=env, startupinfo=startup, stdout=stdout, stderr=stderr)
            result.update(pid=active.pid, state='running')
            save(path, result)
            monitor = threading.Thread(target=progress_log, daemon=True)
            monitor.start()
            try:
                result['exit_code'] = active.wait(timeout=timeout)
                result['state'] = 'exited'
            except subprocess.TimeoutExpired:
                result.update(timeout=True, state='timeout')
                raise
    except Exception as error:
        result['error_type'] = type(error).__name__
        if result['state'] == 'launching':
            result['state'] = 'launch-failed'
        raise
    finally:
        stopped.set()
        if monitor:
            monitor.join(timeout=2)
        try:
            if active and active.poll() is None:
                active.kill()
                active.wait(timeout=15)
        finally:
            if active:
                result['exit_code'] = active.poll()
            result['ended_ns'] = time.time_ns()
            save(path, result)
    return result


def run(args):
    require_ci(os.environ)  # Before any configure, launch, save reset or mutation.
    repo = args.repository.resolve(strict=True)
    game = below(args.game, os.environ['RUNNER_TEMP']).resolve(strict=True)
    out = below(args.output, os.environ['RUNNER_TEMP'])
    build = below(args.build, os.environ['GITHUB_WORKSPACE']).resolve(strict=True)
    payload = args.payload.resolve(strict=True)
    if out.exists() or out.is_relative_to(game) or game.is_relative_to(out):
        raise ValueError('Use a new evidence directory outside the existing fixture game.')
    git = ['git', '-C', str(repo)]
    head = subprocess.check_output(git + ['rev-parse', 'HEAD'], text=True).strip()
    tree = subprocess.check_output(git + ['rev-parse', 'HEAD^{tree}'], text=True).strip()
    subprocess.run(git + ['diff', '--exit-code', 'HEAD', '--'], check=True)
    bundle = json.loads((payload / 'first-person-bundle.json').read_text(encoding='utf-8-sig'))
    if head != os.environ['GITHUB_SHA'] or head != bundle['ncmm_source_commit']:
        raise ValueError('Source checkout, workflow and prepared bundle differ.')
    host = game / 'cataclysm-tiles.ncmm.exe'
    module = game / 'code_mods/FirstPersonView/ncmm_mod.dll'
    expected_shipping = bundle['files']['code_mods/FirstPersonView/ncmm_mod.dll']
    if sha(host) != bundle['host_sha256'] or sha(module) != expected_shipping:
        raise ValueError('Prepared Host/module does not match the verified bundle.')
    terrain_definitions = native_material_definitions(game / 'data/json/furniture_and_terrain')
    reference = Path(__file__).parent / 'reference/terrain_edges_pre71.inc'
    if sha(reference) != REFERENCE_SHA:
        raise ValueError('Historical terrain include changed.')
    out.mkdir()
    shutil.copyfile(payload / 'first-person-bundle.json', out / 'bundle.json')
    # Use the existing module build, with identical support objects and options.
    try:
        subprocess.run(['cmake', '-S', str(repo / 'experiments/FirstPersonView'), '-B', str(build),
                        '-DFIRST_PERSON_TERRAIN_REFERENCE_SOURCE=' + reference.as_posix()], check=True)
        subprocess.run(['cmake', '--build', str(build), '--config', 'Release', '--target',
                        'ncmm_mod_terrain_current', 'ncmm_mod_terrain_reference', '--parallel', '4'], check=True)
    finally:
        subprocess.run(['cmake', '-S', str(repo / 'experiments/FirstPersonView'), '-B', str(build),
                        '-DFIRST_PERSON_TERRAIN_REFERENCE_SOURCE='], check=True)
    reference_dll = build / 'Release/ncmm_mod_terrain_reference.dll'
    current_dll = build / 'Release/ncmm_mod_terrain_current.dll'
    shipping_copy = out / 'shipping-module.dll'
    shutil.copyfile(module, shipping_copy)
    current_hash = sha(current_dll)
    reference_hash = sha(reference_dll)
    if sha(build / 'Release/ncmm_mod.dll') != expected_shipping or len({current_hash, reference_hash, expected_shipping}) != 3:
        raise ValueError('Shipping build changed or diagnostic targets are indistinguishable.')
    immutable = {}
    # Bind actual settings, graphics data and runtime assets across every child.
    for directory in ('config', 'code_mods', 'gfx', 'data'):
        for file in sorted((game / directory).rglob('*')):
            if file.is_file() and file != module:
                immutable[file.relative_to(game).as_posix()] = sha(file)
    assets_digest = hashlib.sha256(json.dumps(immutable, sort_keys=True).encode()).hexdigest()
    save(out / 'fixture-files.json', immutable)
    env = {k: v for k, v in os.environ.items() if not k.startswith('NCMM_FP_')}
    env.update(NCMM_FP_GPU_BENCHMARK_ONLY='1', NCMM_FP_TERRAIN_PROFILE='1', NCMM_FP_TERRAIN_COLD_WORLD='1',
               NCMM_FP_APPEARANCE_SMOKE='1', SDL_ASSERT='abort')
    receipt = dict(schema=3, terrain_fixture_schema=4, terrain_material_ids=list(MATERIALS),
                   diagnostic_only=True, cpu_world_policy='rebuild-each-call',
                   terrain_material_definitions=terrain_definitions,
                   source_commit=head, source_tree=tree, engine_commit=bundle['source_commit'],
                   workflow_run=os.environ['GITHUB_RUN_ID'], host_sha256=sha(host),
                   shipping_module_sha256=expected_shipping,
                   current_module_sha256=current_hash, reference_module_sha256=reference_hash,
                   reference_include_sha256=REFERENCE_SHA, reference_include_commit=REFERENCE_COMMIT,
                   current_include_sha256=sha(repo / 'experiments/FirstPersonView/src/terrain_edges.inc'),
                   reference_kind='current-source-with-pre71-terrain-include',
                   fixture_files_sha256=sha(out / 'fixture-files.json'), fixture_digest=assets_digest,
                   pairs=args.pairs, trials=[], module_restored=False,
                   ordinary_game_performance=False, gpu_execution_timestamps_available=False,
                   phase='prepared', active_trial=None, failure_type=None, failure_stage=None)
    save(out / 'pair.json', receipt)
    try:
        for pair in range(args.pairs):
            for order in range(2):
                kind = ('reference', 'current')[(pair + order) % 2]
                directory = out / f'pair-{pair + 1:02d}-{kind}'
                directory.mkdir()
                receipt.update(phase='reset-world', active_trial=directory.name)
                save(out / 'pair.json', receipt)
                shutil.copyfile(reference_dll if kind == 'reference' else current_dll, module)
                module_hash = sha(module)
                if sha(host) != receipt['host_sha256']:
                    raise ValueError('Host changed before the next pair.')
                subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                                str(Path(__file__).parent / 'Reset-SmokeWorld.ps1'), '-GameRoot', str(game)], check=True)
                settings = {name: sha(game / name) for name in immutable}
                if settings != immutable:
                    raise ValueError('Fixture settings/assets changed between children.')
                paths = [game / 'ncmm/first-person-terrain-benchmark.json'] + [
                    game / 'ncmm' / f'first-person-gpu-{scene}-640x480-{mode}.bmp'
                    for scene in SCENES for mode in ('cpu', 'gpu')]
                old_times = {p: p.stat().st_mtime_ns if p.exists() else None for p in paths}
                receipt['phase'] = 'child'
                save(out / 'pair.json', receipt)
                identity = dict(source_commit=head, engine_commit=bundle['source_commit'],
                                host_sha256=sha(host), module_sha256=sha(module), fixture_digest=assets_digest)
                result = run_child([str(host), '--ncmm-gameplay-smoke', '--seed', 'ncmm-gameplay-0121',
                                    '--userdir', str(game)], game, env, directory, identity)
                result.update(host_sha256=sha(host), module_sha256=sha(module))
                save(directory / 'process.json', result)
                receipt['phase'] = 'validate-report'
                save(out / 'pair.json', receipt)
                if result['exit_code'] != 0 or result['host_sha256'] != receipt['host_sha256'] or result['module_sha256'] != module_hash:
                    raise ValueError('Native child failed or binary identity changed.')
                for path in paths:
                    if not path.is_file() or path.stat().st_mtime_ns == old_times[path]:
                        raise ValueError('Fresh Native report/capture is missing.')
                    shutil.copyfile(path, directory / path.name)
                report = json.loads((directory / paths[0].name).read_text(encoding='utf-8-sig'))
                verify_profile(report)
                receipt['trials'].append(dict(pair=pair, order=order, kind=kind, directory=directory.name,
                                             report_sha256=sha(directory / paths[0].name),
                                             process_sha256=sha(directory / 'process.json'),
                                             images={p.name: sha(directory / p.name) for p in paths[1:]}))
                save(out / 'pair.json', receipt)
    except Exception as error:
        receipt.update(failure_type=type(error).__name__, failure_stage=receipt['phase'])
        raise
    finally:
        shutil.copyfile(shipping_copy, module)
        receipt['module_restored'] = sha(module) == expected_shipping
        receipt['fixture_restored'] = all(sha(game / name) == value for name, value in immutable.items())
        save(out / 'pair.json', receipt)
    if not receipt['module_restored'] or not receipt['fixture_restored']:
        raise ValueError('Native fixture/bundle restoration failed.')
    from verify_terrain_pair import verify
    receipt.update(phase='verify-pairs', active_trial=None)
    save(out / 'pair.json', receipt)
    try:
        verified = verify(out)
    except Exception as error:
        receipt.update(failure_type=type(error).__name__, failure_stage=receipt['phase'])
        save(out / 'pair.json', receipt)
        raise
    receipt['phase'] = 'complete'
    save(out / 'pair.json', receipt)
    save(out / 'verified.json', verified)
    print('Same-Host terrain pair, exact captures, clean exits and source binding PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    for name in ('repository', 'game', 'payload', 'build', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--pairs', type=int, choices=range(3, 8), default=7)
    run(parser.parse_args())
