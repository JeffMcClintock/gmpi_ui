<img src="docs/images/GMPI_Icon.png" width="128"/>

# GMPI-Drawing

A cross-platform drawing library for Windows, macOS and Linux which is lightweight and can be used in a shared library, which makes it a good fit for audio plugin GUIs.

For JUCE projects there is also a backend that renders through juce::Graphics (see `backends/JuceGfx.h`), which additionally supports any platform JUCE runs on.

<img src="docs/images/text.png" width="260"/>
<img src="docs/images/lines.png" width="260"/>

# Features

GMPI Drawing:
* Has a permissive open-source license
* An open standard. No fees, contracts or NDAs
* Has cross-platform support (Windows, macOS and Linux)
* The Windows and macOS backends need nothing but the standard OS features and the standard C++ library; Linux adds a few common system libraries (see below)
* A clean and bloat-free API surface
* Provides all APIs in both C++ and in pure portable 'C' for maximum compatibility
* Uses the platforms native drawing (Cocoa / Direct-2D) to minimise overhead and build times.
* Optional GPU backend (Direct3D 12 / Metal / Vulkan) with custom shader brushes - new, and lightly tested so far
* Optional JUCE backend (juce::Graphics) for use inside JUCE projects and on other platforms
* Text in any languages via UTF-8
* Color emojis
* Bitmap images
* Vector paths with lines, bezier curves, and arcs
* Solid, gradient, and tiled image fills
* Rotation, scaling and translation
* DPI aware

# Backends

Every backend implements the same API. The Direct2D, software and GPU backends are tested against the same Direct2D reference images ([gimpi_ui_tests](https://github.com/JeffMcClintock/gimpi_ui_tests)), so a GUI draws the same whichever of them it gets.

| Backend | Source | Platforms | |
|---|---|---|---|
| Direct2D | `backends/DirectXGfx.h` | Windows | the default on Windows |
| CoreGraphics | `backends/CocoaGfx.h` | macOS | the default on macOS |
| Software | `backends/CpuGfx.h` | all | the default on Linux (X11 and Wayland); text through `helpers/CpuTextEngine.h` (HarfBuzz) |
| GPU | `backends/GpuGfx.h`, `backends/gpu/` | Windows, macOS, Linux | opt-in, see below |
| JUCE | `backends/JuceGfx.h` | anywhere JUCE runs | |

Direct2D and CoreGraphics are the mature backends. The Linux frames and the GPU backend are recent and have seen little real-world use yet, so expect rough edges.

## Linux

Linux has two window frames, one per display protocol, and a plugin editor can be embedded through either (VST3 hands an X11 window id or, from SDK 3.8, a `wl_surface`; [docs/vst3-linux-editor.md](docs/vst3-linux-editor.md) covers both):

| Frame | Source | Needs |
|---|---|---|
| X11 | `backends/DrawingFrameX11.h/.cpp` | Xlib, XShm, Xshape, xkbcommon |
| Wayland | `backends/DrawingFrameWayland.h` | wayland-client, wayland-cursor, xkbcommon, libdecor |

Both draw with the software backend by default (text through FreeType and HarfBuzz) and present with shared memory; both can present through the GPU backend instead (Vulkan). Both provide the whole `IDialogHost` surface: popup menus, tooltips, in-place text edit, the colour picker, child windows, clipboard, and file dialogs through the XDG desktop portal (D-Bus). Tested on Mutter and Weston (Wayland) and Xorg (X11); other compositors are untried.

## GPU backend

`backends/GpuGfx.h` draws through gmpi_ui's own GPU renderer (`backends/gpu/`, absorbed from [Visage](https://github.com/VitalAudio/visage)) over a vendored subset of [bgfx](https://github.com/bkaradzic/bgfx): Direct3D 12 on Windows, Metal on macOS, Vulkan on Linux. Like the Direct2D and software backends it works in premultiplied linear half-float, and it draws text with the software backend's text engine.

It is opt-in. Build it into a target with

```cmake
include(${GMPI_UI_SDK}/backends/gpu/gmpi_gpu.cmake)
target_link_libraries(my_target PRIVATE gmpi_gpu)
target_compile_definitions(my_target PRIVATE GMPI_UI_GPU_BACKEND=1)
```

then ask for it at run time: `GMPI_UI_GPU=1` in the environment, or `gmpi::gpugfx::hostPreference()` from the host's own settings (the environment variable, 1 or 0, overrides the preference). Frames built with `GMPI_UI_GPU_BACKEND` then present through it: the Windows native-window frame (`DxDrawingFrameHwnd`), macOS (`DrawingFrameMac`), X11 and Wayland. A machine that cannot run the renderer keeps the default backend.

It is new and has not yet shipped in a plugin or been run on a wide range of GPUs and drivers, which is why Direct2D and CoreGraphics stay the defaults.

It also offers an extension the other backends answer `NoSupport` to: shader brushes, whose colour comes from a fragment shader the module supplies (`GmpiApiShaders.h`).

Building, shaders and tests: [backends/gpu/README.md](backends/gpu/README.md).
