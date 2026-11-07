#!/usr/bin/env bash
# write-rpi-netboot-stub.sh — тонкий SD stub под PasPi (только bootcode.bin → TFTP)
# Использование: write-rpi-netboot-stub.sh /dev/sdX PI1|PI2|PI3
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BOOTCODE="${BOOTCODE:-$ROOT/board/rpi/netboot-stub/bootcode.bin}"
DEV="${1:-}"
TAG="${2:-PI}"
LABEL="${TAG}BOOT"

die() { echo "F: $*" >&2; exit 2; }
[[ -n "$DEV" ]] || die "нужен диск, напр. /dev/sdd"
[[ -b "$DEV" ]] || die "нет блока $DEV"
[[ -f "$BOOTCODE" ]] || die "нет $BOOTCODE"
case "$DEV" in
  /dev/sd*[0-9]|/dev/mmcblk*p*) die "дай целый диск (/dev/sdd), не раздел" ;;
esac
# не трогать системные
for bad in /dev/sda /dev/sdb /dev/sdc /dev/nvme0n1; do
  [[ "$DEV" == "$bad" ]] && die "отказ: $DEV системный"
done

SZ=$(blockdev --getsize64 "$DEV")
# 8G..64G — SD 32G Netac в этом коридоре
(( SZ >= 8*1024*1024*1024 && SZ <= 64*1024*1024*1024 )) || die "размер $SZ не похож на SD 32G"

echo "I: $DEV ($((SZ/1024/1024/1024))G) → stub $LABEL"
# снять монты
while read -r mp; do
  [[ -n "$mp" ]] && umount "$mp" 2>/dev/null || umount -l "$mp" 2>/dev/null || true
done < <(lsblk -nr -o MOUNTPOINT "$DEV" | awk 'NF')

wipefs -a "$DEV"
parted -s "$DEV" mklabel msdos
# 256MiB FAT хватает с запасом под один bootcode.bin
parted -s "$DEV" mkpart primary fat32 1MiB 257MiB
parted -s "$DEV" set 1 boot on
sleep 1
PART="${DEV}1"
[[ -b "$PART" ]] || PART="${DEV}p1"
[[ -b "$PART" ]] || die "нет раздела после parted"

mkfs.vfat -F 32 -n "$LABEL" "$PART"
M=$(mktemp -d)
mount "$PART" "$M"
cp -a "$BOOTCODE" "$M/bootcode.bin"
# метка платы текстом
printf '%s\n' "$TAG" "netboot-stub" "TFTP=/srv/tftp/rpi" "arc2 dnsmasq rpi-netboot" >"$M/STUB.txt"
sync
umount "$M"
rmdir "$M"
echo "I: ok $DEV → $LABEL (только bootcode.bin)"
blkid "$PART"
