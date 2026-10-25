cmake_minimum_required(VERSION 3.20)
project(MacPresent CXX OBJCXX)

# DrawingFrameMac presenting through CoreGraphics and through the GPU backend
# (Metal), captured off-screen and compared. See MacPresent.mm.
#
#   cmake -S examples/MacPresent -B build-macpresent -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-macpresent
#   build-macpresent/mac_present --out /tmp/macpresent

if(NOT APPLE)
    message(FATAL_ERROR "MacPresent is macOS only. See examples/WinBench and examples/WaylandBench.")
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

add_executable(mac_present
    MacPresent.mm
    ${GMPI_UI_SDK}/backends/DrawingFrameMac.mm
    ${GMPI_UI_SDK}/backends/DrawingFrameCommon.cpp
)
target_include_directories(mac_present PRIVATE ${GMPI_SDK} ${GMPI_SDK}/Core ${GMPI_UI_SDK})
# The backend's Objective-C is manually reference counted.
target_compile_options(mac_present PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fno-objc-arc>)
target_link_libraries(mac_present PRIVATE
    "-framework AppKit" "-framework CoreGraphics" "-framework CoreFoundation" "-framework CoreText"
    "-framework ImageIO" "-framework QuartzCore" "-framework AudioUnit" "-framework UniformTypeIdentifiers")

include(${GMPI_UI_SDK}/backends/gpu/gmpi_gpu.cmake)
target_link_libraries(mac_present PRIVATE gmpi_gpu)
target_compile_definitions(mac_present PRIVATE GMPI_UI_GPU_BACKEND=1)

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
target_link_libraries(mac_present PRIVATE gmpi_harfbuzz)
