#!/usr/bin/env bash
# mk-net-init-ramdisk.sh — /init = net_init (r3build/src/init), не busybox/rcS
# Цепь: ядро → /init → mount/root= → (switch) → zcip → MCP :8443 → exec
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${NET_INIT_SRC:-$ROOT/src/init}"
CC="${CROSS_COMPILE:-/opt/x-aarch64/aarch64-linux-musl-cross/bin/aarch64-linux-musl-}gcc"
OUT_BIN="${OUT_BIN:-$ROOT/out/init-a64}"
OUT_RD="${OUT:-$ROOT/out/ramdisk_net_init_a64.cpio.gz}"
WORKDIR="${WORKDIR:-$ROOT/out/work/net-init-a64}"
mkdir -p "$WORKDIR" "$(dirname "$OUT_BIN")" "$ROOT/out"

[[ -f "$SRC/net_init.c" && -f "$SRC/zcip.c" ]] || {
  echo "F: нет $SRC/net_init.c — источники init только в r3build/src/init" >&2
  exit 2
}

cat >"$WORKDIR/main.c" <<EOF
#include <stdint.h>
uint64_t __rand_state = ~0x5EED064FUL;
int applet_init(int argc, char **argv);
int main(int argc, char **argv) { return applet_init(argc, argv); }
EOF

"$CC" -static -Os -Wall -I"$SRC" -o "$OUT_BIN" \
  "$WORKDIR/main.c" "$SRC/net_init.c" "$SRC/zcip.c"
file "$OUT_BIN"

RD="$WORKDIR/rd"; rm -rf "$RD"; mkdir -p "$RD"
install -m 0755 "$OUT_BIN" "$RD/init"
( cd "$RD" && find . | cpio -o -H newc | gzip -9 >"$OUT_RD" )
echo "I: $OUT_RD ($(wc -c <"$OUT_RD") bytes) — /init = net_init"
