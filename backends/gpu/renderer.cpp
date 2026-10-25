/* Copyright Vital Audio, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "defines.h"
#include "graphics_utils.h"
#include "renderer.h"
#include "platform/model_surface.h"

#include "device/device.h"

namespace gmpi::gpu {
  class GraphicsCallbackHandler : public gfx::CallbackI {
    void fatal(const char* file_path, uint16_t line, gfx::Fatal::Enum _code, const char* error) override {
      GMPI_GPU_LOG("%s (%u)", file_path, unsigned(line));
      GMPI_GPU_LOG(error);
      GMPI_GPU_LOG("Graphics fatal error");
      GMPI_GPU_ASSERT(false);
    }

    void traceVargs(const char* file_path, uint16_t line, const char* format, va_list arg_list) override {
#if GMPI_GPU_GRAPHICS_DEBUG_LOGGING
      std::fprintf(stderr, "%s (%u) ", file_path, unsigned(line));
      std::vfprintf(stderr, format, arg_list);
#endif
    }

    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override { }
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override { }
    void profilerEnd() override { }
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override { }

    void screenShot(const char* file_path, uint32_t width, uint32_t height, uint32_t pitch,
                    const void* data, uint32_t size, bool y_flip) override {
      Renderer::instance().setScreenshotData(static_cast<const uint8_t*>(data), width, height, pitch, true);
    }

    void captureBegin(uint32_t, uint32_t, uint32_t, gfx::TextureFormat::Enum, bool) override { }
    void captureEnd() override { }
    void captureFrame(const void*, uint32_t) override { }
  };

  uint32_t Renderer::resetFlags() const {
    // Without FLIP_AFTER_RENDER bgfx presents frame N at the start of frame
    // N+1 (to overlap CPU and GPU), so a window that presents once after
    // being idle shows that frame one present late. A UI presents on demand,
    // not continuously, so flip in the same frame, everywhere.
    uint32_t flags = BGFX_RESET_FLIP_AFTER_RENDER;
#if !GMPI_GPU_WINDOWS
    // Windows passed neither flag in the fork and no Windows build has run
    // since; keep that until one does, then decide with a measurement.
    flags |= (vsync_ ? BGFX_RESET_VSYNC : 0) |
             (srgb_window_swap_chains_ ? BGFX_RESET_SRGB_BACKBUFFER : 0);
#endif
    return flags;
  }

  void Renderer::initializeWindowless() {
    const ModelSurface model = modelSurface();
    initialize(model.window, model.display, model.wayland ? NativeWindowType::Wayland : NativeWindowType::Default);
  }

  Renderer& Renderer::instance() {
    static Renderer renderer;
    return renderer;
  }

  Renderer::Renderer() = default;

  Renderer::~Renderer() = default;

  void Renderer::initialize(void* model_window, void* display, NativeWindowType type) {
    if (initialized_)
      return;

    callback_handler_ = std::make_unique<GraphicsCallbackHandler>();
    initialized_ = true;

    gfx::Init bgfx_init;
    bgfx_init.resolution.numBackBuffers = 1;
    bgfx_init.resolution.width = 0;
    bgfx_init.resolution.height = 0;
    bgfx_init.callback = callback_handler_.get();

    bgfx_init.platformData.ndt = display;
    bgfx_init.platformData.nwh = model_window;
    bgfx_init.platformData.type = type == NativeWindowType::Wayland ? gfx::NativeWindowHandleType::Wayland :
                                                                      gfx::NativeWindowHandleType::Default;

    // One current API per platform (ruled 2026-10-24), the only one compiled
    // in (gmpi_gpu.cmake). Where it is not available the renderer reports
    // unsupported and the host takes the CPU path.
#if GMPI_GPU_WINDOWS
    constexpr auto kRenderer = gfx::RendererType::Direct3D12;
#elif GMPI_GPU_MAC
    constexpr auto kRenderer = gfx::RendererType::Metal;
#elif GMPI_GPU_LINUX
    constexpr auto kRenderer = gfx::RendererType::Vulkan;
#else
    constexpr auto kRenderer = gfx::RendererType::Noop;
#endif
    bgfx_init.type = kRenderer;
#if GMPI_GPU_MAC
    // Metal wants a drawable on the model layer to come up.
    bgfx_init.resolution.width = 1;
    bgfx_init.resolution.height = 1;
#endif
    bgfx_init.resolution.reset = resetFlags();

    gfx::RendererType::Enum supported_renderers[gfx::RendererType::Count];
    const uint8_t num_supported = gfx::getSupportedRenderers(gfx::RendererType::Count, supported_renderers);
    for (int i = 0; i < num_supported && !supported_; ++i)
      supported_ = supported_renderers[i] == kRenderer;
    if (!supported_)
      error_message_ = std::string(gfx::getRendererName(kRenderer)) + " is not available.";

    if (!supported_) {
      GMPI_GPU_ASSERT(false);
      std::string renderer_name = gfx::getRendererName(bgfx_init.type);
      error_message_ = renderer_name + " is required and not supported on this computer.";
    }

    // Upstream ignored the result; a failed init (no device, a native window
    // the renderer cannot make a surface for) then crashed at the first
    // bgfx call. Report it instead so a host can fall back.
    if (!supported_ || !gfx::init(bgfx_init)) {
      supported_ = false;
      if (error_message_.empty())
        error_message_ = std::string(gfx::getRendererName(bgfx_init.type)) + " failed to initialize.";
      return;
    }
    GMPI_GPU_ASSERT(gfx::getRendererType() == bgfx_init.type);
    swap_chain_supported_ = gfx::getCaps()->supported & BGFX_CAPS_SWAP_CHAIN;

    static constexpr uint32_t kHalfFloatCaps = BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER |
                                               BGFX_CAPS_FORMAT_TEXTURE_2D;
    uint32_t half_float_caps = gfx::getCaps()->formats[gfx::TextureFormat::RGBA16F];
    if (half_float_frame_buffers_ && (half_float_caps & kHalfFloatCaps) != kHalfFloatCaps) {
      half_float_frame_buffers_ = false;
      error_message_ = "RGBA16F frame buffers are not supported on this computer.";
    }
  }

  float hdrColorRange() {
    return Renderer::instance().halfFloatFrameBuffers() ? 1.0f : kHdrColorRange10Bit;
  }

  Renderer::ShaderBinary Renderer::shaderBinary() const {
    if (!initialized_ || !supported_)
      return ShaderBinary::None;
    switch (gfx::getRendererType()) {
    case gfx::RendererType::Vulkan: return ShaderBinary::Spirv;
    case gfx::RendererType::Metal: return ShaderBinary::Metal;
    case gfx::RendererType::Direct3D12: return ShaderBinary::Dxbc;
    default: return ShaderBinary::None;
    }
  }

  float hdrColorMultiplier() {
    return 1.0f / hdrColorRange();
  }

  void Renderer::resetResolution(int width, int height) {
#if GMPI_GPU_MAC
    gfx::reset(width, height, instance().resetFlags());
#endif
  }

  void Renderer::setScreenshotData(const uint8_t* data, int width, int height, int pitch, bool blue_red) {
    screenshot_ = Screenshot(data, width, height, pitch, blue_red);
  }
}
