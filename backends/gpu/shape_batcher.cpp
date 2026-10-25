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

#include "shape_batcher.h"

#include "canvas.h"
#include "clip_mask.h"
#include "shaders/compiled/shaders.h"
#include "graphics_caches.h"
#include "layer.h"
#include "path.h"
#include "region.h"
#include "shader.h"
#include "uniforms.h"
#include "space.h"

#include <algorithm>
#include <cstdlib>

#include "device/device.h"

#include <cstdio>

namespace gmpi::gpu {
  static constexpr uint64_t blendModeValue(BlendMode blend_mode) {
    switch (blend_mode) {
    case BlendMode::Opaque:
      return BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ZERO);
    case BlendMode::Composite:
      return BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
    case BlendMode::Alpha:
      return BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
             BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA,
                                            BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
    case BlendMode::Add:
      return BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
    case BlendMode::Sub:
      return BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE) |
             BGFX_STATE_BLEND_EQUATION_SEPARATE(BGFX_STATE_BLEND_EQUATION_REVSUB,
                                                BGFX_STATE_BLEND_EQUATION_ADD);
    case BlendMode::Mult:
      return BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_MULTIPLY;
    case BlendMode::MaskAdd:
      return BGFX_STATE_WRITE_A |
             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
    case BlendMode::Erase:
      return BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_INV_SRC_ALPHA);
    case BlendMode::PlusPremultiplied:
      return BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE);
    case BlendMode::MaskRemove:
      return BGFX_STATE_WRITE_A |
             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE) |
             BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_REVSUB);
    }

    GMPI_GPU_ASSERT(false);
    return 0;
  }

  void setBlendMode(BlendMode blend_mode) {
    gfx::setState(blendModeValue(blend_mode));
  }

  template<const char* name>
  void setUniform(const void* value) {
    static const gfx::UniformHandle uniform = gfx::createUniform(name, gfx::UniformType::Vec4, 1);
    gfx::setUniform(uniform, value);
  }

  template<const char* name>
  void setUniform(float value0, float value1 = 0.0f, float value2 = 0.0f, float value3 = 0.0f) {
    static const gfx::UniformHandle uniform = gfx::createUniform(name, gfx::UniformType::Vec4, 1);
    float vec[4] = { value0, value1, value2, value3 };
    gfx::setUniform(uniform, vec);
  }

  // A vec4 ARRAY uniform: `count` vec4s from `value`.
  template<const char* name, int count>
  void setUniformArray(const float* value) {
    static const gfx::UniformHandle uniform = gfx::createUniform(name, gfx::UniformType::Vec4, count);
    gfx::setUniform(uniform, value, count);
  }

  // The inputs a shader brush's program reads besides the standard ones.
  void setShaderBrushUniforms(const Shader& shader, bool hdr) {
    setUniformArray<Uniforms::kParams, Shader::kMaxParams>(shader.params());
    // Row 0's spare .w carries the HDR range multiplier: a user colour must
    // not go through u_color_mult, which also undoes the gradient atlas's
    // storage scale (Color::kGradientNormalization).
    float rows[8];
    std::copy(shader.brushTransform(), shader.brushTransform() + 8, rows);
    rows[3] = hdr ? hdrColorMultiplier() : 1.0f;
    setUniformArray<Uniforms::kBrushTransform, 2>(rows);
  }

  template<const char* name>
  void setTexture(int stage, gfx::TextureHandle handle) {
    if (!gfx::isValid(handle) || handle.idx >= 4096)
      fprintf(stderr, "[gmpi::gpu] setTexture stage %d: invalid texture handle %u\n", stage, unsigned(handle.idx));
    static const gfx::UniformHandle uniform = gfx::createUniform(name, gfx::UniformType::Sampler, 1);
    gfx::setTexture(stage, uniform, handle);
  }

  // The current shaped clip (clip_mask.h, shaders/clip_mask.sh), set once per
  // batch: shapes batch by clip. Every shape program declares s_clip, so a
  // texture is bound whether or not a clip applies; with none, a 1x1 stand-in.
  void setClipMaskUniforms(const ClipMask* clip, BlendMode blend) {
    static gfx::TextureHandle stand_in = gfx::createTexture2D(1, 1, false, 1, gfx::TextureFormat::R32F);
    const bool premultiplied = blend == BlendMode::Composite || blend == BlendMode::PlusPremultiplied;
    gfx::TextureHandle texture = stand_in;
    float mask[4] = { 0.0f, 0.0f, premultiplied ? 1.0f : 0.0f, 0.0f };
    float map[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
    if (clip && clip->atlas && clip->atlas->hasFrameBuffer()) {
      texture = gfx::getTexture(clip->atlas->frameBufferHandle());
      mask[0] = 1.0f;
      mask[1] = clip->even_odd ? 1.0f : 0.0f;
      map[0] = 1.0f / clip->atlas->width();
      map[1] = 1.0f / clip->atlas->height();
      map[2] = float(clip->packed_path.x() - clip->origin_x);
      map[3] = float(clip->packed_path.y() - clip->origin_y);
    }
    else if (clip) {
      static bool warned = false;
      if (!warned) {
        warned = true;
        fprintf(stderr, "[gmpi::gpu] clip mask's path atlas has no frame buffer (overflowed?): drawing unclipped\n");
      }
    }
    setUniform<Uniforms::kClipMask>(mask);
    setUniform<Uniforms::kClipMap>(map);
    setTexture<Uniforms::kClip>(3, texture);
  }

  inline void setUniformBounds(int x, int y, int width, int height) {
    float scale_x = 2.0f / width;
    float scale_y = -2.0f / height;
    float view_bounds[4] = { scale_x, scale_y, x * scale_x - 1.0f, y * scale_y + 1.0f };
    setUniform<Uniforms::kBounds>(view_bounds);
  }

  inline void setTimeUniform(float time) {
    float time_values[] = { time, time, time, time };
    setUniform<Uniforms::kTime>(time_values);
  }

  void setUniformDimensions(int width, int height) {
    float view_bounds[4] = { 2.0f / width, -2.0f / height, -1.0f, 1.0f };
    setUniform<Uniforms::kBounds>(view_bounds);
  }

  inline void setColorMult(bool hdr) {
    float value = (hdr ? hdrColorMultiplier() : 1.0f) * Color::kGradientNormalization;
    setUniform<Uniforms::kColorMult>(value, value, value, 1.0f);
  }

  void setOriginFlipUniform(bool origin_flip) {
    setUniform<Uniforms::kOriginFlip>(origin_flip ? -1.0 : 1.0, origin_flip ? 1.0 : 0.0);
  }

  bool initTransientQuadBuffers(int num_quads, const gfx::VertexLayout& layout,
                                gfx::TransientVertexBuffer* vertex_buffer,
                                gfx::TransientIndexBuffer* index_buffer) {
    int num_vertices = num_quads * kVerticesPerQuad;
    int num_indices = num_quads * kIndicesPerQuad;
    if (!gfx::allocTransientBuffers(vertex_buffer, layout, num_vertices, index_buffer, num_indices)) {
      GMPI_GPU_LOG("Not enough transient buffer memory for %d quads", num_quads);
      return false;
    }

    uint16_t* indices = reinterpret_cast<uint16_t*>(index_buffer->data);
    for (int i = 0; i < num_quads; ++i) {
      int vertex_index = i * kVerticesPerQuad;
      int index = i * kIndicesPerQuad;
      for (int v = 0; v < kIndicesPerQuad; ++v)
        indices[index + v] = vertex_index + kQuadTriangles[v];
    }

    return true;
  }

  uint8_t* initQuadVerticesWithLayout(int num_quads, const gfx::VertexLayout& layout) {
    gfx::TransientVertexBuffer vertex_buffer {};
    gfx::TransientIndexBuffer index_buffer {};
    if (!initTransientQuadBuffers(num_quads, layout, &vertex_buffer, &index_buffer))
      return nullptr;

    gfx::setVertexBuffer(0, &vertex_buffer);
    gfx::setIndexBuffer(&index_buffer);
    return vertex_buffer.data;
  }

  void submitShapes(const Layer& layer, const EmbeddedFile& vertex_shader,
                    const EmbeddedFile& fragment_shader, bool radial_gradient, int submit_pass) {
    setTimeUniform(layer.time());
    setUniformDimensions(layer.width(), layer.height());
    setColorMult(layer.hdr());
    setOriginFlipUniform(layer.bottomLeftOrigin());
    GradientAtlas* gradient_atlas = layer.gradientAtlas();
    setUniform<Uniforms::kRadialGradient>(radial_gradient ? 1.0f : 0.0f);
    setTexture<Uniforms::kGradient>(0, gradient_atlas->colorTextureHandle());
    gfx::submit(submit_pass, ProgramCache::programHandle(vertex_shader, fragment_shader));
  }

  void setImageAtlasUniform(ImageAtlas* atlas) {
    if (!gfx::isValid(atlas->textureHandle()))
      fprintf(stderr, "[gmpi::gpu] image atlas texture invalid (atlas %dx%d)\n", atlas->width(), atlas->height());
    setTexture<Uniforms::kTexture>(1, atlas->textureHandle());
    setUniform<Uniforms::kAtlasScale>(1.0f / atlas->width(), 1.0f / atlas->height());
  }

  void setPathAtlasUniform(PathAtlas* atlas) {
    if (!gfx::isValid(atlas->frameBufferHandle()))
      fprintf(stderr, "[gmpi::gpu] path atlas frame buffer invalid (atlas %dx%d)\n", atlas->width(), atlas->height());
    setTexture<Uniforms::kTexture>(1, gfx::getTexture(atlas->frameBufferHandle()));
    setUniform<Uniforms::kAtlasScale>(1.0f / atlas->width(), 1.0f / atlas->height());
  }

  void setImageAtlasUniform(const BatchVector<ImageWrapper>& batches) {
    if (!batches.empty() && !batches[0].shapes->empty())
      setImageAtlasUniform(batches[0].shapes->front().image_atlas);
  }

  void setPathDataUniform(const BatchVector<PathFillWrapper>& batches) {
    if (!batches.empty() && !batches[0].shapes->empty())
      setPathAtlasUniform(batches[0].shapes->front().path_atlas);
  }

  void submitShader(const BatchVector<ShaderWrapper>& batches, const Layer& layer, int submit_pass) {
    auto quads = setupQuads(batches);
    if (quads.vertices == nullptr)
      return;

    setUniform<Uniforms::kRadialGradient>(quads.radial_gradient ? 1.0f : 0.0f);
    setBlendMode(BlendMode::Alpha);
    setTimeUniform(layer.time());
    setUniformDimensions(layer.width(), layer.height());
    setTexture<Uniforms::kGradient>(0, layer.gradientAtlas()->colorTextureHandle());
    setColorMult(layer.hdr());
    setOriginFlipUniform(layer.bottomLeftOrigin());
    Shader* shader = batches[0].shapes->front().shader;
    setShaderBrushUniforms(*shader, layer.hdr());
    auto program = ProgramCache::programHandle(shader->vertexShader(), shader->fragmentShader());
    if (std::getenv("GMPI_GPU_SHADER_DEBUG"))
      fprintf(stderr, "[gmpi::gpu] submitShader: vs valid %d fs valid %d program valid %d, %d quads\n",
              gfx::isValid(ShaderCache::shaderHandle(shader->vertexShader())),
              gfx::isValid(ShaderCache::shaderHandle(shader->fragmentShader())), gfx::isValid(program), quads.num_shapes);
    gfx::submit(submit_pass, program);
  }

  void submitShadedPathFills(const BatchVector<PathFillWrapper>& batches, const Layer& layer, int submit_pass) {
    auto quads = setupQuads(batches);
    if (quads.vertices == nullptr)
      return;

    // What submitShapes(layer, vs, fs, ...) sets for a brush fill, plus the
    // shader brush's own inputs. The path atlas texture was bound by
    // setPathDataUniform.
    setUniform<Uniforms::kRadialGradient>(quads.radial_gradient ? 1.0f : 0.0f);
    setTimeUniform(layer.time());
    setUniformDimensions(layer.width(), layer.height());
    setTexture<Uniforms::kGradient>(0, layer.gradientAtlas()->colorTextureHandle());
    setColorMult(layer.hdr());
    setOriginFlipUniform(layer.bottomLeftOrigin());
    Shader* shader = batches[0].shapes->front().shader;
    setShaderBrushUniforms(*shader, layer.hdr());
    gfx::submit(submit_pass,
                 ProgramCache::programHandle(shader->vertexShader(), shader->fragmentShader()));
  }

  void submitSampleRegions(const BatchVector<SampleRegion>& batches, const Layer& layer, int submit_pass) {
    auto quads = setupQuads(batches);
    if (quads.vertices == nullptr)
      return;

    setUniform<Uniforms::kRadialGradient>(quads.radial_gradient ? 1.0f : 0.0f);

    // A layer's pixels are premultiplied (its shapes blend SRC_ALPHA colour
    // over a transparent clear), so sampling it is a premultiplied over.
    // Alpha blending here multiplied semi-transparent colour by alpha twice:
    // invisible on an opaque window, wrong on a transparent render target.
    setBlendMode(BlendMode::Composite);
    setTimeUniform(layer.time());
    Layer* source_layer = batches[0].shapes->front().region->layer();
    setUniform<Uniforms::kAtlasScale>(1.0f / source_layer->width(), 1.0f / source_layer->height());

    if (!gfx::isValid(source_layer->frameBuffer()))
      fprintf(stderr, "[gmpi::gpu] sampled layer frame buffer invalid (%dx%d)\n", source_layer->width(), source_layer->height());
    setTexture<Uniforms::kTexture>(0, gfx::getTexture(source_layer->frameBuffer()));
    setUniformDimensions(layer.width(), layer.height());
    float value = layer.hdr() ? hdrColorMultiplier() : 1.0f;
    setUniform<Uniforms::kColorMult>(value, value, value, 1.0f);
    setOriginFlipUniform(layer.bottomLeftOrigin());
    gfx::submit(submit_pass, ProgramCache::programHandle(SampleRegion::vertexShader(),
                                                          SampleRegion::fragmentShader()));
  }
}