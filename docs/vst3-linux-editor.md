# Plugin editors on Linux: why X11, and what Wayland would take

Short version: **VST3 has no Wayland.** The specification defines exactly one
Linux embedding mechanism, and it is an X11 window id. A Wayland-native host
therefore reaches its plugins through XWayland, and so do we.

This is the reasoning behind `backends/DrawingFrameX11.{h,cpp}` and
`GMPI_Wrappers/wrapper/VST3/SEVSTGUIEditorLinux.{h,cpp}`. It is not a
preference, and it is not something we can fix on our side alone.

## What the specification actually says

From `pluginterfaces/gui/iplugview.h` (SDK 3.7.14, and every earlier version):

    kPlatformTypeHWND              HWND handle.        (Windows)
    kPlatformTypeHIView            HIViewRef.          (macOS)
    kPlatformTypeNSView            NSView pointer.     (macOS)
    kPlatformTypeUIView            UIView pointer.     (iOS)
    kPlatformTypeX11EmbedWindowID  X11 Window ID.      (X11)

There is no `kPlatformTypeWlSurface`, no wl_surface, no
`zxdg_imported_v2` handle. Grepping the whole SDK for "wayland" returns
nothing. `IPlugView::attached` receives an X11 `Window` (an XID) or it receives
nothing we can use.

The same header states the coordinate convention: on Linux, as on Windows, view
coordinates are **physical pixels**, not logical units. Only macOS uses logical
units.

## The other Linux-specific rule: you do not own the event loop

`namespace Steinberg::Linux` in the same header defines `IRunLoop`,
`IEventHandler` and `ITimerHandler`. The contract is:

* the plugin queries `IPlugFrame` for `Linux::IRunLoop` — this happens in
  `setFrame`, which the host calls **before** `attached`;
* the plugin registers file descriptors (`registerEventHandler`) and timers
  (`registerTimer`) with it;
* the host polls, and calls back.

A plugin that spins its own `XNextEvent` loop, or its own timer thread touching
the GUI, is broken on Linux even if it appears to work in one host. `X11Frame`
is built around this: it never blocks, never polls speculatively, and exposes
`connectionFd()` for exactly this purpose.

Unregister before closing the display. The run loop holds the raw fd, and a
descriptor closed while still registered can be reused by something else in the
host process — a bug that only shows up under load, in someone else's code.

## What a Wayland path would actually require

Not "add a wl_surface backend". The missing piece is a **protocol extension**,
because a Wayland client cannot place a surface inside another process's
surface without one:

1. A new VST3 platform type, or a host-side extension interface, that passes an
   exported parent surface handle — `zxdg_exporter_v2` / `zxdg_imported_v2` is
   the existing mechanism for cross-process surface parenting, and it only
   supports parenting a *toplevel*, not embedding into a subsurface tree.
2. Agreement between hosts and plugins on it. One vendor implementing it
   unilaterally buys nothing: both ends must speak it.
3. Input routing, which is the harder half. Wayland delivers pointer and
   keyboard to the surface the compositor decides has focus. A plugin surface
   embedded in a host window is not separately focusable, so the host would have
   to forward events, which means a protocol for *that* too.

Until such a thing exists and hosts ship it, XWayland is the answer, and it is
a perfectly good one: the plugin gets a real X11 window, real input, and real
pointer grabs (which native Wayland does not offer at all — see the capture
notes in `DrawingFrameWayland.h`).

The Wayland backend in this repo is for the *application* case — SynthEdit's own
editor, which owns its toplevel. That is a different problem and it is solved.

## Testing

`GMPI_Wrappers/tests/x11_editor_host.cpp` is a minimal host that actually calls
`attached()`. The Steinberg validator never does, so it cannot distinguish a
working editor from one that returns `kResultTrue` and draws nothing — an
important gap, since that is exactly the state the Linux wrapper was in before
this work.

    run_x11_editor_test.sh <x11_editor_host> <plugin.vst3> [--expect-input]

Runs on a throwaway Xvfb. `--expect-input` additionally synthesises a drag and
requires the picture to change; use it only for plugins that have something
draggable where the drag lands.
