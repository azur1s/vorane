#version 330 core

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
uniform float uRadius;
uniform vec2 uTexelSize; // 1.0 / texture size

float gauss(float x, float sigma) {
  return exp(-0.5 * (x * x) / (sigma * sigma));
}

void main() {
  float radius = clamp(uRadius, 0.0, 50.0);
  int   R      = int(floor(radius));
  float sigma  = max(radius * 0.5, 1.0);

  vec4 sum = vec4(0.0);
  float wsum = 0.0;

  // center sample
  float w0 = gauss(0.0, sigma);
  sum += texture(uTex, vUV) * w0;
  wsum += w0;

  // symmetric taps
  for (int i = 1; i <= R; ++i) {
    float w = gauss(float(i), sigma);
    vec2 off = vec2(float(i), 0.0) * uTexelSize;
    sum  += texture(uTex, vUV + off) * w;
    sum  += texture(uTex, vUV - off) * w;
    wsum += 2.0 * w;
  }

  fragColor = sum / wsum;
}