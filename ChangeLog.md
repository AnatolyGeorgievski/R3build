# ChangeLog

Формат близок к Keep a Changelog. Даты — день работы, не semver.
Образ ноды. Канон восстановления контекста: `log/2026-10-13 сборка образа Alpine.md`.

## [2026-10-13]

### Added
- Журнал сборки: `log/2026-10-13 сборка образа Alpine.md`. С него начинается работа над образом.
- `docs/R3_BCP.md` — канон apk и границ, за которые образ не переступает.
- `fetch-apk-static.sh` скачивает `apk-tools-static` с зеркала Alpine. Бинарник `apk.static` в git не входит.
- Вне git, на диске: корневой `rcS`, `profiles/ant/etc/init.d/rcS`, `rcK`. Прежнее дерево сохранено в `$CWD/r3build.local` (`CWD=/home/{name}`).

### Changed
- `alpine-rootfs.sh`: после установки `alpine-keys` пакеты ставятся без `--allow-untrusted`.

### Not done
- `install-init.sh` в git не затирался. В нём `SRC= "` с пробелом. Локальная копия с правкой и `mkimage` лежит в `r3build.local`.
