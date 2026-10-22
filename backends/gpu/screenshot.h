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

#include "color.h"
#include "space.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace gmpi::gpu {
  class Screenshot {
  public:
    Screenshot() = default;
    Screenshot(const uint8_t* data, int width, int height, bool blue_red = false) :
        width_(width), height_(height) {
      data_.resize(width * height * 4);
      std::copy_n(data, width * height * 4, data_.data());
      if (blue_red)
        flipBlueRed();
    }

    Screenshot(const uint8_t* data, int width, int height, int pitch, bool blue_red = false) :
        width_(width), height_(height) {
      GMPI_GPU_ASSERT(pitch >= width * 4);

      data_.resize(width * height * 4);
      if (pitch == width * 4)
        std::copy_n(data, width * height * 4, data_.data());
      else {
        for (int y = 0; y < height; ++y)
          std::copy_n(data + y * pitch, width * 4, data_.data() + y * width * 4);
      }

      if (blue_red)
        flipBlueRed();
    }

    Screenshot(const Screenshot& other) = default;
    Screenshot& operator=(const Screenshot& other) = default;


    // half_float: the raw buffer receives RGBA16F texels (8 bytes/px) from a
    // read-back; data()/sample() convert to 8-bit lazily, since bgfx fills the
    // raw buffer asynchronously after this call.
    void setDimensions(int width, int height, bool half_float = false) {
      width_ = width;
      height_ = height;
      half_float_ = half_float;
      resolved_ = !half_float;
      data_.clear();
      data_.resize(width * height * 4, 0);
      half_data_.clear();
      if (half_float)
        half_data_.resize(width * height * 4, 0);
    }

    uint8_t* rawData() {
      return half_float_ ? reinterpret_cast<uint8_t*>(half_data_.data()) : data_.data();
    }

    bool halfFloat() const { return half_float_; }
    const uint16_t* halfData() const { return half_data_.data(); }

    const uint8_t* data() const {
      resolve();
      return data_.data();
    }
    uint8_t* data() {
      resolve();
      return data_.data();
    }
    int width() const { return width_; }
    int height() const { return height_; }
    Color sample(int x, int y) const {
      GMPI_GPU_ASSERT(x >= 0 && x < width_ && y >= 0 && y < height_);
      resolve();

      int index = (y * width_ + x) * 4;
      uint32_t color = (data_[index + 3] << 24) | (data_[index] << 16) | (data_[index + 1] << 8) |
                       data_[index + 2];
      return color;
    }

    Color sample(Point point) const { return sample(point.x, point.y); }

    static float halfToFloat(uint16_t half) {
      uint32_t sign = (half & 0x8000u) << 16;
      uint32_t exponent = (half >> 10) & 0x1f;
      uint32_t mantissa = half & 0x3ff;
      uint32_t bits = sign;
      if (exponent == 0) {
        if (mantissa) {  // Subnormal: renormalize
          exponent = 113;
          while ((mantissa & 0x400) == 0) {
            mantissa <<= 1;
            --exponent;
          }
          mantissa &= 0x3ff;
          bits |= (exponent << 23) | (mantissa << 13);
        }
      }
      else if (exponent == 31)
        bits |= 0x7f800000u | (mantissa << 13);
      else
        bits |= ((exponent + 112) << 23) | (mantissa << 13);
      float result;
      std::memcpy(&result, &bits, sizeof(result));
      return result;
    }

  private:
    void resolve() const {
      if (resolved_)
        return;
      resolved_ = true;
      for (size_t i = 0; i < half_data_.size(); ++i) {
        float value = std::min(1.0f, std::max(0.0f, halfToFloat(half_data_[i])));
        data_[i] = static_cast<uint8_t>(value * 255.0f + 0.5f);
      }
    }

    void flipBlueRed() {
      for (int i = 0; i < width_ * height_ * 4; i += 4) {
        uint8_t temp = data_[i];
        data_[i] = data_[i + 2];
        data_[i + 2] = temp;
      }
    }

    int width_ = 0;
    int height_ = 0;
    bool half_float_ = false;
    mutable bool resolved_ = true;
    mutable std::vector<uint8_t> data_;
    std::vector<uint16_t> half_data_;
  };
}
