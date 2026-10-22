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

#include <cstdint>
#include <map>
#include <utility>

namespace gmpi::gpu {
  struct Image {
    Image() = default;
    Image(const unsigned char* data, int data_size, int width = 0, int height = 0) :
        data(data), data_size(data_size), width(width), height(height) { }

    const unsigned char* data = nullptr;
    int data_size = 0;
    int width = 0;
    int height = 0;
    // Identity for images whose memory is transient. The atlas caches by
    // identity, and a pointer is not one: a buffer freed after upload and the
    // next same-sized allocation at the same address would be "the same
    // image" and keep the stale pixels. 0 = identified by data pointer.
    uint64_t id = 0;

    bool operator==(const Image& other) const {
      return id == other.id && data == other.data && data_size == other.data_size && width == other.width &&
             height == other.height;
    }

    bool operator<(const Image& other) const {
      if (id != other.id)
        return id < other.id;
      return data < other.data || (data == other.data && data_size < other.data_size) ||
             (data == other.data && data_size == other.data_size && width < other.width) ||
             (data == other.data && data_size == other.data_size && width == other.width &&
              height < other.height);
    }
  };



  class ImageAtlasTexture;

  class ImageAtlas {
  public:
    static constexpr int kImageBuffer = 1;
    // Images are raw half-float RGBA, premultiplied linear: what a float
    // render target holds.
    static constexpr int kBytesPerPixel = 8;

    struct PackedImageRect {
      explicit PackedImageRect(const Image& image) : image(image) { }

      Image image;
      int x = 0;
      int y = 0;
      int w = 0;
      int h = 0;
    };

    struct PackedImageReference {
      PackedImageReference(std::weak_ptr<ImageAtlas*> atlas, const PackedImageRect* packed_image_rect) :
          atlas(std::move(atlas)), packed_image_rect(packed_image_rect) { }
      ~PackedImageReference();

      std::weak_ptr<ImageAtlas*> atlas;
      const PackedImageRect* packed_image_rect = nullptr;
    };

    class PackedImage {
    public:
      int x() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_image_rect->x;
      }

      int y() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_image_rect->y;
      }

      int w() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_image_rect->w;
      }

      int h() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_image_rect->h;
      }

      const Image& image() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_image_rect->image;
      }

      const PackedImageRect* packedImageRect() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_image_rect;
      }

      explicit PackedImage(std::shared_ptr<PackedImageReference> reference) :
          reference_(std::move(reference)) { }

    private:
      std::shared_ptr<PackedImageReference> reference_;
    };

    ImageAtlas();
    virtual ~ImageAtlas();

    PackedImage addImage(const Image& image, bool force_update = false);
    void clearStaleImages() {
      for (const auto& stale : stale_images_) {
        images_.erase(stale.first);
        atlas_map_.removeRect(stale.second);
        references_.erase(stale.first);
      }
      stale_images_.clear();
    }

    int width() const { return atlas_map_.width(); }
    int height() const { return atlas_map_.height(); }
    const bgfx::TextureHandle& textureHandle();
    void setImageCoordinates(TextureVertex* vertices, const PackedImage& image) const {
      setImageCoordinates(vertices, image, 0.0f, 0.0f, image.w(), image.h());
    }
    // Texture coordinates for `width` x `height` texels of the image from
    // (x, y) within it.
    void setImageCoordinates(TextureVertex* vertices, const PackedImage& image, float x, float y,
                             float width, float height) const;

  private:
    void resize();
    void loadImageRect(PackedImageRect* image) const;
    void updateImage(const PackedImageRect* image) const;

    void removeImage(const Image& image) {
      GMPI_GPU_ASSERT(images_.count(image));
      stale_images_[image] = images_[image].get();
    }

    void removeImage(const PackedImageRect* packed_image_rect) {
      removeImage(packed_image_rect->image);
    }

    std::map<Image, std::weak_ptr<PackedImageReference>> references_;
    std::map<Image, std::unique_ptr<PackedImageRect>> images_;
    std::map<Image, const PackedImageRect*> stale_images_;

    bool repacked_ = false;
    PackedAtlasMap<const PackedImageRect*> atlas_map_;
    std::unique_ptr<ImageAtlasTexture> texture_;
    std::shared_ptr<ImageAtlas*> reference_;
  };
}