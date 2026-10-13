#!/bin/sh
# Скачать apk.static для хоста. Бинарник в git не класть.
# VER=3.21 HOST=x86_64 DEST=./apk.static ./fetch-apk-static.sh
set -eu

VER="${VER:-3.21}"
HOST="${HOST:-$(uname -m)}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
DEST="${DEST:-$SCRIPT_DIR/apk.static}"
MIRROR="https://dl-cdn.alpinelinux.org/alpine/v${VER}/main/${HOST}"

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

curl -fsSL "$MIRROR/APKINDEX.tar.gz" | tar -xz -C "$tmpdir" APKINDEX

pkgver=$(awk '
  $0 == "" {
    if (name == "apk-tools-static" && ver != "") { print ver; exit }
    name = ""; ver = ""
    next
  }
  index($0, "P:") == 1 { name = substr($0, 3) }
  index($0, "V:") == 1 { ver = substr($0, 3) }
' "$tmpdir/APKINDEX" | head -n 1 | tr -d "\r")

[ -n "$pkgver" ] || { echo "apk-tools-static not in $MIRROR" >&2; exit 1; }

echo "fetch apk-tools-static-$pkgver ($HOST, v$VER)"
curl -fsSL "$MIRROR/apk-tools-static-${pkgver}.apk" -o "$tmpdir/pkg.apk"
tar --warning=no-unknown-keyword -xzf "$tmpdir/pkg.apk" -C "$tmpdir" sbin/apk.static
install -m 0755 "$tmpdir/sbin/apk.static" "$DEST"
"$DEST" --version
