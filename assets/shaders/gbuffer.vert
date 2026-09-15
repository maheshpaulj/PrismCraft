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
    vec4 playerPos;
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
    vec3 worldPos = inPosition;
    vec3 outNorm = inNormal;
    float normLen = length(inNormal);

    if (normLen > 1.1) {
        float time = pc.camPos.w;
        outNorm = inNormal / normLen;

        if (normLen > 1.6) {
            // Foliage (tall grass, flowers, saplings, crops): anchored at bottom, sways at top
            float waveWeight = clamp(normLen - 1.0, 0.0, 1.0);
            float waveX = sin(time * 2.6 + inPosition.x * 0.85 + inPosition.z * 0.65) * 0.08
                        + sin(time * 4.2 + inPosition.x * 1.60 + inPosition.z * 1.30) * 0.03;
            float waveZ = cos(time * 2.3 + inPosition.x * 0.60 + inPosition.z * 0.90) * 0.07
                        + cos(time * 3.9 + inPosition.x * 1.20 + inPosition.z * 1.50) * 0.025;
            worldPos.x += waveX * waveWeight;
            worldPos.z += waveZ * waveWeight;
            worldPos.y -= (waveX * waveX + waveZ * waveZ) * 0.5 * waveWeight;
        } else {
            // Leaves: gentle 3D wind rustle and fluttering
            float leafWeight = clamp((normLen - 1.0) * 2.5, 0.0, 1.0);
            float lx = sin(time * 2.1 + inPosition.x * 1.3 + inPosition.y * 1.6) * 0.032;
            float ly = cos(time * 2.6 + inPosition.y * 1.9 + inPosition.z * 1.2) * 0.022;
            float lz = sin(time * 1.9 + inPosition.z * 1.5 + inPosition.x * 1.0) * 0.032;
            worldPos += vec3(lx, ly, lz) * leafWeight;
        }
    }

    gl_Position = pc.mvp * vec4(worldPos, 1.0);
    fragTexCoord = inTexCoord;
    fragNormal = outNorm;
    fragColor = inColor;
    fragWorldPos = worldPos;
}
