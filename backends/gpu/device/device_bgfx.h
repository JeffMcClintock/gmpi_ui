#pragma once

// The bgfx device: gmpi::gpu::gfx as aliases of bgfx's types and inline
// forwards to bgfx's functions. Every alias and forward here is one the
// renderer uses; together they ARE the seam's surface (device.h). Nothing
// outside device/ includes bgfx.

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <cstdint>

namespace gmpi::gpu::gfx {
  // --- types -----------------------------------------------------------------
  using ViewId = bgfx::ViewId;
  constexpr uint16_t kInvalidHandle = bgfx::kInvalidHandle;

  using TextureHandle = bgfx::TextureHandle;
  using FrameBufferHandle = bgfx::FrameBufferHandle;
  using ShaderHandle = bgfx::ShaderHandle;
  using ProgramHandle = bgfx::ProgramHandle;
  using UniformHandle = bgfx::UniformHandle;
  using VertexBufferHandle = bgfx::VertexBufferHandle;
  using IndexBufferHandle = bgfx::IndexBufferHandle;

  using Memory = bgfx::Memory;
  using VertexLayout = bgfx::VertexLayout;
  using TransientVertexBuffer = bgfx::TransientVertexBuffer;
  using TransientIndexBuffer = bgfx::TransientIndexBuffer;

  using RendererType = bgfx::RendererType;
  using TextureFormat = bgfx::TextureFormat;
  using UniformType = bgfx::UniformType;
  using Attrib = bgfx::Attrib;
  using AttribType = bgfx::AttribType;
  using ViewMode = bgfx::ViewMode;
  using NativeWindowHandleType = bgfx::NativeWindowHandleType;
  using Fatal = bgfx::Fatal;

  using Init = bgfx::Init;
  using PlatformData = bgfx::PlatformData;
  using Caps = bgfx::Caps;
  using Stats = bgfx::Stats;
  using CallbackI = bgfx::CallbackI; // renderer.cpp's callback object derives from it

  template<typename Handle>
  inline bool isValid(Handle handle) { return bgfx::isValid(handle); }

  // --- lifecycle and queries ----------------------------------------------------
  inline bool init(const Init& init) { return bgfx::init(init); }
  inline void reset(uint32_t width, uint32_t height, uint32_t flags) { bgfx::reset(width, height, flags); }
  inline uint32_t frame() { return bgfx::frame(); }
  inline const Caps* getCaps() { return bgfx::getCaps(); }
  inline const Stats* getStats() { return bgfx::getStats(); }
  inline RendererType::Enum getRendererType() { return bgfx::getRendererType(); }
  inline const char* getRendererName(RendererType::Enum type) { return bgfx::getRendererName(type); }
  inline uint8_t getSupportedRenderers(uint8_t max, RendererType::Enum* out) { return bgfx::getSupportedRenderers(max, out); }

  // --- memory ------------------------------------------------------------------
  inline const Memory* alloc(uint32_t size) { return bgfx::alloc(size); }
  inline const Memory* copy(const void* data, uint32_t size) { return bgfx::copy(data, size); }
  inline const Memory* makeRef(const void* data, uint32_t size) { return bgfx::makeRef(data, size); }

  // --- textures and frame buffers ---------------------------------------------
  inline TextureHandle createTexture2D(uint16_t width, uint16_t height, bool hasMips, uint16_t numLayers,
                                       TextureFormat::Enum format,
                                       uint64_t flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE) {
    return bgfx::createTexture2D(width, height, hasMips, numLayers, format, flags);
  }
  inline void updateTexture2D(TextureHandle handle, uint16_t layer, uint8_t mip, uint16_t x, uint16_t y,
                              uint16_t width, uint16_t height, const Memory* mem) {
    bgfx::updateTexture2D(handle, layer, mip, x, y, width, height, mem);
  }
  inline uint32_t readTexture(TextureHandle handle, void* data) { return bgfx::readTexture(handle, data); }
  inline TextureHandle getTexture(FrameBufferHandle handle) { return bgfx::getTexture(handle); }
  inline FrameBufferHandle createFrameBuffer(uint16_t width, uint16_t height, TextureFormat::Enum format, uint64_t flags) {
    return bgfx::createFrameBuffer(width, height, format, flags);
  }
  // A swap chain on a native window.
  inline FrameBufferHandle createFrameBuffer(void* nativeWindow, uint16_t width, uint16_t height, TextureFormat::Enum format) {
    return bgfx::createFrameBuffer(nativeWindow, width, height, format);
  }
  inline void destroy(TextureHandle handle) { bgfx::destroy(handle); }
  inline void destroy(FrameBufferHandle handle) { bgfx::destroy(handle); }

  // --- shaders, programs, uniforms -----------------------------------------------
  inline ShaderHandle createShader(const Memory* mem) { return bgfx::createShader(mem); }
  inline ProgramHandle createProgram(ShaderHandle vertex, ShaderHandle fragment, bool destroyShaders) {
    return bgfx::createProgram(vertex, fragment, destroyShaders);
  }
  inline UniformHandle createUniform(const char* name, UniformType::Enum type, uint16_t num) {
    return bgfx::createUniform(name, type, num);
  }
  inline void setUniform(UniformHandle handle, const void* value, uint16_t num = 1) { bgfx::setUniform(handle, value, num); }
  inline void setTexture(uint8_t stage, UniformHandle sampler, TextureHandle handle) { bgfx::setTexture(stage, sampler, handle); }
  inline void destroy(ShaderHandle handle) { bgfx::destroy(handle); }
  inline void destroy(ProgramHandle handle) { bgfx::destroy(handle); }
  inline void destroy(UniformHandle handle) { bgfx::destroy(handle); }

  // --- buffers -----------------------------------------------------------------
  inline VertexBufferHandle createVertexBuffer(const Memory* mem, const VertexLayout& layout) {
    return bgfx::createVertexBuffer(mem, layout);
  }
  inline IndexBufferHandle createIndexBuffer(const Memory* mem) { return bgfx::createIndexBuffer(mem); }
  inline bool allocTransientBuffers(TransientVertexBuffer* vertices, const VertexLayout& layout, uint32_t numVertices,
                                    TransientIndexBuffer* indices, uint32_t numIndices, bool index32 = false) {
    return bgfx::allocTransientBuffers(vertices, layout, numVertices, indices, numIndices, index32);
  }
  inline void allocTransientVertexBuffer(TransientVertexBuffer* vertices, uint32_t num, const VertexLayout& layout) {
    bgfx::allocTransientVertexBuffer(vertices, num, layout);
  }
  inline void setVertexBuffer(uint8_t stream, VertexBufferHandle handle) { bgfx::setVertexBuffer(stream, handle); }
  inline void setVertexBuffer(uint8_t stream, const TransientVertexBuffer* vertices) { bgfx::setVertexBuffer(stream, vertices); }
  inline void setIndexBuffer(IndexBufferHandle handle) { bgfx::setIndexBuffer(handle); }
  inline void setIndexBuffer(const TransientIndexBuffer* indices) { bgfx::setIndexBuffer(indices); }
  inline void destroy(VertexBufferHandle handle) { bgfx::destroy(handle); }
  inline void destroy(IndexBufferHandle handle) { bgfx::destroy(handle); }

  // --- views and submission --------------------------------------------------------
  inline void setState(uint64_t state) { bgfx::setState(state); }
  inline void setViewRect(ViewId view, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    bgfx::setViewRect(view, x, y, width, height);
  }
  inline void setViewFrameBuffer(ViewId view, FrameBufferHandle handle) { bgfx::setViewFrameBuffer(view, handle); }
  inline void setViewClear(ViewId view, uint16_t flags, uint32_t rgba) { bgfx::setViewClear(view, flags, rgba); }
  inline void setViewMode(ViewId view, ViewMode::Enum mode) { bgfx::setViewMode(view, mode); }
  inline void submit(ViewId view, ProgramHandle program) { bgfx::submit(view, program); }
  inline void blit(ViewId view, TextureHandle dst, uint16_t dstX, uint16_t dstY, TextureHandle src,
                   uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height) {
    bgfx::blit(view, dst, dstX, dstY, src, srcX, srcY, width, height);
  }
  inline void requestScreenShot(FrameBufferHandle handle, const char* filePath) { bgfx::requestScreenShot(handle, filePath); }
}
