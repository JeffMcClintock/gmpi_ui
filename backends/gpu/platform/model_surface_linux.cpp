// Linux: a role-less wl_surface, or an unmapped 1x1 X11 window. See model_surface.h.

#include "model_surface.h"

#include <cstdint>
#include <cstring>

#include <X11/Xlib.h>
#if GMPI_GPU_HAVE_WAYLAND
#include <wayland-client.h>
#endif

namespace gmpi::gpu {
  namespace {
    ModelSurface& cached() {
      static ModelSurface surface;
      return surface;
    }

#if GMPI_GPU_HAVE_WAYLAND
    struct CompositorBind {
      wl_compositor* compositor = nullptr;
    };

    void onGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t) {
      if (std::strcmp(interface, "wl_compositor") == 0)
        static_cast<CompositorBind*>(data)->compositor =
            static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));
    }

    void onGlobalRemove(void*, wl_registry*, uint32_t) {}

    const wl_registry_listener kRegistryListener = { onGlobal, onGlobalRemove };
#endif

    ModelSurface x11Surface(Display* display) {
      const Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
      return { reinterpret_cast<void*>(uintptr_t(window)), display, false };
    }
  }

  ModelSurface modelSurface(void* nativeDisplay, void* waylandCompositor) {
    auto& surface = cached();
    if (surface.window)
      return surface;

    if (nativeDisplay) {
#if GMPI_GPU_HAVE_WAYLAND
      if (waylandCompositor) {
        surface = { wl_compositor_create_surface(static_cast<wl_compositor*>(waylandCompositor)), nativeDisplay, true };
        return surface;
      }
#else
      (void)waylandCompositor;
#endif
      surface = x11Surface(static_cast<Display*>(nativeDisplay));
      return surface;
    }

    // No host display: a connection of our own. Safe to roundtrip here - no
    // dispatch of the host's is in progress, there is no host.
#if GMPI_GPU_HAVE_WAYLAND
    if (wl_display* display = wl_display_connect(nullptr)) {
      CompositorBind bind;
      wl_registry* registry = wl_display_get_registry(display);
      wl_registry_add_listener(registry, &kRegistryListener, &bind);
      wl_display_roundtrip(display);
      if (bind.compositor) {
        surface = { wl_compositor_create_surface(bind.compositor), display, true };
        return surface;
      }
      wl_display_disconnect(display);
    }
#endif
    if (Display* display = XOpenDisplay(nullptr)) {
      surface = x11Surface(display);
      return surface;
    }
    return surface; // nothing to initialise on: the renderer reports unsupported
  }
}
