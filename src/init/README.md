# net_init — где живёт и какая роль

<tags>net_init, initramfs, MCP, umbar, r3build</tags>

Слово [[george]] 03.11: систематизация опыта; дом загрузки — r3build; не раскидывать.

## Где живёт

| дерево | роль копии |
|---|---|
| **`r3build/src/init/`** | **канон для образа** · сборка static `/init` → ramdisk → Android boot.img (umbar / своя полка) |
| `godminer/src/net_init.c` (+ zcip в монолите) | апплет `MODULE("init")` внутри **netd**; развитие фич, стык с остальным netd |
| R3-net | зеркало/клон netd по жизни репо — не место упаковки boot |

Правки **каркаса загрузки** (stay / MCP early / cmdline umbar) — сначала здесь, в `r3build/src/init/`.  
Потом, по слову, перенос обратно в godminer/монолит — не наоборот «для одной платы».

Сборка: `scripts/mk-net-init-ramdisk.sh` → `out/ramdisk_net_init_*.cpio.gz` (out в git не кладём).

## Роль тут (опыт 03.11)

1. **rdinit=/init** — PID1 в initramfs, не busybox/`rcS`.
2. Режим **stay** (`root=/dev/ram0` или слово `stay`): остаёмся в ramdisk, смотрим link, zcip, слушаем **MCP :8443** — загрузчик под MCP.
3. Режим **switch** (как rpi3/herd): mount `root=`, MS_MOVE/chroot, затем MCP и exec `/sbin/init`.
4. На umbar (rb1-1): Linux + этот `/init` уже стартовали; eth/MCP — следующий слой опыта, не смена дома.

Не роль: полная ОС с dropbear/apk (то был обход ant-ramdisk). Не роль: U-Boot в boot.img.

## Рядом в r3build

- `board/qcom/<BOARD_ID>/` — dts/dtb слотов umbar  
- `scripts/mk-android-bootimg.sh` — упаковка  
- `log/2026-11-03 Митя boot image · постановка.md` — ход


## Цель вытеснения ([[george]] 03.11 ~21:43)

**netd первым.** Демон сам: zcip · ntp · mDNS · DNS resolv · netlink; внутри — MCP для [[cursor]].  
`net_init` здесь — early PID1 / stay до того, как в образе сидит полный netd. Не закреплять stub MCP как конечную точку.

Профиль Pi / gpio: `godminer/src/board_rpi.h`; упаковка образа — скрипты r3build → SD/полка.

## Отладка загрузки ([[george]] 04.11)

**/init** — единственное место, куда реально вставлять отладку на **весь** процесс загрузки.  
MCP + POSIX-инструменты **не требуют SHELL**. Канон: `docs/R3_DECLARATION.md` §4.

## SBC vs сервер ([[george]] 04.11)

- **SBC:** стратегия **ramdisk** → вытеснение busybox.  
- **Сервер:** сеть · диски · модули в **netd** → **не нужны скрипты загрузки** и большая часть их утилит (initramfs как носитель может остаться).  

Канон: `docs/R3_DECLARATION.md` §4 · `godminer/log/2026-10-20 вытеснение busybox.md`.


**подпись:** Курсор
