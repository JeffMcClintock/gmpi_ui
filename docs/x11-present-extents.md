# X11 `present()` and the cached surface extents

`X11DrawingFrame::Impl::present()` reads the surface size once, then hands
control to the client three times before it uses that size. This note records
what the audit found, because the interesting half of the answer is not the
fix — it is *why the bug cannot fire today*, and how narrow the margin is.

TIDE BACKLOG **P7c**, the X11 sibling of the macOS **P7b** guard in
`DrawingFrameMac.mm` and of the Windows **P4** crash.

## The defect

```
const int pw = d.width;              // read here
const int ph = d.height;
if (!d.ensureImage(pw, ph)) return;  // allocation sized here
    ...
d.client->measure(&avail, &desired); // three re-entrant client calls
d.client->arrange(&all);
d.client->render(rt);
    ...
const gmpi::cpugfx::DestSurface dst{ d.image->data,       // re-read
                                     d.image->bytes_per_line, // re-read
                                     pw, ph };            // STALE
gmpi::cpugfx::encodeDirtyRect(src, dst, area);
```

`d.image->data` and `->bytes_per_line` are re-read, so a *replaced* image is
picked up — but sized by `pw`/`ph`, which are not. `encodeDirtyRect` clips the
rectangle to `dst.width`/`dst.height`, so the clip protects nothing: it clips to
the stale values. The encode then walks `pw` × `ph` pixels down the *new*,
shorter stride and off the end of the allocation.

This is a heap overflow rather than a null dereference, which makes it a worse
outcome than P4: it corrupts before it crashes, and it may not crash at all.

`reSize()` alone does **not** cause it. It moves `d.width`/`d.height` and calls
`XResizeWindow`, but reallocates nothing, so the image still matches `pw`/`ph`
and the frame merely paints at the old size until the `ConfigureNotify` arrives.
The overflow needs the *allocation* to change, and `ensureImage()` has exactly
one caller — `present()` itself. So it requires a **nested `present()`**.

## Is a nested `present()` reachable?

**Through the public interfaces: no, and by construction rather than by
accident.** Four facts, each checked against the source rather than assumed:

| Check | Result |
|---|---|
| `ensureImage()` call sites | **one** — `present()` |
| `Impl::present()` call sites | **two** — `processEvents()` and `onTimer()`, both host-driven entry points |
| `XNextEvent` / `XMaskEvent` / `XIfEvent` / `XPeekEvent` in the backend | **one** `XNextEvent`, inside `processEvents()`. No modal or nested loop anywhere: menus, stock dialogs, the colour dialog, the text edit and the tooltip are all serviced from that single pump, and the portal file chooser is async over D-Bus |
| what a client can call | `IDrawingHost`, `IInputHost`, `IDialogHost` — plus whatever `setFallbackHost` forwards. **None of them expose `processEvents()`, `onTimer()`, `reSize()`, or the `Display*`** |

`processEvents()`, `onTimer()` and `reSize()` are non-virtual members of the
concrete `X11DrawingFrame`. A client holds it only as an interface pointer
obtained through `queryInterface`, so it has no way to name them. This is
`DrawingFrameX11.h`'s stated rule 1 — *"NO EVENT LOOP OF OUR OWN. The host owns
the loop"* — doing real work: because the backend never pumps X on its own
behalf, and the client cannot ask it to, the client cannot re-enter `present()`.

So the row's open question — *can any client callback pump the X event loop?* —
answers **no**.

### The residual, which is not zero

The audit cannot close one path, and it is worth naming rather than rounding
down. During `measure`/`arrange`/`render` the client may call back into the
**host** (`setFallbackHost`'s `IEditorHost`, e.g. to move a parameter). If that
host then re-enters its own run loop — some do, around modal operations — it
reaches `processEvents()`/`onTimer()` and the nesting happens. Nothing in
gmpi_ui can prevent or detect that, and nothing in gmpi_ui can prove no host
does it.

That residual is why this is guarded rather than merely documented. A heap
overflow whose only defence is "no host we know of re-enters" is a bad trade
against three lines.

## The fix

Re-check the extents after the client calls, and drop the frame if the image was
replaced:

```cpp
if (!d.image || d.imageWidth != pw || d.imageHeight != ph)
{
    rtRaw->release();
    return;
}
```

Dropping the frame is correct, not just safe. The nested `present()` already
rendered and blitted the whole surface at the new size and cleared
`d.dirtyAll`/`d.dirty` on its way out, so there is nothing left to show; the
early return leaves those flags exactly as it found them. Had the guard instead
rebuilt `dst` from `d.imageWidth`/`d.imageHeight`, it would have blitted the
top-left crop of a stale, larger render over a correct frame — safe, and wrong.

Placement is after `rt->endDraw()`, which is after **all four** re-entrant calls
(`measure`, `arrange`, `render`, and `activeEdit->render`), not just after
`render`. A nested present during `measure` wastes the outer render, which is
merely slow; the same guard catches it before the blit.

## Verification

`tests/x11_present_reentrant_resize.cpp`, run by
`tests/run_x11_present_reentrant_test.sh`. It is a **positive control for the
guard**, not a reproduction of host behaviour: it reaches the nesting the only
way anything can, by handing the synthetic client a pointer to the concrete
frame — which a real client never has.

A/B on the same harness binary, only the backend source differing:

| backend | result |
|---|---|
| `origin/main` as it stood | **SIGSEGV, exit 139, 3/3** |
| with the guard | **exit 0, PASS, 3/3** |

The fault, under gdb:

```
Program received signal SIGSEGV
#0  gmpi::cpugfx::encodeDirtyRect (...) at backends/CpuEncode.h:269
#1  X11DrawingFrame::Impl::present (...) at backends/DrawingFrameX11.cpp:1332
#2  X11DrawingFrame::onTimer (...) at backends/DrawingFrameX11.cpp:1220
locals: right = 800, bottom = 600, y = 68
```

`right`/`bottom` are the stale extents, against a 64×48 image — the clip
clipping to the lie, exactly as described above. It faulted at row **68**, so
rows 48–67 were written outside the surface *before* the process hit an unmapped
page. That is the difference between this and P4 in one number.

**Two checks in the test stop it passing vacuously**, which matters more than
the exit code: it asserts the nested present actually ran (`nestedPresents == 1`)
and that the frame really did shrink (`64×48`). A guard that prevented the
scenario instead of surviving it would otherwise look identical.

### On the detector

On a local display — including Xvfb — the image is MIT-SHM, so `image->data` is
a `shmat()` mapping of its own and the overflow runs off the end of it into
unmapped address space: a plain SIGSEGV, no sanitizer needed. **AddressSanitizer
does not instrument a shm segment**, so an ASan run of that path is a false
negative. The `ASAN=1` option in the script is for the `XCreateImage` fallback
(a remote display), where the buffer is `malloc`'d and ASan does cover it. Same
shape as the macOS half, where Guard Malloc is the detector and ASan is likewise
blind — for a different reason there (the read happens inside CoreGraphics).
