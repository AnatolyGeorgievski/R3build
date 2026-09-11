#!/bin/sh
# Usage: ARCH=armv7 ROOTFS=rootfs-armv7 ./alpine-rootfs.sh alpine-base dropbear …
set -e

ARCH="${ARCH:-aarch64}"
VER="${VER:-3.21}"
ROOTFS="${ROOTFS:-rootfs-$ARCH}"
MIRROR="https://dl-cdn.alpinelinux.org/alpine/v${VER}/main"
APK="${APK:-./r3build/apk.static}"   # путь к уже лежащему apk.static

# rm -rf "$ROOTFS" || true
mkdir -p "$ROOTFS"

$APK --arch "$ARCH" --root "$ROOTFS" --initdb add 2>/dev/null || true

$APK --arch "$ARCH" --root "$ROOTFS" \
  -X "$MIRROR" --keys-dir "$ROOTFS/etc/apk/keys" --allow-untrusted \
  add alpine-keys

echo "$MIRROR" > "$ROOTFS/etc/apk/repositories"

$APK --arch "$ARCH" --root "$ROOTFS" \
  -X "$MIRROR" --keys-dir "$ROOTFS/etc/apk/keys" --allow-untrusted --no-script --no-cache \
  add "$@"

du -sh "$ROOTFS"