# Plugin editors on Linux: X11 and Wayland

VST3 defines **two** Linux embeddings, and which one you get depends on the
host:

| SDK | platform type | parent handed to `attached()` |
|---|---|---|
| any | `kPlatformTypeX11EmbedWindowID` | an X11 `Window` id |
| 3.8.0+ | `kPlatformTypeWaylandSurfaceID` | the host frame's `wl_surface` |

Both are implemented here:

* `backends/DrawingFrameX11.{h,cpp}` + `GMPI_Wrappers/.../SEVSTGUIEditorLinux.*`
* `backends/DrawingFrameWayland.h` (`WaylandSubsurfaceFrame`) +
  `GMPI_Wrappers/.../SEVSTGUIEditorWayland.*`

`Controller_VST3::createView` probes for `IWaylandHost` and picks Wayland when
the host offers it, X11 otherwise. The probe is deliberately for the interface,
not for `WAYLAND_DISPLAY`: a host running under Wayland may still embed its
plugins through XWayland, and only the host knows which it does.

## Both share one Linux-specific rule: you do not own the event loop

`namespace Steinberg::Linux` in `pluginterfaces/gui/iplugview.h` defines
`IRunLoop`, `IEventHandler` and `ITimerHandler`. The contract:

* the plugin queries `IPlugFrame` for `Linux::IRunLoop` — in `setFrame`, which
  the host calls **before** `attached`;
* the plugin registers file descriptors (`registerEventHandler`) and timers
  (`registerTimer`);
* the host polls and calls back.

A plugin that spins its own `XNextEvent` loop, or its own timer thread touching
the GUI, is broken on Linux even if it appears to work in one host. Both frames
are built around this: neither blocks, neither polls speculatively, and each
exposes a connection fd for the purpose.

**Unregister before closing.** The run loop holds the raw fd; a descriptor
closed while still registered can be reused by something else in the host
process. That bug only shows up under load, in someone else's code.

## X11 specifics

Coordinates are **physical pixels** (as on Windows; only macOS uses logical
units). Inherit the parent's visual and depth rather than assuming the default —
a host on a 32-bit ARGB visual gives `BadMatch` on window creation otherwise,
which presents as "the plugin window never appears" and nothing else.

X11 has a real pointer grab, so `setCapture()` is honoured properly: a drag that
leaves the plugin window keeps delivering motion. Native Wayland cannot do this
at all.

## Wayland specifics (3.8.0)

From `pluginterfaces/gui/iwaylandframe.h`:

* The plugin does **not** connect to the system compositor. The host acts as a
  compositor for its plugins; `IWaylandHost::openWaylandConnection()` returns a
  `wl_display` connected to it, and `closeWaylandConnection()` is its
  counterpart — never `wl_display_disconnect`. `Connection::adopt()` exists for
  exactly this, and leaves `owned_` false so the destructor cannot disconnect.
* `IWaylandHost` is created through `IHostApplication::createInstance`, and
  `IHostApplication` arrives via `IPluginFactory3::setHostContext` — which is
  why `MyVstPluginFactory` keeps it. That is the only route.
* `attached()` receives the host frame's `wl_surface`, of unknown role. We
  create our own and give it the `wl_subsurface` role with that as parent.
* Popups and dialogs need an `xdg_surface` to anchor to, and a subsurface is not
  one. `IWaylandFrame::getParentSurface()` supplies the host's, along with its
  position relative to ours — which is why `popupParent()` and
  `toFrameCoordinates()` have been virtuals on `WaylandFrameBase` from the
  start.

Three things that are silent when you get them wrong:

1. **A new subsurface is synchronized.** Our commits do nothing until the parent
   commits. `wl_subsurface_set_desync()` is what makes the view update on its
   own.
2. **The parent must commit once for the subsurface to appear at all.** Adding a
   sub-surface is a change to the *parent's* state. The plugin may not touch the
   parent (the header says so explicitly), so this is the host's job — real
   hosts do it as part of their own rendering. A host that never re-commits
   shows a permanently blank plugin area, with no error anywhere.
3. **`IWaylandHost::iid` and `IWaylandFrame::iid` must be defined by your own
   module.** They are in none of the SDK's central IID translation units —
   `coreiids.cpp`, `baseiids.cpp`, `commoniids.cpp`, `vstinitiids.cpp`, or the
   samples' `usediids.cpp` — so linking those, which is how a plugin gets every
   other interface's IID, does not supply them. Each consumer writes its own
   `DEF_CLASS_IID`, and the SDK does the same in
   `vstgui4/vstgui/plugin-bindings/vst3editor.cpp` (plugin side) and in the
   editorhost sample's `wayland/window.cpp` (host side).

   Miss it and the module still *links* — a shared object may carry undefined
   symbols — then fails to `dlopen` in the host, with nothing in the build to
   say why. `nm -DC --undefined-only` on the built `.so` is how to catch it.
   Conversely, a module that compiles `vst3editor.cpp` as well already has
   them, and adding ours would be a duplicate definition.

4. **Nothing pumps the portal bus or the clipboard for you.** The standalone
   app does both in `WaylandToplevel::runEventLoop`, so it is easy to write a
   dialog, watch it work in the app, and ship a plugin where it does not. The
   portal answers on the D-Bus session bus and the clipboard on the wayland
   data device; neither is the display's pending queue, and a plugin has no
   loop of its own. `WaylandSubsurfaceFrame::tick()` pumps them from the host's
   timer. Without it a file chooser opens, the user picks a file, and the
   callback never fires - no error, just silence.

### The portal chooser is not parented to the host window

A file dialog is the desktop portal's own window in another process, and
parenting one needs an exported handle string (`wayland:<handle>`) from
`zxdg_exporter_v2`, which only exports **toplevels**. A plugin's view is a
subsurface, and 3.8.0 offers no way to get a handle for the host's window:
`getParentSurface()` returns an `xdg_surface`, `getParentToplevel()` an
`xdg_toplevel`, and the protocol has no accessor from either back to the
`wl_surface` the exporter would need - nor is the host obliged to implement the
exporter at all. So the chooser appears unparented, and this cannot be fixed
from the plugin side.

Note this is specific to the *portal*. Dialogs we draw ourselves - the message
box and the colour picker - are real `xdg_toplevel`s of our own, and those are
parented properly: `getParentToplevel()` is passed to
`WaylandSubsurfaceFrame::setParentToplevel()` and reaches
`xdg_toplevel_set_parent`, which is exactly the use the header describes. X11
has no such gap either, because the portal takes an `x11:<hex window id>`
parent and a plugin knows its own window id.

## Testing

`GMPI_Wrappers/tests/` has a host per path, because the Steinberg validator
never calls `attached()` and so cannot tell a working editor from one that
returns `kResultTrue` and draws nothing — which is exactly the state the Linux
wrapper was in before this work.

    run_x11_editor_test.sh     <x11_editor_host> <plugin.vst3> [--expect-input]
    run_wayland_editor_test.sh <wayland_editor_host> <plugin.vst3>

The X11 one runs on a throwaway Xvfb; `--expect-input` synthesises a drag with
xdotool and requires the picture to change (use it only for plugins with
something draggable where the drag lands).

The Wayland one runs on Xvfb + a nested `gnome-shell --wayland`, never the real
session, and checks by **diffing a screenshot taken before the editor attaches
against one taken after**. Earlier versions of that check counted distinct
colours (the wallpaper alone has thousands) and then derived the window box from
the host's own fill colour (which shrinks to whatever the plugin did not cover,
and reported a working 400x200 analyser as blank). Both passed while something
was broken. The diff is the version that discriminates: with the parent commit
removed it reports zero changed pixels, and with it the changed region matches
each plugin's view rectangle exactly.
