$input a_position, a_color0, a_color1, a_color2, a_texcoord0, a_texcoord1, a_texcoord2, a_texcoord3
$output v_coordinates, v_position, v_gradient_pos, v_gradient_pos2, v_gradient_texture_pos, v_shader_values, v_shader_values1

#include <shader_include.sh>

uniform vec4 u_bounds;
uniform vec4 u_atlas_scale;

void main() {
  vec2 min = a_texcoord1.xy;
  vec2 max = a_texcoord1.zw;
  vec2 clamped = clamp(a_position.xy, min, max);
  vec2 delta = clamped - a_position.xy;

  v_position = clamped;
  v_gradient_texture_pos = a_color0;
  v_gradient_pos = a_color1;
  v_gradient_pos2 = a_color2;
  // A corner the clamp moved samples the texture further along by the
  // quad's texels-per-pixel (a_position.zw: 1 for text, the source/dest
  // ratio for a stretched image).
  vec2 rotated_delta = a_texcoord0.z * delta + a_texcoord0.w * delta.yx;
  v_coordinates = (a_texcoord0.xy + rotated_delta * a_position.zw) * u_atlas_scale.xy;
  v_shader_values = a_texcoord2;   // source rect, atlas texels
  v_shader_values1 = a_texcoord3;  // x: nearest
  vec2 adjusted_position = clamped * u_bounds.xy + u_bounds.zw;
  gl_Position = vec4(adjusted_position, 0.5, 1.0);
}
