# NCMM v0.5.0 — Foundation Hardening

NCMM (Neversalimus Code Mod Manager) — экспериментальная платформа для native/code-модов Cataclysm:DDA.

## UX игрока

Игрок **не устанавливает** Git, MSYS2, GCC, CMake или Visual Studio.

1. Скачать `NCMM_Runtime_v0.5.0.zip`.
2. Распаковать.
3. Запустить `NCMM_Setup.exe`.
4. Выбрать папку CDDA и нажать `Install / Repair NCMM + AWS`.
5. Запускать игру как обычно — из CatLauncher, Catapult или ярлыка.

Setup автоматически обнаруживает существующие установки CatLauncher как удобство, но NCMM от CatLauncher не зависит.

Начиная с v0.3.2 Setup безопаснее работает с несколькими установками CDDA: если найдено больше одной версии, цель не выбирается автоматически. В списке показываются build, короткий source commit и полный путь; перед установкой/восстановлением в multi-install сценарии требуется подтверждение точной цели. После установки Setup показывает target и SHA-256 установленного bootstrap.

Если в папке уже установлен ранний CML prototype, NCMM Setup распознаёт его по сохранённому SHA bootstrap и мигрирует на NCMM, сохраняя существующий `cataclysm-tiles.vanilla.exe`.

## Fail-closed

`cataclysm-tiles.exe` становится маленьким bootstrap, оригинал хранится как `cataclysm-tiles.vanilla.exe`.

При каждом запуске bootstrap:

- вычисляет SHA-256 оригинального exe;
- читает `commit sha` из штатного `VERSION.txt` CDDA;
- принимает локальный host только при точном совпадении binding;
- если host отсутствует, запрашивает публичный NCMM feed по HTTPS;
- скачивает host только для **точного SHA vanilla exe**;
- проверяет SHA-256 скачанного host до установки;
- если чего-либо не хватает или сеть недоступна — запускает vanilla CDDA.

Если NCMM-host не дошёл до `ready` во время инициализации, `boot.pending` остаётся на диске. Следующий запуск создаёт `ncmm.auto_disabled` и идёт в vanilla.

Ручное отключение: создать `ncmm/ncmm.disabled` или запустить с `--ncmm-vanilla`.

## AWS — первый code-mod

`Advanced World Settings` не патчит CDDA напрямую. Он использует `NCMM Host API v1` и требует capability `world_options.v1`.

Он возвращает в Advanced World Generation пять штатных скрытых опций CDDA:

- `SPAWN_DENSITY`
- `ITEM_SPAWNRATE`
- `MONSTER_SPEED`
- `MONSTER_RESILIENCE`
- `EVOLUTION_INVERSE_MULTIPLIER`

Перед изменением AWS проверяет все пять контрактов. Если хотя бы один не подходит, мод отключается целиком и не оставляет частично применённое состояние.

## Module Contract Layer (v0.4.0)

`mod.json` теперь является обязательным pre-load контрактом для native code-мода. Host проверяет до `LoadLibrary`:

- `id`, `version` и `loader_api`;
- все `requires` против capability registry host;
- отсутствие duplicate module id.

После загрузки DLL host дополнительно проверяет совпадение `id`, `version` и полного набора required capabilities между `mod.json` и `ncmm_mod_descriptor_v1`. Несовпадение отключает только конкретный модуль.

Host публикует `ncmm/modules.state.json` со списком capabilities и состоянием каждого модуля (`loaded`, `disabled`, `rejected`, `failed`) плюс machine-readable reason. MCM показывает rejected/failed state вместо безымянного `not loaded`.

Host API v1 сохранён бинарно совместимым. В v0.4.0 в хвост структуры добавлены `get_host_version`, `get_loader_api`, `get_capability_count`, `get_capability`. Новые модули должны требовать `host_info.v1` перед использованием этих полей.

Capabilities v0.4.0:

- `core.v1`
- `world_options.v1`
- `locale.v1`
- `module_contract.v1`
- `host_info.v1`

## Hardening v0.5.0

- Setup получил read-only `Diagnostics` с проверкой bootstrap/vanilla/host SHA, binding, source commit, AWS payload и crash-loop markers.
- `Repair NCMM State` удаляет только `boot.pending` и `ncmm.auto_disabled`, пишет аудит в `ncmm/repair.log` и не трогает exe/binding/modules/manual disable.
- bootstrap публикует runtime state, host и binding через безопасную same-volume замену; ошибка замены не требует предварительного удаления рабочего destination.
- `boot.ready` больше не может остаться ложным маркером для нового host launch; если сам `Process.Start` не состоялся, `boot.pending` очищается и это не считается host crash-loop.
- manifests ограничены по размеру и валидируются строже (`id`, длины, `failure_policy=disable`, обязательный `core.v1`, уникальные capability requirements).
- duplicate module IDs определяются до загрузки DLL и отклоняются симметрично.
- `modules.state.json` публикуется через replace/write-through на Windows.

## Как поддерживаются experimental-сборки

GitHub Actions раз в час смотрит новые `cdda-experimental-*` releases. Для неизвестной версии:

1. checkout точного upstream tag;
2. проверка source-contract NCMM;
3. если контракт не подходит — версия отмечается `unsupported` для текущей ревизии host patch;
4. если подходит — сборка host тем же Windows/MSVC семейством, что использует официальный release CDDA;
5. скачиваются официальные Windows release ZIP и вычисляются SHA их `cataclysm-tiles.exe`;
6. публикуется NCMM host;
7. feed обновляется mapping'ом `vanilla exe SHA -> certified host`.

При изменении host patch ранее rejected-версии снова становятся кандидатами на проверку.

## Feed

По умолчанию runtime использует:

`https://raw.githubusercontent.com/Neversalimus/Cataclysm/master/ncmm-platform/feed/index.json`

Для тестового mirror можно создать `ncmm/feed.url` с другим HTTPS URL.

`--ncmm-refresh` принудительно проверяет feed. Для штатного `raw.githubusercontent.com` runtime 0.3.1 добавляет cache-busting token и отключает локальный HTTP cache, поэтому только что опубликованный certified host не должен скрываться за устаревшим CDN/HTTP cache.

## Runtime Diagnostics (v0.3.3)

Bootstrap атомарно обновляет `ncmm/runtime.state.json`. В нём сохраняются версия runtime/loader API, source commit, SHA vanilla/host/binding, состояние certified host, feed, выбранный режим запуска, crash-loop markers и последний exit code.

`--ncmm-diagnose` выполняет read-only локальную диагностику и завершает работу без запуска CDDA, без network refresh и без изменения crash-loop state.

## Текущий seed

Первый обязательный CI target:

`cdda-experimental-2026-09-23-0546`

Это версия, на которой уже подтверждён bootstrap через реальный CatLauncher.

## Compatibility Engine v0.5.0

`compat/contracts.json` — первый machine-readable реестр source contracts NCMM.
`ci/Test-SourceContracts.ps1` проверяет upstream до патча и пишет `.ncmm_contract_report.json`.
Контрактный отчёт включается в metadata certified host. Изменение реестра входит в patch revision,
поэтому ранее rejected experimental автоматически становятся кандидатами на повторную сертификацию.

Новые стабильные capability-примитивы Host API v1:
- `events.turn.v1` — optional `ncmm_on_turn_v1` callback один раз за игровой turn;
- `character_state.v1` — namespaced int64 state в сериализуемых values текущего персонажа;
- `ui.basic.v1` — host-owned choice menu/message primitives;
- `compatibility.v1` — host собран через source-contract preflight v1.

## Survivor Progression v0.1.0

Первый gameplay vertical slice для NCMM 0.5:
- уровень, XP и perk points сохраняются вместе с персонажем;
- 1 survival XP за игровую минуту;
- 30 уровней в текущем техническом каркасе;
- первый рабочий perk `Fast Learner`: стоимость 1 point, удваивает survival XP;
- при level-up открывается базовое окно выбора;
- smoke-test автоматически проходит полный цикл level -> point -> perk -> persistence API -> doubled XP.

Баланс и survival-time XP временные: цель v0.1 — доказать стабильный end-to-end code-mod API.

## NCMM 0.5.1 — доступ в игре и переназначаемые действия

- Менеджер NCMM теперь доступен прямо в загруженном мире как обычное действие ввода: F2 по умолчанию.
- Действия NCMM участвуют в штатном меню назначения клавиш CDDA, поэтому F2 и модульные клавиши можно переназначать без отдельного конфигуратора.
- Модули с `ui_hotkey` получают собственное игровое действие; Survivor Progression использует F1 по умолчанию.
- Опции Advanced World Settings с контрактом `COPT_WORLDGEN_ONLY` видимы не только при создании мира, но и во вкладке «Текущий мир» уже загруженного мира; сохранение использует штатный механизм world options CDDA.
- Добавлен `gameplay_input.source.v1`, поэтому сертификация fail-closed проверяет точки интеграции input/handle_action до изменения исходников.
- Build-HostPackage сначала использует канонический `cataclysm-tiles.exe` в корне upstream — тот же layout, который подтвердил локальный MSVC build.

## NCMM 0.5.2 — input hotfix и layout API для world options

- F1/F2 defaults теперь регистрируются как эквивалент штатного `keyboard_any`: одновременно keycode + keychar. Это исправляет функцию-клавиши в конфигурациях CDDA, где DEFAULTMODE фактически работает в keychar.
- Пользовательские переназначения не перезаписываются: dual-mode пара применяется только как базовый default.
- NCMM actions также регистрируются перед открытием штатного меню клавиш из главного меню, поэтому NCMM Manager и UI модулей можно переназначать обычным интерфейсом CDDA.
- Добавлен `world_options.layout.v1`: сворачиваемые группы world options и безопасный string-select поверх существующего скрытого string option.
- Compatibility Engine расширен отдельным `world_options_layout.source.v1`; сертификация по-прежнему fail-closed.

## NCMM 0.6.0 — Gameplay Modifier API

- Добавлен `character.modifiers.v1`: ограниченный белым списком runtime API для числовых бонусов персонажа.
- Модули не патчат Character напрямую и не держат указатели на внутренности CDDA: они регистрируют модификаторы у NCMM host.
- Host агрегирует бонусы нескольких модулей и применяет их только к avatar через сертифицированные source hooks.
- Первый набор: STR/DEX/PER/INT, speed, move cost, max stamina, carry weight, dodge, melee hit, healing, reading speed, crafting speed.
- Добавлен fail-closed `character_modifiers.source.v1` для пяти затронутых подсистем CDDA.
- Modifier registry очищается при shutdown; модули восстанавливают runtime-бонусы из собственного persistent state после загрузки персонажа.

## NCMM 0.6.1 / Survivor Progression 0.8.1 — safe polish

- `character.modifiers.v1` получил per-modifier bounds вместо общего ±500 на входе.
- Modifier API принимает runtime effects только для зарегистрированного module id.
- Namespace модификаторов очищается перед init и при init failure, а также перед новым initialize.
- Shutdown callback каждого native-модуля изолирован catch-all; ошибка одного модуля не мешает очистке остальных.
- Healing bonus больше не усиливает отрицательный/дегенеративный healing rate.
- Survivor 0.8.1: overview активных эффектов, более понятные причины lock, точный refund preview при respec.
- Баланс 60 перков и уровни/стоимости не менялись.

## NCMM 0.6.2 — callback ownership и ready-state hardening

- `character_state.v1` и `character.modifiers.v1` теперь принимают namespaced операции только во время callback того же module id.
- Каждый init/turn/locale/UI/shutdown callback выполняется в host-owned module scope; другой мод не может штатным API читать/писать чужой character state или регистрировать эффекты под чужим id.
- Вызовы descriptor/init native DLL получили catch-all containment и machine-readable причины `descriptor_exception` / `init_exception`.
- Повторный `initialize()` сначала безопасно выгружает старые модули, а `atexit` регистрируется только один раз.
- `boot.ready` сначала пишется во временный файл и публикуется атомарно; `boot.pending` удаляется только после успешной публикации ready marker.
- Runtime CI отдельно проверяет наличие этих hardening-инвариантов в loader source до упаковки runtime.
- ABI Host API v1 и capability-набор не менялись; существующим корректным модулям не требуется миграция.

## NCMM 0.6.3 — Certification & Crash-loop Hardening

- Исправлена ложная ошибка source-contract preflight: вызов PowerShell-скрипта больше не проверяет устаревший `$LASTEXITCODE` от предыдущей native-команды.
- Исправлен existing-marker путь `Apply-NCMMHostPatch.ps1`, где повторная проверка обращалась к ещё не созданным `$ch2/$hh2/...`.
- Patch revision теперь включает `Build-HostPackage.ps1` и сам алгоритм `Get-PatchRevision.ps1`, поэтому изменения упаковки/сертификации не переиспользуют старую ревизию.
- Certified host releases получили immutable release tag с коротким patch-revision suffix; после upload GitHub asset digest сверяется с локальным SHA256 до изменения feed.
- Новый feed сначала удаляет entries старых patch revisions. Текущий rejection удаляет ранее рекламировавшийся host для того же CDDA tag.
- Bootstrap 0.6.3 принимает binding только своей runtime-версии/Loader API и требует patch revision metadata. Feed и entry revision должны совпадать.
- Crash-loop recovery различает `pending без ready` (неуспешная загрузка) и `pending + ready` (host дошёл до ready, но очистка pending не завершилась).
- Перед новым host launch старые ready/tmp markers удаляются строго; неоднозначное состояние приводит к vanilla fallback, а не к запуску наугад.
- Seed build `cdda-experimental-2026-09-23-0546` больше не может быть тихо отмечен как unsupported при зелёном pipeline: такой regression делает build job красным.

## NCMM 0.6.3.1 — CI Recovery + BOM Guard

- Исправлен `.github/workflows/ncmm-runtime.yml`: двойной UTF-8 BOM, из-за которого GitHub Actions отклонял workflow до создания jobs, удалён.
- Добавлен `ci/Test-TextEncoding.ps1`: строгая UTF-8 проверка NCMM text sources, обнаружение UTF-16/UTF-32, embedded U+FEFF и нескольких UTF-8 BOM.
- GitHub workflow-файлы требуют UTF-8 без BOM; для остальных текстовых исходников допускается максимум один UTF-8 BOM для совместимости со старым PowerShell 5.
- Guard имеет self-test с synthetic valid/double-BOM/UTF-16 fixtures.
- Runtime и Certified Hosts запускают guard сразу после checkout; Build-Runtime и Build-HostPackage также вызывают его напрямую.
- Добавлен `.editorconfig`, фиксирующий UTF-8 без BOM/LF для workflow и NCMM text source.
- Это CI-only hotfix: runtime/host protocol остаётся 0.6.3, Host API v1, Survivor 0.8.1 и AWS 0.5.0 не меняются.

## NCMM 0.6.4 — Automated Failure Harness

- Runtime CI теперь запускает реальный скомпилированный `NCMMBootstrap` в изолированных временных CDDA-каталогах, а не только проверяет строки/компиляцию.
- Добавлен synthetic child executable, который имитирует vanilla/host process, успешный `ready`, crash до ready и `ready` без очистки pending.
- Автоматизировано 14 fail-closed сценариев: missing vanilla, valid offline host, manual/forced vanilla, stale runtime binding, empty patch revision, bad host SHA, corrupt binding, crash-loop auto-disable, reset recovery, pending+ready recovery, failure записи auto-disable marker, failure удаления stale ready и diagnostics-only с pending.
- Harness проверяет фактический exit code, выбранный child executable, `runtime.state.json` и crash-loop markers.
- Harness запускается ещё в patch pre-commit на Windows и затем повторно внутри `Build-Runtime.ps1` в GitHub Actions.
- Это тестовая/инфраструктурная версия: Host API остаётся v1; Survivor Progression 0.8.1 и AWS 0.5.0 не меняются.
