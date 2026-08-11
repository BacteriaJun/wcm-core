#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
if ! command -v gcc >/dev/null 2>&1; then
  echo "gcc not found; analyzer skipped"
  exit 0
fi
rm -rf build-analyzer
CC=gcc cmake -S . -B build-analyzer -G Ninja \
  -DWCM_BUILD_TESTS=OFF -DWCM_BUILD_REFERENCE=OFF -DWCM_BUILD_POSIX_PORT=OFF \
  -DCMAKE_C_FLAGS='-fanalyzer' >/dev/null
cmake --build build-analyzer
echo "gcc -fanalyzer: PASS"
