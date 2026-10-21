# Visage GPU backend plan

Status: PROPOSAL v2, 2026-10-22. Milestone 0 DONE (see "Measured"). Surveyed
Visage at upstream `7654e32` (2026-10-10) and gmpi_ui at `4d2e73a`.

A GPU-accelerated GMPI-UI backend built on a fork of
[VitalAudio/visage](https://github.com/VitalAudio/visage) (MIT, bgfx underneath:
D3D11 / Metal / Vulkan / WebGL). Primary target is **Linux**, which today has only
the CPU rasteriser (`backends/CpuGfx.h`) and presents through wl_shm / XShm.
Windows and macOS keep Direct2D and CoreGraphics; the GPU backend is a
*second* GPU path there, useful for one-rendering-everywhere consistency but not
required to ship.

Rulings from Jeff, 2026-10-22:

- **Visage is the starting point, not the product.** The fork is gmpi-ui's GPU
  renderer and is reshaped freely: namespace, file layout, dropped subsystems,
  changed semantics. There is no obligation to stay rebase-able on upstream or
  to send changes back. MIT copyright notices stay on the files they came from.
  Fork: `JeffMcClintock/visage`, branch `gmpi`, checkout at `/home/jef/SE/visage`.
- **GPU shaders are a future gmpi-ui feature.** Visage's write-once shader
  pipeline (shaderc transpile to D3D/Metal/SPIR-V, `ShaderCache`, custom post
  effects, live reload) is kept intact, not trimmed, so module authors can get
  user-authored shader effects through a new GMPI interface later (milestone 8).

Guiding principles:

- **Half-float is the default.** Every surface the backend owns — intermediate
  layers, offscreen render targets, the swap chain where the driver allows it —
  is **RGBA16F, premultiplied, linear-light scRGB**, exactly the working format
  decided for the CPU backend (PLAN.md "Pixel format"). 8-bit appears in one
  place only: the final encode to a display that cannot take fp16.
- **GMPI glue stays in gmpi_ui.** `gmpi_ui/backends/GpuGfx.*` implements the
  GMPI interfaces over the fork's Canvas/Layer/Path; the fork does not know
  about GMPI types. That keeps the fork a plain graphics library with its own
  tests, and the backend a thin adaptor like `CpuGfx.h` and `DirectXGfx.h`.
- **The public ABI does not move.** This is a new implementation behind the
  existing `IFactory` / `IDeviceContext` vtables. No virtual is appended
  (see the hazard comment at `GmpiApiDrawing.h:781`). Any new host capability is
  a new interface probed via `queryInterface`.
- **The D2D reference images are the spec.** `gimpi_ui_tests` runs against the
  new backend from milestone 1; the backend is done when it passes at the same
  tolerances the CPU backend does.

## Decisions (v1, open to review)

1. **Separate repo, fetched by CPM** pinned to a commit (same pattern as
   `SE16/CMakeLists.txt:98`), with a folder override for development like
   `GMPI_UI_FOLDER_OVERRIDE`. Only `visage_graphics`, `visage_utils` and
   `visage_file_embed` are built; `visage_ui`, `visage_windowing`, `visage_app`
   and `visage_widgets` are deleted from the fork once the GMPI backend no
   longer needs their examples for reference (we have our own windowing and
   widgets). `visage_graphics` depends on nothing but `visage_utils` +
   `visage_file_embed` (verified).
2. **Text comes from `CpuTextEngine`, not Visage.** Visage's text is
   FreeType-only, advance-based layout, no shaping, no kerning, no system font
   lookup, single-codepoint emoji only (`font.cpp`, `emoji.*`). GMPI's engine
   (HarfBuzz, fallback, CJK line breaking, grapheme clusters, bitmap + COLRv1
   emoji) is strictly more capable and already emits glyphs as path geometry
   through the public `IDeviceContext`. Consequence: the fork drops FreeType,
   `font.cpp`, `emoji.*`, `svg.cpp`, `theme.*`, `palette.*` — fewer fetched
   dependencies, ~4k lines less to carry. (Not done in milestone 0; the stock
   tree builds and the trim is a reshaping step for milestone 1.)
3. **CPU-readable targets are rendered by cpugfx, not read back from the GPU.**
   `createCompatibleRenderTarget(Mask|CpuReadable)` + `lockPixels` is used by
   `CachedBlur`, `BitmapMask::applyMask` and `AnimatedBitmap` to rasterise a
   small mask, blur it on the CPU, then re-upload. bgfx readback is asynchronous
   (`bgfx::readTexture` completes two `frame()`s later), so a synchronous
   `lockPixels` would stall the whole pipeline. Instead a `CpuReadable` target is
   a `cpugfx::RenderTarget` (same fp16 linear format, already on every platform);
   its `getBitmap()` becomes a Visage texture on first draw. GPU-only targets
   (no `CpuReadable`) fail `lockPixels` with an error, as D2D does for
   non-CPU-readable bitmaps. Alternative kept in reserve: readback with a
   forced `frame()` stall, if a real workload needs it.
4. **Gradient strips are evaluated by the CPU backend's stop code.** Visage
   samples a 512-texel RGBA16F strip per gradient (`gradient.cpp:139`), which is
   exactly a LUT. Filling that strip with `cpugfx`'s stop interpolation
   (straight linear colour, D2D `STRAIGHT` mode, all three extend modes —
   PLAN.md milestone 5) makes gradient semantics identical across the two
   backends by construction.
5. **Full affine transforms are applied CPU-side.** Visage's canvas knows only
   translate + uniform scale (`canvas.h:50-60`); `Path::transform` applies an
   affine to flattened points. Under a transform that is not axis-aligned, every
   primitive is routed through path geometry (which `Gfx_base.h`
   `GraphicsContextT` already does) and transformed before submission. Under an
   axis-aligned transform the SDF primitives (rounded rect, ellipse) are used as
   a fast path. Text, bitmaps and gradients under rotation/skew need the fork's
   quads to carry a 2×3 matrix (see "Fork changes").
6. **Clip geometry = layer + mask, v1.** Visage clips only axis-aligned rects
   (`shapes.h:54-72`). `pushClipGeometry` renders the clip into a region's
   intermediate layer via the `MaskAdd` blend mode, draws into that layer, and
   composites it. Correct but allocates a layer per push; v2 is a coverage mask
   sampled in the shape shaders (one extra texture fetch), which also removes
   the layer-per-clip cost for nested clips.
7. **Linux present = bgfx swap chain on the Wayland/X11 surface.** Replaces the
   per-frame `createCpuRenderTarget` → `encodeDirtyRect` → `wl_shm` path in
   `DrawingFrameWayland.h:991-1063` and the XShm path in `DrawingFrameX11.cpp`.
   The CPU path stays as the fallback when `bgfx::init` fails (no Vulkan ICD) —
   a runtime choice, same shape as `UniversalFactory::cpu` on Windows.
8. **Dirty-rect redraw maps onto Visage's persistent layer.** The host unions
   invalidations into one rect (`DrawingFrameWayland.h:377`). Visage keeps
   layer 1 (the content layer) between frames and only re-renders invalidated
   regions, then always blits layer 1 to the swap chain (`canvas.cpp:45,94`).
   That matches our model: `client->render` with the dirty rect as clip into the
   RGBA16F content layer; the composite pass is a full-screen copy plus, when
   the swap chain is 8-bit, the linear→sRGB encode with dither.

## Half-float: what has to change

Visage today (verified):

| Surface | Upstream | Required |
|---|---|---|
| Layer / content framebuffer | RGBA8 (`layer.cpp:34,174`) | **RGBA16F** |
| HDR mode | RGB10A2 with `kHdrColorMultiplier = 1/4` (`graphics_utils.h:56`) | removed — fp16 covers it; multiplier = 1 |
| Window swap chain | bgfx default (RGBA8), `resolution.format` never set (`renderer.cpp:104-157`) | RGBA16F when the renderer offers it, else RGBA8 + encode pass |
| Headless readback texture | RGBA8, hard-coded (`layer.cpp:185`) | RGBA16F |
| Gradient atlas | RGBA16F already (`gradient.cpp:139`) | keep; feed linear values |
| Path coverage atlas | R16F (`path.cpp:839`) | keep |
| Image atlas | RGBA8 (`image.cpp:58`) | RGBA8 **sRGB** sampler flag, so sampling yields linear |
| Blur / bloom buffers | inherit layer format (`post_effects.cpp:139`) | inherit → fp16 for free |
| Screenshot / `screenShot` callback | 4 bytes/px, BGRA swap (`screenshot.h:36`, `renderer.cpp:51`) | fp16 → `encode` via `CpuEncode.h` |

Blending: Visage blends **non-premultiplied in gamma space** (`SRC_ALPHA /
INV_SRC_ALPHA`, `shape_batcher.cpp:38-67`). GMPI is premultiplied linear. The
fork switches every colour blend to `ONE / INV_SRC_ALPHA` and premultiplies in the
vertex/fragment shaders (`shader_utils.sh`). Gradient texels must be stored
premultiplied too, or premultiplied after sampling — pick after-sampling so the
strip stays straight-colour and matches decision 4.

Swap-chain support per renderer, read from bgfx `v1.129.8958-499` source
(milestone 0):

- D3D11: `createFrameBuffer(nwh, …, RGBA16F)` is honoured as-is
  (`renderer_d3d11.cpp:4925`) and `dxgi.cpp:645` tags the swap chain
  `DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709` — linear scRGB, identical to the
  D2D backend's `R16G16B16A16_FLOAT` flip-model chain (`DrawingFrameWin.h:211`).
  Mirror the D2D backend's 512 MB VRAM + `CheckFormatSupport` gate.
- Vulkan (Linux): `SwapChainVK::findSurfaceFormat` (`renderer_vk.cpp:7718`)
  honours the requested format **but only with `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`**,
  hard-coded at `:7295`. Drivers expose `R16G16B16A16_SFLOAT` surfaces only
  with `EXTENDED_SRGB_LINEAR_EXT`, so today bgfx silently falls back to BGRA8
  (trace: "Surface format RGBA16F not found! Defaulting to BGRA8"). Fork
  change: accept `EXTENDED_SRGB_LINEAR_EXT` for float formats. Even then,
  Wayland and X11 WSI on Mesa rarely offer float surfaces, so the Linux
  default is the fp16 content layer + an encode (linear→sRGB + dither) pass
  into an 8-bit swap chain. Nothing is lost: the swap chain only ever receives
  the composite blit.
- Metal: `CAMetalLayer.pixelFormat = RGBA16Float` + extended-range colour
  space; nothing configures this today (`windowing_macos.mm:92`).
- WebGL: not a target. Float blending unchecked there — ignore.

Caps checked at init: `RGBA16F & (FRAMEBUFFER|TEXTURE_2D)`, `R16F` (path atlas
already assumes it), blit + readback (already checked at `layer.cpp:181`). If
RGBA16F framebuffers are unsupported the backend refuses to initialise and the
host falls back to cpugfx — there is no 8-bit-layer mode.

## What already exists (verified) and gets reused

- `backends/Gfx_base.h`: `generic_graphics::GeometrySink` (arcs → cubics →
  lines, with the x/y radius fix from PLAN.md milestone 2), `GraphicsContextT`
  (rects/ellipses/lines through path geometry), `StrokeStyle`. The Visage
  backend derives from these like cpugfx does.
- `helpers/DecodeImage.h` (`decodeImageFile`/`decodeImageMemory` → straight
  RGBA8 sRGB), `helpers/SavePng.h`, `ImageCache`: backend-agnostic.
- `backends/CpuEncode.h`: fp16 → 8-bit sRGB with blue-noise dither. Reused for
  screenshots and, as a shader port, for the 8-bit swap-chain composite pass.
- `helpers/CpuTextEngine.h`: on a non-cpugfx context it falls through to the
  plain-geometry path (`:1503`, `:2206`). Slow (re-flattens glyph outlines every
  frame) but correct; milestone 6 fixes the speed.
- `modules/se_sdk3_hosting/GmpiCpuUniversalContext.h`: SDK3 bridge over any
  `IDeviceContext` — works unchanged.
- `gimpi_ui_tests`: `DrawingTestContext_cpu.cpp` is the template for
  `DrawingTestContext_visage.cpp`; fixture renders 64×64 at `kRenderFlags=0`
  (fp16) and compares after sRGB encode, tolerances 0–20, NCC 0.85 for text.
- Visage itself: analytic path coverage into an R16F atlas (`path.cpp:691-822`,
  conservative raster when available), CPU stroker with all caps/joins/dashes/
  miter limit (`path.cpp:413-603`), SDF primitives, shape auto-batching with
  overlap detection (`shape_batcher.h:324`), per-region invalid rects, blur /
  bloom chains, one Canvas per native window with its own swap chain
  (`examples/BringYourOwnWindow`, `examples/MultiWindow`).

## Fork changes (branch `gmpi`)

1. **RGBA16F layers by default — DONE (milestone 0).**
   `Renderer::setHalfFloatFrameBuffers()` (default on; `initialize()` clears
   it with an error message if `RGBA16F` lacks `FRAMEBUFFER|TEXTURE_2D` caps),
   honoured in `Layer::checkFrameBuffer` for layers and swap chains and for
   the headless read-back texture (blit needs matching formats). The RGB10A2
   range hack is bypassed when float: `kHdrColorMultiplier`/`kHdrColorRange`
   constants became `hdrColorMultiplier()`/`hdrColorRange()` returning 1 on
   float layers (`graphics_utils.h`, `shape_batcher.cpp`, `post_effects.cpp`).
   `Screenshot` grew a half-float raw buffer with lazy fp16→8-bit resolve and
   `halfData()` for callers that want the real values.
2. Drop FreeType, font/emoji/svg/theme/palette (decision 2); keep shaders,
   post effects, `ShaderCache` (shader ruling).
3. Build `shaderc` from bgfx.cmake (`BGFX_BUILD_TOOLS=ON`, tools only) instead
   of the committed x86-64 binaries in `visage_graphics/bin` (29 MB; no
   aarch64 Linux).
4. `BGFX_WITH_WAYLAND=ON` and `platformData.type = Wayland` when given a
   `wl_surface*`; keep the X11 path. Vulkan swap chain: accept
   `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT` for float formats (bgfx patch,
   carried in the fork's bgfx fetch as a patch step or a bgfx fork).
5. Premultiplied-alpha blend states + shader premultiply; linear colour in.
6. View-id base per Canvas and a host-driven `bgfx::frame()` so several
   Canvases (main window, popups, tooltips) submit within one frame instead of
   each calling `frame()` (`canvas.cpp:72-111`). The MultiWindow example only
   works today because each submit is a whole frame.
7. 2×3 matrix on textured/gradient quads (images, gradient fills, text-mask
   quads) so rotated/skewed draws don't have to fall back to path geometry.
8. Image draw with source sub-rect, non-atlas "standalone" textures for large
   bitmaps and bitmap brushes (wrap/mirror can't tile inside an atlas), sRGB
   sampling flag.
9. Bitmap-brush fill shader (fill geometry with a tiled texture + brush
   transform + extend modes).
10. Coverage-mask clip in shape shaders (decision 6, v2).
11. Rename: `visage::` → a gmpi-owned namespace, `visage_graphics/` → the
    fork's own layout, once the GMPI backend compiles against it (so the
    rename is one mechanical commit, not a moving target during milestone 1).

Everything else — the `IFactory`/`IDeviceContext` classes, brushes, stroke
styles, path geometry wrappers, the cpugfx bridge for `CpuReadable` targets,
window/present code — lives in gmpi_ui.

## Measured (milestone 0, 2026-10-22, this VM: VMware SVGA II, lavapipe, g++ 13)

- Stock upstream `7654e32` configures in 91 s (fetches bgfx.cmake
  `v1.129.8958-499`, FreeType 2.14.1, Catch2, CLAP) and builds 563 targets
  clean with Ninja. `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`
  makes lavapipe the device; all 44 upstream graphics tests pass headless,
  including the visual-validation tests that read pixels back. The windowed
  examples (`ExampleBasic`, `ExampleBringYourOwnWindow`) run on X11/Xwayland
  under lavapipe.
- RGBA16F default (fork change 1): 44/44 upstream tests still pass. Three new
  tests in `visage_graphics/tests/precision_tests.cpp` prove the format rather
  than trust it: the default is half-float; 300 passes of white at alpha 1/512
  accumulate to ~112/255 (an RGBA8 layer rounds every pass to 0 — verified by
  flipping the default: reads exactly 0); a colour with HDR multiplier 2.5
  reads back as 2.5 through `halfData()` and clamps to 0xff in the 8-bit view.
- Upstream race found: `Layer::submit` called `bgfx::readTexture` and pumped
  one frame, but `readTexture` returns the frame number when data is ready
  (two frames later). 1 in 4 full-suite runs sampled an unfilled buffer
  (`canvas_tests.cpp:283`). Fixed by pumping `bgfx::frame()` until that frame:
  0 failures in 8 runs. Any `lockPixels`-style readback the GMPI backend does
  must use the same wait.

Milestone 1 (2026-10-22), GPU backend through `gimpi_ui_tests`:

- **76 / 144 pass** (`DrawingTest.*` minus the 5 `Blur*` cases, which lock an
  unflagged target ReadWrite — see landmines). Passing: clear, transparent
  fills/overlap, clips (basic, nested), all four transform tests, ellipse and
  rounded-rect fills, boundary/degenerate cases, and the text tests that only
  need shapes (e.g. TransparentText). Failing, by cause: strokes (milestone 2:
  `drawGeometry`/`drawLine` are NoSupport), gradients (2), fill-rule and
  diagonal-edge AA (2, below), bitmaps and bitmap brushes (3), text metrics /
  multiline (6, AA-sensitive), SVG (needs strokes + gradients).
- **Diagonal edges are eroded** (`fillModeAlternateStar`: 546 px differ,
  mean 38; the star is ~1 px thinner on every slanted edge while axis-aligned
  edges are exact). lavapipe reports no `BGFX_CAPS_CONSERVATIVE_RASTER`, so
  the fork's non-conservative edge-triangle fallback runs, and it
  under-covers. **Metal has no conservative rasterisation either**, so that
  fallback is what macOS always gets: milestone 2 must make it exact (expand
  each edge triangle by a pixel and let the signed-area shader do the
  clipping, or rasterise coverage the way cpugfx does and upload). One stray
  corner pixel in `fillRectangle` ((56,55), 25% coverage) is the same family.
- **NaN from an uncleared atlas.** Intermittent 1–3 px alpha-zero holes along
  the path quad's border rows/columns, only after other canvases had lived
  and died in the process, fp16 layers only, not fixed by single-threaded
  lavapipe. Cause: the R16F path atlas is created without a clear; only each
  path's own rect is cleared, but the sample quad has a 1 px buffer and
  LINEAR filtering reads the neighbouring texels. Reused GPU memory holds
  NaN bit patterns; `fs_sample_path` makes NaN alpha; an fp16 blend keeps NaN
  (an RGBA8 blend flushes it to 0, which is why upstream never saw it) and
  the read-back resolves NaN to 0 — a black, transparent pixel. Fixed by one
  full `BGFX_CLEAR_COLOR` on a freshly created atlas framebuffer (and the same
  for every Layer framebuffer, `fresh_frame_buffer_`): 0 failures in 12 fork
  suite runs, from 4 in 10. Rule for the fork: **never create a float render
  target without clearing it whole.**
- **bgfx runs a view's blits before its draws** (`renderer_vk.cpp:8836`,
  inside the view-change block). Upstream blitted the screenshot at
  `submit_pass`, so it captured the composite from the *previous* frame and
  only worked because `takeScreenshot` re-submits. The fork blits at
  `submit_pass + 1`.
- Upstream leaked the read-back texture once per Layer
  (`destroyFrameBuffer` only destroyed the frame buffer); fixed. A frame buffer
  cannot be built from a `READ_BACK` texture (lavapipe faults in
  `vkCmdBeginRenderPass`), so the blit copy stays.
- The `gimpi_ui_tests` fixture **overwrote 13 reference PNGs** on the first
  GPU run: `loadImageU` returned NoSupport, the fixture took that as "no
  reference yet" and saved the candidate over the D2D truth. Restored from
  git; the fixture now refuses to replace a reference that exists but will
  not load (`DrawingTestFixture.h`, "Reference exists but could not be
  loaded").

## Landmines

- **No real GPU in the dev VM.** `lspci` shows VMware SVGA II; ICDs present are
  lavapipe (`lvp`), virtio, etc. All Linux work here runs on **lavapipe**
  (software Vulkan). Correctness only; performance numbers need bare metal or
  Windows/macOS. Also means headless CI can use lavapipe — set
  `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`.
- **Single global bgfx context.** `Renderer::instance()` is a singleton; bgfx
  must be initialised once per process with a native display before any Canvas.
  In a plugin that is the *host's* process — two SynthEdit-based plugins in one
  DAW must share one bgfx, and a non-SE plugin that also uses bgfx (Vital!)
  would collide on bgfx's own globals. Mitigation: static-link bgfx with hidden
  visibility and a per-module init refcount; verify two instances of the VST3
  in Ardour and `vst3-validator`.
- **`bgfx::frame()` presents every swap chain.** With per-Canvas frames,
  popups cause extra presents of the main window; fork change 6 fixes it.
- **Vulkan on a Wayland subsurface** (VST3 3.8 editor path,
  `WaylandSubsurfaceFrame`): needs the parent's `wl_display` and our own
  `wl_surface`; frame callbacks still pace us. Unverified until milestone 4.
- **X11 plugin embedding**: the host owns the event loop; we present from
  `onTimer()`. A Vulkan swap chain on a foreign (reparented) window works on
  Mesa but has had resize bugs; keep `XShm` fallback until proven.
- **Stroke re-flattened every draw** (`path.cpp`): cache widened outlines on
  the `PathGeometry` object keyed by (stroke style, width, transform scale),
  as the CPU backend does.
- **16-bit indices** (`shape_batcher.cpp:122`): one shape = one quad, so this
  limits a batch to 16k shapes — but a flattened path becomes many edge
  triangles in the atlas pass; measure on a 2k-segment glyph-heavy frame.
- **Conservative rasterisation** is used for path coverage when available;
  lavapipe and WARP differ from hardware here. Expect a few code values of
  AA difference between CI and desktop — tolerances exist for this.
- **`CachedBlur` branches on bytes-per-pixel** (16 = float, 8 = half); the
  cpugfx-backed readable target reports RGBA_16f, same as today on Linux.
- **bgfx readback is asynchronous.** `readTexture` returns the frame number
  when the data lands; pump `bgfx::frame()` to it before touching the buffer
  (fixed in `Layer::submit`; see "Measured"). Applies to any future GPU
  `lockPixels`.
- **`createCpuRenderTarget` targets are writable by contract** ("force-ORs
  CpuReadable"): `CachedBlur`, `BitmapMask::applyMask` and the Blur tests lock
  an unflagged target ReadWrite. The skeleton's `lockPixels` is read-only
  (NoSupport otherwise), and the `Drawing.h` wrapper does not null-check, so
  those crash rather than fail. Milestone 3: on a write-lock release, upload
  the modified fp16 buffer as a texture and draw it Opaque over the target —
  which needs fp16 raw textures in the fork (upstream `Image` is RGBA8 only).
- **Float render targets start as garbage.** Clear every new RGBA16F/R16F
  framebuffer in full once; NaN survives fp16 blending and sampling. See
  "Measured", milestone 1.
- **CPM/offline builds**: bgfx.cmake pulls bgfx + bx + bimg (~150 MB of
  git). Pin the tag, let CPM cache it (`SE16/CMakeLists.txt:50` anchoring note).
- **ABI**: nothing here needs a vtable change. If a "present via GPU" host hook
  turns out necessary it is a new interface, not an append — see
  `GmpiApiDrawing.h:781`.

## Milestones

Each is verifiable on its own; nothing lands in SE16 before milestone 4.

0. **Spike — DONE (Oct 2026):** fork; build with lavapipe; BringYourOwnWindow
   on an X11 window; layers RGBA16F by default with precision tests; swap-chain
   format answered from bgfx source (D3D11 yes, Vulkan needs the colour-space
   patch); readback race fixed. See "Measured". Deferred to milestone 1: the
   font/svg trim (decision 2).
1. **Walking skeleton — DONE (Oct 2026):** `backends/GpuGfx.h` — factory,
   solid brush, `clear`, `fillGeometry` through the fork's path atlas,
   rect/rounded-rect/ellipse via `Gfx_base` routing, axis-aligned clips,
   full affine transforms (CPU-side on the flattened figures), text through
   `CpuTextEngine`'s plain-geometry fallback, fp16 read-back `lockPixels`.
   Geometry, stroke styles and brushes are the cpugfx classes; flagged
   (Mask/SRGB/CpuReadable) targets, images and compatible render targets go
   to an embedded `cpugfx::Factory`. `gimpi_ui_tests` gained
   `DrawingTestContext_gpu.cpp` behind `-DGMPI_UI_TESTS_GPU_BACKEND=ON
   -DGMPI_GPU_GRAPHICS_FOLDER=<fork>`; build dir `/home/jef/SE/build-uitests-gpu`.
   **76 / 144 DrawingTest cases pass** headless under lavapipe; see "Measured".
2. **Strokes, gradients, clips:** stroke styles mapped to Visage's stroker
   (verify dash offset and miter semantics against the D2D references;
   `cpu_vs_d2d_tests.cpp` is the pattern), gradient strips from cpugfx stop
   code, axis-aligned clip → `ClampBounds`, `pushClipGeometry` via mask layer.
   Target: `tests/` pixel battery parity with the CPU backend except text.
3. **Bitmaps and offscreens:** `loadImageU`/`createImage` → textures (sRGB
   sampled), `drawBitmap` sub-rects + interpolation modes, bitmap brush,
   `createCompatibleRenderTarget` (default RGBA16F GPU layer; `CpuReadable` →
   cpugfx bridge), `CachedBlur` drop shadows render. Fork changes 7–9.
4. **Present on Linux:** `DrawingFrameWayland` and `DrawingFrameX11` grow a
   GPU present path: bgfx swap chain on the surface, dirty rect → Visage
   invalidation, frame-callback pacing, fallback to the shm path when bgfx
   fails to init. SynthEditWayland editor and the VST3 subsurface editor run on
   it; `gmpi_ui/tests/run.sh` headless Wayland tests pass under lavapipe.
   Fork changes 3 and 6.
5. **Multi-window and lifetime:** popups, tooltips, dialogs as extra Canvases
   in one frame; editor open/close/reopen 100× with no bgfx handle leak
   (`bgfx::getStats()` counters); two plugin instances in Ardour; headless
   Ardour recording recipe as the regression check.
6. **Text speed:** glyph coverage cached in Visage's path atlas (or a dedicated
   R8 glyph atlas fed by `CpuTextEngine`'s existing atlas code) so a text-heavy
   frame doesn't re-rasterise outlines. Target: text tests pass at tol 2 /
   NCC 0.85, and a 500-label frame renders in < 2 ms on a real GPU.
7. **Windows / macOS (optional):** same backend under D3D11 / Metal behind the
   existing `m_disable_gpu`/software-renderer switch
   (`SoftwareRendererOption.h`), as a third option. Lets the D2D references be
   compared against Visage on the machine that defines them.
8. **User shaders (future gmpi-ui feature):** expose the fork's shader
   pipeline to module authors — a new interface (own guid, `queryInterface`)
   that takes shader source written once in bgfx's `.sc` dialect and applies
   it as a post effect over a region or as a custom fill, with uniforms set
   from the module. The transpile step is shaderc at build time today; a
   runtime path needs shaderc as a library or precompiled per-platform blobs
   shipped in the module bundle. Decide after milestone 4, when the present
   path exists to hang it on. Prerequisite is only that fork changes keep
   `ShaderCache`/`ProgramCache` and the post-effect machinery alive.

## Testing

- `gimpi_ui_tests` with `GMPI_UI_TESTS_BACKEND=visage` (new option beside the
  CPU/native switch in `DrawingTestContext.h`), run under lavapipe in CI and on
  a real GPU locally. Same reference images, same tolerances; any test needing
  a looser tolerance gets a one-line reason in this file.
- Cross-backend battery: extend `cpu_vs_d2d_tests.cpp` to cpu-vs-visage, so a
  divergence shows as a diff image rather than a reference mismatch.
- Format assertions: a test that locks a default offscreen target and checks
  `getPixelFormat() == RGBA_16f`; a test that renders a 0.001-step gradient and
  confirms no banding after encode (fp16 vs 8-bit intermediate is visible in
  the histogram).
- Lifetime: `bgfx::getStats()` texture/framebuffer counts equal before and
  after 100 editor open/close cycles.
- Visual sign-off on bare metal: Windows (D3D11) and macOS (Metal) before
  milestone 7 is called done; Linux on a machine with a real Vulkan driver
  before milestone 4 is.
