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

CXXFLAGS="-std=c++20 -O2 -I$ROOT -I$GMPI -I$HERE/gen"
LIBS=$(pkg-config --cflags --libs wayland-client wayland-cursor xkbcommon libdecor-0 dbus-1)

# --demo-gpu builds the demo against the GPU backend (backends/GpuGfx.h over
# the renderer in backends/gpu). Point GPU_BUILD at a configured
# gimpi_ui_tests build with -DGMPI_UI_TESTS_GPU_BACKEND=ON: it holds
# libgmpi_gpu.a and the vendored bgfx's (libgmpi_bgfx.a, libgmpi_bimg.a, libgmpi_bx.a). Run with GMPI_UI_GPU=1 (and
# VK_ICD_FILENAMES for lavapipe on a GPU-less box).
if [ "${1:-}" = "--demo-gpu" ]; then
    GPU_BUILD=${GPU_BUILD:-$HOME/SE/build-uitests-gpu}
    TEXTLIBS=$(pkg-config --cflags --libs fontconfig harfbuzz freetype2)
    BGFX_SRC="$ROOT/backends/gpu/3rdparty/bgfx"  # vendored
    GPULIBS="$GPU_BUILD/libgmpi_gpu.a $GPU_BUILD/libgmpi_bgfx.a $GPU_BUILD/libgmpi_bimg.a $GPU_BUILD/libgmpi_bx.a \
        -lX11 -lXext -ldl -lrt -lpthread"
    g++ $CXXFLAGS -g -DNDEBUG -DGMPI_UI_GPU_BACKEND=1 -isystem "$BGFX_SRC/bgfx/include" -isystem "$BGFX_SRC/bx/include" \
        -o "$HERE/wayland_demo_gpu" "$HERE/wayland_demo.cpp" \
        "$HERE"/gen/*-protocol.o $LIBS $TEXTLIBS -lpng $GPULIBS
    echo "built $HERE/wayland_demo_gpu"
    exit 0
fi

# --demo builds the interactive app instead: the same backend, but driven by a
# real compositor so menus, grabs and dialogs are actually exercised.
if [ "${1:-}" = "--demo" ]; then
    # the demo draws text, so it needs the font stack the CPU backend leaves to
    # the app: fontconfig to find faces, freetype to rasterise, harfbuzz to shape
    TEXTLIBS=$(pkg-config --cflags --libs fontconfig harfbuzz freetype2)
    g++ $CXXFLAGS -o "$HERE/wayland_demo" "$HERE/wayland_demo.cpp" \
        "$HERE"/gen/*-protocol.o $LIBS $TEXTLIBS -lpng
    echo "built $HERE/wayland_demo"
    exit 0
fi

g++ $CXXFLAGS -o "$HERE/wayland_backend_test" \
    "$HERE/wayland_backend_test.cpp" "$HERE"/gen/*-protocol.o $LIBS -lpng
exec "$HERE/wayland_backend_test"
