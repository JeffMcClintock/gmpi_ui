cmake_minimum_required(VERSION 3.20)
project(MacBench CXX OBJCXX)

# A headless throughput benchmark for gmpi_ui's macOS backends: CoreGraphics and
# the GPU renderer (Metal). See MacBench.mm.
#
#   cmake -S examples/MacBench -B build-macbench -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-macbench
#   build-macbench/mac_bench --backend cg
#   build-macbench/mac_bench --backend gpu

if(NOT APPLE)
    message(FATAL_ERROR "MacBench is macOS only. See examples/WinBench and examples/WaylandBench.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)

get_filename_component(GMPI_UI_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(GMPI_SDK_FOLDER_OVERRIDE "" CACHE PATH "Path to a local GMPI repo, or blank for ../../../GMPI")
if(NOT "${GMPI_SDK_FOLDER_OVERRIDE}" STREQUAL "")
    set(GMPI_SDK "${GMPI_SDK_FOLDER_OVERRIDE}")
else()
    get_filename_component(GMPI_SDK "${GMPI_UI_SDK}/../GMPI" ABSOLUTE)
endif()

add_executable(mac_bench MacBench.mm)
target_include_directories(mac_bench PRIVATE ${GMPI_SDK} ${GMPI_SDK}/Core ${GMPI_UI_SDK})
# The CoreGraphics backend's Objective-C is manually reference counted.
target_compile_options(mac_bench PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fno-objc-arc>)
target_link_libraries(mac_bench PRIVATE
    "-framework Cocoa" "-framework CoreGraphics" "-framework CoreText" "-framework ImageIO")

include(${GMPI_UI_SDK}/backends/gpu/gmpi_gpu.cmake)
target_link_libraries(mac_bench PRIVATE gmpi_gpu)
target_compile_definitions(mac_bench PRIVATE GMPI_UI_GPU_BACKEND=1)

# CpuTextEngine shapes with HarfBuzz (the amalgamation, as gimpi_ui_tests builds it).
include(FetchContent)
FetchContent_Declare(harfbuzz
    GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz
    GIT_TAG        14.2.1
    GIT_SHALLOW    ON
    SOURCE_SUBDIR  do-not-configure)
FetchContent_MakeAvailable(harfbuzz)
add_library(gmpi_harfbuzz STATIC ${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc)
target_include_directories(gmpi_harfbuzz PUBLIC ${harfbuzz_SOURCE_DIR}/src)
target_compile_definitions(gmpi_harfbuzz PUBLIC HB_NO_MT)
set_target_properties(gmpi_harfbuzz PROPERTIES CXX_STANDARD 17)
target_link_libraries(mac_bench PRIVATE gmpi_harfbuzz)
