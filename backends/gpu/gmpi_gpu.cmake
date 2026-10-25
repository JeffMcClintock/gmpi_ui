# gmpi_ui's GPU renderer, for backends/GpuGfx.h. Include this file from a
# consumer's CMakeLists and link the target:
#
#   include(${GMPI_UI_DIR}/backends/gpu/gmpi_gpu.cmake)
#   target_link_libraries(my_target PRIVATE gmpi_gpu)
#   target_compile_definitions(my_target PRIVATE GMPI_UI_GPU_BACKEND=1)
#
# The renderer is bgfx underneath, vendored in 3rdparty/bgfx (a pinned subset,
# built here; nothing is fetched). The shaders are committed already
# compiled, one C++ file per renderer (shaders/compiled/), so a build needs
# neither shaderc nor an embed step. After editing a .sc file, regenerate
# them: shaders/compile_shaders.cmake. GMPI_GPU_BUILD_SHADERC=ON fetches and
# builds the matching shaderc for that (tools/shaderc.cmake).
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
# Vendored: the subset of bgfx, bimg and bx the renderer compiles, at a pinned
# tag, with our one local change applied (3rdparty/bgfx/VENDORED.md says what
# and why; tools/vendor_bgfx.sh re-vendors). Nothing is fetched or patched at
# configure time. The targets are prefixed gmpi_ so they cannot collide with
# a consumer's own bgfx.
set(GMPI_BGFX_DIR ${GMPI_GPU_DIR}/3rdparty/bgfx)

if (MSVC)
  set(GMPI_GPU_NO_WARNINGS /W0)
else ()
  set(GMPI_GPU_NO_WARNINGS -w)
endif ()

# bx: one translation unit (bx's own amalgamation).
add_library(gmpi_bx STATIC ${GMPI_BGFX_DIR}/bx/src/amalgamated.cpp)
if (WIN32)
  set(GMPI_BX_COMPAT msvc)
elseif (APPLE)
  set(GMPI_BX_COMPAT osx)
else ()
  set(GMPI_BX_COMPAT linux)
endif ()
target_include_directories(gmpi_bx PUBLIC ${GMPI_BGFX_DIR}/bx/include ${GMPI_BGFX_DIR}/bx/3rdparty
                                          ${GMPI_BGFX_DIR}/bx/include/compat/${GMPI_BX_COMPAT})
target_compile_definitions(gmpi_bx PUBLIC BX_CONFIG_DEBUG=$<IF:$<CONFIG:Debug>,1,0>
                                          __STDC_LIMIT_MACROS __STDC_FORMAT_MACROS __STDC_CONSTANT_MACROS
                                   PRIVATE BX_CONFIG_EXCEPTION_HANDLING_USE_WINDOWS_SEH=0)
target_compile_features(gmpi_bx PUBLIC cxx_std_17)
target_compile_options(gmpi_bx PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus /Zc:preprocessor>
                               PRIVATE ${GMPI_GPU_NO_WARNINGS})
if (WIN32)
  target_link_libraries(gmpi_bx PUBLIC psapi)
elseif (NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(gmpi_bx PUBLIC Threads::Threads ${CMAKE_DL_LIBS} rt)
endif ()

# bimg: its format table and conversions only. Every texture here is raw
# pixels the renderer made, so none of bimg's container decoders or codecs is
# vendored; BIMG_DECODE_ASTC=0 leaves astcenc.h (included unconditionally)
# unused.
add_library(gmpi_bimg STATIC ${GMPI_BGFX_DIR}/bimg/src/image.cpp ${GMPI_BGFX_DIR}/bimg/src/image_gnf.cpp)
target_include_directories(gmpi_bimg PUBLIC ${GMPI_BGFX_DIR}/bimg/include
                                     PRIVATE ${GMPI_BGFX_DIR}/bimg/3rdparty/astc-encoder/include)
target_compile_definitions(gmpi_bimg PRIVATE BIMG_DECODE_ASTC=0)
target_compile_options(gmpi_bimg PRIVATE ${GMPI_GPU_NO_WARNINGS})
target_link_libraries(gmpi_bimg PUBLIC gmpi_bx)

# bgfx: one translation unit of our own (3rdparty/bgfx/gmpi/), listing only
# the vendored sources; .mm on Apple, where the Metal renderer is Objective-C++.
if (APPLE)
  add_library(gmpi_bgfx STATIC ${GMPI_BGFX_DIR}/gmpi/amalgamated.mm)
else ()
  add_library(gmpi_bgfx STATIC ${GMPI_BGFX_DIR}/gmpi/amalgamated.cpp)
endif ()
target_include_directories(gmpi_bgfx PUBLIC ${GMPI_BGFX_DIR}/bgfx/include
                                     PRIVATE ${GMPI_BGFX_DIR}/bgfx/src ${GMPI_BGFX_DIR}/bgfx/3rdparty
                                             ${GMPI_BGFX_DIR}/bgfx/3rdparty/khronos)
if (WIN32)
  # Newer than the Windows SDK's, and what bgfx's Direct3D 12 renderer is written against.
  target_include_directories(gmpi_bgfx PRIVATE ${GMPI_BGFX_DIR}/bgfx/3rdparty/directx-headers/include/directx
                                               ${GMPI_BGFX_DIR}/bgfx/3rdparty/directx-headers/include)
  target_compile_definitions(gmpi_bgfx PRIVATE _CRT_SECURE_NO_WARNINGS)
endif ()

# One current API per platform, nothing older compiled in (ruled 2026-10-24):
# Direct3D 12 on Windows, Metal on Apple, Vulkan on Linux. renderer.cpp asks
# for nothing else, and a machine without its API keeps the CPU path. Naming
# any renderer switches bgfx's config.h to "only what is named", so every one
# is named, on or off. The ones off everywhere are not vendored at all;
# gmpi/renderer_stubs.cpp supplies their entry points and asserts they stay off.
set(GMPI_GPU_RENDERERS
  BGFX_CONFIG_RENDERER_DIRECT3D11=0 BGFX_CONFIG_RENDERER_OPENGL=0 BGFX_CONFIG_RENDERER_OPENGLES=0
  BGFX_CONFIG_RENDERER_WEBGPU=0 BGFX_CONFIG_RENDERER_GNM=0 BGFX_CONFIG_RENDERER_NVN=0 BGFX_CONFIG_RENDERER_AGC=0)
if (WIN32)
  list(APPEND GMPI_GPU_RENDERERS BGFX_CONFIG_RENDERER_DIRECT3D12=1 BGFX_CONFIG_RENDERER_VULKAN=0 BGFX_CONFIG_RENDERER_METAL=0)
elseif (APPLE)
  list(APPEND GMPI_GPU_RENDERERS BGFX_CONFIG_RENDERER_METAL=1 BGFX_CONFIG_RENDERER_VULKAN=0 BGFX_CONFIG_RENDERER_DIRECT3D12=0)
else ()
  list(APPEND GMPI_GPU_RENDERERS BGFX_CONFIG_RENDERER_VULKAN=1 BGFX_CONFIG_RENDERER_DIRECT3D12=0 BGFX_CONFIG_RENDERER_METAL=0)
endif ()
target_compile_definitions(gmpi_bgfx
  PRIVATE ${GMPI_GPU_RENDERERS}
  PUBLIC BGFX_CONFIG_MAX_DRAW_CALLS=8192 BGFX_CONFIG_MAX_VIEWS=256 BGFX_CONFIG_MAX_FRAME_BUFFERS=1024
         BGFX_CONFIG_MULTITHREADED=0
         BGFX_CONFIG_DEBUG_ANNOTATION=$<OR:$<CONFIG:Debug>,$<BOOL:${GMPI_GPU_DEBUG_LOGGING}>>)
target_compile_options(gmpi_bgfx PRIVATE ${GMPI_GPU_NO_WARNINGS})
target_link_libraries(gmpi_bgfx PUBLIC gmpi_bx PRIVATE gmpi_bimg)
if (APPLE)
  target_link_libraries(gmpi_bgfx PUBLIC "-framework Cocoa" "-framework Metal" "-framework QuartzCore"
                                         "-framework IOKit" "-framework CoreFoundation")
elseif (UNIX)
  # Vulkan's Xlib surface path. The library is dlopen'ed; only X11's link
  # line is needed, as bgfx.cmake had it.
  find_package(X11 REQUIRED)
  target_link_libraries(gmpi_bgfx PUBLIC ${X11_LIBRARIES})
endif ()

# shaderc is a TOOL for regenerating compiled shaders, not part of the build.
if (GMPI_GPU_BUILD_SHADERC)
  include(${GMPI_GPU_DIR}/tools/shaderc.cmake)
endif ()

# --- the renderer ------------------------------------------------------------
# The device seam (device/device.h): the renderer talks to the GPU through
# gmpi::gpu::gfx only. A renderer source that names bgfx:: directly has gone
# round the seam; refuse it here rather than find out when a second device
# arrives.
file(GLOB GMPI_GPU_SEAM_SOURCES ${GMPI_GPU_DIR}/*.cpp ${GMPI_GPU_DIR}/*.h ${GMPI_GPU_DIR}/*.mm
                                ${GMPI_GPU_DIR}/platform/*.cpp ${GMPI_GPU_DIR}/platform/*.mm ${GMPI_GPU_DIR}/platform/*.h
                                ${GMPI_GPU_DIR}/../GpuGfx.h)
foreach (src ${GMPI_GPU_SEAM_SOURCES})
  file(READ ${src} contents)
  if (contents MATCHES "bgfx::")
    message(FATAL_ERROR "gmpi_gpu: ${src} names bgfx:: directly; go through gmpi::gpu::gfx (backends/gpu/device/device.h)")
  endif ()
endforeach ()

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
  set(GMPI_GPU_SHADERS ${GMPI_GPU_DIR}/shaders/compiled/shaders_dxbc.cpp)
elseif (APPLE)
  set(GMPI_GPU_SHADERS ${GMPI_GPU_DIR}/shaders/compiled/shaders_metal.cpp)
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

# The model surface the renderer initialises on, per platform
# (platform/model_surface.h).
if (WIN32)
  target_sources(gmpi_gpu PRIVATE ${GMPI_GPU_DIR}/platform/model_surface_win.cpp)
elseif (APPLE)
  target_sources(gmpi_gpu PRIVATE ${GMPI_GPU_DIR}/platform/model_surface_mac.mm)
  set_source_files_properties(${GMPI_GPU_DIR}/platform/model_surface_mac.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
else ()
  target_sources(gmpi_gpu PRIVATE ${GMPI_GPU_DIR}/platform/model_surface_linux.cpp)
  target_include_directories(gmpi_gpu PRIVATE ${X11_INCLUDE_DIR})
  # Wayland when its client library is installed; the X11 branch alone otherwise.
  find_package(PkgConfig QUIET)
  if (PkgConfig_FOUND)
    pkg_check_modules(GMPI_GPU_WAYLAND QUIET IMPORTED_TARGET wayland-client)
  endif ()
  if (GMPI_GPU_WAYLAND_FOUND)
    target_compile_definitions(gmpi_gpu PRIVATE GMPI_GPU_HAVE_WAYLAND=1)
    target_link_libraries(gmpi_gpu PRIVATE PkgConfig::GMPI_GPU_WAYLAND)
  endif ()
endif ()

target_compile_features(gmpi_gpu PUBLIC cxx_std_17)
target_include_directories(gmpi_gpu PRIVATE ${GMPI_GPU_DIR} ${GMPI_BGFX_DIR}/bgfx/3rdparty)
target_link_libraries(gmpi_gpu PUBLIC gmpi_bgfx gmpi_bx PRIVATE gmpi_bimg)
if (GMPI_GPU_DEBUG_LOGGING)
  target_compile_definitions(gmpi_gpu PRIVATE GMPI_GPU_GRAPHICS_DEBUG_LOGGING=1)
endif ()
if (UNIX AND NOT APPLE)
  target_compile_definitions(gmpi_gpu PRIVATE _LARGEFILE64_SOURCE)
endif ()
if (WIN32)
  target_compile_definitions(gmpi_gpu PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000000)
endif ()
