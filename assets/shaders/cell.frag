#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor; // r = skylight/shadow (0.08..1.0), g = smoothLighting (faceDir * vertexAO), b = torchLight (or torchLight + 2.0 if submerged)
layout(location = 3) in vec3 fragWorldPos;

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform sampler2DArrayShadow shadowMap;

layout(std140, binding = 2) uniform ShadowUBO {
    mat4 lightViewProj[2];
    vec4 cascadeSplits; // x = split0, y = split1
} shadowUBO;

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

// ACES Filmic Tonemapper: cinematic contrast, deep rich blacks, and smooth golden highlight roll-off
vec3 acesFilmicTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Vibrance: selectively boost saturation on desaturated tones without oversaturating already saturated colors
vec3 applyVibrance(vec3 color, float amount) {
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float maxC = max(color.r, max(color.g, color.b));
    float minC = min(color.r, min(color.g, color.b));
    float sat = (maxC - minC) / max(maxC, 0.001);
    return mix(vec3(lum), color, 1.0 + amount * (1.0 - sat));
}

// Subtle cinematic contrast S-curve
vec3 contrastSCurve(vec3 c) {
    vec3 clamped = clamp(c, 0.0, 1.0);
    vec3 s = clamped * clamped * (3.0 - 2.0 * clamped);
    return mix(c, s, 0.35);
}

// Colored torch incandescent glow falloff gradient
vec3 getTorchGlow(float dist, float maxDist, bool vibrant, bool optTorchColorBleed) {
    if (!vibrant || !optTorchColorBleed) {
        return vec3(1.0, 0.62, 0.18);
    }
    float ratio = clamp(dist / maxDist, 0.0, 1.0);
    vec3 core = vec3(1.0, 0.90, 0.75);   // Incandescent warm white
    vec3 mid = vec3(1.0, 0.62, 0.18);    // Rich amber
    vec3 edge = vec3(0.95, 0.35, 0.06);  // Deep ember copper
    return mix(mix(core, mid, smoothstep(0.0, 0.35, ratio)), edge, smoothstep(0.35, 1.0, ratio));
}

// Real-Time Dynamic Light Shadow: Player body casts shadow from held torch
float computePlayerTorchOcclusion(vec3 fragPos, vec3 torchPos, vec3 playerPos) {
    if (length(fragPos - torchPos) < 0.65) return 1.0; // Held torch and player hand are never self-occluded!
    vec3 rayDir = fragPos - torchPos;
    float rayLen = length(rayDir);
    if (rayLen < 0.1) return 1.0;

    // Test ray against player cylinder (radius 0.38m, height [playerPos.y, playerPos.y + 1.85m])
    vec2 pXZ = playerPos.xz;
    vec2 dXZ = rayDir.xz;
    float dLen2 = dot(dXZ, dXZ);
    if (dLen2 > 0.0001) {
        float t = clamp(dot(pXZ - torchPos.xz, dXZ) / dLen2, 0.0, 1.0);
        if (t > 0.04 && t < 0.96) {
            vec2 closestXZ = torchPos.xz + dXZ * t;
            float distXZ = length(closestXZ - pXZ);
            float hitY = torchPos.y + rayDir.y * t;
            if (distXZ < 0.38 && hitY >= playerPos.y && hitY <= (playerPos.y + 1.85)) {
                return smoothstep(0.12, 0.38, distXZ);
            }
        }
    }
    return 1.0;
}

// Real-Time Dynamic Light Shadow: Block/terrain obstacles cast shadows from torches
float sampleTorchBlockOcclusion(vec3 fragPos, vec3 torchPos) {
    vec3 toTorch = torchPos - fragPos;
    float dist = length(toTorch);
    if (dist < 0.6) return 1.0;

    float occlusion = 1.0;
    for (int k = 1; k <= 3; ++k) {
        float frac = float(k) * 0.25;
        vec3 P = fragPos + toTorch * frac;

        vec4 sc = shadowUBO.lightViewProj[0] * vec4(P, 1.0);
        vec3 coords = sc.xyz / sc.w;
        if (coords.x >= 0.01 && coords.x <= 0.99 &&
            coords.y >= 0.01 && coords.y <= 0.99 &&
            coords.z >= 0.01 && coords.z <= 0.99) {
            float shadowZ = texture(shadowMap, vec4(coords.xy, 0.0, coords.z));
            if (shadowZ < 0.5) {
                occlusion *= 0.35;
            }
        }
    }
    return occlusion;
}

// Hardware PCF Real-Time Cascaded Shadow Sampling
float sampleRealtimeShadow(vec3 worldPos, vec3 N, vec3 L) {
    float viewDist = length(worldPos - pc.camPos.xyz);

    int cascade = 0;
    if (viewDist > shadowUBO.cascadeSplits.x) {
        cascade = 1;
    }
    if (viewDist > shadowUBO.cascadeSplits.y) {
        return 1.0;
    }

    // Normal offset bias to completely prevent surface self-shadowing acne
    float normalBiasFactor = (cascade == 0) ? 0.04 : 0.12;
    vec3 biasedPos = worldPos + N * normalBiasFactor;

    vec4 sc = shadowUBO.lightViewProj[cascade] * vec4(biasedPos, 1.0);
    vec3 shadowCoords = sc.xyz / sc.w;

    if (shadowCoords.x < 0.0 || shadowCoords.x > 1.0 ||
        shadowCoords.y < 0.0 || shadowCoords.y > 1.0 ||
        shadowCoords.z < 0.0 || shadowCoords.z > 1.0) {
        return 1.0;
    }

    float cosAlpha = max(dot(N, L), 0.0);
    float depthBias = max(0.0030 * (1.0 - cosAlpha), 0.0007);

    float texelSize = 1.0 / 2048.0;
    float shadow = 0.0;
    shadow += texture(shadowMap, vec4(shadowCoords.xy + vec2(-0.5, -0.5) * texelSize, float(cascade), shadowCoords.z - depthBias));
    shadow += texture(shadowMap, vec4(shadowCoords.xy + vec2( 0.5, -0.5) * texelSize, float(cascade), shadowCoords.z - depthBias));
    shadow += texture(shadowMap, vec4(shadowCoords.xy + vec2(-0.5,  0.5) * texelSize, float(cascade), shadowCoords.z - depthBias));
    shadow += texture(shadowMap, vec4(shadowCoords.xy + vec2( 0.5,  0.5) * texelSize, float(cascade), shadowCoords.z - depthBias));
    shadow *= 0.25;

    // Blend cascade transition smoothly
    if (cascade == 0 && viewDist > (shadowUBO.cascadeSplits.x - 4.0)) {
        float blendFactor = (viewDist - (shadowUBO.cascadeSplits.x - 4.0)) / 4.0;
        vec3 biasedPos1 = worldPos + N * 0.12;
        vec4 sc1 = shadowUBO.lightViewProj[1] * vec4(biasedPos1, 1.0);
        vec3 sc1Coords = sc1.xyz / sc1.w;
        if (sc1Coords.x >= 0.0 && sc1Coords.x <= 1.0 && sc1Coords.y >= 0.0 && sc1Coords.y <= 1.0) {
            float s1 = texture(shadowMap, vec4(sc1Coords.xy, 1.0, sc1Coords.z - depthBias));
            shadow = mix(shadow, s1, clamp(blendFactor, 0.0, 1.0));
        }
    }

    // Fade at far edge
    if (cascade == 1 && viewDist > (shadowUBO.cascadeSplits.y - 12.0)) {
        float fade = (viewDist - (shadowUBO.cascadeSplits.y - 12.0)) / 12.0;
        shadow = mix(shadow, 1.0, clamp(fade, 0.0, 1.0));
    }

    return shadow;
}

// -------------------------------------------------------------
// Fast 2D OpenSimplex-like Noise & Terrain Height (Matching TerrainGen.cpp)
// -------------------------------------------------------------
vec2 hash2D(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float terrainNoise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(dot(hash2D(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0)),
                   dot(hash2D(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
               mix(dot(hash2D(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
                   dot(hash2D(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x), u.y);
}

float getTerrainHeight(vec2 worldXZ) {
    vec2 p = vec2(worldXZ.x, worldXZ.y * 0.8660254);
    float cont = terrainNoise2D(p * 0.0035);
    float det  = terrainNoise2D(p * 0.015 + vec2(10.1, 15.3));
    float h = 52.0;
    if (cont < -0.30) {
        float t = (-cont - 0.30) / 0.70;
        h = 42.0 - t * 14.0 + det * 2.0;
    } else if (cont < -0.15) {
        float t = (cont + 0.30) / 0.15;
        h = 43.0 + t * 7.0 + det * 1.5;
    } else if (cont <= 0.45) {
        float t = (cont + 0.15) / 0.60;
        h = 50.5 + t * 3.0 + det * 2.0;
    } else if (cont <= 0.75) {
        float t = (cont - 0.45) / 0.30;
        h = 54.0 + t * 16.0 + det * 4.0;
    } else {
        float t = (cont - 0.75) / 0.25;
        h = 70.0 + t * 24.0 + det * 6.0;
    }
    return h;
}

// -------------------------------------------------------------
// Dynamic Cloud Shadowing matching 3D sky cumulus clusters
// -------------------------------------------------------------
float sampleCloudShadow(vec3 worldPos, vec3 L) {
    if (L.y <= 0.02) return 1.0;
    float tCloud = (196.0 - worldPos.y) / L.y;
    if (tCloud <= 0.0) return 1.0;
    vec2 cloudHit = worldPos.xz + L.xz * tCloud;
    vec2 wind = vec2(pc.camPos.w * 2.0, 0.0);
    vec2 ws = cloudHit + wind;

    // Macro cluster coordinate matching cloud.frag: 0.00025
    vec2 pMacro = ws * 0.00025;
    float macro = sin(pMacro.x * 3.14 + cos(pMacro.y * 2.5)) * cos(pMacro.y * 3.14) * 0.5 + 0.5;
    float coverage = smoothstep(0.42, 0.65, macro);
    return 1.0 - coverage * 0.65;
}

// -------------------------------------------------------------
// Volumetric Crepuscular God Rays (Light Shafts through clouds & trees)
// -------------------------------------------------------------
vec3 computeGodRays(vec3 rayOrigin, vec3 targetPos, vec3 L, float isDay, float goldenHour) {
    vec3 rayDir = targetPos - rayOrigin;
    float rayDist = length(rayDir);
    if (rayDist < 4.0) return vec3(0.0);
    rayDir /= rayDist;

    float cosTheta = dot(rayDir, L);
    float phase = pow(max(cosTheta * 0.5 + 0.5, 0.0), 3.6) * 1.8 + 0.12;

    vec3 goldenSun = vec3(1.85, 1.32, 0.68);
    vec3 midDaySun = vec3(1.15, 1.08, 0.95);
    vec3 activeSun = mix(midDaySun, goldenSun, goldenHour);

    // 1. Sun Occlusion by Distant Mountains / LOD Hills (Tracing up to 750m)
    float sunOcclusion = 1.0;
    if (L.y > 0.02) {
        for (int s = 1; s <= 10; ++s) {
            float testDist = float(s) * 75.0;
            vec3 testP = rayOrigin + L * testDist;
            float mountainY = getTerrainHeight(testP.xz);
            if (testP.y < mountainY) {
                float diff = mountainY - testP.y;
                sunOcclusion = min(sunOcclusion, clamp(1.0 - diff * 0.25, 0.0, 1.0));
            }
        }
    }
    if (sunOcclusion <= 0.001) return vec3(0.0);

    float dither = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);

    const int SAMPLES = 4;
    float inscatterSum = 0.0;
    float maxDist = min(rayDist, 180.0);

    for (int i = 0; i < SAMPLES; ++i) {
        float frac = (float(i) + dither) / float(SAMPLES);
        float sampleDist = frac * maxDist;
        vec3 p = rayOrigin + rayDir * sampleDist;

        // A. Terrain/tree shadow at point p
        int cascade = (sampleDist > shadowUBO.cascadeSplits.x) ? 1 : 0;
        vec4 sc = shadowUBO.lightViewProj[cascade] * vec4(p, 1.0);
        vec3 coords = sc.xyz / sc.w;
        float shadow = 1.0;
        if (coords.x >= 0.0 && coords.x <= 1.0 &&
            coords.y >= 0.0 && coords.y <= 1.0 &&
            coords.z >= 0.0 && coords.z <= 1.0) {
            shadow = texture(shadowMap, vec4(coords.xy, float(cascade), coords.z - 0.002));
        }

        // B. Foliage gap dappling: tree leaf gaps let sunlight stream through branches
        float leafGap = sin(p.x * 3.6 + p.y * 4.2) * cos(p.z * 3.6 + p.y * 3.1) * 0.5 + 0.5;
        float leafDapple = smoothstep(0.32, 0.70, leafGap) * 0.75;
        float effectiveShadow = max(shadow, leafDapple);

        // C. Cloud shadow at point p (shafts stream through gaps where clouds thin out)
        float cloudLight = sampleCloudShadow(p, L);

        inscatterSum += effectiveShadow * cloudLight;
    }

    float shaftIntensity = inscatterSum / float(SAMPLES);
    float distFade = smoothstep(6.0, 45.0, rayDist) * (1.0 - smoothstep(120.0, 260.0, rayDist) * 0.4);
    float beamPower = (0.35 + goldenHour * 0.65) * isDay;

    return activeSun * (shaftIntensity * phase * beamPower * distFade * sunOcclusion);
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

    // Unpack Shader & Graphics Options
    bool vibrant = (pc.sunDir.w > 0.0);
    int settingsFlags = int(pc.pointLight2.w);
    bool optPlayerShadow = (settingsFlags & 1) != 0;
    bool optClouds = (settingsFlags & 2) != 0;
    bool optCloudShadows = (settingsFlags & 4) != 0;
    bool optSmoothLighting = (settingsFlags & 8) != 0;
    bool optTorchColorBleed = (settingsFlags & 16) != 0;
    int optAtmosFog = (settingsFlags >> 5) & 3;
    int optShadowQuality = int(pc.pointLight2.x + 0.5);
    int optColorGrading = int(pc.pointLight2.z + 0.5);

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(pc.sunDir.xyz);
    float NdotL = dot(N, L);

    float sunIntensity = abs(pc.sunDir.w);
    float isDay = clamp((L.y + 0.12) / 0.55, 0.0, 1.0);

    // 2. Direct Sunlight / Moonlight Shading with Real-Time & Block Shadows
    float blockShadow = fragColor.r;        // 1.0 = exposed to sky, 0.08 = beneath terrain/overhangs
    float smoothLighting = optSmoothLighting ? fragColor.g : 1.0;

    float rtShadow = 1.0;
    if (vibrant && (optShadowQuality > 0)) {
        rtShadow = sampleRealtimeShadow(fragWorldPos, N, L);
    }

    float combinedShadow = min(rtShadow, blockShadow);

    // Tree canopy daylight transmission & soft ambient bounce:
    // Outdoor surfaces under tree canopies (blockShadow >= 0.35) receive filtered daylight and soft bounce
    float outdoorCanopy = smoothstep(0.20, 0.55, blockShadow) * isDay;
    if (outdoorCanopy > 0.01 && combinedShadow < 0.32) {
        // Dappled foliage light passing through tree leaf clusters
        float leafNoise = sin(fragWorldPos.x * 2.8 + fragWorldPos.y * 3.4) * cos(fragWorldPos.z * 2.8 + fragWorldPos.y * 2.2) * 0.5 + 0.5;
        float dappledLight = mix(0.20, 0.36, leafNoise);
        combinedShadow = mix(combinedShadow, dappledLight, outdoorCanopy * (1.0 - combinedShadow));
    }

    float directFactor = max(NdotL, 0.0) * combinedShadow * sunIntensity;

    // Dynamic Cloud Shadows on Terrain: Synchronized 1:1 with sky clouds
    if (vibrant && optCloudShadows && (optShadowQuality > 0) && L.y > 0.03) {
        float cShadow = sampleCloudShadow(fragWorldPos, L);
        directFactor *= cShadow;
    }

    // Golden-hour factor: peaks when sun is at lower afternoon angles (matching reference image)
    float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);

    // Warm golden sunlight during day, rich amber at sunset/golden-hour, cool silver moonlight at night
    vec3 midDaySun = vec3(1.15, 1.08, 0.95);
    vec3 goldenSun = vec3(1.65, 1.18, 0.58); // Rich radiant amber gold
    vec3 sunColor  = mix(midDaySun, goldenSun, goldenHour);
    vec3 moonColor = vec3(0.14, 0.18, 0.28);
    vec3 celestialLightColor = mix(moonColor, sunColor, isDay) * directFactor;

    // 3. Ambient Skylight & SSAO Contact Occlusion
    // Rich ground bounce and soft sky dome illumination with contact shadows
    float skyHemisphere = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambientGround = mix(vec3(0.25, 0.28, 0.22), vec3(0.48, 0.40, 0.24), goldenHour);
    vec3 ambientSky    = mix(vec3(0.42, 0.52, 0.68), vec3(0.62, 0.55, 0.46), goldenHour);
    vec3 daySkyAmbient = mix(ambientGround, ambientSky, skyHemisphere);
    vec3 nightSkyAmbient = mix(vec3(0.018, 0.024, 0.040), vec3(0.035, 0.045, 0.075), skyHemisphere);
    vec3 ambientLight = mix(nightSkyAmbient, daySkyAmbient, isDay);

    float ssaoFactor = pow(smoothLighting, 1.3);
    // Daytime ambient sky bounce stays pleasantly soft under trees and cliffs, only decaying in deep closed caves
    float skyOcc = mix(0.58 * smoothstep(0.02, 0.18, blockShadow), 1.0, clamp(blockShadow * 1.15, 0.0, 1.0));
    vec3 foliageBounce = vec3(0.06, 0.12, 0.04) * (outdoorCanopy * (1.0 - combinedShadow));
    ambientLight = ambientLight * skyOcc * (0.42 + 0.58 * ssaoFactor) + foliageBounce;
    celestialLightColor *= (0.35 + 0.65 * smoothLighting);

    // 4. Dynamic Point Lights with Real-Time Shadows (Torches)
    vec3 torchLight = vec3(0.0);

    // A. Handheld Torch (Steve's right hand)
    if (pc.heldTorch.w > 0.01) {
        vec3 toTorch = pc.heldTorch.xyz - fragWorldPos;
        float dist = length(toTorch);
        if (dist < 15.0) {
            float atten = clamp(1.0 - dist / 15.0, 0.0, 1.0);
            atten = atten * atten;
            vec3 torchDir = toTorch / max(dist, 0.001);
            float NdotT = (dist < 0.65) ? max(dot(N, torchDir) * 0.5 + 0.65, 0.45) : max(dot(N, torchDir), 0.0);
            if (NdotT > 0.0) {
                float playerOcc = computePlayerTorchOcclusion(fragWorldPos, pc.heldTorch.xyz, pc.playerPos.xyz);
                float blockOcc = (vibrant && optShadowQuality > 0 && dist >= 0.65) ? sampleTorchBlockOcclusion(fragWorldPos, pc.heldTorch.xyz) : 1.0;
                float torchShadow = playerOcc * blockOcc;
                vec3 tCol = getTorchGlow(dist, 15.0, vibrant, optTorchColorBleed);
                torchLight += tCol * (atten * NdotT * 3.2 * smoothLighting * torchShadow);
            }
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
            float dropDiffuse = max(dot(N, dropDir), 0.0);
            if (dropDiffuse > 0.0) {
                float blockOcc = (vibrant && optShadowQuality > 0) ? sampleTorchBlockOcclusion(fragWorldPos, pc.pointLight1.xyz) : 1.0;
                vec3 tCol = getTorchGlow(dist, 13.0, vibrant, optTorchColorBleed);
                torchLight += tCol * (atten * dropDiffuse * pc.pointLight1.w * 2.2 * smoothLighting * blockOcc);
            }
        }
    }

    // C. Placed World Torches (baked in vertex color b-channel)
    bool isSubmerged = (fragColor.b >= 1.5);
    float placedTorch = isSubmerged ? (fragColor.b - 2.0) : fragColor.b;
    if (placedTorch > 0.01) {
        vec3 tCol = (vibrant && optTorchColorBleed) ? vec3(1.0, 0.65, 0.20) : vec3(1.0, 0.62, 0.18);
        torchLight += tCol * (placedTorch * 1.6 * smoothLighting);
    }

    // 5. Total Illumination & Material Color
    vec3 totalLight = ambientLight + celestialLightColor + torchLight;
    vec3 litColor = tex.rgb * totalLight;

    // 6. Water Column Beer-Lambert Absorption & Seabed Caustics (Submerged surfaces under water)
    if (isSubmerged) {
        // Depth below water surface (sea level at Y=44.5)
        float waterDepth = max(44.5 - fragWorldPos.y, 0.4);

        // Beer-Lambert wavelength-dependent light extinction:
        // Red light attenuates 6x faster than blue, producing authentic deep aquatic cyan/navy falloff
        vec3 waterAbsorption = vec3(0.26, 0.09, 0.038);
        vec3 beerTransmittance = exp(-waterDepth * waterAbsorption);

        // Attenuate light reaching the seabed
        litColor *= beerTransmittance;

        // Inscatter / water column scattering shifts submerged blocks towards the water's deep aquatic tint
        vec3 deepWaterTint = mix(vec3(0.008, 0.024, 0.055), vec3(0.018, 0.068, 0.15), isDay);
        float depthFog = 1.0 - exp(-waterDepth * 0.16);
        litColor = mix(litColor, deepWaterTint, depthFog * 0.72);

        // Dynamic Seabed Caustics (sharply focused in shallows, fading with depth)
        float t = pc.camPos.w * 2.0;
        vec2 p = fragWorldPos.xz;
        float c1 = sin(p.x * 2.4 + t * 1.5) * sin(p.y * 2.4 + t * 1.2);
        float c2 = sin(p.x * 4.8 - t * 1.8 + p.y * 2.6) * cos(p.y * 4.8 + t * 1.5);
        float caustic = pow(clamp(c1 * 0.5 + c2 * 0.5 + 0.5, 0.0, 1.0), 3.0);
        vec3 causticColor = mix(vec3(0.12, 0.35, 0.60), vec3(1.05, 0.98, 0.75), isDay);
        float causticFade = exp(-waterDepth * 0.28);
        litColor += caustic * 0.22 * blockShadow * max(sunIntensity, 0.20) * causticColor * causticFade;
    }

    // 7. Atmospheric Distance Fog & Golden Hour Scattering
    float dist = length(fragWorldPos - pc.camPos.xyz);
    bool cameraUnderwater = (pc.skyFog.w < 0.0);
    float fogEnd = abs(pc.skyFog.w);
    float fogStart = cameraUnderwater ? 2.0 : (fogEnd * 0.58);
    float fogFactor = clamp((dist - fogStart) / max(fogEnd - fogStart, 1.0), 0.0, 1.0);
    fogFactor = fogFactor * fogFactor;

    vec3 fogColor = cameraUnderwater ? vec3(0.02, 0.14, 0.38) : pc.skyFog.rgb;
    vec3 finalColor = mix(litColor, fogColor, fogFactor);

    if (cameraUnderwater) {
        finalColor = mix(finalColor, vec3(0.02, 0.12, 0.36), 0.32);
    }

    // Volumetric Crepuscular God Rays & Golden-Hour Haze (Vibrant Visuals Mode)
    if (vibrant && !cameraUnderwater) {
        vec3 godRays = computeGodRays(pc.camPos.xyz, fragWorldPos, L, isDay, goldenHour);
        finalColor += godRays;
    }

    // 8. Color Grading (Cinematic Complementary Style)
    if (vibrant && optColorGrading > 0) {
        if (optColorGrading == 1) { // Cinematic (Warm Golden-Hour Split Tone)
            finalColor = applyVibrance(finalColor, 0.28);
            finalColor = contrastSCurve(finalColor);
            float lum = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
            vec3 shadowTint = vec3(0.92, 0.96, 1.04);   // Subtle cool depth in shadows
            vec3 highlightTint = vec3(1.14, 1.05, 0.88); // Radiant warm golden highlights
            finalColor *= mix(shadowTint, highlightTint, smoothstep(0.12, 0.72, lum));
        } else if (optColorGrading == 2) { // Vibrant
            finalColor = applyVibrance(finalColor, 0.45);
            finalColor = contrastSCurve(finalColor);
        } else if (optColorGrading == 3) { // Warm
            finalColor = applyVibrance(finalColor, 0.22);
            finalColor *= vec3(1.08, 1.03, 0.90);
        } else if (optColorGrading == 4) { // Cool
            finalColor = applyVibrance(finalColor, 0.15);
            finalColor *= vec3(0.92, 0.97, 1.08);
        }
    } else if (vibrant) {
        // Default subtle warm tone
        finalColor = applyVibrance(finalColor, 0.18);
    }

    // 9. ACES Filmic Tonemapping
    finalColor = acesFilmicTonemap(finalColor);

    outColor = vec4(finalColor, tex.a);
}
