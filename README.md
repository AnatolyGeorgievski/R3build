# R3build

Дом сборки образов Ring-3: rootfs, `/init`, платы, профили. Коллаборанты собирают здесь; write ≠ доля во всех контурах.

## Норматив (актуально 2026-11-04)

| документ | о чём |
|---|---|
| [docs/R3_DECLARATION.md](docs/R3_DECLARATION.md) | совместная работа · этика netd · лицензия · контракты |
| [docs/R3_ECOSYSTEM.md](docs/R3_ECOSYSTEM.md) | экосистема Ring-3 (открыто / private / закрыто) |
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
*   `profiles/` — Профили под целевые устройства (например, `ant`, `hurd` , `swarn`)
*   `docs/` — норматив: `R3_DECLARATION.md`, `R3_ECOSYSTEM.md`; BCP и прочее.

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
```
├── repo/${ARCH}/             # Репозиторий прикладных программ: bmu, gminer, godminer
├── profiles/                 # К какому "типу" относится прошивка
│   ├── ant/
│   │   └── packages          # dropbear, mtd-utils, ca-certificates, bmu-parser
│   └── herd/
│       └── packages          # dropbear, mtd-utils, ca-certificates, R3-mcp
└── board/                    # Железо (файлы fstab, dts, kernel)
    ├── bb/                   # TI Sitara   (armhf) -> console=ttyO0
    ├── zynq7000/             # Xilinx Zynq (armv7) -> console=ttyPS0
    ├── aml/                  # Amlogic   (aarch64) -> console=ttyAML0
    └── cv18x/                # CVtech    (aarch64) -> console=ttyS0
```

## 🛠️ Доступные профили пакетов

Система сборки поддерживает модульные профили для разных задач. 
Например, профиль `ant` автоматически включает в себя:
* `dropbear` и `dropbear-scp` — легковесный SSH-доступ.
* `ca-certificates` — корневые сертификаты для доверенного подключения к репозиториям.
* `mtd-utils-flash` и `mtd-utils-ubi` — набор утилит для обновления прошивки (NAND/NOR).

