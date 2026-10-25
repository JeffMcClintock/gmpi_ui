#pragma once

// The device seam: everything the renderer (backends/gpu/*.cpp) asks of the
// GPU, as one namespace, gmpi::gpu::gfx. The renderer includes this and
// nothing of bgfx's; gmpi_gpu.cmake refuses a renderer source that names
// bgfx:: (see the seam check there).
//
// The API is deliberately bgfx-shaped - the measured subset the renderer
// uses, 61 symbols (GPU_SINGLE_DEPENDENCY_PLAN.md), with bgfx's names and
// argument orders - so that the first implementation, device_bgfx.h, is
// inline forwards and the move behind the seam changed no behaviour. A
// second implementation (a metal-cpp device, METAL_BACKEND_PLAN.md) provides
// the same names; the renderer never knows which it has. Chosen at compile
// time: nothing needs both in one binary.
//
// Still bgfx's for now, by design of the first cut: the BGFX_* constants
// (state, caps, reset, texture, sampler and clear flags, BGFX_INVALID_HANDLE)
// come from bgfx/defines.h, a self-contained header any device can carry;
// and renderer.cpp's callback object derives from gfx::CallbackI, whose
// shape is bgfx's. Both narrow when a second device exists to say what it
// needs. Light forward declarations for headers: device_fwd.h.

#include "device_bgfx.h"
