#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# The X11 frame's present, CPU and GPU (x11_present_test.cpp): each build
# draws the same picture into its window, reads it back, checks the flat
# colours, and the two readbacks are compared pixel by pixel.
#
# A private X server, never the user's: Xvfb from $WLTEST_TOOLS, else a
# rootless Xwayland with no window manager (its windows never reach the
# screen). The GPU build needs a gimpi_ui_tests build made with
# -DGMPI_UI_TESTS_GPU_BACKEND=ON (GPU_BUILD) for libgmpi_gpu.a and the
# vendored bgfx; without one, only the CPU build runs.
# ---------------------------------------------------------------------------
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
GMPI=${GMPI_DIR:-$HOME/SE/GMPI}
TOOLS=${WLTEST_TOOLS:-$HOME/.cache/wayland-testtools}
GPU_BUILD=${GPU_BUILD:-$HOME/SE/build-uitests-gpu}
OUT=${WORKDIR:-$(mktemp -d)}
mkdir -p "$OUT"

CXXFLAGS="-std=c++20 -O1 -g -DNDEBUG -I$ROOT -I$ROOT/helpers -I$GMPI -I$GMPI/Core"
LIBS=$(pkg-config --cflags --libs x11 xext fontconfig harfbuzz freetype2 dbus-1)
BGFX_SRC="$ROOT/backends/gpu/3rdparty/bgfx"

g++ $CXXFLAGS -o "$OUT/x11_present_test_cpu" "$HERE/x11_present_test.cpp" "$ROOT/backends/DrawingFrameX11.cpp" \
    $LIBS -lpng || exit 1
MODES=cpu
if [ -f "$GPU_BUILD/libgmpi_gpu.a" ]; then
    GPULIBS="$GPU_BUILD/libgmpi_gpu.a $GPU_BUILD/libgmpi_bgfx.a $GPU_BUILD/libgmpi_bimg.a $GPU_BUILD/libgmpi_bx.a \
        -lX11 -lXext -lwayland-client -ldl -lrt -lpthread"
    g++ $CXXFLAGS -DGMPI_UI_GPU_BACKEND=1 -isystem "$BGFX_SRC/bgfx/include" -isystem "$BGFX_SRC/bx/include" \
        -o "$OUT/x11_present_test_gpu" "$HERE/x11_present_test.cpp" "$ROOT/backends/DrawingFrameX11.cpp" \
        $LIBS -lpng $GPULIBS || exit 1
    MODES="cpu gpu"
else
    echo "note: no GPU build at $GPU_BUILD; CPU present only"
fi

DISP=""
for n in $(seq 70 88); do [ -e "/tmp/.X11-unix/X$n" ] || { DISP=":$n"; break; }; done
[ -n "$DISP" ] || { echo "FAIL: no free X display"; exit 1; }
if [ -x "$TOOLS/usr/bin/Xvfb" ]; then
    export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:$TOOLS/usr/lib/x86_64-linux-gnu
    "$TOOLS/usr/bin/Xvfb" "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XSERVER=$!
elif command -v Xwayland >/dev/null && [ -n "${WAYLAND_DISPLAY:-}" ]; then
    Xwayland "$DISP" -rootless -noreset >/dev/null 2>&1 & XSERVER=$!
else
    echo "SKIP: neither Xvfb nor Xwayland on a Wayland session"; exit 77
fi
trap 'kill $XSERVER 2>/dev/null' EXIT
for _ in $(seq 1 50); do [ -e "/tmp/.X11-unix/X${DISP#:}" ] && break; sleep 0.1; done
sleep 0.5
export DISPLAY=$DISP

RC=0
rm -f "$OUT/cpu.ppm" "$OUT/gpu.ppm"   # written only where the server reads back
for mode in $MODES; do
    echo "--- $mode"
    if [ $mode = gpu ]; then export GMPI_UI_GPU=1 GMPI_UI_GPU_TRACE=1; else unset GMPI_UI_GPU GMPI_UI_GPU_TRACE; fi
    "$OUT/x11_present_test_$mode" "$OUT/$mode.ppm" >"$OUT/$mode.log" 2>&1 || RC=1
    grep -v "^\[gpugfx\]" "$OUT/$mode.log"
    # The GPU build must really have presented through the GPU, not fallen
    # back to the CPU path.
    if [ $mode = gpu ]; then
        grep -q "^\[presentGpu x11\] render" "$OUT/$mode.log" \
            && echo "ok  : presented through the GPU ($(grep -m1 '^\[presentGpu x11\] render' "$OUT/$mode.log"))" \
            || { echo "FAIL: the GPU build never presented through the GPU"; RC=1; }
    fi
done

if [ "$MODES" = "cpu gpu" ] && [ -f "$OUT/cpu.ppm" ] && [ -f "$OUT/gpu.ppm" ]; then
    python3 - "$OUT/cpu.ppm" "$OUT/gpu.ppm" <<'PY' || RC=1
import sys
def read(p):
    d = open(p, 'rb').read()
    return d[d.index(b'255\n') + 4:]
a, b = read(sys.argv[1]), read(sys.argv[2])
worst = max(abs(x - y) for x, y in zip(a, b))
off = sum(1 for i in range(0, len(a), 3) if max(abs(a[i+k] - b[i+k]) for k in range(3)) > 3) if a else 0
print(f"cpu vs gpu: worst channel difference {worst}, pixels off by more than 3: {off}")
raise SystemExit(0 if off <= 40 else 1)
PY
fi
[ $RC -eq 0 ] && echo "PASS" || echo "FAIL"
exit $RC
