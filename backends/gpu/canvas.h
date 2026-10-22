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

#include "graphics_utils.h"
#include "layer.h"
#include "path.h"
#include "region.h"
#include "screenshot.h"
#include "shape_batcher.h"
#include "space.h"

#include <limits>

namespace gmpi::gpu {
  class Shader;

  // The drawing surface: a composite layer (a window's swap chain, or a
  // windowless render target), regions recorded into it, and the atlases
  // shapes draw from. Trimmed to what the GMPI adaptor (backends/GpuGfx.h)
  // draws, plus custom shaders and regions for post effects.
  class Canvas {
  public:
    static bool swapChainSupported();

    struct State {
      float x = 0;
      float y = 0;
      float scale = 1.0f;
      Brush set_brush;
      const PackedBrush* brush = nullptr;
      ClampBounds clamp;
      BlendMode blend_mode = BlendMode::Alpha;
      Region* current_region = nullptr;
    };

    Canvas();
    Canvas(const Canvas& other) = delete;
    Canvas& operator=(const Canvas&) = delete;

    void clearDrawnShapes();

    // Render-target mode: the content layer keeps its pixels between
    // submits and each submit composites only the shapes added since the
    // last one. Pair with clearShapesKeepPixels() after every submit.
    void setRetainPixels(bool retain) { default_region_.setPreserveContents(retain); }
    void clearShapesKeepPixels() { default_region_.clear(); }
    // Shapes do not invalidate anything by themselves (the UI layer does);
    // call before submit() so the content layer draws what was recorded.
    void invalidateContent() { default_region_.invalidate(); }
    void invalidateContentRect(int x, int y, int width, int height) {
      default_region_.invalidateRect({ x, y, width, height });
    }
    int submit(int submit_pass = 0);

    // redraw_regions: re-render the content regions (the UI model, where
    // shapes are re-recorded every frame) or only re-composite what the
    // layers already hold (render-target mode).
    const Screenshot& takeScreenshot(bool redraw_regions = true);
    const Screenshot& screenshot() const;

    void ensureLayerExists(int layer);
    Layer* layer(int index) {
      ensureLayerExists(index);
      return layers_[index];
    }

    void invalidateRectInRegion(IBounds rect, const Region* region, int layer);
    void addToPackedLayer(Region* region, int layer_index);
    void removeFromPackedLayer(const Region* region, int layer_index);
    void changePackedLayer(Region* region, int from, int to);

    void pairToWindow(void* window_handle, int width, int height) {
      GMPI_GPU_ASSERT(swapChainSupported());
      composite_layer_.pairToWindow(window_handle, width, height);
      setDimensions(width, height);
    }

    void setWindowless(int width, int height);
    void removeFromWindow() { composite_layer_.removeFromWindow(); }

    int width() const { return composite_layer_.width(); }
    int height() const { return composite_layer_.height(); }
    void setDimensions(int width, int height);
    void setDpiScale(float scale) { dpi_scale_ = scale; }
    void setNativePixelScale() { state_.scale = 1.0f; }
    void setLogicalPixelScale() { state_.scale = dpi_scale_; }

    float dpiScale() const { return dpi_scale_; }
    void updateTime(double time);
    double time() const { return render_time_; }
    double deltaTime() const { return delta_time_; }
    int frameCount() const { return render_frame_; }

    void setBlendMode(BlendMode blend_mode) { state_.blend_mode = blend_mode; }

    const Brush& brush() { return state_.set_brush; }
    void setBrush(const Brush& brush) {
      state_.set_brush = brush;
      state_.brush = state_.current_region->addBrush(&gradient_atlas_, brush.gradient(),
                                                     brush.position() * state_.scale);
    }
    void setColor(const Brush& brush) { setBrush(brush); }
    void setColor(unsigned int color) { setBrush(Brush::solid(color)); }
    void setColor(const Color& color) { setBrush(Brush::solid(color)); }

    void fill() {
      addShape(Fill(state_.clamp, state_.brush, state_.x, state_.y,
                    state_.clamp.right - state_.clamp.left, state_.clamp.bottom - state_.clamp.top));
    }

    void fill(float x, float y, float width, float height) {
      float fill_x = pixels(x);
      float fill_y = pixels(y);
      float fill_w = pixels(width);
      float fill_h = pixels(height);
      addShape(Fill(state_.clamp.clamp(fill_x, fill_y, fill_w, fill_h), state_.brush,
                    state_.x + fill_x, state_.y + fill_y, fill_w, fill_h));
    }

    void circle(float x, float y, float width) {
      addShape(Circle(state_.clamp, state_.brush, state_.x + pixels(x), state_.y + pixels(y),
                      pixels(width)));
    }

    void rectangle(float x, float y, float width, float height) {
      addShape(Rectangle(state_.clamp, state_.brush, state_.x + pixels(x), state_.y + pixels(y),
                         pixels(width), pixels(height)));
    }

    // A custom shader over a rect. Not driven by the GMPI adaptor yet: kept
    // for the shader feature (VISAGE_BACKEND_PLAN.md, milestone 8).
    void shader(Shader* shader, float x, float y, float width, float height) {
      addShape(ShaderWrapper(state_.clamp, state_.brush, state_.x + pixels(x), state_.y + pixels(y),
                             pixels(width), pixels(height), shader));
    }

    // Fill a flattened path through the coverage atlas.
    void fill(const Path& path, float x, float y, float width, float height) {
      if (path.numPoints() == 0)
        return;

      addShape(PathFillWrapper(state_.clamp, state_.brush, state_.x + pixels(x), state_.y + pixels(y),
                               pixels(width), pixels(height), path, pathAtlas(), state_.scale));
    }

    void fill(const Path& path, float x, float y) {
      if (path.numPoints() == 0)
        return;

      auto bounding_box = path.boundingBox();
      addShape(PathFillWrapper(state_.clamp, state_.brush, state_.x + pixels(x),
                               state_.y + pixels(y), bounding_box.right() * state_.scale + 1.0f,
                               bounding_box.bottom() * state_.scale + 1.0f, path, pathAtlas(),
                               state_.scale));
    }

    void fill(const Path& path) { fill(path, 0, 0); }

    // Draw raw RGBA16F pixels (premultiplied, linear, width*height*8 bytes,
    // tightly packed) stretched into dest_width x dest_height at (x, y). The
    // data must stay valid for as long as the shape is drawn: the atlas reads
    // it again when it repacks. The brush tints: use a solid grey of the
    // opacity, so colour and alpha scale together.
    void halfImage(const unsigned char* data, int width, int height, float x, float y,
                   float dest_width, float dest_height, uint64_t id = 0) {
      halfImage(data, width, height, x, y, dest_width, dest_height, id, 0.0f, 0.0f,
                static_cast<float>(width), static_cast<float>(height));
    }

    // The same, drawing only source_width x source_height texels of the
    // image from (source_x, source_y).
    void halfImage(const unsigned char* data, int width, int height, float x, float y,
                   float dest_width, float dest_height, uint64_t id, float source_x, float source_y,
                   float source_width, float source_height) {
      Image image(data, width * height * 8, width, height);
      image.id = id;
      addShape(ImageWrapper(state_.clamp, state_.brush, state_.x + x, state_.y + y, dest_width,
                            dest_height, image, halfImageAtlas(), source_x, source_y, source_width,
                            source_height));
    }

    // Keep raw fp16 pixels resident in the half-float atlas for as long as
    // the returned handle lives, so a bitmap drawn every frame is uploaded
    // once. Draw it with halfImage() and the same data pointer and id. The
    // data must stay valid while the handle lives: the atlas reads it again
    // when it repacks.
    ImageAtlas::PackedImage retainHalfImage(const unsigned char* data, int width, int height, uint64_t id) {
      Image image(data, width * height * 8, width, height);
      image.id = id;
      return halfImageAtlas()->addImage(image);
    }

    void saveState() { state_memory_.push_back(state_); }

    void restoreState() {
      GMPI_GPU_ASSERT(!state_memory_.empty());
      if (!state_memory_.empty()) {
        state_ = state_memory_.back();
        state_memory_.pop_back();
      }
    }

    void setPosition(float x, float y) {
      state_.x += x * state_.scale;
      state_.y += y * state_.scale;
    }

    void addRegion(Region* region) {
      default_region_.addRegion(region);
      region->setCanvas(this);
    }

    void beginRegion(Region* region) {
      region->clear();
      saveState();
      state_.x = 0;
      state_.y = 0;
      setLogicalPixelScale();
      state_.brush = nullptr;
      state_.blend_mode = BlendMode::Alpha;
      setClampBounds(0, 0, region->width(), region->height());
      state_.current_region = region;
    }

    void endRegion() { restoreState(); }

    void setClampBounds(float x, float y, float width, float height) {
      GMPI_GPU_ASSERT(width >= 0);
      GMPI_GPU_ASSERT(height >= 0);
      state_.clamp.left = state_.x + x * state_.scale;
      state_.clamp.top = state_.y + y * state_.scale;
      state_.clamp.right = state_.clamp.left + width * state_.scale;
      state_.clamp.bottom = state_.clamp.top + height * state_.scale;
    }

    void setClampBounds(const ClampBounds& bounds) { state_.clamp = bounds; }

    void trimClampBounds(float x, float y, float width, float height) {
      state_.clamp = state_.clamp.clamp(state_.x + x * state_.scale, state_.y + y * state_.scale,
                                        width * state_.scale, height * state_.scale);
    }

    const ClampBounds& currentClampBounds() const { return state_.clamp; }
    bool totallyClamped() const { return state_.clamp.totallyClamped(); }

    std::vector<std::string> debugInfo() const;

    PathAtlas* pathAtlas() { return &path_atlas_; }
    ImageAtlas* halfImageAtlas() { return &image_atlas_half_; }
    GradientAtlas* gradientAtlas() { return &gradient_atlas_; }

    State* state() { return &state_; }

  private:
    float pixels(float value) const { return state_.scale * value; }

    template<typename T>
    void addShape(T shape) {
      state_.current_region->shape_batcher_.addShape(std::move(shape), state_.blend_mode);
    }

    float dpi_scale_ = 1.0f;
    double render_time_ = 0.0;
    double delta_time_ = 0.0;
    int render_frame_ = 0;
    int last_skipped_frame_ = 0;

    std::vector<State> state_memory_;
    State state_;

    GradientAtlas gradient_atlas_;
    PathAtlas path_atlas_;
    ImageAtlas image_atlas_half_;

    Region window_region_;
    Region default_region_;
    Layer composite_layer_;
    std::vector<std::unique_ptr<Layer>> intermediate_layers_;
    std::vector<Layer*> layers_;

    float refresh_time_ = 0.0f;

    GMPI_GPU_LEAK_CHECKER(Canvas)
  };
}
