#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

if grep -R "wcm_runtime_commit_intent" include >/dev/null 2>&1; then
  echo "public commit API found" >&2
  exit 1
fi
if grep -R "WCM_WITNESS_PERSISTENT" include src >/dev/null 2>&1; then
  echo "persistent physical Witness support found" >&2
  exit 1
fi
if grep -R -nE 'TODO|FIXME' --exclude='audit.sh' --exclude-dir='.git' --exclude-dir='build*' . >/dev/null 2>&1; then
  echo "unfinished release marker found" >&2
  exit 1
fi
if find include src ports -type f \( -name '*fake_clock*' -o -name '*mock*' \) | grep . >/dev/null 2>&1; then
  echo "test double found in production surface" >&2
  exit 1
fi
if grep -R -nE '\b(malloc|calloc|realloc|free)[[:space:]]*\(' include src ports --include='*.c' --include='*.h' >/dev/null 2>&1; then
  echo "heap allocation call found in Core/port code" >&2
  exit 1
fi
for JSON in deploy/*.json; do
  python3 -m json.tool "$JSON" >/dev/null
done

echo "public API/release audit: PASS"
