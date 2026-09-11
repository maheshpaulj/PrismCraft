#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor; // r = water depth factor (0..1), g = sunlight, b = torchlight
layout(location = 3) in vec3 fragWorldPos;

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform sampler2DArrayShadow shadowMap;

layout(std140, binding = 2) uniform ShadowUBO {
    mat4 lightViewProj[2];
    vec4 cascadeSplits; // x = split0, y = split1
} shadowUBO;

layout(binding = 3) uniform sampler2D ssrSampler;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 sunDir;     // xyz = normalized sun dir, w = sun intensity (positive = vibrant visuals ON, negative = OFF)
    vec4 skyFog;     // xyz = fog color, w = fog distance (negative if underwater)
    vec4 camPos;     // xyz = camera pos, w = time
    vec4 playerPos;  // xyz = player pos, w = packed
    vec4 pointLight1;
    vec4 pointLight2;
    vec4 heldTorch;
    vec4 shaderOptions; // x = shadowQuality, y = waterQuality, z = colorGrading, w = packed settingsFlags
    vec4 dayInfo;       // x = isDay, y = sunHeight, z = 0, w = 0
    vec4 pointLight3;
    vec4 pointLight4;
} pc;

layout(location = 0) out vec4 outColor;

// Accurate piecewise sRGB to Linear EOTF
vec3 srgbToLinear(vec3 c) {
    bvec3 cutoff = lessThanEqual(c, vec3(0.04045));
    vec3 higher = pow((c + vec3(0.055)) / 1.055, vec3(2.4));
    vec3 lower  = c / 12.92;
    return mix(higher, lower, cutoff);
}

// -------------------------------------------------------------
// Fast 3D Noise for Reflected 3D Cumulus Clouds
// -------------------------------------------------------------
float hash3D(vec3 p) {
    p = fract(p * vec3(443.8975, 397.2973, 491.1871));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = hash3D(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash3D(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash3D(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash3D(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash3D(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash3D(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash3D(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash3D(i + vec3(1.0, 1.0, 1.0));

    return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
               mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

float cloudFBM(vec3 p) {
    float f = 0.0;
    f += 0.5200 * noise3D(p); p = p * 2.08 + vec3(1.3, 0.7, 2.1);
    f += 0.2800 * noise3D(p); p = p * 2.12 + vec3(2.1, 1.4, 0.9);
    f += 0.1400 * noise3D(p);
    return f;
}

float sampleRealtimeShadow(vec3 worldPos, vec3 N, vec3 L) {
    float viewDist = length(worldPos - pc.camPos.xyz);
    int cascade = 0;
    if (viewDist > shadowUBO.cascadeSplits.x) cascade = 1;
    if (viewDist > shadowUBO.cascadeSplits.y) return 1.0;

    vec3 biasedPos = worldPos + N * (cascade == 0 ? 0.04 : 0.12);
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

    vec2 pMacro = ws * 0.00025;
    float macro = sin(pMacro.x * 3.14 + cos(pMacro.y * 2.5)) * cos(pMacro.y * 3.14) * 0.5 + 0.5;
    float coverage = smoothstep(0.42, 0.65, macro);
    return 1.0 - coverage * 0.65;
}


// -------------------------------------------------------------
// Real-Time Physically-Based Screen-Space Reflection (SSR) & Sky
// -------------------------------------------------------------
vec3 traceSSR(vec3 origin, vec3 R, vec3 L, float isDay, float sunIntensity, int waterQuality, float dither) {
    float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);

    // 1. Physically-Based Atmospheric Sky Dome Gradient (matching sky.frag)
    vec3 skyZenith  = mix(vec3(0.015, 0.035, 0.10), vec3(0.18, 0.42, 0.82), isDay);
    vec3 horizonDay = mix(vec3(0.58, 0.72, 0.88), vec3(1.35, 0.95, 0.52), goldenHour * 0.75);
    vec3 skyHorizon = mix(srgbToLinear(pc.skyFog.rgb), horizonDay, isDay);
    vec3 proceduralSky = mix(skyHorizon, skyZenith, clamp(max(R.y, 0.0) * 1.6, 0.0, 1.0));

    // Solar atmospheric forward glare reflected in sky
    float cosSun = dot(R, L);
    if (cosSun > 0.0 && isDay > 0.05) {
        float sunReflectionGlare = pow(cosSun, 28.0) * 2.2 * sunIntensity;
        vec3 sunGlareColor = mix(vec3(1.15, 1.08, 0.95), vec3(1.85, 1.30, 0.65), goldenHour);
        proceduralSky += sunGlareColor * sunReflectionGlare;
    }

    // Reflected 3D Volumetric Cumulus Clouds in the sky dome
    if (R.y > 0.012 && waterQuality >= 1) {
        float tCloud = (195.0 - origin.y) / max(R.y, 0.02);
        if (tCloud > 0.0 && tCloud < 7000.0) {
            vec3 pCloud = origin + R * tCloud;
            vec2 wind = vec2(pc.camPos.w * 3.6, pc.camPos.w * 1.4);
            vec3 ws = pCloud + vec3(wind.x, 0.0, wind.y);

            float dH = length(pCloud.xz - pc.camPos.xz);
            float yCurv = pCloud.y + (dH * dH) / (2.0 * 95000.0);

            if (yCurv >= 180.0 && yCurv <= 330.0) {
                float macroNoise = cloudFBM(ws * 0.00028);
                if (macroNoise > 0.36) {
                    float cDensity = smoothstep(0.36, 0.60, macroNoise);
                    if (cDensity > 0.0) {
                        float cAlpha = clamp(cDensity * 1.6, 0.0, 1.0);
                        float silver = pow(max(cosSun * 0.5 + 0.5, 0.0), 3.0) * 0.65 + 0.85;
                        vec3 goldenCloud = mix(vec3(1.40, 1.30, 1.15), vec3(2.20, 1.45, 0.70), goldenHour);
                        vec3 cloudLit = mix(vec3(0.30, 0.38, 0.50), goldenCloud * silver, isDay);
                        proceduralSky = mix(proceduralSky, cloudLit, cAlpha * 0.88);
                    }
                }
            }
        }
    }

    vec3 reflectedColor = proceduralSky;
    bool hitIsland = false;

    // 2. High-Precision 3D Raymarch to intersect with above-water island terrain
    if (waterQuality >= 1) {
        // Ensure reflection ray points above the water plane (water level ~44.0)
        vec3 safeR = R;
        if (safeR.y < 0.006) safeR = normalize(vec3(safeR.x, 0.006, safeR.z));

        float dist = 0.8 + dither * 0.6;
        int maxSteps = (waterQuality >= 2) ? 38 : 24;
        float maxDist = (waterQuality >= 2) ? 500.0 : 300.0;

        for (int i = 0; i < maxSteps; ++i) {
            if (dist >= maxDist) break;
            vec3 P = origin + safeR * dist;

            // Only test above-water terrain (P.y >= 43.6).
            // This strictly eliminates any false hits on the underwater seabed!
            if (P.y >= 43.6) {
                float groundH = getTerrainHeight(P.xz);
                float canopyH = (groundH > 50.0) ? (groundH + 4.8) : groundH;

                if (P.y <= canopyH) {
                    // Ray hit the above-water island terrain or tree canopy!
                    // Project 3D hit point into screen space coordinates
                    vec4 clip = pc.mvp * vec4(P, 1.0);
                    if (clip.w > 0.0) {
                        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
                        if (uv.x >= 0.001 && uv.x <= 0.999 && uv.y >= 0.001 && uv.y <= 0.999) {
                            // Sample the actual rendered island (trees, grass, stone, sand) from the screen texture!
                            vec3 islandCol = texture(ssrSampler, uv).rgb;
                            vec2 edgeDist = min(uv, 1.0 - uv);
                            float edgeFade = smoothstep(0.0, 0.06, min(edgeDist.x, edgeDist.y));
                            float distFade = 1.0 - smoothstep(maxDist * 0.60, maxDist, dist);

                            reflectedColor = mix(proceduralSky, islandCol, edgeFade * distFade);
                            hitIsland = true;
                            break;
                        }
                    }
                }
            }

            // Exponential step scaling for rapid, efficient distance traversal
            float stepSize = 0.8 + 0.085 * dist;
            dist += stepSize;
        }
    }

    // 3. Screen-Space Reflection of Sky and Clouds (when ray doesn't hit island)
    if (!hitIsland) {
        vec3 safeR = R;
        if (safeR.y < 0.005) safeR = normalize(vec3(safeR.x, 0.005, safeR.z));
        vec3 skyTarget = origin + safeR * 250.0;
        vec4 skyClip = pc.mvp * vec4(skyTarget, 1.0);
        if (skyClip.w > 0.0) {
            vec2 skyUV = (skyClip.xy / skyClip.w) * 0.5 + 0.5;
            if (skyUV.x >= 0.001 && skyUV.x <= 0.999 && skyUV.y >= 0.001 && skyUV.y <= 0.999) {
                // Sample rendered clouds and sky from the screen
                vec3 screenSky = texture(ssrSampler, skyUV).rgb;
                vec2 edgeDist = min(skyUV, 1.0 - skyUV);
                float edgeFade = smoothstep(0.0, 0.08, min(edgeDist.x, edgeDist.y));
                reflectedColor = mix(proceduralSky, screenSky, edgeFade * 0.90);
            }
        }
    }

    return reflectedColor;
}

// -------------------------------------------------------------
// Clean Horizon Distance Fog: Smoothly fades chunks into horizon at edge of render distance
// -------------------------------------------------------------
vec3 applyHorizonDistanceFog(vec3 surfaceColor, vec3 fragPos, vec3 camPos, vec3 L, vec3 skyFogColor, float isDay, float goldenHour, int optAtmosFog, float fogEndDist) {
    vec3 rayDir = fragPos - camPos;
    float rayDist = length(rayDir);
    if (rayDist < 0.001) return surfaceColor;
    vec3 V = rayDir / rayDist;

    // Horizon fade start: 70% of render distance by default (55% if dense, 88% if off)
    float startRatio = 0.70;
    if (optAtmosFog == 0) startRatio = 0.88;
    else if (optAtmosFog == 2) startRatio = 0.55;

    float fogStart = fogEndDist * startRatio;

    // Everything closer than fogStart has ZERO fog: 100% crisp, vibrant, high-contrast nearby world!
    if (rayDist <= fogStart) {
        return surfaceColor;
    }

    // Smooth cubic Hermite interpolation between fogStart and fogEndDist
    float fogNorm = clamp((rayDist - fogStart) / max(fogEndDist - fogStart, 1.0), 0.0, 1.0);
    float fogFactor = smoothstep(0.0, 1.0, fogNorm);

    // Directional In-Scattered Air-Light Color matching sky dome at horizon
    vec3 linearHorizonSky = srgbToLinear(skyFogColor);
    vec3 zenithSky = linearHorizonSky * vec3(0.68, 0.84, 1.22);
    vec3 baseAirLight = mix(linearHorizonSky, zenithSky, clamp(V.y * 0.55 + 0.15, 0.0, 1.0));

    // Smooth forward solar warming without any hard cone cutoff or circle artifacts
    float cosTheta = dot(V, L);
    if (cosTheta > 0.0 && isDay > 0.05) {
        float forwardPhase = pow(cosTheta, 4.0) * 0.35;
        vec3 goldenHaze = mix(vec3(1.10, 1.04, 0.95), vec3(1.65, 1.25, 0.70), goldenHour);
        baseAirLight = mix(baseAirLight, baseAirLight * goldenHaze, forwardPhase);
    }

    return mix(surfaceColor, baseAirLight, fogFactor);
}


// -------------------------------------------------------------
// GGX Microfacet Specular Sun Reflection
// -------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265 * denom * denom;
    return a2 / max(denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx2 * ggx1;
}

void main() {
    bool vibrant = (pc.sunDir.w > 0.0);
    float sunIntensity = abs(pc.sunDir.w);
    bool cameraUnderwater = (pc.skyFog.w < 0.0);
    float depthFactor = clamp(fragColor.r, 0.0, 1.0); // 0.0 = shore/shallow, 1.0 = deep
    float isDay = pc.dayInfo.x;
    int optWaterQuality = int(pc.shaderOptions.y + 0.5);
    int settingsFlags = int(pc.shaderOptions.w + 0.5);
    int optAtmosFog = (settingsFlags >> 5) & 3;

    vec3 V = normalize(pc.camPos.xyz - fragWorldPos);
    bool viewingFromBelow = (fragWorldPos.y > pc.camPos.y) || (dot(fragNormal, V) < 0.0);

    // -------------------------------------------------------------
    // Classic Vanilla Mode: Flat Blue Water with Animated Texture
    // -------------------------------------------------------------
    if (!vibrant || optWaterQuality == 0) {
        vec3 N = normalize(fragNormal);
        float NdotV = clamp(dot(N, V), 0.0, 1.0);

        vec3 vanillaBlue = mix(vec3(0.08, 0.22, 0.45), vec3(0.18, 0.45, 0.78), isDay);
        float ripple = sin(fragWorldPos.x * 2.2 + pc.camPos.w * 1.8) *
                       cos(fragWorldPos.z * 2.2 + pc.camPos.w * 1.5) * 0.035;
        vanillaBlue += ripple;

        float fresnel = 0.04 + 0.45 * pow(1.0 - NdotV, 3.0);
        vec3 skyRefl = mix(pc.skyFog.rgb, vec3(0.40, 0.65, 0.95), isDay);
        vec3 color = mix(vanillaBlue, skyRefl, fresnel);

        color += vec3(1.0, 0.65, 0.22) * fragColor.b;
        if (pc.heldTorch.w > 0.001) {
            float dHeld = length(fragWorldPos - pc.heldTorch.xyz);
            float atten = clamp(1.0 - dHeld / 14.0, 0.0, 1.0);
            color += vec3(1.0, 0.65, 0.22) * (atten * atten * 1.8 * pc.heldTorch.w);
        }

        float horizDist = length(fragWorldPos.xz - pc.camPos.xz);
        float fogEnd = abs(pc.skyFog.w);
        float fogStart = cameraUnderwater ? 2.0 : (fogEnd * 0.78);
        float fogNorm = clamp((horizDist - fogStart) / max(fogEnd - fogStart, 1.0), 0.0, 1.0);
        float fogFactor = smoothstep(0.0, 1.0, fogNorm);
        vec3 fogCol = cameraUnderwater ? vec3(0.02, 0.14, 0.40) : pc.skyFog.rgb;
        color = mix(color, fogCol, fogFactor);

        outColor = vec4(color, 0.65);
        return;
    }

    // -------------------------------------------------------------
    // 1. Physically-Calibrated Beer-Lambert Depth Absorption
    // -------------------------------------------------------------
    // Red absorbs rapidly (0.38/m), green moderately (0.11/m), blue penetrates deepest (0.035/m)
    vec3 sigmaA = vec3(0.38, 0.11, 0.035);
    float waterDepthMeters = depthFactor * 8.5;
    vec3 transmittance = exp(-waterDepthMeters * sigmaA);

    // Deep ocean bed: rich oceanic sapphire navy (from reference image)
    vec3 deepWaterRadiance = mix(vec3(0.003, 0.010, 0.028), vec3(0.007, 0.026, 0.068), isDay);
    // Shallow waterbed: luminous translucent turquoise / aquamarine
    vec3 shallowWaterRadiance = mix(vec3(0.012, 0.042, 0.055), vec3(0.045, 0.180, 0.210), isDay);

    vec3 waterBedColor = mix(deepWaterRadiance, shallowWaterRadiance, transmittance);

    // Soft shoreline edge blending without harsh block clipping
    float shoreEdge = smoothstep(0.012, 0.11, depthFactor);
    float baseAlpha = mix(0.28, 0.90, smoothstep(0.03, 0.65, depthFactor)) * shoreEdge;

    // -------------------------------------------------------------
    // 2. Broad Cohesive Ocean Swells (Wavelengths 6.5m to 28m)
    // Coherent movement across multiple chunks, NOT noisy per-block chop
    // -------------------------------------------------------------
    float t = pc.camPos.w * 0.85;
    vec2 pos = fragWorldPos.xz;
    float distToCam = length(fragWorldPos - pc.camPos.xyz);

    // Swell 1: Primary broad ocean roller (Wavelength = 28m)
    vec2  d1 = normalize(vec2(0.82, 0.57));
    float k1 = 0.2244;
    float a1 = 0.042;
    float w1 = dot(pos, d1) * k1 - t * 1.30;

    // Swell 2: Secondary angled roller (Wavelength = 17m)
    vec2  d2 = normalize(vec2(-0.48, 0.88));
    float k2 = 0.3696;
    float a2 = 0.026;
    float w2 = dot(pos, d2) * k2 + t * 1.60;

    // Swell 3: Gentle rolling swell (Wavelength = 10.5m)
    vec2  d3 = normalize(vec2(0.70, -0.71));
    float k3 = 0.5984;
    float a3 = 0.016;
    float w3 = dot(pos, d3) * k3 - t * 2.05;

    // Swell 4: Subtle surface swell (Wavelength = 6.5m)
    vec2  d4 = normalize(vec2(-0.75, -0.66));
    float k4 = 0.9666;
    float a4 = 0.008;
    float w4 = dot(pos, d4) * k4 + t * 2.45;

    // Distance LOD fade: smoothly calm fine ripples at distance to eliminate shimmering
    float fade4 = 1.0 - smoothstep(25.0, 75.0, distToCam);
    float fade3 = 1.0 - smoothstep(60.0, 160.0, distToCam);
    float fadeSwell = 1.0 - smoothstep(120.0, 450.0, distToCam) * 0.35;

    // Trochoidal wave gradient (normal slope)
    vec2 waveGrad = (d1 * (cos(w1) * k1 * a1) +
                     d2 * (cos(w2) * k2 * a2)) * fadeSwell +
                    (d3 * (cos(w3) * k3 * a3)) * fade3 +
                    (d4 * (cos(w4) * k4 * a4)) * fade4;

    vec3 N = normalize(fragNormal);
    if (abs(N.y) > 0.5) {
        N = normalize(vec3(-waveGrad.x, (N.y > 0.0 ? 1.0 : -1.0), -waveGrad.y));
    }

    vec3 effN = viewingFromBelow ? -N : N;
    float NdotV = clamp(dot(effN, V), 0.0, 1.0);
    vec3 L = normalize(pc.sunDir.xyz);

    vec3 waterSurfaceColor;
    float finalAlpha;

    if (viewingFromBelow || cameraUnderwater) {
        // -------------------------------------------------------------
        // UNDERWATER VIEW: Snell's Window & Upward Caustic Beams
        // -------------------------------------------------------------
        float internalReflection = pow(1.0 - NdotV, 3.5);
        finalAlpha = mix(0.18, 0.58, internalReflection * depthFactor);

        float underCaustic = pow(sin(pos.x * 2.5 + t * 1.5) * sin(pos.y * 2.5 + t * 1.2) * 0.5 + 0.5, 2.0) * 0.35 * sunIntensity;
        vec3 sunBeams = mix(vec3(0.2, 0.4, 0.7), vec3(1.1, 1.05, 0.85), isDay) * underCaustic;

        vec3 underWaterTint = mix(vec3(0.08, 0.35, 0.52), vec3(0.03, 0.16, 0.32), depthFactor);
        waterSurfaceColor = mix(underWaterTint, vec3(0.02, 0.08, 0.22), internalReflection * 0.7) + sunBeams;

    } else {
        // -------------------------------------------------------------
        // ABOVE WATER VIEW: Physical Schlick Fresnel & GGX Sun Reflection
        // -------------------------------------------------------------
        float dither = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));

        // Physical Schlick Fresnel:
        // F0 = 0.020 for water (n = 1.333)
        // Looking straight down: F ~ 0.020 -> crystal clear transparent waterbed
        // Grazing angles towards horizon: F -> 0.98 -> brilliant reflective mirror
        const float F0 = 0.020;
        float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

        // Trace screen-space reflection (island trees, terrain, sky, and clouds)
        vec3 R = reflect(-V, effN);
        vec3 reflectedScene = traceSSR(fragWorldPos, R, L, isDay, sunIntensity, optWaterQuality, dither);

        // GGX Microfacet Specular Sun Reflection (Glittering Sun Trail)
        vec3 H = normalize(L + V);
        float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);

        float roughness = 0.052; // Smooth liquid surface for crisp, glistening sun reflections
        float D = DistributionGGX(effN, H, roughness);
        float G = GeometrySmith(effN, V, L, roughness);
        float F_sun = F0 + (1.0 - F0) * pow(1.0 - max(dot(V, H), 0.0), 5.0);

        float NdotL = max(dot(effN, L), 0.0);
        float specularGGX = (D * F_sun * G) / (4.0 * max(NdotV, 0.001) * max(NdotL, 0.001) + 0.0001);

        float rtShadow = vibrant ? sampleRealtimeShadow(fragWorldPos, effN, L) : 1.0;
        vec3 sunGlintColor = mix(vec3(2.40, 2.25, 2.05), vec3(3.40, 2.20, 0.95), goldenHour);
        vec3 sunGlint = specularGGX * NdotL * sunIntensity * sunGlintColor * isDay * rtShadow;

        // Subsurface Water Scattering: emerald glow through wave crests
        vec3 sssColor = vec3(0.0);
        if (vibrant && optWaterQuality >= 2) {
            float sss = pow(clamp(dot(V, -L), 0.0, 1.0), 3.5) * (1.0 - depthFactor * 0.45) * 0.40 * isDay;
            sssColor = vec3(0.05, 0.42, 0.48) * sss * rtShadow;
        }

        // Shoreline Foam Wash along shallow banks
        float foamEdge = smoothstep(0.12, 0.015, depthFactor);
        float foamWave = 0.5 + 0.5 * sin(dot(pos, d1) * k1 * 1.5 - t * 1.6);
        float foam = foamEdge * smoothstep(0.42, 0.85, foamWave) * isDay;
        vec3 foamColor = vec3(0.92, 0.96, 1.0) * (foam * 0.40);

        // Composite water radiance
        waterSurfaceColor = mix(waterBedColor, reflectedScene, fresnel) + sunGlint + foamColor + sssColor;
        finalAlpha = clamp(mix(baseAlpha, 0.98, fresnel) + foam * 0.30, 0.0, 1.0);
    }


    // Dynamic torch lights
    waterSurfaceColor += vec3(1.0, 0.65, 0.22) * fragColor.b;

    if (pc.heldTorch.w > 0.001) {
        float dHeld = length(fragWorldPos - pc.heldTorch.xyz);
        float atten = clamp(1.0 - dHeld / 16.0, 0.0, 1.0);
        waterSurfaceColor += vec3(1.0, 0.65, 0.22) * (atten * atten * 2.2 * pc.heldTorch.w);
    }

    if (pc.pointLight1.w > 0.001) {
        float dLight = length(fragWorldPos - pc.pointLight1.xyz);
        float atten = clamp(1.0 - dLight / 16.0, 0.0, 1.0);
        waterSurfaceColor += vec3(1.0, 0.65, 0.22) * (atten * atten * 2.0 * pc.pointLight1.w);
    }
    if (pc.pointLight2.w > 0.001) {
        float dLight = length(fragWorldPos - pc.pointLight2.xyz);
        float atten = clamp(1.0 - dLight / 16.0, 0.0, 1.0);
        waterSurfaceColor += vec3(1.0, 0.65, 0.22) * (atten * atten * 2.0 * pc.pointLight2.w);
    }

    // Atmospheric Perspective & Distance Fog
    float fogEnd = abs(pc.skyFog.w);
    float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);

    if (cameraUnderwater) {
        float dist = length(fragWorldPos - pc.camPos.xyz);
        float uFactor = smoothstep(2.0, 24.0, dist);
        waterSurfaceColor = mix(waterSurfaceColor, vec3(0.005, 0.035, 0.12), uFactor);
    } else {
        waterSurfaceColor = applyHorizonDistanceFog(
            waterSurfaceColor,
            fragWorldPos,
            pc.camPos.xyz,
            L,
            pc.skyFog.rgb,
            isDay,
            goldenHour,
            optAtmosFog,
            fogEnd
        );
    }


    // Output linear HDR radiance directly to HDR buffer (master tonemap handled in post-processing pass)
    outColor = vec4(waterSurfaceColor, finalAlpha);
}
