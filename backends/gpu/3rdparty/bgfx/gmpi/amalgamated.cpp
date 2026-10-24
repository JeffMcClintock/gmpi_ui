// gmpi_ui's build of bgfx: one translation unit, like bgfx's own
// src/amalgamated.cpp, but listing only the sources vendored here. bgfx's
// file includes every renderer it has (GL, D3D11, the consoles) and relies
// on guards inside each; ours includes the one renderer per platform that
// gmpi_ui uses - Vulkan, Direct3D 12, Metal (amalgamated.mm) - and takes the
// entry points of the rest from renderer_stubs.cpp. Each renderer source
// compiles to its own stub where its BGFX_CONFIG_RENDERER_* is 0, so all
// three are listed on every platform. See ../VENDORED.md.

#include "bgfx.cpp"
#include "debug_renderdoc.cpp"
#include "renderer_noop.cpp"
#include "shader.cpp"
#include "shader_dxbc.cpp"
#include "shader_spirv.cpp"
#include "topology.cpp"
#include "vertexlayout.cpp"

#include "renderer_vk.cpp"
#include "renderer_d3d12.cpp"
#include "dxgi.cpp"
#include "nvapi.cpp"

#include "renderer_stubs.cpp"
