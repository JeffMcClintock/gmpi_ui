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

#include "path.h"

#include "shaders/compiled/shaders.h"
#include "graphics_caches.h"
#include "shape_batcher.h"
#include "uniforms.h"

#include "device/device.h"
#include <memory>

namespace gmpi::gpu {
  struct PathAtlasTexture {
    gfx::FrameBufferHandle handle = { gfx::kInvalidHandle };

    ~PathAtlasTexture() {
      if (gfx::isValid(handle))
        gfx::destroy(handle);
    }
  };

  PathAtlas::PackedPathReference::~PackedPathReference() {
    if (auto atlas_pointer = atlas.lock())
      (*atlas_pointer)->removePath(packed_path_rect);
  }

  PathAtlas::PathAtlas() {
    reference_ = std::make_shared<PathAtlas*>(this);
    atlas_map_.setPadding(kBuffer);
  }

  PathAtlas::~PathAtlas() = default;

  template<const char* name>
  void setPathUniform(float value0, float value1 = 0.0f, float value2 = 0.0f, float value3 = 0.0f) {
    float values[4] = { value0, value1, value2, value3 };
    static const gfx::UniformHandle uniform = gfx::createUniform(name, gfx::UniformType::Vec4, 1);
    gfx::setUniform(uniform, values);
  }

  bool PathAtlas::clearUpdatedPathAreas(int submit_pass) {
    int total_need_update = 0;
    for (auto& path : paths_) {
      if (path->needs_update)
        ++total_need_update;
    }

    if (total_need_update == 0)
      return false;

    float width_scale = 2.0f / width_;
    float height_scale = 2.0f / height_;

    gfx::setViewMode(submit_pass, gfx::ViewMode::Sequential);
    gfx::setViewRect(submit_pass, 0, 0, width_, height_);
    // Only each path's own rect is ever cleared, but the sample quad has a
    // one-pixel buffer and LINEAR filtering reads the texels around it. A
    // new atlas holds whatever its memory held before; in R16F that can be
    // NaN, which fs_sample_path turns into NaN alpha and the blend spreads.
    // Clear the whole atlas once when it is created.
    gfx::setViewClear(submit_pass, fresh_frame_buffer_ ? BGFX_CLEAR_COLOR : BGFX_CLEAR_NONE, 0x00000000);
    fresh_frame_buffer_ = false;
    gfx::setViewFrameBuffer(submit_pass, frame_buffer_->handle);
    gfx::setState(BGFX_STATE_WRITE_R | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ZERO));
    auto clear_vertices = initQuadVertices<UvVertex>(total_need_update);
    int vertex_index = 0;
    for (auto& path : paths_) {
      if (path->needs_update) {
        float left = path->x * width_scale - 1.0f;
        float top = 1.0f - path->y * height_scale;
        float right = left + path->w * width_scale;
        float bottom = top - path->h * height_scale;
        clear_vertices[vertex_index].x = left;
        clear_vertices[vertex_index].y = top;
        clear_vertices[vertex_index + 1].x = right;
        clear_vertices[vertex_index + 1].y = top;
        clear_vertices[vertex_index + 2].x = left;
        clear_vertices[vertex_index + 2].y = bottom;
        clear_vertices[vertex_index + 3].x = right;
        clear_vertices[vertex_index + 3].y = bottom;
        clear_vertices[vertex_index].u = 1.0f;
        clear_vertices[vertex_index].v = 0.0f;
        clear_vertices[vertex_index + 1].u = 1.0f;
        clear_vertices[vertex_index + 1].v = 0.0f;
        clear_vertices[vertex_index + 2].u = 0.0f;
        clear_vertices[vertex_index + 2].v = 0.0f;
        clear_vertices[vertex_index + 3].u = 0.0f;
        clear_vertices[vertex_index + 3].v = 0.0f;
        vertex_index += 4;
      }
    }

    if (gfx::getCaps()->originBottomLeft) {
      for (int i = 0; i < vertex_index; ++i)
        clear_vertices[i].y = -clear_vertices[i].y;
    }

    setPathUniform<Uniforms::kColor>(0.0f);
    gfx::submit(submit_pass, ProgramCache::programHandle(shaders::vs_clear, shaders::fs_clear));

    return true;
  }

  int PathAtlas::updatePaths(int submit_pass) {
    constexpr int kQuadIndices[] = { 0, 1, 2, 0, 2, 3 };
    constexpr int kVerticesPerEdge = 4;
    constexpr int kIndicesPerEdge = sizeof(kQuadIndices) / sizeof(int);

    checkInit();

    if (!clearUpdatedPathAreas(submit_pass))
      return submit_pass;

    int num_edges = 0;
    for (auto& path : paths_) {
      if (path->needs_update) {
        for (const auto& sub_path : path->path.subPaths()) {
          if (sub_path.points.size() > 2)
            num_edges += sub_path.points.size();
        }
      }
    }

    gfx::setState(BGFX_STATE_WRITE_R | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE));
    gfx::TransientVertexBuffer vertex_buffer {};
    gfx::TransientIndexBuffer index_buffer {};
    int num_vertices = num_edges * kVerticesPerEdge;
    int num_indices = num_edges * kIndicesPerEdge;
    if (!gfx::allocTransientBuffers(&vertex_buffer, PathVertex::layout(), num_vertices,
                                     &index_buffer, num_indices, true)) {
      GMPI_GPU_LOG("PathAtlas::updatePaths: Failed to allocate transient buffers");
      return submit_pass + 1;
    }

    auto vertices = reinterpret_cast<PathVertex*>(vertex_buffer.data);
    auto indices = reinterpret_cast<uint32_t*>(index_buffer.data);
    if (indices == nullptr || vertices == nullptr) {
      GMPI_GPU_LOG("PathAtlas::updatePaths: Failed to map transient buffers");
      return submit_pass + 1;
    }

    gfx::setVertexBuffer(0, &vertex_buffer);
    gfx::setIndexBuffer(&index_buffer);

    // Coverage by signed-area accumulation, one quad per edge, as font
    // rasterisers do it: each edge adds to every pixel in the rows it spans
    // the area of that pixel lying to its right, signed by its direction.
    // Summed over a closed sub-path that is the pixel's winding-weighted
    // coverage. A pixel entirely right of an edge gets exactly the edge's
    // overlap with its row, and the overlaps of the edges meeting at a
    // vertex sum to exactly one row in fp32, so the exterior comes out
    // exactly zero. The fan of triangles this replaced swept exterior pixels
    // with dozens of slivers whose individual area errors (~1e-4 each in
    // fp32) accumulated to visible lines radiating from the fan's anchor.
    uint32_t vertex = 0;
    uint32_t index = 0;
    for (auto& path : paths_) {
      if (!path->needs_update)
        continue;
      path->needs_update = false;

      const float origin_x = static_cast<float>(path->x);
      const float origin_y = static_cast<float>(path->y);
      const float right = origin_x + static_cast<float>(path->w);

      for (const auto& sub_path : path->path.subPaths()) {
        const int num_points = static_cast<int>(sub_path.points.size());
        if (num_points <= 2)
          continue;

        for (int i = 0; i < num_points; ++i) {
          const auto& p0 = sub_path.points[i];
          const auto& p1 = sub_path.points[(i + 1) % num_points];

          for (int k = 0; k < kIndicesPerEdge; ++k)
            indices[index++] = vertex + kQuadIndices[k];

          for (int v = 0; v < kVerticesPerEdge; ++v) {
            vertices[vertex].index = static_cast<float>(v);
            vertices[vertex].origin_y = origin_y;
            vertices[vertex].x1 = origin_x + p0.x;
            vertices[vertex].y1 = origin_y + p0.y;
            vertices[vertex].x2 = origin_x + p1.x;
            vertices[vertex].y2 = origin_y + p1.y;
            vertices[vertex].origin_x = origin_x;
            vertices[vertex].right = right;
            ++vertex;
          }
        }
      }
    }

    GMPI_GPU_ASSERT(vertex == num_vertices);
    GMPI_GPU_ASSERT(index == num_indices);

    bool origin_flip = gfx::getCaps()->originBottomLeft;
    setPathUniform<Uniforms::kColor>(1.0f);
    setPathUniform<Uniforms::kOriginFlip>(origin_flip ? -1.0 : 1.0);

    if (origin_flip)
      setPathUniform<Uniforms::kBounds>(2.0f / width_, 2.0f / height_, -1.0f, -1.0f);
    else
      setPathUniform<Uniforms::kBounds>(2.0f / width_, -2.0f / height_, -1.0f, 1.0f);

    gfx::submit(submit_pass, ProgramCache::programHandle(shaders::vs_path_fill, shaders::fs_path_fill));
    return submit_pass + 1;
  }

  void PathAtlas::destroy() {
    frame_buffer_.reset();
  }

  // Coverage is accumulated additively, one signed area per fan triangle. A
  // fan from a far anchor sweeps a pixel with many thin slivers, so the
  // running sum takes many fractional steps before it lands back on an exact
  // 0 or 1, and in R16F each step rounds at 1/2048: the exterior of a
  // 400-point graph fill was left speckled with residues of ~1e-3, which an
  // sRGB display turns into visible fan lines from the anchor. fp32 blending
  // (every desktop GPU, lavapipe too) leaves ~1e-7, invisible.
  static gfx::TextureFormat::Enum coverageFormat() {
    static constexpr uint32_t kNeeded = BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER |
                                        BGFX_CAPS_FORMAT_TEXTURE_2D;
    uint32_t caps = gfx::getCaps()->formats[gfx::TextureFormat::R32F];
    if ((caps & kNeeded) == kNeeded)
      return gfx::TextureFormat::R32F;
    return gfx::TextureFormat::R16F;
  }

  void PathAtlas::checkInit() {
    constexpr uint64_t kFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    if (needs_packing_) {
      resize();
      needs_packing_ = false;
    }

    if (frame_buffer_ == nullptr)
      frame_buffer_ = std::make_unique<PathAtlasTexture>();

    if (!gfx::isValid(frame_buffer_->handle) && atlas_map_.width() && atlas_map_.height()) {
      frame_buffer_->handle = gfx::createFrameBuffer(atlas_map_.width(), atlas_map_.height(),
                                                      coverageFormat(), kFlags);
      width_ = atlas_map_.width();
      height_ = atlas_map_.height();
      fresh_frame_buffer_ = true;
    }
  }

  void PathAtlas::resize() {
    static constexpr float kShrinkFactor = 0.5f;
    atlas_map_.pack(width_, height_);
    if (atlas_map_.width() > width_ || atlas_map_.height() > height_ ||
        atlas_map_.width() < width_ * kShrinkFactor || atlas_map_.height() < height_ * kShrinkFactor)
      frame_buffer_.reset();

    for (auto& path : paths_) {
      const PackedRect& rect = atlas_map_.rectForId(path.get());
      path->x = rect.x;
      path->y = rect.y;
      path->w = rect.w;
      path->h = rect.h;
      path->needs_update = true;
    }
  }

  const gfx::FrameBufferHandle& PathAtlas::frameBufferHandle() {
    return frame_buffer_->handle;
  }

  bool PathAtlas::hasFrameBuffer() const {
    return frame_buffer_ && gfx::isValid(frame_buffer_->handle);
  }
}