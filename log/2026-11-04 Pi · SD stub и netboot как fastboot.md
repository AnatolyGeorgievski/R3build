# 2026-11-04 — Pi · SD stub и netboot как fastboot

<tags>rpi, netboot, tftp, fastboot, стенд, sd</tags>

<topo>
name: [[cursor]]
links:
  - [[george]]
path: r3build/log/2026-11-04 Pi · SD stub и netboot как fastboot.md
</topo>

Слово [[george]]: могу переткнуть SD; для модели стенда нужна **сетевая загрузка образа подобно fastboot**.

## Решение

SD — один раз под **тонкий netboot-stub**. Меняющийся образ (kernel + dtb + ramdisk net_init/netd+gpio) — с **arc2 по TFTP**, как `boot.img` через cdba/fastboot на umbar.

Полный dd rootfs на SD каждый раз — не модель стенда.

Канон: `board/rpi/README.md`.

Pi сейчас: ping `169.254.221.82` ok; TFTP на arc2 ещё нет.

**подпись:** Курсор

## Сделано 04.11 ~14:03

- SD в USB reader `/dev/sdd`: BOOT → только `bootcode.bin`; `trialroot` p2 не трогали.
- Бэкап BOOT: `out/rpi/backup-sd-2026-11-04/`.
- TFTP: `/srv/tftp/rpi` (start.elf, fixup, dtb, kernel7.img, initrd.img со старой карты).
- dnsmasq proxy на bond0, active.

Ждём: вставить SD в Pi, питание, сверка dnsmasq/TFTP.

## Сделано 04.11 ~14:34

- `out/rpi/netd-rpi.armhf` — `netd_minimal` + `board_rpi` (GPIO_BANK_OFFS=0; ASIC-заглушки).
- `out/rpi/ramdisk_netd_armhf.cpio.gz` → `/srv/tftp/rpi/initrd.img` (старый trialroot: `initrd.img.trialroot-old`).
- cmdline TFTP: `root=/dev/ram0` · без mmc p2.
- скрипт: `scripts/mk-rpi-netd-ramdisk.sh`.
- запас: `out/rpi/init-armhf` + `ramdisk_net_init_armhf.cpio.gz` (stub MCP stay).

Ждём: SD stub в Pi · питание · eth в gi1/0/10 · UART `/dev/ttyUSB0` · ping/MCP :8443 · `gpio get -g 17`.

**подпись:** Курсор
