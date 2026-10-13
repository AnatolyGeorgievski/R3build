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
REPO="${SRC:-$BASE/r3build/repo/$ARCH}"

[ -d "$ROOTFS" ] || { echo "no $ROOTFS"; exit 1; }
[ -d "$SRC" ]    || { echo "no profile $SRC"; exit 1; }

# дерево профиля, например:
# profiles/herd/init
# profiles/herd/etc/inittab
# profiles/herd/etc/fstab
# profiles/herd/etc/init.d/rcS
# profiles/herd/etc/init.d/rcK
# profiles/herd/etc/motd
# profiles/herd/etc/passwd
# profiles/herd/etc/shadow

cp -a "$SRC"/. "$ROOTFS"/

# права на точки входа
[ -f "$ROOTFS/init" ] && chmod +x "$ROOTFS/init"
[ -f "$ROOTFS/etc/init.d/rcS" ] && chmod +x "$ROOTFS/etc/init.d/rcS"
[ -f "$ROOTFS/etc/init.d/rcK" ] && chmod +x "$ROOTFS/etc/init.d/rcK"
[ -f "$ROOTFS/sbin/netd" ] && chmod +x "$ROOTFS/sbin/netd"
[ -f "$ROOTFS/root/e9pro" ] && chmod +x "$ROOTFS/root/e9pro"

# init → busybox, если нет
if [ ! -e "$ROOTFS/sbin/init" ] && [ -x "$ROOTFS/bin/busybox" ]; then
  mkdir -p "$ROOTFS/sbin"
  ln -sf /bin/busybox "$ROOTFS/sbin/init"
fi

# netd в образе — бинарник этой платформы, не чужой загрузчик
case "$ARCH" in
  armv7) loader=ld-musl-armhf.so.1 ;;
  aarch64) loader=ld-musl-aarch64.so.1 ;;
  *) echo "ARCH $ARCH: no loader"; exit 1 ;;
esac
netd="$ROOTFS/sbin/netd"
[ -f "$netd" ] || { echo "no $netd"; exit 1; }
grep -a -F -q "/lib/$loader" "$netd" || { echo "netd is not for $ARCH"; exit 1; }
[ -e "$ROOTFS/lib/$loader" ] || { echo "no $ROOTFS/lib/$loader"; exit 1; }

# Функция инсталляции пакетов
REPO="${SRC:-$BASE/r3build/repo/$ARCH}"
r3_packages() {
  for pkg in "$@"; do
    local  archive=${REPO}/${pkg}.tar.gz
    [ -e ${archive} ] && tar -xzf ${archive} -C ${ROOTFS}/
  done
}

# установка прикладных программ r3-libs r3-mcp bmu
r3_packages r3-libs bmu

cd $ROOTFS
find . | cpio -o -H newc | gzip -9 > ../ramdisk_${PROFILE}_${ARCH}.cpio.gz
cd ..
# упаковка ramdisk
# mkimage -f fit618.its fitImage_new
echo "OK $SRC → $ROOTFS"