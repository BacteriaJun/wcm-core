#!/usr/bin/env sh
set -eu
if [ "$#" -ne 1 ]; then
  echo "usage: $0 WCM-Core-1.1.zip" >&2
  exit 2
fi
ARCHIVE=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
unzip -q "$ARCHIVE" -d "$TMP"
ROOT="$TMP/WCM-Core-1.1"
[ -d "$ROOT" ]
(
  cd "$ROOT"
  sha256sum -c SOURCE_MANIFEST.sha256 >/dev/null
  if find . -type f \( -name '*.o' -o -name '*.a' -o -name '*.so' -o -name '*.dylib' -o -name '*.exe' \) | grep .; then
    echo "compiled artifact found in source archive" >&2
    exit 1
  fi
  ./scripts/audit.sh
  for JSON in deploy/*.json; do python3 -m json.tool "$JSON" >/dev/null; done
  cmake -S . -B build-gcc-standard -G Ninja -DCMAKE_BUILD_TYPE=Release -DWCM_PROFILE=STANDARD >/dev/null
  cmake --build build-gcc-standard >/dev/null
  ctest --test-dir build-gcc-standard --output-on-failure
  ./build-gcc-standard/wcm_size_report
  ./scripts/check_install.sh
)
echo "archive verification: PASS"
