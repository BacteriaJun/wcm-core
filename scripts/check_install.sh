#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PREFIX=$(mktemp -d)
CONSUMER=$(mktemp -d)
trap 'rm -rf "$PREFIX" "$CONSUMER"' EXIT

cmake --install "$ROOT/build-gcc-standard" --prefix "$PREFIX" >/dev/null
cat > "$CONSUMER/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.20)
project(wcm_consumer C)
find_package(WCMCore 1.1 REQUIRED)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE WCM::Core)
if(TARGET WCM::PortPOSIX)
  target_link_libraries(consumer PRIVATE WCM::PortPOSIX)
  target_compile_definitions(consumer PRIVATE HAVE_WCM_POSIX=1)
endif()
CMAKE
cat > "$CONSUMER/main.c" <<'C'
#include "wcm/wcm.h"
#ifdef HAVE_WCM_POSIX
#include "wcm/ports/posix.h"
#endif
#include "wcm/ports/baremetal.h"
int main(void) {
    wcm_build_info_t info;
    wcm_get_build_info(&info);
#ifdef HAVE_WCM_POSIX
    wcm_posix_port_t port;
    if (wcm_posix_port_init(&port) != WCM_OK) return 2;
    wcm_time_t now = 0u;
    if (wcm_posix_clock_read(NULL, &now) != WCM_OK) return 3;
    wcm_posix_port_destroy(&port);
#endif
    return (info.abi_version == WCM_ABI_VERSION && WCM_VERSION_MAJOR == 1 && WCM_VERSION_MINOR == 1) ? 0 : 1;
}
C
cmake -S "$CONSUMER" -B "$CONSUMER/build" -G Ninja -DCMAKE_PREFIX_PATH="$PREFIX" >/dev/null
cmake --build "$CONSUMER/build" >/dev/null
"$CONSUMER/build/consumer"

if command -v pkg-config >/dev/null 2>&1; then
  export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
  pkg-config --exists 'wcm-core >= 1.1'
  cat > "$CONSUMER/pkg_consumer.c" <<'C'
#include "wcm/wcm.h"
int main(void) {
    wcm_build_info_t info;
    wcm_get_build_info(&info);
    return info.abi_version == WCM_ABI_VERSION ? 0 : 1;
}
C
  cc "$CONSUMER/pkg_consumer.c" $(pkg-config --cflags --libs wcm-core) -o "$CONSUMER/pkg_consumer"
  "$CONSUMER/pkg_consumer"
fi

echo "install consumers: PASS"
