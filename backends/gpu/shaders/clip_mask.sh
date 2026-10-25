// The current shaped clip, as coverage sampled from the path atlas
// (clip_mask.h). Included by every fragment shader that draws a shape;
// applyClip() is the last thing each does to gl_FragColor.
//
//   u_clip_mask.x  1 when a clip applies to this batch
//   u_clip_mask.y  1 = even-odd fill rule, 0 = non-zero
//   u_clip_mask.z  1 when gl_FragColor is premultiplied (scale rgb as well as a)
//   u_clip_map.xy  1 / atlas size
//   u_clip_map.zw  atlas texel minus frame-buffer pixel
//
// Call as applyClip(color, gl_FragCoord.xy): bgfx's shader front end only
// exposes gl_FragCoord inside main(). It is the frame-buffer pixel centre,
// top-left origin on all three renderers (Vulkan, Direct3D 12, Metal), so
// the lookup lands on the texel centre the atlas rasterised for that pixel.

uniform vec4 u_clip_mask;
uniform vec4 u_clip_map;
SAMPLER2D(s_clip, 3);

float clipCoverage(vec2 frag_coord) {
  vec2 uv = (frag_coord + u_clip_map.zw) * u_clip_map.xy;
  float coverage = abs(texture2D(s_clip, uv).r);
  float t = mod(coverage, 2.0);
  return u_clip_mask.y * (1.0 - abs(t - 1.0)) + (1.0 - u_clip_mask.y) * clamp(coverage, 0.0, 1.0);
}

vec4 applyClip(vec4 color, vec2 frag_coord) {
  if (u_clip_mask.x == 0.0)
    return color;
  float coverage = clipCoverage(frag_coord);
  float rgb_scale = mix(1.0, coverage, u_clip_mask.z);
  return vec4(color.rgb * rgb_scale, color.a * coverage);
}
