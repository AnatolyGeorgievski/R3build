#!/usr/bin/env bash
# mk-qcom-alpine-boot.sh — как Zynq/ant, но aarch64 + ttyMSM0 → Android boot.img для umbar
#
# Цепь (аналог ARCH=armv7 alpine-rootfs + install-init):
#   Alpine minirootfs aarch64 → profile ant + netd → ramdisk → mkbootimg
# Без stay: BusyBox init → getty ttyMSM0 → rcS → netd
#
# Usage:
#   ./scripts/mk-qcom-alpine-boot.sh              # rootfs+ramdisk+boot rb3-1
#   ./scripts/mk-qcom-alpine-boot.sh rb5-1
#   SKIP_ROOTFS=1 ./scripts/mk-qcom-alpine-boot.sh ifc6560-1
#   ONLY=ramdisk ./scripts/mk-qcom-alpine-boot.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CWD="$(dirname "$ROOT")"
BOARD_ID="${1:-rb3-1}"
ONLY="${ONLY:-all}"          # all | rootfs | ramdisk | boot
SKIP_ROOTFS="${SKIP_ROOTFS:-0}"
ARCH=aarch64
CONSOLE="${CONSOLE:-ttyMSM0}"
ROOTFS="${ROOTFS:-$CWD/rootfs-$ARCH}"
APK="${APK:-$ROOT/apk.static}"
NETD_BIN="${NETD:-$CWD/godminer/src/netd-a64}"
RAMDISK_OUT="${RAMDISK_OUT:-$ROOT/out/ramdisk_ant_${ARCH}_netd.cpio.gz}"
KERNEL_GZ="${KERNEL_GZ:-$ROOT/out/kernel/Image-qcom.gz}"
KERNEL_IMG="${KERNEL_IMG:-$ROOT/out/kernel/Image-qcom}"
CMDLINE_ALPINE="${CMDLINE_ALPINE:-console=${CONSOLE},115200n8 clk_ignore_unused ignore_loglevel rdinit=/init root=/dev/ram0 rw}"

CONF="$ROOT/board/qcom/${BOARD_ID}.conf"
[[ -f "$CONF" ]] || { echo "F: нет $CONF — слот umbar?" >&2; exit 2; }

# shellcheck disable=SC1090
source "$CONF"
OUT_BOOT="${OUT:-$ROOT/out/boot-${BOARD_ID}-alpine.img}"

do_rootfs() {
  if [[ "$SKIP_ROOTFS" == 1 && -x "$ROOTFS/bin/busybox" ]]; then
    echo "I: SKIP_ROOTFS=1 · используем $ROOTFS"
    return 0
  fi
  [[ -x "$APK" ]] || { echo "F: нет apk.static ($APK)" >&2; exit 2; }
  echo "I: Alpine minirootfs → $ROOTFS (чистый каталог)"
  rm -rf "$ROOTFS"
  mkdir -p "$ROOTFS"
  (
    cd "$CWD"
    ARCH="$ARCH" VER="${VER:-3.24}" ROOTFS="$ROOTFS" APK="$APK" \
      "$ROOT/alpine-rootfs.sh" \
        alpine-base busybox dropbear dropbear-scp ca-certificates \
        iproute2 iputils
  )
}

do_ramdisk() {
  [[ -d "$ROOTFS" ]] || { echo "F: нет $ROOTFS — сначала rootfs" >&2; exit 2; }
  [[ -f "$NETD_BIN" ]] || { echo "F: нет NETD=$NETD_BIN" >&2; exit 2; }
  echo "I: ramdisk ant+netd console=$CONSOLE"
  ARCH="$ARCH" PROFILE=ant CONSOLE="$CONSOLE" \
    SRC_ROOTFS="$ROOTFS" NETD="$NETD_BIN" OUT="$RAMDISK_OUT" \
    "$ROOT/scripts/mk-netd-ramdisk.sh"
}

do_boot() {
  [[ -f "$RAMDISK_OUT" ]] || { echo "F: нет $RAMDISK_OUT" >&2; exit 2; }
  base_dtb="$(basename "${DTB_NAME}")"
  DTB_PATH="${DTB:-$ROOT/board/qcom/${BOARD_ID}/${base_dtb}}"
  [[ -f "$DTB_PATH" ]] || { echo "F: нет DTB=$DTB_PATH" >&2; exit 2; }

  if [[ "${KERNEL_KIND}" == "Image" ]]; then
    KER="${KERNEL:-$KERNEL_IMG}"
  else
    KER="${KERNEL:-$KERNEL_GZ}"
  fi
  [[ -f "$KER" ]] || { echo "F: нет KERNEL=$KER (собери CFG-64 Image)" >&2; exit 2; }

  echo "I: boot.img $BOARD_ID MODE=$MODE → $OUT_BOOT"
  echo "I: cmdline (alpine, no stay): $CMDLINE_ALPINE"
  CMDLINE_OVERRIDE="$CMDLINE_ALPINE" \
    KERNEL="$KER" DTB="$DTB_PATH" RAMDISK="$RAMDISK_OUT" OUT="$OUT_BOOT" \
    "$ROOT/scripts/mk-android-bootimg.sh" "$CONF"
  echo "I: cdba -b ${BOARD_ID} -h umbar.lab ${OUT_BOOT}"
}

case "$ONLY" in
  rootfs)  do_rootfs ;;
  ramdisk) do_ramdisk ;;
  boot)    do_boot ;;
  all)
    do_rootfs
    do_ramdisk
    do_boot
    ;;
  *) echo "F: ONLY=$ONLY (all|rootfs|ramdisk|boot)" >&2; exit 2 ;;
esac
