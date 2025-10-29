#version 330 core
in vec2 vUV; // expect [0,1] range
out vec4 FragColor;
uniform sampler2D uTex;
uniform mat3 uXform; // maps dst UV -> src UV in homogeneous coords
vec4 sampleBilinear(sampler2D tex, vec2 uv){
  vec2 texSize = textureSize(tex, 0);
  vec2 coord = uv * texSize - 0.5;
  vec2 i = floor(coord);
  vec2 f = fract(coord);
  vec4 a = texelFetch(tex, ivec2(i), 0);
  vec4 b = texelFetch(tex, ivec2(i) + ivec2(1,0), 0);
  vec4 c = texelFetch(tex, ivec2(i) + ivec2(0,1), 0);
  vec4 d = texelFetch(tex, ivec2(i) + ivec2(1,1), 0);
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
void main() {
  vec3 src = uXform * vec3(vUV, 1.0);
  vec2 suv = src.xy / src.z;
  if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) {
    FragColor = vec4(0.0);
  } else {
    FragColor = sampleBilinear(uTex, suv);
  }
}