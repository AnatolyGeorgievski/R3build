# R3 BCP — образ ноды

**BCP** = Best Current Practice: как сейчас собирается rootfs. Не план прошивки платы.

**Updated:** 2026-10-13. Полный контракт и состав деревьев: `log/2026-10-13 сборка образа Alpine.md`. Короткий сценарий загрузки: `docs/minirootfs-short.md`.

## Граница

- Пакеты: Alpine 3.21, `apk.static` 3.0.8, зеркало `v3.21/main`.
- `--allow-untrusted` только для первого `add alpine-keys`. Дальше проверка по `$ROOTFS/etc/apk/keys`.
- `--no-script --no-cache`. OpenRC нет.
- Живые `/home/george/rootfs-*` и `/home/george/alpine` не пересобирать. Проба — `/tmp`.
- Прошивка (`flash_erase`, `nandwrite`, `mkimage`) не часть сборки rootfs.
- `init.d` скрыт маской `.gitignore` `*.d`. Файлы профиля на диске обязательны.

## Загрузка

ядро → `/sbin/init` (busybox) → `/etc/inittab` → `rcS` / `rcK`.

`rcS` монтирует proc, sys, dev, создаёт `/config` и `/sn`, пишет `/var/log/boot.log`. Прикладной `netd` стартует из образа, не из нового апплета godminer.

## Архитектуры

| apk `--arch` | Загрузчик |
|---|---|
| `armv7` | `/lib/ld-musl-armhf.so.1` |
| `aarch64` | `/lib/ld-musl-aarch64.so.1` |

Минимум, проверенный 2026-10-13: world `alpine-keys` + `busybox`, зависимость `musl`.
