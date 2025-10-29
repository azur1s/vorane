#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
uniform mat3 uXform;

void main() {
  // apply affine in homogeneous coords
  vec3 p = uXform * vec3(vUV, 1.0);
  vec2 uv = p.xy;

  // transparent outside bounds (so you can layer/compose)
  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
    fragColor = vec4(0.0);
  } else {
    fragColor = texture(uTex, uv);
  }
}
