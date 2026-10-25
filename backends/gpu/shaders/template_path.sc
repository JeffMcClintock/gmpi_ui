$input v_coordinates, v_position, v_dimensions, v_gradient_pos, v_gradient_pos2, v_gradient_texture_pos

// Shader brush, path stage: fs_sample_path with the user's shade(p) in place
// of gradient(). Coverage comes from the path atlas exactly as for a brush
// fill; only the colour differs. Vertex shader: vs_sample_path, whose $output
// list the $input list above matches exactly (see template_quad.sc).
// See template_quad.sc for the contract shade() meets.

#include <shader_include.sh>
#include <clip_mask.sh>

SAMPLER2D(s_gradient, 0);
SAMPLER2D(s_texture, 1);

uniform vec4 u_time;
uniform vec4 u_params[8];
uniform vec4 u_brush_transform[2];

vec2 gmpiBrushPosition(vec2 device) {
  return vec2(dot(u_brush_transform[0].xy, device) + u_brush_transform[0].z,
              dot(u_brush_transform[1].xy, device) + u_brush_transform[1].z);
}

#include <user_shader.sh>

void main() {
  gl_FragColor = shade(gmpiBrushPosition(v_position)); // see template_quad.sc on u_color_mult
  gl_FragColor.rgb = gl_FragColor.rgb * u_brush_transform[0].w;
  float coverage = abs(texture2D(s_texture, v_coordinates).r);
  float t = mod(coverage, 2.0);
  float alpha = v_dimensions.x * (1.0 - abs(t - 1.0)) + (1.0 - v_dimensions.x) * clamp(coverage, 0.0, 1.0);
  gl_FragColor.a = gl_FragColor.a * alpha;
  gl_FragColor = applyClip(gl_FragColor, gl_FragCoord.xy);
}
