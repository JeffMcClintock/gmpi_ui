# bgfx's shader compiler, for regenerating compiled shader blobs
# (shaders/compile_shaders.cmake). Included by gmpi_gpu.cmake when
# GMPI_GPU_BUILD_SHADERC=ON; then `cmake --build <dir> --target shaderc`.
#
# This is the ONE place gmpi_ui still fetches bgfx: shaderc needs glslang,
# spirv-tools, spirv-cross and friends (most of bgfx.cmake's ~600 MB), which
# are tools, not renderer, and so are not vendored. The tag must be the one
# 3rdparty/bgfx was vendored from: the shader binary format is versioned, and
# a blob from another bgfx may not load. The check below enforces that.

include_guard(GLOBAL)

set(GMPI_SHADERC_TAG v1.129.8958-499)

file(STRINGS ${GMPI_BGFX_DIR}/VERSIONS GMPI_VENDORED_TAG REGEX "^bgfx.cmake ")
string(REPLACE "bgfx.cmake " "" GMPI_VENDORED_TAG "${GMPI_VENDORED_TAG}")
if (NOT GMPI_VENDORED_TAG STREQUAL GMPI_SHADERC_TAG)
  message(FATAL_ERROR "tools/shaderc.cmake fetches bgfx ${GMPI_SHADERC_TAG} but 3rdparty/bgfx is ${GMPI_VENDORED_TAG}; "
                      "shaders compiled by it would not match the renderer. Bump them together.")
endif ()

include(FetchContent)
set(BGFX_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_EXAMPLE_COMMON OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS ON CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS_SHADER ON CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS_BIN2C OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TOOLS_TEXTURE OFF CACHE INTERNAL "" FORCE)
set(BGFX_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(BGFX_INSTALL OFF CACHE INTERNAL "" FORCE)
FetchContent_Declare(gmpi_bgfx_tools GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
  GIT_SHALLOW TRUE
  GIT_TAG ${GMPI_SHADERC_TAG}
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(gmpi_bgfx_tools)
