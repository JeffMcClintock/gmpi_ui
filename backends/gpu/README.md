# backends/gpu — gmpi_ui's GPU renderer

The renderer under `backends/GpuGfx.h`: half-float layers, an exact
path-coverage atlas, gradient and image atlases, SDF rectangles and circles,
post effects and custom shaders, over [bgfx](https://github.com/bkaradzic/bgfx)
(Direct3D 11 / Metal / Vulkan). Namespace `gmpi::gpu`.

## Origin

Absorbed from [VitalAudio/visage](https://github.com/VitalAudio/visage) by way
of the gmpi fork, [JeffMcClintock/visage](https://github.com/JeffMcClintock/visage)
branch `gmpi`, at commit `4eeb51c6b92cc151772ad61b9edc157ba04b1ede`
(2026-10-22). Visage is MIT licensed; every file that came from it keeps its
copyright and permission notice. The fork's history stays in that repository.

What changed on the way in, in short (`VISAGE_BACKEND_PLAN.md` has the table
and the reasons):

- **Kept:** canvas, layers and regions, the shape batcher, the path coverage
  atlas (per-edge exact coverage, R32F), gradient and half-float image
  atlases, post effects, `Shader` and the shader/program caches, the renderer
  singleton, screenshots.
- **Dropped:** text, fonts and emoji (gmpi_ui's `CpuTextEngine` draws text),
  SVG, theme and palette, the SDF shapes the adaptor does not draw (rounded
  rectangles, squircles, arcs, segments, triangles, Béziers, diamonds), graph
  and heat-map shapes, the Visage stroker, curve flattening and SVG path
  parsing (`Path` is now flattened polygons; cpugfx widens strokes),
  encoded-image decoding, 8-bit and float data atlases, PNG saving, colour
  and gradient serialisation, the background render thread, WebGL shader
  preprocessing, and Visage's string, file, time, thread and dimension
  utilities.
- **Not absorbed:** `visage_ui`, `visage_windowing`, `visage_app`,
  `visage_widgets`, the examples, and the 29 MB of prebuilt shader compilers.

## Building

gmpi_ui has no CMake of its own; consumers list its sources. The renderer is
the exception because it fetches bgfx, so it is one fragment:

```cmake
include(${GMPI_UI_SDK}/backends/gpu/gmpi_gpu.cmake)
target_link_libraries(my_target PRIVATE gmpi_gpu)
target_compile_definitions(my_target PRIVATE GMPI_UI_GPU_BACKEND=1)
```

bgfx is fetched and patched at a pinned tag (`patch_bgfx.cmake` says what
the patch does and why). Platform macros (`GMPI_GPU_LINUX` and so on) come
from `defines.h`, so consumers pass nothing.

## Shaders

The `.sc` sources live in `shaders/`. Their compiled form is committed, one
C++ file per renderer in `shaders/compiled/`, so a build needs neither
shaderc nor an embed step:

| Renderer | File | Platform |
|---|---|---|
| Vulkan (SPIR-V) | `shaders_spirv.cpp` | Linux |
| Metal | `shaders_metal.cpp` | macOS |
| Direct3D 11 | `shaders_dx11.cpp` | Windows — not generated yet |

After changing a shader, regenerate them:

```sh
cmake -DSHADERC=/path/to/shaderc -P backends/gpu/shaders/compile_shaders.cmake
```

shaderc must come from the same bgfx as the build, because the shader binary
format is versioned. Configure any consumer with `-DGMPI_GPU_BUILD_SHADERC=ON`
and build the `shaderc` target to get one. SPIR-V and Metal can be produced on
any host. Direct3D needs shaderc on Windows (`-DRENDERERS=dx11`), because the
HLSL compiler is Microsoft's.

Custom shaders and post effects are kept for a future gmpi_ui feature; the
GMPI adaptor does not drive them yet.

## Tests

`gimpi_ui_tests` with `-DGMPI_UI_TESTS_GPU_BACKEND=ON`: the drawing tests run
through `GpuGfx.h` against the Direct2D references, `gpu_backend_tests.cpp`
covers the adaptor's own seams, and `gpu_renderer_tests.cpp` pins the
renderer's precision properties below the adaptor (ported from the fork's
Catch2 suite). On a machine without a GPU, Mesa's lavapipe will run them:
`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`.
