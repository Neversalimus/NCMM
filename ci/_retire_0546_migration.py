"""Temporary reviewed migration; removed by its qualification workflow."""
from pathlib import Path
import base64
import hashlib
import json
import re
import shutil
import subprocess
import sys

r = Path(__file__).resolve().parents[1]
u = Path(sys.argv[1]).resolve()
OLD = 'e262adb299a7613b4aedc5f12c08fe0413c56a84'
NEW = '3f7fb352bf492ba521bd9408a0c9f6ce239e8d83'
OT = 'cdda-experimental-2026-09-23-0546'
NT = 'cdda-experimental-2026-10-01-1040'
old_adapter = 'adapters/cdda_2026_09_23_0546.ps1'
new_adapter = 'adapters/cdda_2026_10_01_1040.ps1'
assert subprocess.check_output(['git', '-C', str(u), 'rev-parse', 'HEAD'], text=True).strip() == NEW

def put(p, text):
    path = r / p
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode('utf-8'))

def edit(p, changes):
    text = (r / p).read_bytes().decode('utf-8').replace('\r\n', '\n')
    for before, after in changes:
        if before not in text:
            raise RuntimeError('Missing reviewed anchor in ' + p + ': ' + before[:100])
        text = text.replace(before, after)
    put(p, text)

def identity(p):
    text = (r / p).read_bytes().decode('utf-8').replace('\r\n', '\n')
    for before, after in [(OLD, NEW), (OT, NT), ('2026-09-23-0546', '2026-10-01-1040'),
                          ('2026_09_23_0546', '2026_10_01_1040'), ('cdda_0546', 'cdda_1040'),
                          ('Exact 0546', 'Exact 1040')]:
        text = text.replace(before, after)
    put(p, text)

# Correct the transport transcription of the tag validator before any use.
p = 'ci/host_support_policy.py'
text = (r / p).read_text(encoding='utf-8')
lines = text.splitlines()
assert sum(line.startswith('TAG = ') for line in lines) == 1
lines = [r'TAG = re.compile(r"cdda-experimental-\d{4}-\d{2}-\d{2}-\d{4}\Z")' if line.startswith('TAG = ') else line for line in lines]
put(p, '\n'.join(lines) + '\n')

text = (r / old_adapter).read_bytes().decode('utf-8').replace('\r\n', '\n')
refs = re.findall(r"'(src/[^']+)' = '([0-9a-f]{40})'", text)
assert len(refs) == 27 and not (r / new_adapter).exists()
blob_map = {}
for path, old_hash in refs:
    data = (u / path).read_bytes().replace(b'\r\n', b'\n')
    new_hash = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    blob_map[old_hash] = new_hash
    text = text.replace("'" + path + "' = '" + old_hash + "'", "'" + path + "' = '" + new_hash + "'")
for before, after in [(OLD, NEW), ('2026-09-23-0546-e262adb2', '2026-10-01-1040-3f7fb352'),
                      (OT, NT), ('2026_09_23_0546', '2026_10_01_1040'), ('cdda_0546', 'cdda_1040')]:
    text = text.replace(before, after)
put(new_adapter, text)
(r / old_adapter).unlink()
for path in ['.github/workflows/ncmm-certify.yml', '.github/workflows/ncmm-equipment-doll-pr-host.yml',
             'ci/Test-Infrastructure083.ps1', 'ci/Test-ComponentCatalog.ps1', 'tests/InstallationMatrixHarness.cs']:
    identity(path)
edit('.github/workflows/ncmm-pr-engineering-integration.yml', [(f'          - tag: {OT}\n            source: {OLD}\n', '')])
edit('.github/workflows/ncmm-host.yml', [(OT, NT)])
edit('ci/seed-hosts.txt', [(OT + '\n', '')])
edit('payload/SURVIVOR_0911_0915_v8.7.6.8.ps1', [
    (f'[string]$TargetCommit = "{OLD}"', f'[string]$TargetCommit = "{NEW}"'),
    (f'[string]$TargetTag = "{OT}"', f'[string]$TargetTag = "{NT}"'),
    ('[string]$TargetFolder = "cdda_experimental_2026_09_23_0546"', '[string]$TargetFolder = "cdda_experimental_2026_10_01_1040"'),
    ('[string]$TargetCacheKey = "cdda_0546"', '[string]$TargetCacheKey = "cdda_1040"')])
text = (r / 'ci/Test-SurvivorPayloadContracts.ps1').read_text(encoding='utf-8-sig')
edit('ci/Test-SurvivorPayloadContracts.ps1', [(old_adapter.replace('/', '\\'), new_adapter.replace('/', '\\'))] +
     [(before, after) for before, after in blob_map.items() if before != after and before in text])
edit('ci/Test-LegacyCharacterPoints.ps1', [(f"    '0546'='{OLD}'\n", ''), ("@('0546','1040')", "@('1040')"), ('0546/1040, LF/CRLF', '1040, LF/CRLF')])
edit('mods/LegacyCharacterPoints/README.md', [('exact official 0546 and 1040 sources', 'the exact official 1040 baseline')])
for path in ['mods/ItemGlyphs/CMakeLists.txt', 'ci/test_item_glyphs_source.py', 'tests/item_glyphs_test.cpp']:
    identity(path)
    put(path, (r / path).read_text(encoding='utf-8').replace('CDDA 0546', 'CDDA 1040'))
# These files only fetch obsolete fixed PR187 artifacts, not the normal installer.
removals = {
    '.github/workflows/ncmm-ebm-test-installer.yml': 'a2555ceb1cee242da8cbb66fd97923ea5e10faee',
    'ci/ebm-test/Install_EBM_Test.ps1': '4c8e08732898c1a156628a6965ec24c6a3d119ee',
    'ci/ebm-test/README_RU.txt': '9cc666a5bb3e2f790f4aeec80e152db1c992dd0f',
    'ci/ebm-test/Restore_EBM_Test.ps1': '2241e1f5d2917cc9efac1b6896292fadf8634ced',
    f'compat/feed/commits/{OLD}.json': 'ffc1fc310e0c44b97935a7de573efaadc845be3c'
}
for path, expected in removals.items():
    actual = subprocess.check_output(['git', '-C', str(r), 'rev-parse', 'HEAD:' + path], text=True).strip()
    assert actual == expected, 'Concurrent change in deleted path: ' + path
    (r / path).unlink()
(r / 'ci/ebm-test').rmdir()
edit('compat/compatibility.manifest.json', [('cdda-2026-09-23-0546-e262adb2', 'cdda-2026-10-01-1040-3f7fb352')])
edit('compat/package-files.txt', [(old_adapter.replace('/', '\\'), new_adapter.replace('/', '\\')),
                                (f'compat\\feed\\commits\\{OLD}.json', f'compat\\feed\\commits\\{NEW}.json')])
index = json.loads((r / 'compat/feed/index.json').read_text())
assert all(entry['commit'] != NEW for entry in index['entries'])
index['entries'] = [entry for entry in index['entries'] if entry['commit'] != OLD]
index['entries'].append({'commit': NEW, 'status': 'supported_exact', 'manifest': f'commits/{NEW}.json'})
put('compat/feed/index.json', json.dumps(index, indent=2) + '\n')
manifest = {
    'schema': 1, 'commit': NEW, 'tag': NT, 'build_label': 'cdda_experimental_2026_10_01_1040',
    'status': 'supported_exact',
    'certification': {
        'state': 'exact-source-baseline',
        'note': "Pinned baseline after retiring 0546 at the owner's request on 2026-10-10. Reference blobs were verified from this exact official source commit. This entry is not a binary certificate and does not claim player live verification; current-revision Host publication remains gated by the real CI certification and installation workflows."
    },
    'adapter': {'id': 'cdda-2026-10-01-1040-3f7fb352', 'inherits': 'base-cdda-2026-series-v1', 'support': 'exact'}
}
put(f'compat/feed/commits/{NEW}.json', json.dumps(manifest, indent=2) + '\n')
for path, heading, body, marker in [
    ('README.md', '## Supported CDDA baseline', "As of 2026-10-10, `cdda-experimental-2026-09-23-0546` is retired at the owner's request. The pinned source/install/PR qualification baseline is `cdda-experimental-2026-10-01-1040` (`3f7fb352bf492ba521bd9408a0c9f6ce239e8d83`). Newer experimentals still require their own exact-identity certified Host; retirement does not relax SHA, source-contract, rollback or save-safety checks. The old PR187/0546-only preview installer is removed. Historical reports and already published releases are retained; no existing saves or backups are deleted.", '## Current stack'),
    ('README_RU.md', '## Поддерживаемая базовая версия CDDA', 'С 10 октября 2026 года поддержка `cdda-experimental-2026-09-23-0546` прекращена по решению владельца проекта. Закреплённая база исходников, установки и проверок PR — `cdda-experimental-2026-10-01-1040` (`3f7fb352bf492ba521bd9408a0c9f6ce239e8d83`). Более новые experimental по-прежнему требуют отдельного сертифицированного Host с точным совпадением версии и хешей. Проверки исходников, откат и защита сохранений не ослаблены. Одноразовый тестовый инсталлер PR187 только для 0546 удалён. Исторические отчёты и опубликованные релизы сохранены; существующие сохранения и резервные копии не удаляются.', '## Текущий стек')
]:
    edit(path, [(marker, heading + '\n\n' + body + '\n\n' + marker)])

def one(path, before, after):
    text = (r / path).read_bytes().decode('utf-8').replace('\r\n', '\n')
    assert before in text, (path, before[:100])
    put(path, text.replace(before, after, 1))

p = '.github/workflows/ncmm-host.yml'
one(p, "      - 'ci/seed-hosts.txt'", "      - 'ci/seed-hosts.txt'\n      - 'ci/retired-hosts.json'\n      - 'ci/host_support_policy.py'\n      - 'ci/test_host_support_policy.py'")
one(p, '          if [ -n "${REQUESTED_TAG:-}" ]; then\n            tags=', '          if [ -n "${REQUESTED_TAG:-}" ]; then\n            python3 ci/host_support_policy.py check --tag "$REQUESTED_TAG"\n            tags=')
one(p, "cat ci/seed-hosts.txt /tmp/upstream_tags.txt 2>/dev/null | awk 'NF && !seen[$0]++' > /tmp/candidates.txt", 'cat ci/seed-hosts.txt /tmp/upstream_tags.txt | python3 ci/host_support_policy.py filter-tags > /tmp/candidates.txt')
one(p, '          old_rev=$(jq -r', '          python3 ci/host_support_policy.py prune-feed "$index"\n\n          old_rev=$(jq -r')
one(p, '            short_rev=${PATCH_REVISION:0:12}', '            python3 ci/host_support_policy.py check --tag "$tag" --commit "$commit"\n\n            short_rev=${PATCH_REVISION:0:12}')
one(p, '      - name: Select upstream releases', '      - name: Validate active baseline and retirement policy\n        run: python3 ci/test_host_support_policy.py\n\n      - name: Select upstream releases')
p = '.github/workflows/ncmm-text-audit.yml'
one(p, "      - 'ci/Build-HostPackage.ps1'", "      - 'ci/Build-HostPackage.ps1'\n      - 'ci/retired-hosts.json'\n      - 'ci/host_support_policy.py'\n      - 'ci/test_host_support_policy.py'")
one(p, '      - name: Audit checked-in player-facing text', '      - name: Test active baseline and retired-Host publication policy\n        run: python ci/test_host_support_policy.py\n\n      - name: Audit checked-in player-facing text')
p = '.github/workflows/ncmm-pr-engineering-integration.yml'
one(p, '      - name: Verify source, canonical payload and layout before compilation', '      - name: Verify active policy and every exact 1040 reference blob\n        run: python ncmmrepo/ci/test_host_support_policy.py --source-root upstream\n      - name: Verify source, canonical payload and layout before compilation')
for relative in ['ci/host_support_policy.py', 'ci/retired-hosts.json']:
    for path, entry in [('ci/patch-revision-files.txt', relative), ('compat/package-files.txt', relative.replace('/', '\\'))]:
        text = (r / path).read_text()
        assert entry not in text
        put(path, text.rstrip() + '\n' + entry + '\n')
p = 'compat/package-files.txt'
put(p, (r / p).read_text().rstrip() + '\nci\\test_host_support_policy.py\n')
for relative in ['ci/host_support_policy.py', 'ci/retired-hosts.json', 'ci/test_host_support_policy.py']:
    windows_path = relative.replace('/', '\\')
    one('ci/Sync-CanonicalPayload.ps1', "    'ci\\toolchain.lock.json',", "    '" + windows_path + "',\n    'ci\\toolchain.lock.json',")
    path = 'payload/SURVIVOR_0911_0915_v8.7.6.8.ps1'
    text = (r / path).read_bytes().decode('utf-8')
    marker = "Write-NcmmCanonicalPayloadFile 'ci\\toolchain.lock.json' '"
    assert text.count(marker) == 1
    content = (r / relative).read_bytes().replace(b'\r\n', b'\n')
    line = "Write-NcmmCanonicalPayloadFile '" + windows_path + "' '" + base64.b64encode(content).decode() + "'\n"
    put(path, text.replace(marker, line + marker, 1))
edit('ci/Test-CanonicalPayloadSync.ps1', [('Count -ne 35', 'Count -ne 38'), ('35 sources', '38 sources')])
subprocess.run([sys.executable, str(r / 'ci/host_support_policy.py'), 'prune-feed', str(r / 'feed/index.json')], check=True)
print('Retirement migration complete:', len(refs), 'verified 1040 source blobs,', sum(a != b for a, b in blob_map.items()), 'changed blobs.')
