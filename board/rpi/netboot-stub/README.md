# netboot-stub — что на SD

На карте (FAT `BOOT`) **только** `bootcode.bin`.

Так обходятся баги ROM Pi3: дальше start.elf / kernel / initrd идут с **TFTP** с arc2 (`/srv/tftp/rpi`), как payload после fastboot.

Раздел `trialroot` (ext4, p2) при первой пробе **оставлен** — cmdline на TFTP пока указывает `root=/dev/mmcblk0p2`. Следующий шаг — ramdisk stay / netd без корня на SD.

## Прошивка (уже сделано на arc2 2026-11-04)

USB reader `/dev/sdd`: p1 очищен → один `bootcode.bin`. Бэкап старого BOOT: `out/rpi/backup-sd-2026-11-04/`.

## Сервер

- dnsmasq proxy DHCP + TFTP на `bond0`
- conf: `/etc/dnsmasq.d/rpi-netboot.conf`
- tftp-root: `/srv/tftp/rpi`

## После вставки SD

Питание Pi → смотреть `journalctl -u dnsmasq -f` и линк MES `gi1/0/10`.
