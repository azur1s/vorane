#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
void main() {
    vec4 color = texture(uTex, vUV);
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    fragColor = vec4(vec3(gray), color.a);
}
