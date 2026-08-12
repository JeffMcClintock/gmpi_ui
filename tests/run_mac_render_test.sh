#!/bin/sh
# Build and run the macOS re-entrant-resize guard test (TIDE BACKLOG P7b).
#
# No CMake and no VST3: the whole test is this script, one .mm of harness and
# two backend sources. That is deliberate -- the defect is entirely inside
# gmpi_ui, so pulling in a plugin host to reach it would only add ways for the
# test to fail for unrelated reasons.
#
# THE DETECTOR IS GUARD MALLOC, AND ADDRESSSANITIZER IS NOT A SUBSTITUTE.
#
# The unguarded code reads a freed CGContext -- but it reads it from INSIDE
# CoreGraphics (CGContextRestoreGState), and ASan only checks loads the
# compiler instrumented plus the handful of functions it intercepts. Nothing in
# a system framework is either. Measured: an ASan build of this test reports a
# clean PASS on the unfixed sources, while the same build flags a hand-written
# read of the same freed pointer immediately. So an ASan-only run is a false
# negative machine here.
#
# libgmalloc puts every allocation on its own page and unmaps it on free, so
# the fault is a SIGSEGV taken inside CoreGraphics regardless of who compiled
# the reader. Unfixed sources: exit 139. Fixed: exit 0.
#
#   ./tests/run_mac_render_test.sh
#   GMPI_DIR=~/src/GMPI ./tests/run_mac_render_test.sh
#   GMALLOC=0 ./tests/run_mac_render_test.sh   # just watch it run
#   ASAN=1   ./tests/run_mac_render_test.sh    # see the paragraph above first
#
# Exit status is the test's own: 0 pass, 1 setup failure, 3 renderer never live.
# A regression dies on a signal (139 under Guard Malloc) instead of returning.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
GMPI=${GMPI_DIR:-$HOME/Documents/GitHub/GMPI}

if [ ! -f "$GMPI/Core/GmpiApiEditor.h" ]; then
    echo "no GMPI at $GMPI -- set GMPI_DIR" >&2
    exit 1
fi

OUT=${OUT_DIR:-$HERE/build-mac}
mkdir -p "$OUT"

SAN=
[ "${ASAN:-0}" = "1" ] && SAN="-fsanitize=address -fno-omit-frame-pointer"

# -fno-objc-arc: the backend's Objective-C is manually reference counted
# (MacColorDialog.h calls -retain), so ARC does not compile it.
CXXFLAGS="-std=c++20 -g -O0 -fno-objc-arc -w -I$ROOT -I$GMPI -I$GMPI/Core $SAN"

echo "building ..."
clang++ $CXXFLAGS \
    -o "$OUT/mac_render_reentrant_resize" \
    "$HERE/mac_render_reentrant_resize.mm" \
    "$ROOT/backends/DrawingFrameMac.mm" \
    "$ROOT/backends/DrawingFrameCommon.cpp" \
    -framework AppKit \
    -framework CoreGraphics \
    -framework CoreFoundation \
    -framework AudioUnit \
    -framework UniformTypeIdentifiers

echo "running (gmalloc=${GMALLOC:-1}, asan=${ASAN:-0}) ..."
if [ "${GMALLOC:-1}" = "1" ] && [ -f /usr/lib/libgmalloc.dylib ] && [ -z "$SAN" ]; then
    DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib "$OUT/mac_render_reentrant_resize"
else
    "$OUT/mac_render_reentrant_resize"
fi
