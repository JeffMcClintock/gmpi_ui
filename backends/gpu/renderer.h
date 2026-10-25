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

#pragma once

#include "defines.h"

#include "screenshot.h"
#include <atomic>
#include <memory>
#include <string>

namespace gmpi::gpu {
  class GraphicsCallbackHandler;

  // The process-wide bgfx device. Single-threaded: bgfx renders on the
  // thread that submits (BGFX_CONFIG_MULTITHREADED=0 in gmpi_gpu.cmake).
  class Renderer {
  public:
    static Renderer& instance();

    Renderer();
    ~Renderer();

    static void resetResolution(int width, int height);

    // Every bgfx reset goes through here, with resetFlags() plus `extra`, so
    // no caller can drop a flag the others rely on (a windowless canvas used
    // to reset without SRGB_BACKBUFFER, and every window made after it came
    // up linear). The flags last applied, for tests.
    static void reset(int width, int height, uint32_t extra = 0);
    uint32_t lastResetFlags() const { return last_reset_flags_; }

    enum class NativeWindowType {
      Default, // HWND, NSView, X11 Window
      Wayland, // wl_surface*, with display = wl_display*
    };

    // Initialise with no window of the host's: on the process's model
    // surface (platform/model_surface.h). For offscreen rendering only; a
    // host that will show windows initialises through its frame.
    void initializeWindowless();
    void initialize(void* model_window, void* display, NativeWindowType type = NativeWindowType::Default);

    // Must be set before initialize(). Off lets a host pace frames itself
    // (Wayland frame callbacks), on lets the swap chain block for the display.
    void setVsync(bool vsync) { vsync_ = vsync; }
    bool vsync() const { return vsync_; }
    uint32_t resetFlags() const;

    // Window swap chains: 8-bit sRGB (the GPU encodes linear -> sRGB on
    // write) or RGBA16F linear where the platform's compositor takes it. On
    // Linux the Vulkan WSI rarely offers float surfaces, so sRGB is the
    // default there. Must be set before initialize().
    void setSrgbWindowSwapChains(bool srgb) { srgb_window_swap_chains_ = srgb; }
    bool srgbWindowSwapChains() const { return srgb_window_swap_chains_; }
    void setScreenshotData(const uint8_t* data, int width, int height, int pitch, bool blue_red);
    const Screenshot& screenshot() const { return screenshot_; }

    const std::string& errorMessage() const { return error_message_; }
    bool supported() const { return supported_; }
    bool swapChainSupported() const { return swap_chain_supported_; }
    bool initialized() const { return initialized_; }

    // Layers, offscreen targets and (where the driver allows) swap chains are
    // RGBA16F by default. Must be set before initialize(); initialize() clears it
    // if the renderer cannot use RGBA16F as a frame buffer.
    void setHalfFloatFrameBuffers(bool half_float) { half_float_frame_buffers_ = half_float; }
    bool halfFloatFrameBuffers() const { return half_float_frame_buffers_; }

    // Which compiled-shader format the running renderer loads: what a shader
    // brush's blobs (GmpiApiShaders.h) are matched against. None until
    // initialize() has run, or if it failed.
    enum class ShaderBinary { None, Spirv, Metal, Dxbc };
    ShaderBinary shaderBinary() const;

  private:
    bool initialized_ = false;
    bool supported_ = false;
    bool swap_chain_supported_ = false;
    bool half_float_frame_buffers_ = true;
    // Off by default: every gmpi_ui host paces its own frames (Wayland frame
    // callbacks, the X11 timer), and whichever creates the renderer first -
    // an offscreen canvas, say - must not impose vsync on windows made later.
    bool vsync_ = false;
    uint32_t last_reset_flags_ = 0;
    // Linear content into a window needs an encoding step. Windows gets an
    // RGBA16F (scRGB) swap chain. bgfx's Metal swap chain ignores the format
    // asked for and is BGRA8Unorm, where linear values present too dark (a
    // mid-grey 128 showed as 55); its sRGB variant encodes in hardware.
#if GMPI_GPU_LINUX || GMPI_GPU_MAC
    bool srgb_window_swap_chains_ = true;
#else
    bool srgb_window_swap_chains_ = false;
#endif

    Screenshot screenshot_;
    std::string error_message_;
    std::unique_ptr<GraphicsCallbackHandler> callback_handler_;
  };
}
