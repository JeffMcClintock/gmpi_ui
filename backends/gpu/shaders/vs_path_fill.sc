$input a_position, a_texcoord0
$output v_shader_values, v_shader_values1

// One quad per path edge: the columns from the edge's leftmost pixel to the
// right edge of the path's atlas rect, over the rows the edge spans. The
// fragment shader adds, for each pixel, the area of the pixel lying to the
// right of the edge (see fs_path_fill). Everything is in coordinates local
// to the path's rect, so a path far into a large atlas keeps its precision.
//
// a_position = (corner index 0..3, rect origin y, edge x0, edge y0)
// a_texcoord0 = (edge x1, edge y1, rect origin x, rect right)

uniform vec4 u_bounds;

void main() {
  int index = int(a_position.x);
  vec2 origin = vec2(a_texcoord0.z, a_position.y);
  vec2 p0 = a_position.zw - origin;
  vec2 p1 = a_texcoord0.xy - origin;
  float right = a_texcoord0.w - origin.x;

  float x0 = floor(min(p0.x, p1.x));
  float y0 = floor(min(p0.y, p1.y));
  float y1 = ceil(max(p0.y, p1.y));

  vec2 position = vec2(x0, y0);
  if (index == 1)
    position = vec2(right, y0);
  else if (index == 2)
    position = vec2(right, y1);
  else if (index == 3)
    position = vec2(x0, y1);

  v_shader_values = vec4(p0, p1);
  v_shader_values1 = vec4(position, 0.0, 0.0);
  gl_Position = vec4((position + origin) * u_bounds.xy + u_bounds.zw, 0.5, 1.0);
}
