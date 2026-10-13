#!/bin/sh
# Usage: ARCH=armv7 ROOTFS=rootfs-armv7 ./alpine-rootfs.sh alpine-base dropbear …
set -e

ARCH="${ARCH:-aarch64}"
VER="${VER:-3.21}"
ROOTFS="${ROOTFS:-rootfs-$ARCH}"
MIRROR="https://dl-cdn.alpinelinux.org/alpine/v${VER}/main"
APK="${APK:-./r3build/apk.static}"   # fetch-apk-static.sh, не из git

# rm -rf "$ROOTFS" || true
mkdir -p "$ROOTFS"

$APK --arch "$ARCH" --root "$ROOTFS" --initdb add 2>/dev/null || true

$APK --arch "$ARCH" --root "$ROOTFS" \
  -X "$MIRROR" --keys-dir "$ROOTFS/etc/apk/keys" --allow-untrusted \
  add alpine-keys

echo "$MIRROR" > "$ROOTFS/etc/apk/repositories"

# alpine-keys already in keys-dir. Further packages must verify.
$APK --arch "$ARCH" --root "$ROOTFS" \
  -X "$MIRROR" --keys-dir "$ROOTFS/etc/apk/keys" --no-script --no-cache \
  add "$@"

du -sh "$ROOTFS"