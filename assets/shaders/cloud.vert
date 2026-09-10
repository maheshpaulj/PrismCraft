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
    vec4 dayInfo;
    vec4 pointLight3;
    vec4 pointLight4;
} pc;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec3 fragWorldPos;

void main() {
    // Offset dome geometry by camera position in world space
    vec3 worldPos = pc.camPos.xyz + inPosition;
    fragWorldPos = worldPos;
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
    fragColor = inColor;

    gl_Position = pc.mvp * vec4(worldPos, 1.0);
}
