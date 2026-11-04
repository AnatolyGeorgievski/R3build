#!/usr/bin/env bash
# mk-qcom-boot-family.sh — упаковка boot.img по матрице Мити (build.sh), пути r3build
# Не копирует ~/Projects/RPB: RAMDISK/KERNEL/DTB снаружи или из out/ + board/qcom.
#
# Usage:
#   KERNEL=out/kernel/Image-qcom.gz RAMDISK=out/ramdisk_net_init_a64.cpio.gz \
#     ./scripts/mk-qcom-boot-family.sh rb1
#   KERNEL_IMAGE=…/Image ./scripts/mk-qcom-boot-family.sh rb5   # v2 nogz
#   ./scripts/mk-qcom-boot-family.sh --list
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MK="$ROOT/scripts/mk-android-bootimg.sh"

list() {
  cat <<EOF
Слоты (conf → рецепт Мити):
  rb1      board/qcom/rb1.conf       legacy Image.gz+dtb   pagesize 2048
  rb2      board/qcom/rb2.conf       legacy Image.gz+dtb
  rb5      board/qcom/rb5.conf       v2 Image (nogz) +dtb
  rb3gen2  board/qcom/rb3gen2.conf   v2 Image (nogz) +dtb

Канон .config: board/qcom/kernel/CFG-64
Исходный build.sh: inbox/mitya-2026-11-04/build.sh
EOF
}

[[ "${1:-}" == "--list" || -z "${1:-}" ]] && { list; [[ -n "${1:-}" ]] || exit 0; exit 0; }

BOARD="$1"
CONF="$ROOT/board/qcom/${BOARD}.conf"
[[ -f "$CONF" ]] || { echo "F: нет $CONF — --list" >&2; exit 2; }

# shellcheck disable=SC1090
source "$CONF"
DTB_DEFAULT="$ROOT/board/qcom/${BOARD_ID}/${DTB_NAME##*/}"
# DTB_NAME is qcom/foo.dtb — file is board/qcom/<id>/foo.dtb
base_dtb="$(basename "${DTB_NAME}")"
DTB_DEFAULT="$ROOT/board/qcom/${BOARD_ID}/${base_dtb}"

export DTB="${DTB:-$DTB_DEFAULT}"
export RAMDISK="${RAMDISK:-$ROOT/out/ramdisk_net_init_a64.cpio.gz}"
export OUT="${OUT:-$ROOT/out/boot-${BOARD_ID}.img}"

if [[ "${KERNEL_KIND}" == "Image" ]]; then
  export KERNEL="${KERNEL:-${KERNEL_IMAGE:-}}"
  [[ -n "${KERNEL}" ]] || {
    echo "F: для ${BOARD} нужен KERNEL= или KERNEL_IMAGE= (Image, не gz — как nogz у Мити)" >&2
    exit 2
  }
else
  export KERNEL="${KERNEL:-$ROOT/out/kernel/Image-qcom.gz}"
fi

[[ -f "$DTB" ]] || { echo "F: нет DTB=$DTB" >&2; exit 2; }
[[ -f "$KERNEL" ]] || { echo "F: нет KERNEL=$KERNEL" >&2; exit 2; }
[[ -f "$RAMDISK" ]] || echo "W: нет RAMDISK=$RAMDISK — mk-android сделает empty" >&2

echo "I: ${BOARD} MODE=${MODE} PAGESIZE=${ABOOT_PAGESIZE} KERNEL=$(basename "$KERNEL")"
exec "$MK" "$CONF"
