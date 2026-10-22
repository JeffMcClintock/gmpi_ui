$input v_shader_values, v_shader_values1

#include <shader_include.sh>

// Signed-area accumulation (the font rasteriser scheme). For the edge
// p0 -> p1 and this pixel's square, output the area of the square lying to
// the right of the edge within the rows the edge spans, signed by the edge's
// direction. Summed over every edge of a sub-path (additive blend into the
// atlas) that is the pixel's winding-weighted coverage.
//
// Exactness matters more than speed here: a pixel entirely to the right of
// the edge gets exactly its row overlap with the edge, and the overlaps of
// the two edges that meet at a vertex sum to exactly one row in fp32, so
// pixels outside the path accumulate to exactly zero. Only pixels the edge
// crosses carry any rounding, and their coverage is fractional anyway.

// Antiderivative of clamp(g, 0, 1).
float clampIntegral(float g) {
  if (g <= 0.0)
    return 0.0;
  if (g >= 1.0)
    return g - 0.5;
  return 0.5 * g * g;
}

void main() {
  vec2 p0 = v_shader_values.xy;
  vec2 p1 = v_shader_values.zw;
  vec2 corner = floor(v_shader_values1.xy);

  float dy = p1.y - p0.y;
  if (dy == 0.0) {
    gl_FragColor.r = 0.0;
    return;
  }

  // The part of the edge inside this pixel's row.
  float ya = max(min(p0.y, p1.y), corner.y);
  float yb = min(max(p0.y, p1.y), corner.y + 1.0);
  if (yb <= ya) {
    gl_FragColor.r = 0.0;
    return;
  }
  float rows = yb - ya;

  // g(y): how far the pixel's right edge is to the right of the edge at
  // height y, clamped to the pixel's width. Linear in y.
  float slope = (p1.x - p0.x) / dy;
  float ga = corner.x + 1.0 - (p0.x + (ya - p0.y) * slope);
  float gb = corner.x + 1.0 - (p0.x + (yb - p0.y) * slope);

  float area;
  if (min(ga, gb) >= 1.0)
    area = rows;  // pixel entirely right of the edge: exact
  else if (max(ga, gb) <= 0.0)
    area = 0.0;   // entirely left
  else {
    float s = gb - ga;
    if (abs(s) < 0.0001)
      area = rows * clamp(0.5 * (ga + gb), 0.0, 1.0);
    else
      area = rows * (clampIntegral(gb) - clampIntegral(ga)) / s;
  }

  gl_FragColor.r = sign(dy) * area;
}
