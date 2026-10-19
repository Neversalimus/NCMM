"""Prepare a private, digest-pinned SDL graphics fix; never change global vcpkg."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

PINS={'3f7fb352bf492ba521bd9408a0c9f6ce239e8d83','074aa98bd5be3de4c35f154082db32a0e63bb0f1'}
PORT_MANIFEST_SHA256='4510d96c09bbc863570ccbb051f47f75d561ec0d5d05d8a4f50c79ce889993b0'
ENGINE_MANIFEST_SHA256='f523e20ad567b7cf62e1e4eed4f3a5d54d2670a6177a4cb4e406cfdb814ba6af'
VENDOR=Path(__file__).resolve().parent/'third_party/sdl3-port'

def digest(data):return hashlib.sha256(data).hexdigest()

def plan(root,identity,vendor=VENDOR):
    root=Path(root);vendor=Path(vendor)
    if identity not in PINS:raise ValueError('Unknown Native source pin for private SDL preparation')
    raw=(vendor/'manifest.json').read_bytes()
    if digest(raw)!=PORT_MANIFEST_SHA256:raise ValueError('Private SDL manifest identity differs')
    manifest=json.loads(raw)
    files={p.relative_to(vendor).as_posix() for p in vendor.rglob('*') if p.is_file()}
    if files!=set(manifest['files'])|{'manifest.json'}:raise ValueError('Unexpected or missing vendored SDL source')
    data={}
    for name,expected in manifest['files'].items():
        content=(vendor/name).read_bytes()
        if digest(content)!=expected:raise ValueError('Vendored SDL byte identity differs: '+name)
        data[name]=content
    engine=root/'msvc-full-features'
    value=json.loads((engine/'vcpkg.json').read_text(encoding='utf-8-sig'))
    normalized=json.dumps(value,sort_keys=True,separators=(',',':')).encode()
    if digest(normalized)!=ENGINE_MANIFEST_SHA256:raise ValueError('Pinned Native vcpkg manifest differs')
    if (engine/'vcpkg-configuration.json').exists() or (engine/'ncmm-sdl-overlay').exists():
        raise ValueError('Private SDL overlay requires a fresh disposable Native source')
    updates={engine/'vcpkg-configuration.json':b'{"overlay-ports":["ncmm-sdl-overlay"]}\n'}
    for name,content in data.items():
        if name.startswith('overlay/'):
            updates[engine/'ncmm-sdl-overlay'/name.removeprefix('overlay/')]=content
    metadata=dict(sdl_version=manifest['sdl_version'],base_port_version=manifest['base_port_version'],
        private_port_version=manifest['private_port_version'],registry_commit=manifest['registry_commit'],
        source_archive_sha512=manifest['source_archive_sha512'],port_manifest_sha256=digest(raw),
        patch_sha256=manifest['files']['overlay/sdl3/ncmm-graphics-descriptors.patch'],
        original_d3d12_sha256=manifest['original_d3d12_sha256'],patched_d3d12_sha256=manifest['patched_d3d12_sha256'],
        scope=manifest['patch_scope'],runtime_vendor=manifest['runtime_vendor'],windows_build_only=True,global_vcpkg_unchanged=True,
        files={path.relative_to(root).as_posix():digest(content) for path,content in updates.items()})
    return updates,metadata

def apply(root,identity,vendor=VENDOR,verify_only=False):
    root=Path(root);updates,metadata=plan(root,identity,vendor)
    if verify_only:return metadata
    receipt=root/'first-person-source.json'
    value=json.loads(receipt.read_text(encoding='utf-8-sig'))
    if value['source_commit']!=identity or value['status']!='experimental-not-certified' or 'windows_sdl' in value:
        raise ValueError('SDL overlay requires this exact freshly prepared experimental Host')
    value['windows_sdl']=metadata
    # All source/SDK checks finish before the first write. No installation,
    # environment, global registry, save, canonical payload or public ABI changes.
    updates[receipt]=(json.dumps(value,indent=2)+'\n').encode()
    for target,content in updates.items():
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(content)
    return metadata

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source_root',type=Path);parser.add_argument('--verify-only',action='store_true')
    args=parser.parse_args();root=args.source_root.resolve()
    identity=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    print(json.dumps(apply(root,identity,verify_only=args.verify_only)))
