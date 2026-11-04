#!/usr/bin/env bash
# mk-netd-ramdisk.sh — как install-init: профиль ant → /init → rcS → /sbin/netd → cpio.gz
# НЕ пишет в живой $CWD/rootfs-* : только staging.
# Usage:
#   ARCH=aarch64 NETD=/path/to/netd-a64 CONSOLE=ttyMSM0 \
#     ./scripts/mk-netd-ramdisk.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CWD="$(dirname "$ROOT")"
ARCH="${ARCH:-aarch64}"
PROFILE="${PROFILE:-ant}"
CONSOLE="${CONSOLE:-ttyMSM0}"
SRC_ROOTFS="${SRC_ROOTFS:-$CWD/rootfs-$ARCH}"
NETD_BIN="${NETD:-$CWD/godminer/src/netd-a64}"
PROFILE_DIR="${PROFILE_DIR:-$ROOT/profiles/$PROFILE}"
STAGING="${STAGING:-/tmp/rootfs-${PROFILE}-${ARCH}-netd}"
OUT="${OUT:-$ROOT/out/ramdisk_${PROFILE}_${ARCH}_netd.cpio.gz}"

[[ -d "$SRC_ROOTFS" ]] || { echo "F: нет SRC_ROOTFS=$SRC_ROOTFS" >&2; exit 2; }
[[ -d "$PROFILE_DIR" ]] || { echo "F: нет профиля $PROFILE_DIR" >&2; exit 2; }
[[ -f "$NETD_BIN" ]] || { echo "F: нет NETD=$NETD_BIN (aarch64 musl)" >&2; exit 2; }

echo "I: staging $STAGING from $SRC_ROOTFS"
rm -rf "$STAGING"
mkdir -p "$STAGING"
# копия без чужих mountpoints
tar -C "$SRC_ROOTFS" -cf - . | tar -C "$STAGING" -xf -

echo "I: profile $PROFILE_DIR → staging"
cp -a "$PROFILE_DIR"/. "$STAGING"/

chmod +x "$STAGING/init" \
  "$STAGING/etc/init.d/rcS" \
  "$STAGING/etc/init.d/rcK"

# sbin/init → busybox
if [[ ! -e "$STAGING/sbin/init" && -x "$STAGING/bin/busybox" ]]; then
  mkdir -p "$STAGING/sbin"
  ln -sf /bin/busybox "$STAGING/sbin/init"
fi

# Alpine apk --no-script часто не ставит applet-symlinks (getty/hostname/…)
if [[ -x "$STAGING/bin/busybox" ]]; then
  echo "I: busybox --install -s (getty и пр.)"
  chroot "$STAGING" /bin/busybox --install -s 2>/dev/null \
    || "$STAGING/bin/busybox" --install -s "$STAGING" 2>/dev/null \
    || true
  # запас: явные ссылки, если install не сработал в chroot без qemu
  for pair in sbin/getty bin/hostname sbin/mdev bin/login; do
    dir=$(dirname "$pair"); base=$(basename "$pair")
    mkdir -p "$STAGING/$dir"
    if [[ ! -e "$STAGING/$pair" ]]; then
      ln -sf /bin/busybox "$STAGING/$pair"
    fi
  done
fi

# console под плату (ant: ttyPS0 → ${CONSOLE}, плюс /dev/console)
if [[ -f "$STAGING/etc/inittab" ]]; then
  sed -i \
    -e "s/ttyPS0/${CONSOLE}/g" \
    -e "s/^#ttyS0/${CONSOLE}/" \
    "$STAGING/etc/inittab" || true
  if ! grep -qE "^${CONSOLE}::" "$STAGING/etc/inittab"; then
    printf '%s::respawn:/sbin/getty -L 115200 %s vt100\n' "$CONSOLE" "$CONSOLE" \
      >>"$STAGING/etc/inittab"
  fi
  if ! grep -q 'getty -L 115200 console' "$STAGING/etc/inittab"; then
    printf '::respawn:/sbin/getty -L 115200 console vt100\n' >>"$STAGING/etc/inittab"
  fi
  # UART getty с ожиданием /dev/$CONSOLE
  if [[ -x "$STAGING/sbin/getty-uart" ]] && ! grep -q "getty-uart ${CONSOLE}" "$STAGING/etc/inittab"; then
    printf '%s::respawn:/sbin/getty-uart %s 115200\n' "$CONSOLE" "$CONSOLE" >>"$STAGING/etc/inittab"
  fi
  chmod +x "$STAGING/sbin/getty-uart" 2>/dev/null || true
fi

echo "I: install netd → /sbin/netd"
install -m 0755 "$NETD_BIN" "$STAGING/sbin/netd"

# config рядом с корнем (rcS может прикрыть /config через ubi — дубль в /root)
mkdir -p "$STAGING/root/config" "$STAGING/root/schema"
if [[ -d "$STAGING/config" ]]; then
  cp -a "$STAGING/config"/. "$STAGING/root/config/" 2>/dev/null || true
fi
if [[ -d "$STAGING/schema" ]]; then
  cp -a "$STAGING/schema"/. "$STAGING/root/schema/" 2>/dev/null || true
fi

# sanity
[[ -x "$STAGING/init" ]] || { echo "F: /init не исполняемый" >&2; exit 3; }
[[ -x "$STAGING/sbin/netd" ]] || { echo "F: /sbin/netd нет" >&2; exit 3; }
[[ -x "$STAGING/etc/init.d/rcS" ]] || { echo "F: rcS нет" >&2; exit 3; }

mkdir -p "$(dirname "$OUT")"
(
  cd "$STAGING"
  find . | cpio -o -H newc 2>/dev/null | gzip -9 >"$OUT"
)
echo "I: wrote $OUT ($(wc -c <"$OUT") bytes)"
echo "I: chain: kernel → /init → /sbin/init → rcS → /sbin/netd"
