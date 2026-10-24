#!/bin/sh
# Builds tests/x11_menu_test twice - CPU path and GPU path (backends/GpuGfx.h
# over the renderer in backends/gpu) - runs each under Xvfb, right-clicks for
# the menu, and saves root screenshots as PNG for comparison. Needs a
# gimpi_ui_tests build made with -DGMPI_UI_TESTS_GPU_BACKEND=ON (GPU_BUILD):
# it holds libgmpi_gpu.a and the vendored bgfx's (libgmpi_bgfx.a, libgmpi_bimg.a, libgmpi_bx.a).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
GMPI=${GMPI_DIR:-$HOME/SE/GMPI}
TOOLS=${WLTEST_TOOLS:-$HOME/.cache/wayland-testtools}
GPU_BUILD=${GPU_BUILD:-$HOME/SE/build-uitests-gpu}
OUT=${WORKDIR:-$HERE/build-x11}
mkdir -p "$OUT"
[ -x "$TOOLS/usr/bin/Xvfb" ] || { echo "SKIP: no Xvfb in $TOOLS"; exit 77; }
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:$TOOLS/usr/lib/x86_64-linux-gnu
CXXFLAGS="-std=c++20 -O1 -g -DNDEBUG -I$ROOT -I$ROOT/helpers -I$GMPI -I$GMPI/Core"
LIBS=$(pkg-config --cflags --libs x11 xext fontconfig harfbuzz freetype2 dbus-1)
BGFX_SRC="$ROOT/backends/gpu/3rdparty/bgfx"  # vendored
GPULIBS="$GPU_BUILD/libgmpi_gpu.a $GPU_BUILD/libgmpi_bgfx.a $GPU_BUILD/libgmpi_bimg.a $GPU_BUILD/libgmpi_bx.a \
    -lX11 -lXext -ldl -lrt -lpthread"
echo "building cpu ..."
g++ $CXXFLAGS -o "$OUT/x11_menu_test_cpu" "$HERE/x11_menu_test.cpp" "$ROOT/backends/DrawingFrameX11.cpp" $LIBS -lpng || exit 1
echo "building gpu ..."
g++ $CXXFLAGS -DGMPI_UI_GPU_BACKEND=1 -isystem "$BGFX_SRC/bgfx/include" -isystem "$BGFX_SRC/bx/include" \
    -o "$OUT/x11_menu_test_gpu" "$HERE/x11_menu_test.cpp" "$ROOT/backends/DrawingFrameX11.cpp" $LIBS -lpng $GPULIBS || exit 1

DISP=""
for n in $(seq 70 88); do [ -e "/tmp/.X11-unix/X$n" ] || { DISP=":$n"; break; }; done
[ -n "$DISP" ] || { echo "FAIL: no free X display"; exit 1; }
"$TOOLS/usr/bin/Xvfb" "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XVFB=$!
trap 'kill $XVFB 2>/dev/null' EXIT
sleep 2
export DISPLAY=$DISP

for mode in cpu gpu; do
    if [ $mode = gpu ]; then export GMPI_UI_GPU=1 GMPI_UI_GPU_TRACE=1; else unset GMPI_UI_GPU GMPI_UI_GPU_TRACE; fi
    "$OUT/x11_menu_test_$mode" 400 >"$OUT/$mode.log" 2>&1 & TEST_PID=$!
    for _ in $(seq 1 40); do grep -q "frame open" "$OUT/$mode.log" 2>/dev/null && break; sleep 0.1; done
    sleep 1.5
    xwd -root -silent > "$OUT/$mode-plain.xwd" 2>/dev/null
    "$TOOLS/usr/bin/xdotool" mousemove 60 40 sleep 0.3 click 3 >/dev/null 2>&1
    sleep 1.2
    xwd -root -silent > "$OUT/$mode-menu.xwd" 2>/dev/null
    "$TOOLS/usr/bin/xdotool" key Escape >/dev/null 2>&1
    sleep 0.3
    kill $TEST_PID 2>/dev/null; wait $TEST_PID 2>/dev/null
    echo "$mode: $(grep -E 'GPU backend|frame open' "$OUT/$mode.log" | tr '\n' ' ')"
done
echo "screenshots in $OUT (xwd); convert with tests/xwd2png.py"
