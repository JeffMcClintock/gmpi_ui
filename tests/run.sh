#!/bin/sh
# Headless checks for the Wayland backend. No compositor, no display.
# Needs: libwayland-dev wayland-protocols libxkbcommon-dev libdecor-0-dev
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
GMPI=${GMPI_DIR:-$HOME/SE/GMPI}
D=$(pkg-config --variable=pkgdatadir wayland-protocols)

mkdir -p "$HERE/gen" && cd "$HERE/gen"
for p in stable/xdg-shell/xdg-shell staging/fractional-scale/fractional-scale-v1 \
         stable/viewporter/viewporter staging/cursor-shape/cursor-shape-v1 \
         unstable/tablet/tablet-unstable-v2 unstable/xdg-foreign/xdg-foreign-unstable-v2; do
    n=$(basename $p)
    [ -f "$n-client-protocol.h" ] || wayland-scanner client-header "$D/$p.xml" "$n-client-protocol.h"
    [ -f "$n-protocol.c" ]        || wayland-scanner private-code  "$D/$p.xml" "$n-protocol.c"
done
[ -f protocols.o ] || { gcc -c -O2 *.c && ld -r *.o -o protocols.o 2>/dev/null || true; }
cd "$HERE"

g++ -std=c++20 -O2 -I"$ROOT" -I"$GMPI" -I"$HERE/gen" -o "$HERE/wayland_backend_test" \
    "$HERE/wayland_backend_test.cpp" "$HERE"/gen/*-protocol.o \
    $(pkg-config --cflags --libs wayland-client xkbcommon libdecor-0) -lpng
exec "$HERE/wayland_backend_test"
