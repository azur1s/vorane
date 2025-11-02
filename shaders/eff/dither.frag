#version 330 core

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
uniform vec2 uViewportSize; // viewport size in pixels
uniform float uDitherScale; // 1=normal, 2=2x bigger, etc.

// how many discrete steps per channel after quantization
uniform float uSteps;

// 4x4 Bayer ordered dither thresholds / 16.0
float bayer4(vec2 pixel) {
    vec2 p = floor(pixel / uDitherScale);
    // p is integer pixel coord mod 4
    int x = int(p.x) & 3;
    int y = int(p.y) & 3;

    // classic 4x4 Bayer matrix
    int m[16] = int[16](
         0,  8,  2, 10,
        12,  4, 14,  6,
         3, 11,  1,  9,
        15,  7, 13,  5
    );

    int idx = y * 4 + x;
    return float(m[idx]) / 16.0; // in [0,1)
}

void main() {
    vec4 src = texture(uTex, vUV);

    // pixel coord in screen space
    vec2 pixel = floor(vUV * uViewportSize);

    // ordered threshold in [0,1)
    float d = bayer4(pixel);

    // scale dither strength:
    // typical strength is about 1.0/uSteps so it nudges by <1 LSB
    float ditherAmount = (1.0 / uSteps);

    vec3 c = src.rgb + ditherAmount * (d - 0.5); // center around 0

    // quantize to uSteps levels (simulate lower precision)
    vec3 quantized = floor(c * uSteps) / uSteps;

    fragColor = vec4(quantized, src.a);
}