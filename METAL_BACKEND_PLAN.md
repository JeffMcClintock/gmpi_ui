# Metal (metal-cpp) GPU backend plan

Status: PROPOSAL v1, 2026-10-23. Companion to `VISAGE_BACKEND_PLAN.md`
(branch `gpu-backend`), which this plan builds on and does not repeat.
Surveyed gmpi_ui `origin/gpu-backend` at `cd70e1c`, the fork
`JeffMcClintock/visage` branch `gmpi` at `4eeb51c`, gimpi_ui_tests
`origin/gpu-backend` at `730557e`.

## Acceptance (Jeff, 2026-10-23)

1. **Composition at 16-bit float.** Every surface the backend owns, swap chain
   included, is RGBA16F premultiplied linear-light. On macOS there is no 8-bit
   anywhere in the present path: the drawable is `MTLPixelFormatRGBA16Float`
   on an extended-linear-sRGB `CAMetalLayer`, the same scRGB chain the D2D
   backend presents through on Windows.
2. **Passes the gmpi-ui tests that compare against Direct2D.** `gimpi_ui_tests`
   with the GPU backend on Metal: all `DrawingTest.*` cases against the D2D
   reference images, at the tolerances the CPU and Vulkan paths use, no
   per-test loosening without a one-line reason in this file.

## Shape of it

The Vulkan backend is not Vulkan code. It is `backends/GpuGfx.h` (GMPI
interfaces) over the Visage fork (`Canvas`, `Path`, `Brush`, layers, atlases,
shape batcher), over **bgfx**, which happens to pick Vulkan on Linux.
`GpuGfx.h` never calls bgfx; it uses only `visage::` types (Canvas, Path,
Brush, Gradient, ImageAtlas, BlendMode, Screenshot, Renderer).

So "a Metal backend" can sit at one of three levels:

| | Level | Cost | Verdict |
|---|---|---|---|
| A | bgfx's own Metal renderer, unchanged (Vulkan plan milestone 7) | ~nothing | **Use as milestone 0 only** |
| B | **Replace bgfx with a metal-cpp device under the fork** | the bgfx call surface, ~60 entry points | **Recommended** |
| C | A new `MetalGfx.h` with its own shaders, no Visage | everything above bgfx again | No |

**Why B.** Everything that made the Vulkan path pass 151/151 lives *above*
bgfx: per-edge exact coverage in the path atlas, SDF fast paths, the
CPU-fallback seam, gradient LUTs from `GradientstopCollection::colorAt`,
retained half-float bitmaps, power-of-two atlases, clear-every-float-target.
C throws that away and re-finds every bug. A keeps it but also keeps the
things that are wrong for a Mac plugin:

- **bgfx is process-global.** The Vulkan plan's landmine "two plugins, one
  bgfx; Vital also uses bgfx" applies to every Mac DAW. A metal-cpp device is
  ordinary C++ objects owned by our binary. No singleton is shared across
  plugins, and with `-fvisibility=hidden` metal-cpp's static selector and class
  caches stay private to each binary.
- **No Objective-C classes.** metal-cpp is header-only C++ that calls
  `objc_msgSend` through cached selectors and defines no ObjC classes. That
  avoids the class-name collision `GmpiObjCNames.h` exists to work around.
- **We own the CAMetalLayer.** Pixel format, colour space, EDR and
  `presentsWithTransaction` are ours to set. bgfx sets none of them today
  (Vulkan plan, "Swap-chain support": "nothing configures this").
- **Synchronous readback.** `waitUntilCompleted` on a blit into a shared
  buffer replaces bgfx's two-frames-later `readTexture` and the frame-pumping
  workaround in `Layer::submit`. `lockPixels` gets simple and stays correct.
- **No 150 MB bgfx/bx/bimg fetch** in Apple builds, and the same device runs
  on iOS (`DrawingFrameIos.mm`).

The win depends on the seam being narrow, and it is. The bgfx calls in the
fork sit in ten files (`canvas`, `layer`, `path`, `shape_batcher`, `image`,
`gradient`, `post_effects`, `renderer`, `graphics_utils`, `graphics_caches`,
plus `font.cpp`, which decision 2 deletes). They use about 60 distinct entry
points: textures and frame buffers (create, update, blit, read back), transient
vertex and index buffers, uniforms, programs, views (rect, frame buffer, clear,
mode), `setState` blend flags, `submit` and `frame`.

## Design

### The device seam (fork)

- New `visage_graphics/gpu/` (or the fork's renamed equivalent, fork change 11)
  with a small `gpu::` API shaped like the subset of bgfx the fork uses:
  handles, views, `submit(view, program, state)`, transient buffers. Keeping it
  bgfx-shaped makes the first commit a mechanical `bgfx::` → `gpu::` rename,
  with no behaviour change on Linux.
- Two implementations, chosen at **compile time**. `gpu_bgfx.cpp` is inline
  forwards to bgfx (Linux, Windows). `gpu_metal.cpp` is metal-cpp (macOS, iOS).
  Nothing needs both in one binary. The runtime choice on a Mac stays "GPU or
  CoreGraphics", the same shape as Linux's "GPU or cpugfx".
- Views map to Metal render passes. The device records each view's draws and
  blits, then encodes them in view-id order at `frame()`, the ordering bgfx
  guarantees. Keep bgfx's "a view's blits run before its draws" rule
  (the fork already relies on it, `submit_pass + 1`), or drop it and fix the
  two call sites. Decide when writing it; whichever is chosen, a fork test
  pins it.
- **The fp16 rules move with it.** A new RGBA16F, R16F or R32F target gets
  `MTLLoadActionClear` on its first pass, which is the Vulkan plan's
  "never create a float render target without clearing it whole" for free.
  After that passes use `Load` plus scissor, which is the persistent-content-layer
  dirty-rect model. Textures are `StorageModePrivate`; upload and readback go
  through `StorageModeShared` staging buffers and a blit. That is correct on
  Apple silicon and on Intel Macs with discrete AMD GPUs.
- State: bgfx blend flags → a cached `MTLRenderPipelineState` keyed by
  (program, colour format, blend). Sampler flags → a cached `MTLSamplerState`.
  Uniforms → `setVertexBytes`/`setFragmentBytes` for small blocks.
- Memory: `NS::SharedPtr` for everything retained. Each `frame()` and each
  present runs inside an `NS::AutoreleasePool`, because `commandBuffer` and
  `nextDrawable` return autoreleased objects and a plugin's timer-driven
  present has no pool of its own.
- Device selection: the view's screen device
  (`CGDirectDisplayCopyCurrentMetalDevice`), else
  `MTLCreateSystemDefaultDevice`. This is the Mac counterpart of
  "choose the DXGI adapter" (`8853714`). One command queue per device, shared
  by every canvas in the binary.

### Shaders

The `.sc` sources stay the single source of truth (the shader ruling: the
write-once pipeline is kept for milestone 8). shaderc already ships in
`visage_graphics/bin/macos` and emits Metal.

- **v1: reuse the bgfx Metal shader binaries.** Load the embedded `-p metal`
  output, parse bgfx's shader-binary header (magic, hash, uniform table with
  offsets, then MSL source), and compile with `newLibraryWithSource` once per
  process, cached. No new build tooling. Runtime MSL compilation is also what
  user shaders (milestone 8) will need, so the path gets exercised early.
  *To verify first:* the exact header layout for the pinned bgfx tag, and how
  bgfx's Metal output binds uniforms (one packed struct per stage) and vertex
  attributes (`[[stage_in]]` from the vertex layout). Write the parser against
  the source, not from memory.
- v2 (load time, later): shaderc → MSL → `xcrun metal` → an embedded
  `.metallib` per target, with the reflection data emitted next to it.

### Coverage atlas precision

The Vulkan work found R16F accumulation visible in fans and moved the atlas to
**R32F, where the device renders to it** (per-edge coverage then makes the
exterior exactly zero). On Metal:

- Query at init whether R32Float is renderable **and blendable** on this GPU
  family. Do not assume it; Metal's feature-set tables restrict 32-bit float
  blending by family. This Mac is an M1 (Apple7), the floor for Apple-silicon
  Macs.
- If it is not blendable, the fallback is Metal-specific and better than R16F:
  on Apple-family GPUs the fragment shader can read the current attachment
  value (programmable blending, `[[color(0)]]`) and add in fp32 itself. That is
  exact accumulation without fixed-function float blend.
- Pin it with the existing tests: fork "Fan accumulation leaves a graph fill
  clean" and gimpi_ui_tests `GpuStrokedGraphLeavesTheBackgroundClean`.

### Present (gmpi_ui, `DrawingFrameMac.mm`)

Today `GMPI_VIEW_CLASS` is a CoreGraphics `drawRect:` view. Invalidations
reach it through `setNeedsDisplayInRect:` (`:387-408`), and `drawRect:` flips
the rect and calls `drawingFrame.onRender` (`:766`). The CPU path already
renders float (`kCGBitmapFloatComponents`, `:557`).

GPU branch, taken when `gpugfx::wanted()` (same `GMPI_UI_GPU` environment
variable then host-preference rule as Linux) and device init succeeds:

- The view is layer-hosting with a `CAMetalLayer`:
  `pixelFormat = MTLPixelFormatRGBA16Float`,
  `colorspace = kCGColorSpaceExtendedLinearSRGB`,
  `wantsExtendedDynamicRangeContent = YES`,
  `contentsScale`/`drawableSize` from `backingScaleFactor`, updated in
  `viewDidChangeBackingProperties`. scRGB 1.0 is SDR white under EDR, so the
  composite is a plain copy, with no white-level scaling and no encode pass.
  This is acceptance 1 end to end.
- `wantsUpdateLayer = YES`, so `drawRect:` stops being called and its
  dirty-rect argument goes with it. The frame keeps its own union of
  invalidated rects (as Wayland does, `DrawingFrameWayland.h:377`). In
  `updateLayer`: `nextDrawable` (a nil drawable means occluded, so skip and
  keep the dirty rect), `invalidate(dirty)`, `client->render` into the
  persistent fp16 content layer, composite the whole layer into the drawable
  (drawables are not preserved; a full-screen fp16 copy is cheap), present.
- Pacing stays event-driven, like today. No display link: `CADisplayLink` on
  macOS is 14+, and plugins target 10.15 (`se-plugin-deployment-target`).
  `maximumDrawableCount = 2` and `allowsNextDrawableTimeout = YES`, so a DAW
  with many open editors never blocks the main thread for a second.
- Live resize: `presentsWithTransaction = YES` while
  `inLiveResize`, so the Metal frame and the window frame commit together
  (no stretched-content flash).
- Coordinates: Metal render targets are top-left origin, like GMPI. The
  flip in `drawRect:` does not apply to the GPU branch; `isFlipped` is
  irrelevant to a hosted layer.
- Popups, tooltips and dialogs stay on CoreGraphics at first, as they stayed
  on shm on Linux.

### Test harness (gimpi_ui_tests)

- `-DGMPI_UI_TESTS_GPU_BACKEND=ON` on macOS builds the fork with the Metal
  device. `DrawingTestContext_gpu.cpp` should need no changes because it talks
  to `GpuGfx.h`. The CMake check for a Vulkan ICD becomes platform-conditional.
- Metal runs headless with no window. The fork's
  `macos/windowless_context.mm` fake-layer hack goes away on the Metal device.
- Same 64×64 fp16 fixture, same D2D references (177 PNGs in
  `reference_images`), same tolerances. The fixture's "never overwrite a
  reference that fails to load" guard (`730557e`) stays on. A first Metal run
  must not repaint the D2D truth.
- Real-GPU CI: the self-hosted Mac runners on this iMac (M1) can run the suite
  on every push. These are the project's first GPU numbers that are not
  lavapipe.

## Milestones

Each is checkable on its own. Nothing lands in SynthEdit before M4.

0. **Baseline on bgfx-Metal (level A), this Mac.** Build the fork and
   gimpi_ui_tests GPU target with bgfx's own Metal renderer and run the suite.
   Failures here belong to the algorithms on an Apple GPU (float blending, TBDR,
   sampler rounding), not to the new device, so they get fixed before the device
   exists. Also answer the R32Float-blend question. Exit: pass count recorded
   here, with each failure triaged.
1. **Device seam, no behaviour change.** `bgfx::` → `gpu::` in the fork, with
   bgfx behind it everywhere. Exit: the fork's tests and gimpi_ui_tests pass at
   the M0 count on the Mac, and 151/151 still under lavapipe on Linux (needs
   the Linux VM).
2. **metal-cpp device, headless.** Textures, frame buffers, first-pass clears,
   transient buffers, programs from parsed bgfx-Metal binaries, views,
   blend/sampler caches, blit, synchronous readback. Exit: the fork's precision
   tests pass on it (300 passes of alpha 1/512 accumulate to ~112/255; an HDR
   2.5 value reads back as 2.5 through `halfData()`), then **gimpi_ui_tests
   151/151 against the D2D references**, 3 clean full runs. bgfx leaves Apple
   builds here.
3. **Present on macOS.** `DrawingFrameMac.mm` GPU branch as above, opt-in via
   `GMPI_UI_GPU=1`. Exit: a demo window on the GPU path matches the CoreGraphics
   path when captured as fp16 (window capture, compare after encode). Drawable
   format confirmed RGBA16Float at runtime. Resize and Retina/non-Retina moves
   are correct.
4. **SynthEdit Mac editor and plugins.** The editor's renderer preference (the
   `GpuDisabled` attribute, as on Windows and Linux). Plugin isolation: two
   different SE-built AUs/VST3s plus a bgfx-using plugin open together in one
   host, and `auval`. Editor open/close 100× with no growth in Metal
   allocations (Instruments, or a counter in the device).
5. **iOS** (optional): `DrawingFrameIos.mm` on the same device.
6. **Load time and speed:** precompiled `.metallib` (shader v2); real-GPU
   timings for the Vulkan plan's milestone 6 text target.

## Landmines

- **32-bit float blending is not universal** on Metal GPU families. Check
  caps at init (see "Coverage atlas precision"). Never let a missing cap
  quietly fall back to R16F; the test above catches it.
- **ODR across plugins.** metal-cpp needs `NS_PRIVATE_IMPLEMENTATION`,
  `MTL_PRIVATE_IMPLEMENTATION` and `CA_PRIVATE_IMPLEMENTATION` in exactly one
  translation unit per binary, and its globals must be hidden. Check with
  `nm -gU` on a built plugin that no `MTL::Private` symbol is exported.
- **Autoreleased objects on a timer.** A present outside an
  `NS::AutoreleasePool` leaks one drawable per frame. A leak that grows with
  frame rate points here first.
- **`nextDrawable` blocks** up to a second when all drawables are in flight.
  Keep it off anything latency-sensitive and keep the timeout on.
- **Discrete-GPU Macs.** `StorageModeShared` textures are slow or unavailable
  for render targets on AMD, so use private textures plus staging buffers
  everywhere. Do not special-case Apple silicon.
- **Shader binary format drift.** The parser is tied to the pinned bgfx tag's
  header layout. A bgfx bump needs a format test (load every embedded Metal
  shader, compile, check uniform names).
- **macOS 10.15 floor** for plugins: every API above exists on 10.15 except
  `CADisplayLink`, which is deliberately unused. Build with the plugin
  deployment target, not the editor's 13.3, or the compiler will not flag
  a newer API.
- Inherited from the Vulkan plan unchanged: clear every new float target
  whole; texture-identity cache keys, not pointers; layout must not depend
  on `NDEBUG`; power-of-two atlases.

## Open decisions for Jeff

1. Seam at the bgfx-shaped `gpu::` level (recommended), or a narrower
   Canvas-level interface? The bgfx-shaped one makes M1 mechanical. A narrower
   one would be cleaner but would rewrite canvas, layer and path code that the
   Linux suite has just made pass.
2. Drop bgfx from Apple builds once M2 passes (recommended), or keep
   bgfx-Metal as a fallback?
3. On the Mac, GPU default or opt-in at M4? Recommendation: opt-in until two
   real hosts have run it for a release, then the default with CoreGraphics as
   the software option, matching Windows.
