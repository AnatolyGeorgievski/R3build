# board/aml — Amlogic A113D (aarch64)

Консоль: `ttyAML0`. Серия и инструкция Макса Фурмана (production A113D).

Канон r3build (цель + донор Макс): [`docs/R3_AML_A113D.md`](../../docs/R3_AML_A113D.md).

Носитель в r3build: этот каталог. Исходный дубль на диске: `/home/george/aml_kernel/`.  
Сборка 2026-10-30 по инструкции «отдельно на Linux»: артефакты `/home/george/a113d-build/build/` (`Image.gz`, dtb).

**Не в git:** приватный SSH-ключ; бинарники USB-boot (~40 МБ `.enc`/`.img`) — остаются в `/home/george/aml_kernel/alpine/usb-boot/` (здесь только `alpine/usb-boot/README.txt` и pubkey).

**03.11 ~23:16:** с Максом — продолжение AML; его Qualcomm/NPU и наше r3build — вместе (он знает Митю; GPU Adreno не его зона). Ход: `log/2026-10-30 коллаборанты Борис Макс.md`.

Коллаборанту «можно ли собрать ядро»: **`ЕСЛИ-СОБРАТЬ-ЯДРО.md`** (да — Image.gz+dtb; нет — mtd4/rootfs/USB без доп. файлов).

---

# A113D: ядро, DTS и конфигурация

Самостоятельный набор для `MINER_CTRL_BOARD_A113D_V1.1`: Linux **6.18.42**,
ARM64, 256 МиБ RAM и 256 МиБ NAND. Версия архива и SHA-256 закреплены в
[`source.lock`](source.lock). Каталог можно передавать отдельно от репозитория.

## Состав

- `patches/series` — порядок применения трёх патчей:
  1. импорт vendor NBBT в драйвер Meson NAND;
  2. GPIO IRQ bridge для событий через GPIO character device;
  3. DT binding для GPIO IRQ bridge.
- `dts/meson-axg-a113d-miner-production.dts` — описание платы.
- `configs/a113d-production.config` — фрагмент поверх `arm64 defconfig`.

Это основные исходники production-профиля. Сборщики репозитория и экспорт
`prod/` используют их отсюда. Лабораторные DTS и конфиги остаются в `kernel/`.
Патчи применяются и к лабораторным профилям; функции включаются через DTS/конфиг.

## Сборка в репозитории

Нужны Docker с поддержкой `linux/arm64`, `curl`, `tar`, `xz` и `sha256sum`
(на macOS последний доступен в GNU coreutils). Из корня репозитория:

```sh
./kernel/fetch_source.sh
./kernel/build_kernel.sh --profile production
```

Результат — `kernel/out/6.18.42-production/`: `Image.gz`, DTB, `kernel.config`,
`kernel.release`, `System.map`, `SHA256SUMS`. `--clean` очищает только каталог
объектов и выходные файлы выбранного профиля.

## Сборка отдельно, на Linux

Для Debian/Ubuntu нужны `build-essential gcc-aarch64-linux-gnu bc bison flex
libssl-dev libelf-dev dwarves cpio rsync device-tree-compiler patch curl xz-utils`.
Команды ниже выполняются из переданного каталога `series/`; рядом создаётся
`a113d-build/`. Используйте пустой каталог сборки.

```sh
set -eu
SERIES_DIR=$(pwd)
. "$SERIES_DIR/source.lock"
mkdir -p ../a113d-build
cd ../a113d-build
test ! -e "linux-$KERNEL_VERSION"
test ! -e build
curl --fail --location --retry 3 -o "$KERNEL_ARCHIVE" "$KERNEL_URL"
printf '%s  %s\n' "$KERNEL_SHA256" "$KERNEL_ARCHIVE" | sha256sum -c -
tar -xJf "$KERNEL_ARCHIVE"
SOURCE_DIR="$PWD/linux-$KERNEL_VERSION"
BUILD_DIR="$PWD/build"

while IFS= read -r name || [ -n "$name" ]; do
    case "$name" in ''|\#*) continue ;; esac
    patch --batch --forward --fuzz=0 -d "$SOURCE_DIR" -p1 \
        -i "$SERIES_DIR/patches/$name"
done < "$SERIES_DIR/patches/series"
cp "$SERIES_DIR/dts/meson-axg-a113d-miner-production.dts" \
    "$SOURCE_DIR/arch/arm64/boot/dts/amlogic/"

export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
export KBUILD_BUILD_USER=a113d-devboard KBUILD_BUILD_HOST=container
export KBUILD_BUILD_TIMESTAMP='1970-01-01 00:00:00 UTC' SOURCE_DATE_EPOCH=0
make -C "$SOURCE_DIR" O="$BUILD_DIR" defconfig
"$SOURCE_DIR/scripts/kconfig/merge_config.sh" -m -O "$BUILD_DIR" \
    "$BUILD_DIR/.config" "$SERIES_DIR/configs/a113d-production.config"
make -C "$SOURCE_DIR" O="$BUILD_DIR" olddefconfig
make -C "$SOURCE_DIR" O="$BUILD_DIR" -j"$(nproc)" \
    Image.gz amlogic/meson-axg-a113d-miner-production.dtb
```

На ARM64 Linux с нативным GCC можно задать `CROSS_COMPILE=`. Результаты:
`build/arch/arm64/boot/Image.gz`,
`build/arch/arm64/boot/dts/amlogic/meson-axg-a113d-miner-production.dtb`,
`build/.config` и `build/System.map`. DTB собирается по явной цели: плата
не добавлена в список `make dtbs`. Версии компилятора и зависимостей могут
изменить байты результата; для окружения проекта используйте Docker-сборку.

## Границы набора

Команды собирают ядро и DTB. Rootfs/initramfs с `/init` и упаковка FIT выполняются
отдельно; в репозитории это `./devboard build-rootfs` и `./devboard build-lab`.
Модули и firmware этими командами не собираются и не устанавливаются.

DTS сохраняет `boot=mtd4`, `nvdata=mtd6`, `nenv-journal-ro=mtd7`; порядок
разделов менять нельзя без согласованного изменения userspace. Production
разрешает запись в boot/nvdata и поддерживает UBI/UBIFS: загрузка из RAM сама
по себе не делает этот профиль read-only. GPIO sysfs выключен, IRQ bridge
включён только для periphs, USB в DTS выключен.

Это локальная серия для закреплённого ядра, не готовая upstream-подача.
Свойство `amlogic,legacy-nbbt` пока не описано в YAML binding; полная
`dtbs_check`-валидация не заявляется. Перенос файлов не расширяет ранее
проверенную совместимость платы и периферии.
