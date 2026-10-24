# gmpi_ui's GPU renderer, for backends/GpuGfx.h. Include this file from a
# consumer's CMakeLists and link the target:
#
#   include(${GMPI_UI_DIR}/backends/gpu/gmpi_gpu.cmake)
#   target_link_libraries(my_target PRIVATE gmpi_gpu)
#   target_compile_definitions(my_target PRIVATE GMPI_UI_GPU_BACKEND=1)
#
# The renderer is bgfx underneath (fetched here, pinned and patched). The
# shaders are committed already compiled, one C++ file per renderer
# (shaders/compiled/), so a build needs neither shaderc nor an embed step.
# After editing a .sc file, regenerate them: shaders/compile_shaders.cmake.
# GMPI_GPU_BUILD_SHADERC=ON builds bgfx's shaderc for that.
#
# Origin: absorbed from VitalAudio/visage (MIT); see README.md here.

include_guard(GLOBAL)

set(GMPI_GPU_DIR ${CMAKE_CURRENT_LIST_DIR})

option(GMPI_GPU_BUILD_SHADERC "Also build bgfx's shaderc, to regenerate backends/gpu/shaders/compiled" OFF)
option(GMPI_GPU_DEBUG_LOGGING "bgfx trace output on stderr (debug builds)" OFF)

if (APPLE)
  # bgfx's Metal renderer is Objective-C++. CMake honours enable_language
  # only at directory scope, so the includer's directory must have it.
  enable_language(OBJC OBJCXX)
endif ()

# --- bgfx --------------------------------------------------------------------
include(FetchContent)
set(BGFX_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_EXAMPLE_COMMON OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS ${GMPI_GPU_BUILD_SHADERC} CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS_SHADER ${GMPI_GPU_BUILD_SHADERC} CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS_BIN2C OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS_TEXTURE OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(BGFX_INSTALL OFF CACHE INTERNAL "" FORCE)
set(BX_AMALGAMATED ON CACHE INTERNAL "" FORCE)
set(BX_CONFIG_DEBUG OFF CACHE INTERNAL "" FORCE)
set(BGFX_AMALGAMATED ON CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_DEBUG_ANNOTATION ${GMPI_GPU_DEBUG_LOGGING} CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_MAX_FRAME_BUFFERS 1024 CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_MAX_VIEWS 256 CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_MAX_DRAW_CALLS 8192 CACHE INTERNAL "" FORCE)
set(BGFX_CONFIG_MULTITHREADED 0 CACHE INTERNAL "" FORCE)
# Vulkan presents on a wl_surface without bgfx's Wayland/EGL support.
set(BGFX_WITH_WAYLAND OFF CACHE INTERNAL "" FORCE)

# GMPI_BGFX_PATCH_LEVEL is in the command so that editing patch_bgfx.cmake and
# bumping it re-applies the patch to an existing build tree: FetchContent
# re-runs a patch step only when its command changes.
FetchContent_Declare(bgfx GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
  GIT_SHALLOW TRUE
  GIT_TAG v1.129.8958-499
  PATCH_COMMAND ${CMAKE_COMMAND} -DBGFX_SRC=<SOURCE_DIR> -DGMPI_BGFX_PATCH_LEVEL=2 -P ${GMPI_GPU_DIR}/patch_bgfx.cmake
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(bgfx)

# Linux renders through Vulkan only (renderer.cpp asks for nothing else, and
# a machine without it keeps the CPU path), so bgfx's OpenGL renderer is dead
# code there - and the one thing that needed the GL development headers
# (renderer_gl.h includes <GL/gl.h>). Naming any renderer switches bgfx's
# config.h to "only what is named", so Vulkan is named too. patch_bgfx.cmake
# drops the find_package(OpenGL REQUIRED) that went with it. Windows and
# macOS keep bgfx's defaults.
if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_VULKAN=1 BGFX_CONFIG_RENDERER_OPENGL=0
                                          BGFX_CONFIG_RENDERER_OPENGLES=0)
endif ()

if (MSVC)
  set(GMPI_GPU_NO_WARNINGS /W0)
else ()
  set(GMPI_GPU_NO_WARNINGS -w)
endif ()
foreach (t bgfx bx bimg)
  if (TARGET ${t})
    target_compile_options(${t} PRIVATE ${GMPI_GPU_NO_WARNINGS})
  endif ()
endforeach ()
target_compile_definitions(bx PRIVATE BX_CONFIG_EXCEPTION_HANDLING_USE_WINDOWS_SEH=0)

# --- the renderer ------------------------------------------------------------
add_library(gmpi_gpu STATIC
  ${GMPI_GPU_DIR}/canvas.cpp
  ${GMPI_GPU_DIR}/color.cpp
  ${GMPI_GPU_DIR}/gradient.cpp
  ${GMPI_GPU_DIR}/graphics_caches.cpp
  ${GMPI_GPU_DIR}/graphics_utils.cpp
  ${GMPI_GPU_DIR}/image.cpp
  ${GMPI_GPU_DIR}/layer.cpp
  ${GMPI_GPU_DIR}/path.cpp
  ${GMPI_GPU_DIR}/post_effects.cpp
  ${GMPI_GPU_DIR}/region.cpp
  ${GMPI_GPU_DIR}/renderer.cpp
  ${GMPI_GPU_DIR}/shape_batcher.cpp
  ${GMPI_GPU_DIR}/shapes.cpp
)

# The compiled shaders for this platform's renderer.
if (WIN32)
  set(GMPI_GPU_SHADERS ${GMPI_GPU_DIR}/shaders/compiled/shaders_dx11.cpp)
elseif (APPLE)
  set(GMPI_GPU_SHADERS ${GMPI_GPU_DIR}/shaders/compiled/shaders_metal.cpp)
  target_sources(gmpi_gpu PRIVATE ${GMPI_GPU_DIR}/windowless_context.mm)
  set_source_files_properties(${GMPI_GPU_DIR}/windowless_context.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
  # bgfx links Metal, QuartzCore and Cocoa itself; MetalKit is ours, as in the fork.
  find_library(GMPI_GPU_METALKIT MetalKit)
  target_link_libraries(gmpi_gpu PRIVATE ${GMPI_GPU_METALKIT})
else ()
  set(GMPI_GPU_SHADERS ${GMPI_GPU_DIR}/shaders/compiled/shaders_spirv.cpp)
endif ()
if (NOT EXISTS ${GMPI_GPU_SHADERS})
  message(FATAL_ERROR "gmpi_gpu: ${GMPI_GPU_SHADERS} has not been generated for this platform. "
                      "Run backends/gpu/shaders/compile_shaders.cmake with RENDERERS for it.")
endif ()
target_sources(gmpi_gpu PRIVATE ${GMPI_GPU_SHADERS})

target_compile_features(gmpi_gpu PUBLIC cxx_std_17)
target_include_directories(gmpi_gpu PRIVATE ${GMPI_GPU_DIR} ${bgfx_SOURCE_DIR}/bgfx/3rdparty)
target_link_libraries(gmpi_gpu PUBLIC bgfx bx PRIVATE bimg)
if (GMPI_GPU_DEBUG_LOGGING)
  target_compile_definitions(gmpi_gpu PRIVATE GMPI_GPU_GRAPHICS_DEBUG_LOGGING=1)
endif ()
if (UNIX AND NOT APPLE)
  target_compile_definitions(gmpi_gpu PRIVATE _LARGEFILE64_SOURCE)
endif ()
if (WIN32)
  target_compile_definitions(gmpi_gpu PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000000)
endif ()
