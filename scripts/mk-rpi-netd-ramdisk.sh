#!/usr/bin/env bash
# mk-rpi-netd-ramdisk.sh — lean armhf ramdisk: busybox + netd-rpi --mcp :8443
# Prefers out/rpi/netd-rpi.armhf (board_rpi, GPIO_BANK_OFFS=0).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RF="${SRC_ROOTFS:-/home/george/rootfs-armhf}"
NETD="${NETD:-$ROOT/out/rpi/netd-rpi.armhf}"
OUT="${OUT:-$ROOT/out/rpi/ramdisk_netd_armhf.cpio.gz}"
RD="${WORKDIR:-$ROOT/out/work/rpi-netd}/rd"
MANIFEST="${MANIFEST:-/home/george/godminer/src/mcp_netd.json}"

[[ -x "$NETD" ]] || { echo "F: нет $NETD — собери board_rpi netd" >&2; exit 2; }
[[ -x "$RF/bin/busybox" ]] || { echo "F: нет $RF" >&2; exit 2; }
[[ -f "$MANIFEST" ]] || { echo "F: нет $MANIFEST" >&2; exit 2; }

rm -rf "$RD"
mkdir -p "$RD"/{bin,sbin,lib,proc,sys,dev,run,tmp,etc,config,schema,root}
install -m 0755 "$RF/lib/ld-musl-armhf.so.1" "$RD/lib/"
ln -sf ld-musl-armhf.so.1 "$RD/lib/libc.so"
ln -sf ld-musl-armhf.so.1 "$RD/lib/libc.musl-armhf.so.1"
install -m 0755 "$RF/bin/busybox" "$RD/bin/busybox"
for a in sh mount umount mkdir ln ls cat echo sleep ip; do ln -sf busybox "$RD/bin/$a"; done
install -m 0755 "$NETD" "$RD/sbin/netd"
install -m 0644 "$MANIFEST" "$RD/mcp_netd.json"
install -m 0644 "$MANIFEST" "$RD/config/mcp.json"

cat >"$RD/init" <<'INIT'
#!/bin/busybox sh
export PATH=/sbin:/bin
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mount -t devtmpfs devtmpfs /dev 2>/dev/null || /bin/busybox mount -t tmpfs tmpfs /dev
/bin/busybox mkdir -p /dev/pts /run /tmp
/bin/busybox mount -t devpts devpts /dev/pts 2>/dev/null || true
/bin/busybox mount -t tmpfs -o mode=0755 tmpfs /run 2>/dev/null || true
exec >/dev/console 2>&1 </dev/console || true
echo "rpi-netd: init start"
IF=eth0
if [ ! -e /sys/class/net/$IF ]; then
  for c in eth0 end0; do
    [ -e /sys/class/net/$c ] && IF=$c && break
  done
fi
echo "rpi-netd: if=$IF"
/bin/busybox ip link set lo up 2>/dev/null || true
/bin/busybox ip link set "$IF" up 2>/dev/null || true
cd /
/sbin/netd zcip "$IF" || echo "rpi-netd: zcip soft-fail"
echo "rpi-netd: exec netd --mcp"
exec /sbin/netd --mcp --tcp 8443 -M /mcp_netd.json -r "$IF"
INIT
chmod 755 "$RD/init"

mkdir -p "$(dirname "$OUT")"
( cd "$RD" && find . | cpio -o -H newc | gzip -9 >"$OUT" )
echo "I: $OUT ($(wc -c <"$OUT") bytes)"
