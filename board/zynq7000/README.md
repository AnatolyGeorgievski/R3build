# board/zynq7000 — Zynq (armv7)

Консоль: `ttyPS0`. Носитель подробностей и патча ядра в **r3build** (раньше DTB лежал только в `dts/`).

## Состав

| путь | что |
|------|-----|
| `dts/zynq7020.dts` | Device-Tree контрольной платы Zynq7020 (E9/E9-pro) |
| `dts/zynq7020.dtb` | собранный DTB (копия прежнего `r3build/dts/zynq7020.dtb`) |
| `patches/nand_micron.patch` | On-Die ECC Micron под PL35x (без патча NAND не видит поддержку) |
| `patches/series` | порядок: `nand_micron.patch` |

Канон установки SD/NAND и fit: `godminer/bin/E9pro-boot/INSTALL.md`.

## Сборка ядра (linux-xlnx)

Как в INSTALL:

```sh
git clone https://github.com/Xilinx/linux-xlnx.git
cd linux-xlnx/
git checkout xlnx_rebase_v6.18_LTS
# патч из этой серии (формат файла — как в linux-xlnx/nand_micron.patch)
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make xilinx_zynq_defconfig
make -j$(nproc) zImage dtbs
```

Замечание: новые ядра без GPIO sysfs — `gpiod`. FPGA Manager и `/dev/mem` — для AXI.
