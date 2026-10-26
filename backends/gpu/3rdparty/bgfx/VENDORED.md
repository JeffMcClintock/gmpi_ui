# Vendored bgfx (bx, bimg, bgfx)

A subset of [bgfx](https://github.com/bkaradzic/bgfx) and its two libraries,
[bimg](https://github.com/bkaradzic/bimg) and [bx](https://github.com/bkaradzic/bx),
taken from [bgfx.cmake](https://github.com/bkaradzic/bgfx.cmake) at the tag
and commits in `VERSIONS`. All three are BSD-2-Clause, Copyright Branimir
Karadžić; each directory keeps its `LICENSE`. The Vulkan headers
(`bgfx/3rdparty/khronos`) carry their licence in each file; the DirectX
headers keep `directx-headers/LICENSE` (MIT); `astcenc.h` keeps
`astc-encoder/LICENSE.txt` (Apache-2.0).

Built by `../../gmpi_gpu.cmake` as three static libraries, `gmpi_bx`,
`gmpi_bimg` and `gmpi_bgfx`. Nothing is fetched or patched at configure time.

## What is here, and what is not

Only what gmpi_ui's renderer compiles, measured from the compiler's
dependency records (see `GPU_SINGLE_DEPENDENCY_PLAN.md`, milestone 0), about
6.5 MB against ~600 MB for a bgfx.cmake checkout:

- **One renderer per platform**, the current API on each: Vulkan
  (`renderer_vk`, `khronos/vulkan-local`) on Linux, Direct3D 12
  (`renderer_d3d12`, `dxgi`, `nvapi`, a 14-file subset of `directx-headers`)
  on Windows, Metal (`renderer_mtl.mm`) on Apple. Direct3D 11, OpenGL/GLES,
  WebGPU and the console renderers are not vendored; `gmpi/renderer_stubs.cpp`
  provides the entry points bgfx.cpp's renderer table still names, and
  asserts they stay compiled out.
- **bimg's core only** (`image.cpp`, `image_gnf.cpp`): the format table and
  conversions bgfx needs. Its codecs and container decoders are not vendored;
  every texture the renderer makes is raw pixels. `astcenc.h` is here only
  because `image.cpp` includes it unconditionally; `BIMG_DECODE_ASTC=0`
  leaves it unused.
- **bx** in full apart from tests and the freebsd/ios/mingw compat shims.
- `bgfx/3rdparty/stb/stb_rect_pack.h`, used by the renderer itself
  (`graphics_utils.cpp`), and `renderdoc_app.h`.
- Not here: shaderc and the other tools, examples, the remaining
  third-party trees (glslang, spirv-tools, spirv-cross, ...). shaderc is
  fetched on demand by `../../tools/shaderc.cmake` at the same tag.

## Ours, not upstream's

- `gmpi/amalgamated.cpp` / `.mm`: the single translation unit, listing only
  the vendored sources (bgfx's own `amalgamated.cpp` includes every renderer).
- `gmpi/renderer_stubs.cpp`: see above.
- `gmpi/astcenc_stubs.cpp`: `image.cpp` guards the ASTC decoder with a plain
  `if`, so a Debug build links astcenc calls the branch never makes. These
  stubs answer them; the ASTC encoder itself is not vendored.
- `gmpi.patch`: the changes to upstream source, applied by the vendoring
  script. Two, both in `renderer_vk.cpp`:
  1. **A stable pipeline key** (2026-10-25). bgfx hashed each pipeline with
     its render pass's *handle*, a pointer that differs every run, and the
     same hash keys the on-disk pipeline cache (`CallbackI::cacheRead/Write`,
     which `gpu/renderer.cpp` implements) - so that cache could never hit.
     The patch hashes the render pass's description (formats, samples,
     resolve: what Vulkan render-pass compatibility means), which
     `getRenderPass` already computes and caches by. Measured: a cold first
     frame with NVIDIA's own shader cache off drops from 94 ms to 15 ms of
     submit+present. Worth offering upstream.
  2. It marks `VK_PRESENT_MODE_MAILBOX_KHR` as a no-vsync mode: bgfx's table treats MAILBOX as vsync, so "no vsync"
  accepts only IMMEDIATE, which Mesa's Wayland WSI does not offer, and the
  swap chain falls back to FIFO. Mesa's FIFO throttles on frame callbacks:
  the commit of frame N happens inside the NEXT WSI call, so a window that
  presents once after being idle shows that frame one present late.
  MAILBOX commits immediately, so it is the no-vsync mode here.

**Never edit files under `bx/`, `bimg/` or `bgfx/` directly.** A change to
upstream code goes in `gmpi.patch`, or it is lost at the next re-vendor.

## Updating

```sh
backends/gpu/tools/vendor_bgfx.sh            # clones bgfx.cmake at the script's TAG
backends/gpu/tools/vendor_bgfx.sh <checkout> # or vendors from a pristine local checkout
```

The script wipes `bx/`, `bimg/`, `bgfx/`, copies the file lists in it,
applies `gmpi.patch` and rewrites `VERSIONS`. After a version bump, also bump
`GMPI_SHADERC_TAG` in `tools/shaderc.cmake` (configure fails if they differ)
and regenerate every compiled shader blob - gmpi_ui's
(`shaders/compile_shaders.cmake`) and every module's - because the shader
binary format is versioned.
