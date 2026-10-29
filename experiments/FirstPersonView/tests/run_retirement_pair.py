"""CI-only backend ablation in the existing disposable Host build and game."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from run_terrain_pair import below, require_ci, run_child, save
from verify_retirement_pair import (CANDIDATE_SHA, CANDIDATE_SOURCE, PARSER_SHA,
                                    REFERENCE_SHA, REFERENCE_SOURCE, sha, stair_metrics, verify)


def reference_backend(candidate):
    if hashlib.sha256(candidate).hexdigest() != CANDIDATE_SHA:
        raise ValueError('Unreviewed current backend')
    text = candidate.decode('utf-8')
    start = text.index('        poll();if(retirement_failed_)return false;\n', text.index('const double renderer_flush_ms='))
    end = text.index('        if(!cells_||cell_size_', start)
    block = text[start:end]
    anchor = '        readback *read=nullptr;for(auto &r:readbacks_)if(!r.fence){read=&r;break;}\n'
    if text.count(anchor) != 1:
        raise ValueError('Retirement reference anchor changed')
    text = text[:start] + text[end:]
    text = text.replace(anchor, block + anchor, 1).replace(
        '        // Retire already completed SDL submissions before allocating this\n'
        "        // frame's buffers or preparing new static geometry resources.\n",
        '        // The existing completed-fence cleanup below can then reclaim finished\n'
        '        // SDL submissions before this frame starts cycling its own resources.\n', 1)
    raw = text.encode('utf-8')
    if hashlib.sha256(raw).hexdigest() != REFERENCE_SHA:
        raise ValueError('Unreviewed parent backend reconstruction')
    return raw


def run_build(command, upstream):
    require_ci(os.environ)
    active = subprocess.Popen(command, cwd=upstream)
    try:
        code = active.wait(timeout=600)
        if code:
            raise subprocess.CalledProcessError(code, command)
    except subprocess.TimeoutExpired:
        # Only the owned CI compiler tree; never a process-name/global kill.
        subprocess.run(['taskkill.exe', '/PID', str(active.pid), '/T', '/F'],
                       check=False, timeout=30)
        active.wait(timeout=15)
        raise


def run(args):
    require_ci(os.environ)  # Before any path access, mutation, build or launch.
    repo = below(args.repository, os.environ['GITHUB_WORKSPACE']).resolve(strict=True)
    upstream = below(args.upstream, os.environ['GITHUB_WORKSPACE']).resolve(strict=True)
    game = below(args.game, os.environ['RUNNER_TEMP']).resolve(strict=True)
    out = below(args.output, os.environ['RUNNER_TEMP'])
    payload = below(args.payload, os.environ['GITHUB_WORKSPACE']).resolve(strict=True)
    if out.exists() or out.is_relative_to(game) or game.is_relative_to(out):
        raise ValueError('A new evidence directory outside the existing game is required')
    git = ['git', '-C', str(repo)]
    head = subprocess.check_output(git + ['rev-parse', 'HEAD'], text=True).strip()
    tree = subprocess.check_output(git + ['rev-parse', 'HEAD^{tree}'], text=True).strip()
    subprocess.run(git + ['diff', '--exit-code', 'HEAD', '--'], check=True)
    bundle = json.loads((payload / 'first-person-bundle.json').read_text(encoding='utf-8-sig'))
    if head != os.environ['GITHUB_SHA'] or head != bundle['ncmm_source_commit']:
        raise ValueError('Workflow, checkout and shipping bundle differ')
    host, module = game / 'cataclysm-tiles.ncmm.exe', game / 'code_mods/FirstPersonView/ncmm_mod.dll'
    shipping_hash, module_hash = sha(host), sha(module)
    if shipping_hash != bundle['host_sha256'] or module_hash != bundle['files']['code_mods/FirstPersonView/ncmm_mod.dll']:
        raise ValueError('Existing fixture does not contain the shipping Host/module')
    parser_path = Path(__file__).parent / 'verify_turn_attribution.py'
    if sha(parser_path) != PARSER_SHA:
        raise ValueError('Original turn/stair verifier changed')
    header = upstream / 'src/ncmm_gpu_backend.hpp'
    current = header.read_bytes()
    if current != (repo / 'experiments/graphics_bridge/ncmm_gpu_backend.hpp').read_bytes():
        raise ValueError('Prepared and repository backends differ')
    reference = reference_backend(current)
    source = json.loads((upstream / 'first-person-source.json').read_text(encoding='utf-8-sig'))
    packaged_source = json.loads((payload / 'host/first-person-source.json').read_text(encoding='utf-8-sig'))
    if source != packaged_source or source['source_commit'] != bundle['source_commit'] or source['status'] != 'experimental-not-certified':
        raise ValueError('Prepared and packaged Native source/SDK receipts differ')
    current_inputs = {name: sha(upstream / 'src' / name) for name in source['inputs']}
    if current_inputs != source['inputs'] or len(current_inputs) <= 100:
        raise ValueError('Prepared Native inputs do not match their complete receipt')
    reference_inputs = dict(current_inputs, **{'ncmm_gpu_backend.hpp': REFERENCE_SHA})
    out.mkdir()
    save(out / 'native-inputs.json', {'candidate': current_inputs, 'reference': reference_inputs})
    shutil.copyfile(host, out / 'candidate-host.exe')
    engine = upstream / 'cataclysm-tiles.exe'
    if sha(engine) != shipping_hash:
        raise ValueError('Shipping and current upstream Host differ')
    pdbs = list(upstream.glob('*.pdb'))
    pdb_hashes = {pdb.name: sha(pdb) for pdb in pdbs}
    for pdb in pdbs:
        shutil.copyfile(pdb, out / ('shipping-' + pdb.name))
    loader = upstream / 'src/ncmm_loader.cpp'
    loader_times = loader.stat().st_atime_ns, loader.stat().st_mtime_ns
    command = ['msbuild', '-m', '-p:Configuration=Release', '-p:Platform=x64', '-p:CLToolExe=cl.bat',
               '-p:CLToolPath=' + os.environ['NCMM_SCCACHE_WRAPPER_DIR'], '-p:TrackFileAccess=false',
               '-p:UseMultiToolTask=true', '-p:_CDDA_USE_CCACHE=true', '-target:Cataclysm-vcpkg-static',
               'msvc-full-features/Cataclysm-vcpkg-static.sln']
    receipt = dict(schema=1, diagnostic_only=True, ordinary_game_performance=False,
                   whole_historical_parent_measured=False, source_commit=head, source_tree=tree,
                   engine_commit=bundle['source_commit'], workflow_run=os.environ['GITHUB_RUN_ID'],
                   reference_backend_source=REFERENCE_SOURCE, candidate_backend_source=CANDIDATE_SOURCE,
                   reference_backend_sha256=REFERENCE_SHA, candidate_backend_sha256=CANDIDATE_SHA,
                   turn_parser_sha256=PARSER_SHA, module_sha256=module_hash,
                   native_inputs_sha256=sha(out / 'native-inputs.json'),
                   native_source_receipt_sha256=sha(upstream / 'first-person-source.json'),
                   build_command=command, pairs=7, trials=[], phase='build-reference',
                   runner_sha256=sha(Path(__file__)), verifier_sha256=sha(Path(__file__).with_name('verify_retirement_pair.py')),
                   host_restored=False, source_restored=False, fixture_restored=False,
                   host_sha256={'candidate': shipping_hash},
                   full_qualification_gates_unchanged=True, failure_type=None, failure_stage=None)
    save(out / 'pair.json', receipt)
    try:
        try:
            header.write_bytes(reference)
            # TrackFileAccess=false is the canonical build setting. Touch the
            # including TU explicitly rather than trusting header dependency logs.
            now = time.time_ns()
            os.utime(loader, ns=(now, now))
            run_build(command, upstream)
            reference_hash = sha(engine)
            if reference_hash == shipping_hash or sha(header) != REFERENCE_SHA:
                raise ValueError('Reference Host was not rebuilt from the parent backend')
            shutil.copyfile(engine, out / 'reference-host.exe')
            receipt['host_sha256']['reference'] = reference_hash
        finally:
            header.write_bytes(current)
            os.utime(loader, ns=loader_times)
            shutil.copyfile(out / 'candidate-host.exe', engine)
            for pdb in pdbs:
                shutil.copyfile(out / ('shipping-' + pdb.name), pdb)
            receipt['source_restored'] = all(sha(upstream / 'src' / name) == value for name, value in current_inputs.items())
            save(out / 'pair.json', receipt)
        if not receipt['source_restored']:
            raise ValueError('Prepared Native source was not restored')
        immutable = {}
        for directory in ('config', 'code_mods', 'gfx', 'data', 'font', 'sound', 'lang'):
            directory_root = game / directory
            if directory_root.exists():
                below(directory_root, game)
            for file in sorted(directory_root.rglob('*')):
                if file.is_symlink() or getattr(file, 'is_junction', lambda: False)():
                    raise ValueError('Fixture links are not allowed')
                if file.is_file():
                    immutable[file.relative_to(game).as_posix()] = sha(file)
        config_root = game / 'config'
        config_snapshot = {file.relative_to(config_root): file.read_bytes()
                           for file in config_root.rglob('*') if file.is_file()}
        def restore_config():
            # Only this already checked owned CI config directory is reset.
            below(config_root, game)
            for file in config_root.rglob('*'):
                if file.is_symlink() or getattr(file, 'is_junction', lambda: False)():
                    raise ValueError('Fixture config link appeared')
                if file.is_file() and file.relative_to(config_root) not in config_snapshot:
                    file.unlink()
            for name, content in config_snapshot.items():
                file = config_root / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(content)
        save(out / 'fixture-files.json', immutable)
        receipt['fixture_digest'] = sha(out / 'fixture-files.json')
        # This is a bounded stair comparison, separate from the unchanged full
        # qualification's 60/1800-second soak. Both variants use the same 60 s.
        env = {k: v for k, v in os.environ.items() if not k.startswith('NCMM_FP_')}
        env.update(NCMM_FP_GAMEPLAY_AUDIT='1', NCMM_FP_APPEARANCE_SMOKE='1', NCMM_FP_SOAK_SECONDS='60', SDL_ASSERT='abort')
        for pair in range(7):
            for order in range(2):
                kind = ('reference', 'candidate')[(pair + order) % 2]
                directory = out / f'pair-{pair + 1:02d}-{kind}'
                directory.mkdir()
                receipt.update(phase='reset-world', active_trial=directory.name)
                save(out / 'pair.json', receipt)
                restore_config()
                shutil.copyfile(out / (kind + '-host.exe'), host)
                subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                                str(Path(__file__).parent / 'Reset-SmokeWorld.ps1'), '-GameRoot', str(game)], check=True)
                if any(sha(game / name) != value for name, value in immutable.items()):
                    raise ValueError('Fixture settings/assets changed between children')
                report_path = game / 'ncmm/first-person-gameplay-audit.json'
                old_time = report_path.stat().st_mtime_ns if report_path.exists() else None
                identity = dict(source_commit=head, engine_commit=bundle['source_commit'],
                                host_sha256=sha(host), module_sha256=sha(module), fixture_digest=receipt['fixture_digest'])
                receipt['phase'] = 'child'
                save(out / 'pair.json', receipt)
                result = run_child([str(host), '--ncmm-gameplay-smoke', '--seed', 'ncmm-gameplay-0121',
                                    '--userdir', str(game)], game, env, directory, identity, timeout=600)
                if result['exit_code'] != 0 or sha(host) != receipt['host_sha256'][kind] or sha(module) != module_hash:
                    raise ValueError('Native child failed or timed binary changed')
                if not report_path.is_file() or report_path.stat().st_mtime_ns == old_time:
                    raise ValueError('Fresh gameplay report missing')
                shutil.copyfile(report_path, directory / 'gameplay.json')
                stair_metrics(json.loads(report_path.read_text(encoding='utf-8-sig')))
                receipt['trials'].append(dict(pair=pair, order=order, kind=kind, directory=directory.name,
                                             report_sha256=sha(directory / 'gameplay.json'),
                                             process_sha256=sha(directory / 'process.json')))
                save(out / 'pair.json', receipt)
    except Exception as error:
        receipt.update(failure_type=type(error).__name__, failure_stage=receipt['phase'])
        raise
    finally:
        shutil.copyfile(out / 'candidate-host.exe', host)
        receipt['host_restored'] = (sha(host) == shipping_hash and sha(engine) == shipping_hash
                                    and all(sha(upstream / name) == value for name, value in pdb_hashes.items()))
        if 'restore_config' in locals():
            restore_config()
        if 'immutable' in locals():
            receipt['fixture_restored'] = all(sha(game / name) == value for name, value in immutable.items())
        save(out / 'pair.json', receipt)
    receipt.update(phase='complete', active_trial=None)
    save(out / 'pair.json', receipt)
    save(out / 'verified.json', verify(out))
    print('Native retirement backend ablation complete; original full qualification and performance admission remain separate.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    for name in ('repository', 'upstream', 'game', 'payload', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    run(parser.parse_args())
