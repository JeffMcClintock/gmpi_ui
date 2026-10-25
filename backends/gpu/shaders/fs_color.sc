$input v_position, v_gradient_pos, v_gradient_pos2, v_gradient_texture_pos

#include <shader_include.sh>
#include <clip_mask.sh>

SAMPLER2D(s_gradient, 0);

void main() {
  gl_FragColor = applyClip(gradient(s_gradient, v_gradient_texture_pos, v_gradient_pos, v_gradient_pos2, v_position), gl_FragCoord.xy);
}
