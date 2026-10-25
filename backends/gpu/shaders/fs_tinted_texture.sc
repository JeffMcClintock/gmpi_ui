$input v_coordinates, v_position, v_gradient_pos, v_gradient_pos2, v_gradient_texture_pos, v_shader_values, v_shader_values1

#include <shader_include.sh>
#include <clip_mask.sh>

SAMPLER2D(s_gradient, 0);
SAMPLER2D(s_texture, 1);

uniform vec4 u_atlas_scale;

void main() {
  vec4 color = gradient(s_gradient, v_gradient_texture_pos, v_gradient_pos, v_gradient_pos2, v_position);
  // cpugfx's sampling rule, in atlas texels: clamp the coordinate to the
  // source rect (a cropped draw never reads past its crop), then either
  // filter or take the texel under it.
  vec2 texel = clamp(v_coordinates / u_atlas_scale.xy, v_shader_values.xy, v_shader_values.zw);
  // Nearest: the texel under the coordinate. Where a pixel centre lands
  // exactly on a texel boundary (a 16 -> 56 stretch does, every other
  // texel) either neighbour is a correct answer, and which one cpugfx picks
  // depends on float rounding in its scale factor, row by row. The
  // interpolated coordinate here tends to come out a hair low; a
  // 1/1024-texel bias agrees with cpugfx on most ties, not all.
  if (v_shader_values1.x > 0.5)
    texel = floor(texel + 1.0 / 1024.0) + 0.5;
  gl_FragColor = applyClip(color * texture2D(s_texture, texel * u_atlas_scale.xy), gl_FragCoord.xy);
}
