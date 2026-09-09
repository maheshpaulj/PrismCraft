#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor; // r = skylight/shadow (0.08..1.0), g = smoothLighting (faceDir * vertexAO), b = torchLight (or torchLight + 2.0 if submerged)
layout(location = 3) in vec3 fragWorldPos;

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 sunDir;      // xyz = normalized sun/light dir, w = sun intensity (sign indicates vibrant)
    vec4 skyFog;      // xyz = fog color, w = fog distance (negative if underwater)
    vec4 camPos;      // xyz = camera pos, w = time
    vec4 playerPos;   // xyz = player pos, w = packed
    vec4 pointLight1; // xyz = dropped torch pos, w = intensity
    vec4 pointLight2; // xyz = placed torch pos, w = intensity
    vec4 heldTorch;   // xyz = held torch pos, w = active
} pc;

layout(location = 0) out vec4 outColor;

// Filmic Reinhard tonemapper: preserves rich texture colors, prevents highlight clipping, avoids crushing shadows
vec3 filmicTonemap(vec3 c) {
    const float whitePoint = 3.2;
    return (c * (1.0 + c / (whitePoint * whitePoint))) / (1.0 + c);
}

void main() {
    // 1. Self-illuminated celestial bodies (Sun, Moon) & HUD overlays
    if (abs(pc.sunDir.w) >= 1.9) {
        outColor = vec4(fragColor, 1.0);
        return;
    }

    vec4 tex = texture(texSampler, fragTexCoord);

    // Alpha cutout for foliage, torches, and flowers
    if (tex.a < 0.35) {
        discard;
    }

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(pc.sunDir.xyz);
    float NdotL = dot(N, L);

    float sunIntensity = abs(pc.sunDir.w);
    float isDay = clamp((L.y + 0.12) / 0.55, 0.0, 1.0);

    // 2. Direct Sunlight / Moonlight Shading with Block Shadows
    float blockShadow = fragColor.r;        // 1.0 = exposed to sky, 0.08 = beneath terrain/overhangs
    float smoothLighting = fragColor.g;     // faceDir (orientation multiplier) * vertexAO (smooth corner occlusion)

    // Direct lighting term: only surfaces oriented towards the celestial light source receive direct illumination
    float directFactor = max(NdotL, 0.0) * blockShadow * sunIntensity;

    // Palette: Warm golden sunlight during day, cool silver moonlight at night
    vec3 sunColor = vec3(1.06, 0.98, 0.88);
    vec3 moonColor = vec3(0.14, 0.18, 0.28);
    vec3 celestialLightColor = mix(moonColor, sunColor, isDay) * directFactor;

    // 3. Ambient Skylight & Minecraft Discrete Face Lighting
    // Upward-facing faces receive full skylight; downward/lateral faces receive bounced ambient
    float skyHemisphere = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 daySkyAmbient = mix(vec3(0.24, 0.28, 0.34), vec3(0.38, 0.46, 0.58), skyHemisphere);
    
    // Genuinely dark night ambient: moonlight illumination is dim and moody
    vec3 nightSkyAmbient = mix(vec3(0.015, 0.020, 0.035), vec3(0.030, 0.040, 0.065), skyHemisphere);
    vec3 ambientLight = mix(nightSkyAmbient, daySkyAmbient, isDay);

    // Ambient light is modulated by block skylight exposure and interpolated smooth lighting (faceDir * vertexAO)
    ambientLight *= (0.28 + 0.72 * blockShadow) * (0.35 + 0.65 * smoothLighting);

    // Also modulate direct light with smoothLighting to preserve face definition and corner AO under sunlight
    celestialLightColor *= (0.30 + 0.70 * smoothLighting);

    // 4. Dynamic Point Lights (Torches)
    vec3 torchLight = vec3(0.0);
    const vec3 torchColor = vec3(1.0, 0.62, 0.18); // Warm, vibrant fire glow

    // A. Handheld Torch (Steve's right hand)
    if (pc.heldTorch.w > 0.01) {
        vec3 toTorch = pc.heldTorch.xyz - fragWorldPos;
        float dist = length(toTorch);
        if (dist < 15.0) {
            float atten = clamp(1.0 - dist / 15.0, 0.0, 1.0);
            atten = atten * atten;
            vec3 torchDir = toTorch / max(dist, 0.001);
            float torchDiffuse = max(dot(N, torchDir), 0.0) * 0.70 + 0.30;
            torchLight += torchColor * (atten * torchDiffuse * 2.4 * smoothLighting);
        }
    }

    // B. Dropped Torch (floating item drop)
    if (pc.pointLight1.w > 0.01) {
        vec3 toDrop = pc.pointLight1.xyz - fragWorldPos;
        float dist = length(toDrop);
        if (dist < 13.0) {
            float atten = clamp(1.0 - dist / 13.0, 0.0, 1.0);
            atten = atten * atten;
            vec3 dropDir = toDrop / max(dist, 0.001);
            float dropDiffuse = max(dot(N, dropDir), 0.0) * 0.70 + 0.30;
            torchLight += torchColor * (atten * dropDiffuse * pc.pointLight1.w * 2.0 * smoothLighting);
        }
    }

    // C. Placed World Torches (baked in vertex color b-channel)
    bool isSubmerged = (fragColor.b >= 1.5);
    float placedTorch = isSubmerged ? (fragColor.b - 2.0) : fragColor.b;
    if (placedTorch > 0.01) {
        torchLight += torchColor * (placedTorch * 1.6 * smoothLighting);
    }

    // 5. Total Illumination & Material Color
    vec3 totalLight = ambientLight + celestialLightColor + torchLight;
    vec3 litColor = tex.rgb * totalLight;

    // 6. Dynamic Seabed Caustics (Submerged surfaces under water)
    if (isSubmerged) {
        float t = pc.camPos.w * 2.0;
        vec2 p = fragWorldPos.xz;
        float c1 = sin(p.x * 2.4 + t * 1.5) * sin(p.y * 2.4 + t * 1.2);
        float c2 = sin(p.x * 4.8 - t * 1.8 + p.y * 2.6) * cos(p.y * 4.8 + t * 1.5);
        float caustic = pow(clamp(c1 * 0.5 + c2 * 0.5 + 0.5, 0.0, 1.0), 3.0);
        vec3 causticColor = mix(vec3(0.12, 0.35, 0.60), vec3(1.05, 0.98, 0.75), isDay);
        litColor += caustic * 0.18 * blockShadow * max(sunIntensity, 0.20) * causticColor;
    }

    // 7. Atmospheric Distance Fog / Deep Ocean Fog ("edges foggy and what not")
    float dist = length(fragWorldPos - pc.camPos.xyz);
    bool cameraUnderwater = (pc.skyFog.w < 0.0);
    float fogEnd = abs(pc.skyFog.w);
    float fogStart = cameraUnderwater ? 2.0 : (fogEnd * 0.58);
    float fogFactor = clamp((dist - fogStart) / max(fogEnd - fogStart, 1.0), 0.0, 1.0);
    fogFactor = fogFactor * fogFactor; // Smooth quadratic haze blending chunks seamlessly into horizon

    vec3 fogColor = cameraUnderwater ? vec3(0.02, 0.14, 0.38) : pc.skyFog.rgb;
    vec3 finalColor = mix(litColor, fogColor, fogFactor);

    if (cameraUnderwater) {
        finalColor = mix(finalColor, vec3(0.02, 0.12, 0.36), 0.32);
    }

    // 8. Balanced Tone Reproduction: true colors, zero green tint, beautiful highlights
    finalColor = filmicTonemap(finalColor);

    outColor = vec4(finalColor, tex.a);
}
