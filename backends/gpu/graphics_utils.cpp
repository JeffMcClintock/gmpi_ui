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

#include "graphics_utils.h"

#include "device/device.h"
#include <vector>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>


namespace gmpi::gpu {
  struct PackedAtlasData {
    stbrp_context pack_context {};
    std::unique_ptr<stbrp_node[]> pack_nodes;
  };

  AtlasPacker::AtlasPacker() : data_(std::make_unique<PackedAtlasData>()) { }

  AtlasPacker::~AtlasPacker() = default;

  bool AtlasPacker::addRect(PackedRect& rect) {
    if (!packed_)
      return false;

    stbrp_rect r;
    r.w = rect.w + padding_;
    r.h = rect.h + padding_;
    r.id = rect_index_++;

    packed_ = stbrp_pack_rects(&data_->pack_context, &r, 1);
    if (packed_) {
      rect.x = r.x;
      rect.y = r.y;
    }
    return packed_;
  }

  void AtlasPacker::clear() {
    packed_ = false;
  }

  bool AtlasPacker::pack(std::vector<PackedRect>& rects, int width, int height) {
    data_->pack_nodes = std::make_unique<stbrp_node[]>(width);
    std::vector<stbrp_rect> packed_rects;
    rect_index_ = 0;
    for (const PackedRect& rect : rects) {
      stbrp_rect r;
      r.w = rect.w + padding_;
      r.h = rect.h + padding_;
      r.id = rect_index_++;
      packed_rects.push_back(r);
    }

    stbrp_init_target(&data_->pack_context, width, height, data_->pack_nodes.get(), width);
    packed_ = stbrp_pack_rects(&data_->pack_context, packed_rects.data(), packed_rects.size());
    if (packed_) {
      for (int i = 0; i < packed_rects.size(); ++i) {
        rects[i].x = packed_rects[i].x;
        rects[i].y = packed_rects[i].y;
      }
    }
    return packed_;
  }

  gfx::VertexLayout& UvVertex::layout() {
    static gfx::VertexLayout layout;
    static bool initialized = false;

    if (!initialized) {
      initialized = true;
      layout.begin().add(gfx::Attrib::Position, 4, gfx::AttribType::Float).end();
    }

    return layout;
  }

  gfx::VertexLayout& PathVertex::layout() {
    static gfx::VertexLayout layout;
    static bool initialized = false;

    if (!initialized) {
      initialized = true;
      layout.begin()
          .add(gfx::Attrib::Position, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::TexCoord0, 4, gfx::AttribType::Float)
          .end();
    }

    return layout;
  }

  gfx::VertexLayout& ShapeVertex::layout() {
    static gfx::VertexLayout layout;
    static bool initialized = false;

    if (!initialized) {
      initialized = true;
      layout.begin()
          .add(gfx::Attrib::Position, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::Color0, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::Color1, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::Color2, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::TexCoord0, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::TexCoord1, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::TexCoord2, 4, gfx::AttribType::Float)
          .end();
    }

    return layout;
  }

  gfx::VertexLayout& TextureVertex::layout() {
    static gfx::VertexLayout layout;
    static bool initialized = false;

    if (!initialized) {
      initialized = true;
      layout.begin()
          .add(gfx::Attrib::Position, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::Color0, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::Color1, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::Color2, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::TexCoord0, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::TexCoord1, 4, gfx::AttribType::Float)
          .end();
    }

    return layout;
  }

  gfx::VertexLayout& PostEffectVertex::layout() {
    static gfx::VertexLayout layout;
    static bool initialized = false;

    if (!initialized) {
      initialized = true;
      layout.begin()
          .add(gfx::Attrib::Position, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::Color0, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::Color1, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::Color2, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::TexCoord0, 4, gfx::AttribType::Float)
          .add(gfx::Attrib::TexCoord1, 4, gfx::AttribType::Float)
          .end();
    }

    return layout;
  }
}