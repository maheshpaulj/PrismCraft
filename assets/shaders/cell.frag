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
    vec4 sunDir;        // xyz = normalized active light dir, w = signed intensity
    vec4 skyFog;        // xyz = fog color, w = fog distance (negative if underwater)
    vec4 camPos;        // xyz = camera pos, w = time
    vec4 playerPos;     // xyz = player pos, w = packed
    vec4 pointLight1;   // xyz = dynamic light 1 pos, w = intensity
    vec4 pointLight2;   // xyz = dynamic light 2 pos, w = intensity
    vec4 heldTorch;     // xyz = exact held torch world pos, w = active
    vec4 shaderOptions; // x = shadowQuality, y = waterQuality, z = colorGrading, w = packed settingsFlags
    vec4 dayInfo;       // x = isDay, y = sunHeight, z = 0, w = 0
    vec4 pointLight3;   // xyz = dynamic light 3 pos, w = intensity
    vec4 pointLight4;   // xyz = dynamic light 4 pos, w = intensity
} pc;

layout(location = 0) out vec4 outColor;

// Accurate piecewise sRGB to Linear EOTF
vec3 srgbToLinear(vec3 c) {
    bvec3 cutoff = lessThanEqual(c, vec3(0.04045));
    vec3 higher = pow((c + vec3(0.055)) / 1.055, vec3(2.4));
    vec3 lower  = c / 12.92;
    return mix(higher, lower, cutoff);
}

// Accurate piecewise Linear to sRGB OETF
vec3 linearToSrgb(vec3 c) {
    bvec3 cutoff = lessThanEqual(c, vec3(0.0031308));
    vec3 higher = 1.055 * pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.4)) - 0.055;
    vec3 lower  = c * 12.92;
    return clamp(mix(higher, lower, cutoff), 0.0, 1.0);
}

// Academy Color Encoding System (ACES) Hill / Narkowicz Fitted
// Transforms from linear sRGB to ACES AP1 wide gamut, evaluates RRT curve, and transforms back to sRGB.
// Ensures natural desaturation of highlights towards white without hue shifts or neon saturation burn.
vec3 acesFilmicTonemap(vec3 color) {
    const mat3 sRGB_2_AP1 = mat3(
        0.59719, 0.07600, 0.02840,
        0.35458, 0.90834, 0.13383,
        0.04823, 0.01566, 0.83777
    );
    const mat3 AP1_2_sRGB = mat3(
        1.60475, -0.10208, -0.00327,
        -0.53108,  1.10813, -0.07276,
        -0.07367, -0.00605,  1.07602
    );
    vec3 v = sRGB_2_AP1 * color;
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return clamp(AP1_2_sRGB * (a / b), 0.0, 1.0);
}

// Physically-Based Windowed Dynamic Point Light (Solid 7 blocks illumination)
vec3 evalPointLight(vec3 lightPos, float intensity, float radius, vec3 fragPos, vec3 N, bool vibrant, bool optTorchColorBleed) {
    if (intensity <= 0.001) return vec3(0.0);
    vec3 toLight = lightPos - fragPos;
    float dist = length(toLight);
    if (dist >= radius || dist < 0.001) return vec3(0.0);

    vec3 L = toLight / dist;
    float rawNdotL = dot(N, L);
    // Faces pointing away from torch are in their own shadow (no light bleed through blocks)
    if (rawNdotL <= -0.02) return vec3(0.0);

    float NdotL = max(rawNdotL, 0.0);
    float diffuse = NdotL * 0.75 + 0.25 * smoothstep(0.0, 0.45, rawNdotL);

    // Physically-based windowed attenuation: solid light for 7 blocks, smoothly tapering to zero at radius (15m)
    float normDist = dist / radius;
    float window = clamp(1.0 - normDist * normDist, 0.0, 1.0);
    float eDist = max(dist, 1.15);
    float atten = (window * window) / (eDist * 0.35 + 0.65);

    // Dynamic torch incandescent color gradient (core warm white -> amber -> deep ember)
    vec3 coreCol = vec3(1.0, 0.92, 0.78);
    vec3 midCol  = vec3(1.0, 0.62, 0.18);
    vec3 edgeCol = vec3(0.95, 0.35, 0.06);
    vec3 lightCol = mix(mix(coreCol, midCol, smoothstep(0.0, 0.35, normDist)), edgeCol, smoothstep(0.35, 1.0, normDist));
    if (!vibrant || !optTorchColorBleed) {
        lightCol = vec3(1.0, 0.65, 0.22);
    }

    return lightCol * (diffuse * atten * intensity * 2.4);
}

// Real-Time Dynamic Light Shadow: Player body casts shadow from held torch
float computePlayerTorchOcclusion(vec3 fragPos, vec3 torchPos, vec3 playerPos) {
    // 1. Steve's own body is never self-occluded by the cylinder shadow!
    vec2 toFragXZ = fragPos.xz - playerPos.xz;
    if (dot(toFragXZ, toFragXZ) < 0.26 && fragPos.y >= (playerPos.y - 0.1) && fragPos.y <= (playerPos.y + 2.0)) {
        return 1.0;
    }
    if (length(fragPos - torchPos) < 0.75) return 1.0; // Held torch and player hand are never self-occluded!

    vec3 rayDir = fragPos - torchPos;
    float rayLen = length(rayDir);
    if (rayLen < 0.2) return 1.0;

    // Test ray against player cylinder (radius 0.38m, height [playerPos.y + 0.35m, playerPos.y + 1.85m])
    vec2 pXZ = playerPos.xz;
    vec2 dXZ = rayDir.xz;
    float dLen2 = dot(dXZ, dXZ);
    if (dLen2 > 0.0001) {
        float t = clamp(dot(pXZ - torchPos.xz, dXZ) / dLen2, 0.0, 1.0);
        if (t > 0.06 && t < 0.94) {
            vec2 closestXZ = torchPos.xz + dXZ * t;
            float distXZ = length(closestXZ - pXZ);
            float hitY = torchPos.y + rayDir.y * t;
            if (hitY >= (playerPos.y + 0.35) && hitY <= (playerPos.y + 1.85)) {
                float shadow = smoothstep(0.04, 0.58, distXZ);
                return mix(0.38, 1.0, shadow);
            }
        }
    }
    return 1.0;
}

// High-frequency screen-space dither noise (Jimenez / Next Generation Post Processing)
float interleavedGradientNoise(vec2 screenPos) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

// 16 Vogel disk sample offsets uniformly distributed in the unit circle
const vec2 VOGEL_16[16] = vec2[](
    vec2( 0.1705,  0.0384),
    vec2(-0.2229,  0.2238),
    vec2( 0.0537, -0.3926),
    vec2( 0.2312,  0.3957),
    vec2(-0.4851, -0.1689),
    vec2( 0.4727, -0.3204),
    vec2(-0.1689,  0.6148),
    vec2(-0.3168, -0.6033),
    vec2( 0.6970,  0.2520),
    vec2(-0.6934,  0.3340),
    vec2( 0.3015, -0.7511),
    vec2( 0.2921,  0.7937),
    vec2(-0.7904, -0.3703),
    vec2( 0.8659, -0.2878),
    vec2(-0.4735,  0.8174),
    vec2(-0.1983, -0.9634)
);

// Evaluates PCF shadow on a specific cascade with precision bias and rotated Vogel disk filter
float evaluateCascadeShadow(int cascade, vec3 worldPos, vec3 N, vec3 L, mat2 rotMat, int quality) {
    float worldTexelSize = (cascade == 0) ? 0.024 : 0.082;
    float texelSize = 1.0 / 2048.0;

    // Slope-scaled normal offset bias tied to cascade world texel footprint
    float cosAlpha = clamp(dot(N, L), 0.0, 1.0);
    float sinAlpha = sqrt(max(1.0 - cosAlpha * cosAlpha, 0.0));
    float tanAlpha = sinAlpha / max(cosAlpha, 0.08);
    float normalBias = worldTexelSize * clamp(tanAlpha, 0.0, 1.8);
    vec3 biasedPos = worldPos + N * normalBias;

    vec4 sc = shadowUBO.lightViewProj[cascade] * vec4(biasedPos, 1.0);
    vec3 shadowCoords = sc.xyz / sc.w;

    if (shadowCoords.x < 0.001 || shadowCoords.x > 0.999 ||
        shadowCoords.y < 0.001 || shadowCoords.y > 0.999 ||
        shadowCoords.z < 0.0 || shadowCoords.z > 1.0) {
        return 1.0;
    }

    // Minimized depth bias since hardware depth bias is already applied in rasterizer
    float depthBias = max(0.00030 * (1.0 - cosAlpha), 0.00008);
    float compareDepth = shadowCoords.z - depthBias;

    // Radius in shadow map UV coordinates
    float filterRadius = (cascade == 0) ? (2.4 * texelSize) : (1.9 * texelSize);

    float shadow = 0.0;
    if (quality >= 3) {
        // High Quality: 16-tap rotated Vogel disk PCF (64 bilinear depth comparisons)
        for (int i = 0; i < 16; ++i) {
            vec2 offset = rotMat * VOGEL_16[i] * filterRadius;
            shadow += texture(shadowMap, vec4(shadowCoords.xy + offset, float(cascade), compareDepth));
        }
        shadow *= (1.0 / 16.0);
    } else if (quality == 2) {
        // Medium Quality: 10-tap rotated Vogel disk PCF
        for (int i = 0; i < 10; ++i) {
            vec2 offset = rotMat * VOGEL_16[i] * (filterRadius * 1.25);
            shadow += texture(shadowMap, vec4(shadowCoords.xy + offset, float(cascade), compareDepth));
        }
        shadow *= (1.0 / 10.0);
    } else {
        // Low Quality: 4-tap Poisson PCF
        vec2 p4[4] = vec2[](
            vec2(-0.45, -0.45), vec2( 0.45, -0.45),
            vec2(-0.45,  0.45), vec2( 0.45,  0.45)
        );
        for (int i = 0; i < 4; ++i) {
            vec2 offset = rotMat * p4[i] * filterRadius;
            shadow += texture(shadowMap, vec4(shadowCoords.xy + offset, float(cascade), compareDepth));
        }
        shadow *= 0.25;
    }

    return shadow;
}

// Hardware PCF Real-Time Cascaded Shadow Sampling with Cross-Cascade Seamless Blending
float sampleRealtimeShadow(vec3 worldPos, vec3 N, vec3 L, int quality) {
    if (dot(N, L) <= -0.05) {
        return 0.0; // Back-facing surface is entirely in self-shadow
    }

    float viewDist = length(worldPos - pc.camPos.xyz);
    float split0 = shadowUBO.cascadeSplits.x;
    float split1 = shadowUBO.cascadeSplits.y;

    if (viewDist > split1) {
        return 1.0;
    }

    // Per-pixel interleaved gradient noise rotation matrix
    float rotAngle = interleavedGradientNoise(gl_FragCoord.xy) * 6.2831853;
    float sinR = sin(rotAngle);
    float cosR = cos(rotAngle);
    mat2 rotMat = mat2(cosR, -sinR, sinR, cosR);

    // Cascade 0 with cross-cascade transition blend
    if (viewDist < split0) {
        float shadow0 = evaluateCascadeShadow(0, worldPos, N, L, rotMat, quality);
        float blendStart = max(split0 - 6.0, 0.0);
        if (viewDist > blendStart) {
            float blendFactor = (viewDist - blendStart) / (split0 - blendStart);
            float shadow1 = evaluateCascadeShadow(1, worldPos, N, L, rotMat, quality);
            return mix(shadow0, shadow1, clamp(blendFactor, 0.0, 1.0));
        }
        return shadow0;
    }

    // Cascade 1
    float shadow1 = evaluateCascadeShadow(1, worldPos, N, L, rotMat, quality);
    float fadeStart = max(split1 - 16.0, split0);
    if (viewDist > fadeStart) {
        float fade = (viewDist - fadeStart) / (split1 - fadeStart);
        return mix(shadow1, 1.0, clamp(fade, 0.0, 1.0));
    }
    return shadow1;
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

// Dynamic Point Light Block Shadowing (Terrain & Block Occlusion)
float testTorchBlockOcclusion(vec3 fragPos, vec3 lightPos) {
    vec3 toLight = lightPos - fragPos;
    float dist = length(toLight);
    if (dist < 2.5) return 1.0;
    vec3 p = mix(fragPos, lightPos, 0.5);
    float th = getTerrainHeight(p.xz);
    if (th > (p.y + 0.22)) {
        return 0.15; // Terrain/block shadow
    }
    return 1.0;
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
    // Early out if facing away from the sun or sun is down
    if (cosTheta < 0.20 || isDay < 0.05 || L.y < 0.02) return vec3(0.0);

    float phase = pow(max(cosTheta * 0.5 + 0.5, 0.0), 3.6) * 1.8 + 0.12;

    vec3 goldenSun = vec3(1.85, 1.32, 0.68);
    vec3 midDaySun = vec3(1.15, 1.08, 0.95);
    vec3 activeSun = mix(midDaySun, goldenSun, goldenHour);

    // 1. Sun Occlusion by Distant Mountains / LOD Hills (Tracing up to 450m)
    float sunOcclusion = 1.0;
    for (int s = 1; s <= 3; ++s) {
        float testDist = float(s) * 150.0;
        vec3 testP = rayOrigin + L * testDist;
        float mountainY = getTerrainHeight(testP.xz);
        if (testP.y < mountainY) {
            float diff = mountainY - testP.y;
            sunOcclusion = min(sunOcclusion, clamp(1.0 - diff * 0.25, 0.0, 1.0));
        }
    }
    if (sunOcclusion <= 0.001) return vec3(0.0);

    const int SAMPLES = 3;
    float inscatterSum = 0.0;
    float maxDist = min(rayDist, 160.0);

    for (int i = 0; i < SAMPLES; ++i) {
        float frac = (float(i) + 0.5) / float(SAMPLES);
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

        // C. Cloud shadow at point p
        float cloudLight = sampleCloudShadow(p, L);

        inscatterSum += effectiveShadow * cloudLight;
    }

    float shaftIntensity = inscatterSum / float(SAMPLES);
    float distFade = smoothstep(6.0, 45.0, rayDist) * (1.0 - smoothstep(120.0, 260.0, rayDist) * 0.4);
    float beamPower = (0.35 + goldenHour * 0.65) * isDay;

    return activeSun * (shaftIntensity * phase * beamPower * distFade * sunOcclusion);
}

// -------------------------------------------------------------
// Analytical Line Integral of Exponential Height Density Profile along 3D View Ray
// -------------------------------------------------------------
float integrateExponentialDensity(float camY, float fragY, float dist, float baseHeight, float scaleHeight) {
    float deltaY = fragY - camY;
    float normCamY = (camY - baseHeight) / scaleHeight;
    float rhoCam = exp(-clamp(normCamY, -3.0, 8.0));
    if (abs(deltaY) < 0.001) {
        return rhoCam * dist;
    }
    float normDeltaY = deltaY / scaleHeight;
    float integralFactor = (1.0 - exp(-clamp(normDeltaY, -8.0, 8.0))) / normDeltaY;
    return rhoCam * dist * max(integralFactor, 0.0);
}

// -------------------------------------------------------------
// Physically-Inspired Atmospheric Perspective & Directional Air-Light Scattering
// -------------------------------------------------------------
vec3 applyAtmosphericPerspective(vec3 surfaceColor, vec3 fragPos, vec3 camPos, vec3 L, vec3 skyFogColor, float isDay, float goldenHour, int optAtmosFog, float fogParamDensity, float fogEndDist) {
    vec3 rayDir = fragPos - camPos;
    float rayDist = length(rayDir);
    if (rayDist < 0.001) return surfaceColor;
    vec3 V = rayDir / rayDist;

    // Tunable near threshold where atmospheric scattering begins (0 to 14m remains 100% crisp)
    float fogStart = 14.0;
    float effectiveDist = max(rayDist - fogStart, 0.0);
    if (effectiveDist <= 0.0) return surfaceColor;

    // Tunable atmospheric density multiplier based on options and dayInfo
    float densitySetting = (optAtmosFog == 0) ? 0.35 : ((optAtmosFog == 2) ? 1.60 : 1.00);
    float userDensity = (fogParamDensity > 0.01) ? fogParamDensity : 1.0;
    float totalDensityMult = densitySetting * userDensity;

    // Layer 1: Broad Tropospheric Planetary Air-Light (Continuous contrast reduction on distant terrain)
    float opticalDepthRayleigh = integrateExponentialDensity(camPos.y, fragPos.y, effectiveDist, 62.0, 95.0) * (0.0035 * totalDensityMult);

    // Layer 2: Low-Altitude Ground/Valley Haze (Soft aerial mist in lowlands and valleys)
    float opticalDepthHaze = integrateExponentialDensity(camPos.y, fragPos.y, effectiveDist, 52.0, 26.0) * (0.0045 * totalDensityMult);

    float totalTau = opticalDepthRayleigh + opticalDepthHaze;

    // Horizon Chunk Boundary Dissolve (smoothly dissolves terrain into sky dome at the render distance edge)
    float horizonDissolveStart = fogEndDist * 0.78;
    if (rayDist > horizonDissolveStart) {
        float hNorm = (rayDist - horizonDissolveStart) / max(fogEndDist - horizonDissolveStart, 1.0);
        totalTau += pow(clamp(hNorm, 0.0, 1.0), 2.4) * 5.0;
    }

    // Transmittance via Beer-Lambert extinction
    float transmittance = exp(-totalTau);

    // Directional In-Scattered Air-Light Color
    vec3 linearHorizonSky = srgbToLinear(skyFogColor);
    // Upper zenith sky is deeper blue
    vec3 zenithSky = linearHorizonSky * vec3(0.68, 0.84, 1.22);
    vec3 baseAirLight = mix(linearHorizonSky, zenithSky, clamp(V.y * 0.55 + 0.15, 0.0, 1.0));

    // Forward Solar Mie Scattering Phase Function (radiant golden haze when looking toward sun)
    float cosTheta = dot(V, L);
    float forwardPhase = pow(max(cosTheta * 0.5 + 0.5, 0.0), 3.6);
    vec3 goldenHaze = mix(vec3(1.12, 1.04, 0.95), vec3(1.85, 1.30, 0.65), goldenHour);
    vec3 inscatterRadiance = mix(baseAirLight, baseAirLight * goldenHaze, forwardPhase * 0.65 * isDay);

    return mix(inscatterRadiance, surfaceColor, transmittance);
}

void main() {
    // 1. Self-illuminated celestial bodies (Sun, Moon) & HUD overlays
    if (abs(pc.sunDir.w) >= 1.9) {
        vec3 emissive = fragColor * 3.5;
        outColor = vec4(emissive, 1.0);
        return;
    }

    vec4 texSample = texture(texSampler, fragTexCoord);
    if (texSample.a < 0.35) {
        discard;
    }

    // Convert authored sRGB texture albedo into true physical linear reflectance
    vec3 albedo = srgbToLinear(texSample.rgb);

    // Unpack Shader & Graphics Options
    bool vibrant = (pc.sunDir.w > 0.0);
    int optShadowQuality = int(pc.shaderOptions.x + 0.5);
    int optWaterQuality = int(pc.shaderOptions.y + 0.5);
    int optColorGrading = int(pc.shaderOptions.z + 0.5);
    int settingsFlags = int(pc.shaderOptions.w + 0.5);
    bool optPlayerShadow = (settingsFlags & 1) != 0;
    bool optClouds = (settingsFlags & 2) != 0;
    bool optCloudShadows = (settingsFlags & 4) != 0;
    bool optSmoothLighting = (settingsFlags & 8) != 0;
    bool optTorchColorBleed = (settingsFlags & 16) != 0;
    int optAtmosFog = (settingsFlags >> 5) & 3;

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(pc.sunDir.xyz);
    vec3 V = normalize(pc.camPos.xyz - fragWorldPos);
    float NdotL = dot(N, L);

    float sunIntensity = abs(pc.sunDir.w);

    // Day / Night Factor: 1.0 = full daylight, 0.0 = dark night
    float isDay = pc.dayInfo.x;
    float actualSunY = pc.dayInfo.y;

    // 2. Direct Sunlight / Moonlight Shading with Real-Time & Block Shadows
    float blockShadow = fragColor.r;        // 1.0 = exposed to sky, 0.08 = beneath terrain/overhangs
    float smoothLighting = optSmoothLighting ? fragColor.g : 1.0;

    float viewDist = length(fragWorldPos - pc.camPos.xyz);
    float rtShadow = 1.0;
    if (vibrant && (optShadowQuality > 0)) {
        rtShadow = sampleRealtimeShadow(fragWorldPos, N, L, optShadowQuality);
    }

    // Seamless blend between real-time CSM and distant terrain static shadow
    float split1 = shadowUBO.cascadeSplits.y;
    float fadeToDistant = clamp((viewDist - (split1 - 16.0)) / 16.0, 0.0, 1.0);
    float combinedShadow = (vibrant && (optShadowQuality > 0))
                         ? mix(rtShadow, blockShadow, fadeToDistant)
                         : blockShadow;

    // Natural diffuse falloff with soft wrap at terminator to prevent harsh black pixel boundaries
    float diff = clamp((NdotL + 0.06) / 1.06, 0.0, 1.0);
    float directFactor = diff * combinedShadow * sunIntensity;

    // Dynamic Cloud Shadows on Terrain: Synchronized 1:1 with sky clouds
    if (vibrant && optCloudShadows && (optShadowQuality > 0) && isDay > 0.1 && L.y > 0.03) {
        float cShadow = sampleCloudShadow(fragWorldPos, L);
        directFactor *= cShadow;
    }

    // Golden-hour factor: peaks when actual sun is at horizon angles
    float goldenHour = smoothstep(0.40, 0.02, actualSunY) * step(-0.05, actualSunY);

    // Physical solar spectral irradiance in linear units
    // Midday: warm solar white (~5500K)
    vec3 midDaySun = vec3(2.60, 2.50, 2.30);
    // Sunset / Golden Hour: rich radiant amber gold
    vec3 goldenSun = vec3(3.20, 2.10, 0.95);
    vec3 sunColor  = mix(midDaySun, goldenSun, goldenHour);
    // Moonlight: soft lunar silver
    vec3 moonColor = vec3(0.20, 0.28, 0.42);
    vec3 celestialLightColor = mix(moonColor, sunColor, isDay) * directFactor;

    // 3. Foliage Translucency / Backlight (Subsurface transmission through leaves and grass)
    bool isFoliage = (texSample.g > texSample.r * 1.05 && texSample.g > texSample.b * 1.02) || (texSample.a < 0.95);
    vec3 foliageGlow = vec3(0.0);
    if (vibrant && isFoliage) {
        // Natural canopy hue and value variation per world chunk/tree position
        vec3 leafHash = sin(floor(fragWorldPos * 0.35) * 1.57 + vec3(0.0, 1.8, 3.4));
        float hueShift = leafHash.x * 0.05;
        float valShift = leafHash.y * 0.06;
        albedo = albedo * vec3(1.0 + valShift - hueShift * 0.5, 1.0 + hueShift, 0.94 - hueShift);

        // Chlorophyll forward scattering & two-sided wrap diffuse backlight
        float sss = pow(clamp(dot(V, -L), 0.0, 1.0), 3.0);
        float leafWrap = clamp((dot(N, L) + 0.4) / 1.4, 0.0, 1.0);
        vec3 chlorophyllTint = albedo * vec3(1.35, 1.55, 0.45);
        float foliageTransmission = (sss * 0.65 + leafWrap * 0.35) * combinedShadow * isDay * sunIntensity;
        foliageGlow = chlorophyllTint * foliageTransmission;
    }

    // 4. Ambient Skylight & Contact Occlusion in Linear Space
    float skyHemisphere = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambientGround = mix(vec3(0.20, 0.24, 0.18), vec3(0.28, 0.22, 0.14), goldenHour);
    vec3 ambientSky    = mix(vec3(0.42, 0.54, 0.74), vec3(0.52, 0.40, 0.28), goldenHour);
    vec3 daySkyAmbient = mix(ambientGround, ambientSky, skyHemisphere);
    vec3 nightSkyAmbient = mix(vec3(0.045, 0.060, 0.095), vec3(0.070, 0.095, 0.150), skyHemisphere);
    vec3 ambientLight = mix(nightSkyAmbient, daySkyAmbient, isDay);

    // Directional ambient response: vertical walls facing away from the sun receive cooler, darker ambient (~25% darker)
    vec2 Lxz = (length(L.xz) > 0.001) ? normalize(L.xz) : vec2(0.0, 1.0);
    vec2 Nxz = (length(N.xz) > 0.001) ? normalize(N.xz) : vec2(0.0, 0.0);
    float horizFacing = dot(Nxz, Lxz); // +1.0 facing sun, -1.0 facing away
    float wallWeight = 1.0 - abs(N.y); // 1.0 for vertical walls, 0.0 for horizontal floors/ceilings
    float dirAmbientMod = 1.0 + (horizFacing * 0.25) * wallWeight * isDay;
    ambientLight *= dirAmbientMod;

    // Calibrated contact AO for grounded corners, crevices, and wall seams
    float skyOcc = mix(0.40 * smoothstep(0.02, 0.18, blockShadow), 1.0, clamp(blockShadow * 1.15, 0.0, 1.0));
    float optAoStrength = (pc.shaderOptions.z > 0.001) ? pc.shaderOptions.z : 1.0;
    float rawAo = optSmoothLighting ? pow(clamp(smoothLighting, 0.0, 1.0), 1.35) : 1.0;
    float ao = mix(1.0, rawAo, optAoStrength);
    float contactAmbient = mix(0.32, 1.0, ao);
    float contactDirect  = mix(0.55, 1.0, ao);

    ambientLight = ambientLight * skyOcc * contactAmbient;
    celestialLightColor *= contactDirect;

    // 5. Dynamic Point Lights with Smooth Inverse-Square Falloff (Torches)
    vec3 torchLight = vec3(0.0);

    // A. Handheld Torch (in player's right hand)
    if (pc.heldTorch.w > 0.001) {
        float playerOcc = optPlayerShadow ? computePlayerTorchOcclusion(fragWorldPos, pc.heldTorch.xyz, pc.playerPos.xyz) : 1.0;
        float blockOcc = testTorchBlockOcclusion(fragWorldPos, pc.heldTorch.xyz);
        torchLight += evalPointLight(pc.heldTorch.xyz, pc.heldTorch.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * (playerOcc * blockOcc * smoothLighting);
    }

    // B. Nearest Placed and Dropped Torches (dynamic per-pixel point lights 1 to 4)
    if (pc.pointLight1.w > 0.001) {
        float blockOcc = testTorchBlockOcclusion(fragWorldPos, pc.pointLight1.xyz);
        torchLight += evalPointLight(pc.pointLight1.xyz, pc.pointLight1.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * (blockOcc * smoothLighting);
    }
    if (pc.pointLight2.w > 0.001) {
        float blockOcc = testTorchBlockOcclusion(fragWorldPos, pc.pointLight2.xyz);
        torchLight += evalPointLight(pc.pointLight2.xyz, pc.pointLight2.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * (blockOcc * smoothLighting);
    }
    if (pc.pointLight3.w > 0.001) {
        float blockOcc = testTorchBlockOcclusion(fragWorldPos, pc.pointLight3.xyz);
        torchLight += evalPointLight(pc.pointLight3.xyz, pc.pointLight3.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * (blockOcc * smoothLighting);
    }
    if (pc.pointLight4.w > 0.001) {
        float blockOcc = testTorchBlockOcclusion(fragWorldPos, pc.pointLight4.xyz);
        torchLight += evalPointLight(pc.pointLight4.xyz, pc.pointLight4.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * (blockOcc * smoothLighting);
    }

    // C. Distant Placed World Torches (baked into vertex color b-channel)
    bool isSubmerged = (fragColor.b >= 1.5);
    float placedTorch = isSubmerged ? (fragColor.b - 2.0) : fragColor.b;
    if (placedTorch > 0.01) {
        vec3 tCol = (vibrant && optTorchColorBleed) ? vec3(2.0, 1.25, 0.35) : vec3(1.8, 1.15, 0.30);
        torchLight += tCol * (placedTorch * 1.8 * smoothLighting);
    }

    // 6. Total Linear Illumination & Surface Radiance
    vec3 totalLight = ambientLight + celestialLightColor + torchLight;
    vec3 surfaceRadiance = albedo * totalLight + foliageGlow;

    // 7. Water Column Beer-Lambert Absorption & Seabed Caustics (Submerged surfaces under water)
    if (isSubmerged) {
        float waterDepth = max(44.5 - fragWorldPos.y, 0.4);
        vec3 waterAbsorption = vec3(0.26, 0.09, 0.038);
        vec3 beerTransmittance = exp(-waterDepth * waterAbsorption);
        surfaceRadiance *= beerTransmittance;

        vec3 deepWaterTint = mix(vec3(0.004, 0.015, 0.045), vec3(0.008, 0.035, 0.095), isDay);
        float depthFog = 1.0 - exp(-waterDepth * 0.16);
        surfaceRadiance = mix(surfaceRadiance, deepWaterTint, depthFog * 0.72);

        float t = pc.camPos.w * 2.0;
        vec2 p = fragWorldPos.xz;
        float c1 = sin(p.x * 2.4 + t * 1.5) * sin(p.y * 2.4 + t * 1.2);
        float c2 = sin(p.x * 4.8 - t * 1.8 + p.y * 2.6) * cos(p.y * 4.8 + t * 1.5);
        float caustic = pow(clamp(c1 * 0.5 + c2 * 0.5 + 0.5, 0.0, 1.0), 3.0);
        vec3 causticColor = mix(vec3(0.12, 0.35, 0.60), vec3(1.05, 0.98, 0.75), isDay);
        float causticFade = exp(-waterDepth * 0.28);
        surfaceRadiance += caustic * 0.22 * blockShadow * max(sunIntensity, 0.20) * causticColor * causticFade;
    }

    // 8. Atmospheric Aerial Perspective & Distance Fog in Linear Space
    bool cameraUnderwater = (pc.skyFog.w < 0.0);
    float fogEnd = abs(pc.skyFog.w);
    vec3 linearSceneColor = surfaceRadiance;

    if (cameraUnderwater) {
        float dist = length(fragWorldPos - pc.camPos.xyz);
        float uFactor = smoothstep(2.0, 24.0, dist);
        linearSceneColor = mix(surfaceRadiance, vec3(0.005, 0.035, 0.12), uFactor);
        linearSceneColor = mix(linearSceneColor, vec3(0.005, 0.025, 0.08), 0.32);
    } else {
        linearSceneColor = applyAtmosphericPerspective(
            surfaceRadiance,
            fragWorldPos,
            pc.camPos.xyz,
            L,
            pc.skyFog.rgb,
            isDay,
            goldenHour,
            optAtmosFog,
            pc.dayInfo.w,
            fogEnd
        );
    }

    // Volumetric Crepuscular God Rays in linear space
    if (vibrant && !cameraUnderwater) {
        vec3 godRays = computeGodRays(pc.camPos.xyz, fragWorldPos, L, isDay, goldenHour);
        linearSceneColor += godRays;
    }

    // 9. Output linear HDR radiance directly to HDR buffer (master tonemapping handled in post-processing pass)
    outColor = vec4(linearSceneColor, texSample.a);
}
