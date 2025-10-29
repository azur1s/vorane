#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
uniform mat3 uXform;
uniform vec2 uCanvasSize; // window framebuffer size in pixels
uniform float uCheckerSize; // size of checker squares in pixels

uniform vec3 uCheckerColor1   = vec3(0.137, 0.137, 0.137); // #232323
uniform vec3 uCheckerColor2   = vec3(0.227, 0.227, 0.227); // #3a3a3a
uniform vec3 uBackgroundColor = vec3(0.0, 0.0, 0.0);

uniform bool uPremultiplied = false;

vec3 checker(vec2 uv) {
  ivec2 texSize = textureSize(uTex, 0);
  vec2 texel    = uv * vec2(texSize);
  vec2 cell     = floor(texel / uCheckerSize);
  float m       = mod(cell.x + cell.y, 2.0);
  return mix(uCheckerColor1, uCheckerColor2, m);
}

void main() {
  // Map screen quad uv -> source uv
  vec3 p  = uXform * vec3(vUV, 1.0);
  vec2 uv = p.xy;

  // Decide which background to show
  bool inCanvas = (uv.x >= 0.0 && uv.x <= 1.0 &&
                   uv.y >= 0.0 && uv.y <= 1.0);

  // Background color for this pixel
  vec3 bg = inCanvas ? checker(uv) : uBackgroundColor;

  // Sample image (clamp outside so we get transparent)
  vec4 texel;
  if (inCanvas) {
    texel = texture(uTex, uv);
  } else {
    texel = vec4(0.0);
  }

  // Composite image over background
  vec3 rgb;
  if (uPremultiplied) {
    // texel.rgb already multiplied by texel.a
    rgb = texel.rgb + bg * (1.0 - texel.a);
  } else {
    rgb = mix(bg, texel.rgb, texel.a);
  }

  fragColor = vec4(rgb, 1.0);
}