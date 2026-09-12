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

    // Physical vertex wave displacement for top water surfaces and wall top rims
    // Synchronized 1:1 with wave normals in water.frag
    if (inNormal.y > 0.2) {
        float t = pc.camPos.w * 0.95;
        vec2 p = inPosition.xz;

        // 3 broad gentle ocean swells (calm, cohesive, vast water body)
        vec2 d1 = vec2(0.8, 0.6);
        float k1 = 0.1745;
        float a1 = 0.038;
        float w1 = dot(p, d1) * k1 - t * 1.25;

        vec2 d2 = vec2(-0.6, 0.8);
        float k2 = 0.2856;
        float a2 = 0.024;
        float w2 = dot(p, d2) * k2 + t * 1.50;

        vec2 d3 = vec2(0.5, -0.86);
        float k3 = 0.4488;
        float a3 = 0.012;
        float w3 = dot(p, d3) * k3 - t * 1.90;

        float waveHeight = sin(w1) * a1 + sin(w2) * a2 + sin(w3) * a3;
        worldPos.y += waveHeight;
    }

    gl_Position = pc.mvp * vec4(worldPos, 1.0);
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
    fragColor = inColor;
    fragWorldPos = worldPos;
}