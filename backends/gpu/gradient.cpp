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

#include "gradient.h"

#include <cstring>

#include <bgfx/bgfx.h>

namespace gmpi::gpu {

  struct GradientAtlasTexture {
    bgfx::TextureHandle handle = { bgfx::kInvalidHandle };

    ~GradientAtlasTexture() {
      if (bgfx::isValid(handle))
        bgfx::destroy(handle);
    }
  };

  GradientAtlas::PackedGradientReference::~PackedGradientReference() {
    if (auto atlas_pointer = atlas.lock())
      (*atlas_pointer)->removeGradient(packed_gradient_rect);
  }

  GradientAtlas::GradientAtlas() {
    reference_ = std::make_shared<GradientAtlas*>(this);
    atlas_map_.fixWidth(Gradient::kMaxGradientResolution);
    atlas_map_.setPadding(0);
  }

  GradientAtlas::~GradientAtlas() = default;

  void GradientAtlas::updateGradient(const PackedGradientRect* gradient) {
    if (texture_ == nullptr || !bgfx::isValid(texture_->handle))
      return;

    int resolution = gradient->gradient.resolution();
    if (resolution == 0)
      return;

    std::unique_ptr<uint64_t[]> color_data = std::make_unique<uint64_t[]>(resolution);
    float step = 1.0f / std::max(1, resolution - 1);
    for (int i = 0; i < resolution; ++i)
      color_data[i] = gradient->gradient.sample(i * step).toABGR16F();

    bgfx::updateTexture2D(texture_->handle, 0, 0, gradient->x, gradient->y, resolution, 1,
                          bgfx::copy(color_data.get(), resolution * sizeof(uint64_t)));
  }

  void GradientAtlas::checkInit() {
    if (texture_ == nullptr)
      texture_ = std::make_unique<GradientAtlasTexture>();

    bool created = false;
    if (!bgfx::isValid(texture_->handle)) {
      texture_->handle = bgfx::createTexture2D(atlas_map_.width(), atlas_map_.height(), false, 1,
                                               bgfx::TextureFormat::RGBA16F);
      // A new texture is recycled GPU memory, most likely a dead canvas's
      // atlas. Zero it, and re-upload every strip: a gradient added before
      // the texture existed was never uploaded (updateGradient returns early
      // without a handle), and upstream only re-uploaded after a repack.
      const int bytes = atlas_map_.width() * atlas_map_.height() * 8;
      const bgfx::Memory* zeros = bgfx::alloc(bytes);
      std::memset(zeros->data, 0, zeros->size);
      bgfx::updateTexture2D(texture_->handle, 0, 0, 0, 0, atlas_map_.width(), atlas_map_.height(), zeros);
      created = true;
    }

    if (repacked_ || created) {
      repacked_ = false;
      for (auto& gradient : gradients_)
        updateGradient(gradient.second.get());
    }
  }

  void GradientAtlas::destroy() {
    texture_.reset();
  }

  void GradientAtlas::resize() {
    int prev_width = atlas_map_.width();
    int prev_height = atlas_map_.height();
    atlas_map_.pack();
    repacked_ = true;
    // EITHER dimension, not both: the width is fixed at kMaxGradientResolution
    // (see the constructor), so with && a taller atlas kept its shorter
    // texture and updateGradient wrote strips past its last row - a GPU-side
    // out-of-bounds copy that crashed in the driver once a frame brought
    // ~22 new gradients (wayland_bench --fill linear --load 22).
    if (atlas_map_.width() != prev_width || atlas_map_.height() != prev_height)
      texture_.reset();

    for (auto& gradient : gradients_) {
      const PackedRect& rect = atlas_map_.rectForId(gradient.second.get());
      gradient.second->x = rect.x;
      gradient.second->y = rect.y;
    }
  }

  const bgfx::TextureHandle& GradientAtlas::colorTextureHandle() {
    checkInit();
    return texture_->handle;
  }

}