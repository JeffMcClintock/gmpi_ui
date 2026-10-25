cmake_minimum_required(VERSION 3.20)
project(WinBench CXX)

# A headless throughput benchmark for gmpi_ui's Windows backends: Direct2D and,
# with -DWINBENCH_GPU=ON, the GPU renderer (Direct3D 12). See WinBench.cpp.
#
#   cmake -S examples/WinBench -B build-winbench -G Ninja -DCMAKE_BUILD_TYPE=Release -DWINBENCH_GPU=ON
#   cmake --build build-winbench
#   build-winbench/win_bench.exe --backend d2d
#   build-winbench/win_bench.exe --backend gpu

if(NOT WIN32)
    message(FATAL_ERROR "WinBench is Windows only (Direct2D). See examples/WaylandBench for Linux.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(GMPI_UI_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(GMPI_SDK_FOLDER_OVERRIDE "" CACHE PATH "Path to a local GMPI repo, or blank for ../../../GMPI")
if(NOT "${GMPI_SDK_FOLDER_OVERRIDE}" STREQUAL "")
    set(GMPI_SDK "${GMPI_SDK_FOLDER_OVERRIDE}")
else()
    get_filename_component(GMPI_SDK "${GMPI_UI_SDK}/../GMPI" ABSOLUTE)
endif()

option(WINBENCH_GPU "Also benchmark the GPU backend (Direct3D 12)" ON)

add_executable(win_bench WinBench.cpp ${GMPI_UI_SDK}/backends/DirectXGfx.cpp)

target_include_directories(win_bench PRIVATE ${GMPI_SDK} ${GMPI_SDK}/Core ${GMPI_UI_SDK})
target_compile_definitions(win_bench PRIVATE UNICODE _UNICODE)
target_link_libraries(win_bench PRIVATE d3d11 d2d1 dwrite windowscodecs dxguid ole32)
if(MSVC)
    target_compile_options(win_bench PRIVATE /utf-8)
endif()

if(WINBENCH_GPU)
    include(${GMPI_UI_SDK}/backends/gpu/gmpi_gpu.cmake)
    target_link_libraries(win_bench PRIVATE gmpi_gpu)
    target_compile_definitions(win_bench PRIVATE GMPI_UI_GPU_BACKEND=1)

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
    target_compile_options(gmpi_harfbuzz PRIVATE /wd4244 /wd4267 /wd4996 /bigobj)
    target_link_libraries(win_bench PRIVATE gmpi_harfbuzz)
endif()
