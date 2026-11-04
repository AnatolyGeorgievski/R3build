# board/qcom — Android boot.img для лабы (как у Мити)

Как у [[lumag]]: ядро (+dtb) → `mkbootimg` → `boot.img` → `cdba -b <board> -h umbar.lab ./boot.img`.

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
