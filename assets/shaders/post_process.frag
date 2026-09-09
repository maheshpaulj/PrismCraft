#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D hdrSceneTexture;

layout(push_constant) uniform PushConstants {
    vec4 params; // x = exposure, y = vibrance, z = bloomStrength, w = time
} pc;

vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 triangularDither(vec3 color, vec2 uv) {
    float r = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
    float g = fract(sin(dot(uv, vec2(93.9898, 67.345))) * 24634.6345);
    float b = fract(sin(dot(uv, vec2(41.1234, 19.987))) * 58972.1234);
    return color + (vec3(r, g, b) - 0.5) * (1.0 / 255.0);
}

void main() {
    vec3 hdr = texture(hdrSceneTexture, inUV).rgb;

    // Stylized vibrance boost (Complementary Reimagined aesthetic)
    float luma = dot(hdr, vec3(0.2126, 0.7152, 0.0722));
    hdr = mix(vec3(luma), hdr, 1.12);

    // ACES filmic tonemapping
    vec3 ldr = ACESFilm(hdr * 1.0);

    // Blue/triangular noise dither to eliminate 8-bit banding
    ldr = triangularDither(ldr, inUV);

    outColor = vec4(ldr, 1.0);
}
