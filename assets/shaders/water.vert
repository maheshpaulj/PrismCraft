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

    // Geometric Gerstner macro wave displacement for water surfaces and top rims
    if (inNormal.y > 0.2) {
        float t = pc.camPos.w * 0.35;
        vec2 p = inPosition.xz;

        // 4 varied non-aligned Gerstner waves: broad, calm rolling swells
        // Direction, wavenumber k=2pi/lambda, amplitude a, speed s
        vec2 d0 = vec2(0.8944, 0.4472);  float k0 = 0.1142; float a0 = 0.014; float s0 = 0.32;
        vec2 d1 = vec2(-0.3846, 0.9231); float k1 = 0.1653; float a1 = 0.009; float s1 = 0.42;
        vec2 d2 = vec2(0.7071, -0.7071); float k2 = 0.2513; float a2 = 0.005; float s2 = 0.52;
        vec2 d3 = vec2(-0.7809, -0.6247);float k3 = 0.3700; float a3 = 0.002; float s3 = 0.62;

        float phi0 = dot(p, d0) * k0 - t * s0;
        float phi1 = dot(p, d1) * k1 - t * s1;
        float phi2 = dot(p, d2) * k2 + t * s2;
        float phi3 = dot(p, d3) * k3 - t * s3;

        float c0 = cos(phi0); float s_0 = sin(phi0);
        float c1 = cos(phi1); float s_1 = sin(phi1);
        float c2 = cos(phi2); float s_2 = sin(phi2);
        float c3 = cos(phi3); float s_3 = sin(phi3);

        // Vertical displacement only (preserves triangle mesh integrity without horizontal tearing)
        float dy = a0 * s_0 + a1 * s_1 + a2 * s_2 + a3 * s_3;
        worldPos.y += dy;

        // Analytic Gerstner normal from wave partial derivatives
        if (inNormal.y > 0.7) {
            float nx = -(d0.x * k0 * a0 * c0 + d1.x * k1 * a1 * c1 + d2.x * k2 * a2 * c2 + d3.x * k3 * a3 * c3);
            float nz = -(d0.y * k0 * a0 * c0 + d1.y * k1 * a1 * c1 + d2.y * k2 * a2 * c2 + d3.y * k3 * a3 * c3);
            float ny = 1.0;

            vec3 waveNorm = normalize(vec3(nx, ny, nz));

            // Blend flow direction if flowing
            if (length(inNormal.xz) > 0.02) {
                outNorm = normalize(vec3(inNormal.x + waveNorm.x * 0.5, 1.0, inNormal.z + waveNorm.z * 0.5));
            } else {
                outNorm = waveNorm;
            }
        }
    }

    gl_Position = pc.mvp * vec4(worldPos, 1.0);
    fragTexCoord = inTexCoord;
    fragNormal = outNorm;
    fragColor = inColor;
    fragWorldPos = worldPos;
}