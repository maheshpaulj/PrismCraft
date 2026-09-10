#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 sunDir;
    vec4 skyFog;
    vec4 camPos;
    vec4 lightColor;
    vec4 pointLight1;
    vec4 pointLight2;
    vec4 heldTorch;
    vec4 shaderOptions;
    vec4 dayInfo;
    vec4 pointLight3;
    vec4 pointLight4;
} pc;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

void main() {
    gl_Position = pc.mvp * vec4(inPosition.xy, 0.0, 1.0);
    fragTexCoord = inTexCoord;
    float alpha = (inNormal.z > 0.0) ? inNormal.z : 1.0;
    fragColor = vec4(inColor, alpha);
}
