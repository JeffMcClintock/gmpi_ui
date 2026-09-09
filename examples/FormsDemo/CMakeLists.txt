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

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
