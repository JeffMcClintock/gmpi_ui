// Windows: bgfx's Direct3D 12 renderer initialises without a window (no
// main swap chain is created when nwh is null) and reports swap-chain
// support regardless, so there is no model window to make. UNVERIFIED: no
// Windows build of this backend has run yet (GPU_SINGLE_DEPENDENCY_PLAN.md).
// If it turns out one is needed, a hidden 1x1 HWND goes here.

#include "model_surface.h"

namespace gmpi::gpu {
  ModelSurface modelSurface(void*, void*) {
    return {};
  }
}
