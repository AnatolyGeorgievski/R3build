#!/bin/sh
# install-profile.sh — копирует файлы профиля в ROOTFS
# Usage: PROFILE=ant ARCH=armv7 ./install-profile.sh
# Usage: PROFILE=herd ARCH=armv7 ./install-profile.sh
# Usage: PROFILE=swarm ROOTFS=rootfs-armv7 ./install-profile.sh

# распаковка ramdisk
# gzip -dc ../ramdisk.cpio.gz | cpio -idmv
set -e

ARCH="${ARCH:-armv7}"
ROOTFS="${ROOTFS:-rootfs-$ARCH}"
PROFILE="${PROFILE:-ant}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BASE="${BASE:-$(dirname "$SCRIPT_DIR")}"   # parent of r3build/
SRC="${SRC:-$BASE/r3build/profiles/$PROFILE}"

[ -d "$ROOTFS" ] || { echo "no $ROOTFS"; exit 1; }
[ -d "$SRC" ]    || { echo "no profile $SRC"; exit 1; }

# дерево профиля, например:
# profiles/herd/init
# profiles/herd/etc/inittab
# profiles/herd/etc/fstab
# profiles/herd/etc/init.d/rcS
# profiles/herd/etc/init.d/rcK
# profiles/herd/etc/passwd
# profiles/herd/etc/shadow

cp -a "$SRC"/. "$ROOTFS"/

# права на точки входа
[ -f "$ROOTFS/init" ] && chmod +x "$ROOTFS/init"
[ -f "$ROOTFS/etc/init.d/rcS" ] && chmod +x "$ROOTFS/etc/init.d/rcS"
[ -f "$ROOTFS/etc/init.d/rcK" ] && chmod +x "$ROOTFS/etc/init.d/rcK"

# init → busybox, если нет
if [ ! -e "$ROOTFS/sbin/init" ] && [ -x "$ROOTFS/bin/busybox" ]; then
  mkdir -p "$ROOTFS/sbin"
  ln -sf /bin/busybox "$ROOTFS/sbin/init"
fi
# упаковка ramdisk
# gzip -dc ../ramdisk.cpio.gz | cpio -idmv
# mkimage -f fit618.its fitImage_new
cd $ROOTFS
find . | cpio -o -H newc | gzip -9 > ../ramdisk_${PROFILE}_${ARCH}.cpio.gz
cd ..
echo "OK $SRC → $ROOTFS"