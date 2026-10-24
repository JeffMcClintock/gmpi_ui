#pragma once

// Shader brushes: a brush whose colour at each pixel is computed by a
// fragment shader the module supplies, usable with every fill that takes an
// IBrush (fillRectangle, fillGeometry, ...). Milestone 8 of
// VISAGE_BACKEND_PLAN.md; the design is in SHADER_API_PLAN.md.
//
// This is an EXTENSION, found by probing IShaderFactory::guid through
// queryInterface on the IFactory that IDeviceContext::getFactory() returns.
// IDeviceContext itself is untouched (see the vtable policy in
// GmpiApiDrawing.h); a backend that has no GPU answers NoSupport and the
// module keeps whatever it would have drawn anyway.
//
// Authoring: the module writes one function in bgfx's .sc dialect,
//
//     vec4 shade(vec2 p);   // straight (NOT premultiplied) linear RGBA
//
// where p is the pixel's position in BRUSH space: local coordinates taken
// through the inverse of BrushProperties::transform, exactly as a gradient
// brush places its axis. Available to it: `u_time.x` (seconds, host clock),
// `u_params[8]` (the vec4s given at creation) and the helpers in
// backends/gpu/shaders/shader_utils.sh. The host compiles that function
// into its own programs - one per fill kind - ahead of time
// (backends/gpu/shaders/compile_shaders.cmake, USER_SHADER mode) and the
// module ships the resulting blobs for each renderer it was compiled for.
// The host picks the blob for the renderer it is running; blobs are bgfx
// binaries and must come from the same bgfx the host was built with.

#include "GmpiApiDrawing.h"

#pragma pack(push, 8)

namespace gmpi
{
namespace drawing
{
namespace api
{

// The renderer a blob was compiled for (shaderc's --platform/-p pair).
enum class ShaderRenderer : int32_t
{
    Spirv = 1, // Vulkan: Linux
    Metal = 2, // macOS, iOS
    Dx11  = 3, // Windows (Direct3D 11 and 12 share the HLSL 5 blob)
};

// Which host program the user function was compiled into.
enum class ShaderStage : int32_t
{
    Quad = 1, // axis-aligned rectangles: one quad, the shader paints every pixel
    Path = 2, // everything that goes through the path atlas: coverage * shade(p)
};

struct ShaderBlob
{
    ShaderRenderer renderer;
    ShaderStage stage;
    const uint8_t* data;
    int32_t size;
};

// INTERFACE 'IShaderBrush'
// Immutable, like the gradient brushes: parameters, fallback colour and
// brush properties are fixed at creation. A new value means a new brush -
// cheap, since the compiled program is shared by every brush made from the
// same blobs. Where a backend cannot run the program (a brush space the
// fill cannot provide, text, a geometry clip, a stroke that falls back to
// the CPU) it paints the fallback colour instead, so a shader brush never
// draws nothing.
struct DECLSPEC_NOVTABLE IShaderBrush : IBrush
{
    // {3A0D9E2B-5C41-4F8E-9B27-6E1F0C8D4A52}
    inline static const gmpi::api::Guid guid =
    { 0x3A0D9E2B, 0x5C41, 0x4F8E, { 0x9B, 0x27, 0x6E, 0x1F, 0x0C, 0x8D, 0x4A, 0x52} };
};

// INTERFACE 'IShaderFactory'
// Probed from IFactory. NoSupport when the backend has no GPU path, or when
// no blob matches its renderer; the module then draws its fallback.
struct DECLSPEC_NOVTABLE IShaderFactory : gmpi::api::IUnknown
{
    // blobs: every (renderer, stage) the module has. A Quad blob for the
    // running renderer is required; a Path blob is optional (path fills
    // paint the fallback colour without one).
    // params: up to 8 vec4s, read in the shader as u_params[i]; missing
    // entries are zero.
    virtual gmpi::ReturnCode createShaderBrush(
        const ShaderBlob* blobs, int32_t blobCount,
        const float* params, int32_t paramCount,
        const Color* fallbackColor,
        const BrushProperties* brushProperties,
        IShaderBrush** returnShaderBrush) = 0;

    // {B7E4C1A9-2D63-4B5F-8E0A-91C7F3D2E6B4}
    inline static const gmpi::api::Guid guid =
    { 0xB7E4C1A9, 0x2D63, 0x4B5F, { 0x8E, 0x0A, 0x91, 0xC7, 0xF3, 0xD2, 0xE6, 0xB4} };
};

// A shader brush takes at most this many vec4 parameters.
inline constexpr int32_t kShaderBrushMaxParams = 8;

} // namespace api
} // namespace drawing
} // namespace gmpi

#pragma pack(pop)
