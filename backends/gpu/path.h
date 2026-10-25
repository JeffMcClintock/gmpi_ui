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
#include "space.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <vector>

namespace gmpi::gpu {
  // A polygon set for the coverage atlas: sub-paths of points, a fill rule.
  // The GMPI adaptor hands it figures that are already flattened and
  // transformed (Gfx_base / cpugfx), and strokes already widened by cpugfx's
  // D2D-exact stroker, so curves, strokes and SVG parsing are not here.
  class Path {
  public:
    struct SubPath {
      std::vector<Point> points;
      bool closed = false;
    };

    enum class FillRule {
      NonZero,
      Positive,
      EvenOdd
    };

    void moveTo(Point point) {
      if (!paths_.empty() && !paths_.back().points.empty())
        startNewPath();
      last_point_ = point;
    }

    void moveTo(float x, float y) { moveTo(Point(x, y)); }

    void lineTo(Point point) {
      if (currentPath().points.empty())
        addPoint(last_point_);
      addPoint(point);
    }

    void lineTo(float x, float y) { lineTo(Point(x, y)); }

    void close() {
      static constexpr float kCloseEpsilon = 0.000001f;

      if (paths_.empty() || paths_.back().points.empty())
        return;

      if ((paths_.back().points.front() - paths_.back().points.back()).squareMagnitude() < kCloseEpsilon) {
        paths_.back().points.back() = paths_.back().points.front();
        last_point_ = paths_.back().points.front();
      }
      else if (paths_.back().points.front() != paths_.back().points.back())
        addPoint(paths_.back().points.front());

      currentPath().closed = true;
    }

    int numPoints() const {
      int total = 0;
      for (const auto& path : paths_)
        total += path.points.size();
      return total;
    }

    const std::vector<SubPath>& subPaths() const { return paths_; }

    void clear() {
      paths_.clear();
      last_point_ = {};
    }

    Path scaled(float mult) const {
      Path result = *this;
      for (auto& path : result.paths_) {
        for (Point& point : path.points)
          point *= mult;
      }
      return result;
    }

    Path translated(float x, float y) const {
      Path result = *this;
      const Point offset(x, y);
      for (auto& path : result.paths_) {
        for (Point& point : path.points)
          point += offset;
      }
      return result;
    }

    void setFillRule(FillRule fill_rule) { fill_rule_ = fill_rule; }
    FillRule fillRule() const { return fill_rule_; }

    Bounds boundingBox() const {
      float min_x = (std::numeric_limits<float>::max)();
      float min_y = (std::numeric_limits<float>::max)();
      float max_x = std::numeric_limits<float>::lowest();
      float max_y = std::numeric_limits<float>::lowest();
      for (const auto& path : paths_) {
        for (const auto& point : path.points) {
          min_x = (std::min)(min_x, point.x);
          min_y = (std::min)(min_y, point.y);
          max_x = (std::max)(max_x, point.x);
          max_y = (std::max)(max_y, point.y);
        }
      }
      if (min_x > max_x || min_y > max_y)
        return { 0, 0, 0, 0 };
      return { min_x, min_y, max_x - min_x, max_y - min_y };
    }

  private:
    void startNewPath() {
      if (paths_.empty() || !paths_.back().points.empty())
        paths_.emplace_back();
    }

    SubPath& currentPath() {
      if (paths_.empty() || paths_.back().closed)
        paths_.emplace_back();
      return paths_.back();
    }

    void addPoint(const Point& point) {
      if (!currentPath().points.empty() && point == currentPath().points.back())
        return;
      last_point_ = point;
      currentPath().points.push_back(point);
    }

    std::vector<SubPath> paths_;
    FillRule fill_rule_ = FillRule::EvenOdd;
    Point last_point_;
  };

  struct PathAtlasTexture;

  class PathAtlas {
  public:
    static constexpr int kBuffer = 1;

    struct PackedPathRect {
      explicit PackedPathRect(const Path& p) : path(p) { }

      Path path;
      int x = 0;
      int y = 0;
      int w = 0;
      int h = 0;
      bool needs_update = true;
    };

    struct PackedPathReference {
      PackedPathReference(std::weak_ptr<PathAtlas*> atlas, const PackedPathRect* packed_path_rect) :
          atlas(std::move(atlas)), packed_path_rect(packed_path_rect) { }
      ~PackedPathReference();

      std::weak_ptr<PathAtlas*> atlas;
      const PackedPathRect* packed_path_rect = nullptr;
    };

    class PackedPath {
    public:
      explicit PackedPath(std::shared_ptr<PackedPathReference> reference) :
          reference_(std::move(reference)) { }

      PackedPath() = default;
      ~PackedPath() = default;

      int x() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_path_rect->x;
      }

      int y() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_path_rect->y;
      }

      int w() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_path_rect->w;
      }

      int h() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_path_rect->h;
      }

      const Path& path() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_path_rect->path;
      }

      const PackedPathRect* packedImageRect() const {
        GMPI_GPU_ASSERT(reference_->atlas.lock().get());
        return reference_->packed_path_rect;
      }

    private:
      std::shared_ptr<PackedPathReference> reference_;
    };

    PathAtlas();
    ~PathAtlas();

    PackedPath addPath(const Path& path, int width, int height) {
      std::unique_ptr<PackedPathRect> packed_path_rect = std::make_unique<PackedPathRect>(path);
      if (!atlas_map_.addRect(packed_path_rect.get(), width, height))
        needs_packing_ = true;

      const PackedRect& rect = atlas_map_.rectForId(packed_path_rect.get());
      packed_path_rect->x = rect.x;
      packed_path_rect->y = rect.y;
      packed_path_rect->w = rect.w;
      packed_path_rect->h = rect.h;
      auto packed_rect = packed_path_rect.get();
      paths_.push_back(std::move(packed_path_rect));

      auto reference = std::make_shared<PackedPathReference>(reference_, packed_rect);
      references_[packed_rect] = reference;
      return PackedPath(reference);
    }

    int updatePaths(int submit_pass);
    void destroy();
    int width() const { return width_; }
    int height() const { return height_; }

    const gfx::FrameBufferHandle& frameBufferHandle();
    bool hasFrameBuffer() const; // false once the atlas overflowed

    void removePath(const PackedPathRect* packed_path_rect) {
      atlas_map_.removeRect(packed_path_rect);
      references_.erase(packed_path_rect);
      paths_.erase(std::remove_if(paths_.begin(), paths_.end(),
                                  [&](const std::unique_ptr<PackedPathRect>& ptr) {
                                    return ptr.get() == packed_path_rect;
                                  }),
                   paths_.end());
    }

    static void setPathAtlasCoordinates(TextureVertex* vertices, const PackedPath& rect) {
      float left = rect.x();
      float top = rect.y();
      float right = left + rect.w();
      float bottom = top + rect.h();

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

  private:
    bool clearUpdatedPathAreas(int submit_pass);
    void checkInit();
    void resize();

    std::map<const PackedPathRect*, std::weak_ptr<PackedPathReference>> references_;
    std::vector<std::unique_ptr<PackedPathRect>> paths_;
    PackedAtlasMap<const PackedPathRect*> atlas_map_;
    std::unique_ptr<PathAtlasTexture> frame_buffer_;
    int width_ = 0;
    int height_ = 0;
    bool needs_packing_ = false;
    bool fresh_frame_buffer_ = false; // created since last use: clear it whole once
    std::shared_ptr<PathAtlas*> reference_;

    GMPI_GPU_LEAK_CHECKER(PathAtlas)
  };
}
