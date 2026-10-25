#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# One modal dialog per window at a time, on the X11 backend (x11_modal_test.cpp).
#
# Needs a private X server, never the user's: Xvfb from $WLTEST_TOOLS if it is
# there, otherwise a rootless Xwayland on the running Wayland session started
# WITHOUT a window manager - its windows never get a shell role, so the
# compositor shows none of them. The test drives itself (XSendEvent), so no
# xdotool.
# ---------------------------------------------------------------------------
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
GMPI=${GMPI_DIR:-$HOME/SE/GMPI}
TOOLS=${WLTEST_TOOLS:-$HOME/.cache/wayland-testtools}

CXXFLAGS="-std=c++20 -O1 -I$ROOT -I$ROOT/helpers -I$GMPI -I$GMPI/Core"
LIBS=$(pkg-config --cflags --libs x11 xext fontconfig harfbuzz freetype2 dbus-1)

g++ $CXXFLAGS -o "$HERE/x11_modal_test" \
    "$HERE/x11_modal_test.cpp" "$ROOT/backends/DrawingFrameX11.cpp" $LIBS -lpng || exit 1

DISP=""
for n in $(seq 70 88); do
    [ -e "/tmp/.X11-unix/X$n" ] || { DISP=":$n"; break; }
done
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

DISPLAY=$DISP "$HERE/x11_modal_test"
