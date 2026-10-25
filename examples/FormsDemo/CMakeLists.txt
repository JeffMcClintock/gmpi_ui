cmake_minimum_required(VERSION 3.20)
project(FormsDemo CXX)

# A standalone desktop app built from gmpi_ui's widget set: a window, a Form,
# and three controls (a number entry, a text entry, a tick box) wired the way
# SynthEdit's PropertiesBrowser wires its rows. No plugin, no audio, no JUCE.
#
# Windows only for now: it hosts gmpi_ui's Direct2D DrawingFrame (the frame the
# GMPI VST3 wrapper embeds in a DAW) inside a plain overlapped HWND.

if(NOT WIN32)
    message(FATAL_ERROR "FormsDemo only has a Windows shell (Win32 + Direct2D).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# gmpi_ui is the repo this example lives in.
get_filename_component(GMPI_UI_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# GMPI (the plugin API headers: Core/GmpiApiEditor.h etc). A sibling checkout
# is used when there is one; otherwise it is fetched from GitHub.
set(GMPI_SDK_FOLDER_OVERRIDE "" CACHE PATH
    "Path to a local GMPI repo, or blank to use ../../../GMPI or fetch from GitHub.")

if(NOT "${GMPI_SDK_FOLDER_OVERRIDE}" STREQUAL "")
    set(GMPI_SDK "${GMPI_SDK_FOLDER_OVERRIDE}")
elseif(EXISTS "${GMPI_UI_SDK}/../GMPI/Core/GmpiApiEditor.h")
    get_filename_component(GMPI_SDK "${GMPI_UI_SDK}/../GMPI" ABSOLUTE)
else()
    include(FetchContent)
    FetchContent_Declare(
        gmpi
        GIT_REPOSITORY https://github.com/JeffMcClintock/GMPI
        GIT_TAG        origin/main
        SOURCE_SUBDIR  do-not-configure   # headers only; nothing to add_subdirectory
    )
    FetchContent_MakeAvailable(gmpi)
    set(GMPI_SDK ${gmpi_SOURCE_DIR})
endif()
message(STATUS "FormsDemo: gmpi_ui = ${GMPI_UI_SDK}")
message(STATUS "FormsDemo: GMPI    = ${GMPI_SDK}")

# immer - persistent (immutable) data structures. Header-only, so it is
# fetched and put on the include path rather than configured: its own
# CMakeLists would otherwise offer to build its tests, examples and docs.
# The demo keeps its model's history in an immer::vector (see DemoForm.h).
include(FetchContent)
FetchContent_Declare(
    immer
    GIT_REPOSITORY https://github.com/arximboldi/immer
    GIT_TAG        v0.9.1
    GIT_SHALLOW    ON
    GIT_SUBMODULES ""                 # none: the repo's submodules are pybind11 and a docs theme, and the latter's paths exceed MAX_PATH on Windows
    SOURCE_SUBDIR  do-not-configure   # header-only; nothing to add_subdirectory
)
FetchContent_MakeAvailable(immer)

add_executable(${PROJECT_NAME} WIN32
    MainWin32.cpp
    DemoForm.h
    DemoForm.cpp

    # The widget set and the Form that hosts it.
    ${GMPI_UI_SDK}/experimental/builders.cpp
    ${GMPI_UI_SDK}/experimental/forms.cpp
    ${GMPI_UI_SDK}/experimental/grid.cpp
    ${GMPI_UI_SDK}/experimental/primatives.cpp
    ${GMPI_UI_SDK}/experimental/ui_facades.cpp
    ${GMPI_UI_SDK}/helpers/Timer.cpp

    # The Direct2D editor frame and its window.
    ${GMPI_UI_SDK}/backends/DrawingFrameCommon.cpp
    ${GMPI_UI_SDK}/backends/DrawingFrameWin.cpp
    ${GMPI_UI_SDK}/backends/DirectXGfx.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GMPI_SDK}
    ${GMPI_SDK}/Core
    ${GMPI_UI_SDK}
)

# SYSTEM, so immer's headers are compiled as external code: MSVC then keeps
# its warnings (unknown '#pragma GCC', among others) out of this build's output.
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${immer_SOURCE_DIR})

# compat/ holds a shim for a SynthEdit SDK header that builders.cpp includes.
# See compat/it_enum_list.h.
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE _UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11 d2d1 dwrite windowscodecs   # the drawing backend, and WIC for images
    Dwmapi dxguid                     # window metrics, and the D3D/D2D IIDs
    ole32                             # COM apartment for the file/colour dialogs
)

# The GPU backend (Direct3D 12): built in, and used when GMPI_UI_GPU=1 is set.
# Without it the demo draws through Direct2D as before.
option(FORMSDEMO_GPU "Build in gmpi_ui's GPU backend (run with GMPI_UI_GPU=1 to use it)" OFF)
if(FORMSDEMO_GPU)
    include(${GMPI_UI_SDK}/backends/gpu/gmpi_gpu.cmake)
    target_link_libraries(${PROJECT_NAME} PRIVATE gmpi_gpu)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GMPI_UI_GPU_BACKEND=1)

    # The GPU path lays text out with CpuTextEngine, which shapes with HarfBuzz
    # (the amalgamation, compiled here as gimpi_ui_tests does).
    FetchContent_Declare(
        harfbuzz
        GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz
        GIT_TAG        14.2.1
        GIT_SHALLOW    ON
        SOURCE_SUBDIR  do-not-configure
    )
    FetchContent_MakeAvailable(harfbuzz)
    add_library(gmpi_harfbuzz STATIC ${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc)
    target_include_directories(gmpi_harfbuzz PUBLIC ${harfbuzz_SOURCE_DIR}/src)
    target_compile_definitions(gmpi_harfbuzz PUBLIC HB_NO_MT)
    set_target_properties(gmpi_harfbuzz PROPERTIES CXX_STANDARD 17)
    target_compile_options(gmpi_harfbuzz PRIVATE /wd4244 /wd4267 /wd4996 /bigobj)
    target_link_libraries(${PROJECT_NAME} PRIVATE gmpi_harfbuzz)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
