#pragma once

// The "model surface": a native window handle the renderer is initialised
// on and never draws to. bgfx's Vulkan renderer only reports swap-chain
// support when initialised on SOME window, and that window must not be one a
// canvas later pairs to (it would get a second Vulkan surface). Metal needs a
// CAMetalLayer to come up headless. Each real window then gets its own swap
// chain (gpugfx::Factory::createWindowRenderTarget).
//
// One per process, made on first use and never destroyed. On Linux it is
// made on the host's display connection when one is given, because bgfx
// creates every later window surface against the display it was initialised
// with; with no display (offscreen use before any window: tests, a plugin
// rendering to bitmaps) it opens a connection of its own, Wayland first,
// then X11. A window on a different connection cannot follow that - a host
// that will show windows should initialise through the frame, as the frames
// do, not through Canvas::setWindowless.

namespace gmpi::gpu {
  struct ModelSurface {
    void* window = nullptr;   // wl_surface* / X11 Window / CAMetalLayer* / null on Windows
    void* display = nullptr;  // wl_display* / X Display*, else null
    bool wayland = false;
  };

  // nativeDisplay: wl_display* or X Display* (Linux), else ignored.
  // waylandCompositor: the host's wl_compositor* when nativeDisplay is a
  // Wayland display (binding one here would need a roundtrip inside the
  // host's dispatch).
  ModelSurface modelSurface(void* nativeDisplay = nullptr, void* waylandCompositor = nullptr);
}
