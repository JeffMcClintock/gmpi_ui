// Entry points for the ASTC encoder gmpi_ui does not vendor.
//
// bimg's image.cpp guards its ASTC decoder with a plain if on BIMG_DECODE_ASTC,
// not #if, so an unoptimised (Debug) build still references these functions
// even though the branch can never run. gmpi_gpu.cmake builds with
// BIMG_DECODE_ASTC=0; the assert catches a configuration that turns it back on.

#include <astcenc.h>

static_assert(BIMG_DECODE_ASTC == 0, "the ASTC encoder is not vendored");

astcenc_error astcenc_config_init(astcenc_profile, unsigned int, unsigned int, unsigned int, float, unsigned int, astcenc_config*)
{
	return ASTCENC_ERR_BAD_PARAM;
}

astcenc_error astcenc_context_alloc(const astcenc_config*, unsigned int, astcenc_context** context)
{
	*context = nullptr;
	return ASTCENC_ERR_BAD_PARAM;
}

astcenc_error astcenc_decompress_image(astcenc_context*, const uint8_t*, size_t, astcenc_image*, const astcenc_swizzle*, unsigned int)
{
	return ASTCENC_ERR_BAD_PARAM;
}

void astcenc_context_free(astcenc_context*)
{
}

const char* astcenc_get_error_string(astcenc_error)
{
	return "ASTC is not vendored in gmpi_ui";
}
