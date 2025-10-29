#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uBase;  // previous image in the chain
uniform sampler2D uLayer; // this op's result
uniform int   uMode;      // blend mode enum (see above)
uniform float uOpacity;   // 0..1

// if your layer may extend beyond [0,1], clamp or set transparent outside as you like
#define USE_CLAMP 1

// all math assumes LINEAR color space
// if your textures are sRGB, enable GL_FRAMEBUFFER_SRGB or convert manually
float sat(float x) { return clamp(x, 0.0, 1.0); }
vec3  sat(vec3  x) { return clamp(x, 0.0, 1.0); }

vec3 blend_multiply(vec3 b, vec3 s) { return b * s; }
vec3 blend_screen  (vec3 b, vec3 s) { return 1.0 - (1.0 - b) * (1.0 - s); }
vec3 blend_overlay (vec3 b, vec3 s) {
  return mix(2.0*b*s, 1.0 - 2.0*(1.0-b)*(1.0-s), step(0.5, b));
}
vec3 blend_softlight(vec3 b, vec3 s) {
  // photoshop-like soft light approximation (W3C/SVG version)
  vec3 d = mix(
    (1.0 - (1.0 - b) * (1.0 - 2.0 * s)),
    sqrt(b) * (2.0 * s - 1.0) + 2.0 * b * (1.0 - s),
    step(0.5, s)
);
  return sat(d);
}
vec3 blend_hardlight   (vec3 b, vec3 s) { return blend_overlay(s, b); }
vec3 blend_dodge       (vec3 b, vec3 s) { return sat(b / max(vec3(1e-5), 1.0 - s)); }
vec3 blend_burn        (vec3 b, vec3 s) { return 1.0 - sat((1.0 - b) / max(vec3(1e-5), s)); }
vec3 blend_linear_dodge(vec3 b, vec3 s) { return sat(b + s); }
vec3 blend_linear_burn (vec3 b, vec3 s) { return sat(b + s - 1.0); }
vec3 blend_lighten     (vec3 b, vec3 s) { return max(b, s); }
vec3 blend_darken      (vec3 b, vec3 s) { return min(b, s); }
vec3 blend_difference  (vec3 b, vec3 s) { return abs(b - s); }
vec3 blend_exclusion   (vec3 b, vec3 s) { return b + s - 2.0*b*s; }

vec3 apply_mode(int mode, vec3 base, vec3 src) {
  if (mode ==  1) return blend_multiply(base, src);
  if (mode ==  2) return blend_screen(base, src);
  if (mode ==  3) return blend_overlay(base, src);
  if (mode ==  4) return blend_softlight(base, src);
  if (mode ==  5) return blend_hardlight(base, src);
  if (mode ==  6) return blend_dodge(base, src);
  if (mode ==  7) return blend_burn(base, src);
  if (mode ==  8) return blend_linear_dodge(base, src);
  if (mode ==  9) return blend_linear_burn(base, src);
  if (mode == 10) return blend_lighten(base, src);
  if (mode == 11) return blend_darken(base, src);
  if (mode == 12) return blend_difference(base, src);
  if (mode == 13) return blend_exclusion(base, src);
  // normal
  return src;
}

// porter-duff "over" with optional premultiplied source
vec4 over(vec4 base, vec4 src, bool srcPremul) {
  vec3  Cb = base.rgb;
  vec3  Cs = srcPremul ? src.rgb : src.rgb * src.a;
  float Ab = base.a;
  float As = src.a;
  vec3  Co = Cs + Cb * (1.0 - As);
  float Ao = As + Ab * (1.0 - As);
  // avoid divide by zero when returning straight alpha
  vec3 outRGB = (Ao > 1e-5) ? Co / Ao : vec3(0.0);
  return vec4(outRGB, Ao);
}

// final blend combining a *mode-modified* src with base, with opacity controlling the src contribution
// both base & src are STRAIGHT alpha here; we premultiply internally as needed
vec4 blend_composite(vec4 base, vec4 src, int mode, float opacity) {
  vec3 mixed = apply_mode(mode, base.rgb, src.rgb);
  vec4 srcMode = vec4(mixed, src.a * sat(opacity));
  return over(base, srcMode, false);
}

void main() {
  vec4 base  = texture(uBase,  vUV);
  vec4 layer =
#if USE_CLAMP
    texture(uLayer, clamp(vUV, vec2(0.0), vec2(1.0)));
#else
    texture(uLayer, vUV);
#endif
  fragColor = blend_composite(base, layer, uMode, uOpacity);
}