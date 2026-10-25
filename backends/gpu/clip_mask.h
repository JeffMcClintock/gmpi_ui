#pragma once

// A shaped clip as a coverage mask on the GPU. The clip's geometry is an
// ordinary entry in the canvas's path atlas - the same exact per-edge
// coverage a fill gets - and every shape drawn while it is current samples
// that coverage at its own pixel and multiplies it in (shaders/clip_mask.sh).
// Shapes batch by clip (SubmitBatch's identity includes it), so one clip is
// one uniform setting per batch, not per shape.
//
// The adaptor (GpuGfx.h) makes one per pushClipGeometry; the canvas keeps
// every mask used this frame alive until its shapes have been submitted
// (Canvas::clearShapesKeepPixels).

#include "path.h"

namespace gmpi::gpu {
  struct ClipMask {
    PathAtlas* atlas = nullptr;
    PathAtlas::PackedPath packed_path; // in the atlas until this mask is destroyed
    int origin_x = 0;                  // device pixel that atlas texel (packed x, packed y) covers
    int origin_y = 0;
    bool even_odd = false;
  };
}
