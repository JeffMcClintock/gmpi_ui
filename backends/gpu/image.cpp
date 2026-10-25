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

#include "image.h"

#include "device/device.h"
#include <cstring>

namespace gmpi::gpu {

  class ImageAtlasTexture {
  public:
    ImageAtlasTexture(int width, int height) : width_(width), height_(height) { }

    ~ImageAtlasTexture() { destroyHandle(); }

    void destroyHandle() {
      if (gfx::isValid(texture_handle_))
        gfx::destroy(texture_handle_);

      texture_handle_ = BGFX_INVALID_HANDLE;
    }

    bool hasHandle() const { return gfx::isValid(texture_handle_); }

    gfx::TextureHandle& handle() { return texture_handle_; }

    void checkHandle() {
      if (!gfx::isValid(texture_handle_)) {
        // Zero-filled: the padding texels around each image are never written
        // but LINEAR filtering reads them, and uninitialised float memory can
        // be NaN. Filled by an update, not at creation: a texture created with
        // data is immutable in bgfx and silently ignores every later update.
        texture_handle_ = gfx::createTexture2D(width_, height_, false, 1, gfx::TextureFormat::RGBA16F);
        const gfx::Memory* zeros = gfx::alloc(width_ * height_ * ImageAtlas::kBytesPerPixel);
        std::memset(zeros->data, 0, zeros->size);
        gfx::updateTexture2D(texture_handle_, 0, 0, 0, 0, width_, height_, zeros);
      }
    }

    void updateTexture(const unsigned char* data, int x, int y, int width, int height) {
      GMPI_GPU_ASSERT(gfx::isValid(texture_handle_));
      gfx::updateTexture2D(texture_handle_, 0, 0, x, y, width, height,
                            gfx::copy(data, width * height * ImageAtlas::kBytesPerPixel));
    }

  private:
    int width_ = 0;
    int height_ = 0;
    gfx::TextureHandle texture_handle_ = BGFX_INVALID_HANDLE;
  };

  ImageAtlas::PackedImageReference::~PackedImageReference() {
    if (auto atlas_pointer = atlas.lock())
      (*atlas_pointer)->removeImage(packed_image_rect);
  }

  ImageAtlas::ImageAtlas() {
    reference_ = std::make_shared<ImageAtlas*>(this);
    atlas_map_.setPadding(kImageBuffer);
  }

  ImageAtlas::~ImageAtlas() = default;

  ImageAtlas::PackedImage ImageAtlas::addImage(const Image& image, bool force_update) {
    if (images_.count(image) == 0) {
      int width = image.width;
      int height = image.height;
      std::unique_ptr<PackedImageRect> packed_image_rect = std::make_unique<PackedImageRect>(image);
      if (!atlas_map_.addRect(packed_image_rect.get(), width, height))
        resize();

      loadImageRect(packed_image_rect.get());
      updateImage(packed_image_rect.get());
      images_[image] = std::move(packed_image_rect);
    }
    else if (force_update)
      updateImage(images_[image].get());

    stale_images_.erase(image);

    if (auto reference = references_[image].lock())
      return PackedImage(reference);

    auto reference = std::make_shared<PackedImageReference>(reference_, images_[image].get());
    references_[image] = reference;
    return PackedImage(reference);
  }

  void ImageAtlas::resize() {
    clearStaleImages();

    int last_width = atlas_map_.width();
    int last_height = atlas_map_.height();
    atlas_map_.pack();
    for (auto& image : images_)
      loadImageRect(image.second.get());

    if (atlas_map_.width() != last_width || atlas_map_.height() != last_height)
      texture_ = std::make_unique<ImageAtlasTexture>(atlas_map_.width(), atlas_map_.height());
    else
      repacked_ = true;
  }

  void ImageAtlas::loadImageRect(PackedImageRect* image) const {
    const PackedRect& rect = atlas_map_.rectForId(image);
    image->x = rect.x;
    image->y = rect.y;
    image->w = rect.w;
    image->h = rect.h;
  }

  void ImageAtlas::updateImage(const PackedImageRect* image) const {
    if (texture_ == nullptr || !gfx::isValid(texture_->handle()))
      return;

    PackedRect packed_rect = atlas_map_.rectForId(image);
    texture_->updateTexture(image->image.data, packed_rect.x, packed_rect.y, packed_rect.w, packed_rect.h);
  }

  const gfx::TextureHandle& ImageAtlas::textureHandle() {
    GMPI_GPU_ASSERT(texture_.get());
    if (!texture_->hasHandle() || repacked_) {
      texture_->checkHandle();
      for (auto& image : images_) {
        if (stale_images_.count(image.first) == 0)
          updateImage(image.second.get());
      }
      repacked_ = false;
    }
    return texture_->handle();
  }

  void ImageAtlas::setImageCoordinates(ImageVertex* vertices, const PackedImage& image, float x,
                                       float y, float width, float height) const {
    float left = image.x() + x;
    float top = image.y() + y;
    float right = left + width;
    float bottom = top + height;

    vertices[0].texture_x = left;
    vertices[0].texture_y = top;
    vertices[1].texture_x = right;
    vertices[1].texture_y = top;
    vertices[2].texture_x = left;
    vertices[2].texture_y = bottom;
    vertices[3].texture_x = right;
    vertices[3].texture_y = bottom;

    for (int i = 0; i < kVerticesPerQuad; ++i) {
      vertices[i].direction_x = 1.0f;
      vertices[i].direction_y = 0.0f;
    }
  }
}