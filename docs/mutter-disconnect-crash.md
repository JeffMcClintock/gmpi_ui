# mutter 46: compositor segfault when a client disconnects after xdg_popup use

Draft bug report, ready to file against mutter (GNOME GitLab). Everything below
was observed on this machine; the workaround shipped in
`backends/DrawingFrameWayland.h` (`~Connection`), which is also where the notes
were first recorded.

## Environment

- Ubuntu 24.04.4 LTS
- gnome-shell 46.0-0ubuntu6~24.04.13
- libmutter-14-0 46.2-1ubuntu0.24.04.14
- Session: Wayland (native GNOME session; also reproduces in
  `gnome-shell --nested --wayland`)

## Summary

If a Wayland client creates an `xdg_popup` at any point in its session and then
disconnects without draining pending requests, mutter can segfault inside its
resource-destroy handling during `wl_client_destroy` - taking the whole desktop
(or the nested session) down with the client. The user-visible symptom on a
native session is an instant fall to the login prompt.

Crash stack (from the journal after a native-session crash; apport retraced):

    #0 g_type_check_instance
    #1 g_signal_handler_disconnect
    #2 libmutter-14 (resource destroy handler)
    #3 wl_client_destroy

i.e. mutter calls `g_signal_handler_disconnect` on an object it has already
finalised, while tearing down the disconnecting client's resources in bulk.

## Conditions

- Only after the client has created at least one `xdg_popup` (with an input
  grab, in our case). A client that only maps a toplevel and exits does not
  trigger it.
- The popups were already destroyed, correctly ordered (topmost first), well
  before disconnect. What matters is that popup-related resources existed at
  some point in the session.
- Killing the client with a signal (no orderly disconnect) triggers the same
  teardown path.

## Reproduction used here

The client is the SynthEdit Wayland editor / gmpi_ui demo, but nothing in it is
special: open a toplevel (libdecor CSD), open and close one grabbing
`xdg_popup` menu, exit. Automated in `gmpi_ui/tests/autotest.sh` against a
nested `gnome-shell --wayland`; before the workaround the nested compositor
died on 4 of 4 exit-after-menu runs, and a native session fell to the login
prompt the same way.

## Workaround (client-side)

A `wl_display_roundtrip()` immediately before `wl_display_disconnect()`. That
makes mutter tear the client's remaining resources down while the client is
still connected, one reply at a time, instead of all at once inside
`wl_client_destroy`. Measured: 2 of 2 runs crashed the compositor without the
roundtrip, 0 of 2 with it; the nested-harness rate went from 4/4 to 0/4.

A client crash (SIGKILL, no chance to roundtrip) presumably still triggers the
bug, which is why this is worth fixing in mutter rather than only working
around: any Wayland client that uses popups and then crashes can take down a
GNOME 46 session.

## Filing notes

- The retraced apport report on this machine is
  `/var/crash/_usr_bin_gnome-shell.1000.crash` (root to read; already uploaded
  to the Ubuntu error tracker once by whoopsie - see the `.uploaded` marker).
- Search upstream for duplicates before filing: `g_signal_handler_disconnect`
  + `wl_client_destroy` is a distinctive pair. mutter 47+ reworked some of the
  resource-teardown paths, so also check whether 46.x backports exist.
