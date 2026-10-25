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

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "device/device_fwd.h"

namespace gmpi::gpu {
  enum class BlendMode {
    Opaque,
    Composite,
    Alpha,
    Add,
    Sub,
    Mult,
    MaskAdd,
    MaskRemove,
    // The two halves of a REPLACE under a shaped clip (a masked clear):
    // Erase scales the destination by (1 - source alpha), then
    // PlusPremultiplied adds a premultiplied source. Together, with the clip
    // coverage m in the source alpha: dst * (1 - m) + colour * m, exact for
    // any clear colour, transparent included.
    Erase,
    PlusPremultiplied,
  };

  // Range hack for 10-bit HDR layers: colours are scaled by 1/4 into RGB10A2
  // and scaled back when sampled. Half-float layers (the default) hold values
  // above 1.0 natively, so both return 1 there. See Renderer::halfFloatFrameBuffers().
  static constexpr float kHdrColorRange10Bit = 4.0f;
  float hdrColorRange();
  float hdrColorMultiplier();
  static constexpr int kVerticesPerQuad = 4;
  static constexpr int kIndicesPerQuad = 6;

  const uint16_t kQuadTriangles[] = {
    0, 1, 2, 2, 1, 3,
  };

  struct PackedAtlasData;

  struct PackedRect {
    int x;
    int y;
    int w;
    int h;
  };

  struct TextureRect {
    int left;
    int top;
    int right;
    int bottom;
  };

  class AtlasPacker {
  public:
    AtlasPacker();
    ~AtlasPacker();

    bool addRect(PackedRect& rect);
    void clear();
    bool pack(std::vector<PackedRect>& rects, int width, int height);
    void setPadding(int padding) { padding_ = padding; }
    int padding() const { return padding_; }

    bool packed() const { return packed_; }

  private:
    std::unique_ptr<PackedAtlasData> data_;
    bool packed_ = false;
    int padding_ = 1;
    int rect_index_ = 0;
  };

  template<typename T = int>
  class PackedAtlasMap {
  public:
    static constexpr int kDefaultWidth = 64;

    PackedAtlasMap() = default;

    bool addRect(T id, int width, int height) {
      GMPI_GPU_ASSERT(lookup_.count(id) == 0);

      int index = packed_rects_.size();
      lookup_[id] = index;
      packed_rects_.push_back({ 0, 0, std::max(0, width), std::max(0, height) });
      return packer_.addRect(packed_rects_.back());
    }

    bool hasId(T id) const { return lookup_.count(id) > 0; }

    void removeRect(T id) {
      GMPI_GPU_ASSERT(lookup_.count(id) > 0);
      lookup_.erase(id);
    }

    void pack(int start_width = kDefaultWidth, int start_height = kDefaultWidth) {
      static constexpr int kMaxDimension = 1 << 14;

      checkRemovedRects();
      if (packed_rects_.size() == 1) {
        // Power-of-two sides, like the doubling below gives: in an atlas of
        // 65 x 50 texels a texel centre's normalised coordinate is inexact,
        // and a 1:1 quad sampled there mixes in a neighbour by one fp16
        // ulp on whichever rows round unluckily. Byte-identical pixels
        // then read back differently depending on image size.
        width_ = nextPowerOfTwo(std::max(1, packed_rects_[0].w + packer_.padding()));
        height_ = nextPowerOfTwo(std::max(1, packed_rects_[0].h + packer_.padding()));
        if (!packer_.pack(packed_rects_, width_, height_))
          GMPI_GPU_ASSERT(false);
      }
      else if (!packed_rects_.empty()) {
        width_ = std::max(kDefaultWidth, start_width);
        height_ = std::max(kDefaultWidth, start_height);

        while (width_ < kMaxDimension * 2 || height_ < kMaxDimension * 2) {
          width_ = fixed_width_ ? fixed_width_ : std::min(kMaxDimension, width_);
          height_ = std::min(kMaxDimension, height_);
          if (packer_.pack(packed_rects_, width_, height_))
            return;

          width_ *= 2;
          height_ *= 2;
        }
        // Does not fit in the largest atlas. Report it instead of claiming a
        // 32768x32768 atlas no texture can back: a frame buffer that size is
        // never created and whatever bound its handle crashed the driver.
        overflowed_ = true;
        width_ = 0;
        height_ = 0;
        GMPI_GPU_ASSERT(false);
      }
    }

    void clear() {
      lookup_.clear();
      packer_.clear();
      packed_rects_.clear();
    }

    void setPadding(int padding) { packer_.setPadding(padding); }
    int padding() const { return packer_.padding(); }

    const PackedRect& rectAtIndex(int index) const {
      GMPI_GPU_ASSERT(index >= 0 && index < packed_rects_.size());
      return packed_rects_[index];
    }

    TextureRect texturePositionsForIndex(int rect_index, bool bottom_left_origin = false) const {
      const PackedRect& packed_rect = rectAtIndex(rect_index);
      TextureRect result = { packed_rect.x, packed_rect.y, packed_rect.x + packed_rect.w,
                             packed_rect.y + packed_rect.h };

      if (bottom_left_origin) {
        result.top = height_ - result.top;
        result.bottom = height_ - result.bottom;
      }
      return result;
    }

    const PackedRect& rectForId(T id) const {
      GMPI_GPU_ASSERT(lookup_.count(id) > 0);
      return rectAtIndex(lookup_.at(id));
    }

    TextureRect texturePositionsForId(T id, bool bottom_left_origin = false) const {
      GMPI_GPU_ASSERT(lookup_.count(id) > 0);
      return texturePositionsForIndex(lookup_.at(id), bottom_left_origin);
    }

    void fixWidth(int width) { fixed_width_ = width; }
    bool overflowed() const { return overflowed_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool packed() const { return packer_.packed(); }
    int numRects() const { return packed_rects_.size(); }

  private:
    static int nextPowerOfTwo(int value) {
      int result = 1;
      while (result < value)
        result *= 2;
      return result;
    }

    void checkRemovedRects() {
      if (packed_rects_.size() == lookup_.size())
        return;

      std::vector<PackedRect> old_rects = std::move(packed_rects_);
      packed_rects_.reserve(lookup_.size());
      for (auto& packed : lookup_) {
        int index = packed_rects_.size();
        packed_rects_.push_back(old_rects[packed.second]);
        packed.second = index;
      }
    }

    int fixed_width_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool overflowed_ = false;
    std::vector<PackedRect> packed_rects_;
    AtlasPacker packer_;
    std::map<T, int> lookup_;
  };

  struct UvVertex {
    float x;
    float y;
    float u;
    float v;

    static gfx::VertexLayout& layout();
  };

  // One quad per path edge (see PathAtlas::updatePaths): the edge's two
  // points in atlas pixels, the origin of the path's atlas rect and the
  // rect's right edge, so the shaders can work in rect-local coordinates.
  struct PathVertex {
    float index;     // quad corner 0..3
    float origin_y;
    float x1;
    float y1;
    float x2;
    float y2;
    float origin_x;
    float right;

    static gfx::VertexLayout& layout();
  };

  struct GradientTexturePosition {
    float from_x;
    float from_y;
    float to_x;
    float to_y;
  };

  struct GradientVertexPosition {
    float from_x;
    float from_y;
    float to_x;
    float to_y;
    float coefficient1;
    float coefficient2;
    float coefficient3;
    float cone_height;

    float* position1() { return &from_x; }
    float* position2() { return &coefficient1; }
  };

  struct ShapeVertex {
    float x;
    float y;
    float garbage1;
    float garbage2;
    GradientTexturePosition gradient_texture_position;
    GradientVertexPosition gradient;
    float coordinate_x;
    float coordinate_y;
    float dimension_x;
    float dimension_y;
    float clamp_left;
    float clamp_top;
    float clamp_right;
    float clamp_bottom;
    float thickness;
    float fade;
    float value1;
    float value2;

    static gfx::VertexLayout& layout();
  };


  struct TextureVertex {
    float x;
    float y;
    float dimension_x;
    float dimension_y;
    GradientTexturePosition gradient_texture_position;
    GradientVertexPosition gradient;
    float texture_x;
    float texture_y;
    float direction_x;
    float direction_y;
    float clamp_left;
    float clamp_top;
    float clamp_right;
    float clamp_bottom;

    static gfx::VertexLayout& layout();
  };

  struct PostEffectVertex {
    float x;
    float y;
    float dimension_x;
    float dimension_y;
    GradientTexturePosition gradient_texture_position;
    GradientVertexPosition gradient;
    float texture_x;
    float texture_y;
    float value1;
    float value2;
    float clamp_left;
    float clamp_top;
    float clamp_right;
    float clamp_bottom;

    static gfx::VertexLayout& layout();
  };
}
