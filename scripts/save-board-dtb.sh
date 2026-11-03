#!/usr/bin/env bash
# save-board-dtb.sh — dts/dtb → board/qcom/<BOARD_ID>/
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXTRA="${1:-}"
DEFAULTS=("$EXTRA" /tmp/r3-rb1-build/u-boot/dts/upstream/src/arm64/qcom /tmp/qcom-linux/arch/arm64/boot/dts/qcom "$ROOT/out/dtb")
MAP=(rb1-1:qrb2210-rb1 rb5-1:qrb5165-rb5 rb2-1:qrb4210-rb2 rb3gen2-1:qcs6490-rb3gen2 rb3-1:sdm845-db845c ifc6560-1:sda660-inforce-ifc6560 db410c-1:apq8016-sbc db820c-1:apq8096-db820c sm8150-hdk-1:sm8150-hdk sm8350-hdk-1:sm8350-hdk sm8450-hdk-1:sm8450-hdk openq835-1:msm8998-mtp sdm845-hdk-1:sdm845-mtp ifc6410-1:apq8064-ifc6410 db8074-1:apq8074-dragonboard rb5gen2-1:qcs8550-rb5gen2)
find_file(){ local base="$1" ext="$2" d f; for d in "${DEFAULTS[@]}"; do [[ -n "$d" && -d "$d" ]] || continue; f="$d/${base}.${ext}"; [[ -f "$f" ]] && { echo "$f"; return 0; }; f=$(find "$d" -maxdepth 1 -name "*${base}*.${ext}" 2>/dev/null | head -1 || true); [[ -n "$f" && -f "$f" ]] && { echo "$f"; return 0; }; done; return 1; }
for pair in "${MAP[@]}"; do
  board="${pair%%:*}"; base="${pair#*:}"; dest="$ROOT/board/qcom/$board"; mkdir -p "$dest"
  dts=$(find_file "$base" dts || true); dtb=$(find_file "$base" dtb || true)
  if [[ "$board" == rb5gen2-1 ]]; then case "${dtb:-}/${dts:-}" in *qrb2210-rb1*) dts=""; dtb="";; esac; fi
  {
    echo "BOARD_ID=$board"; echo "DTB_BASE=$base"; echo "origin=${EXTRA:-u-boot/linux qcom dts trees}"
    if [[ -n "${dts}" ]]; then cp -a "$dts" "$dest/$(basename "$dts")"; echo "dts: $dts"; fi
    if [[ -n "${dtb}" ]]; then cp -a "$dtb" "$dest/$(basename "$dtb")"; mkdir -p "$ROOT/out/dtb"; cp -a "$dtb" "$ROOT/out/dtb/$(basename "$dtb")"; echo "dtb: $dtb"; echo "STATUS=saved"; sha256sum "$dest/$(basename "$dtb")" | awk '{print "sha256_dtb="$1}'; 
    elif [[ -n "${dts}" ]]; then echo "STATUS=saved-dts-only — dtb собрать (dtc) или взять из build"
    else echo "STATUS=missing — положить ${base}.dts/.dtb в $dest"; echo "candidates=$base"; fi
  } >"$dest/SOURCE.txt"
  if [[ -n "${dtb}" ]]; then echo "I: $board saved"; elif [[ -n "${dts}" ]]; then echo "I: $board dts-only"; else echo "I: $board missing"; fi
done
python3 "$ROOT/scripts/refresh-dtb-index.py"
