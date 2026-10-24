// Entry points for the bgfx renderers gmpi_ui does not vendor.
//
// bgfx.cpp's s_rendererCreator table names every renderer's rendererCreate /
// rendererDestroy whatever the configuration; each upstream renderer source
// supplies a stub in its #else branch when compiled out. These are those
// stubs, so the sources themselves (GL alone is 277 KB) need not be vendored.
// gmpi_gpu.cmake compiles these renderers out on every platform; the asserts
// catch a configuration that tries to turn one back on.

#include "bgfx_p.h"

static_assert(!BGFX_CONFIG_RENDERER_AGC, "AGC is not vendored");
static_assert(!BGFX_CONFIG_RENDERER_DIRECT3D11, "Direct3D 11 is not vendored: gmpi_ui uses Direct3D 12");
static_assert(!BGFX_CONFIG_RENDERER_GNM, "GNM is not vendored");
static_assert(!BGFX_CONFIG_RENDERER_NVN, "NVN is not vendored");
static_assert(!BGFX_CONFIG_RENDERER_OPENGL && !BGFX_CONFIG_RENDERER_OPENGLES, "OpenGL is not vendored");

#define GMPI_BGFX_RENDERER_STUB(_namespace)                                      \
	namespace bgfx { namespace _namespace {                                      \
		RendererContextI* rendererCreate(const Init& _init) { BX_UNUSED(_init); return NULL; } \
		void rendererDestroy() {}                                                \
	} }

GMPI_BGFX_RENDERER_STUB(agc)
GMPI_BGFX_RENDERER_STUB(d3d11)
GMPI_BGFX_RENDERER_STUB(gnm)
GMPI_BGFX_RENDERER_STUB(nvn)
GMPI_BGFX_RENDERER_STUB(gl)

#undef GMPI_BGFX_RENDERER_STUB
