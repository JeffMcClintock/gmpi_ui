$input v_coordinates, v_dimensions, v_shader_values, v_position, v_gradient_texture_pos, v_gradient_pos, v_gradient_pos2

// Shader brush, quad stage: an axis-aligned rectangle filled by the user's
// shade(p). The host owns main(); the module supplies shade() in the file
// compile_shaders.cmake (USER_SHADER mode) makes visible as <user_shader.sh>.
// Vertex shader: vs_shape, so v_position is the device-space pixel. The
// $input list above must be EXACTLY vs_shape's $output list: bgfx hashes the
// varyings of each stage and createProgram refuses a pair whose hashes differ.
//
// shade() returns STRAIGHT linear RGBA (not premultiplied), like gradient();
// the Alpha blend premultiplies into the fp16 layer. u_color_mult is the HDR
// range scale every brush fill applies.

#include <shader_include.sh>
#include <clip_mask.sh>

SAMPLER2D(s_gradient, 0);

uniform vec4 u_time;
uniform vec4 u_params[8];
uniform vec4 u_brush_transform[2];

vec2 gmpiBrushPosition(vec2 device) {
  return vec2(dot(u_brush_transform[0].xy, device) + u_brush_transform[0].z,
              dot(u_brush_transform[1].xy, device) + u_brush_transform[1].z);
}

#include <user_shader.sh>

void main() {
  // Not u_color_mult: that undoes the gradient atlas's 1/64 storage scale,
  // which a shader's colour never had. u_brush_transform[0].w carries the
  // only scale a user colour needs, the HDR range multiplier (1 on fp16).
  gl_FragColor = shade(gmpiBrushPosition(v_position));
  gl_FragColor.rgb = gl_FragColor.rgb * u_brush_transform[0].w;
  gl_FragColor = applyClip(gl_FragColor, gl_FragCoord.xy);
}
