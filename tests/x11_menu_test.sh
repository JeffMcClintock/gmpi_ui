#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Right-click -> context menu, on the X11 backend.
#
# Runs on a throwaway Xvfb. Checks three things, in increasing strength:
#
#   1. the frame asked the client for menu items  (the client counts the calls)
#   2. the screen CHANGED when the menu opened    (before/after diff)
#   3. clicking an item delivered its id          (the client records it)
#
# (1) alone would pass with a menu that never mapped, which is why (2) is here.
# ---------------------------------------------------------------------------
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
GMPI=${GMPI_DIR:-$HOME/SE/GMPI}
TOOLS=${WLTEST_TOOLS:-$HOME/.cache/wayland-testtools}
OUT=${WORKDIR:-$(mktemp -d)}
mkdir -p "$OUT"

[ -x "$TOOLS/usr/bin/Xvfb" ] || { echo "SKIP: no Xvfb in $TOOLS"; exit 77; }
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:$TOOLS/usr/lib/x86_64-linux-gnu

CXXFLAGS="-std=c++20 -O1 -I$ROOT -I$ROOT/helpers -I$GMPI -I$GMPI/Core"
LIBS=$(pkg-config --cflags --libs x11 xext fontconfig harfbuzz freetype2)

g++ $CXXFLAGS -o "$HERE/x11_menu_test" \
    "$HERE/x11_menu_test.cpp" "$ROOT/backends/DrawingFrameX11.cpp" $LIBS -lpng || exit 1

DISP=""
for n in $(seq 70 88); do
    [ -e "/tmp/.X11-unix/X$n" ] || { DISP=":$n"; break; }
done
[ -n "$DISP" ] || { echo "FAIL: no free X display"; exit 1; }

"$TOOLS/usr/bin/Xvfb" "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XVFB=$!
trap 'kill $XVFB 2>/dev/null' EXIT
sleep 2

DISPLAY=$DISP "$HERE/x11_menu_test" 6 >"$OUT/test.log" 2>&1 & TEST_PID=$!
for _ in $(seq 1 40); do grep -q "frame open" "$OUT/test.log" 2>/dev/null && break; sleep 0.1; done
sleep 0.7

export DISPLAY=$DISP
xwd -root -silent > "$OUT/before.xwd" 2>/dev/null

# Right-click inside the plugin window (which is at the root's top-left).
"$TOOLS/usr/bin/xdotool" mousemove 60 40 sleep 0.3 click 3 >/dev/null 2>&1
sleep 1.2
xwd -root -silent > "$OUT/menu.xwd" 2>/dev/null

# Click the FIRST item. The menu opens with its top-left at the click point,
# so the first row's middle is a little below and to the right.
"$TOOLS/usr/bin/xdotool" mousemove 100 58 sleep 0.3 click 1 >/dev/null 2>&1
sleep 0.8

wait $TEST_PID
RC=$?
cat "$OUT/test.log"

[ $RC -eq 77 ] && { echo "SKIP"; exit 77; }
[ $RC -eq 0 ] || { echo "FAIL: test exited $RC"; exit 1; }

python3 "$HERE/../../GMPI_Wrappers/tests/check_wayland_capture.py" \
    "$OUT/before.xwd" "$OUT/menu.xwd" 2>/dev/null \
  || python3 - "$OUT/before.xwd" "$OUT/menu.xwd" <<'PY' || exit 1
import struct, sys
def read(path):
    d = open(path,'rb').read()
    h = struct.unpack('>25I', d[:100])
    off = h[0] + h[19]*12
    bpp = h[11]//8
    return h[4], h[5], bpp, h[12], d, off
w,ht,bpp,bpl,d1,o1 = read(sys.argv[1])
_,_,_,_,d2,o2 = read(sys.argv[2])
changed = sum(1 for y in range(ht) for x in range(w)
              if d1[o1+y*bpl+x*bpp:o1+y*bpl+x*bpp+3] != d2[o2+y*bpl+x*bpp:o2+y*bpl+x*bpp+3])
print(f"pixels changed when the menu opened: {changed}")
if changed < 1000:
    print("FAIL: right-click changed nothing on screen - no menu was mapped")
    raise SystemExit(1)
PY

grep -q "^item chosen: 1$" "$OUT/test.log" \
  || { echo "FAIL: clicking the first item did not deliver its id (see test.log)"; exit 1; }

echo "PASS: menu populated, mapped, and returned the clicked item"
