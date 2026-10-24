# User shaders in gmpi_ui, proven by a fluid module (plan, 2026-10-24)

Milestone 8 of `gmpi_ui/VISAGE_BACKEND_PLAN.md` says: expose the absorbed
renderer's shader pipeline to module authors through a new interface found by
`queryInterface`. This is the plan for the first cut of that API and the module
that exercises it: **SE Fluid**, a GUI-only control that paints an animated,
fluid-looking surface with a fragment shader on the GPU path and degrades to a
cheap CPU drawing everywhere else.

Everything below is from reading the code on this machine (gmpi_ui
`gpu-backend`, SynthEdit `gpu-backend`, SynthEditLib `main`); nothing is built
yet.

## What exists, verified

- `gmpi::gpu::Canvas::shader(Shader*, x, y, w, h)` (`backends/gpu/canvas.h:153`)
  queues a quad drawn by a caller-supplied vertex+fragment pair. Its batch
  path `submitShader` (`shape_batcher.cpp:193`) sets `u_time`, `u_dimensions`,
  the gradient atlas, colour mult and origin flip, then submits
  `ProgramCache::programHandle(vs, fs)`. Nothing in `GpuGfx.h` calls it:
  "not driven by the GMPI adaptor yet".
- `Shader` (`shader.h`) is two `EmbeddedFile`s (name, bytes, size) and a blend
  mode. `ShaderCache` turns the bytes into `bgfx::createShader` on first use,
  keyed by data pointer, so a shader that lives in a module's static data is
  compiled once per process. `ShaderPostEffect` (`post_effects.h`) already has
  the per-effect named-uniform map (`setUniformValue(name, 4 floats)`) that
  `submitShader` lacks.
- The vertex a custom shader receives is `ShapeVertex` (`graphics_utils.h:285`)
  through `vs_shape.sc`: `v_coordinates` runs -1..1 across the quad,
  `v_dimensions` is the quad size in pixels, `v_position` is the device
  position, `v_shader_values` carries `value1/value2` (unused for a plain
  `ShaderWrapper`). A fragment shader needs only `v_coordinates` and
  `u_time` to animate.
- `u_time` comes from `Layer::time()`, set by `Canvas::updateTime(double)`
  (`canvas.cpp:211`). **Nothing calls `updateTime`** on the adaptor path
  (`GpuGfx.h`, `DrawingFrameWayland.h`, `DrawingFrameX11.cpp`): every layer
  is at t = 0. Small fix, listed below.
- Shaders are bgfx `.sc`, compiled ahead of time by shaderc into per-renderer
  blobs committed as C++ (`shaders/compiled/shaders_spirv.cpp`,
  `shaders_metal.cpp`; `shaders_dx11.cpp` still waits for a Windows run).
  `shaders/compile_shaders.cmake` regenerates them; `GMPI_GPU_BUILD_SHADERC=ON`
  builds shaderc from the fetched bgfx (the binary format is versioned, so it
  must be that bgfx). The compiled blob for the running renderer is chosen by
  `bgfx::getRendererType()` at the host; a module cannot know in advance.
- Layers are RGBA16F, linear, premultiplied. A custom fragment shader must
  write premultiplied linear colour; `submitShader` blends `Alpha`.
- Clips on the GPU path are clamp bounds (axis-aligned). A shader quad takes
  the current clamp like an image does (`beginGpuShape`, `GpuGfx.h:505`);
  rotation/skew and geometry clips have no GPU shader path (same rule as
  bitmaps: `axisAligned()`, `gpuSolid()` at `GpuGfx.h:497-503`).
- ABI policy (`GmpiApiDrawing.h:858-889`): `IDeviceContext` cannot grow
  (`IBitmapRenderTarget` sits in the next vtable slot); `IFactory` may be
  appended to; **anything new is gated by probing a new interface guid**.
  Precedent for serving an extension without touching the base vtable:
  `GmpiUiToSDK3.h:43-76` returns an aggregated `ExtAdapter` member from
  `queryInterface` (making it a base class once shifted `GetBitmap` and
  crashed Scope3). `RenderTarget::queryInterface` (`GpuGfx.h:1389`) and
  `Factory::queryInterface` (`:1536`) answer only their base interfaces today.
- A GMPI module's `render()` gets the backend's raw `gpugfx::RenderTarget*`
  (`DrawingFrameWayland.h:1230` → `ViewBase::render` → `ModuleViewPanel::render`,
  `ModuleView.cpp:1250-1270`); nothing wraps it, so `queryInterface` on the
  context reaches the backend. Legacy SDK3 modules get a wrapper; this API is
  GMPI-only.
- Animated GMPI module template: `SynthEditLib/modules/Controls/Scope4Gui.cpp`
  (`PluginEditor + gmpi::TimerClient`, `onTimer` → `invalidateRect(&bounds)`,
  `Register<>::withXml`), built by `gmpi_plugin(... HAS_GUI IS_OFFICIAL_MODULE
  SOURCE_FILES ... ${GMPI_UI_SDK}/helpers/Timer.cpp)`.

## Prerequisite found on the way: module timers never tick on Linux

`helpers/Timer.cpp:165-170`: on Linux a `Timer` has no native source and
relies on the host calling `TimerManager::pump()`. `TimerManager::instance()`
is a function-local static, compiled into whoever links `Timer.cpp`. The app
pumps its own (`SynthEditWayland/Main.cpp:226`) but does not export the symbol
(`nm -D SynthEditWayland | grep TimerManager` → nothing) and modules are
`dlopen`ed `RTLD_LAZY` without `RTLD_GLOBAL` (`xp_dynamic_linking.cpp:35`), so
`Controls.gmpi` has its own `TimerManager::instance` (`nm -D` shows it) that
nobody pumps. By inspection, Scope4 and every other timer-driven GMPI module
is frozen in the Wayland editor. Not yet seen running; milestone 0 checks it
in one step (open a project with Scope4, watch for redraws).

Fix options, cheapest first:

1. Each `.gmpi` exports `extern "C" void gmpi_pump_timers()` (one line in
   `Timer.cpp` under the Linux branch, calling `TimerManager::instance()->pump()`),
   and the module loader keeps the `dlsym` result in `Module_Info3`; the
   Wayland main loop calls every loaded module's pump next to its own. No ABI
   change, Linux-only, matches the "host-pumped" design `Timer.h` already
   describes.
2. A host interface (`ITimerHost::requestTick(client)`) queried from the
   drawing host; `Timer::start` on Linux registers with it instead of relying
   on an outside pump. Cleaner, works for third-party hosts, but a new guid in
   `GmpiApiEditor.h` and a change to every host.

Recommend 1 now, 2 when the VST3/CLAP plugin GUIs land on Linux (they will
need it anyway: a DAW has no idea about our pump).

## The API (gmpi_ui) - revised 2026-10-24: a BRUSH, immutable

Ruled in discussion: the shader is a brush, not a draw verb. Skia's `SkShader`
is the precedent (its gradients are shaders; `SkRuntimeEffect::makeShader` is
the user-programmable one), and a gradient brush is already a function from
position to colour - a shader brush is that function written in code. The
`drawShader(rect)` verb of the first draft is `fillRectangle(rect, shaderBrush)`.

What it buys: no context-side interface at all (only the factory grows, which
the ABI policy allows, still guid-gated); the brush composes with every fill
(`fillGeometry`, rounded rects, ellipses, strokes, eventually text); brush
space already exists (`BrushProperties::transform`), so scaling or rotating
the pattern is the call a gradient uses.

Immutable, also ruled: the GPU adaptor DEFERS draws to `endDraw`, so a brush
mutated between two fills in one frame would clobber the first fill's
parameters unless the backend snapshotted them (PackedBrush does exactly that
copying to survive `SolidColorBrush::setColor`, the one mutable brush in the
API). Fixed at creation = that class of bug cannot exist, and it matches the
gradient brushes. The program is shared through `ProgramCache` by blob
address, so a new brush per parameter change is a struct copy, not a compile;
`u_time` is the host's, so most animation needs no new brush at all.

Header `gmpi_ui/GmpiApiShaders.h`:

```cpp
enum class ShaderRenderer : int32_t { Spirv = 1, Metal = 2, Dx11 = 3 };
enum class ShaderStage    : int32_t { Quad = 1, Path = 2 };
struct ShaderBlob { ShaderRenderer renderer; ShaderStage stage; const uint8_t* data; int32_t size; };

struct IShaderBrush : IBrush { /* guid only: an immutable brush */ };

struct IShaderFactory : IUnknown {            // probed from IFactory
    ReturnCode createShaderBrush(const ShaderBlob* blobs, int32_t blobCount,
                                 const float* params, int32_t paramCount,   // up to 8 vec4s: u_params[8]
                                 const Color* fallbackColor,
                                 const BrushProperties* brushProperties,
                                 IShaderBrush** returnShaderBrush);
};
```

Authoring: the module writes `vec4 shade(vec2 p)` - straight (not
premultiplied) linear RGBA, like `gradient()` returns; the blend does the
premultiply - with `p` in brush space, `u_time.x`, `u_params[8]` and
`shader_utils.sh` in scope. **Precompiled blobs don't compose, but the fill
paths need to**: a rect is one quad program, but a path fill is coverage
sampled from the atlas times the brush colour. So the host owns the `main()`s
(`backends/gpu/shaders/template_quad.sc`, `template_path.sc`) and
`compile_shaders.cmake` in `USER_SHADER` mode compiles the user's function
into each template for each renderer, emitting one `shaders_<name>.cpp` that
defines a `ShaderBlob` table the module hands to `createShaderBrush`. Two
blobs per renderer, author never sees it.

Fallback rule: where a backend cannot run the program it paints
`fallbackColor` as a solid brush. On the GPU adaptor that is every draw that
is not GPU-native (the `viaCpu` cases: rotation/skew, geometry clips, opacity
brushes, text, strokes that widen on the CPU). The CPU backend answers
NoSupport from the probe, so a module that wants a prettier CPU fallback can
branch; one that doesn't still draws the same way everywhere.

Backend changes (`gmpi_ui`, branch `shader-api`):

| Where | Change |
|---|---|
| `backends/gpu/shader.h` | `Shader` gains `params[8][4]`, `paramCount`, `brushTransform[6]` (device -> brush). |
| `backends/gpu/uniforms.h`, `graphics_caches.h` | `u_params` (vec4 x8), `u_brush_transform` (vec4 x2); array uniform handles. |
| `backends/gpu/shape_batcher.{h,cpp}` | `submitShader` sets them; `PathFillWrapper` carries an optional `Shader*` and submits with its program. |
| `backends/gpu/canvas.h` | `fill(path, shader)`: the path-atlas fill with a shader instead of the packed brush. |
| `backends/GpuGfx.h` | `ShaderBrush` (IShaderBrush; holds the two `Shader`s, params, fallback `cpugfx::SolidColorBrush`); `Factory` serves `IShaderFactory` by aggregation and picks blobs by `bgfx::getRendererType()`; `fillRectangle`/`fillGeometry` (and the rect-figure shortcut) take the shader route, every other brush-taking method substitutes the fallback brush. Per draw the target interns one `Shader` per (brush, device transform) for the frame, so two fills under different transforms get their own brush matrix. |
| `GmpiApiShaders.h` | The interfaces above. |
| `shaders/template_*.sc`, `compile_shaders.cmake` | Host templates; USER_SHADER mode. |

Not in this cut: textures as shader inputs, render-to-texture feedback,
custom vertex shaders, named uniforms, post-effects over a region.

## The module: SE Fluid (`SynthEditLib/modules/GpuFx/`)

A new `gmpi_plugin` bundle `GpuFx.gmpi` (GUI-only, `HAS_GUI
IS_OFFICIAL_MODULE`, `Timer.cpp`), added under "# GMPI plugins" in
`modules/CMakeLists.txt` so it lands in `build/modules` with the rest. A new
bundle rather than a `Controls` member so the shader-compile step and the
committed blobs stay in one folder, and so it can be left out of a build
without a GPU backend.

Files: `FluidGui.cpp`, `fs_fluid.sc`, `shaders/compiled/shaders_fluid_spirv.cpp`
(+ metal, + dx11 when a Windows run exists), `CMakeLists.txt`.

Pins (`<GUI graphicsApi="GmpiGui">`), all float 0..10 unless noted:

| Pin | u_params slot | Meaning |
|---|---|---|
| Speed | 0.x | time scale |
| Scale | 0.y | noise frequency across the control |
| Turbulence | 0.z | domain-warp strength (0 = smooth blobs, high = tendrils) |
| Energy | 0.w | an external drive (wire a Peak Meter / envelope here): brightens and speeds the flow |
| Colour A, Colour B | 1, 2 | string pins "RRGGBB" → linear RGB, mixed by the noise field |
| Opacity | 3.x | 0..1 |

Fragment shader (`fs_fluid.sc`), the classic three-level domain-warped fBM:

```glsl
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) { /* value noise, smoothstep-interpolated */ }
float fbm(vec2 p) { float a = 0.5, f = 0.0; for (int i = 0; i < 5; ++i) { f += a * noise(p); p = p * 2.03 + 17.0; a *= 0.5; } return f; }

void main() {
vec4 shade(vec2 p) {   // p: brush space, 0..1 across the control
  float t = u_time.x * u_params[0].x * (1.0 + u_params[0].w);
  p = (p - 0.5) * u_params[0].y;
  float k = u_params[0].z;
  vec2 q = vec2(fbm(p + t * 0.10), fbm(p + vec2(5.2, 1.3) - t * 0.07));
  vec2 r = vec2(fbm(p + k * q + vec2(1.7, 9.2) + t * 0.15), fbm(p + k * q + vec2(8.3, 2.8) - t * 0.12));
  float f = fbm(p + k * r);
  vec3 rgb = mix(u_params[1].rgb, u_params[2].rgb, smoothstep(0.2, 0.9, f)) * (0.6 + 0.8 * u_params[0].w);
  return vec4(rgb, u_params[3].x); // straight linear RGBA; the host's template blends it
}
```

"Fluid-like" is the honest description: it is a stateless advection *look*
driven by time, not a simulation. It costs one quad and ~15 noise lookups per
pixel, has no state to lose on resize, and renders identically on every
renderer. A real Stam-style solver (ping-pong velocity/dye textures) is the
natural v2 once offscreens live on the GPU; the API above does not preclude
it (a `IShader` that reads a texture is one more field in `ShaderBlob`).

Module logic, in order of `render()`:

1. First call: `g.getFactory().as<IShaderFactory>()`; remember whether it
   exists.
2. On any pin change (and the first render): `createShaderBrush` with the
   generated blob table, the pins packed as `u_params`, the fallback colour
   (Colour A) and a brush transform mapping the control's bounds to 0..1.
3. `fillRectangle(bounds, shaderBrush)`. With no factory: a two-colour radial
   gradient whose centre wanders with `sin(time)` — recognisably "the same
   control", still cheap.
4. `onTimer` → `invalidateRect(&bounds)` at 30 Hz by default, 60 Hz while
   Energy > 0 (the dirty-rect model redraws only this control's quad; the rest
   of the persistent layer is untouched).
5. Stop the timer when Speed and Energy are both 0 (same pattern as Scope4),
   so a static blob costs nothing.

## Milestones

0. **Prerequisites.** (a) Confirm the module-timer finding in the Wayland
   editor and apply fix 1. (b) `canvas().updateTime` at `beginDraw`. (c) Build
   shaderc once (`GMPI_GPU_BUILD_SHADERC=ON`) and keep the binary outside the
   build tree. *Done when:* Scope4 animates in SynthEditWayland on both
   renderers, and a `GMPI_UI_GPU_TRACE`-style print shows `u_time` advancing.
1. **API + backend - DONE 2026-10-24 (uncommitted, gmpi_ui + gimpi_ui_tests
   branch `shader-api`).** 12/12 `GpuShader.*` under lavapipe and on the
   NVIDIA card; the full gimpi_ui_tests GPU suite is otherwise unchanged
   (one lavapipe-only text failure is pre-existing). Found on the way: bgfx
   `createProgram` refuses a vs/fs pair whose `$output`/`$input` lists differ
   (hashed), so each template's `$input` is its vertex shader's list verbatim;
   `u_color_mult` also undoes the gradient atlas's 1/64 storage scale, so user
   colours take only the HDR multiplier (carried in `u_brush_transform[0].w`);
   `vs_shape` grows every quad by half a pixel for the SDF edge fade, so a
   shader quad clamps to its own rect. Not done: `BrushProperties::opacity`
   (ignored), ellipses/strokes/text on the shader (fallback colour), the dx11
   blob (no Windows shaderc here). Header, the shader brush, `u_params`/brush transform,
   templates, compile script USER_SHADER mode. *Done when:* gimpi_ui_tests
   `GpuShader.*` (lavapipe, deterministic): a constant `shade()` fills its
   rect; `u_params` reach the shader; brush space follows the brush
   transform; a path fill (`fillGeometry`) is shaded; the clip is honoured; a
   rotated transform paints the fallback colour; the CPU factory answers
   NoSupport; two brushes from one blob table share one program.
2. **The module, GPU path.** `GpuFx` bundle, `fs_fluid.sc`, spirv + metal
   blobs committed. *Done when:* it appears in the browser, drops onto a
   panel, animates under `GMPI_UI_GPU=1` on the NVIDIA card, and the first
   frame's `[presentGpu]` trace stays in the ~2 ms band the editor is at
   today.
3. **Fallback and headless.** CPU drawing; `SynthEditCL --screenshot` of a
   panel holding one Fluid matches a reference PNG (CPU path → deterministic);
   the same on the GPU path under lavapipe with time pinned (`GMPI_UI_TIME=0`
   or a test hook on `updateTime`) gives a second reference.
4. **Windows blob + Mac check.** Regenerate `shaders_fluid_dx11.cpp` on a
   Windows machine (same shaderc version); run the module on the Mac Metal
   path, which `VISAGE_BACKEND_PLAN.md` notes is unverified since absorption.

## Risks and open questions

- **bgfx version coupling.** Blobs compiled by one shaderc only load in the
  same bgfx; the module's blobs must be regenerated whenever gmpi_ui bumps
  bgfx. Mitigation: the module's compile step is the same script as gmpi_ui's,
  and the script already records the shaderc version in the generated file.
  Third-party module authors inherit this constraint; the API doc must say so.
- **Plugin GUIs.** VST3/CLAP editors are not on the GPU path yet ("still to
  do", plan milestone 4), so in a DAW the module shows its fallback. Fine for a
  first module; it is the argument for keeping the fallback decent.
- **Premultiplied/HDR.** `u_color_mult` and the fp16 layer are easy to get
  wrong by one factor; the constant-colour test in milestone 1 pins it against
  the CPU path's encode of the same colour.
- **Visage licence.** The `.sc` includes (`shader_include.sh`,
  `shader_utils.sh`) are MIT from Vital Audio; a module compiled against them
  carries that notice — already the case for gmpi_ui's own shaders.
- **Where this document lives.** It belongs next to
  `VISAGE_BACKEND_PLAN.md` in gmpi_ui (as `SHADER_API_PLAN.md`); it is in the
  session notes folder until Jeff says to put it there.
