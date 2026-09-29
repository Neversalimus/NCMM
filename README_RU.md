# NCMM

NCMM (Neversalimus Code Mod Manager) — платформа для native/code-модов Cataclysm: Dark Days Ahead. Это не форк CDDA: NCMM отдельно поддерживает bootstrap, сертифицированный Host, API модулей, source-contracts, диагностику, CI и дополнительные native-модули.

## Текущий стек

| Компонент | Версия |
| --- | --- |
| NCMM Infrastructure | 0.8.3.1 |
| NCMM Runtime / Host | 0.8.1 |
| Loader ABI | 1 |
| Legacy semantic Host API | 1.9 |
| Queried Host API | 2.0 Core |
| Survivor Progression | 0.12.0 |
| Advanced World Settings | 0.6.2 |

Survivor Progression сохраняет state schema 8. В текущем исходнике 369 узлов перков; ветки интеграций для поддерживаемых модов появляются только при наличии соответствующих активных world-модов.

## Установка

Для обычного игрока нужен пакет **Full**.

1. Скачать `NCMM_Full_v0.8.1.zip` из релиза `ncmm-runtime-v0.8.1`.
2. Распаковать архив вне папки CDDA.
3. Запустить `NCMM_Setup.exe`.
4. Выбрать установленную CDDA и нужные дополнительные модули.
5. Нажать **Install / Repair selected**.
6. Запускать CDDA обычным способом — через CatLauncher, Catapult или ярлык.

Git, Visual Studio, CMake и MSYS2 игроку не нужны. Если для конкретного exe CDDA нет точного certified Host, NCMM работает fail-closed и запускает vanilla CDDA.

В самом репозитории также остаётся `NCMM.cmd` — единая точка входа для разработки и обслуживания: install, verify, update, diagnostics, adapter и selftest. Build-cache намеренно переиспользуется между сборками.

## Что видно игроку

- **NCMM / Настройка модов** встроен в меню настроек CDDA. Текущий интерфейс двухпанельный: список модов слева, версия/статус/описание/горячая клавиша/настройки выбранного модуля справа.
- **Survivor Progression 0.12.0** по умолчанию открывается на F1, но клавиша переназначается штатно через CDDA. В менеджере доступны live-настройки получения опыта и силы прямых стат-перков; схема сохранения осталась 8.
- **Advanced World Settings 0.6.2** использует штатный интерфейс world options. Настройки с областью NEW_MAP / NEW_WORLD доступны во время создания мира, а LIVE / RELOAD остаются в менеджере NCMM.
- В текущем main-коде навигация по интерфейсу перков использует штатный звук CDDA `menu_move`, а в главном меню выводится компактная подпись `NCMM 0.8.1`.
- Ошибка отдельного модуля по возможности изолируется и отражается в machine-readable runtime/module state вместо тихой загрузки несовместимого кода.

## Совместимость и безопасность

NCMM не определяет совместимость по имени папки или версии лаунчера. Runtime сверяет SHA-256 vanilla exe, source commit CDDA, Loader API, версию NCMM и patch revision Host с certified feed. Host публикуется только после source-contract preflight и Windows/MSVC-сертификации конкретного upstream release.

Исходный exe сохраняется как `cataclysm-tiles.vanilla.exe`. При отсутствующем, повреждённом или неподходящем Host запускается vanilla. Состояние bootstrap/runtime пишется в `ncmm/runtime.state.json`, состояние Host и модулей — в `ncmm/modules.state.json`.

## Структура репозитория

- `runtime/` — bootstrap и графический Setup.
- `host_patch/` — интеграция с исходниками CDDA для сборки certified Host.
- `sdk/` — публичный API native-модулей.
- `mods/` — сами native-модули.
- `compat/` — source-contracts, compatibility metadata и package integrity.
- `components/` — каталог устанавливаемых компонентов.
- `ci/` и `.github/workflows/` — сборка, сертификация, feed и regression tests.
- `adapters/` — адаптеры конкретных и родственных сборок CDDA.
- `checkpoints/` — намеренно сохранённые исторические снимки для воспроизводимости.

Текущая архитектура и границы доверия описаны в `ARCHITECTURE.md`.

## Статус проекта

`Neversalimus/NCMM` — единственный канонический репозиторий NCMM. Старый `Neversalimus/Cataclysm` остался только как исторический источник и не участвует в текущей сборке, feed или публикации.

Старые changelog/audit-файлы в корне не являются дополнительными инструкциями для игрока: те, что сохранены, используются как история пакета или регрессионные доказательства.
