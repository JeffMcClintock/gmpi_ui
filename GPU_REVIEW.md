# Review: gmpi_ui GPU work — sharing across platforms, CPU → shaders (2026-10-24)

Scope: gmpi_ui branch `vendor-bgfx` (= `gpu-backend` + `shader-api` + vendoring).
Read: `backends/GpuGfx.h`, `backends/gpu/*`, the five platform frames, the
D2D and CoreGraphics backends, the Metal plan. Line numbers are from that branch.

## The shape of it today

| Platform | Window + present | 2D renderer | Text raster |
|---|---|---|---|
| Windows | `DrawingFrameWin.cpp` (2.9k) | **Direct2D** (`DirectXGfx.{h,cpp}`, 3.2k) | DirectWrite |
| macOS / iOS | `DrawingFrameMac.mm` (1.1k), `DrawingFrameIos.mm` | **CoreGraphics** (`CocoaGfx.h`, 3.1k) | CoreText |
| Linux Wayland | `DrawingFrameWayland.h` (4.6k) | cpugfx (3.4k) or **GpuGfx** (1.8k) | `CpuTextEngine` (FreeType + HarfBuzz, 2.6k) |
| Linux X11 | `DrawingFrameX11.cpp` (3.0k) | same | same |

So there are **three independent implementations of the same 30-method
drawing interface**, each with its own gradients, clips, text and bitmap
code, and two of them are the thing the GPU backend was built to replace.
The GPU adaptor itself is already clean on this axis: `GpuGfx.h` has **no
platform `#if` at all**, `backends/gpu/*.cpp` has them only in `renderer.cpp`,
and every shader is written once in `.sc` and compiled for Vulkan, Metal and
D3D12. The platform-specific code that remains is (a) the two 3k-line native
renderers, (b) the present loop, duplicated per frame, and (c) text.

## A. Share code between platforms

### A1. One renderer, three devices (the big one)

`DirectXGfx` and `CocoaGfx` implement exactly the method list `GpuGfx.h`
implements (fill/draw rect, ellipse, rounded rect, geometry, text, bitmap,
clips, brushes, factory). The Metal plan already rules that the GPU backend
goes under `DrawingFrameMac.mm`; the review's point is the size of the prize:
once it does, and D3D12 does the same on Windows, **~6.2k lines of
platform renderer become dead** and the only 2D renderer is `GpuGfx.h` +
`backends/gpu/` (with cpugfx as the software fallback everywhere, which is
also cross-platform). D2D stays as the *test oracle* on Windows, nothing more.

Jeff, 2026-10-25: client code still uses the CoreGraphics backend, so
`CocoaGfx.h` (and `DirectXGfx`) stay as they are until that migration;
nothing in step 1 below touches them.

What this needs that doesn't exist yet: the present loop on Win/Mac (A2 is the
shared form of it), a model surface per platform (A3), and text that doesn't
depend on the native engine (B1). Nothing in `GpuGfx.h` changes.

### A2. The present loop is the same code twice; make it one function

`DrawingFrameWayland.h:1151-1243` and `DrawingFrameX11.cpp:1236-1304` are the
same ~50 lines: create-or-resize the window target, `measure`/`arrange` on
`needsMeasure`, map the dirty rect to device pixels (same `max(0, …*scale)`
and `+1` rounding), `beginDraw`, `invalidate(area)`, push the clip, scale
transform, `client->render`, overlay, pop, `endDraw`. Only four things differ:
the native handle, Wayland's `wl_surface_set_opaque_region` /
`wp_viewport_set_destination`, how the logical size is obtained, and the
text-edit overlay call. Windows and Mac will need the identical loop.

Proposed: `gpugfx::WindowPresenter` (in `GpuGfx.h`, next to `initialize()`),
holding the target and `gpuW/gpuH/dirtyAll`, with
`present(client, dirtyLogical, scale, pw, ph, overlay, onSized)`. The two
Linux frames shrink to the four differences; Win/Mac get the loop for free.
Same for the init block (`wanted()` → tried-once → model surface →
`initialize` → fallback message), which is also duplicated.

### A3. The "model surface" belongs in `backends/gpu/`, per platform

bgfx must be initialised on *some* window before swap chains work. Mac has
`windowless_context.mm` (a hidden `CAMetalLayer`) inside `backends/gpu/`;
Linux has the equivalent (a role-less `wl_surface` at Wayland `:1044`, a 1×1
`XCreateSimpleWindow` at X11 `:1372`) **inside the frames**, with
`gpugfx::initialize(model, display, wayland)` carrying the handle in. Two
consequences:

- `Canvas::setWindowless` → `Renderer::initializeWindowless()` →
  `initialize(windowlessContext(), nullptr)` works on Mac and, on Linux,
  initialises bgfx with `nwh = nullptr` if no frame has run yet. An offscreen
  canvas created before the first window (tests do this; a plugin host could)
  would then leave the process without swap-chain caps. Not reproduced here;
  worth one test.
- The frames carry code that is really the renderer's.

Proposed: `backends/gpu/platform/model_surface_{linux,mac,win}.{cpp,mm}`
providing `modelSurface(display)`; `initialize()` loses its handle parameter.

### A4. `renderer.cpp` is three branches where one line would do

- `resetFlags()` (`:62-79`): Windows `FLIP`, Mac `FLIP|vsync`, Linux
  `vsync|FLIP`. That is `FLIP | (windows ? 0 : vsync)`, and Windows dropping
  vsync/sRGB wants a comment or wants removing.
- `initialize()` (`:110-128`): the Windows "scan for D3D12" loop duplicates
  the generic supported-renderer loop six lines below it. A
  `constexpr RendererType kPlatformRenderer` plus the existing check is
  enough; only the error string is per-platform.
- Stale: `GpuGfx.h:4` still says "D3D11 / Metal / Vulkan".

### A5. One thing to check, not to assume

**Status 2026-10-25: FIXED (same branches).** Every reset now goes through
`Renderer::reset(w, h, extra)` = `resetFlags() | extra`, and a
configure-time check refuses `gfx::reset(` outside renderer.cpp (verified to
fire). vsync now defaults OFF: every gmpi_ui host paces its own frames, and
whichever creates the renderer first (an offscreen canvas) must not impose
vsync on later windows. Test: `GpuRenderer.WindowlessCanvasKeepsTheWindowResetFlags`.

`Canvas::setWindowless` (`canvas.cpp:52-57`) ends with
`bgfx::reset(0, 0, FLUSH_AFTER_RENDER | FLIP_AFTER_RENDER)`, which is **not
`Renderer::resetFlags()`**: it drops `VSYNC` and, on Linux,
`SRGB_BACKBUFFER`. Window swap chains created afterwards (`layer.cpp:180-184`
relies on that flag for the sRGB variant) would come up linear. The editor
never hits it (it creates no windowless canvas), the tests never create a
window after one, so it is invisible today. One test: windowless canvas, then
a window target, check a mid-grey.

### A6. Smaller duplications

- Mac `drawRect` (`:273-290`) and iOS (`:424-440`) are the same CPU present
  block; A2's presenter covers them too once they are on the GPU.
- `drawGeometry` on CPU fallback widens the stroke **twice** (`GpuGfx.h:1118`,
  then `cpu.drawGeometry` widens again); the first result is only used for
  bounds.
- `asCpuBitmap` (`:901-919`) does a synchronous GPU read-back to feed a GPU
  draw: a GPU target drawn as a bitmap should be a texture-to-texture blit.

## B. Replace platform-specific / CPU code with cross-platform shaders

Ranked by what it removes. Everything in this list is work the GPU path does
on the CPU today and then uploads at 8 bytes per pixel.

### B1. Text: a coverage atlas tinted on the GPU (removes a platform engine per OS)

**Status 2026-10-25: DONE in its first form, uncommitted, on `text-atlas`
branches** (gmpi_ui on `clip-mask`; gimpi_ui_tests). Finding that shaped
it: the text engine's coverage is NOT colour-independent - to match
DirectWrite's weight it warps opaque text's coverage by a curve whose
strength follows the brush's LUMINANCE (`CpuTextEngine.h`, kTextWarpDark),
and translucent text skips the warp. So the cache keys a *coverage class*
(opaque at luminance L, or translucent) instead of the colour, rasterises
with a stand-in brush of that class (grey L, or 50% white), stores
coverage, and tints in `fs_tinted_texture` with the premultiplied brush
colour - exact, not approximate (0 outlier pixels against the real-colour
raster for black, white, mid, translucent; a sabotaged class fails the
test). Colour glyphs that do not tint are detected at rasterisation and
fall back to an exact-colour entry (untested here: no colour-emoji font).
`drawTextLayout` now goes through the same cache, keyed by the immutable
layout's identity (the entry holds a reference). Effect in the editor:
CPU text fallbacks 239 → 2 in 6 s; steady submit+present 2.4 ms vs
10.5 ms with the cache off. Side effect: the long-standing lavapipe failure
`TextLayoutMatchesDrawTextTrailingMultiline` passes (layout and plain text
now share one rasterisation path) - 274/274 on both drivers.
Not done yet: storage is still RGBA16F holding (c,c,c,c) (a true R16F/R8
coverage atlas is the ~4-8x memory win); `drawRichTextU` still CPU;
gradient/shader-brush text still uses the fallback colour; the cache's
quarter-pixel origin snapping (pre-existing) differs from exact CPU text by
up to ~0.02 at fractional origins.

Today `drawTextU` rasterises through `CpuTextEngine` into **RGBA16F colour
pixels** and caches them keyed by (text, format, **colour**, scale, phase)
(`GpuGfx.h:184-199, 1440-1487`); `drawRichTextU` and `drawTextLayout`
re-rasterise and re-upload **every draw** (`:1506, 1528`); anything under a
geometry clip or rotation is uncached. The shader that draws them,
`fs_tinted_texture`, already multiplies the texture by `gradient()` — the
adaptor just sets the tint to white (`:223`).

Change: cache **coverage** (R8 or R16F) per glyph (or per run), tint in the
shader. One raster then serves every colour, gradient and shader-brush text
comes free (the "v1: fallback colour" at `:1351` goes), layouts and rich text
become quads instead of uploads, and the cache shrinks ~8×. This is milestone
6 in the Visage plan; it is also what lets Windows and Mac stop rasterising
text through DirectWrite/CoreText — `CpuTextEngine` (FreeType + HarfBuzz,
already vendored for Linux) becomes the one text engine, and the native
APIs are left with font enumeration only.

### B2. Geometry clips as a mask in the path atlas (removes whole-draw fallback)

**Status 2026-10-25: DONE, uncommitted, on `clip-mask` branches** (gmpi_ui on
`device-seam`; gimpi_ui_tests and SynthEditLib for regenerated blobs). A
geometry clip pushed with no other active becomes a `ClipMask` (path-atlas
entry + device origin, `backends/gpu/clip_mask.h`); the canvas stamps the
current mask on every shape; batches split by clip (part of `SubmitBatch`'s
identity, so layers never merge across clips); `shaders/clip_mask.sh`'s
`applyClip(colour, gl_FragCoord.xy)` is the last step of every shape
shader (rect, circle, colour, path, tinted texture, both shader-brush
templates), scaling alpha, or all four channels for premultiplied blends.
`clear()` under a mask is an exact replace (new `Erase` + `PlusPremultiplied`
blend modes: dst*(1-m) + colour*m), which also fixes a GPU bug - the GPU
clear used to ignore the clip. Nested geometry clips: the inner one stays
CPU, the outer mask still applies to its upload. `GMPI_UI_GPU_CLIP_MASKS=0`
restores the old behaviour for comparison. Tests: 7 new (6 GPU-vs-cpugfx
scenes at worst < 0.01, ≤2 outliers, and a shader brush under a clip); with
masks off the clear and shader tests fail, as they should. Full suite
271/271 NVIDIA, 270 + the known text case lavapipe; SynthEdit 181/181. In
the editor every frame pushes a geometry clip; all now GPU masks. Gotcha:
bgfx's shader front end only exposes `gl_FragCoord` inside `main()`, so the
helper takes it as a parameter.

A geometry clip today turns **every** draw under it into a CPU raster
(`gpuSolid`/`gpuBrush` refuse at `:557/:708`, bitmaps at `:1284`, the text
cache at `:1380`, shader brushes at `:1092/:1133`), and `viaCpu` re-renders
the clip's coverage into a fresh target per draw (`:859-866`). The path atlas
already computes exact coverage for fills; a clip is the same coverage
sampled as a second texture and multiplied into alpha. One extra sampler in
`fs_sample_path`, `fs_rectangle`, `fs_circle`, `fs_tinted_texture` and the
two templates (or one `clipCoverage(uv)` helper in `shader_utils.sh`).
Milestone 2 lists this as still to do; it is the single biggest source of
"why is this frame slow" in a real document.

### B3. Paths over 512×512 texels go to the CPU (removes the size cliff)

`tooLargeForAtlas` (`:727, 749-754`) sends any figure whose bbox exceeds 512²
texels (or a side > 4096) to cpugfx, except one axis-aligned rect. A 600 px
ellipse, a big graph fill, a stroked circle wider than 512: all rasterised
and uploaded every frame. Two routes, both cross-platform:

- **Tile it**: split a large figure's bbox into ≤512² atlas rects, accumulate
  each with the same edge shader (edges clipped to the tile), sample each
  tile's quad. Exactness unchanged; no new shader.
- **SDF primitives for the common large shapes**: `shader_utils.sh` already
  has `sdRoundedRectangle`, `sdSegment`, `sdArc`, and the rect/circle SDFs
  take a `thickness`, so stroked rects and circles can be SDF too. The
  rounded rect was rejected once for a ~20-code-value corner difference from
  D2D's flattened arcs (`:1235-1237`). That is D2D being wrong by its own
  flattening tolerance; a decision for Jeff: hold the D2D reference, or
  accept the analytic answer and re-baseline those references.

### B4. `drawBitmap`: three small shader cases (removes per-draw uploads)

**Status 2026-10-25: cropped and nearest DONE, rotation not; uncommitted on
`bitmap-gradient` branches** (gmpi_ui on `text-atlas`; gimpi_ui_tests).
Images have their own vertex type, `ImageVertex` (TextureVertex + the
source rect in atlas texels + a nearest flag); `fs_tinted_texture` clamps
the sample coordinate to the source rect and, for nearest, snaps to the
texel centre - cpugfx's sampling rule, which it turns out clamps the
*coordinate* to the crop (not half a texel in) and then filters normally.
Cropped Linear stretches match cpugfx with 0 outliers; nearest matches
except at exact texel-boundary ties, where cpugfx's own pick depends on
float rounding of its scale, row by row (a 1/1024-texel bias agrees on
most; the test allows exactly the tie lines). Open question found on the
way: widening the SHARED TextureVertex (path fills use it too) by two
attributes made path fills vanish even with the old shaders - not
explained; giving images their own vertex type avoided it, and it matters
for the Metal device (a layout with attributes its vertex shader does not
read). Rotated bitmaps still go to the CPU.

GPU only for 1:1 integer-aligned or a Linear stretch of the **whole**
bitmap (`:1276-1320`). Cropped stretches (bilinear bleed past the crop),
nearest-neighbour, rotation, mirrored source rects all go to the CPU and
upload the destination rect every draw. In `fs_tinted_texture`: clamp UVs to
the source sub-rect (half a texel in), a point-sampling variant, and a
transformed quad for rotation. Three small shader edits.

### B5. Opacity brushes: a second texture, not a fallback

An opacity brush refuses the GPU everywhere (`:555-561`). It is a mask
texture multiplied into alpha — the same sampler slot B2 adds.

### B6. Gradients: stop re-evaluating 512 stops per draw

**Status 2026-10-25: DONE (same branches).** `Bitmap::gradientCache` keeps
each strip per (stop collection, brush opacity), the collection held by
reference; trimmed with the text cache. The atlas's 512-colour compare on
lookup remains. Not measured on its own.

`gradientStrip` (`:629-638`) calls `colorAt` 512 times per draw and compares
the strip against the atlas map every time; the upload is deduplicated, the
CPU work is not. Cache the strip on the `GradientstopCollection` (it is
immutable). Not a shader change, but it is the per-draw CPU cost that stays
after B1-B5. (Evaluating ≤8 stops in the shader is possible and removes the
strip entirely; the strip is the simpler win.)

### B7. Offscreen targets and blur

`createCompatibleRenderTarget` is always cpugfx (`:1533-1536`), so every
offscreen draw is CPU and `asCpuBitmap` reads GPU targets back. `fs_blur`,
`fs_small_blur`, `fs_mult_threshold` are compiled and unused; blurs go
through `CachedBlur` on locked pixels. Milestone 3 (GPU layers) unlocks
both: an offscreen is a layer, a blur is the existing post effect.

### B8. Paths are rebuilt every frame

Flattening is CPU (`Gfx_base.h:269`), transform per point is CPU (`:807-817`),
edge quads are rebuilt into transient buffers every submit
(`path.cpp:122-210`), and shapes are cleared each frame. A static document
re-accumulates every path every frame. Not a shader change either, but the
retained-pixels layer makes it mostly moot for *unchanged* regions; for a
scrolling or zooming view, retaining atlas entries keyed by
(geometry identity, transform) is the next step.

## Suggested order

1. **A2 + A3 + A4 - DONE 2026-10-25 (uncommitted, gmpi_ui branch
   `gpu-presenter`, on `vendor-bgfx`).** `gpugfx::WindowPresenter` in
   `GpuGfx.h` (ensureTarget / present / release / lastTiming); the Wayland
   `presentGpu` is now 20 lines (opaque region, viewport, one call) and the
   X11 one 10. `backends/gpu/platform/model_surface.{h,_linux.cpp,_mac.mm,
   _win.cpp}` replaces `windowless_context.{h,mm}` and the frames' hand-rolled
   model windows; `gpugfx::initialize(display, compositor)` makes it itself,
   and `Renderer::initializeWindowless` opens its own connection on Linux
   (Wayland then X11) so an offscreen canvas before any window no longer
   initialises bgfx on a null handle. `renderer.cpp`: one `kRenderer` per
   platform, one `resetFlags` expression (Windows still passes no
   vsync/sRGB flag, kept until a Windows build measures it); stale "D3D11"
   comment fixed. Verified: gimpi_ui_tests 264/264 NVIDIA, 263 + the known
   case lavapipe, on both the Wayland and the X11 branch of the model
   surface; SynthEdit 181/181 and SE Fluid presenting through the presenter;
   `DrawingFrameX11.cpp` compiles with the GPU define (no X11 host to run);
   the GPU Wayland demo presents. A5 (the windowless reset flags) is
   unchanged and still to test.
2. **The device seam - DONE 2026-10-25 (uncommitted, gmpi_ui `device-seam`,
   on `gpu-presenter`).** `backends/gpu/device/{device,device_bgfx,device_fwd}.h`:
   `gmpi::gpu::gfx`, 25 type aliases + 46 inline forwards, each with only the
   arguments the renderer passes; mechanical `bgfx::` → `gfx::` across the
   renderer (18 files, +350/-347); a configure-time check refuses `bgfx::`
   in renderer sources. Same results on every suite. Left bgfx-shaped on
   purpose: `BGFX_*` constants and `gfx::CallbackI`.
3. **B2 geometry clips** then **B1 text coverage atlas**: the two changes
   that turn "GPU path with CPU holes" into "GPU path". B1 is also the
   prerequisite for Mac/Windows to leave their native text rasterisers.
4. **B4, B5, B6**: small, each removes a class of per-draw uploads.
5. **A1**: D3D12 under `DrawingFrameWin`, Metal under `DrawingFrameMac`;
   retire `DirectXGfx`/`CocoaGfx` as renderers.
6. **B3, B7, B8** as the measurements say.

## What is already right

No platform `#if` in the adaptor; one `.sc` per shader for three APIs; one
renderer configured per platform and the rest not even vendored; exact
coverage in the atlas rather than MSAA; the shader-brush API lands user code
in the same write-once pipeline. The review is about finishing that design,
not changing it.
