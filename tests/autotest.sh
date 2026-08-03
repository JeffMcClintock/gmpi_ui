#!/usr/bin/env bash
# Drive the REAL Wayland backend with real input serials and real popup grabs,
# without risking the developer's login session.
#
#   Xvfb :5  ->  gnome-shell --nested (real mutter)  ->  tests/wayland_demo
#   xdotool injects into Xvfb; mutter forwards it as genuine Wayland input.
#
# A popup grab needs a real button-press serial, so no purely headless mode can
# produce one - this is the only way we have found to exercise menus unattended.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
TOOLS=${WLTEST_TOOLS:-$HOME/.cache/wayland-testtools}
LOG=${TMPDIR:-/tmp}/gmpi-autotest.log
DISP=${WLTEST_XDISPLAY:-:5}
SOCK=wayland-gmpitest

[ -x "$HERE/wayland_demo" ] || { echo "no demo binary - run ./tests/run.sh --demo"; exit 1; }

if [ ! -x "$TOOLS/usr/bin/Xvfb" ] || [ ! -x "$TOOLS/usr/bin/xdotool" ]; then
    echo "fetching test tools into $TOOLS (no root required) ..."
    mkdir -p "$TOOLS/dl" && ( cd "$TOOLS/dl" &&
        apt-get download xdotool libxdo3 xvfb xserver-common x11-xkb-utils >/dev/null 2>&1
        for d in *.deb; do dpkg -x "$d" "$TOOLS/"; done )
    [ -x "$TOOLS/usr/bin/xdotool" ] || { echo "tool install failed; see $TOOLS/dl"; exit 1; }
fi
export LD_LIBRARY_PATH=$TOOLS/usr/lib/x86_64-linux-gnu
xdo() { env DISPLAY="$DISP" "$TOOLS/usr/bin/xdotool" "$@"; }

XVFB=""; NEST=""; DEMO=""
cleanup() { for p in "$DEMO" "$NEST" "$XVFB"; do [ -n "$p" ] && kill "$p" 2>/dev/null; done; }
trap cleanup EXIT

fail() { echo "FAILED: $*"; echo "--- demo log ---"; cat "$LOG" 2>/dev/null; exit 1; }

# --- Xvfb -------------------------------------------------------------------
if ! xdo getdisplaygeometry >/dev/null 2>&1; then
    "$TOOLS/usr/bin/Xvfb" "$DISP" -screen 0 1400x900x24 >/dev/null 2>&1 & XVFB=$!
    for i in $(seq 1 40); do xdo getdisplaygeometry >/dev/null 2>&1 && break; sleep 0.25; done
fi
xdo getdisplaygeometry >/dev/null 2>&1 || fail "Xvfb $DISP never came up"

# --- nested mutter ----------------------------------------------------------
# A stale socket satisfies the wait loop instantly and we then connect to a dead
# compositor; and the desktop-icons extension crash-loops in a nested shell,
# which destabilises it, so run without extensions.
rm -f "$XDG_RUNTIME_DIR/$SOCK" "$XDG_RUNTIME_DIR/$SOCK.lock"
env -u WAYLAND_DISPLAY DISPLAY="$DISP" GNOME_SHELL_DISABLE_EXTENSIONS=1 dbus-run-session -- \
    gnome-shell --nested --wayland --wayland-display="$SOCK" >"${TMPDIR:-/tmp}/gmpi-compositor.log" 2>&1 & NEST=$!
for i in $(seq 1 80); do [ -S "$XDG_RUNTIME_DIR/$SOCK" ] && break; sleep 0.5; done
[ -S "$XDG_RUNTIME_DIR/$SOCK" ] || fail "nested mutter never came up"
sleep 3    # the socket appears slightly before clients are accepted

# --- the demo ---------------------------------------------------------------
: > "$LOG"
env WAYLAND_DISPLAY="$SOCK" "$HERE/wayland_demo" >"$LOG" 2>&1 & DEMO=$!
for i in $(seq 1 40); do grep -q "render #1" "$LOG" && break; sleep 0.25; done
grep -q "render #1" "$LOG" || fail "demo never rendered"
kill -0 $DEMO 2>/dev/null || fail "demo exited early"
echo "demo is rendering through the real backend"

# --- calibrate: X coords -> surface coords ----------------------------------
click_surface_coords() {
    local before after i
    before=$(grep -c 'pointer down' "$LOG")
    xdo mousemove "$1" "$2"; sleep 0.4; xdo click 1
    after=$before
    for i in $(seq 1 12); do
        after=$(grep -c 'pointer down' "$LOG")
        [ "$after" -gt "$before" ] && break
        sleep 0.2
    done
    [ "$after" -gt "$before" ] || return 1
    grep 'pointer down' "$LOG" | tail -1 | sed 's/.*at //; s/,/ /'
}

xdo mousemove 400 350; sleep 0.5; xdo click 1; sleep 0.8   # warm-up: focuses the window

C1=$(click_surface_coords 300 250) || fail "no input reached the client"
read -r s1x s1y <<<"$C1"
OFFX=$((300 - s1x)); OFFY=$((250 - s1y))
echo "calibration: surface = X - ($OFFX,$OFFY)"
X() { echo $((OFFX + $1)); }; Y() { echo $((OFFY + $1)); }

# --- message box ------------------------------------------------------------
# Before the menu: Escape is ambiguous once a menu may or may not still be up,
# and the demo quits on an Escape it receives itself.
xdo key m; sleep 2
kill -0 $DEMO 2>/dev/null || fail "demo died opening a message box"
xdo key Escape; sleep 1.5                       # the dialog consumes this
grep -q "message box ->" "$LOG" || fail "message box never reported a result"
echo "message box shown and answered: $(grep 'message box ->' "$LOG" | tail -1)"

# --- context menu, with a real grab -----------------------------------------
xdo mousemove "$(X 150)" "$(Y 150)"; sleep 0.5; xdo click 3; sleep 2
grep -q "context menu requested" "$LOG" || fail "right-click did not reach populateContextMenu"
kill -0 $DEMO 2>/dev/null || fail "demo died opening the context menu"
echo "context menu opened (real popup grab)"

# hover down the menu so the submenu opens
for dy in 30 55 80 100 120; do xdo mousemove "$(X 190)" "$(Y $((150 + dy)))"; sleep 0.5; done
kill -0 $DEMO 2>/dev/null || fail "demo died opening a submenu"

# dismiss by choosing a plain item, NOT Escape: if the menu has already closed,
# an Escape reaches the app and quits it, which reads as a crash further down
xdo mousemove "$(X 190)" "$(Y 160)"; sleep 0.5; xdo click 1; sleep 1.5
kill -0 $DEMO 2>/dev/null || fail "demo died dismissing the menu"
echo "submenu opened and menu dismissed"
kill -0 $NEST 2>/dev/null || fail "compositor died DURING the test, before the app quit"

# --- colour picker ----------------------------------------------------------
# Driven by keyboard only: the picker is a window of its own, and the nested
# compositor places it where it likes, so there are no coordinates to click.
# Return accepts, which exercises the whole path including the sRGB conversion.
xdo key k; sleep 2
grep -q "colour dialog opened" "$LOG" || fail "colour dialog never opened"
kill -0 $DEMO 2>/dev/null || fail "demo died opening the colour dialog"
xdo key Return; sleep 2
grep -q "colour ->" "$LOG" || fail "colour dialog never reported a colour"
echo "colour picker: $(grep 'colour ->' "$LOG" | tail -1)"
kill -0 $DEMO 2>/dev/null || fail "demo died closing the colour dialog"

# --- in-place text edit -----------------------------------------------------
# This one IS on the client's surface, so its rect is known: (40,220)-(320,248).
xdo key t; sleep 1.5
grep -q "text edit opened" "$LOG" || fail "text edit never opened"

# setText selects everything, so typing replaces it
xdo type --delay 60 "Filter"; sleep 1
xdo key ctrl+a; sleep 0.4          # select all
xdo key ctrl+c; sleep 0.4          # copy
xdo key End;    sleep 0.4
xdo key ctrl+v; sleep 0.6          # paste it back: "FilterFilter"
xdo key Return; sleep 1.5

grep -q "edit committed ->" "$LOG" || fail "text edit never committed"
COMMITTED=$(grep "edit committed ->" "$LOG" | tail -1)
echo "text edit: $COMMITTED"
case "$COMMITTED" in
  *"FilterFilter") echo "        clipboard round-tripped through the compositor" ;;
  *"Filter")       echo "        WARNING: typing worked but the clipboard did not round-trip" ;;
  *)               fail "text edit produced unexpected content: $COMMITTED" ;;
esac
kill -0 $DEMO 2>/dev/null || fail "demo died in the text edit"

# --- quit -------------------------------------------------------------------
echo "compositor alive before quit: yes"
xdo key Escape; sleep 2
for i in $(seq 1 20); do kill -0 $DEMO 2>/dev/null || break; sleep 0.25; done

RC=0
if kill -0 $NEST 2>/dev/null; then echo "RESULT: compositor survived"
else echo "RESULT: *** COMPOSITOR DIED ***"; RC=1; fi
if kill -0 $DEMO 2>/dev/null; then echo "        demo still running (did not quit on Escape)"; RC=1
elif grep -q "demo exited cleanly" "$LOG"; then echo "        demo exited cleanly"
else echo "        demo did NOT exit cleanly - log tail:"; tail -5 "$LOG" | sed 's/^/          /'; RC=1; fi

echo "log: $LOG"
exit $RC
