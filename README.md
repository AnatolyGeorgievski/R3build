# R3build

Дом сборки образов Ring-3: rootfs, `/init`, платы, профили. Коллаборанты собирают здесь; write ≠ доля во всех контурах.

## Норматив (актуально 2026-11-04)

| документ | о чём |
|---|---|
| [docs/R3_DECLARATION.md](docs/R3_DECLARATION.md) | совместная работа · этика netd · лицензия · контракты |
| [docs/R3_ECOSYSTEM.md](docs/R3_ECOSYSTEM.md) | экосистема Ring-3 (открыто / private / закрыто) |
| [docs/R3_STAND.md](docs/R3_STAND.md) | **концепция стенда** · полка отладки · netboot как fastboot · электрощит |
| [docs/R3_UMBAR_STATUS.md](docs/R3_UMBAR_STATUS.md) | **статус полки umbar** · stay / Alpine login / eth · диск Мити `PARTLABEL=rootfs` |
| [src/init/README.md](src/init/README.md) | роль `net_init` · SBC ramdisk / сервер без скриптов загрузки |

Связка: **R3-lib** (канон ABI) · **R3-net** (netd, **private**) · **r3build** (этот репозиторий).  
Стратегия: SBC → ramdisk → вытеснение busybox; сервер → сеть/диски/модули в netd → без скриптов загрузки (initramfs как носитель может остаться).

---

# R3build 🛠️

Скрипт-конструктор для сборки сверх-компактных и независимых образов **minimal rootfs** на базе дистрибутива [Alpine Linux](#). Ориентирован на встраиваемые системы (Embedded Linux).

## ✨ Особенности архитектуры

*   **Минимальный размер:** Вес неупакованной системы составляет от **3 МБ** (содержит только `musl` и `busybox`).
*   **Полноценный APK:** менеджер пакетов `apk` для динамической установки утилит в процессе работы.
*   **Автономная сборка:** процесс сборки выполняется на любой GNU/Linux хост-машине с использованием утилиты `apk.static`.

*Как запускается apk.static*. В пакет встроена библиотека musl со своим загрузчиком, что позволяет запускать утилиту на не-родной системе. Загрузчик `ld-musl` не конфликтует с загрузчиком хост-системы GNU. Сборка не требует настройки `chroot`, Docker или QEMU на этапе разворачивания базовых пакетов.

## 🗂️ Структура репозитория

*   Предназначен для одноплатных компьютеров с архитектурой `armhf` / `armv7` / `aarh64` / `riscv64` / `x86_64`.
*   `alpine-rootfs.sh` — Основной сценарий сборки базы rootfs.
*   `install-init.sh` — Скрипт установки кастомной инициализации.
*   `profiles/` — дерево rootfs по типу прошивки (сейчас: `ant`; `herd`/`swarm` — задуманы в `install-init.sh`)
*   `board/` — железо и референс стендов (`bb`, `zynq7000`, `aml`, `qcom`, `cv18x`, `radxa`, `orange`, `rpi`)
*   `src/init/` — `net_init` / zcip для ramdisk (не legacy BusyBox init)
*   `scripts/` — ramdisk, Android `boot.img`, сохранение dtb слотов umbar
*   `docs/` — норматив: `R3_DECLARATION.md`, `R3_ECOSYSTEM.md`, **`R3_STAND.md`** (стенд); BCP и прочее.

## 🚀 Быстрый старт (Пример сборки)

Для сборки образа под архитектуру ARMv7 (Antminer) с необходимым набором утилит, выполните:

```bash
# Сборка базовой файловой системы с утилитами MTD
ARCH=armv7 ./alpine-rootfs.sh busybox dropbear dropbear-scp ca-certificates mtd-utils-flash mtd-utils-ubi

# Установка скриптов инициализации и базовой конфигурации
ARCH=armv7 ./install-init.sh
```

## 📐 Модель загрузки (3 уровня)

```text
BOOT.bin (загрузка с SD/QSPI/NAND/NOR):
 ├── FSBL — первичный пред-загрузчик, настраивает память
 ├── FPGA bitstream.bin — загрузка конфигурации PL логики на Xilinx Zynq-7000
 └── u-Boot — загрузчик второй стадии (инициализация сети, консоли, памяти)

Образ загрузки на носителе:
 ├── 1. zImage (ядро Linux)
 ├── 2. devicetree.dtb (описывает периферию CPU)
 └── 3. ramdisk.cpio.gz (образ файловой системы в RAM)

Последовательность инициализации ОС:
 Ядро Linux
   └── /init (ранний init: свой net_init/netd-контур или legacy-скрипт)
       └── дальше — корень / службы; цель — MCP+POSIX без обязательного SHELL
           (legacy: BusyBox init + rcS — вытесняется)
           └── /etc/inittab (конфигурация действий на загрузку/завершение)
               ├── sysinit:  /etc/init.d/rcS (Старт служб: syslog → mount → net → ntp → ssh)
               └── shutdown: /etc/init.d/rcK (Корректная остановка демонов перед reboot)
```

## ⚙️ Слияние профилей и аппаратура плат

Профиль ≠ плата: `profiles/` даёт дерево в rootfs; `board/` — dts/dtb, conf загрузчика, патчи ядра.
Пакеты apk задаются аргументами `alpine-rootfs.sh`, не файлом `packages` в профиле.

```
├── profiles/                 # тип прошивки → файлы в rootfs
│   └── ant/                  # единственный живой профиль
│       ├── init              # точка входа (legacy BusyBox-контур)
│       ├── etc/              # inittab, fstab, passwd, init.d/rcS|rcK, …
│       ├── config/           # config.json, mcp.json
│       └── schema/           # schema/config.json
├── src/init/                 # net_init + zcip → /init в ramdisk (цель)
├── scripts/                  # mk-*-ramdisk, mk-android-bootimg, save-board-dtb
└── board/                    # железо + референс стендов (см. board/README.md)
    ├── bb/                   # TI Sitara / BeagleBone (armhf) console=ttyO0
    ├── zynq7000/             # Xilinx Zynq (armv7)  console=ttyPS0
    ├── aml/                  # Amlogic A113D (a64)  console=ttyAML0
    ├── qcom/                 # слоты umbar: dts/dtb + *.conf → boot.img
    │   ├── DTB-INDEX.md
    │   ├── rb1.conf …
    │   └── <BOARD_ID>/       # *.dts *.dtb SOURCE.txt
    ├── cv18x/                # CVtech (aarch64) console=ttyS0
    ├── radxa/                # стенд: ROCK 5C | 2F | 3B
    ├── orange/               # стенд: Orange Pi
    └── rpi/                  # стенд: Raspberry Pi (gpio/MCP)
```

**Стенд:** канон — [`docs/R3_STAND.md`](docs/R3_STAND.md). Старт с готового SBC (radxa / orange / rpi) → payload с host (TFTP/fastboot-подобно) → `/init`, netd, mcp, gpio. Своя плата — не обязательный вход.

| сейчас | позже |
|---|---|
| карта слотов + `ant` + qcom/aml/zynq; Pi TFTP **netd-rpi**; канон стенда в `docs/R3_STAND.md` | smoke gpio на живой Pi; dts radxa·orange; bb/cv18x по железу |
| umbar: Linux+`net_init` stay (навык) | eth→MCP на rb1 — по слову; NPU-артефакт с Максом |

Подробнее: [`board/README.md`](board/README.md) § «Сейчас / позже».

`bb/` и `cv18x/` могут быть пусты в git — слоты не списывать; dts/скрипты не выдумывать.
`herd` / `swarm` — имена в usage `install-init.sh`; каталогов пока нет.
`repo/${ARCH}/` — прикладные пакеты вне apk; каталога в репо сейчас нет.

## 🛠️ Профиль `ant` и пакеты apk

Профиль копирует файлы (`install-init.sh` / `mk-netd-ramdisk.sh`). Набор apk — отдельно, при сборке rootfs, например:

* `dropbear`, `dropbear-scp` — SSH
* `ca-certificates` — доверие к репозиториям
* `mtd-utils-flash`, `mtd-utils-ubi` — NAND/NOR (ant / Zynq / AML)

