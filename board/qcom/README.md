# board/qcom — Android boot.img для лабы (как у Мити)

Как у [[lumag]]: ядро (+dtb) → `mkbootimg` → `boot.img` → `cdba -b <board> -h umbar.lab ./boot.img`.

## Alpine minirootfs (как Zynq/ant)

Аналог `ARCH=armv7 ./alpine-rootfs.sh …` + `install-init.sh`, но **aarch64** и консоль **ttyMSM0**:

```bash
# полный конвейер: Alpine → ramdisk ant+netd → boot-<slot>-alpine.img
./scripts/mk-qcom-alpine-boot.sh rb3-1
# только упаковка, rootfs уже есть:
SKIP_ROOTFS=1 ./scripts/mk-qcom-alpine-boot.sh ifc6560-1
```

| | Zynq | qcom |
|---|---|---|
| ARCH | armv7 | aarch64 |
| console | ttyPS0 | ttyMSM0 |
| носитель | fit/SD | Android `boot.img` → cdba |
| после `/init` | BusyBox init → getty | то же (**без** `stay`) |

Ramdisk: `out/ramdisk_ant_aarch64_netd.cpio.gz` (~7 M).  
Образ: `out/boot-<BOARD_ID>-alpine.img`.  
Рядом остаётся путь **net_init stay** (тонкий MCP) — другой режим.


## dts/dtb на каждую плату

Слот umbar = каталог `board/qcom/<BOARD_ID>/`:

| файл | роль |
|------|------|
| `*.dts` | исходник устройства |
| `*.dtb` | blob для mkbootimg/cdba |
| `SOURCE.txt` | откуда / sha256 / missing |

Указатель: [`DTB-INDEX.md`](DTB-INDEX.md).

```bash
./scripts/save-board-dtb.sh [/path/to/arm64/boot/dts/qcom]
```

## Режимы mkbootimg

| mode | когда |
|------|--------|
| `legacy` | zImage/Image.gz **cat** dtb → `--kernel` |
| `v2` | `--header_version 2` + `--dtb` |

```bash
KERNEL=path/to/Image.gz DTB=board/qcom/rb1-1/qrb2210-rb1.dtb \
  RAMDISK=out/ramdisk_net_init_a64.cpio.gz \
  ./scripts/mk-android-bootimg.sh board/qcom/rb1.conf
```

Ramdisk для нашей загрузки: **net_init** (`out/ramdisk_net_init_a64.cpio.gz`).

Источники `/init`: `../../src/init/` (не соседние репозитории).

## Канон Мити (2026-11-04, без слов)

Прислал `CFG-64` + `build.sh` → приняли как рецепт:

| что | куда |
|---|---|
| `.config` arm64 7.3.0-rc7 | `kernel/CFG-64` |
| матрица boot.img | `inbox/mitya-2026-11-04/build.sh` · наш адаптер `scripts/mk-qcom-boot-family.sh` |

Цифры с его `build.sh`: **base `0x80000000`**, **pagesize `2048`** (не 4096).  
rb5 / rb3gen2: **v2 + Image без gzip** (`nogz`).  
cmdline лаборатории: его `ttyMSM0` + `clk_ignore_unused` + наш `rdinit=/init root=/dev/ram0 stay` (не копируем `PARTLABEL=rootfs` / RPB paths).

```bash
./scripts/mk-qcom-boot-family.sh --list
KERNEL=out/kernel/Image-qcom.gz RAMDISK=out/ramdisk_net_init_a64.cpio.gz \
  ./scripts/mk-qcom-boot-family.sh rb1
```
