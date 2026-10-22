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

#include "shapes.h"

#include "shaders/compiled/shaders.h"
#include "layer.h"
#include "region.h"

#define GMPI_GPU_SET_PROGRAM(shape, vertex, fragment) \
  const EmbeddedFile& shape::vertexShader() {       \
    return vertex;                                  \
  }                                                 \
  const EmbeddedFile& shape::fragmentShader() {     \
    return fragment;                                \
  }

namespace gmpi::gpu {
  GMPI_GPU_SET_PROGRAM(Fill, shaders::vs_color, shaders::fs_color)
  GMPI_GPU_SET_PROGRAM(Rectangle, shaders::vs_shape, shaders::fs_rectangle)
  GMPI_GPU_SET_PROGRAM(Circle, shaders::vs_shape, shaders::fs_circle)
  GMPI_GPU_SET_PROGRAM(ImageWrapper, shaders::vs_tinted_texture, shaders::fs_tinted_texture)
  GMPI_GPU_SET_PROGRAM(PathFillWrapper, shaders::vs_sample_path, shaders::fs_sample_path)
  GMPI_GPU_SET_PROGRAM(SampleRegion, shaders::vs_post_effect, shaders::fs_post_effect)

  SampleRegion::SampleRegion(const ClampBounds& clamp, const PackedBrush* brush, float x, float y,
                             float width, float height, const Region* region, PostEffect* post_effect) :
      Shape(region->layer(), clamp, brush, x, y, width, height), region(region),
      post_effect(post_effect) {
    if (post_effect)
      batch_id = post_effect;
  }

  void SampleRegion::setVertexData(Vertex* vertices) const {
    region->layer()->setTexturePositionsForRegion(region, vertices);
  }
}
