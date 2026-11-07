# R3 AML · A113D — образ и перенос на платы

<tags>aml, a113d, alpine, fit, netd, стенд, макс, series</tags>

**Updated:** 2026-11-07.  
**Цель r3build:** собрать **один образ** (ядро + Alpine miniroot + dropbear + netd) для майнеров и научиться **переносить его на все платы AML** того же класса — как на дешёвые одноплатники под свои проекты, без привязки к одной задаче.

Донор фактов: документ Макса Фурмана (coord `2026-11-07-Макс-курсор-a113d-build-boot-install-ru.md`).  
Проработан глубоко; **не** копировать как руководство к действию без адаптации. Ниже — его техническое тело с рамкой R3.

## Рамка R3 (читать до разделов 1–16)

| берём | не тащим слепо |
|---|---|
| полный каталог `series/` (патчи · DTS · config · `source.lock`) | `devboard` / pinout / `nenv` как обязательный userspace |
| штатный U-Boot → FIT (Image.gz + DTB + rootfs.cpio) → `/init` | USB FAT `fatload` как **единственный** путь переноса |
| адреса FIT / пересечение с Image · осторожность `saveenv` | `CONFIG_CMDLINE_FORCE` miner-cmdline как навсегда |
| Alpine в RAM; NAND `nvdata`/Overlay — **опция** платы | побайтовый прежний дистрибутив · assisted burn без нужды |
| AO UART 115200 | установочный `aml_autoscript` на постоянно воткнутом носителе |

### Наш целевой контур

```text
series + Image.gz + dtb     (ядро уже: a113d-build/ · board/aml/)
        ↓
Alpine miniroot + dropbear + /sbin/netd     (как e9pro · R3_BCP · цель Pi)
        ↓
один артефакт (FIT / nand-файл)
        ↓
перенос на плату: пробная загрузка → запись → MCP :8443
        ↓
тот же рецепт на следующую AML того же класса (SBC, не «религия майнера»)
```

Стенд — та же идея, что Pi / umbar: **хост толкает образ**, плата не обязана жить с картой.  
Путь Макса «FAT на USB → U-Boot» остаётся **одним** способом переноса (разделы 7–12); сеть / Linux-writer (раздел 13) — предпочтительнее для стенда, когда eth жив.

Rootfs: канон пакетов — [R3_BCP.md](R3_BCP.md) / `alpine-rootfs.sh` (miniroot + dropbear). Раздел 5 ниже — донор схемы `/init` и Overlay; наш payload подменяем на netd, не на майнерский pinout.

Локальные якоря:

| | |
|---|---|
| series / инструкция рядом | `board/aml/` · `/home/george/aml_kernel/` |
| собранное ядро 6.18.42 | `/home/george/a113d-build/build/` |
| концепция стенда | [R3_STAND.md](R3_STAND.md) |
| выжимка адаптации | `/coord/notes/2026-11-07-курсор-нил-AML-Макс-адаптация.md` |

---

## Тело донора (Макс) · с пометками R3

> Ниже сохранены разделы 1–16 исходного руководства. Перед каждым крупным блоком — короткая пометка, зачем он нам и где свернуть.

Руководство для платы **MINER_CTRL_BOARD_A113D_V1.1** с 256 МиБ RAM, 256 МиБ NAND и сохранённым штатным Amlogic U-Boot. Оно описывает сборку ядра, Alpine initramfs, FIT и носителя, пробную загрузку, запись системы в NAND и настройку последующей загрузки без карты.

Из проекта нужен только **полный каталог `series/`**: патчи, `source.lock`, DTS платы и фрагмент конфигурации. Одних трёх файлов `.patch` недостаточно: они не содержат описание платы и полный выбор параметров сборки. Остальные файлы создаются командами этого руководства. `devboard`, проектные Python-модули, готовый `base.itb`, `nand/build.sh` и экспортированный rootfs не требуются.

Самостоятельный пример воспроизводит архитектуру загрузки и установки, но не побайтово прежний дистрибутив: служебные программы проекта для pinout и чтения `nenv` в него не включены. Для работы initramfs, Ethernet, SSH, UBI/UBIFS и OverlayFS они не нужны.

Основной маршрут: разделы 3–8 — сборка на компьютере; 9–10 — пробная загрузка и backup; 11 — подготовка `nvdata`, только если она нужна; 12 — установка из карты и сохранение проверенного окружения; 14 — проверка результата. Раздел 13 заменяет запись через U-Boot записью из Linux, а раздел 15 объясняет отдельный внешний bootstrap.

### R3 · §1
Носитель у Макса = каталог установщика (часто USB FAT), не обязательно слот SD. Для стенда тот же смысл: **доставить FIT**, физика носителя вторична.

## 1 Что именно находится на карте

В проекте слово SD обозначает носитель с файлами установщика. Генератор выдаёт **каталог файлов, а не образ диска с загрузчиком**. Команды в шаблонах используют `fatload usb 0`, поэтому описанный здесь путь рассчитан на USB-накопитель или SD-карту в USB-картридере. Наличие слота SD само по себе не доказывает поддержку `mmc` этим загрузчиком. Для прямого SD/MMC нужны отдельно проверенные номер устройства, раздел и команды U-Boot.

Загрузочная цепочка сохраняет имеющиеся первые стадии загрузки:

```text
ROM и штатные ранние стадии → штатный U-Boot
                                  │
             FAT на SD через USB ──┤ пробная загрузка
                      boot в NAND ─┤ установленная система
                                  ↓
                         FIT загружается в RAM
                                  ↓
                    Image.gz + DTB + rootfs.cpio.gz
                                  ↓
                      Linux запускает /init
                                  ↓
             nvdata → UBI → UBIFS → OverlayFS → Alpine
```

При нормальной работе корень системы не находится на SD. Его нижний слой распакован из initramfs в RAM; изменения сохраняются в `nvdata` на NAND. Сам FIT лежит в разделе NAND `boot`. После установки носитель можно убрать.

Если `nvdata` не подключается, `/init` запускает систему из RAM без автоматического форматирования. Загрузка production FIT из RAM **не означает отсутствие записей в NAND**: успешное подключение `nvdata` создаёт каталоги, ключ SSH и впоследствии сохраняет изменения.

### R3 · §2
Критично: адреса FIT vs Image, пять переменных, `bootm` по шагам, `CMDLINE_FORCE`. Перед `saveenv` — прогон с новым `S` и адресом `0x08000000`. Не оставлять старый `fit_start` при новом `fit_load`.

## 2 Переменные U-Boot и передача управления ядру

### Значения которые формирует исходный код

Функция `boot_environment()` формирует следующие пять значений. `S` в таблице означает размер **выровненного NAND-файла**, рассчитанный при сборке, а не размер всего раздела.

| Переменная | Значение в генераторе проекта | Назначение |
|---|---|---|
| `fit_load` | `nand read 0x2200000 0x2200000 S` | Прочитать `S` байт из NAND со смещения `0x02200000` в RAM по адресу `0x02200000` |
| `fit_start` | `bootm start 0x2200000#normal; bootm loados` | Выбрать конфигурацию FIT `normal`, проверить компоненты, распаковать ядро |
| `fit_finish` | `bootm ramdisk; bootm fdt; bootm prep; bootm go` | Подготовить initramfs и DTB, затем передать управление ядру |
| `nandboot` | `run fit_load; run fit_start; run fit_finish` | Выполнить последовательность загрузки |
| `bootcmd` | `run nandboot` | Запустить её после таймера автозагрузки |

Первый и второй аргументы `nand read` — разные сущности: адрес RAM и физическое смещение NAND. Их совпадение в исходном профиле случайно; при переносе FIT выше в RAM смещение NAND остаётся прежним.

`setenv` меняет рабочую копию окружения в RAM. `saveenv` сохраняет её для следующих включений. Установщики SD/assisted задают все пять переменных после проверки записи. Linux-установщик production окружение вообще не меняет; отдельный U-Boot finalizer меняет **только `fit_load`**, предполагая, что остальные четыре значения уже подходят.

Не следует считать старые размеры константой платы. Например, прежний `0xf80000` обрезал новый FIT; в истории встречаются `0x1500000` и `0x1540000`. Сохранённый пакет production с GPIO IRQ и `nenv` имеет размер `0x1560000`. После другой сборки значение может измениться.

### Адреса и пересечение распакованного ядра с FIT

| Объект | Адрес или диапазон |
|---|---|
| Начало и точка входа распакованного Linux Image | `0x01080000` |
| Исторический адрес FIT при загрузке из NAND | `0x02200000` |
| Зарезервированная область secure monitor | `0x05000000`–`0x05300000` |
| U-Boot script `aml_autoscript` | `0x06000000` |
| FIT при RAM-загрузке и проверке finalizer | `0x08000000` |
| Верхняя граница RAM | `0x10000000` |
| Верхняя граница загрузок с запасом 16 МиБ | `0x0f000000` |

У сохранённого `kernel/out/6.18.42-production/Image.gz` размер распакованного файла равен `0x11d5008`, поэтому конец файла при загрузке — `0x02255008`. Поле `image_size` заголовка ARM64 равно `0x1240000`, то есть заявленный диапазон памяти заканчивается на `0x022c0000`. Оба конца выше исторического адреса FIT `0x02200000`. Это статически установленное пересечение диапазонов; его конкретное проявление зависит от работы vendor U-Boot.

Поэтому самостоятельный пример ниже использует **`0x08000000` и для чтения NAND FIT, и для `bootm start`**. Этот адрес уже используется проектом для RAM-загрузки и проверки NAND. Это осознанное отличие от старой функции генератора; загрузку конкретного нового образа с NAND необходимо проверить на плате до сохранения окружения. Переставить только `fit_load`, оставив прежний `fit_start`, нельзя.

Для нового образа требуются следующие значения, где вместо `S` подставляется рассчитанное шестнадцатеричное число:

```text
fit_load=nand read 0x8000000 0x2200000 S
fit_start=bootm start 0x8000000#normal; bootm loados
fit_finish=bootm ramdisk; bootm fdt; bootm prep; bootm go
nandboot=run fit_load; run fit_start; run fit_finish
bootcmd=run nandboot
```

Длинные значения при `setenv` заключают в одинарные кавычки, чтобы `;` стал частью значения. Команда `bootm go` выполняет окончательную передачу управления. Разбиение на `start`, `loados`, `ramdisk`, `fdt`, `prep`, `go` намеренное: такой путь используется для этого vendor U-Boot. Не заменяйте его непроверенным единым `bootm`.

### Почему не нужно задавать bootargs

В production-фрагменте ядра закреплено:

```text
CONFIG_CMDLINE="init=/init console=ttyAML0,115200n8 earlycon loglevel=4"
CONFIG_CMDLINE_FORCE=y
```

Поэтому `setenv bootargs ...`, включая `root=...`, не заменит эту строку. Linux использует initramfs и запускает `/init`; подключение `nvdata` выполняет userspace. Не нужны `root=/dev/mmcblk...`, отдельный раздел rootfs на SD или `extlinux.conf`. `devboard.rescue=1` также нельзя добавить только через окружение при `CONFIG_CMDLINE_FORCE=y`: для такого режима нужно пересобрать конфигурацию либо использовать предусмотренные `/init` файлы-маркеры на `nvdata`.

### Когда исполняется aml_autoscript

Само имя файла не гарантирует автоматического запуска. Штатная конфигурация платы может сканировать USB, но не исполнять скрипт. Проверенный ручной способ:

```text
usb start
fatload usb 0 0x6000000 aml_autoscript
autoscr 0x6000000
```

На этом vendor U-Boot используется `autoscr`; команда `source` отсутствует. Для GPT-носителя FAT иногда оказывается разделом 2, тогда используют `usb 0:2`. В этом руководстве создаётся MBR с одним FAT32-разделом.

Необязательный USB hook в проекте имеет такую логику:

```text
devboard_usbboot=usb start; if fatload usb 0 0x6000000 aml_autoscript; then autoscr 0x6000000; fi
bootcmd=run devboard_usbboot; run devboard_stock_bootcmd
```

В `devboard_stock_bootcmd` сохраняется точное прежнее значение `bootcmd`. Hook не нужен для ручной установки. Особенно не оставляйте автоматически исполняемый **установочный** `aml_autoscript` на постоянно подключённом носителе: его повторный запуск может повторить стирание `boot`.

### R3 · §3
Сборка на arc2/Linux; UART AO; eth. Docker ARM64 для rootfs — если идём его Dockerfile; наш минимум — `apk.static` без Docker ([R3_BCP.md](R3_BCP.md)).

## 3 Что подготовить

Нужны Linux-компьютер для сборки, полный каталог `series/`, доступ в интернет для исходников и Alpine, Docker с BuildKit для rootfs, USB-носитель или SD с USB-картридером, AO UART 115200 8N1, Ethernet с DHCP и устойчивое питание платы. Уровень сигнала UART и распиновку проверяют для своей ревизии; к TTL UART не подключают интерфейс RS-232.

Для rootfs проще использовать Linux ARM64 с нативным Docker. На x86_64 нужен Docker builder, способный **выполнять ARM64 RUN-команды** через настроенную эмуляцию или удалённый ARM64 builder. Одного параметра `--platform linux/arm64` для этого недостаточно.

На Debian/Ubuntu установите стандартные инструменты:

```sh
sudo apt-get update
sudo apt-get install build-essential gcc-aarch64-linux-gnu bc bison flex \
  libssl-dev libelf-dev dwarves cpio rsync device-tree-compiler \
  u-boot-tools patch curl xz-utils gzip python3 openssh-client \
  dosfstools mtools fdisk
```

Docker Engine и плагин Buildx должны быть уже установлены. Проверьте `docker version` и `docker buildx version`. Для самостоятельной сборки kernel не нужен Docker; он используется здесь только как стандартная среда сборки Alpine rootfs.

Создайте отдельный рабочий каталог. Дальше команды хоста выполняются в Bash с переменными из этого блока; команды платы и U-Boot обозначены отдельно.

```sh
bash
set -euo pipefail
mkdir -p "$PWD/a113d-manual"
cd "$PWD/a113d-manual"
export WORKDIR="$PWD"
# Скопируйте сюда переданный каталог series, чтобы существовал series/source.lock.
export SERIES_DIR="$WORKDIR/series"
test -f "$SERIES_DIR/source.lock"
mkdir -p out rootfs-context
```

Нужен только публичный ED25519-ключ. Укажите существующий `.pub` или создайте отдельную пару. Приватный ключ остаётся на компьютере оператора:

```sh
# Выполняйте ssh-keygen только если новой пары ещё нет.
ssh-keygen -t ed25519 -f "$WORKDIR/operator_ed25519"
export PRIVATE_KEY="$WORKDIR/operator_ed25519"
export PUBLIC_KEY="$PRIVATE_KEY.pub"
ssh-keygen -lf "$PUBLIC_KEY"
```

До установки остановите автозагрузку U-Boot по UART и сохраните полный вывод `version` и `printenv` в журнал терминала. Команды `help nand`, `help bootm`, `help fatload`, `help autoscr` позволяют проверить доступный интерфейс. Сверьте NAND и разметку с разделом 10. Нужен рабочий recovery-носитель именно этой платы; образ ниже не восстанавливает уничтоженные ранние стадии загрузки.

### R3 · §4
Уже сделано 2026-10-30 (`a113d-build`). Повторять только при смене `series/` / lock. Модули не ставятся — драйверы в ядре.

## 4 Сборка ядра и DTB

`source.lock` закрепляет Linux `6.18.42` и SHA-256 архива:

```text
https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.18.42.tar.xz
35a8ef5e8fb44467e56a42b40c23c4c10a61d1e95ff8d738f285230e8bed63e7
```

Используйте именно согласованный архив. Если он недоступен, нужен архив с тем же хешем из сохранённого набора; подмена версией «посвежее» требует переноса патчей и повторной квалификации.

```sh
cd "$WORKDIR"
. "$SERIES_DIR/source.lock"
test ! -e "linux-$KERNEL_VERSION"
test ! -e kernel-build
curl --fail --location --retry 3 -o "$KERNEL_ARCHIVE" "$KERNEL_URL"
printf '%s  %s\n' "$KERNEL_SHA256" "$KERNEL_ARCHIVE" | sha256sum -c -
tar -xJf "$KERNEL_ARCHIVE"
export SOURCE_DIR="$WORKDIR/linux-$KERNEL_VERSION"
export BUILD_DIR="$WORKDIR/kernel-build"

while IFS= read -r name || [ -n "$name" ]; do
  case "$name" in ''|\#*) continue ;; esac
  patch --batch --forward --fuzz=0 -d "$SOURCE_DIR" -p1 \
    -i "$SERIES_DIR/patches/$name"
done < "$SERIES_DIR/patches/series"

cp "$SERIES_DIR/dts/meson-axg-a113d-miner-production.dts" \
  "$SOURCE_DIR/arch/arm64/boot/dts/amlogic/"
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
export KBUILD_BUILD_USER=a113d KBUILD_BUILD_HOST=linux
export KBUILD_BUILD_TIMESTAMP='1970-01-01 00:00:00 UTC'
export SOURCE_DATE_EPOCH=0
make -C "$SOURCE_DIR" O="$BUILD_DIR" defconfig
"$SOURCE_DIR/scripts/kconfig/merge_config.sh" -m -O "$BUILD_DIR" \
  "$BUILD_DIR/.config" "$SERIES_DIR/configs/a113d-production.config"
make -C "$SOURCE_DIR" O="$BUILD_DIR" olddefconfig
make -C "$SOURCE_DIR" O="$BUILD_DIR" -j"$(nproc)" \
  Image.gz amlogic/meson-axg-a113d-miner-production.dtb

cp "$BUILD_DIR/arch/arm64/boot/Image.gz" out/
cp "$BUILD_DIR/arch/arm64/boot/dts/amlogic/meson-axg-a113d-miner-production.dtb" \
  out/board.dtb
cp "$BUILD_DIR/.config" out/kernel.config
make -s -C "$SOURCE_DIR" O="$BUILD_DIR" kernelrelease > out/kernel.release
```

На ARM64 с нативным GCC можно вместо cross compiler задать `CROSS_COMPILE=`. DTB собирается явной целью, поскольку плата не добавлена в общий список `make dtbs`. Модули этими командами не устанавливаются; необходимые для загрузки UART, Ethernet, NAND, UBI/UBIFS и OverlayFS включены в ядро.

Патчи по порядку добавляют импорт vendor NBBT в драйвер Meson NAND, GPIO IRQ bridge и его DT binding. Production DTS задаёт аппаратный ECC BCH8 на 512 байт и `amlogic,legacy-nbbt = <20 4>`. Без согласованного NBBT-пути generic bad-block scan неверно трактовал служебные данные этой платы. Не переносите NAND-процедуру на непатченное ядро.

Конфигурация содержит драйверы USB/MMC, но production DTS оставляет USB выключенным. Поэтому Linux не обязан видеть карту, из которой U-Boot загрузил FIT. Для Linux-установки файл передают по сети, а для установки непосредственно с карты запись выполняет U-Boot.

### R3 · §5
Донор `/init` + Overlay/`nvdata`. **Наш образ:** miniroot + dropbear + netd MCP; Overlay на NAND — позже и по слову, не блокер первого MCP. Не тащить pinout/nenv.

## 5 Самостоятельная сборка Alpine initramfs

Initramfs должен содержать исполняемый `/init`, `/bin/sh` вместе с его библиотеками, init-систему, сетевые утилиты, SSH и MTD/UBI-инструменты. Формат — gzip от CPIO `newc`. Общая механика описана в [документации Linux об initramfs](https://docs.kernel.org/filesystems/ramfs-rootfs-initramfs.html).

Ниже полностью приведены входные файлы. Скрипт `/init` сохраняет схему проекта: `ubi0:nvdata`, версионированный OverlayFS, отдельные `/home`, `/opt` и SSH host keys, а при ошибке — запуск из RAM. Автоматического форматирования нет.

### Начальный процесс

```sh
cd "$WORKDIR"
cat > rootfs-context/init <<'INIT'
#!/bin/sh

PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PATH

DEVBOARD_ARCH=armv7
DEVBOARD_IMAGE_ID=alpine-3.24.1-armv7-v1
if [ -r /etc/devboard-release ]; then
    . /etc/devboard-release
fi
IMAGE_ID=$DEVBOARD_IMAGE_ID
NVDATA=/mnt/nvdata
STATE_DIR=$NVDATA/devboard
PERSIST_DIR=$STATE_DIR/persist
OVERLAY_DIR=$STATE_DIR/overlays/$IMAGE_ID
LOWER_DIR=/run/devboard-lower
NEWROOT=/newroot

log()
{
    echo "devboard-init: $*"
}

is_mounted()
{
    grep -qs " $1 " /proc/mounts
}

mount_early_filesystems()
{
    mkdir -p /dev /proc /sys /run "$NVDATA" "$NEWROOT"
    mount -t devtmpfs devtmpfs /dev 2>/dev/null || true
    mount -t proc proc /proc 2>/dev/null || true
    mount -t sysfs sysfs /sys 2>/dev/null || true
    mount -t tmpfs -o mode=0755,nosuid,nodev tmpfs /run 2>/dev/null || true
}

mount_nvdata()
{
    if [ ! -e /dev/ubi0 ]; then
        log "attaching MTD6 as UBI0"
        ubiattach -d 0 -m 6 || return 1
    fi

    [ -e /dev/ubi0_0 ] || return 1
    mount -t ubifs ubi0:nvdata "$NVDATA" \
        || mount -t ubifs /dev/ubi0_0 "$NVDATA"
}

ensure_host_key()
{
    key_dir=$1
    mkdir -p "$key_dir"
    if [ ! -s "$key_dir/ssh_host_ed25519_key" ]; then
        log "generating a persistent ED25519 host key"
        ssh-keygen -q -t ed25519 -N '' -f "$key_dir/ssh_host_ed25519_key" \
            || return 1
    fi
    chmod 0700 "$key_dir"
    chmod 0600 "$key_dir/ssh_host_ed25519_key"
}

prepare_persistent_layout()
{
    mkdir -p \
        "$STATE_DIR/boot" \
        "$PERSIST_DIR/home" \
        "$PERSIST_DIR/opt" \
        "$PERSIST_DIR/ssh" \
        "$OVERLAY_DIR/upper" \
        "$OVERLAY_DIR/work"
    ensure_host_key "$PERSIST_DIR/ssh"
}

bind_host_keys_for_rescue()
{
    mkdir -p /etc/ssh/host_keys
    if is_mounted "$NVDATA" && [ -d "$PERSIST_DIR/ssh" ]; then
        mount -o bind "$PERSIST_DIR/ssh" /etc/ssh/host_keys || return 1
    else
        ensure_host_key /etc/ssh/host_keys || return 1
    fi
}

start_rescue()
{
    log "starting immutable Alpine rescue system"
    bind_host_keys_for_rescue || log "host-key setup failed"
    exec /sbin/init
    log "failed to start rescue init"
    exec /bin/sh
}

wants_rescue()
{
    grep -qw 'devboard.rescue=1' /proc/cmdline && return 0
    [ -e "$STATE_DIR/boot/rescue" ] && return 0
    if [ -e "$STATE_DIR/boot/rescue-once" ]; then
        rm -f "$STATE_DIR/boot/rescue-once"
        sync
        return 0
    fi
    return 1
}

mount_overlay_root()
{
    mkdir -p "$LOWER_DIR" "$NEWROOT"
    mount -o bind / "$LOWER_DIR" || return 1
    mount -t overlay overlay \
        -o "lowerdir=$LOWER_DIR,upperdir=$OVERLAY_DIR/upper,workdir=$OVERLAY_DIR/work" \
        "$NEWROOT" || return 1

    [ -x "$NEWROOT/sbin/init" ] || return 1

    mkdir -p \
        "$NEWROOT/dev" \
        "$NEWROOT/proc" \
        "$NEWROOT/sys" \
        "$NEWROOT/run" \
        "$NEWROOT/mnt/nvdata" \
        "$NEWROOT/home" \
        "$NEWROOT/opt" \
        "$NEWROOT/etc/ssh/host_keys"

    mount -o bind "$NVDATA" "$NEWROOT/mnt/nvdata" || return 1
    mount -o bind "$PERSIST_DIR/home" "$NEWROOT/home" || return 1
    mount -o bind "$PERSIST_DIR/opt" "$NEWROOT/opt" || return 1
    mount -o bind "$PERSIST_DIR/ssh" "$NEWROOT/etc/ssh/host_keys" || return 1

    mount -t devtmpfs devtmpfs "$NEWROOT/dev" || return 1
    mount -t proc proc "$NEWROOT/proc" || return 1
    mount -t sysfs sysfs "$NEWROOT/sys" || return 1
    mount -t tmpfs -o mode=0755,nosuid,nodev tmpfs "$NEWROOT/run" || return 1
}

mount_early_filesystems

if grep -qw 'devboard.nand_probe=1' /proc/cmdline; then
    log "NAND probe mode; skipping UBI attach and persistent mounts"
    start_rescue
fi

if ! mount_nvdata; then
    log "nvdata is unavailable; preserving NAND and entering rescue"
    start_rescue
fi

if ! prepare_persistent_layout; then
    log "persistent layout setup failed"
    start_rescue
fi

if wants_rescue; then
    start_rescue
fi

if ! mount_overlay_root; then
    log "overlay setup failed; entering rescue"
    start_rescue
fi

log "starting $IMAGE_ID with persistent overlay"
exec chroot "$NEWROOT" /sbin/init

log "chroot failed; entering emergency shell"
exec /bin/sh
INIT
```

`devboard.nand_probe=1` проверяется до UBI attach. `devboard.rescue=1` и маркеры `devboard/boot/rescue` либо `rescue-once` выбирают rescue после подключения `nvdata`. При production `CONFIG_CMDLINE_FORCE=y` изменить эти аргументы одним `setenv` нельзя.

### Конфигурация служб

```sh
cd "$WORKDIR"
cat > rootfs-context/inittab <<'EOF'
::sysinit:/sbin/openrc sysinit
::sysinit:/sbin/openrc boot
::wait:/sbin/openrc default
console::respawn:/sbin/getty -L 115200 console vt100
::ctrlaltdel:/sbin/reboot
::shutdown:/sbin/openrc shutdown
EOF

cat > rootfs-context/interfaces <<'EOF'
auto lo
iface lo inet loopback
auto eth0
iface eth0 inet dhcp
EOF

cat > rootfs-context/fstab <<'EOF'
proc    /proc    proc    defaults 0 0
sysfs   /sys     sysfs   defaults 0 0
devpts  /dev/pts devpts  gid=5,mode=620,ptmxmode=666 0 0
tmpfs   /dev/shm tmpfs   mode=1777,nosuid,nodev 0 0
tmpfs   /tmp     tmpfs   mode=1777,nosuid,nodev 0 0
tmpfs   /run     tmpfs   mode=0755,nosuid,nodev 0 0
EOF

cat > rootfs-context/sshd_config <<'EOF'
Port 22
HostKey /etc/ssh/host_keys/ssh_host_ed25519_key
PermitRootLogin prohibit-password
PubkeyAuthentication yes
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitEmptyPasswords no
AuthorizedKeysFile .ssh/authorized_keys
Subsystem sftp internal-sftp
EOF
```

В этом самостоятельном варианте SSH принимает только ключ оператора. Для UART root-пароль первоначально не задан; при необходимости установите его после первого входа по SSH командой `passwd`. В исходном проекте также предусмотрен отдельный wrapper автологина, но production-командная строка не включает разрешающий его флаг.

### Dockerfile и упаковка

Основа Alpine и digest взяты из закреплённого Dockerfile проекта. Версии устанавливаемых APK-пакетов там отдельно не закреплены, поэтому обновления репозиториев могут изменить результат. Для побайтового повторения сохраняйте rootfs, список пакетов и его SHA-256.

```sh
cd "$WORKDIR"
cat > rootfs-context/Dockerfile <<'DOCKERFILE'
# syntax=docker/dockerfile:1.7
FROM alpine:3.24.1@sha256:28bd5fe8b56d1bd048e5babf5b10710ebe0bae67db86916198a6eec434943f8b AS runtime
RUN apk add --no-cache bash ca-certificates cpio iproute2 libgpiod \
    mtd-utils openrc openssh-client-default openssh-server \
    rsync util-linux
COPY init /init
COPY inittab /etc/inittab
COPY interfaces /etc/network/interfaces
COPY fstab /etc/fstab
COPY sshd_config /etc/ssh/sshd_config
RUN --mount=type=secret,id=authorized_key,required=true \
    mkdir -p /root/.ssh /etc/ssh/host_keys /home /opt /mnt/nvdata \
      /newroot /dev/pts /dev/shm \
    && cp /run/secrets/authorized_key /root/.ssh/authorized_keys \
    && chmod 0700 /root/.ssh \
    && chmod 0600 /root/.ssh/authorized_keys \
    && chmod 0755 /init \
    && rm -f /etc/ssh/ssh_host_* \
    && passwd -d root \
    && printf 'a113d\n' > /etc/hostname \
    && printf 'DEVBOARD_ARCH=arm64\nDEVBOARD_IMAGE_ID=manual-alpine-arm64-v1\n' \
      > /etc/devboard-release \
    && apk info -v > /etc/build-packages.txt \
    && rc-update add devfs sysinit \
    && rc-update add dmesg sysinit \
    && rc-update add networking default \
    && rc-update add sshd default
FROM scratch
COPY --from=runtime / /
DOCKERFILE

docker buildx build --platform linux/arm64 \
  --secret "id=authorized_key,src=$PUBLIC_KEY" \
  --output "type=tar,dest=$WORKDIR/out/rootfs.tar" \
  "$WORKDIR/rootfs-context"

mkdir "$WORKDIR/rootfs-tree"
sudo tar --numeric-owner -xpf out/rootfs.tar -C rootfs-tree
sudo chmod 0755 rootfs-tree
(
  cd rootfs-tree
  sudo find . -xdev -print0 | LC_ALL=C sort -z | \
    sudo cpio --null -o -H newc --reproducible --owner=0:0 | gzip -n -9
) > out/rootfs.cpio.gz
gzip -t out/rootfs.cpio.gz
```

`passwd -d root` позволяет локальный вход без пароля до настройки `passwd`; SSH при этом остаётся только по ключу благодаря конфигурации выше. Если это неприемлемо для места эксплуатации, задайте пароль до установки или отключите console getty. При нормальной загрузке последующий `passwd` сохраняется в OverlayFS, а в rescue без `nvdata` — только до перезагрузки.

Экспорт `type=tar` выдаёт файловую систему результата сборки, а не Docker image archive; см. [Docker local and tar exporters](https://docs.docker.com/build/exporters/local-tar/). В FAT-носитель кладут FIT, поэтому права Linux-файлов сохраняются внутри CPIO, а не зависят от FAT.

### R3 · §6
FIT = наш «один артефакт». Выравнивание и размер `S` для `nand read` обязательны.

## 6 FIT и файл для раздела NAND

FIT — контейнер ядра, DTB и initramfs. Его описание собирает `mkimage`, используя `dtc`; конфигурация `normal` связывает три компонента. См. [официальное описание сборки FIT](https://docs.u-boot.org/en/latest/usage/fit/howto.html).

```sh
cd "$WORKDIR/out"
cat > image.its <<'ITS'
/dts-v1/;

/ {
	description = "A113D Devboard Linux 6.18 production";
	#address-cells = <1>;

	images {
		kernel {
			description = "Linux 6.18 AArch64 Image.gz";
			data = /incbin/("Image.gz");
			type = "kernel";
			arch = "arm64";
			os = "linux";
			compression = "gzip";
			load = <0x01080000>;
			entry = <0x01080000>;
			hash-1 { algo = "sha1"; };
		};

		fdt {
			description = "A113D miner control board production DTB";
			data = /incbin/("board.dtb");
			type = "flat_dt";
			arch = "arm64";
			compression = "none";
			hash-1 { algo = "sha1"; };
		};

		ramdisk {
			description = "Alpine AArch64 initramfs";
			data = /incbin/("rootfs.cpio.gz");
			type = "ramdisk";
			arch = "arm64";
			os = "linux";
			compression = "gzip";
			hash-1 { algo = "sha1"; };
		};
	};

	configurations {
		default = "normal";
		normal {
			description = "A113D production boot with persistent nvdata and GPIO edge events";
			kernel = "kernel";
			fdt = "fdt";
			ramdisk = "ramdisk";
		};
	};
};
ITS

SOURCE_DATE_EPOCH=0 mkimage -f image.its a113d.itb
dumpimage -l a113d.itb
```

SHA-1 в каждом узле нужен для совместимости с данным U-Boot и проверки повреждения компонентов. Это **не цифровая подпись и не подтверждение происхождения**. Дополнительно сохраняются SHA-256 готовых файлов на компьютере.

Создайте NAND-файл из FIT, дополнив конец байтами `0xff` до границы 128 КиБ. Это data-only файл для `boot`, без OOB/ECC, без загрузчика и без остальных разделов NAND:

```sh
cd "$WORKDIR/out"
python3 - <<'PY'
from pathlib import Path
import gzip, struct

raw = gzip.decompress(Path('Image.gz').read_bytes())
assert raw[0x38:0x3c] == b'ARM\x64', 'not an ARM64 Image'
span = max(len(raw), struct.unpack_from('<Q', raw, 0x10)[0])
assert 0x01080000 + span <= 0x05000000, 'kernel overlaps secure monitor'

fit = Path('a113d.itb').read_bytes()
assert struct.unpack_from('>I', fit)[0] == 0xd00dfeed
assert struct.unpack_from('>I', fit, 4)[0] == len(fit)
size = (len(fit) + 0x1ffff) & ~0x1ffff
assert 0 < size <= 0x02000000, 'FIT exceeds boot partition'
assert 0x08000000 + size <= 0x0f000000, 'FIT exceeds RAM limit'
Path('a113d-nand.bin').write_bytes(fit + b'\xff' * (size - len(fit)))
Path('nand-size.hex').write_text(f'0x{size:x}\n')
print(f'FIT: {len(fit)}; NAND: {size} (0x{size:x})')
print(f'Kernel memory end: 0x{0x01080000 + span:x}')
PY

sha256sum Image.gz board.dtb rootfs.cpio.gz kernel.config kernel.release \
  image.its a113d.itb a113d-nand.bin nand-size.hex > SHA256SUMS
sha256sum -c SHA256SUMS
```

Формула размера: `S = ceil(FIT_size / 0x20000) × 0x20000`. Именно `S` попадёт в `nand read` и `nand write`. Размер раздела `boot` равен `0x02000000`, но заполнять весь раздел padding до 32 МиБ не требуется.

### R3 · §7–8
Путь USB/SD установщика — **вариант переноса**. Параллель стенда: отдать FIT по сети / с хоста, как Pi TFTP или umbar boot.img.

## 7 Подготовка файлов для SD карты

Сначала создаётся носитель для пробной загрузки, который сам не стирает `boot`. Установочные команды будут отдельным файлом и вызываются вручную после проверки и резервного копирования.

```sh
cd "$WORKDIR"
mkdir -p sd-card
cp out/a113d.itb out/a113d-nand.bin out/nand-size.hex sd-card/
cat > sd-card/aml_autoscript.txt <<'UBOOT'
echo A113D RAM boot
if fatload usb 0 0x8000000 a113d.itb; then
    if bootm start 0x8000000#normal; then
        bootm loados
        bootm ramdisk
        bootm fdt
        bootm prep
        bootm go
    else
        echo ERROR FIT validation failed
    fi
else
    echo ERROR cannot load a113d.itb
fi
UBOOT
SOURCE_DATE_EPOCH=0 mkimage -A arm64 -O linux -T script -C none \
  -n 'A113D RAM boot' -d sd-card/aml_autoscript.txt sd-card/aml_autoscript
```

`aml_autoscript` — бинарный legacy script image с заголовком U-Boot. Обычного переименования текстового файла недостаточно. `a113d.itb` остаётся plain FIT.

Подготовьте также скрипт записи из карты, но **без `saveenv`**. По сравнению с assisted-шаблоном проекта здесь сохранение окружения вынесено в отдельный шаг, чтобы сначала проверить фактическую загрузку нового NAND-образа. Источник расположен в `0x02200000`, копия для сравнения — в `0x08000000`. До завершения записи выполняется только `bootm start`, без распаковки ядра в пересекающуюся область.

```sh
cd "$WORKDIR"
python3 - <<'PY'
from pathlib import Path
size = int(Path('out/nand-size.hex').read_text(), 16)
assert size == Path('sd-card/a113d-nand.bin').stat().st_size
script = '''echo A113D write boot partition after backup
if fatload usb 0 0x2200000 a113d-nand.bin; then
  if test "${filesize}" = "@SIZE_BARE@"; then
    if bootm start 0x2200000#normal; then
      if nand erase.part boot; then
        if nand write 0x2200000 0x2200000 @SIZE@; then
          if nand read 0x8000000 0x2200000 @SIZE@; then
            if cmp.b 0x2200000 0x8000000 @SIZE@; then
              if bootm start 0x8000000#normal; then
                echo NAND verified - environment unchanged - reset now
              else
                echo ERROR readback FIT invalid
              fi
            else
              echo ERROR readback differs
            fi
          else
            echo ERROR NAND read failed
          fi
        else
          echo ERROR NAND write failed
        fi
      else
        echo ERROR boot erase failed
      fi
    else
      echo ERROR source FIT invalid - no erase
    fi
  else
    echo ERROR source size differs - no erase
  fi
else
  echo ERROR source load failed - no erase
fi
'''
script = script.replace('@SIZE_BARE@', f'{size:x}').replace('@SIZE@', f'0x{size:x}')
Path('sd-card/install-boot.txt').write_text(script)
env = f'''setenv fit_load 'nand read 0x8000000 0x2200000 0x{size:x}'
setenv fit_start 'bootm start 0x8000000#normal; bootm loados'
setenv fit_finish 'bootm ramdisk; bootm fdt; bootm prep; bootm go'
setenv nandboot 'run fit_load; run fit_start; run fit_finish'
setenv bootcmd 'run nandboot'
'''
Path('sd-card/boot-env.txt').write_text(env)
print(env)
PY
SOURCE_DATE_EPOCH=0 mkimage -A arm64 -O linux -T script -C none \
  -n 'A113D write boot' -d sd-card/install-boot.txt sd-card/install-boot.scr
(
  cd sd-card
  sha256sum a113d.itb a113d-nand.bin nand-size.hex aml_autoscript \
    aml_autoscript.txt install-boot.scr install-boot.txt boot-env.txt > SHA256SUMS
  sha256sum -c SHA256SUMS
)
```

Файлы `boot-env.txt` и `nand-size.hex` нужны оператору; U-Boot не исполняет их сам. В готовом `boot-env.txt` уже нет условного `S`: записан точный размер.

На носителе **не должно быть `aml_sdc_burn.ini`**. Этот файл выбирает иной Amlogic burn-путь и не нужен ни для RAM-загрузки, ни для описанной записи `boot`.

## 8 Создание образа SD карты

Если карта уже имеет один раздел FAT32 в MBR, достаточно скопировать в его корень содержимое `sd-card/` и проверить хеши после копирования. Для передачи готового файла диска можно собрать `a113d-sd.img` стандартными инструментами. Это дополнительная упаковка; сам репозиторий такой raw-образ не генерирует.

Следующие команды создают **обычные файлы**, не обращаясь к физическому накопителю. В образе 512 МиБ первый раздел начинается с сектора 2048, то есть 1 МиБ. Его длина 511 МиБ, или 1 046 528 секторов по 512 байт.

```sh
cd "$WORKDIR"
test ! -e a113d-sd.img
test ! -e sd-fat32.img
truncate -s 512M a113d-sd.img
printf 'label: dos\nunit: sectors\n\nstart=2048, size=1046528, type=c\n' | \
  sfdisk a113d-sd.img
truncate -s 511M sd-fat32.img
mkfs.vfat -F 32 -n A113D sd-fat32.img
mcopy -i sd-fat32.img sd-card/* ::/
mdir -i sd-fat32.img ::/
dd if=sd-fat32.img of=a113d-sd.img bs=512 seek=2048 conv=notrunc status=progress
sfdisk --dump a113d-sd.img
sha256sum a113d-sd.img > a113d-sd.img.sha256
```

Для записи этого файла в SD-карту сначала определите **весь носитель** через `lsblk -o NAME,SIZE,MODEL,TRAN,MOUNTPOINTS`. Команда ниже уничтожает прежнюю разметку и файлы выбранного носителя. `SD_DEVICE` задаётся оператором после проверки; это не путь раздела вроде `/dev/sdb1`.

```sh
export SD_DEVICE=/dev/REPLACE_WITH_CARD
lsblk -o NAME,SIZE,MODEL,TRAN,MOUNTPOINTS "$SD_DEVICE"
# Отмонтируйте каждый смонтированный раздел выбранной карты через umount.
test -b "$SD_DEVICE"
sudo dd if="$WORKDIR/a113d-sd.img" of="$SD_DEVICE" bs=4M conv=fsync status=progress
sudo cmp -n 536870912 "$WORKDIR/a113d-sd.img" "$SD_DEVICE"
sync
```

MBR здесь нужна для доступа к FAT, а не для запуска PC boot sector. Загрузчик платы продолжает находиться в NAND. Запись этого `.img` не делает карту средством восстановления стёртого bootloader.

### R3 · §9
Пробная загрузка FIT в RAM **до** записи NAND — обязательный шаг квалификации на каждой новой плате.

## 9 Пробная загрузка из RAM

Подключите носитель к USB платы, остановите автозагрузку **до первого `bootm`**. Для ручной проверки выполните по UART последовательно:

```text
usb start
fatls usb 0
fatload usb 0 0x8000000 a113d.itb
bootm start 0x8000000#normal
bootm loados
bootm ramdisk
bootm fdt
bootm prep
bootm go
```

После каждой команды проверяйте результат, при ошибке следующую не выполняйте. `bootm start` должен сообщить об успешной проверке хешей ядра, ramdisk и FDT. Альтернатива — загрузить и вызвать подготовленный `aml_autoscript` способом из раздела 2. Команда `bootm` выбирает FIT-конфигурацию суффиксом `#normal`; это описано в [справке U-Boot](https://docs.u-boot.org/en/latest/usage/cmd/bootm.html).

При `Trying to execute a command out of order` перезагрузите плату и снова остановите таймер до первого `bootm`: после предыдущей попытки могло остаться состояние поэтапной загрузки. Не используйте `saveenv` для исправления этой ошибки.

После появления Alpine найдите IP в DHCP-таблице или UART-логе. С компьютера:

```sh
ssh -i "$PRIVATE_KEY" -o IdentitiesOnly=yes root@BOARD_IP
```

`BOARD_IP` далее заменяется адресом платы. На плате проверьте:

```sh
uname -r
cat /proc/cmdline
tr '\000' '\n' < /proc/device-tree/compatible
cat /proc/mtd
ip -br address
cat /proc/mounts
dmesg | tail -n 100
```

Ожидаются `6.18.42-a113d-production`, совместимость `a113d-devboard,miner-ctrl-board-a113d-v1.1`, рабочий Ethernet. Если `nvdata` уже годен, `/mnt/nvdata` должен быть `ubifs`, а `/` — `overlay`. Если его нет, SSH из RAM всё равно должен работать, но изменения пока не сохраняются.

### R3 · §10
Backup NAND перед стиранием `boot` — закон. Карта разделов — сверять с DTS этой ревизии.

## 10 Карта NAND и резервное копирование

Номера Linux MTD определяет порядок узлов DTS. `nenv` намеренно добавлен последним, хотя физически находится ближе к началу NAND.

| Linux | Имя | Смещение NAND | Размер | Production |
|---|---|---|---|---|
| `mtd0` | `bootloader` | `0x00000000` | `0x00200000` | Только чтение |
| `mtd1` | `tpl` | `0x00800000` | `0x00800000` | Только чтение |
| `mtd2` | `misc` | `0x01000000` | `0x00200000` | Только чтение |
| `mtd3` | `recovery` | `0x01200000` | `0x01000000` | Только чтение |
| `mtd4` | `boot` | `0x02200000` | `0x02000000` | Чтение и запись |
| `mtd5` | `config` | `0x04200000` | `0x00500000` | Только чтение |
| `mtd6` | `nvdata` | `0x04700000` | `0x0b900000` | Чтение и запись |
| `mtd7` | `nenv-journal-ro` | `0x00300000` | `0x00100000` | Только чтение |

Пропуски между разделами не являются свободным местом для файлов. В них есть служебные данные boot-chain и NBBT. Смещения из таблицы относятся к NAND целиком; `/dev/mtd4` уже начинается с нужного раздела, поэтому `nandwrite /dev/mtd4` выполняется с локального смещения 0, а не `0x02200000`.

Перед любой записью проверьте на плате:

```sh
mtdinfo /dev/mtd4
mtdinfo /dev/mtd6
for n in 4 6; do
  for f in name size erasesize writesize bad_blocks ecc_failures; do
    printf 'mtd%s %s: ' "$n" "$f"
    cat "/sys/class/mtd/mtd$n/$f"
  done
done
```

Для этой платы eraseblock — 131 072 байта, страница — 2048 байт, `mtd4` — 33 554 432 байта, `mtd6` — 193 986 560 байт. При несовпадении карты, ECC-ошибках или bad blocks в `boot` данный простой порядок установки не применяется. Корректируемые ECC-события требуют оценки динамики, а `ecc_failures` означают ошибку чтения.

Сохраните весь старый `boot` на компьютере, не расходуя RAM платы на 32-МиБ временный файл. Выполните на компьютере с Linux Bash:

```sh
cd "$WORKDIR"
mkdir -p backup
ssh -i "$PRIVATE_KEY" root@BOARD_IP \
  'nanddump --omitoob --bb=padbad -q -l 33554432 /dev/mtd4' > backup/boot-before.bin
test "$(wc -c < backup/boot-before.bin)" -eq 33554432
ssh -i "$PRIVATE_KEY" root@BOARD_IP 'cat /proc/mtd; uname -a' > backup/board.txt
sha256sum backup/boot-before.bin > backup/boot-before.sha256
```

Сохраните рядом UART-журнал полного `printenv`. При необходимости архивируйте важные файлы смонтированного `nvdata` отдельно до форматирования. Дамп `boot` и `printenv` не являются полным заводским backup: для восстановления уничтоженных bootloader/TPL/NBBT нужны отдельные recovery-материалы.

Окружение Amlogic — вращающийся журнал `nenv` в диапазоне `0x00300000`–`0x00400000`. Это **не `mtd1`** и не обычный фиксированный U-Boot env blob. На mainline нет vendor `/dev/nand_env`. Для записи окружения используйте только штатный U-Boot `saveenv`; не направляйте Linux `fw_setenv` в `mtd1` или `mtd7`.

### R3 · §11
`nvdata` — опция постоянства. Первый успешный образ может жить в RAM-only.

## 11 Подготовка постоянных данных

Этот этап нужен только если `nvdata` пуст, повреждён или его содержимое решено полностью заменить. Рабочий существующий UBI/UBIFS сохраняйте. Ошибка mount сама по себе не является разрешением форматировать данные.

UBIFS работает поверх UBI и MTD, а не непосредственно поверх обычного блочного раздела; см. [документацию Linux UBIFS](https://docs.kernel.org/filesystems/ubifs.html). Для новой системы достаточно одного UBI-тома `nvdata` максимального размера; первый mount создаёт файловую систему на пустом динамическом томе.

Команды ниже выполняются в RAM-загруженной Alpine **после сохранения нужных данных**. Они отказываются работать, если `mtd6` уже подключён к UBI или `/mnt/nvdata` смонтирован. Это отдельная разрушительная операция только над `mtd6`.

```sh
sh <<'SH'
set -eu
test "$(uname -r)" = 6.18.42-a113d-production
tr '\000' '\n' < /proc/device-tree/compatible | \
  grep -Fxq a113d-devboard,miner-ctrl-board-a113d-v1.1
test "$(cat /sys/class/mtd/mtd6/name)" = nvdata
test "$(cat /sys/class/mtd/mtd6/size)" = 193986560
test "$(cat /sys/class/mtd/mtd6/erasesize)" = 131072
test "$(cat /sys/class/mtd/mtd6/bad_blocks)" = 0
test "$(cat /sys/class/mtd/mtd6/ecc_failures)" = 0
if awk '$2 == "/mnt/nvdata" { found=1 } END { exit !found }' /proc/mounts; then
  echo 'nvdata mounted; stop' >&2; exit 1
fi
for p in /sys/class/ubi/ubi*/mtd_num; do
  test -f "$p" || continue
  if test "$(cat "$p")" = 6; then
    echo 'mtd6 already attached; stop' >&2; exit 1
  fi
done
test ! -e /dev/ubi0
ubiformat /dev/mtd6 -y
ubiattach -m 6 -d 0
ubimkvol /dev/ubi0 -N nvdata -m
mkdir -p /mnt/nvdata
mount -t ubifs ubi0:nvdata /mnt/nvdata
mkdir -p /mnt/nvdata/devboard/boot /mnt/nvdata/devboard/overlays
mkdir -p /mnt/nvdata/devboard/persist/home \
  /mnt/nvdata/devboard/persist/opt /mnt/nvdata/devboard/persist/ssh
sync
SH
```

Перезагрузите плату и снова выполните RAM-загрузку с карты. `/init` создаст upper/work и постоянный host key; проверьте `/` типа `overlay`. SSH fingerprint после перехода с временного ключа на постоянный может измениться. Сверьте новый fingerprint по UART, прежде чем заменять запись `known_hosts`.

### R3 · §12–13
Установка: U-Boot с носителя **или** запись из Linux (предпочтительнее на стенде). После — холодный старт без носителя + проверка SSH/MCP.

## 12 Установка из карты непосредственно через U-Boot

Этот путь не требует доступа Linux к USB. К этому моменту должны быть выполнены RAM-загрузка, проверка платы и NAND, резервное копирование; `nvdata` должен быть подготовлен, если нужна постоянная система.

Перезагрузите плату, остановите U-Boot до автозагрузки и запустите **именно `install-boot.scr`**:

```text
usb start
fatload usb 0 0x6000000 install-boot.scr
autoscr 0x6000000
```

Скрипт выполняет следующую последовательность:

1. Читает `a113d-nand.bin` из FAT в RAM и проверяет размер.
2. Проверяет исходный FIT через `bootm start` до стирания.
3. Стирает **только раздел `boot`** командой `nand erase.part boot`.
4. Записывает файл с data-only байтами по NAND-смещению `0x02200000`.
5. Читает записанный диапазон в другую область RAM и сравнивает все `S` байт через `cmp.b`.
6. Повторно проверяет хеши FIT в считанной копии.

Любой `ERROR` означает остановку; успешная загрузка Linux в прошлом не заменяет readback. Скрипт не создаёт `nvdata`, не меняет загрузчик и не сохраняет окружение. После сообщения `NAND verified - environment unchanged - reset now` выполните `reset` и снова остановите автозагрузку. Это сбрасывает состояние `bootm start`.

### Проверка нового NAND пути без saveenv

На компьютере откройте `sd-card/boot-env.txt`. Вставьте все пять строк в свежую консоль U-Boot. Они меняют только текущую RAM-копию окружения. Проверьте её и попробуйте загрузку:

```text
printenv fit_load fit_start fit_finish nandboot bootcmd
run nandboot
```

Убедитесь, что чтение идёт из NAND, хеши прошли, загрузился нужный Linux, работают SSH, `/mnt/nvdata` и OverlayFS. Этот шаг проверяет именно высокое размещение FIT и новый размер чтения. После перезагрузки несохранённые значения исчезнут.

### Сохранение проверенного окружения

Снова перезагрузите плату и остановите U-Boot. Вставьте те же пять строк из `boot-env.txt`. Убедитесь, что значения совпадают с только что проверенными. Лишь затем:

```text
printenv fit_load fit_start fit_finish nandboot bootcmd
saveenv
reset
```

`saveenv` сохраняет всю текущую RAM-копию окружения, поэтому до него не экспериментируйте с посторонними переменными. Предыдущий журнал может содержать более раннюю запись, но это не автоматическая процедура отката; сохранённый полный `printenv` остаётся необходимым.

Уберите карту и проверьте самостоятельную загрузку. В исходном assisted-установщике эти пять переменных сохраняются сразу после readback. Раздельная процедура выше позволяет проверить новый адрес FIT до постоянной настройки.

## 13 Альтернатива запись boot из работающего Linux

Это эквивалент основной механики production Linux-установщика. Его исходный вариант дополнительно требует совпадения release/compatible, геометрии, монтированного UBIFS, активного OverlayFS, хеша пакета и подтверждения хеша оператором. Затем он сохраняет полный старый `mtd4`, стирает и пишет только `mtd4`, сверяет readback и оставляет окружение U-Boot неизменным.

Используйте **один** путь записи: этот или раздел 12. После обоих требуется проверка и настройка окружения. Linux USB выключен в DTS, поэтому файл передаётся с компьютера по SSH:

```sh
cd "$WORKDIR"
scp -i "$PRIVATE_KEY" out/a113d-nand.bin root@BOARD_IP:/tmp/
sha256sum out/a113d-nand.bin
```

На плате после всех проверок разделов 9–11 и резервного копирования выполните блок, подставив SHA-256, который только что получили на компьютере:

```sh
EXPECTED_SHA256=REPLACE_WITH_HOST_SHA256 sh <<'SH'
set -eu
test "$(uname -r)" = 6.18.42-a113d-production
tr '\000' '\n' < /proc/device-tree/compatible | \
  grep -Fxq a113d-devboard,miner-ctrl-board-a113d-v1.1
test "$(cat /sys/class/mtd/mtd4/name)" = boot
test "$(cat /sys/class/mtd/mtd4/size)" = 33554432
test "$(cat /sys/class/mtd/mtd4/erasesize)" = 131072
test "$(cat /sys/class/mtd/mtd4/bad_blocks)" = 0
test "$(cat /sys/class/mtd/mtd4/ecc_failures)" = 0
awk '$2=="/mnt/nvdata" && $3=="ubifs" {ok=1} END {exit !ok}' /proc/mounts
awk '$2=="/" && $3=="overlay" {ok=1} END {exit !ok}' /proc/mounts
printf '%s  /tmp/a113d-nand.bin\n' "$EXPECTED_SHA256" | sha256sum -c -
S=$(wc -c < /tmp/a113d-nand.bin)
test "$S" -gt 0
test "$S" -le 33554432
test "$((S % 131072))" -eq 0
AVAILABLE_KB=$(df -Pk /mnt/nvdata | awk 'END {print $4}')
test "$((AVAILABLE_KB * 1024))" -ge "$((33554432 + S + 1048576))"
BACKUP_DIR="/mnt/nvdata/boot-backup-$(date -u +%Y%m%dT%H%M%SZ)"
test ! -e "$BACKUP_DIR"
mkdir "$BACKUP_DIR"
nanddump --omitoob --bb=padbad -q -l 33554432 \
  -f "$BACKUP_DIR/boot-before.bin" /dev/mtd4
test "$(wc -c < "$BACKUP_DIR/boot-before.bin")" -eq 33554432
sha256sum "$BACKUP_DIR/boot-before.bin" > "$BACKUP_DIR/boot-before.sha256"
sync
flash_erase /dev/mtd4 0 0
nandwrite -p /dev/mtd4 /tmp/a113d-nand.bin
sync
nanddump --omitoob --bb=padbad -q -l "$S" \
  -f "$BACKUP_DIR/boot-readback.bin" /dev/mtd4
test "$(wc -c < "$BACKUP_DIR/boot-readback.bin")" -eq "$S"
cmp /tmp/a113d-nand.bin "$BACKUP_DIR/boot-readback.bin"
sha256sum "$BACKUP_DIR/boot-readback.bin"
sync
echo 'NAND verified; U-Boot environment unchanged'
SH
```

Не заменяйте `nandwrite` на `dd` в MTD: raw NAND требует обработки страниц, ECC и bad blocks. `--omitoob` исключает OOB из backup/readback; файл FIT тоже не содержит OOB. Рецепт рассчитан на проверенный `boot` без bad blocks, как и ограничение исходного production-установщика.

После записи перейдите к проверке нового NAND пути и сохранению окружения из раздела 12. Простой исходный finalizer, который меняет только `fit_load`, **не подходит для смены адреса FIT**, пока не согласован и `fit_start`.

Для уже квалифицированной системы с подходящим адресом и неизменным `fit_start` логика исходного finalizer короче: прочитать весь новый FIT из NAND в `0x08000000`, выполнить `bootm start`, изменить только длину в `fit_load`, выполнить `saveenv`, затем сразу `reset`. При совпадающем значении finalizer пропускает `saveenv`.

### R3 · §14
Приёмка: UART login/dropbear · eth · MCP `:8443`. Затем — тот же FIT на следующую AML-плату класса.

## 14 Проверка результата и восстановление

После загрузки без карты проверьте версию ядра, успешные FIT hashes по UART, DHCP и SSH, правильную MTD-карту, `/` типа `overlay`, `/mnt/nvdata` типа `ubifs`, отсутствие новых неисправимых ECC/UBI-ошибок.

Проверьте сохранение файлов и ключа между перезагрузками:

```sh
date -u > /home/persistence-check.txt
ssh-keygen -lf /etc/ssh/host_keys/ssh_host_ed25519_key.pub
sync
reboot
```

После повторного входа файл должен сохраниться, а fingerprint — остаться прежним. Затем выполните отдельную проверку холодного включения: штатно выключите систему, снимите и верните питание, проверьте автономную загрузку. Успешный warm reset не заменяет этот тест.

Если новый `boot` не загружается, остановите U-Boot и запустите сохранённый recovery FIT с карты через высокий RAM-адрес. Это работает, пока сохранны штатные ранние стадии и USB-загрузка U-Boot. Из RAM-системы можно восстановить прежний data-only `boot-before.bin` с теми же проверками NAND, `flash_erase`, `nandwrite` и полным readback, а затем вернуть точные прежние значения из сохранённого `printenv`. Сам старый `fit_load` должен быть достаточен для старого FIT. Не стирайте всю NAND для восстановления одного `boot`.

Если требуется обход повреждённого OverlayFS при рабочем `nvdata`, создайте `/mnt/nvdata/devboard/boot/rescue` и перезагрузитесь. Для возвращения в обычный режим удалите этот маркер из rescue. При неисправности самого UBI `/init` и так переходит к запуску из RAM, не форматируя данные.

### R3 · §15
Assisted/burn bootstrap — **внешняя** зависимость. Не смешивать с независимым путём «живой U-Boot + plain FIT», пока не упрёмся.

## 15 Что делает двухэтапный assisted установщик проекта

Это отдельная ветка, не обязательная для процедуры с доступным штатным U-Boot. `build_assisted_package()` собирает ZIP из уже готового production-пакета и **внешнего** набора `vnishru-s21-aml-sd-v1.2.6-install`.

| Каталог | Содержимое и действие |
|---|---|
| `01-bootstrap-card` | Три готовых signed/encrypted файла донора; Amlogic burn-путь |
| `02-install-card` | Production NAND-файл и скомпилированный `aml_autoscript`; проверка FIT, стирание `boot`, запись, readback, пять переменных, `saveenv` |
| `03-nvdata-ssh` | Helper форматирования только `mtd6` и создания `ubi0:nvdata` |
| `metadata` | Публичный ключ, конфигурация ядра и манифесты |

Генератор **не компилирует и не подписывает bootstrap**: он проверяет размеры/хеши готовых файлов и копирует их. В `aml_sdc_burn.ini` проверяются строки `erase_bootloader =1`, `erase_flash =1`, `reboot =0` с точным исходным форматированием. Первый этап стирает загрузчик и содержимое flash; это гораздо шире записи одного `mtd4`.

Зафиксированный входной набор:

| Файл | Байт | SHA-256 |
|---|---:|---|
| `aml_sdc_burn.ini` | 602 | `a37b0bd7b0fd1442b4ee84a5e824808326990ee3f83155eefabcf5a820f70744` |
| `aml_sdc_burn.UBOOT.ENC` | 818688 | `5438fb730e9610bfdbb96c454d92cfb626980f21a358f695f9b2c155a7bf3691` |
| `aml_upgrade_package_enc.img` | 22991024 | `be2a3e147d0de0bda57404c78a41da4fde9aa9554ffac5c8e32cdbc6c6212b74` |

При наличии ровно этих файлов упаковка первого носителя — обычное копирование с проверкой `sha256sum`; подбирать похожую версию вместо зафиксированной нельзя. Первый и второй этапы размещают на разных носителях. Если второй носитель не запускается автоматически, нужна ручная проверенная команда `autoscr`; распознавание USB не доказывает исполнение скрипта.

Собрать эти подписанные bootstrap-бинарники из серии патчей Linux невозможно. Для платы, которая не допускает ручную загрузку plain FIT, остаётся внешняя зависимость от подходящего recovery/bootstrap и его совместимости с конкретной ревизией. Настоящее руководство полностью раскрывает независимый путь **при работающем U-Boot, способном загрузить plain FIT**, но не заменяет отсутствующую подписанную первую стадию.

В исходниках также описан будущий factory FIT со встроенным production payload и одноразовой установкой. В `docs/architecture.md` он помечен как план; это не реализованный рецепт, которым можно заменить шаги выше. Установка на новую вторую плату и холодный старт требуют собственной квалификации.

### R3 · §16
Таблица путей — к репо Макса. У нас зеркало серии: `board/aml/` и `/home/george/aml_kernel/`.

## 16 Где сверять реализацию

Технические значения ниже относятся к рабочим файлам репозитория, просмотренным 7 ноября 2026 года. Базовый Git commit — `949ccc343fcf14644963a8a411ee112439ba3e8c`; рабочее дерево содержит последующие изменения, поэтому одного checkout этого commit недостаточно для воспроизведения нынешнего `series/`.

| Предмет | Основной источник в репозитории |
|---|---|
| Версия ядра и архив | `series/source.lock` |
| Патчи и порядок | `series/patches/series` |
| DTB, память, NAND, UART, Ethernet | `series/dts/meson-axg-a113d-miner-production.dts` |
| Командная строка и встроенные драйверы | `series/configs/a113d-production.config` |
| Пять переменных окружения | `src/devboard_tool.py`, `boot_environment()` |
| FIT, выравнивание, legacy script | `lab_fit_source()`, `create_nand_image()`, `compile_uboot_script()` в том же файле |
| Адреса NAND и RAM | `platforms/a113d-axg/platform.json`, константы `LAB_*` в `src/devboard_tool.py` |
| Rootfs и OverlayFS | `rootfs/Dockerfile`, `rootfs/rootfs/init` |
| Прежний SD installer | `templates/sd/aml_autoscript.txt.in` |
| Production Linux writer и finalizer | `templates/nand/install-production.sh.in`, `templates/nand/finalize-production-env.txt.in` |
| Assisted и внешний bootstrap | `templates/assisted/`, `build_assisted_package()` |
| Результаты прежних аппаратных проверок | `docs/nand-audit.md` |
| Выполненная архитектура и будущий factory mode | `docs/architecture.md` |

Исходный SD-шаблон проверяет размер и readback, но не делает предварительный `bootm start`; assisted-шаблон добавляет проверку FIT до и после записи. Полный старый `boot` автоматически сохраняет именно Linux installer; U-Boot SD-шаблон этого не делает. Поэтому резервное копирование в этом руководстве вынесено перед обоими путями.

Самостоятельные команды выше составлены по этим контрактам. Сборка нового rootfs и kernel, установка по данному руководству и аппаратная квалификация высокого адреса NAND-загрузки должны выполняться и фиксироваться как отдельный новый прогон; результаты старых образов на них автоматически не распространяются.


---

## Подпись R3

Донор: Макс · 2026-11-07.  
Адаптация под цель «образ + перенос на все AML как SBC»: [[cursor]] по слову [[george]] · 2026-11-07.  
Новые прогоны железа фиксировать в `log/`, не считать старые образы автоматически годными.
