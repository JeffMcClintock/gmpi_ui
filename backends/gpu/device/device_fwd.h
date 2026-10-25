#pragma once

// Forward declarations of the gfx types renderer HEADERS mention by pointer
// or reference, so that including a renderer header (through GpuGfx.h, into
// a host) pulls in no device header. See device.h.

namespace bgfx {
  struct VertexLayout;
  struct TextureHandle;
  struct ShaderHandle;
  struct ProgramHandle;
  struct UniformHandle;
  struct IndexBufferHandle;
  struct VertexBufferHandle;
  struct FrameBufferHandle;
  struct TransientIndexBuffer;
  struct TransientVertexBuffer;
}

namespace gmpi::gpu::gfx {
  using VertexLayout = bgfx::VertexLayout;
  using TextureHandle = bgfx::TextureHandle;
  using ShaderHandle = bgfx::ShaderHandle;
  using ProgramHandle = bgfx::ProgramHandle;
  using UniformHandle = bgfx::UniformHandle;
  using IndexBufferHandle = bgfx::IndexBufferHandle;
  using VertexBufferHandle = bgfx::VertexBufferHandle;
  using FrameBufferHandle = bgfx::FrameBufferHandle;
  using TransientIndexBuffer = bgfx::TransientIndexBuffer;
  using TransientVertexBuffer = bgfx::TransientVertexBuffer;
}
