#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Does hovering the Wayland backend put a tooltip on screen?
#
#   Xvfb :N  ->  gnome-shell --nested (real mutter)  ->  wayland_demo
#
# A nested compositor, never the user's session. Build the demo first:
#
#   ./run.sh --demo
#
# The demo's getToolTip returns "hovering <x>,<y>", so the capture also shows
# the frame passed the pointer position through rather than something stale.
#
# Checked by diffing a capture taken before the hover delay expires against one
# after: the tooltip is an xdg_popup on the compositor, so nothing in this
# process can see its pixels.
# ---------------------------------------------------------------------------
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
TOOLS=${WLTEST_TOOLS:-$HOME/.cache/wayland-testtools}
OUT=${WORKDIR:-$(mktemp -d)}
mkdir -p "$OUT"

[ -x "$HERE/wayland_demo" ] || { echo "SKIP: build it first with ./run.sh --demo"; exit 77; }
[ -x "$TOOLS/usr/bin/Xvfb" ] || { echo "SKIP: no Xvfb in $TOOLS"; exit 77; }
command -v gnome-shell >/dev/null || { echo "SKIP: no gnome-shell"; exit 77; }

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:$TOOLS/usr/lib/x86_64-linux-gnu

DISP=""
for n in $(seq 50 68); do
    [ -e "/tmp/.X11-unix/X$n" ] || { DISP=":$n"; break; }
done
[ -n "$DISP" ] || { echo "FAIL: no free X display"; exit 1; }
SOCK=gmpi-wltip-$$

XVFB=""; NEST_PGID=""
cleanup() {
    # Process GROUP: a nested shell spawns children of its own.
    [ -n "$NEST_PGID" ] && kill -- -"$NEST_PGID" 2>/dev/null
    [ -n "$XVFB" ] && kill "$XVFB" 2>/dev/null
}
trap cleanup EXIT

"$TOOLS/usr/bin/Xvfb" "$DISP" -screen 0 1400x900x24 >/dev/null 2>&1 & XVFB=$!
sleep 2
setsid env -u WAYLAND_DISPLAY DISPLAY="$DISP" GNOME_SHELL_DISABLE_EXTENSIONS=1 dbus-run-session -- \
    gnome-shell --nested --wayland --wayland-display="$SOCK" >"$OUT/compositor.log" 2>&1 & NEST=$!
NEST_PGID=$(ps -o pgid= -p "$NEST" 2>/dev/null | tr -d " ")

for _ in $(seq 1 80); do [ -S "$XDG_RUNTIME_DIR/$SOCK" ] && break; sleep 0.5; done
[ -S "$XDG_RUNTIME_DIR/$SOCK" ] || { echo "FAIL: nested compositor never came up"; exit 1; }
sleep 3
DISPLAY=$DISP "$TOOLS/usr/bin/xdotool" key Escape >/dev/null 2>&1
sleep 1

env WAYLAND_DISPLAY="$SOCK" "$HERE/wayland_demo" >"$OUT/demo.log" 2>&1 & DEMO=$!
sleep 4

export DISPLAY=$DISP

# Nested mutter never sees an absolute pointer warp - it tracks the pointer from
# device motion - so travel there relatively from a known corner.
home() {
    "$TOOLS/usr/bin/xdotool" mousemove 1399 899 >/dev/null 2>&1
    "$TOOLS/usr/bin/xdotool" mousemove_relative -- -4000 -4000 >/dev/null 2>&1
}
home
"$TOOLS/usr/bin/xdotool" mousemove_relative -- 500 400 >/dev/null 2>&1
sleep 0.4
xwd -root -silent > "$OUT/before.xwd" 2>/dev/null

# One pixel, so a motion event fires and arms the timer, then hold still past
# the 650ms delay.
"$TOOLS/usr/bin/xdotool" mousemove_relative -- 1 0 >/dev/null 2>&1
sleep 2.0
xwd -root -silent > "$OUT/hover.xwd" 2>/dev/null

kill $DEMO 2>/dev/null
wait $DEMO 2>/dev/null

python3 "$HERE/../../GMPI_Wrappers/tests/check_wayland_capture.py" \
    "$OUT/before.xwd" "$OUT/hover.xwd" \
  || { echo "FAIL: nothing appeared on hover - no tooltip"; exit 1; }

echo "PASS: hovering put a tooltip on screen"
