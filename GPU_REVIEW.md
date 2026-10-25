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
  **DONE 2026-10-26:** the fallback fills the figures already widened
  (`cpugfx::RenderTarget::fillFlattenedFigures`); test
  GpuStrokeOnTheCpuPathMatchesCpu.
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

**Status 2026-10-26: figures DONE by tiling (uncommitted, gmpi_ui
`large-paths` on `modal-dialogs`; gimpi_ui_tests `large-paths`).** A figure
over the limit is cut (Sutherland-Hodgman, `clipPathToRect`) into tiles of
at most 512² over its VISIBLE part (bounds ∩ clip ∩ surface), each an
ordinary atlas entry; no shader or vertex change, exact (the cut's added
edges lie on tile sides, where the signed-area scheme makes them stand in
for what was cut). A per-frame budget of 4 Mpx of tiles keeps the atlas
bounded; past it the CPU path as before. Brushes are device-space, so
gradients and shader brushes need nothing per tile; tiles batch under a
GPU clip mask like any shape. `GMPI_UI_GPU_TILES=0` restores the old
behaviour; `Bitmap::cpuFallbacks` counts viaCpu draws. StructureBench: its
last 2 CPU fallbacks -> 0 (2 tiles each). The SDF route below is not taken.

**Geometry clips over the limit DONE 2026-10-26 (uncommitted, gmpi_ui
`large-clips` on `large-paths`; gimpi_ui_tests `large-clips`).** A clip
over 512² is one mask entry of only its visible part (`visibleClipMask`:
cut to bounds ∩ clip rect ∩ surface, charged to the same frame budget).
The clip rect narrows to the cut polygon's whole-pixel bounds: the cut
polygon can be far smaller than the rect it was cut to, and a fragment
past the mask's entry would sample a neighbouring entry (test
GpuHugeGeometryClipCutToVisible fails without it). A clip with nothing
visible empties the clip rect. Nested shaped clips are still CPU.

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

## Measured on real documents (2026-10-25)

Six real documents (Poly Synth, TideSynth DefaultRack, Thick2, Impulse
Response, StructureBench, WavetableSynth; structure view, 6 s each, NVIDIA):
after clips, text, bitmaps and the fix below, CPU fallbacks went 21 -> 2,
and both are one large figure (StructureBench, > 512^2 texels: B3). The
other 17 were one text draw per document - the browser pane's multi-line
block - refused by the text cache because its padding was the BLOCK's
height on every side instead of a line's (~1.9 Mpx, over the 1 Mpx limit);
fixed on `gpu-text-misc` (padding = ascent + descent + line gap from the
format; cache refusals now say why under GMPI_UI_GPU_TRACE_DRAWS; test
GpuTallTextBlockIsCachedAndComplete fails without the fix).

Not a regression, measured: a document that redraws at ~3 fps shows
submit+present climbing from 1 ms to a ~8.4 ms plateau within seconds. The
NVIDIA card drops from P0 (2490 MHz) to P8 (210 MHz) over the same seconds
(nvidia-smi); the same document at P0 is 1 ms. Same with clip masks off.

What is left by measurement: first frames cost 15-25 ms submit+present
(shader programs created on first use, atlases allocated and filled) and
7-25 ms render; B3 for the occasional large figure. Rich text (B1 tail) did
not appear in these documents.

## First-frame cost: pipeline compilation (2026-10-25)

Measured with NVIDIA's own shader cache off (`__GL_SHADER_DISK_CACHE=0`,
which is what a fresh install or the first run after a driver update
looks like): a cold first frame is ~74-94 ms of submit+present, against
~15-20 ms with the driver's cache - pipeline compilation is most of a cold
open. bgfx has a pipeline-cache callback (Vulkan and D3D12 use it); ours
returned nothing, and bgfx's Vulkan pipeline key included the render pass
HANDLE, so it could never hit anyway. Done on `pipeline-cache`
(uncommitted, gmpi_ui on `gpu-text-misc`): `gpu/renderer.cpp` keeps the
blobs on disk (per-user cache folder, one subfolder per renderer + GPU,
write-then-rename; `GMPI_UI_GPU_PIPELINE_CACHE` = folder or 0), and
`gmpi.patch` makes the key stable. Result, driver cache off: 94 -> 15 ms.
D3D12's key is unchecked (no Windows build). Not done: render time on a
first frame (7-25 ms) is CPU - text rasterisation on cache misses.

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

## State on main, 2026-10-26

Everything above marked DONE is on `main` (gmpi_ui, gimpi_ui_tests,
SynthEditLib), fast-forwarded from the branch stack. Measured: real
documents draw with no CPU fallbacks; StructureBench's steady frames are
~0.1 ms render, ~1 ms submit+present. Since the stack:

- **clear() under a CPU-only clip** (nested shaped clips, or over the tile
  budget) filled the clip's bounding box, and with an outer GPU mask the
  masked replace ignored the inner clip. Now cpugfx supplies the coverage
  and the GPU does the same erase/add as the masked replace (`viaCpu` split
  into `renderViaCpu` + `compositeCpuLayer`). With
  `GMPI_UI_GPU_CLIP_MASKS=0` every picture test matches cpugfx.
- **A6 stroke widened twice** on the CPU path: fixed (above).
- **X11 run for the first time**, on a private rootless Xwayland with no
  window manager (nothing reaches the screen): `tests/x11_modal_test.sh`
  (modal queue, 16 checks) and `tests/x11_present_test.sh` (CPU and GPU
  present; the GPU one presents on NVIDIA and lavapipe). That server gives
  back no window contents, so pixels are checked only under Xvfb.
- **CI**: `GmpiApiDrawing.h` now includes `<compare>` (MSVC / libc++), and
  the GPU tests sit inside their `GMPI_UI_TESTS_BACKEND_GPU` guard;
  gimpi_ui_tests CI green on macOS and Windows.

Left, and why:
- **A1** (D3D12 under DrawingFrameWin, Metal under DrawingFrameMac) and the
  D3D12 pipeline-cache key: need a Windows / Mac build. (Both done since:
  see the Windows and macOS sections below.)
- **B3 SDF route** for large rounded rects / circles: a decision for Jeff
  (hold D2D's flattened arcs or re-baseline to the analytic answer).
- **B4 rotation, B5 opacity brushes, B7 offscreen targets / blur, B8 path
  retention, nested shaped clips on the GPU**: no measured user. Offscreen
  targets are used only by CachedBlur, once per invalidate; nothing in the
  six documents nests shaped clips or rotates a bitmap.
- **xdg-dialog-v1 set_modal**: GNOME 46 here does not offer it, so it
  could not be tested.

## Windows / Direct3D 12, 2026-10-26 (first run)

Branches, none merged: gmpi_ui `d3d12-fixes` (on `dxbc-shaders`), gimpi_ui_tests
`d3d12-tests`, SynthEditLib `dxbc-fluid-shader`. Release, Ninja, VS 18 / MSVC 14.51.

- **Baseline** (`GMPI_UI_TESTS_GPU_BACKEND=OFF`): 310 tests, all pass (1 skipped).
- **DXBC shaders generated** (`shaders_dxbc.cpp`, `s_5_0`) from the vendored
  bgfx's shaderc, plus the test brush's and SE Fluid's `Dxbc` blobs (SPIR-V
  and Metal blobs regenerated byte-identical). `GMPI_GPU_BUILD_SHADERC=ON` used
  to be unusable before the file existed; it now warns instead.
- **GPU suite on D3D12: 353 of 364 pass.** Everything that compares against the
  Direct2D reference images passes, which covers `gl_FragCoord`'s top-left
  origin (clip masks), the R32F path atlas, and the 1/1024 nearest-neighbour
  bias. Of the 11 failures:
  - 9 `TextBaselineLowestPixel*` (125/175/200 %): `SRGBPixels` targets go to
    cpugfx, so these run on the CPU backend, and the test predicts Direct2D's
    half-DIP snap. cpugfx puts the baseline at ~16 *device* px at every DPI
    (31 expected, 15 measured at 200 %). Not a GPU issue; unexamined.
  - `CpuVsD2D.PaddedStrideWidth` (meanBad 13.5, limit 12) and
    `CpuVsD2D.RadialGradientBasic` (12.34, limit 12): these compare cpugfx with
    "Direct2D", which in a GPU build is the GPU renderer. Anti-aliasing-level
    differences, just over thresholds tuned for CPU vs D2D.
- **Three real GPU bugs found and fixed** (not D3D12 specific; the Linux suite
  does not contain these Windows-only tests): a mirrored (negative-scale) rect
  drew nothing; a rotated `drawBitmap`'s CPU patch used two corners and was too
  small; a rect at 1e7 lost its edge in the SDF quad.
- **Pipeline cache**: two runs against one `GMPI_UI_GPU_PIPELINE_CACHE` folder
  wrote the same 13 files, so the D3D12 key is stable across runs.
- **Direct2D opacity brush** (step 4): `fillGeometry` with an opacity brush draws
  nothing on a bitmap render target here, whatever the antialias mode, and
  `EndDraw` still says Ok. The test is committed `DISABLED_`; the one-word fix on
  main is therefore unverified.
- **A1, the present under `DrawingFrameWin`** (branch `win-gpu-present`): done for
  the native-HWND frame (`DxDrawingFrameHwnd`, which `gmpi::hosting::DrawingFrame`
  and the GMPI Windows wrappers use). With `GMPI_UI_GPU=1` in a build that defines
  `GMPI_UI_GPU_BACKEND`, `open()` takes `WindowPresenter` on the HWND instead of
  `CreateSwapPanel` (no D2D device, swap chain or DirectComposition target); the
  client gets the GPU factory through a `gpuFactory()` hook on `DxDrawingFrameBase`
  (not an override of `getDrawingFactory`, which SynthEditLib's `DrawingFrameBase2`
  overrides too); WM_SIZE and WM_PAINT feed the presenter; the HDR poll is skipped.
  Direct2D remains the default and the fallback. FormsDemo: `-DFORMSDEMO_GPU=ON`.
  Measured off-screen on FormsDemo (`PrintWindow` of the frame, nothing on screen):
  - Renders the whole UI; the window background (44) and field (56) are exactly
    Direct2D's, so a mid-grey presents as mid-grey with `resetFlags()` as it is.
    Mean difference 1.1 / 255 per channel over the frame; text is greyscale where
    Direct2D uses ClearType.
  - First frame 3-24 ms render + 21-28 ms submit+present (90 ms the very first
    time on the machine); a partial repaint 0.2-1.5 ms + 1.0-1.4 ms.
  - Cold vs warm app pipeline cache: 20.6 ms vs 22.0 ms, so no measurable gain
    here (the driver has its own cache); 7 cache files, the same on every run.
  - 0 `viaCpu` fallbacks over 50 draw ops.
  - **Not covered**: SynthEdit's own frames (`DrawingFrameBase2`, HostedView / WinUI3
    SwapChainPanel) and the GMPI VST3 wrapper do not start it; SE's SDK3 modules
    would need the GPU factory bridged. Untested: DPI change between monitors,
    interactive resize, a text edit overlay, HDR displays.
- Also needed: `std::min/max` in the GPU headers are now parenthesised, since
  `Windows.h` (without NOMINMAX) is included ahead of them in the frame.

## Where the GPU backend's CPU time goes (Windows profile, 2026-10-26)

`examples/WinBench` (Direct2D vs D3D12, offscreen, no vsync) showed the GPU backend
slower than Direct2D on everything but gradients, and almost all of its frame time
was CPU-side recording (cpu_ms ~ frame_ms). Profiled with VSDiagnostics + xperf
(RelWithDebInfo):

- **Rounded rects / ellipses / strokes (the "rects" scene)**: ~1/5 of the time was
  `PathAtlas::removePath` scanning `paths_` to find the entry to erase (O(n^2) when
  thousands expire per frame): fixed, swap-and-pop with a stored slot (5000 rects
  76 -> 59 ms). What is left is by design: every rounded rect is built as a geometry
  (heap churn: figure vectors, flattened arcs, widened stroke), packed into the path
  atlas by a skyline packer (~11 %, repacked as paths come and go) and rendered by
  coverage accumulation. Rects moving every frame are the worst case, since the atlas
  keeps paths that repeat. The structural fix is B3 (analytic SDF rounded rects and
  circles, no path, no atlas), which changes pixels against Direct2D.
- **Text**: a key per string, per 1/4-pixel phase in x AND y (16 variants), per
  opaque-colour luminance; each entry is the layout box as RGBA16F (22 KB for a
  140 x 20 label). A thousand strings of distinct colours moving every frame blow the
  64 MB budget and re-rasterise and re-upload each frame (~60 % of the time is the
  driver and kernel: texture uploads). `trimTextCache` also scanned for the oldest
  entry once per eviction (25 % of the profile): now one sort. A static UI is
  unaffected (one entry per string). Cheaper entries (R16F coverage, ink-sized
  boxes: noted above) and a coarser luminance key would cut it further.
- **Gradients**: `WinBench` drifts the bounce colours every frame by default, so every
  gradient is new and the strip texture is uploaded again (60 % driver and kernel).
  With `--static-colors` (colours reused, as in a UI) the GPU beats Direct2D on the
  same scene by 1.7x (4 stars), 2.3x (16) and 3.4x (64: 2.3 vs 7.7 ms).

## macOS / Metal, 2026-10-26

All on `main`, committed locally, not pushed: gmpi_ui `e6ec82c` (presenter fix,
sRGB swap chain), `562a23e` (A1), then MacBench and this note; gimpi_ui_tests
`242b37e`, `0646877`. iMac M1 (8 GB), macOS 26.3, built-in Retina 4.5K (2x,
Display P3), Xcode SDK 26.5, Release, Ninja.

- **Baseline** (`GMPI_UI_TESTS_GPU_BACKEND=OFF`, cpugfx against the D2D
  references): 233 tests, 230 pass, 3 skipped (styled text runs; two COLRv1
  emoji tests, no such font here), no failures. With the three tests below:
  236, 233 pass.
- **The GPU suite did not compile on macOS.** Tests written on Linux since the
  last Mac run used bare `Point`/`Rect`/`Size`, which CoreFoundation's
  MacTypes.h (through SavePng.h) makes ambiguous. Qualified (`242b37e`).
- **GPU suite on Metal: 289 tests, 286 pass, 3 skipped (the same three), no
  failures, under `MTL_DEBUG_LAYER=1` (no API misuse).** No regression from the
  Windows work. With the three tests below, and after A1's renderer changes:
  292, 289 pass. The four things to check:
  - R32F is renderable here (`ReportRendererCapabilities` now also logs the
    origin), so the path atlas is R32F, not the R16F fallback.
  - `caps->originBottomLeft` is false on Metal, as on D3D12 and Vulkan.
  - `gl_FragCoord` is top-left in `clip_mask.sh`: the clip-mask scenes match
    cpugfx, including a five-point star clipped to the top 36 rows
    (`GpuClipGeometryIntersectsRectClip`), which a flipped lookup would fail.
  - The 1/1024-texel nearest bias: `GpuBitmapCroppedAndNearestStretchesMatchCpu`
    passes.
- **The three Windows-found bugs, now tested everywhere** (`0646877`,
  `transform_edge_tests.cpp`: pixel assertions, no references):
  `FillUnderNegativeScaleIsMirrored`, `RotatedNearestBitmapKeepsEveryCorner`,
  `HugeRectKeepsItsFractionalEdge`. Pass on cpugfx and on Metal (the CPU
  backend agrees, nothing to report). With `9bf65f8` reverted, all three fail
  on Metal (mirrored rect: coverage 0; rotated bitmap: two corners white; 1e7
  rect: no fractional bottom edge), so they would have caught the bugs.
- **A1, the present under `DrawingFrameMac.mm`** (`562a23e`). With
  `GMPI_UI_GPU_BACKEND` and `GMPI_UI_GPU=1`, `initWithClient` starts the
  renderer before the client is attached (its first `getDrawingFactory` sees
  the GPU factory), makes the view layer-backed, and `updateLayer` presents
  through `WindowPresenter` to a `CAMetalLayer` **sublayer**. A `CAMetalLayer`
  returned from `makeBackingLayer` is never sent `updateLayer` (AppKit clears
  `needsDisplay` and draws nothing; found by measurement), so it sits inside an
  ordinary backing layer. `updateLayer` carries no rect, so the frame keeps its
  own dirty union; `open()` requests the first present through it, resize and
  a new client set `needsMeasure`, `viewDidChangeBackingProperties` marks all
  dirty and the next present takes the new scale. Fallback to CoreGraphics
  when `initialize()` fails. `GMPI_VIEW_VERSION` bumped to `_05`, plus a
  `_05_GPU` name, since a GPU build differs from a plain one in body and ivar
  layout. Without the define the file compiles as before (checked).
- **Two bugs found by the first Mac present** (`e6ec82c`):
  - **Mid-grey presented as 55, not 128.** bgfx's Metal swap chain ignores the
    format the renderer asks for (`FrameBufferMtl::create` has
    `BX_UNUSED(_format)`) and is `BGRA8Unorm`, so linear values went out
    unencoded. Windows gets an RGBA16F (scRGB) swap chain from the same request.
    macOS now takes the sRGB swap chain, as Linux does (`BGRA8Unorm_sRGB`,
    encoded in hardware), and the layer is tagged sRGB. Result: 128 -> 128, the
    field 56 vs CoreGraphics' 55-56, a black-to-white gradient's midpoint 188 vs
    188. Not done: the Metal plan's Acceptance 1 (an RGBA16Float drawable on an
    extended-linear-sRGB layer, no 8-bit anywhere) needs bgfx to honour the
    format, a `gmpi.patch` change; the sRGB 8-bit drawable is the interim.
  - **A partial repaint presented black on Retina.** `WindowPresenter` pushed
    the dirty-area clip (device pixels) under the transform the target still
    held from the last frame - the 2x scale - so the clip was at twice its place
    and nothing redrew the area. It now sets identity, pushes the clip, then
    sets the scale, every frame. Shared code: any host at a scale other than 1
    had it (Windows at 100% and X11 never set a transform, which hid it).
- **Proof without the screen**: `examples/MacPresent` opens the view through
  each backend in a borderless window beyond every display, reads the window
  back from the window server (`CGWindowListCreateImage` through `dlsym`: the
  macOS 15 SDK marks it obsolete, but a process may still capture its own
  windows without permission), compares, and closes everything. Exit 0 =
  match. 400x300 logical, 800x600 captured at 2x:
  - Mean per-channel difference **2.44 / 255** (max 201, at glyph edges).
    Background 128/128/128 both; field 55-56 vs 56; red, blue, gradient
    midpoint identical; a pixel inside the partial repaint 128 both.
  - The rest is text: CpuTextEngine's title sits ~3 logical px lower than Core
    Text's and is lighter (mean darkening of the title box 15.1 vs 17.7), as
    Windows saw against DirectWrite.
  - Resize down to 300x200 and back: the presenter measured and arranged at
    each size, and the frame still matches.
  - Clean under `MTL_DEBUG_LAYER=1`. 0 `viaCpu` fallbacks over 693 traced draw
    ops (text draws are not in that trace).
- **First frames.** Metal's own shader cache decides the first frame:
  - Cold (the first run after the build): render 9.7 ms, submit+present 504 ms.
  - Warm: render 2-10 ms, submit+present 1-5 ms; time from view creation to
    the first `render` 6-15 ms against CoreGraphics' 11-18 ms.
  - The app's pipeline cache (`GMPI_UI_GPU_PIPELINE_CACHE`) does nothing on
    Metal: bgfx's Metal renderer makes no `cacheRead`/`cacheWrite` calls, and
    the folder is never created. The OS cache is per user and shared by every
    command-line tool, so I did not clear it to repeat the cold number.
- **Partial repaint** (one 80x24 counter): render ~1.2 ms, submit+present
  median 0.6 ms over 60 back-to-back repaints.
- **vsync**: with `setVsync(true)` after `initialize()`, submit+present over the
  same 60 repaints was median 0.6 ms (max 34, the first frame), the same as
  without. That is an off-screen window, which may have no display to sync to,
  so whether vsync blocks the main thread on screen is **not verified**.
  `resetFlags()` on macOS is now `FLIP | SRGB_BACKBUFFER`, vsync off.
- **Retina**: the frame presents at the backing scale (2.0, 800x600 drawable
  for a 400x300 view). **Not verified**: a move to a display with another
  scale. This Mac has one display; the path (`viewDidChangeBackingProperties`
  -> dirty all -> the next present resizes to the new scale) is untested.
- **Not covered**: SynthEdit's own frames, `HostedView` and the VST3/AU
  wrappers do not start the GPU path (by design for now); a text edit overlay
  over the Metal layer; interactive (live) resize; EDR / HDR.

### Benchmark: Metal vs CoreGraphics (`examples/MacBench`)

`examples/MacBench` is WinBench for macOS: headless, offscreen, no vsync,
`--backend cg|gpu`, the same output. The scenes, the case list and the timing
loop moved to `examples/BenchScenes.h`, which WinBench now includes too
(WinBench is not rebuilt here: no Windows build on this Mac). 1000x700,
240 frames per case after 30 warm-up, ms per frame:

| scene load | CoreGraphics | Metal | Metal cpu_ms | Metal / CG |
|---|---|---|---|---|
| bounce 4 | 1.91 | 0.38 | 0.32 | 0.20 |
| bounce 16 | 4.85 | 2.36 | 2.28 | 0.49 |
| bounce 64 | 13.63 | 8.77 | 8.65 | 0.64 |
| lines 100 | 1.26 | 0.38 | 0.31 | 0.30 |
| lines 1000 | 8.60 | 4.14 | 3.98 | 0.48 |
| lines 5000 | 41.37 | 18.16 | 17.84 | 0.44 |
| rects 100 | 1.40 | 1.29 | 1.22 | 0.92 |
| rects 1000 | 10.92 | 10.17 | 10.12 | 0.93 |
| rects 5000 | 53.13 | 58.79 | 58.73 | 1.11 |
| ellipses 100 | 2.95 | 1.60 | 1.52 | 0.54 |
| ellipses 1000 | 25.66 | 11.81 | 11.74 | 0.46 |
| ellipses 5000 | 127.75 | 73.31 | 73.26 | 0.57 |
| text 50 | 1.52 | 0.95 | 0.89 | 0.62 |
| text 200 | 5.07 | 3.60 | 3.54 | 0.71 |
| text 1000 | 22.97 | 84.97 | 84.85 | 3.70 |
| gradients 50 | 2.33 | 0.76 | 0.67 | 0.33 |
| gradients 200 | 7.95 | 2.33 | 2.23 | 0.29 |
| gradients 1000 | 37.38 | 13.42 | 13.31 | 0.36 |

**Metal does not behave like D3D12 did against Direct2D.** It is faster than
CoreGraphics in 15 of 18 cases (gradients and lines ~2-3x, small loads up to
5x), because CoreGraphics is the slower native renderer: rects 5000 cost 53 ms
here against Direct2D's 17. What Metal shares with D3D12 is the shape of the
cost: **cpu_ms is within 0.1-0.2 ms of frame_ms in every case**, so the GPU
time is CPU-side recording and the device is idle. The two losses, profiled
with `sample` (12 s each, main thread ~9.1k samples):

- **text 1000 (85 ms, 3.7x CoreGraphics)**: half the main thread is
  `endDraw`, and 37% is `Bitmap::trimTextCache()`. Each opaque label's cache
  key includes its colour's luminance (`coverageClass`), and the scene gives
  each of 1000 labels its own hue, so 1000 classes x the subpixel phases
  overflow the 64 MB budget every frame. The over-budget path then evicts by
  rescanning the whole map for the oldest entry, once per eviction ("Rare, so a
  scan is fine"): quadratic. `drawTextU` is 27% (cache lookups 8%, rasterising
  misses 5%, coverage 3%), `Canvas::submit` 11%. Two fixes suggest
  themselves: evict in one pass (sort or nth_element by `lastUsed`), and key
  coverage without the colour where the coverage does not depend on it. Very
  likely the same cause as Windows' text 1000 (318 ms).
- **rects 5000 (59 ms, 1.11x)**: the scene is rounded rectangles, which the
  adaptor draws as geometry, not SDF: `drawRoundedRectangle` 34% (arcs
  flattened, the stroke widened on the CPU, then the path atlas),
  `fillRoundedRectangle` 17%, `endDraw` 15%, of which `PathAtlas::removePath`
  and `stbrp_pack_rects` (packing the atlas afresh each frame) are most;
  malloc/free throughout. This is the B3 SDF route for rounded rects (a
  decision for Jeff) plus B8 (paths rebuilt every frame).


## What is already right

No platform `#if` in the adaptor; one `.sc` per shader for three APIs; one
renderer configured per platform and the rest not even vendored; exact
coverage in the atlas rather than MSAA; the shader-brush API lands user code
in the same write-once pipeline. The review is about finishing that design,
not changing it.
