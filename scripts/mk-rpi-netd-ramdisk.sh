#!/usr/bin/env bash
# mk-rpi-netd-ramdisk.sh — lean armhf ramdisk: busybox + netd-rpi --mcp :8443
# Prefers out/rpi/netd-rpi.armhf (board_rpi, GPIO_BANK_OFFS=0).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RF="${SRC_ROOTFS:-/home/george/rootfs-armhf}"
NETD="${NETD:-$ROOT/out/rpi/netd-rpi.armhf}"
OUT="${OUT:-$ROOT/out/rpi/ramdisk_netd_armhf.cpio.gz}"
RD="${WORKDIR:-$ROOT/out/work/rpi-netd}/rd"
MANIFEST="${MANIFEST:-$ROOT/board/rpi/mcp/mcp_netd.json}"
MCP_CONFIG="${MCP_CONFIG:-$ROOT/board/rpi/mcp/config.json}"

[[ -x "$NETD" ]] || { echo "F: нет $NETD — собери board_rpi netd" >&2; exit 2; }
[[ -x "$RF/bin/busybox" ]] || { echo "F: нет $RF" >&2; exit 2; }
[[ -f "$MANIFEST" ]] || { echo "F: нет $MANIFEST" >&2; exit 2; }
[[ -f "$MCP_CONFIG" ]] || { echo "F: нет $MCP_CONFIG" >&2; exit 2; }

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
install -m 0644 "$MCP_CONFIG" "$RD/config/config.json"

cat >"$RD/init" <<'INIT'
#!/bin/busybox sh
export PATH=/sbin:/bin
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mount -t devtmpfs devtmpfs /dev 2>/dev/null || /bin/busybox mount -t tmpfs tmpfs /dev
/bin/busybox mkdir -p /dev/pts /run /tmp /dev/shm
/bin/busybox mount -t devpts devpts /dev/pts 2>/dev/null || true
/bin/busybox mount -t tmpfs -o mode=0755 tmpfs /run 2>/dev/null || true
# UART first — /dev/console on Pi often = fb/tty1, not the cable
for c in /dev/ttyAMA0 /dev/ttyS0 /dev/console /dev/tty0; do
  [ -e "$c" ] || continue
  exec >"$c" 2>&1 <"$c" && break
done
echo "rpi-netd: init start"
/bin/busybox uname -a || true
echo "rpi-netd: nets=$(/bin/busybox ls /sys/class/net 2>/dev/null)"
# bring up every non-lo iface (name may be eth0/end0/enx…)
for c in $(/bin/busybox ls /sys/class/net 2>/dev/null); do
  [ "$c" = lo ] && continue
  echo "rpi-netd: link set $c up"
  /bin/busybox ip link set "$c" up 2>/dev/null || true
done
/bin/busybox ip link set lo up 2>/dev/null || true
IF=
i=0
while [ $i -lt 60 ]; do
  for c in $(/bin/busybox ls /sys/class/net 2>/dev/null); do
    [ "$c" = lo ] && continue
    /bin/busybox ip link set "$c" up 2>/dev/null || true
    st=$(/bin/busybox cat /sys/class/net/$c/operstate 2>/dev/null)
    echo "rpi-netd: wait $i $c operstate=$st"
    if echo "$st" | /bin/busybox grep -q up; then IF=$c; break; fi
  done
  [ -n "$IF" ] && break
  /bin/busybox sleep 1
  i=$((i+1))
done
if [ -z "$IF" ]; then
  for c in eth0 end0; do
    [ -e /sys/class/net/$c ] && IF=$c && break
  done
fi
[ -z "$IF" ] && IF=eth0
echo "rpi-netd: if=$IF"
echo "rpi-netd: operstate=$(/bin/busybox cat /sys/class/net/$IF/operstate 2>/dev/null)"
MAC=$(/bin/busybox cat /sys/class/net/$IF/address 2>/dev/null)
IP=192.168.1.89
case "$MAC" in
  b8:27:eb:b7:69:31) IP=192.168.1.82 ;;  # pi-shelf serial 2cb76931
  b8:27:eb:f2:2c:be) IP=192.168.1.83 ;;  # pi-net1 serial 36f22cbe
  b8:27:eb:24:c0:e8) IP=192.168.1.84 ;;  # pi-net2 serial 8c24c0e8
esac
echo "rpi-netd: mac=$MAC ip=$IP"
/bin/busybox ip addr flush dev "$IF" 2>/dev/null || true
/bin/busybox ip addr add "$IP/24" dev "$IF" 2>/dev/null || true
/bin/busybox ip route add default via 192.168.1.126 2>/dev/null || true
/bin/busybox ip link set "$IF" up 2>/dev/null || true
/bin/busybox ip addr || true
echo "rpi-netd: starting mcp on $IP:8443"
cd /
while true; do
  /sbin/netd --mcp --tcp 8443 -M /mcp_netd.json -r "$IF"
  echo "rpi-netd: netd exited rc=$? — retry in 2s"
  /bin/busybox sleep 2
done
INIT
chmod 755 "$RD/init"

mkdir -p "$(dirname "$OUT")"
( cd "$RD" && find . | cpio -o -H newc | gzip -9 >"$OUT" )
echo "I: $OUT ($(wc -c <"$OUT") bytes)"
