# Metal (metal-cpp) GPU backend plan

Status: PROPOSAL v2, 2026-10-23. Companion to `VISAGE_BACKEND_PLAN.md`,
which this plan builds on and does not repeat. Surveyed gmpi_ui
`gpu-backend` at `cd70e1c`, the Visage fork `JeffMcClintock/visage` branch
`gmpi` at `4eeb51c`, gimpi_ui_tests `gpu-backend` at `730557e`.

v2: Visage is absorbed into gmpi_ui (ruling below), so the renderer, the device
seam and the Metal device are all gmpi_ui code under `backends/gpu/`.

## Rulings and acceptance (Jeff, 2026-10-23)

- **Visage is absorbed into gmpi_ui, customised only for gmpi_ui, reusing
  code where pragmatic.** It is not a dependency. What gets kept, replaced and
  dropped is in `VISAGE_BACKEND_PLAN.md`, "Absorbing Visage into gmpi_ui".
- **Acceptance 1: composition at 16-bit float.** Every surface the backend
  owns, swap chain included, is RGBA16F premultiplied linear-light. On macOS
  there is no 8-bit anywhere in the present path: the drawable is
  `MTLPixelFormatRGBA16Float` on an extended-linear-sRGB `CAMetalLayer`, the
  same scRGB chain the D2D backend presents through on Windows.
- **Acceptance 2: passes the gmpi-ui tests that compare against Direct2D.**
  `gimpi_ui_tests` with the GPU backend on Metal: every `DrawingTest.*` case
  against the D2D reference images, at the tolerances the CPU and Vulkan
  paths use. No test gets a looser tolerance without a one-line reason in
  this file.

## What Visage has for Metal today

Visage has a Metal path, but none of it is Visage code:

- `renderer.cpp:141` asks bgfx for `RendererType::Metal` on macOS, and bgfx's
  own Objective-C++ Metal renderer does the work.
- `embedded.cmake:75` compiles every `.sc` shader with
  `shaderc -p metal --platform osx` at build time, using the committed
  `visage_graphics/bin/macos/shaderc`.
- Visage's only Metal code of its own is `macos/windowless_context.mm`, a
  hidden `CAMetalLayer` so that bgfx can initialise headless.

Upstream Visage apps therefore render through Metal on a Mac. The gmpi fork's
changes (fp16 layers, exact coverage, R32F atlas, the bug fixes) have only
ever run under lavapipe. Nobody has run them on Metal.

## Shape of it

`backends/GpuGfx.h` (the GMPI interfaces) sits over the renderer (`Canvas`,
`Path`, layers, atlases, shape batcher), which sits over bgfx. `GpuGfx.h` never
calls bgfx itself. A Metal backend can go in at one of three levels:

| | Level | Cost | Verdict |
|---|---|---|---|
| A | bgfx's own Metal renderer, unchanged (what Visage does today) | ~nothing | **Milestone 0 baseline only** |
| B | **A metal-cpp device under the absorbed renderer, behind a seam** | the bgfx call surface, ~60 entry points | **Recommended** |
| C | A new `MetalGfx.h` with its own shaders, no Visage code | everything above bgfx again | No |

**Why B.** Everything that made the Vulkan path pass 151/151 lives *above*
bgfx: per-edge exact coverage, SDF fast paths, the CPU-fallback seam, gradient
lookup tables from `GradientstopCollection::colorAt`, retained half-float
bitmaps, power-of-two atlases, clear-every-float-target. C throws all of that
away, which is the opposite of reusing code where pragmatic. A keeps it but
also keeps what is wrong for a Mac plugin:

- **bgfx is process-global.** The Vulkan plan's landmine "two plugins, one
  bgfx; Vital also uses bgfx" applies to every Mac DAW. The metal-cpp device
  is plain C++ objects owned by our binary. With `-fvisibility=hidden`,
  metal-cpp's static selector and class caches stay private to each binary.
- **No Objective-C classes.** metal-cpp is header-only C++ that calls
  `objc_msgSend` through cached selectors and defines no ObjC classes, so there
  is nothing for `GmpiObjCNames.h` to work around.
- **We own the CAMetalLayer**: pixel format, colour space, EDR,
  `presentsWithTransaction`. bgfx configures none of them (Vulkan plan,
  "Swap-chain support"). Acceptance 1 needs all of them.
- **Synchronous readback.** `waitUntilCompleted` on a blit into a shared
  buffer replaces bgfx's two-frames-later `readTexture` and the frame-pumping
  workaround in `Layer::submit`.
- **No bgfx/bx/bimg fetch (~150 MB) in Apple builds**, and the same device
  serves iOS (`DrawingFrameIos.mm`).

The seam is narrow. bgfx calls sit in ten renderer files (`canvas`, `layer`,
`path`, `shape_batcher`, `image`, `gradient`, `post_effects`, `renderer`,
`graphics_utils`, `graphics_caches`; `font.cpp` is dropped at absorption).
They use about 60 distinct entry points: textures and frame buffers (create,
update, blit, read back), transient vertex and index buffers, uniforms,
programs, views (rect, frame buffer, clear, mode), `setState` blend flags,
`submit` and `frame`.

## Design

### The device seam (`backends/gpu/device/`)

- `device.h` is a small `gmpi::gpu` API shaped like the subset of bgfx the
  renderer uses: handles, views, `submit(view, program, state)`, transient
  buffers. Keeping it bgfx-shaped makes the first commit a mechanical
  `bgfx::` → device rename with no behaviour change on Linux. Because the
  renderer is gmpi_ui code now, the seam can narrow later, once the Metal
  device has shown which bgfx concepts it actually needs.
- Two implementations, chosen at **compile time**: `device_bgfx.cpp` (inline
  forwards, Linux and Windows) and `device_metal.cpp` (metal-cpp, macOS and
  iOS). Nothing needs both in one binary. The runtime choice on a Mac is
  "GPU or CoreGraphics", the same shape as Linux's "GPU or cpugfx".
- Views map to Metal render passes. The device records each view's draws and
  blits, then encodes them in view-id order at `frame()`, the ordering bgfx
  guarantees. Either keep bgfx's "a view's blits run before its draws" rule
  (the renderer relies on it, `submit_pass + 1`) or drop it and fix the two
  call sites. Decide when writing it, and pin the choice with a test.
- **The fp16 rules move with it.** A new RGBA16F, R16F or R32F target gets
  `MTLLoadActionClear` on its first pass. That is the Vulkan plan's "never
  create a float render target without clearing it whole", for free. Later
  passes use `Load` plus scissor, which is the persistent-content-layer
  dirty-rect model. Textures are `StorageModePrivate`. Upload and readback go
  through `StorageModeShared` staging buffers and a blit, which is correct on
  Apple silicon and on Intel Macs with discrete AMD GPUs.
- State: bgfx blend flags map to a cached `MTLRenderPipelineState` keyed by
  (program, colour format, blend), and sampler flags to a cached
  `MTLSamplerState`. Uniforms go through `setVertexBytes`/`setFragmentBytes`
  for small blocks.
- Memory: `NS::SharedPtr` for everything retained. Each `frame()` and each
  present runs inside an `NS::AutoreleasePool`, because `commandBuffer` and
  `nextDrawable` return autoreleased objects and a plugin's timer-driven
  present has no pool of its own.
- Device selection: the view's screen device
  (`CGDirectDisplayCopyCurrentMetalDevice`), else
  `MTLCreateSystemDefaultDevice`. This is the Mac counterpart of "choose the
  DXGI adapter" (`8853714`). One command queue per device, shared by every
  canvas in the binary.
- metal-cpp (Apache-2.0, header-only) is vendored under
  `backends/gpu/third_party/metal-cpp/` with its licence. It is not fetched.

### Shaders

The `.sc` sources stay the single source of truth (the shader ruling keeps the
write-once pipeline for milestone 8). Visage already produces Metal output
(`shaderc -p metal`, above).

- **v1: reuse those bgfx-format Metal shaders.** Take the `-p metal` outputs.
  These are the compiled shaders that the Visage plan's absorption section
  recommends committing, so no consumer needs shaderc. Parse bgfx's
  shader-binary header (magic, hash, uniform table with offsets, then MSL
  source) and compile with `newLibraryWithSource` once per process, cached. This
  adds no build tooling, and runtime MSL compilation is also what user shaders
  (milestone 8) will need. *Verify first:* the header layout for the pinned
  bgfx tag, and how bgfx's Metal output binds uniforms (a packed struct per
  stage) and vertex attributes (`[[stage_in]]` from the vertex layout). Write
  the parser against bgfx's source, not from memory.
- v2 (load time, later): shaderc → MSL → `xcrun metal` → an embedded
  `.metallib`, with the reflection data emitted alongside.

### Coverage atlas precision

The Vulkan work found R16F accumulation visible in fans and moved the atlas to
**R32F where the device renders to it**. With per-edge coverage, the exterior
is then exactly zero. On Metal:

- At init, query whether R32Float is renderable **and blendable** on this GPU
  family. Don't assume it: Metal's feature-set tables restrict 32-bit float
  blending by family. This Mac is an M1 (Apple7), the lowest Apple-silicon
  Mac.
- If R32Float isn't blendable, Metal offers a fallback that beats R16F. On
  Apple-family GPUs the fragment shader can read the current attachment value
  (programmable blending, `[[color(0)]]`) and do the add in fp32 itself. That
  is exact accumulation without fixed-function float blending.
- The existing tests pin it: the renderer's "Fan accumulation leaves a graph
  fill clean" and gimpi_ui_tests `GpuStrokedGraphLeavesTheBackgroundClean`.

### Present (`DrawingFrameMac.mm`)

Today `GMPI_VIEW_CLASS` is a CoreGraphics `drawRect:` view. Invalidations
reach it through `setNeedsDisplayInRect:` (`:387-408`), and `drawRect:` flips
the rect and calls `drawingFrame.onRender` (`:766`). The CPU path already
renders float (`kCGBitmapFloatComponents`, `:557`).

The GPU branch is taken when `gpugfx::wanted()` (the same `GMPI_UI_GPU`
environment variable, then host-preference rule as Linux) and device init both
succeed:

- The view hosts a `CAMetalLayer` with
  `pixelFormat = MTLPixelFormatRGBA16Float`,
  `colorspace = kCGColorSpaceExtendedLinearSRGB` and
  `wantsExtendedDynamicRangeContent = YES`. `contentsScale` and `drawableSize`
  come from `backingScaleFactor` and are updated in
  `viewDidChangeBackingProperties`. Under EDR, scRGB 1.0 is SDR white, so the
  composite is a plain copy, with no white-level scaling and no encode pass.
  This is acceptance 1 end to end.
- With `wantsUpdateLayer = YES`, `drawRect:` (and its dirty rect) stops being
  called. The frame keeps its own union of invalidated rects, as Wayland does
  (`DrawingFrameWayland.h:377`). `updateLayer` does the following:
  - `nextDrawable`. A nil drawable means the view is occluded: skip the frame
    and keep the dirty rect.
  - `invalidate(dirty)`, then `client->render` into the persistent fp16 content
    layer.
  - Composite the whole layer into the drawable, then present. Drawables are
    not preserved, and a full-screen fp16 copy is cheap.
- Pacing stays event-driven, as now. There is no display link, because
  `CADisplayLink` on macOS needs 14+ and plugins target 10.15
  (`se-plugin-deployment-target`). `maximumDrawableCount = 2` and
  `allowsNextDrawableTimeout = YES`, so a DAW with many open editors never
  blocks the main thread for a second.
- Live resize: `presentsWithTransaction = YES` while `inLiveResize`, so the
  Metal frame and the window frame commit together (no stretched-content
  flash).
- Metal render targets are top-left origin, like GMPI. The flip in `drawRect:`
  does not apply, and `isFlipped` is irrelevant to a hosted layer.
- Popups, tooltips and dialogs stay on CoreGraphics at first, as they stayed
  on shm on Linux.

### Test harness (gimpi_ui_tests)

- `-DGMPI_UI_TESTS_GPU_BACKEND=ON` on macOS builds the absorbed renderer
  through gmpi_ui's `backends/gpu/gmpi_gpu.cmake`, with the Metal device.
  `GMPI_GPU_GRAPHICS_FOLDER` and the `add_subdirectory` of the fork go away.
  `DrawingTestContext_gpu.cpp` should need no changes because it only talks to
  `GpuGfx.h`. The "needs a Vulkan ICD" note becomes Linux-only.
- Metal runs headless with no window, so `windowless_context.mm` is not
  absorbed.
- Same 64×64 fp16 fixture, same D2D references (177 PNGs in
  `reference_images`), same tolerances. The fixture's "never overwrite a
  reference that fails to load" guard (`730557e`) stays on, so a first Metal
  run cannot repaint the D2D truth.
- The renderer's own pinning tests (`precision_tests.cpp` and the regression
  cases) arrive in gimpi_ui_tests as gtest cases at absorption, so they run on
  Metal with no extra harness.
- Real-GPU CI: the self-hosted Mac runners on this iMac (M1) can run the suite
  on every push. These would be the project's first GPU numbers that are not
  lavapipe.

## Milestones

Each one can be checked on its own. Nothing lands in SynthEdit before M4.

0. **Baseline on bgfx-Metal (level A), on this Mac.** Run the suite through
   bgfx's own Metal renderer, before or after absorption, whichever lands
   first. This is measurement, not renderer work. Failures here belong to the
   renderer on an Apple GPU (float blending, TBDR, sampler rounding), not to a
   new device, so they get fixed before the device exists. Also answer the
   R32Float-blend question. Exit: the pass count is recorded here and every
   failure is triaged.
1. **Absorption** (`VISAGE_BACKEND_PLAN.md`, "Absorbing Visage into gmpi_ui").
   This is shared with Linux and is a prerequisite for everything below. Exit:
   151/151 under lavapipe (needs the Linux VM) and the M0 count on the Mac,
   built from gmpi_ui alone.
2. **Device seam, no behaviour change.** `bgfx::` → the `gmpi::gpu` device
   API, with bgfx behind it everywhere. Exit: the same counts as M1.
3. **metal-cpp device, headless.** Textures, frame buffers, first-pass clears,
   transient buffers, programs from parsed bgfx-Metal shaders, views,
   blend and sampler caches, blit, synchronous readback. Exit: the precision
   tests pass (300 passes of alpha 1/512 accumulate to ~112/255; an HDR 2.5
   value reads back 2.5), then **gimpi_ui_tests 151/151 against the D2D
   references**, in 3 clean full runs. bgfx leaves Apple builds at this point.
4. **Present on macOS and in SynthEdit.** The `DrawingFrameMac.mm` GPU branch
   above, opt-in through `GMPI_UI_GPU=1`. Exit:
   - The drawable format is confirmed RGBA16Float at runtime.
   - A demo window on the GPU path matches the CoreGraphics path when captured
     as fp16.
   - Resize and Retina ↔ non-Retina moves are correct.
   - The editor's renderer preference works (`GpuDisabled`, as on Windows and
     Linux).
   - Plugin isolation: two different SE-built AUs or VST3s plus a bgfx-using
     plugin run together in one host, and `auval` passes.
   - Editor open/close 100× with no growth in Metal allocations.
5. **iOS** (optional): `DrawingFrameIos.mm` on the same device.
6. **Load time and speed:** a precompiled `.metallib` (shaders v2), and
   real-GPU timings for the Vulkan plan's milestone-6 text target.

## Landmines

- **32-bit float blending is not universal** across Metal GPU families. Check
  caps at init (see "Coverage atlas precision"). A missing cap must never fall
  back to R16F quietly; the test above catches it if it does.
- **ODR across plugins.** metal-cpp needs `NS_PRIVATE_IMPLEMENTATION`,
  `MTL_PRIVATE_IMPLEMENTATION` and `CA_PRIVATE_IMPLEMENTATION` in exactly one
  translation unit per binary, and its globals must be hidden. Check with
  `nm -gU` on a built plugin that no `MTL::Private` symbol is exported.
- **Autoreleased objects on a timer.** A present outside an
  `NS::AutoreleasePool` leaks one drawable per frame. If a leak grows with frame
  rate, look here first.
- **`nextDrawable` blocks** for up to a second when every drawable is in
  flight. Keep it off anything latency-sensitive, and leave the timeout on.
- **Discrete-GPU Macs.** On AMD, `StorageModeShared` textures are slow or
  unavailable as render targets, so use private textures plus staging buffers
  everywhere. Don't special-case Apple silicon.
- **Shader binary format drift.** The parser is tied to the pinned bgfx tag's
  header layout. A bgfx bump needs a format test: load every committed Metal
  shader, compile it, and check the uniform names.
- **macOS 10.15 floor** for plugins. Every API above exists on 10.15 except
  `CADisplayLink`, which is deliberately unused. Build with the plugin
  deployment target, not the editor's 13.3, so the compiler flags any
  newer API.
- Inherited unchanged from the Vulkan plan: clear every new float target
  whole; key texture caches by identity, not pointers; layout must not depend
  on `NDEBUG`; atlases are power-of-two.

## Open decisions for Jeff

1. Seam at the bgfx-shaped device level (recommended), or a narrower
   Canvas-level interface? Absorption makes narrowing possible later. Doing it
   first would rewrite renderer code that the Linux suite has only just made
   pass.
2. Drop bgfx from Apple builds once M3 passes (recommended), or keep
   bgfx-Metal as a fallback?
3. On the Mac, GPU by default or opt-in at M4? Recommendation: opt-in until two
   real hosts have run it for a release, then the default, with CoreGraphics
   as the software option, as on Windows.
