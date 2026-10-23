# wayland_bench: a frame-rate benchmark for gmpi_ui's Wayland backends.
# See README.md. Linux only; needs a Wayland session to run.

cmake_minimum_required(VERSION 3.25)
project(WaylandBench CXX C)

set(CMAKE_CXX_STANDARD 20)
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)   # a benchmark of a debug build measures the wrong thing
endif ()

get_filename_component(GMPI_UI_DIR ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
set(GMPI_SDK ${GMPI_UI_DIR}/../GMPI CACHE PATH "GMPI SDK checkout (for GmpiApiCommon headers)")
option(WAYLAND_BENCH_GPU "Build with the GPU backend as well as the CPU one" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(BENCH_DEPS REQUIRED IMPORTED_TARGET
  wayland-client wayland-cursor xkbcommon libdecor-0 dbus-1 fontconfig harfbuzz freetype2 libpng)

# The Wayland protocols DrawingFrameWayland.h is compiled against - the same
# list tests/run.sh generates.
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
set(BENCH_GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${BENCH_GEN})
set(BENCH_PROTOCOL_SOURCES)
foreach (p stable/xdg-shell/xdg-shell
           staging/fractional-scale/fractional-scale-v1
           stable/viewporter/viewporter
           staging/cursor-shape/cursor-shape-v1
           unstable/tablet/tablet-unstable-v2
           unstable/xdg-foreign/xdg-foreign-unstable-v2)
  get_filename_component(n ${p} NAME)
  set(xml ${WAYLAND_PROTOCOLS_DIR}/${p}.xml)
  add_custom_command(
    OUTPUT ${BENCH_GEN}/${n}-client-protocol.h ${BENCH_GEN}/${n}-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header ${xml} ${BENCH_GEN}/${n}-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code  ${xml} ${BENCH_GEN}/${n}-protocol.c
    DEPENDS ${xml})
  list(APPEND BENCH_PROTOCOL_SOURCES ${BENCH_GEN}/${n}-client-protocol.h ${BENCH_GEN}/${n}-protocol.c)
endforeach ()

add_executable(wayland_bench WaylandBench.cpp ${BENCH_PROTOCOL_SOURCES})
target_include_directories(wayland_bench PRIVATE ${GMPI_UI_DIR} ${GMPI_SDK} ${BENCH_GEN})
target_link_libraries(wayland_bench PRIVATE PkgConfig::BENCH_DEPS)

if (WAYLAND_BENCH_GPU)
  include(${GMPI_UI_DIR}/backends/gpu/gmpi_gpu.cmake)
  target_link_libraries(wayland_bench PRIVATE gmpi_gpu)
  target_compile_definitions(wayland_bench PRIVATE GMPI_UI_GPU_BACKEND=1)
endif ()
