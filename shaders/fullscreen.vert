#version 330 core
const vec2 verts[4] = vec2[](
  vec2(-1.0, -1.0),
  vec2( 1.0, -1.0),
  vec2(-1.0,  1.0),
  vec2( 1.0,  1.0)
);
out vec2 vUV;
void main() {
  gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
  vUV = (verts[gl_VertexID] + 1.0) * 0.5; // map from [-1,1] to [0,1]
}