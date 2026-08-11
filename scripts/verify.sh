#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

./scripts/audit.sh

for JSON in deploy/*.json; do
  python3 -m json.tool "$JSON" >/dev/null
done
if python3 -c 'import jsonschema' >/dev/null 2>&1; then
  ./scripts/validate_deploy_records.py
fi

for CC_NAME in gcc clang; do
  if ! command -v "$CC_NAME" >/dev/null 2>&1; then
    continue
  fi
  for PROFILE in TINY STANDARD EXTENDED; do
    B="build-${CC_NAME}-$(printf '%s' "$PROFILE" | tr '[:upper:]' '[:lower:]')"
    rm -rf "$B"
    CC="$CC_NAME" cmake -S . -B "$B" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DWCM_PROFILE="$PROFILE"
    cmake --build "$B"
    ctest --test-dir "$B" --output-on-failure
    "$B/wcm_size_report"
  done
done

rm -rf build-sanitize
cmake -S . -B build-sanitize -G Ninja -DWCM_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure

rm -rf build-tsan
cmake -S . -B build-tsan -G Ninja -DWCM_ENABLE_TSAN=ON -DWCM_BUILD_REFERENCE=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure

rm -rf build-hardening
cmake -S . -B build-hardening -G Ninja -DCMAKE_BUILD_TYPE=Release -DWCM_ENABLE_HARDENING=ON
cmake --build build-hardening
ctest --test-dir build-hardening --output-on-failure

./scripts/analyze.sh
./scripts/check_install.sh
