#!/usr/bin/env bash
# mk-android-bootimg.sh — Android boot.img как у Мити (mkbootimg), под наш путь → cdba / netd
# Usage:
#   KERNEL=... DTB=... [RAMDISK=...] ./scripts/mk-android-bootimg.sh board/qcom/<board>.conf
#   ./scripts/mk-android-bootimg.sh board/qcom/<board>.conf --check
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONF="${1:-}"
CHECK_ONLY=0
[[ "${2:-}" == "--check" || "${1:-}" == "--check" ]] && CHECK_ONLY=1
if [[ "${1:-}" == "--check" ]]; then
  CONF="${2:-}"
fi
if [[ -z "${CONF}" || ! -f "${CONF}" ]]; then
  echo "F: нужен файл конфига: board/qcom/<board>.conf" >&2
  echo "Q: какая плата umbar (rb1-1, rb5-1, …)?" >&2
  exit 2
fi
# shellcheck disable=SC1090
source "${CONF}"

QUESTIONS=()
ask() { QUESTIONS+=("$*"); }

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    ask "нет команды '$1' на хосте — поставить пакет (Debian: mkbootimg и/или skales)?"
    return 1
  fi
  return 0
}

# [[lumag]]: Skales-mkbootimg or Android one
# v2 needs --header_version → Android mkbootimg; skales only for legacy
MKBOOTIMG="${MKBOOTIMG:-}"
if [[ -z "${MKBOOTIMG}" ]]; then
  if [[ "${MODE:-}" == "v2" ]] && command -v mkbootimg >/dev/null 2>&1; then
    MKBOOTIMG=mkbootimg
  elif command -v skales-mkbootimg >/dev/null 2>&1; then
    MKBOOTIMG=skales-mkbootimg
  elif command -v mkbootimg >/dev/null 2>&1; then
    MKBOOTIMG=mkbootimg
  else
    MKBOOTIMG=mkbootimg
  fi
fi

[[ -n "${BOARD_ID:-}" ]] || ask "BOARD_ID пуст в ${CONF}"
[[ -n "${MODE:-}" ]] || ask "MODE пуст (legacy|v2) в ${CONF}"
# ABOOT_BASE: [[george]] — не важен; пусто → 0x0
ABOOT_BASE="${ABOOT_BASE:-0x0}"
[[ -n "${ABOOT_PAGESIZE:-}" ]] || ask "ABOOT_PAGESIZE пуст"
[[ -n "${CMDLINE:-}" ]] || ask "CMDLINE пуст — что передать ядру/netd?"
[[ -n "${DTB_NAME:-}" || -n "${DTB:-}" ]] || ask "нет DTB_NAME/DTB — какой dtb для слота?"

KERNEL_IN="${KERNEL:-}"
DTB_IN="${DTB:-}"
RAMDISK_IN="${RAMDISK:-}"
OUT="${OUT:-${ROOT}/out/boot-${BOARD_ID:-unknown}.img}"

if [[ "${CHECK_ONLY}" -eq 0 ]]; then
  [[ -n "${KERNEL_IN}" && -f "${KERNEL_IN}" ]] || ask "KERNEL= путь к ${KERNEL_KIND:-ядру} не задан или файл нет: '${KERNEL_IN}'"
  if [[ -z "${DTB_IN}" && -n "${DTB_NAME:-}" ]]; then
    ask "DTB= не задан (ожидали файл для ${DTB_NAME})"
  fi
  [[ -n "${DTB_IN}" && -f "${DTB_IN}" ]] || ask "DTB= путь к dtb не задан или файл нет: '${DTB_IN}'"
fi

need_cmd "${MKBOOTIMG}" || true
echo "I: using MKBOOTIMG=${MKBOOTIMG}" >&2

if ((${#QUESTIONS[@]})); then
  echo "=== не собирается: вопросы ===" >&2
  i=1
  for q in "${QUESTIONS[@]}"; do
    echo "Q${i}: ${q}" >&2
    i=$((i + 1))
  done
  echo "=== конец вопросов ===" >&2
  exit 3
fi

if [[ "${CHECK_ONLY}" -eq 1 ]]; then
  echo "I: конфиг ${CONF} ок (BOARD_ID=${BOARD_ID} MODE=${MODE})"
  exit 0
fi

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/r3-aboot.XXXXXX")"
trap 'rm -rf "${WORKDIR}"' EXIT

EMPTY_RD="${WORKDIR}/empty-ramdisk"
if [[ -n "${RAMDISK_IN}" ]]; then
  RD="${RAMDISK_IN}"
else
  : >"${EMPTY_RD}"
  RD="${EMPTY_RD}"
fi

mkdir -p "$(dirname "${OUT}")"
case "${MODE}" in
  legacy)
    BLOB="${WORKDIR}/kernel-dtb"
    cat "${KERNEL_IN}" "${DTB_IN}" >"${BLOB}"
    echo "I: legacy cat kernel+dtb → mkbootimg"
    "${MKBOOTIMG}" \
      --base "${ABOOT_BASE}" \
      --pagesize "${ABOOT_PAGESIZE}" \
      --kernel "${BLOB}" \
      --ramdisk "${RD}" \
      --cmdline "${CMDLINE}" \
      --output "${OUT}"
    ;;
  v2)
    HV="${HEADER_VERSION:-2}"
    echo "I: v2 header=${HV} kernel + --dtb"
    "${MKBOOTIMG}" \
      --header_version "${HV}" \
      --base "${ABOOT_BASE}" \
      --pagesize "${ABOOT_PAGESIZE}" \
      --kernel_offset "${KERNEL_OFFSET:-0x00008000}" \
      --ramdisk_offset "${RAMDISK_OFFSET:-0x01000000}" \
      --tags_offset "${TAGS_OFFSET:-0x00000100}" \
      --dtb_offset "${DTB_OFFSET:-0x01f00000}" \
      --kernel "${KERNEL_IN}" \
      --dtb "${DTB_IN}" \
      --ramdisk "${RD}" \
      --cmdline "${CMDLINE}" \
      --output "${OUT}"
    ;;
  *)
    echo "F: MODE=${MODE} неизвестен (legacy|v2)" >&2
    exit 2
    ;;
esac

echo "I: wrote ${OUT}"
echo "I: дальше: cdba -b ${BOARD_ID} -h umbar.lab ${OUT}"
