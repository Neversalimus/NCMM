# NCMM v0.3.3 — Runtime Diagnostics + Certified Host Feed

NCMM (Neversalimus Code Mod Manager) — экспериментальная платформа для native/code-модов Cataclysm:DDA.

## UX игрока

Игрок **не устанавливает** Git, MSYS2, GCC, CMake или Visual Studio.

1. Скачать `NCMM_Runtime_v0.3.3.zip`.
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
