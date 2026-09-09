#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor; // r = water depth factor (0..1), g = sunlight, b = torchlight
layout(location = 3) in vec3 fragWorldPos;

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 sunDir;     // xyz = normalized sun dir, w = sun intensity (positive = vibrant visuals ON, negative = OFF)
    vec4 skyFog;     // xyz = fog color, w = fog distance (negative if underwater)
    vec4 camPos;     // xyz = camera pos, w = time
    vec4 playerPos;  // xyz = player pos, w = packed
    vec4 pointLight1; // xyz = dropped torch pos, w = intensity
    vec4 pointLight2; // xyz = placed torch pos, w = intensity
    vec4 heldTorch;   // xyz = held torch pos, w = active
} pc;

layout(location = 0) out vec4 outColor;

// Filmic Reinhard tonemapper: preserves colors and matches cell.frag
vec3 filmicTonemap(vec3 c) {
    const float whitePoint = 3.2;
    return (c * (1.0 + c / (whitePoint * whitePoint))) / (1.0 + c);
}

void main() {
    bool vibrant = (pc.sunDir.w > 0.0);
    float sunIntensity = abs(pc.sunDir.w);
    bool cameraUnderwater = (pc.skyFog.w < 0.0);
    float depthFactor = fragColor.r; // 0.0 = shore, 1.0 = deep
    float isDay = clamp(pc.skyFog.b * 1.8 - 0.25, 0.0, 1.0);

    vec3 V = normalize(pc.camPos.xyz - fragWorldPos);
    bool viewingFromBelow = (fragWorldPos.y > pc.camPos.y) || (dot(fragNormal, V) < 0.0);

    // 1. Depth Color Absorption (Beer-Lambert model)
    // Shallow water: crystal clear turquoise / cyan
    vec3 shallowColor = mix(vec3(0.14, 0.52, 0.68), vec3(0.20, 0.72, 0.86), isDay);
    // Deep water: rich oceanic sapphire navy
    vec3 deepColor = mix(vec3(0.02, 0.10, 0.28), vec3(0.05, 0.24, 0.48), isDay);

    float dSmooth = smoothstep(0.0, 0.85, depthFactor);
    vec3 waterBaseColor = mix(shallowColor, deepColor, dSmooth);

    // 2. Animated Multi-Wave Directional Gerstner / Wave Derivatives
    float t = pc.camPos.w * 1.6;
    vec2 pos = fragWorldPos.xz;
    
    // 4 Directional Wind Wave harmonics (angled wavefronts moving across the lake)
    vec2 d1 = vec2(0.85, 0.52);
    vec2 d2 = vec2(-0.45, 0.89);
    vec2 d3 = vec2(0.92, -0.38);
    vec2 d4 = vec2(0.35, 0.94);
    
    float w1 = dot(pos, d1) * 1.5 - t * 1.4;
    float w2 = dot(pos, d2) * 3.1 + t * 1.1;
    float w3 = dot(pos, d3) * 6.4 - t * 1.9;
    float w4 = dot(pos, d4) * 12.8 + t * 2.6;
    
    float waveAmp = vibrant ? 0.85 : 0.65;
    vec2 waveGrad = (
        d1 * (cos(w1) * 0.022) +
        d2 * (cos(w2) * 0.014) +
        d3 * (cos(w3) * 0.008) +
        d4 * (cos(w4) * 0.004)
    ) * waveAmp;

    vec3 N = normalize(fragNormal);
    if (abs(N.y) > 0.5) {
        N = normalize(vec3(waveGrad.x, (N.y > 0.0 ? 1.0 : -1.0), waveGrad.y));
    }

    vec3 effN = N;
    if (viewingFromBelow) {
        // Looking up from underwater: invert normal to face downward towards camera
        effN = -N;
    }

    float NdotV = clamp(dot(effN, V), 0.0, 1.0);
    vec3 L = normalize(pc.sunDir.xyz);

    vec3 waterSurfaceColor;
    float finalAlpha;

    if (viewingFromBelow || cameraUnderwater) {
        // -------------------------------------------------------------
        // UNDERWATER VIEW: Realistic See-Through Surface & Snell's Window
        // -------------------------------------------------------------
        float internalReflection = pow(1.0 - NdotV, 3.5);
        
        finalAlpha = mix(0.18, 0.58, internalReflection * dSmooth);
        
        float underCaustic = pow(sin(pos.x * 3.0 + t * 2.0) * sin(pos.y * 3.0 + t * 1.6) * 0.5 + 0.5, 2.0) * 0.35 * sunIntensity;
        vec3 sunBeams = mix(vec3(0.2, 0.4, 0.7), vec3(1.1, 1.05, 0.85), isDay) * underCaustic;
        
        vec3 underWaterTint = mix(vec3(0.12, 0.50, 0.68), vec3(0.04, 0.20, 0.40), dSmooth);
        waterSurfaceColor = mix(underWaterTint, vec3(0.02, 0.08, 0.22), internalReflection * 0.7) + sunBeams;
        
    } else {
        // -------------------------------------------------------------
        // ABOVE WATER VIEW: Schlick Fresnel Reflection & Sun Trail
        // -------------------------------------------------------------
        // Crystal-clear shallow transparency allowing submerged sand blocks to show through cleanly
        float baseAlpha = mix(0.25, 0.76, dSmooth);
        float fresnel = 0.03 + 0.62 * pow(1.0 - NdotV, 4.0);

        // Sky Mirror Reflection
        vec3 R = reflect(-V, effN);
        vec3 skyZenith = mix(vec3(0.04, 0.08, 0.20), vec3(0.15, 0.42, 0.88), isDay);
        vec3 skyHorizon = mix(pc.skyFog.rgb, vec3(0.55, 0.75, 0.95), isDay);
        vec3 reflectedSky = mix(skyHorizon, skyZenith, clamp(R.y * 1.5, 0.0, 1.0));

        // Shoreline & Tree mirror reflection for low-angle rays
        vec3 foliageGreen = vec3(0.12, 0.26, 0.08);
        vec3 shoreSand = vec3(0.55, 0.46, 0.30);
        
        // Near waterline: sandy shore; above: lush oak trees & hills
        vec3 terrainReflection = mix(shoreSand, foliageGreen, smoothstep(0.02, 0.12, R.y));
        float terrainSun = clamp(dot(vec3(-R.x, 0.5, -R.z), L), 0.25, 1.0);
        terrainReflection *= mix(0.45, 1.0, terrainSun * isDay);

        // Blend terrain into sky reflection based on low elevation angle R.y
        float terrainWeight = smoothstep(0.30, 0.02, R.y) * (1.0 - dSmooth * 0.45);
        vec3 reflectedScene = mix(reflectedSky, terrainReflection, terrainWeight * isDay);

        // Crisp narrow golden sun specular glint
        vec3 H = normalize(L + V);
        float NdotH = max(dot(effN, H), 0.0);
        float specSharp = pow(NdotH, 200.0) * 3.8 * sunIntensity;
        vec3 sunColor = mix(vec3(0.4, 0.6, 0.9), vec3(1.35, 1.20, 0.85), isDay);
        vec3 sunGlint = specSharp * sunColor;

        // Shoreline foam wave along shallow banks (Complementary Reimagined wave edges)
        float foamEdge = smoothstep(0.26, 0.03, depthFactor);
        float foamWave = 0.5 + 0.5 * sin(pos.x * 6.0 + pos.y * 6.0 + t * 3.2);
        float foam = foamEdge * smoothstep(0.35, 0.75, foamWave) * isDay;
        vec3 foamColor = vec3(0.88, 0.96, 1.0) * (foam * 0.70);

        waterSurfaceColor = mix(waterBaseColor, reflectedScene, fresnel * 0.65) + sunGlint + foamColor;
        finalAlpha = clamp(mix(baseAlpha, 0.90, fresnel) + foam * 0.3, 0.0, 1.0);
    }

    // Block light from torches placed nearby
    waterSurfaceColor += vec3(1.0, 0.65, 0.22) * fragColor.b;

    // Dynamic light from held torch
    if (pc.playerPos.w <= -0.5) {
        float dHeld = length(fragWorldPos - (pc.playerPos.xyz + vec3(0.0, 1.1, 0.0)));
        float atten = clamp(1.0 - dHeld / 14.0, 0.0, 1.0);
        waterSurfaceColor += vec3(1.0, 0.65, 0.22) * (atten * atten * 1.8);
    }

    // Dynamic light from dropped torch
    if (pc.pointLight1.w > 0.01) {
        float dDrop = length(fragWorldPos - pc.pointLight1.xyz);
        float atten = clamp(1.0 - dDrop / 14.0, 0.0, 1.0);
        waterSurfaceColor += vec3(1.0, 0.65, 0.22) * (atten * atten * 2.0 * pc.pointLight1.w);
    }

    // Atmospheric Distance Fog
    float dist = length(fragWorldPos - pc.camPos.xyz);
    float fogStart = cameraUnderwater ? 2.0 : 45.0;
    float fogEnd = abs(pc.skyFog.w);
    float fogFactor = clamp((dist - fogStart) / max(fogEnd - fogStart, 1.0), 0.0, 1.0);
    fogFactor = fogFactor * fogFactor;
    vec3 fogCol = cameraUnderwater ? vec3(0.02, 0.14, 0.40) : pc.skyFog.rgb;
    waterSurfaceColor = mix(waterSurfaceColor, fogCol, fogFactor);
    waterSurfaceColor = filmicTonemap(waterSurfaceColor);

    outColor = vec4(waterSurfaceColor, finalAlpha);
}
