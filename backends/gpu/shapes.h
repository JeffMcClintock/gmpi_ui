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

#include "embedded_file.h"
#include "gradient.h"
#include "image.h"
#include "path.h"

#include <algorithm>
#include <cfloat>

#define GMPI_GPU_CREATE_BATCH_ID \
  static void* batchId() {     \
    static int batch_id = 0;   \
    return &batch_id;          \
  }

namespace gmpi::gpu {
  class PostEffect;
  class Region;
  class Layer;
  class Shader;

  static constexpr float kFullThickness = FLT_MAX;

  enum class Direction {
    Left,
    Up,
    Right,
    Down,
  };

  struct ClampBounds {
    float left = 1.0f;
    float top = 1.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    bool totallyClamped() const { return bottom <= top || right <= left; }

    ClampBounds withOffset(int x, int y) const {
      return { left + x, top + y, right + x, bottom + y };
    }

    ClampBounds clamp(float x, float y, float width, float height) const {
      float new_top = std::max(top, y);
      float new_left = std::max(left, x);
      return { new_left, new_top, std::max(new_left, std::min(right, x + width)),
               std::max(new_top, std::min(bottom, y + height)) };
    }
  };

  template<typename T>
  struct DrawBatch {
    DrawBatch(const std::vector<T>* shapes, std::vector<IBounds>* invalid_rects, int x, int y) :
        shapes(shapes), invalid_rects(invalid_rects), x(x), y(y) { }

    const std::vector<T>* shapes;
    std::vector<IBounds>* invalid_rects;
    int x = 0;
    int y = 0;
  };

  template<typename T>
  using BatchVector = std::vector<DrawBatch<T>>;

  struct BaseShape {
    BaseShape(const void* batch_id, const ClampBounds& clamp, const PackedBrush* brush, float x,
              float y, float width, float height) :
        batch_id(batch_id), clamp(clamp), brush(brush), x(x), y(y), width(width), height(height) { }

    const void* batch_id = nullptr;
    ClampBounds clamp;
    const PackedBrush* brush;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool radialGradient() const {
      return brush && brush->position().shape == GradientPosition::InterpolationShape::Radial;
    }

    bool overlapsShape(const BaseShape& other) const {
      return x < other.x + other.width && x + width > other.x && y < other.y + other.height &&
             y + height > other.y;
    }

    bool totallyClamped(const ClampBounds& clamp) const {
      return clamp.totallyClamped() || clamp.left >= x + width || clamp.right <= x ||
             clamp.top >= y + height || clamp.bottom <= y;
    }
  };

  template<typename T>
  void setCornerCoordinates(T* vertices) {
    vertices[0].coordinate_x = -1.0f;
    vertices[0].coordinate_y = -1.0f;
    vertices[1].coordinate_x = 1.0f;
    vertices[1].coordinate_y = -1.0f;
    vertices[2].coordinate_x = -1.0f;
    vertices[2].coordinate_y = 1.0f;
    vertices[3].coordinate_x = 1.0f;
    vertices[3].coordinate_y = 1.0f;
  }

  template<typename T>
  void setQuadPositions(T* vertices, const BaseShape& shape, ClampBounds clamp,
                        float x_offset = 0.0f, float y_offset = 0.0f) {
    float left = shape.x + x_offset;
    float top = shape.y + y_offset;
    float right = left + shape.width;
    float bottom = top + shape.height;
    PackedBrush::setVertexGradientPositions(shape.brush, vertices, kVerticesPerQuad, x_offset,
                                            y_offset, left, top, right, bottom);

    for (int i = 0; i < kVerticesPerQuad; ++i) {
      vertices[i].dimension_x = shape.width;
      vertices[i].dimension_y = shape.height;
      vertices[i].clamp_left = clamp.left;
      vertices[i].clamp_top = clamp.top;
      vertices[i].clamp_right = clamp.right;
      vertices[i].clamp_bottom = clamp.bottom;
    }

    vertices[0].x = left;
    vertices[0].y = top;
    vertices[1].x = right;
    vertices[1].y = top;
    vertices[2].x = left;
    vertices[2].y = bottom;
    vertices[3].x = right;
    vertices[3].y = bottom;
  }

  template<typename VertexType = ShapeVertex>
  struct Shape : BaseShape {
    typedef VertexType Vertex;

    Shape(const void* batch_id, const ClampBounds& clamp, const PackedBrush* brush, float x, float y,
          float width, float height) : BaseShape(batch_id, clamp, brush, x, y, width, height) { }
  };

  template<typename VertexType = ShapeVertex>
  struct Primitive : Shape<VertexType> {
    static void* taggedPointer(void* pointer, int tag) {
      uintptr_t int_value = reinterpret_cast<uintptr_t>(pointer);
      return reinterpret_cast<void*>(int_value | uintptr_t(tag) & 3);
    }

    Primitive(const void* batch_id, const ClampBounds& clamp, const PackedBrush* brush, float x,
              float y, float width, float height) :
        Shape<VertexType>(batch_id, clamp, brush, x, y, width, height) { }

    void setPrimitiveData(VertexType* vertices) const {
      float thick = thickness == kFullThickness ? (this->width + this->height) * pixel_width : thickness;
      for (int i = 0; i < kVerticesPerQuad; ++i) {
        vertices[i].thickness = thick;
        vertices[i].fade = pixel_width;
      }

      setCornerCoordinates(vertices);
    }

    float thickness = kFullThickness;
    float pixel_width = 1.0f;
  };

  struct Fill : Primitive<> {
    GMPI_GPU_CREATE_BATCH_ID
    static const EmbeddedFile& vertexShader();
    static const EmbeddedFile& fragmentShader();

    Fill(const ClampBounds& clamp, const PackedBrush* brush, float x, float y, float width,
         float height) : Primitive(batchId(), clamp, brush, x, y, width, height) { }

    void setVertexData(Vertex* vertices) const { setPrimitiveData(vertices); }
  };

  struct Rectangle : Primitive<> {
    GMPI_GPU_CREATE_BATCH_ID
    static const EmbeddedFile& vertexShader();
    static const EmbeddedFile& fragmentShader();

    Rectangle(const ClampBounds& clamp, const PackedBrush* brush, float x, float y, float width,
              float height) : Primitive(batchId(), clamp, brush, x, y, width, height) { }

    void setVertexData(Vertex* vertices) const { setPrimitiveData(vertices); }
  };

  struct Circle : Primitive<> {
    GMPI_GPU_CREATE_BATCH_ID
    static const EmbeddedFile& vertexShader();
    static const EmbeddedFile& fragmentShader();

    Circle(const ClampBounds& clamp, const PackedBrush* brush, float x, float y, float width) :
        Primitive(batchId(), clamp, brush, x, y, width, width) { }

    void setVertexData(Vertex* vertices) const { setPrimitiveData(vertices); }
  };

  struct ImageWrapper : Shape<TextureVertex> {
    static const EmbeddedFile& vertexShader();
    static const EmbeddedFile& fragmentShader();

    ImageWrapper(const ClampBounds& clamp, const PackedBrush* brush, float x, float y, float width,
                 float height, const Image& image, ImageAtlas* image_atlas) :
        Shape(image_atlas, clamp, brush, x, y, width, height),
        packed_image(image_atlas->addImage(image)), image_atlas(image_atlas) {
      if (width == 0.0f) {
        this->width = packed_image.w();
        this->height = packed_image.h();
      }
      source_width = packed_image.w();
      source_height = packed_image.h();
    }

    // Draws only `source_width` x `source_height` texels of the image from
    // (source_x, source_y), stretched into the quad.
    ImageWrapper(const ClampBounds& clamp, const PackedBrush* brush, float x, float y, float width,
                 float height, const Image& image, ImageAtlas* image_atlas, float source_x,
                 float source_y, float source_width, float source_height) :
        ImageWrapper(clamp, brush, x, y, width, height, image, image_atlas) {
      this->source_x = source_x;
      this->source_y = source_y;
      this->source_width = source_width;
      this->source_height = source_height;
    }

    void setVertexData(Vertex* vertices) const {
      image_atlas->setImageCoordinates(vertices, packed_image, source_x, source_y, source_width,
                                       source_height);
      // Texels per pixel, per axis: the vertex shader moves the texture
      // coordinate by this much for every pixel the clamp moves a corner.
      // (setQuadPositions put the quad size here; this shader reads it as
      // the scale, and text quads carry 1.)
      float scale_x = width > 0.0f ? source_width / width : 1.0f;
      float scale_y = height > 0.0f ? source_height / height : 1.0f;
      for (int i = 0; i < kVerticesPerQuad; ++i) {
        vertices[i].dimension_x = scale_x;
        vertices[i].dimension_y = scale_y;
      }
    }

    ImageAtlas::PackedImage packed_image;
    ImageAtlas* image_atlas = nullptr;
    float source_x = 0.0f;
    float source_y = 0.0f;
    float source_width = 0.0f;
    float source_height = 0.0f;
  };

  struct PathFillWrapper : Shape<TextureVertex> {
    GMPI_GPU_CREATE_BATCH_ID
    static constexpr int kLineVerticesPerPoint = 6;
    static constexpr float kBuffer = 1.0f;
    static const EmbeddedFile& vertexShader();
    static const EmbeddedFile& fragmentShader();

    PathFillWrapper(const ClampBounds& clamp, const PackedBrush* brush, float x, float y,
                    float width, float height, const Path& path, PathAtlas* atlas, float scale) :
        Shape(batchId(), clamp, brush, x, y, width, height), path_atlas(atlas), scale(scale) {
      Path adjusted_path = scale == 1.0f ? path : path.scaled(scale);
      Bounds bounding_box = adjusted_path.boundingBox();
      float new_x = static_cast<int>(x + bounding_box.x() - kBuffer);
      float new_y = static_cast<int>(y + bounding_box.y() - kBuffer);
      float new_width = std::ceil(x + bounding_box.right() + kBuffer) - new_x;
      float new_height = std::ceil(y + bounding_box.bottom() + kBuffer) - new_y;

      float shift_x = new_x - x;
      float shift_y = new_y - y;
      this->x = new_x;
      this->y = new_y;
      this->width = new_width;
      this->height = new_height;
      adjusted_path = adjusted_path.translated(-shift_x, -shift_y);
      packed_path = atlas->addPath(std::move(adjusted_path), new_width, new_height);
    }

    void setVertexData(Vertex* vertices) const {
      path_atlas->setPathAtlasCoordinates(vertices, packed_path);
      float even_odd = packed_path.path().fillRule() == Path::FillRule::EvenOdd ? 1.0f : 0.0f;
      for (int i = 0; i < kVerticesPerQuad; ++i)
        vertices[i].direction_x = even_odd;
    }

    PathAtlas* path_atlas = nullptr;
    float scale = 1.0f;
    PathAtlas::PackedPath packed_path;
  };

  template<typename T>
  class VectorPool {
  public:
    static VectorPool& instance() {
      static VectorPool instance;
      return instance;
    }

    std::vector<T> vector(int size) {
      std::vector<T> vector = removeVector(size);
      vector.resize(size);
      return vector;
    }

    void returnVector(std::vector<T>&& vector) {
      if (vector.capacity() == 0)
        return;

      vector.clear();
      auto pos = std::lower_bound(pool_.begin(), pool_.end(), vector,
                                  [](const std::vector<T>& vector, const std::vector<T>& insert) {
                                    return vector.capacity() < insert.capacity();
                                  });
      pool_.insert(pos, std::move(vector));
    }

  private:
    std::vector<T> removeVector(int minimum_capacity) {
      if (!pool_.empty()) {
        auto it = std::lower_bound(pool_.begin(), pool_.end(), minimum_capacity,
                                   [](const std::vector<T>& vector, int capacity) {
                                     return vector.capacity() < capacity;
                                   });
        if (it == pool_.end()) {
          std::vector<T> vector = std::move(pool_.back());
          pool_.pop_back();
          return vector;
        }

        std::vector<T> vector = std::move(*it);
        pool_.erase(it);
        return vector;
      }

      return {};
    }

    std::vector<std::vector<T>> pool_;
  };

  struct ShaderWrapper : Shape<> {
    ShaderWrapper(const ClampBounds& clamp, const PackedBrush* brush, float x, float y, float width,
                  float height, Shader* shader) :
        Shape(shader, clamp, brush, x, y, width, height), shader(shader) { }

    static void setVertexData(Vertex* vertices) { setCornerCoordinates(vertices); }

    Shader* shader = nullptr;
  };

  struct SampleRegion : Shape<PostEffectVertex> {
    static const EmbeddedFile& vertexShader();
    static const EmbeddedFile& fragmentShader();

    SampleRegion(const ClampBounds& clamp, const PackedBrush* brush, float x, float y, float width,
                 float height, const Region* region, PostEffect* post_effect = nullptr);

    void setVertexData(Vertex* vertices) const;

    const Region* region = nullptr;
    PostEffect* post_effect = nullptr;
  };
}