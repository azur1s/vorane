#version 330 core

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;

uniform vec3 uLift;
uniform vec3 uGamma;
uniform vec3 uGain;
uniform vec3 uOffset;

// optional master strength 0..1 for blending
uniform float uStrength;

vec3 toLinear(vec3 c) {
    // approximate sRGB->linear
    return pow(c, vec3(2.2));
}

vec3 toSRGB(vec3 c) {
    // linear->sRGB
    return pow(c, vec3(1.0/2.2));
}

void main() {
    vec4 src = texture(uTex, vUV);

    // vec3 col = toLinear(src.rgb);
    vec3 col = src.rgb;

    // color_lifted = col * (1 - lift) + lift
    vec3 lifted = col * (vec3(1.0) - uLift) + uLift;

    // pow(lifted, 1/gamma). clamp gamma to avoid div0
    vec3 safeGamma = max(uGamma, vec3(0.0001));
    vec3 gammaApplied = pow(max(lifted, vec3(0.0)), 1.0 / safeGamma);

    vec3 gained = gammaApplied * uGain;

    vec3 graded = gained + uOffset;

    // clamp to display range
    graded = clamp(graded, 0.0, 1.0);
    // graded = toSRGB(graded);

    // strength mix
    vec3 outRgb = mix(col, graded, uStrength);

    fragColor = vec4(outRgb, src.a);
}
