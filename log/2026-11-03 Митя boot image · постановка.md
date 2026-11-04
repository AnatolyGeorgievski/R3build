# 2026-11-03 — Митя boot image · постановка в r3build

<tags>r3build, митя, lumag, boot, android-image, zImage, Image.gz, ядро</tags>

<topo>
name: [[cursor]]
links:
  - [[george]]
  - [[lumag]]
path: log/2026-11-03 Митя boot image · постановка.md
</topo>

<refs>
- godminer/log/2026-11-03 Мите · вопрос · откуда boot.img.md
- https://github.com/linux-msm/cdba
- godminer/log/2026-11-03 литература · Android boot.img под платы Мити.md
</refs>

## Постановка ([[george]] 18:23)

Добавить в r3build **правила сборки** образов под лабу Мити (прежде всего тестирование ядра):

| класс досок | форма |
|---|---|
| большая часть | Android boot image · **zImage** + **concat dtb** |
| некоторые | Android image **V2** · **Image.gz** или **Image** |

Канал доставки ([[lumag]] 18:42): через **cdba** (встроен fastboot) — `cdba -b <board> -h umbar.lab <boot.img>`.

Тело: `scripts/mk-android-bootimg.sh` + `board/qcom/*.conf` (слово [[george]] 20:12: как у Мити, под netd; не собирается → вопросы).

**подпись:** Курсор

## Где лежит образ для Tab

На **клиенте** рядом с вызовом `cdba` (cwd / явный путь). Собрали в r3build → положили на видное место → `cdba … boot.img` (Tab). umbar сам список образов нам не отдаёт.


## Ход 03.11 ~19:33

| кто | |
|---|---|
| [[lumag]] | Собрать руками? |
| [[george]] | **Посмотреть твой и собрать свой.** Так быстрее. |

Канон: не копировать его скрипты вслепую и не ждать «собери за нас» — взять **его рецепт/пример** → правила в **r3build** → наш `boot.img` → `cdba … boot.img`.

## Сделано в дереве ([[cursor]] 03.11)

| путь | роль |
|---|---|
| `scripts/mk-android-bootimg.sh` | legacy (cat+mkbootimg) и v2; дыры → Q1..Qn exit 3 |
| `board/qcom/rb1.conf` | цифры из `build-u-boot-rb1.sh` Мити |
| `board/qcom/rb5.conf` | Image.gz + v2; **ABOOT_BASE пуст** → вопрос |
| `board/qcom/_template.conf` | шаблон слота |
| `out/boot-<BOARD_ID>.img` | видное место для Tab/cdba |

Ramdisk: пустой по умолчанию; `RAMDISK=` из alpine/netd когда будет.

## Зерно · base ([[george]] 03.11)

**Base is not important** / ABOOT_BASE не важен. В скрипте пусто → `0x0`; не блокер сборки.


## Инструмент · skales vs Android ([[lumag]] 20:17–18)

Два mkbootimg:

| инструмент | пакет / путь |
|---|---|
| **Android** mkbootimg | пакет `mkbootimg` (AOSP) — уже поставили на arc2 |
| **Skales** mkbootimg | пакет `skales` (Qualcomm/96boards tooling) |

Митя: **Can't promise that 0x0 works** — base платы может быть нужен для boot; не обещать 0x0. В скрипте default 0x0 остаётся fallback; для слота брать цифру из conf/его примера, когда есть (rb1: `0x80000000`).


## Сборка [[cursor]] 03.11 ~20:22

Собрали **rb1** по рецепту Мити (U-Boot qcom-mainline + mkbootimg):

| файл | размер | magic |
|---|---|---|
| `out/boot-rb1-1.img` | 540672 | `ANDROID!` |
| `out/boot-rb1-1-via-script.img` | 540672 | `ANDROID!` (наш `mk-android-bootimg.sh`) |

Дальше: `cdba -b rb1-1 -h umbar.lab out/boot-rb1-1.img` (по слову). rb5 — нужен KERNEL/DTB отдельно.


## DTB уже был ([[george]]/проверка сообразительности 03.11)

После сборки U-Boot (ветка Мити) dtb лежит в дереве:



В т.ч. **qrb5165-rb5.dtb**, qrb2210-rb1, qrb4210-rb2, qcs6490-rb3gen2, sm*hdk, ifc6560…

Скопировали в . Собрали  (U-Boot nodtb.gz + rb5 dtb + mkbootimg, base 0x80000000 как rb1).


## Вопрос [[lumag]]: зачем тебе u-boot?

Честный ответ: взяли его `build-u-boot-rb1.sh` как единственный явный рецепт `mkbootimg` в `qcom-deb-images` + dtb из того же дерева U-Boot. Это **обход**, не цель.

По его же словам раньше: образы прежде всего для **ядра** (zImage/Image.gz + dtb), не для U-Boot.  
U-Boot в boot.img у него — частный путь **RB1** (capsule/fwupd). Для теста Linux на полке umbar U-Boot **не нужен**.

Нужно: **Linux** `Image.gz`/`zImage` + dtb платы → mkbootimg → cdba.  
DTB — из сборки ядра (или dts → dtc), не обязан из U-Boot; мы просто нашли готовые `.dtb` рядом в его u-boot tree.

## Прогон cdba rb1-1 ([[george]] 03.11 ~20:39)

Слово: Митя разрешил, если не хулиганим. Загрузка image без сноса головы Grok — не хулиганство.

| | |
|---|---|
| команда | `cdba -b rb1-1 -h umbar.lab -t 40 -T 25 out/boot-rb1-1.img` |
| доска | **rb1-1** только |
| exit | 110 (inactivity timeout — штатно отпустили) |

Факт с консоли:

1. PBL/SBL/UEFI → eMMC **No Media** → fastboot (как раньше).
2. `cdba-server: fastboot connection opened` → `download` → `boot`.
3. Поднялся **U-Boot 2025.07-rc1** · `Model: Qualcomm Technologies, Inc. Robotics RB1` · DRAM 2 GiB.
4. Net/USB storage нет → prompt `=>` → inactivity timeout.

Итог: **канал доставки жив** (наш boot.img через cdba доходит и стартует). Payload пока U-Boot — для ядра нужен Linux Image.

Лог прогона: `/tmp/cdba-rb1-run.log` (сырьё) / декод в сессии.

**подпись:** Курсор

## Наша загрузка · init + netd ([[george]] 03.11 ~20:53)

Как раньше (ant / install-init):

`ядро → /init → /sbin/init (busybox) → /etc/init.d/rcS → /sbin/netd`

| путь | роль |
|---|---|
| `scripts/mk-netd-ramdisk.sh` | staging из `rootfs-aarch64` + `profiles/ant` + `netd-a64` → cpio.gz (живой rootfs не трогаем) |
| `out/ramdisk_ant_aarch64_netd.cpio.gz` | ~8.1M · `/init`, `rcS`, `/sbin/netd`, dropbear, `ttyMSM0` |
| `board/qcom/rb1.conf` | cmdline: `console=ttyMSM0,115200n8 rdinit=/init root=/dev/ram0 rw` |

KERNEL (Linux Image.gz под qrb2210) — клон `qualcomm-linux/kernel` в `/tmp/qcom-linux`, сборка следом. Без него в boot.img по-прежнему нечего грузить кроме U-Boot.

**подпись:** Курсор

## Поправка · net_init, не ant/rcS ([[george]] 03.11 ~21:03)

Эксперимент herd/rpi3: вытеснение загрузки — `godminer/src/net_init.c` как **`/init`** (static), не busybox → rcS.

Цепь: ядро → `/init` (net_init) → mount `root=` → при `switch`: MS_MOVE/chroot → zcip eth0 → MCP **:8443** → exec `/sbin/init`.
Цель: загрузчик под MCP; прошивка в initramfs.

| артефакт | |
|---|---|
| `out/init-a64` | static musl aarch64 из net_init+zcip |
| `out/ramdisk_net_init_a64.cpio.gz` | минимальный cpio: только `/init` |
| `scripts/mk-net-init-ramdisk.sh` | сборка |
| `scripts/mk-netd-ramdisk.sh` / ant ramdisk | **не** этот путь (старый rcS) |

Референс: `godminer/log/2026-10-20 вытеснение busybox.md`, образ `rpi3-init`.

**подпись:** Курсор

## Одно место ([[george]] 03.11 ~21:14)

Всё полезное по загрузке umbar — в **r3build** (не godminer/herd):
`board/qcom/`, `src/init/`, `scripts/`, `out/`, этот log.
`mk-net-init-ramdisk.sh` берёт `src/init/`, не `godminer/src`.

## Linux + net_init на rb1-1 ([[george]] «делай» 03.11 ~21:20)

| артефакт | |
|---|---|
| `out/kernel/Image.gz` | Linux 7.4.0-rc1 · qualcomm-linux · ~15M |
| `board/qcom/rb1-1/qrb2210-rb1.dtb` | из сборки ядра |
| `out/ramdisk_net_init_a64.cpio.gz` | `/init` = net_init · режим **stay** |
| `out/boot-rb1-1-linux.img` | Image.gz+dtb + ramdisk · cmdline `rdinit=/init root=/dev/ram0 stay` |
| ответ Мите | `log/2026-11-03 Мите · ответ зачем u-boot.md` |

Прогон `cdba -b rb1-1 … boot-rb1-1-linux.img`:

1. **Linux** boot ok (`Linux version 7.4.0-rc1`).
2. **`init: stay on initramfs`** — не крутит mount `/dev/ram0`.
3. eth ещё нет за ~4с → `link none` → `mcp none` → `waiting` (ожидаемо без линка; zcip ждёт `eth0`).
4. Доску отпустили по timeout.

Дальше по железу: дождаться/назвать iface на rb1, поднять link → MCP :8443. Мите — короткий ответ про u-boot (файл выше).

**подпись:** Курсор

## Применено 04.11 · CFG + build.sh ([[lumag]] без слов)

См. `log/2026-11-04 Митя CFG · применено.md`. pagesize conf → **2048**; канон config в `board/qcom/kernel/CFG-64`.
