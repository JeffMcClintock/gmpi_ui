# gmpi_ui as a single dependency, GPU backend included (plan, 2026-10-24)

## The problem, precisely

The renderer's *code* already lives in gmpi_ui (`backends/gpu/`, absorbed from
Visage on 2026-10-23). What is not in gmpi_ui is what it stands on:

- **bgfx is a second dependency, fetched at configure time.** `gmpi_gpu.cmake`
  runs `FetchContent_Declare(bgfx GIT_REPOSITORY bgfx.cmake GIT_TAG
  v1.129.8958-499 ...)`: a shallow clone of bgfx.cmake plus its three
  submodules (bgfx, bimg, bx). On this machine that is **592 MB** in
  `build/_deps/bgfx-src`, of which the parts a build compiles are ~10 MB; the
  rest is git objects and tool-only third-party trees (glslang, spirv-tools,
  spirv-cross, meshoptimizer, dear-imgui, ...).
- **It is patched after fetching** (`patch_bgfx.cmake`: the Vulkan present-mode
  table, and dropping `find_package(OpenGL REQUIRED)`), with a
  `GMPI_BGFX_PATCH_LEVEL` knob because FetchContent only re-patches when the
  command text changes. Fragile by construction.
- **Every consumer wires it separately.** SynthEditWayland, TideSynth,
  gimpi_ui_tests and WaylandBench each `include(gmpi_gpu.cmake)`, each add
  `GMPI_UI_GPU_BACKEND=1`, and each fetches its own 592 MB (no shared cache:
  FetchContent, not CPM).
- **The GPU backend is not on gmpi_ui `main`.** It is 20 commits on
  `gpu-backend` (+3 on `shader-api`); SynthEdit and gimpi_ui_tests track it on
  their own `gpu-backend` branches, and TideSynth's `TIDE_GPU_BACKEND` option
  (default ON) has a guard that silently falls back to the CPU renderer when a
  gmpi_ui without `backends/gpu` is fetched - which is what a fetch of
  `origin/main` gives today.
- Underneath all of it: **gmpi_ui has no CMake of its own.** "Consumers list
  its sources" (SynthEditLib/CMakeLists.txt lines ~58-112 and ~531-541 are one
  such list, gimpi_ui_tests has another, TideSynth a third). The renderer was
  the first part that could not work that way, hence the fragment.

"Single dependency" therefore means two things, and they are separable:

1. **One repository.** A consumer that has gmpi_ui has everything the GPU
   backend needs. No network at configure time beyond gmpi_ui itself, no
   patch step, no submodules.
2. **One target.** A consumer says `add_subdirectory(gmpi_ui)` (or
   FetchContent) and `target_link_libraries(x PRIVATE gmpi_ui)`, with the GPU
   backend an option on that target, instead of copying a source list.

(1) is the real pain and the smaller change; (2) is a refactor of four
consumers' CMake. Recommend (1) now, (2) as its own step after.

## Decision: vendor the bgfx subset into gmpi_ui

Put the compiled subset of bx, bimg and bgfx under
`backends/gpu/3rdparty/bgfx/`, at the pinned tag, with the two patches applied
in the tree and recorded as a diff file beside it. The `gmpi_gpu` target then
compiles three amalgamated translation units from the repository and fetches
nothing.

Why this and not the alternatives:

| Option | Verdict | Why |
|---|---|---|
| **Vendor the compiled subset** | **Do it** | Removes the fetch, the patch step and the version drift in one move; bgfx's own build model (one amalgamated .cpp per library) makes the subset small and the CMake trivial. ~10 MB in the repo, measured below. |
| git submodule | No | A submodule *is* the second dependency, just hidden: every clone needs `--recursive`, CI workflows need changing, and SE_DSP_CORE in SynthEdit shows how such things rot. |
| CPM with the shared cache | No | Still a network fetch and still a patch step; only the duplication goes away. |
| System package | No | No distro ships bgfx; Windows and macOS have no package at all. |
| Drop bgfx for own backends | Not now | `METAL_BACKEND_PLAN.md` option B (a metal-cpp device under the renderer's seam) is the right direction for Apple and would make the vendored bgfx Windows/Linux-only. Vendoring does not block it; it shrinks what it has to replace. |

Licence: bgfx, bimg and bx are BSD-2-Clause (Branimir Karadžić). Each keeps
its LICENSE file in the vendored tree; `backends/gpu/README.md` already names
Visage's MIT notice and gains the bgfx one.

### What gets vendored (sizes from the fetched tree at v1.129.8958-499)

| Part | Size | Needed because |
|---|---|---|
| `bx/include`, `bx/src` (amalgamated) | 1.2 MB | everything in bgfx stands on bx |
| `bx/3rdparty/ini` | tiny | `settings.cpp` in the amalgamation (drop `catch`: tests only) |
| `bimg/include`, `bimg/src/image.*`, `image_gnf.cpp` | 350 KB | bgfx's texture formats and conversions |
| `bimg/3rdparty/astc-encoder` (the decoder part bimg's core links) | ~3.3 MB | `image.cpp` decodes ASTC; measure whether `BIMG` can be built with ASTC decode compiled out - if so this goes, and the whole of `bimg/3rdparty` with it |
| `bgfx/include`, `bgfx/src` (amalgamated) | 3.0 MB | the renderer; renderers are selected by `BGFX_CONFIG_RENDERER_*` defines, so the unused ones compile to nothing but their sources stay |
| `bgfx/3rdparty/khronos` | 3.6 MB → trim to `vulkan/` | Vulkan headers for Linux; GL/GLES/EGL headers go with the GL renderers we compile out |
| `bgfx/3rdparty/directx-headers` | 4.6 MB → measure | D3D12 and the WSL stubs; D3D11 uses the Windows SDK's headers plus bgfx's own `dxgi.h`. If D3D12 is compiled out (we ask for D3D11) this may go entirely |
| `bgfx/3rdparty/renderdoc` | 36 KB | `debug_renderdoc.cpp` in the amalgamation; keep |
| `bgfx/src/*.bin.h`, `*.sc` | included above | bgfx's own clear/debug shaders, embedded |

Not vendored: `bgfx/tools` (shaderc, texturec, ...), `bgfx/examples`,
`bgfx/3rdparty/{glslang,spirv-tools,spirv-cross,glsl-optimizer,fcpp,
dear-imgui,meshoptimizer,cgltf,stb,sdf,iconfontheaders,native_app_glue}`,
`bimg/3rdparty/{nvtt,libsquish,etc1,etc2,pvrtc,edtaa3,iqa,lodepng,tinyexr}`
(encoders and decoders bimg's *encode/decode* libraries use, not its core),
`bgfx.cmake`'s own CMake. Expected total **8-12 MB**, versus 592 MB fetched
today and 29 MB the fork once carried for prebuilt shaderc.

### What we actually use of bgfx (measured 2026-10-24)

Jeff's point: there may be parts we simply don't use. There are, and they are
most of it. `grep bgfx::` over `backends/gpu` and the adaptor finds **61
distinct symbols**, ~40 of them calls, all of them the plain 2D-sprite
subset: `init/reset/frame/getCaps/getStats/getRendererType`,
`createTexture2D/updateTexture2D/readTexture/getTexture`,
`createFrameBuffer`, `createShader/createProgram/createUniform/setUniform/
setTexture`, `createVertexBuffer/createIndexBuffer/allocTransient*Buffer(s)/
setVertexBuffer/setIndexBuffer`, `setState` (blend equation and write mask
only), `setViewRect/setViewFrameBuffer/setViewClear/setViewMode`, `submit`,
`blit`, `requestScreenShot`, `copy/makeRef/alloc`. Nothing else: no compute,
no instancing, no indirect draws, no occlusion queries, no 3D textures or
cube maps, no dynamic buffers, no encoder API, no debug text, no image
containers (every texture is raw pixels we made ourselves).

What that lets us drop, in three tiers:

1. **Whole renderers and tools - free.** OpenGL, GLES, WebGPU, GNM, D3D12 and
   everything only they include; shaderc/texturec/geometryc and their
   third-party trees. Already in the plan above.
2. **bimg almost entirely - cheap.** bgfx's core needs bimg for its texture
   *format table* (`bimg::getBlockInfo`, `imageGetSize`, the few conversions
   `updateTexture2D` may do) and for parsing DDS/KTX/PVR containers in
   `createTexture`, which we never call with a container. Vendoring
   `bimg/src/image.cpp` with the ASTC/ETC/PVRTC decode branches compiled out
   (or stubbed to "unsupported") removes `bimg/3rdparty` (3.7 MB) outright and
   leaves ~350 KB. Milestone 0 confirms the define names.
3. **Inside bgfx itself - not worth it.** `bgfx.cpp` is one 6 kLOC file that
   implements the command buffer for every feature; cutting compute or
   instancing out of it means forking bgfx, and the saving is binary size,
   not repository size or build time (the amalgamated TU already compiles in
   one go). Leave it; the unused paths cost nothing a consumer can feel.

And the strategic reading: 61 symbols is a small seam. It is the same seam
`METAL_BACKEND_PLAN.md` option B names ("the bgfx call surface, ~60 entry
points") for putting a metal-cpp device under the renderer. Once that seam is
an interface, bgfx is one implementation of it (Vulkan, D3D11) and could be
replaced per platform without touching the renderer - which is the honest
long-term answer to "parts we don't use": we use a 2D subset, and a 2D
device is a few thousand lines per API. Not this plan's scope; vendoring is
the step that costs least and keeps that door open.

### shaderc: a tool, fetched only when asked

Compiling `.sc` to blobs needs bgfx's shaderc, which drags in glslang and
spirv-tools (most of the 592 MB). It stays out of the repository.
`GMPI_GPU_BUILD_SHADERC=ON` keeps working, but moves to its own fragment,
`backends/gpu/tools/shaderc.cmake`, which FetchContents bgfx.cmake **at the
same tag the vendored subset came from** and builds only the `shaderc`
target. Nothing in a normal build touches it. The blob format is versioned,
so the tag in that file and the tag in `3rdparty/bgfx/VENDORED.md` are one
number, checked by a CMake assert.

### The renderer set, decided once: one current API per platform

Ruled by Jeff 2026-10-24: **Direct3D 12 on Windows, not 11** - this is a new
API and is not to be tied to old, crufty stuff. So exactly one bgfx renderer
per platform, each the platform's current one:

| Platform | Renderer | Shader blob | Minimum OS |
|---|---|---|---|
| Windows | Direct3D 12 | DXBC, shader model 5 (`shaderc -p s_5_0`; bgfx's D3D12 renderer consumes the same DXBC as D3D11 did) | Windows 10 |
| Linux | Vulkan | SPIR-V | any with a Vulkan ICD (lavapipe for the rest) |
| Apple | Metal | Metal | macOS 10.13 / iOS 11 per bgfx; realistically what SynthEdit already requires |

Everything else is compiled out everywhere: `BGFX_CONFIG_RENDERER_DIRECT3D11=0`,
`OPENGL=0`, `OPENGLES=0`, `WEBGPU=0`, `GNM=0`, `NVN=0`, `AGC=0`. In
`renderer.cpp:111-115` the "D3D11, or D3D12 when offered" preference becomes
"D3D12, or nothing" (the host then takes the CPU path, exactly as on a Linux
box without Vulkan). The GL headers leave the tree; `directx-headers` stays
(it is what bgfx's D3D12 renderer compiles against; trim it to
`include/directx` and the WSL stubs, ~2 MB). The `find_package(OpenGL)`
patch disappears with the GL renderer: the vendored CMake is ours, there is
nothing to patch.

Two renames before the API is public, both in `GmpiApiShaders.h` and
`compile_shaders.cmake`: `ShaderRenderer::Dx11` → **`Dxbc`** (name the blob
format, as `Spirv` and `Metal` do; D3D11 never ships), and the comment that
says D3D11 and 12 share it goes. `Renderer::ShaderBinary::Dx11` likewise.
Done now, that is a one-line change in two committed-but-unmerged branches;
done after a module ships blobs, it is an ABI enum value forever.

### Updating bgfx later

`backends/gpu/3rdparty/bgfx/VENDORED.md` records the tag, the exact
subdirectories copied, and the two local changes as a unified diff
(`gmpi.patch`). A script `backends/gpu/tools/vendor_bgfx.sh <tag>` does the
copy from a fresh bgfx.cmake clone, applies the patch, and prints the size,
so a bump is one command plus a shader regeneration (the blob format
changes with bgfx; `compile_shaders.cmake` already records the shaderc
version in every generated file, and the module blobs - `shaders_fluid.cpp`,
`shaders_test_shade.cpp` - are regenerated the same way).

## Milestone 0 results (2026-10-24, gmpi_ui branch `vendor-bgfx`, uncommitted)

Measured on Linux from the compiler's own dependency records (`ninja -t
deps` over every bx/bimg/bgfx object), with only Vulkan compiled in.

**Done on the branch** (all verified: gimpi_ui_tests 264/264 on NVIDIA,
263 + the known lavapipe-only text case; SynthEdit ctest 181/181; SE Fluid
still one shader quad per frame on the GPU editor):

- `ShaderRenderer::Dx11` → **`Dxbc`** in `GmpiApiShaders.h`, likewise
  `Renderer::ShaderBinary`, the `dxbc` key in `compile_shaders.cmake` and
  `shaders_dxbc.cpp` in `gmpi_gpu.cmake`/README.
- `renderer.cpp`: Windows asks for Direct3D 12 only; without it the renderer
  reports unsupported ("Direct3D 12 is not available.") and the host takes
  the CPU path. The `USE_DIRECTX12` switch is gone.
- `gmpi_gpu.cmake`: every bgfx renderer is named, on or off, on every
  platform - D3D12 on Windows, Metal on Apple, Vulkan on Linux, the other
  eight off.
- **bimg trimmed to its own two files.** `BIMG_DECODE_ASTC=0` (bimg's existing
  switch) and the astc-encoder and miniz sources dropped from the `bimg`
  target: `libbimg.a` now holds `image.cpp.o` and `image_gnf.cpp.o` only.
  miniz turned out to be referenced by nothing in bimg's core at all
  (bgfx.cmake links it in regardless). GCC eliminates the dead ASTC branch
  even at `-O0` (`nm -u` shows no astcenc symbols); MSVC Debug is unchecked -
  if it keeps the references, one `#if` around that branch goes in
  `gmpi.patch`.

**What a vendored tree needs** (bytes of source actually compiled or
included):

| Part | Size | Notes |
|---|---|---|
| bx + bimg + bgfx core, platform-independent | ~2.3 MB | 22 bx sources via its amalgamation, bimg's 2 files, bgfx.cpp/shader/topology/vertexlayout, the public headers, tinystl, renderdoc stub |
| + Linux: Vulkan headers + `renderer_vk` | +1.65 MB | `vulkan_core.h` alone is 1.2 MB |
| + Windows: `renderer_d3d12`, `dxgi`, `nvapi`, `shader_dxbc` | +0.4 MB | |
| + Windows: DirectX headers (d3d12*, dxgi*, d3dcommon) | ~+2 MB | from 4.5 MB of `directx-headers`; the exact set settles on a Windows build |
| + Apple: `renderer_mtl.mm/.h` | +0.2 MB | |
| **All three platforms** | **~6.5 MB** | down from 592 MB fetched; below the 8-12 MB first estimate |

Also learned: bgfx's `amalgamated.cpp` `#include`s **every** renderer
source (GL, D3D11, GNM, AGC, NVN, ...), relying on `#if` guards inside each.
The vendored tree should carry **our own** `amalgamated.cpp` listing only
`bgfx.cpp`, `debug_renderdoc.cpp`, `renderer_noop.cpp`, `shader.cpp`,
`shader_spirv.cpp`, `topology.cpp`, `vertexlayout.cpp` plus the platform's
renderer (`renderer_vk.cpp`; `renderer_d3d12.cpp` + `dxgi.cpp` + `nvapi.cpp`
+ `shader_dxbc.cpp`; `renderer_mtl.mm`), so the unused renderers' sources
(~1 MB: `renderer_gl` 277 KB, `renderer_d3d11` 208 KB, the GL contexts,
console stubs) need not be vendored at all.

## Milestones

0. **Measure before copying - DONE, see above.** Build the GPU tests with D3D12, GL, GLES,
   WebGPU compiled out and bimg's ASTC decode off (`BIMG_DECODE_ASTC`-style
   define, or a stub) on Linux; confirm which third-party directories are
   still included (`-H` / `-showIncludes` over the three amalgamated TUs).
   *Done when:* a list of files actually compiled or included, with a size.
1. **Vendor - DONE 2026-10-24 (uncommitted, gmpi_ui `vendor-bgfx`).**
   `backends/gpu/3rdparty/bgfx/` holds 208 files, 6.9 MB on disk
   (`VENDORED.md` there is the reference). Built as `gmpi_bx`, `gmpi_bimg`,
   `gmpi_bgfx` (prefixed so they cannot collide with a consumer's own bgfx);
   `patch_bgfx.cmake` deleted, `gmpi.patch` applied in the tree by
   `tools/vendor_bgfx.sh`. Our own `gmpi/amalgamated.{cpp,mm}` lists only the
   vendored sources, and `gmpi/renderer_stubs.cpp` (with static_asserts)
   replaces the GL / D3D11 / console renderer sources, which bgfx.cpp's
   renderer table still names. Found on the way: the renderer itself needs
   `bgfx/3rdparty/stb/stb_rect_pack.h` (milestone 0 only measured the bgfx
   libraries); and `tests/run.sh --demo-gpu` / `x11_gpu_screenshot.sh` linked
   `-lSM -lICE -lXrandr -lGL` for bgfx's GL path - not needed any more, and
   not installed here. Verified: a fresh gimpi_ui_tests build has no
   `_deps/bgfx-*` and a configure log that never mentions bgfx; 264/264 on
   NVIDIA, 263 + the known text case under lavapipe; SynthEdit rebuilt after
   deleting its fetched bgfx: ctest 181/181, SE Fluid one shader quad per
   frame; the GPU Wayland demo builds and runs. Earlier text: `tools/vendor_bgfx.sh`, the copied subset, `VENDORED.md`,
   `gmpi.patch` applied. `gmpi_gpu.cmake` builds `bx`, `bimg`, `bgfx` from the
   tree (three `add_library(STATIC)` of one amalgamated file each, the
   `BGFX_CONFIG_*` defines it sets today, warnings off) and drops
   FetchContent and `patch_bgfx.cmake`. *Done when:* gimpi_ui_tests builds
   and passes (264 on NVIDIA, 263 + the known text case under lavapipe) with
   `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` and no `_deps/bgfx-*` in the build
   tree; SynthEdit's GPU build does the same; `GpuShader.*` and SE Fluid
   still draw.
2. **shaderc fragment - DONE 2026-10-24 (same branch).** `tools/shaderc.cmake`
   fetches bgfx.cmake at `GMPI_SHADERC_TAG` as `gmpi_bgfx_tools` and fails
   configure if that differs from the tag in `3rdparty/bgfx/VERSIONS`.
   shaderc 1.18.129 from it regenerates `shaders_spirv.cpp`,
   `shaders_metal.cpp` and SE Fluid's `shaders_fluid.cpp` byte-identical to
   the committed ones. Earlier text: `tools/shaderc.cmake`; regenerate all four blob
   sets with it to prove the tag matches; `compile_shaders.cmake`'s comment
   points at it. *Done when:* a fresh build-uitests-gpu with
   `GMPI_GPU_BUILD_SHADERC=ON` produces a shaderc whose blobs are
   byte-identical to the committed ones.
3. **Land on `main`.** Merge `gpu-backend` + `shader-api` + the vendoring
   into gmpi_ui `main` (all fast-forwards today, no conflicts); then
   gimpi_ui_tests `shader-api` → `main`; then TideSynth loses its "no
   backends/gpu" guard (the fetch of `origin/main` now has it); SynthEdit's
   `gpu-backend` → `master`. Order matters: gimpi_ui_tests CI fetches gmpi_ui
   at `origin/main`. *Done when:* every consumer's CI configures from
   `origin/main` of gmpi_ui and builds the GPU backend without network
   access to github.com/bkaradzic.
4. **One target (separate, optional).** `gmpi_ui/CMakeLists.txt` defining
   `gmpi_ui` (the CPU backend, helpers, the platform frames behind
   `GMPI_UI_WAYLAND`/`X11`/`WIN32`/`COCOA` options) and `gmpi_ui_gpu` behind
   `GMPI_UI_GPU`, usable via `add_subdirectory` and FetchContent; then
   replace the three source lists in SynthEditLib, gimpi_ui_tests and
   TideSynth with `target_link_libraries`. *Done when:* no consumer names a
   gmpi_ui source file. This is a week of CMake archaeology across four
   repos and three platforms; it does not block 1-3.

## Risks

- **Repository growth.** 8-12 MB of third-party source in a repo that is
  4.3 MB of history today. Acceptable for a renderer dependency (HarfBuzz
  would be 17 MB if vendored the same way; it stays on CPM because nothing
  patches it). Trimming (milestone 0) is what keeps it near the low end.
- **Windows is unverified on `gpu-backend` as a whole** (`shaders_dx11.cpp`
  not generated; no Direct3D path has ever run). Going D3D12-only is the
  moment to run it, and the only place the DXBC blobs can be generated
  (Microsoft's HLSL compiler): a Windows build is on milestone 1's critical
  path, not an afterthought.
- **macOS**: bgfx's Metal renderer is Objective-C++ (`amalgamated.mm`), and
  `gmpi_gpu.cmake` already enables OBJCXX; the vendored CMake must keep that.
  Unverified since absorption (noted in VISAGE_BACKEND_PLAN.md).
- **Divergence from upstream** becomes invisible without the patch file.
  Hence `gmpi.patch` as the single source of truth for local changes, and the
  rule that any edit to `3rdparty/bgfx/` goes through it.
- **Two copies during transition.** Until milestone 3 lands, `gpu-backend`
  consumers fetch and `main` consumers would vendor; keep the vendoring on
  the same branch chain (`shader-api` → new branch `vendor-bgfx`) so there is
  never a `main` that fetches.
