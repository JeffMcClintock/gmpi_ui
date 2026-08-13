#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build and run the X11 re-entrant-resize guard test (TIDE BACKLOG P7c).
#
# No CMake and no VST3: one .cpp of harness and one backend source. The defect
# is entirely inside gmpi_ui, so pulling a plugin host in to reach it would only
# add ways for the test to fail for unrelated reasons. Same shape as
# run_mac_render_test.sh, which does this for the macOS half (P7b).
#
# THE DETECTOR IS THE ALLOCATION ITSELF, AND ASAN IS NOT A SUBSTITUTE.
#
# On a local display the image is MIT-SHM, so image->data is a shmat() mapping
# of its own and the overflow walks off the end of it into unmapped address
# space: SIGSEGV, no sanitizer required. AddressSanitizer does not instrument a
# shm segment, so ASAN=1 on this path is a false-negative machine - it is
# offered only for the XCreateImage fallback (a remote display), where the
# buffer is malloc'd and ASan does cover it.
#
# Measured on the unfixed backend: SIGSEGV in encodeDirtyRect (CpuEncode.h:269)
# with clip bounds right=800/bottom=600 against a 64x48 image, 3/3. Fixed: exit
# 0, 3/3.
#
#   ./tests/run_x11_present_reentrant_test.sh
#   GMPI_DIR=~/src/GMPI ./tests/run_x11_present_reentrant_test.sh
#   ASAN=1 ./tests/run_x11_present_reentrant_test.sh   # see the paragraph above
#   REPEAT=3 ./tests/run_x11_present_reentrant_test.sh
#
# Exit status: 0 pass, 1 setup/build failure, 3 the test could not build the
# scenario it claims to test, 77 skip (no Xvfb). A regression dies on a signal
# (139) rather than returning.
# ---------------------------------------------------------------------------
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
GMPI=${GMPI_DIR:-$HOME/SE/GMPI}
TOOLS=${WLTEST_TOOLS:-$HOME/.cache/wayland-testtools}
REPEAT=${REPEAT:-1}

[ -f "$GMPI/Core/GmpiApiEditor.h" ] || { echo "no GMPI at $GMPI -- set GMPI_DIR" >&2; exit 1; }
[ -x "$TOOLS/usr/bin/Xvfb" ] || { echo "SKIP: no Xvfb in $TOOLS"; exit 77; }

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:$TOOLS/usr/lib/x86_64-linux-gnu

OUT=${OUT_DIR:-$HERE/build-x11}
mkdir -p "$OUT"

SAN=
[ "${ASAN:-0}" = "1" ] && SAN="-fsanitize=address -fno-omit-frame-pointer"

CXXFLAGS="-std=c++20 -g -O0 -w -I$ROOT -I$ROOT/helpers -I$GMPI -I$GMPI/Core $SAN"
LIBS=$(pkg-config --cflags --libs x11 xext fontconfig harfbuzz freetype2 dbus-1)

echo "building ..."
g++ $CXXFLAGS -o "$OUT/x11_present_reentrant_resize" \
    "$HERE/x11_present_reentrant_resize.cpp" \
    "$ROOT/backends/DrawingFrameX11.cpp" \
    $LIBS -lpng || exit 1

# A private Xvfb rather than the developer's session: the test resizes a window
# and blits to it a few hundred times, and it should not do that on a screen
# somebody is using.
DISP=""
for n in $(seq 70 88); do
    [ -e "/tmp/.X11-unix/X$n" ] || { DISP=":$n"; break; }
done
[ -n "$DISP" ] || { echo "FAIL: no free X display"; exit 1; }

"$TOOLS/usr/bin/Xvfb" "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XVFB=$!
trap 'kill $XVFB 2>/dev/null' EXIT
sleep 2

echo "running (asan=${ASAN:-0}, repeat=$REPEAT) on $DISP ..."
FAILED=0
for i in $(seq 1 "$REPEAT"); do
    DISPLAY=$DISP "$OUT/x11_present_reentrant_resize"
    RC=$?
    echo "run $i: exit $RC"
    [ $RC -eq 77 ] && exit 77
    [ $RC -ne 0 ] && FAILED=1
done

[ $FAILED -eq 0 ] || { echo "FAIL: see the exit codes above (139 = the overflow is back)"; exit 1; }
echo "PASS: $REPEAT/$REPEAT"
