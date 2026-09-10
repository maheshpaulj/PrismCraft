#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 sunDir;     // xyz = normalized sun dir, w = sun intensity
    vec4 skyFog;     // xyz = fog color, w = fog distance
    vec4 camPos;     // xyz = camera pos, w = time
    vec4 lightColor;
    vec4 pointLight1;
    vec4 pointLight2;
    vec4 heldTorch;
    vec4 shaderOptions;
    vec4 dayInfo;    // x = dayFactor, y = sunHeight, z = exposure, w = fogDensity
    vec4 pointLight3;
    vec4 pointLight4;
} pc;

layout(location = 0) out vec3 fragViewDir;

void main() {
    fragViewDir = normalize(inPosition);
    vec3 worldPos = pc.camPos.xyz + inPosition;
    vec4 clipPos = pc.mvp * vec4(worldPos, 1.0);
    // Project directly to the far plane z = w (depth = 1.0 in Vulkan)
    gl_Position = clipPos.xyww;
}
