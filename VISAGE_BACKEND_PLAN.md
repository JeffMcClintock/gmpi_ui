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
  *Absorbed into gmpi_ui's `backends/gpu/` on 2026-10-23 (see "Absorbing
  Visage into gmpi_ui", Result); the fork is history now, not a dependency.
  Build lines below that pass `GMPI_GPU_GRAPHICS_FOLDER` or
  `SE_GPU_GRAPHICS_FOLDER` describe how milestones were run at the time.*
- **GPU shaders are a future gmpi-ui feature.** Visage's write-once shader
  pipeline (shaderc transpile to D3D/Metal/SPIR-V, `ShaderCache`, custom post
  effects, live reload) is kept intact, not trimmed, so module authors can get
  user-authored shader effects through a new GMPI interface later (milestone 8).

Ruling from Jeff, 2026-10-23 (supersedes decision 1 and the "GMPI glue stays
in gmpi_ui" principle below):

- **Visage is absorbed into gmpi_ui, not a dependency.** The renderer becomes
  gmpi_ui source, customised only for gmpi_ui. Code is reused **where
  pragmatic**: keep what is better than what gmpi_ui has, use gmpi_ui's own
  code where Visage duplicates it, and never carry a subsystem just because it
  came with the fork. The fork stops being a library with its own consumers.
  See "Absorbing Visage into gmpi_ui".

Guiding principles:

- **Half-float is the default.** Every surface the backend owns — intermediate
  layers, offscreen render targets, the swap chain where the driver allows it —
  is **RGBA16F, premultiplied, linear-light scRGB**, exactly the working format
  decided for the CPU backend (PLAN.md "Pixel format"). 8-bit appears in one
  place only: the final encode to a display that cannot take fp16.
- ~~**GMPI glue stays in gmpi_ui.**~~ *Superseded 2026-10-23: the renderer
  is gmpi_ui code now and may use gmpi types wherever that removes a
  conversion.* Was: `gmpi_ui/backends/GpuGfx.*` implements the
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

1. ~~**Separate repo, fetched by CPM**~~ *Superseded 2026-10-23: absorbed
   into gmpi_ui, see "Absorbing Visage into gmpi_ui".* Was: pinned to a commit (same pattern as
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

## Absorbing Visage into gmpi_ui (ruling 2026-10-23)

Source: fork branch `gmpi` at `4eeb51c` (2026-10-22). Destination:
`backends/gpu/` for the renderer and `backends/gpu/shaders/` for the `.sc`
sources. `backends/GpuGfx.h` stays the GMPI adaptor. The namespace becomes
`gmpi::gpu`, so fork change 11 happens once, here. Each file that came from
Visage keeps its MIT notice at the top. `backends/gpu/README.md` names upstream,
the fork commit absorbed and the licence.

What each part of Visage becomes, judged on "reuse where pragmatic":

| Visage | Fate | Why |
|---|---|---|
| Path coverage atlas, exact per-edge coverage (`path.cpp`) | keep | the hardest-won code; 151/151 rests on it |
| Visage's stroker (`path.cpp:413-603`) | drop | cpugfx's widener is D2D-exact and is what the GPU path already strokes with (milestone 2) |
| `Path` flattening | drop where gmpi already flattens | `Gfx_base`/cpugfx hand the atlas flattened, transformed figures; check each call site before deleting |
| `shape_batcher`, `shapes` (SDF), `region`, `layer`, `canvas` | keep, thinned | batching, overlap detection and the persistent-layer dirty-rect model *are* the GPU path; trim `Canvas` to what `GpuGfx.h` calls |
| Image atlas and gradient atlas | keep | fp16 atlases with this month's fixes in them |
| Gradient stop evaluation | already gmpi | strips are filled from `GradientstopCollection::colorAt` |
| `Color`, `Point`, `Bounds`, `Transform` | gmpi::drawing types at the `GpuGfx.h` boundary | removes conversions; internal vector maths stays if swapping it is churn |
| Screenshot 8-bit resolve (`screenshot.*`) | `CpuEncode.h` | same encode, one implementation |
| `post_effects`, `ShaderCache`, `ProgramCache` | keep | the shader ruling (milestone 8) |
| `font`, `emoji`, `svg`, `theme`, `palette` | drop | decision 2; `CpuTextEngine` and `SvgParser` exist |
| `visage_utils` (`space.h`, `dimension`, `clone_ptr`, `string_utils`, `thread_utils`, `time_utils`, `file_system`, `defines`) | std or gmpi helpers | copy a header only where replacing it costs more than it saves |
| `visage_file_embed` | keep for now | CMake-only and small; drops out if compiled shaders are committed (below) |
| `visage_ui`, `visage_windowing`, `visage_app`, `visage_widgets`, examples | not absorbed | gmpi_ui has its own |
| `renderer.cpp` process singleton | becomes the device | see `METAL_BACKEND_PLAN.md`, "The device seam"; one per binary, not per process |
| Committed shaderc binaries (`visage_graphics/bin`, 29 MB) | not absorbed | fork change 3 |

Build. gmpi_ui has no CMake of its own: consumers list its sources. The
renderer needs a shader step, so gmpi_ui gains a single fragment,
`backends/gpu/gmpi_gpu.cmake`, defining a static library target. gimpi_ui_tests
and SE16 include it in place of `GMPI_GPU_GRAPHICS_FOLDER` /
`SE_GPU_GRAPHICS_FOLDER` + `add_subdirectory`. bgfx stays a CPM fetch for
Linux and Windows. Apple builds stop fetching it once the Metal device lands.

Shaders. Recommended: commit the compiled outputs per renderer
(`backends/gpu/shaders/compiled/`), regenerated by a script when a `.sc`
changes, so no consumer needs shaderc to build. shaderc is built from
bgfx.cmake's tools only for regeneration. Check the outputs' size before
committing them.

Tests. The fork's Catch2 tests that pin bugs found here (`precision_tests.cpp`
and the regressions under "Measured") move to gimpi_ui_tests as gtest cases,
so one runner covers the renderer and the backend. Tests of dropped code go
with it.

Order. Absorb before any more renderer work, in steps that each pass the
suite (151/151 under lavapipe, plus the bgfx-Metal count on the Mac):

1. Copy the kept files, rename the namespace, add the build fragment, and
   switch gimpi_ui_tests over to it.
2. Drop the subsystems marked "drop" and `visage_utils`.
3. Move the `GpuGfx.h` boundary to gmpi types.
4. Port the pinning tests. Archive the fork read-only; don't delete it.

History, open: a plain copy with the source commit recorded in the commit
message and README (recommended: blame on absorbed code is rarely needed),
or `git subtree add`, which keeps history but imports all of upstream's.

Result (2026-10-23, Jeff: "fold the fork into gmpi_ui, then remove unused
features"). Steps 1, 2 and 4 are done; step 3 (gmpi types at the boundary)
is not, and the fork is not archived on GitHub.

- **Where it is.** `backends/gpu/` as `gmpi::gpu`, absorbed from fork commit
  `4eeb51c` by plain copy (the commit is recorded in `backends/gpu/README.md`
  and the commit message). Macros are `GMPI_GPU_*`; the platform macros come
  from `defines.h`, so no consumer passes `-DVISAGE_LINUX=1` any more.
- **Build.** `backends/gpu/gmpi_gpu.cmake` defines the `gmpi_gpu` static
  library and fetches and patches bgfx. gimpi_ui_tests and SynthEditWayland
  include it; `GMPI_GPU_GRAPHICS_FOLDER` and `SE_GPU_GRAPHICS_FOLDER` are gone.
  `tests/run.sh --demo-gpu` and `tests/x11_gpu_screenshot.sh` link
  `libgmpi_gpu.a`.
- **Shaders.** Compiled outputs are committed as C++
  (`shaders/compiled/shaders_spirv.cpp`, `shaders_metal.cpp`, 476 KB
  together), selected per platform by the fragment; `visage_file_embed` and
  the 29 MB of prebuilt shaderc binaries are not absorbed.
  `shaders/compile_shaders.cmake` regenerates them; `GMPI_GPU_BUILD_SHADERC=ON`
  builds bgfx's shaderc from the fetched source. That matters: the fork's
  prebuilt shaderc was version 1.18.112, older than the bgfx it ran against
  (1.18.129), and the committed shaders are from the matching 1.18.129. The
  Linux shaderc cannot compile HLSL, so `shaders_dx11.cpp` waits for a run on
  Windows; a Windows build stops with that message until then.
- **Dropped.** Text, fonts, emoji, SVG, theme, palette; the SDF shapes the
  adaptor doesn't draw (rounded rectangle, squircle, arcs, segments, triangle,
  quadratic, diamond), graph and heat-map shapes and their 15 shaders; the
  Visage stroker, curve flattening and SVG path parsing (`Path` is flattened
  polygons now); encoded-image decoding (bimg decode, stb resize), the 8-bit
  and float atlases, screenshot PNG saving, colour/gradient/brush string
  serialisation, the named colour maps, the background render thread, WebGL
  shader preprocessing, and Visage's string, file, time, thread, clone and
  dimension utilities. 7,600 lines against the fork's 16,000.
- **Kept for shaders.** `post_effects`, `Shader`, `ShaderCache`,
  `ProgramCache`, `Canvas::shader()`, regions and layers, the time uniform,
  and the `.sc` include files. None of it is driven by the GMPI adaptor yet.
- **Tests.** The fork's `precision_tests.cpp` (11 cases) is
  `gimpi_ui_tests/tests/gpu_renderer_tests.cpp` as gtest `GpuRenderer.*`.
  Under lavapipe: 252 / 252 pass, 3 skipped (text-engine capabilities), the
  GPU demo and SynthEditWayland render as before, the scripted editor drag
  and Slider2 insert run clean, analyser frame 0.6 ms render + 13 ms submit
  and present. **macOS is not re-run** after absorption: the Metal shaders
  are new output (shaderc 1.18.129) and the fragment's Apple branch is
  untested.
- **Found on the way, not caused by it.** On X11 the CPU and GPU paths
  differ by one code value on a flat clear colour (154 vs 155 in one
  channel); the commit before absorption shows the same, so it is the CPU
  encode against the GPU's sRGB swap chain, open.

## Fork changes (branch `gmpi`)

Made in the fork up to absorption; gmpi_ui changes after it.

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
5. ~~Premultiplied-alpha blend states~~ Not needed: layer pixels are already
   premultiplied by the SRC_ALPHA/ONE blend over a transparent clear. What
   was wrong was compositing them with straight alpha — fixed (milestone 2).
   DONE also: RGBA16F image atlas + `Canvas::halfImage` (raw premultiplied
   fp16 upload), zero-filled atlas padding, headless composite clear, exact
   coverage shaders.
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

Milestone 2 (2026-10-22): 76 → 98 → 126 → 131 → 146 → 148 → **151 / 151**.
Fork bugs found by the GMPI suite, each now pinned by a fork test in
`precision_tests.cpp`:

- **A texture created with data is immutable in bgfx**, and later
  `updateTexture2D` calls are silently dropped. My zero-fill of atlas
  textures (against NaN padding) did exactly that and blanked every image.
  Create empty, then update with zeros.
- **Coverage shader approximated area by edge distance**: a pixel half
  covered in x and in y read 0.498, not 0.25. Replaced with exact
  pixel∩triangle area (above). Edges and 45° diagonals were already exact.
- **Headless composite layer never cleared**: a window's composite is
  opaque so upstream never clears it; a transparent render target kept
  showing last frame's pixels through transparent areas (write-back of a
  masked buffer "did nothing"). Headless layers now clear invalid rects too.
- **Intermediate layers composited with straight-alpha blending** although
  their content is premultiplied: semi-transparent colour multiplied by
  alpha twice, darker soft shadows (neumorphic tests, mean diff 20, every
  differing pixel darker). `submitSampleRegions` now uses `Composite`.

Retained pixels and Wayland present (2026-10-22), three more fork bugs found
by running many canvases in one process (1 in 3 suite runs lost the blue
channel in ~20 consecutive tests after the first test that read back and
destroyed a second canvas; by the end of the run everything was correct
again — a transient, which ruled out global state):

- **Gradient atlas strip never uploaded.** `addGradient` uploads only if
  the texture exists; a strip added before that was re-uploaded only after
  a repack. The texture, created later on recycled GPU memory (the dead
  canvas's atlas), kept the dead canvas's colours where the white strip
  should be, so the composite's white tint read yellow = white minus blue.
  Fix: zero-fill and re-upload every strip whenever the texture is created.
  0 failing runs in 10, from 3–4.
- **Pointer-keyed image cache.** Raw images were identified by data
  pointer; the backend frees upload buffers after each submit and malloc
  hands the next same-sized buffer the same address → "same image", stale
  pixels. `Image::id` now carries an identity for transient images.
- **Layout depends on NDEBUG.** `VISAGE_LEAK_CHECKER` adds a member only in
  debug builds; the demo compiled without `NDEBUG` against a RelWithDebInfo
  library found its `Layer` 24 bytes away and crashed. The member now
  exists in both configurations.
- **Every window present was one frame late on Linux.** bgfx presents
  frame N at the start of frame N+1 unless `BGFX_RESET_FLIP_AFTER_RENDER`
  is set, and upstream sets it on Windows and macOS only. A UI presents on
  demand, so after an idle period the frame that answered a keystroke only
  appeared when the next keystroke produced another frame ("arrow keys work,
  letters are ignored"). Linux now flips after render. Repro:
  `wayland_demo_gpu --edit Hello` under nested Weston, screenshot at 5 s;
  the demo's `--edit` and `GMPI_DEMO_BLINK` hooks and `GMPI_UI_GPU_TRACE`
  came out of this. (The Wayland demo's right-click menu never worked on
  any backend: the client returned `Ok` for the press, and the frame opens
  the menu only on `Unhandled`. Fixed in the demo.)
- Also: `bgfx::init` failure was ignored (crash at the next bgfx call);
  `BGFX_CAPS_SWAP_CHAIN` is only reported when bgfx was initialised on a
  window (hence the model surface); a frame buffer cannot be built from a
  READ_BACK texture (lavapipe faults); `pkill -f` with the compositor's
  command line kills the shell running it — use `pkill -x weston`.

## Measured (editor on the GPU path, 2026-10-22, lavapipe)

Jeff's first session in SynthEditWayland with `GMPI_UI_GPU=1`: fine lines on
the Frequency Analyser, a crash inserting Slider, dragging an analyser at
~5 Hz. Reproduced with `~/SynthEdit Projects/Frq Anal test.synthedit`
under nested Weston (screenshots) and under a nested gnome-shell on Xvfb
with xdotool (drag + insert; script in the session scratchpad, built from
`SE16/SynthEditWayland/autotest.sh`).

- **The lines were the path atlas's fan.** A filled or stroked graph was a
  fan of several hundred slivers from its first point, and every exterior
  pixel the fan swept received dozens of signed areas that should cancel.
  Two separate errors kept them from cancelling: R16F accumulation (1/2048
  per step; fixed by an R32F atlas where the device renders to it) and the
  per-sliver area itself (Sutherland–Hodgman in fp32 on atlas coordinates:
  ~1e-4 per triangle, ~3e-3 after a fan — still visible, so R32F alone was
  not enough). Coverage is now accumulated **per edge** the way font
  rasterisers do: each edge adds, to each pixel in the rows it spans, the
  area of the pixel to its right. A pixel right of an edge gets exactly the
  edge's overlap with its row, the overlaps of the edges meeting at a vertex
  sum to exactly one in fp32 (Sterbenz), so the exterior is exactly zero;
  only edge pixels carry rounding, and those are fractional anyway. The
  vertex shader works in rect-local coordinates. No conservative
  rasterisation path any more (`vs_conservative_path_fill.sc` deleted).
  Tests: fork "Fan accumulation leaves a graph fill clean" (exterior 0,
  interior 1, exactly; fails at 1/255 on the old R16F fan),
  gimpi_ui_tests `GpuStrokedGraphLeavesTheBackgroundClean` (a 1 px stroked
  400-point polyline, the analyser's case; leaked 0.003 with the fan).
  Fragment cost per edge is (rect width − edge x) × rows, so a graph is
  ~0.5 M fragments rather than the fan's ~50 M of bounding-box quads.
- **Frame rate.** The host renders the whole client every present and only
  the composite was limited to the dirty area; on the GPU path every op
  outside it still cost a draw or an upload (a repainting analyser redrew
  the module browser's 37 text rows and the whole canvas at 4.5 Hz). The
  GPU presents now push an axis-aligned clip of the dirty area. Then: text
  fell back over the whole clip (26 axis labels × a 400×200 raster and fp16
  upload per frame) — bounded by the measured extent now; the canvas grid
  was ~110 stroked lines per frame through the atlas — axis-aligned solid
  lines are SDF rects now, as in cpugfx; the border stroked around the
  8000×8000 canvas widened to an annulus whose bounding box covers every
  module, so each module's frame fell back to a CPU raster and upload of
  its whole clip for ink nowhere near it — a solid axis-aligned
  `drawRectangle` is four SDF rects now (and a single axis-aligned
  rectangle figure too large for the atlas is one). Analyser repaint
  interval on lavapipe: 220 ms → ~100 ms median; a module drag 4 Hz →
  13 Hz. What remains per frame is the CPU fallbacks for gradient fills,
  bitmap draws and retained text layouts (a raster + upload of the clip
  each) and lavapipe itself.
- **Slider insert.** Inserting HSlider2 and Slider2 from the browser on the
  GPU path did not crash in the scripted run (nor on the CPU path). Jeff's
  first crash, after loading a project, reproduces on the CPU path too and
  is `ModuleViewStruct`'s constructor dereferencing a null `moduleInfo` for
  a module type missing from this install (`UnitTest/Projects/
  WavetableSynth.synthedit`; "unregistered module type" in the log) — a
  SynthEdit bug, not a backend one.

## Measured (GPU gradients and bitmaps, 2026-10-22, lavapipe)

Jeff: "go ahead with the GPU gradient and bitmap brushes". The CPU
fallbacks left in an analyser frame were the graph's gradient fill, the
module's bitmap draws and its text; each a raster of the clip's worth and
an fp16 upload.

- **Gradients are fork brushes.** `gpuBrush()` turns a cpugfx solid, linear
  or radial brush into a `visage::Brush`: the strip is a 512-sample LUT
  filled from `GradientstopCollection::colorAt` (straight colour × brush
  opacity; the fork premultiplies when it blends), so both backends paint
  the same stops by construction. Wrap and Mirror are the fork's own
  repeat/reflect (the strip spans a full atlas row, the sampler wraps).
  Linear: t is affine in device space, so the fork's two points are the
  t = 0 point and that plus g/|g|² for the device-space gradient g of t —
  any brush transform × context transform, no special cases. Radial: the
  fork's focal radial (`GradientPosition::radial` + `transformed()`, the
  quadratic form carries non-uniform scale and skew), with cpugfx's focus
  clamp (|f| < 0.999 by length). Degenerate axis or radii: the last stop,
  solid, as D2D. Opacity brushes and bitmap brushes stay with cpugfx.
- **Bitmaps are retained atlas images.** `Bitmap::cachedImage` keeps a
  cpugfx bitmap in the target's half-float atlas across frames, keyed by
  the bitmap and its new `generation` counter (bumped by a write lock's
  release and by a cpugfx `endDraw`), with a one-texel replicated border
  so a stretched draw's bilinear edge clamps to the edge texel as D2D
  does. Drawn as a 1:1 integer-aligned quad (nearest and linear agree) or
  a linear stretch of the whole bitmap; nearest-neighbour stretches,
  cropped stretches, mirrored or out-of-bitmap sources, transforms with
  rotation and draws under a geometry clip go to cpugfx (the trace says
  which). Entries unused for 120 submits are dropped. Fork: `halfImage`
  takes a source sub-rect, `retainHalfImage` returns the atlas handle to
  hold, `ImageWrapper` carries the sub-rect and per-axis texels-per-pixel
  (the clamp adjustment in `vs_tinted_texture` was per-pixel = per-texel,
  wrong for a stretched quad; text quads carry 1).
- **Retained text layouts** fell back over the whole clip (ITextLayout has
  no box accessor; module titles are laid out in a module-sized box):
  `ICpuTextEngine::getTextLayoutPlacement` reports where the aligned block
  lands and the fallback is bounded by that. Analyser frame fallbacks:
  593 k → 47 k pixels. Render 1.4 ms, submit + present 14 ms on lavapipe;
  the ~100 ms present interval is the analyser's own repaint rate, not
  drawing.
- **Oracle tests** (gimpi_ui_tests, GPU build): extend modes, a radial
  with offset focus + brush transform + non-uniform context transform, a
  linear stretch with opacity, and a write-lock redraw, each against a
  cpugfx target of the same drawing. A 512-entry LUT and SDF edges give a
  few thousandths; a wrapped gradient's seam and a fill's corners are
  single-pixel outliers and are counted, not averaged.
- **A single-image atlas was 65 × 50 texels** (the image plus padding):
  texel-centre coordinates are inexact at those sizes and a 1:1 quad
  sampled there mixes a neighbour in by one fp16 ulp on whichever rows
  round unluckily, so byte-identical content read back differently by
  image size (`TextLayoutMatchesDrawText` demands byte equality between
  two fallbacks of different heights). Atlases are power-of-two sided now.
  Snapping coordinates in the fragment shader could not fix it: the
  sampler re-derives the inexact texel index.

## Renderer choice on Linux (2026-10-23)

Jeff: "synthedit on windows has option for software of GPU rendering, can we
do that for Linux?" The Linux editor's Preferences has a "Renderer (on
restart)" combo, GPU or Software (no GPU), in builds with
`SE_GPU_BACKEND=ON` only. It writes the same `GpuDisabled` attribute as
Windows' "Compatibility (software renderer, no GPU)" mode, so the setting
means the same thing on both. The default is GPU, as on Windows; a GPU that
cannot start falls back to software on its own.

- `gpugfx::hostPreference()` holds the host's choice; `wanted()` takes
  `GMPI_UI_GPU` from the environment first (1 = GPU, 0 = software), then
  the host preference, else off (other hosts stay opt-in).
- The renderer starts once per process at the first present, which comes
  before `InitInstance` loads settings, so `Main.cpp` reads `GpuDisabled`
  off disk before any window exists, as the Windows app does for its
  renderer. Hence "on restart".
- Verified under nested Weston with a scratch `XDG_DATA_HOME`: no setting
  presents through the GPU, `GpuDisabled="true"` through the CPU, and each
  environment value overrides the file. Under the nested shell, choosing
  Software in the combo wrote `GpuDisabled="true"` and the next start
  presented through the CPU.

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
2. **Strokes, gradients, clips, bitmaps — DONE (Oct 2026): 151 / 151
   DrawingTest pass**, the whole `gmpi_ui_tests` binary passes (82 + 2
   skipped), 3 of 3 full runs clean. How:
   - **One CPU-fallback seam** (`RenderTarget::viaCpu`): anything the GPU
     path cannot express yet is rendered by a cpugfx target covering the
     draw's device bounds, with this context's transform and geometry clips
     replayed, then uploaded as RGBA16F and composited premultiplied. cpugfx's
     `drawBitmap` is itself "fill a rect with a bitmap brush", so bitmaps,
     bitmap brushes, gradients, opacity brushes and every draw under a
     geometry clip all go through it unchanged and D2D-exact.
   - **Strokes are GPU-native:** cpugfx's contour widener (D2D-exact caps,
     joins, dashes, local-space widening) produces figures that the path
     atlas fills nonzero.
   - **Text goes through the fallback too**: on a cpugfx context the engine
     takes its D2D-matching glyph-mask path; on this context it took the
     plain-geometry path, which draws measurably lighter glyphs (13 multiline
     tests at mean diff 31) even with exact coverage. Milestone 6 moves the
     masks to a GPU atlas.
   - **Write locks round-trip**: `lockPixels(ReadWrite/Write)` reads back,
     the caller edits, release uploads the buffer and draws it Opaque over
     the target (`CachedBlur`, `applyMask`, the Blur tests).
   - **Exact coverage** (fork): the path-atlas fragment shader now clips the
     pixel square against the triangle and takes the shoelace area, so
     corners are exact (0.251 vs 0.498 before) and no conservative
     rasterisation is needed — the fallback draws each triangle's bounding
     box, which costs fill rate only. Fan anchor is the sub-path's first
     point, so nothing is drawn outside the packed rect.
   - **Flagged targets** (Mask / SRGBPixels) and `createCompatibleRenderTarget`
     are cpugfx targets (decision 3); an unflagged `createCpuRenderTarget`
     target is the GPU (CpuReadable is implied: read-back + write-back).
   Still to do from the original scope, now optimisations with the suite as
   oracle: GPU-native gradients (Visage's gradient atlas with 512-texel
   strips from `GradientstopCollection::colorAt`), GPU-native `drawBitmap`
   (the half-float atlas exists; sub-rects and interpolation flags needed),
   geometry clips as shader masks, and a retained-pixels model for targets
   drawn across several beginDraw/endDraw pairs (today a target re-renders
   all shapes since creation and keeps every uploaded buffer alive).
3. **Offscreens on the GPU:** `createCompatibleRenderTarget` without
   `CpuReadable` becomes a fork layer; bitmap-to-texture caching keyed by
   bitmap identity instead of a per-draw repack; the retained-pixels model.
4. **Present on Linux — Wayland DONE (Oct 2026), X11 to do.**
   `WaylandFrameBase::present()` takes a GPU branch when `GMPI_UI_GPU=1`
   (opt-in while young): bgfx is initialised once on a role-less model
   `wl_surface` (its Vulkan renderer only reports swap-chain support when
   initialised on *some* window, and it must not be a surface a canvas will
   pair to), then the frame's own surface gets a swap chain through
   `gpugfx::Factory::createWindowRenderTarget()`; the host's dirty rect
   becomes `RenderTarget::invalidate()`, the client renders, `endDraw()`
   submits and the swap chain's present commits the surface — no shm
   buffer, no attach/damage. Frame pacing stays with the frame callbacks
   (`Renderer::setVsync(false)`). The swap chain is 8-bit **sRGB**: Mesa's
   Wayland WSI offers no float surface, so the GPU encodes linear→sRGB on
   write and the content layers stay RGBA16F. Verified under a nested
   `weston --backend=headless --renderer=pixman` with `weston-screenshooter`:
   the GPU window matches the CPU window pixel for pixel on the background
   ((97,101,108)) and renders text and decorations. Build with
   `gmpi_ui/tests/run.sh --demo-gpu`.
   **X11 DONE** the same way (`DrawingFrameX11::Impl::presentGpu`: bgfx
   initialised on a hidden 1×1 model window, swap chain on the embedded
   window); `tests/x11_gpu_screenshot.sh` runs `x11_menu_test` on both
   paths under Xvfb and the GPU window is pixel-identical to the CPU one.
   Popups, tooltips and dialogs stay on shm/XShm (separate surfaces; they
   coexist with the GPU main surface in one process — verified on the
   desktop). `SE_GPU_BACKEND` option on the SynthEditWayland target.
   **SynthEditWayland on the GPU path (Oct 2026):** built with
   `-DSE_GPU_BACKEND=ON -DSE_GPU_GRAPHICS_FOLDER=<fork>` (SE16 branch
   `gpu-backend`), run with `GMPI_UI_GPU=1`: pixel-identical to the CPU path
   under nested Weston (lavapipe). Getting there found: the editor's first
   frame overflowed the path atlas (every rect fill is a path stored at full
   pixel size; 32768² requested) — fixed with SDF fast paths for axis-aligned
   rects and circles plus a CPU fallback for paths over 512×512 texels, and
   the fork now refuses to bind an overflowed atlas; `clear()` under a clip
   away from the origin painted nothing (fork `fill()` semantics); the GPU
   surface needed an opaque region. Measured draw mix of the first frame:
   135 path fills, 51 SDF rects, 48 CPU fallbacks (text, images), 2 clears.
   Still to do: the VST3 subsurface editor on the GPU path, `tests/run.sh`
   under lavapipe, a real GPU.
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
8. **User shaders - IN PROGRESS, see `SHADER_API_PLAN.md`.** Ruled a
   BRUSH (`GmpiApiShaders.h`: `IShaderFactory::createShaderBrush`, an
   immutable `IShaderBrush` usable with every fill; the module writes
   `vec4 shade(vec2 p)`, compiled into `shaders/template_{quad,path}.sc` by
   `compile_shaders.cmake`'s USER_SHADER mode). Milestone 1 of that plan is
   on branch `shader-api`: 12 `GpuShader.*` tests pass under lavapipe and on
   an NVIDIA card. Original note: expose the fork's shader
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
