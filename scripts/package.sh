#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${1:-"$ROOT/WCM-Core-1.1.zip"}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/WCM-Core-1.1"
(
  cd "$ROOT"
  tar \
    --exclude='./build*' \
    --exclude='./.git' \
    --exclude='./WCM-Core-1.1.zip' \
    --exclude='./SOURCE_MANIFEST.sha256' \
    -cf - .
) | (cd "$TMP/WCM-Core-1.1" && tar -xf -)

(
  cd "$TMP/WCM-Core-1.1"
  find . -type f ! -name SOURCE_MANIFEST.sha256 -print0 | sort -z | xargs -0 sha256sum > SOURCE_MANIFEST.sha256
)
(
  cd "$TMP"
  zip -qr "$OUT" WCM-Core-1.1
)
printf '%s\n' "$OUT"
