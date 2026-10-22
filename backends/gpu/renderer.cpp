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

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace gmpi::gpu {
  class GraphicsCallbackHandler : public bgfx::CallbackI {
    void fatal(const char* file_path, uint16_t line, bgfx::Fatal::Enum _code, const char* error) override {
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

    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override { }
    void captureEnd() override { }
    void captureFrame(const void*, uint32_t) override { }
  };

  uint32_t Renderer::resetFlags() const {
    uint32_t vsync = (vsync_ ? BGFX_RESET_VSYNC : 0) |
                     (srgb_window_swap_chains_ ? BGFX_RESET_SRGB_BACKBUFFER : 0);
#if GMPI_GPU_WINDOWS
    return BGFX_RESET_FLIP_AFTER_RENDER;
#elif GMPI_GPU_MAC
    return BGFX_RESET_FLIP_AFTER_RENDER | vsync;
#elif GMPI_GPU_LINUX
    // Without FLIP_AFTER_RENDER bgfx presents frame N at the start of frame
    // N+1 (to overlap CPU and GPU), so a window that presents once after
    // being idle shows that frame one present late. A UI presents on demand,
    // not continuously, so flip in the same frame, as on Windows and macOS.
    return vsync | BGFX_RESET_FLIP_AFTER_RENDER;
#else
    return 0;
#endif
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

    bgfx::Init bgfx_init;
    bgfx_init.resolution.numBackBuffers = 1;
    bgfx_init.resolution.width = 0;
    bgfx_init.resolution.height = 0;
    bgfx_init.callback = callback_handler_.get();

    bgfx_init.platformData.ndt = display;
    bgfx_init.platformData.nwh = model_window;
    bgfx_init.platformData.type = type == NativeWindowType::Wayland ? bgfx::NativeWindowHandleType::Wayland :
                                                                      bgfx::NativeWindowHandleType::Default;

    bgfx::RendererType::Enum supported_renderers[bgfx::RendererType::Count];
    uint8_t num_supported = bgfx::getSupportedRenderers(bgfx::RendererType::Count, supported_renderers);

#if GMPI_GPU_WINDOWS
    bgfx_init.type = bgfx::RendererType::Direct3D11;
#if USE_DIRECTX12
    for (int i = 0; i < num_supported; ++i) {
      if (supported_renderers[i] == bgfx::RendererType::Direct3D12)
        bgfx_init.type = bgfx::RendererType::Direct3D12;
    }
#endif
#elif GMPI_GPU_MAC
    bgfx_init.type = bgfx::RendererType::Metal;
    bgfx_init.resolution.width = 1;
    bgfx_init.resolution.height = 1;
#elif GMPI_GPU_LINUX
    bgfx_init.type = bgfx::RendererType::Vulkan;
#elif GMPI_GPU_EMSCRIPTEN
    bgfx_init.type = bgfx::RendererType::OpenGLES;
#endif

    bgfx_init.resolution.reset = resetFlags();

    for (int i = 0; i < num_supported && !supported_; ++i)
      supported_ = supported_renderers[i] == bgfx_init.type;

    if (!supported_) {
      GMPI_GPU_ASSERT(false);
      std::string renderer_name = bgfx::getRendererName(bgfx_init.type);
      error_message_ = renderer_name + " is required and not supported on this computer.";
    }

    // Upstream ignored the result; a failed init (no device, a native window
    // the renderer cannot make a surface for) then crashed at the first
    // bgfx call. Report it instead so a host can fall back.
    if (!supported_ || !bgfx::init(bgfx_init)) {
      supported_ = false;
      if (error_message_.empty())
        error_message_ = std::string(bgfx::getRendererName(bgfx_init.type)) + " failed to initialize.";
      return;
    }
    GMPI_GPU_ASSERT(bgfx::getRendererType() == bgfx_init.type);
    swap_chain_supported_ = bgfx::getCaps()->supported & BGFX_CAPS_SWAP_CHAIN;

    static constexpr uint32_t kHalfFloatCaps = BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER |
                                               BGFX_CAPS_FORMAT_TEXTURE_2D;
    uint32_t half_float_caps = bgfx::getCaps()->formats[bgfx::TextureFormat::RGBA16F];
    if (half_float_frame_buffers_ && (half_float_caps & kHalfFloatCaps) != kHalfFloatCaps) {
      half_float_frame_buffers_ = false;
      error_message_ = "RGBA16F frame buffers are not supported on this computer.";
    }
  }

  float hdrColorRange() {
    return Renderer::instance().halfFloatFrameBuffers() ? 1.0f : kHdrColorRange10Bit;
  }

  float hdrColorMultiplier() {
    return 1.0f / hdrColorRange();
  }

  void Renderer::resetResolution(int width, int height) {
#if GMPI_GPU_MAC
    bgfx::reset(width, height, instance().resetFlags());
#endif
  }

  void Renderer::setScreenshotData(const uint8_t* data, int width, int height, int pitch, bool blue_red) {
    screenshot_ = Screenshot(data, width, height, pitch, blue_red);
  }
}
