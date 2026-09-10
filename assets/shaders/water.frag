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
// Real-Time High-Fidelity Scene Reflection (Sky, Clouds & Shorelines)
// -------------------------------------------------------------
vec3 traceSceneReflection(vec3 origin, vec3 R, vec3 L, float isDay, float sunIntensity, int waterQuality, float dither) {
    float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);

    // 1. Physically-Based Atmospheric Sky Dome Gradient
    vec3 skyZenith  = mix(vec3(0.02, 0.05, 0.14), vec3(0.18, 0.42, 0.82), isDay);
    vec3 horizonDay = mix(vec3(0.58, 0.72, 0.88), vec3(1.45, 1.08, 0.58), goldenHour * 0.65);
    vec3 skyHorizon = mix(pc.skyFog.rgb, horizonDay, isDay);
    vec3 skyColor   = mix(skyHorizon, skyZenith, clamp(max(R.y, 0.0) * 1.5, 0.0, 1.0));

    // Solar atmospheric forward glare reflected in sky
    float sunReflectionGlare = pow(max(dot(R, L), 0.0), 24.0) * 2.5 * isDay;
    vec3 sunGlareColor = mix(vec3(1.2, 1.1, 0.9), vec3(1.8, 1.2, 0.5), goldenHour);
    skyColor += sunGlareColor * sunReflectionGlare;

    // 2. Reflected 3D Volumetric Cumulus Clouds in the sky
    if (R.y > 0.012 && waterQuality >= 1) {
        float tCloud = (180.0 - origin.y) / max(R.y, 0.02);
        if (tCloud > 0.0 && tCloud < 7500.0) {
            vec3 pCloud = origin + R * tCloud;
            vec2 wind = vec2(pc.camPos.w * 4.2, pc.camPos.w * 1.6);
            vec3 ws = pCloud + vec3(wind.x, 0.0, wind.y);

            float dH = length(pCloud.xz - pc.camPos.xz);
            float yCurv = pCloud.y + (dH * dH) / (2.0 * 95000.0);

            if (yCurv >= 175.0 && yCurv <= 340.0) {
                float macroNoise = cloudFBM(ws * 0.00028);
                if (macroNoise > 0.38) {
                    float cDensity = smoothstep(0.38, 0.62, macroNoise);
                    if (cDensity > 0.0) {
                        float cAlpha = clamp(cDensity * 1.8, 0.0, 1.0);
                        float cosTh = dot(R, L);
                        float silver = pow(max(cosTh * 0.5 + 0.5, 0.0), 3.0) * 0.65 + 0.85;
                        vec3 goldenCloud = mix(vec3(1.35, 1.25, 1.05), vec3(2.10, 1.40, 0.65), goldenHour);
                        vec3 cloudLit = mix(vec3(0.35, 0.42, 0.55), goldenCloud * silver, isDay);
                        skyColor = mix(skyColor, cloudLit, cAlpha * 0.94);
                    }
                }
            }
        }
    }

    // 3. Shoreline, Island Hills & Forest Silhouette Reflection
    vec3 terrainReflection = vec3(0.0);
    float terrainWeight = 0.0;

    if (R.y < 0.38 && waterQuality >= 1) {
        float dist = 1.0 + dither * 0.8;
        int maxSteps = (waterQuality >= 2) ? 40 : 26;
        float maxDist = (waterQuality >= 2) ? 700.0 : 500.0;

        for (int i = 0; i < maxSteps; ++i) {
            if (dist >= maxDist) break;
            vec3 P = origin + R * dist;

            float groundH = getTerrainHeight(P.xz);
            float canopyH = (groundH > 50.0) ? (groundH + 4.5) : groundH;

            if (P.y <= canopyH) {
                bool hitCanopy = (P.y > groundH + 0.8);
                vec3 foliageColor = vec3(0.08, 0.22, 0.06) * mix(0.60, 1.20, goldenHour);
                vec3 shoreColor   = vec3(0.28, 0.22, 0.14) * mix(0.55, 1.10, goldenHour);

                if (groundH > 82.0) {
                    foliageColor = vec3(0.88, 0.92, 0.96); // Snow
                } else if (groundH > 66.0) {
                    foliageColor = vec3(0.32, 0.31, 0.30); // Granite
                }

                vec3 hitColor = hitCanopy ? foliageColor : shoreColor;
                float sunDiffuse = clamp(dot(vec3(-R.x, 0.5, -R.z), L), 0.25, 1.0);
                hitColor *= mix(0.40, 1.20, sunDiffuse * isDay);

                float distFade = clamp(dist / maxDist, 0.0, 1.0);
                terrainReflection = mix(hitColor, skyHorizon, distFade * 0.65);
                terrainWeight = clamp(1.0 - distFade * 0.45, 0.0, 0.95);
                break;
            }

            float stepSize = 1.0 + 0.10 * dist;
            dist += stepSize;
        }
    }

    return mix(skyColor, terrainReflection, terrainWeight * max(isDay, 0.35));
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
    // 1. Beer-Lambert Depth Absorption & Transmittance Palette
    // -------------------------------------------------------------
    // Absorption coefficients per meter: red absorbs fastest, blue/cyan penetrates deepest
    vec3 sigmaA = vec3(0.32, 0.10, 0.035);
    float waterDepthMeters = depthFactor * 6.5;
    vec3 transmittance = exp(-waterDepthMeters * sigmaA);

    // Deep ocean bed: rich obsidian navy
    vec3 deepWaterRadiance = mix(vec3(0.006, 0.016, 0.038), vec3(0.014, 0.046, 0.092), isDay);
    // Shallow waterbed: clear, luminous turquoise/aquamarine
    vec3 shallowWaterRadiance = mix(vec3(0.018, 0.055, 0.075), vec3(0.055, 0.165, 0.185), isDay);

    vec3 waterBedColor = mix(deepWaterRadiance, shallowWaterRadiance, transmittance);

    // Soft shoreline edge blending without harsh block clipping
    float shoreEdge = smoothstep(0.03, 0.32, depthFactor);
    float baseAlpha = mix(0.20, 0.88, smoothstep(0.0, 0.80, depthFactor)) * shoreEdge;

    // -------------------------------------------------------------
    // 2. Voxel-Scale Harmonic Wave Octaves & Analytical Normal
    // Wavelengths calibrated to block scale (3.2m down to 0.45m)
    // -------------------------------------------------------------
    float t = pc.camPos.w * 1.05;
    vec2 pos = fragWorldPos.xz;
    float distToCam = length(fragWorldPos - pc.camPos.xyz);

    // Octave 1: Primary gentle swell (L = 3.2m, k = 1.9635)
    vec2  d1 = normalize(vec2(0.78, 0.62));
    float k1 = 1.9635;
    float a1 = 0.045;
    float w1 = dot(pos, d1) * k1 - t * 1.65;

    // Octave 2: Cross wave (L = 2.1m, k = 2.9920)
    vec2  d2 = normalize(vec2(-0.55, 0.83));
    float k2 = 2.9920;
    float a2 = 0.032;
    float w2 = dot(pos, d2) * k2 + t * 2.05;

    // Octave 3: Ripple train (L = 1.4m, k = 4.4880)
    vec2  d3 = normalize(vec2(0.92, -0.39));
    float k3 = 4.4880;
    float a3 = 0.020;
    float w3 = dot(pos, d3) * k3 - t * 2.55;

    // Octave 4: Capillary wave (L = 0.85m, k = 7.3920)
    vec2  d4 = normalize(vec2(-0.38, -0.92));
    float k4 = 7.3920;
    float a4 = 0.012;
    float w4 = dot(pos, d4) * k4 + t * 3.20;

    // Octave 5: Micro-ripple detail (L = 0.45m, k = 13.9626)
    vec2  d5 = normalize(vec2(0.50, -0.866));
    float k5 = 13.9626;
    float a5 = 0.007;
    float w5 = dot(pos, d5) * k5 + t * 4.10;

    // Distance LOD smooth fade to eliminate high-frequency sparkling at distance
    float fade5 = 1.0 - smoothstep(14.0, 32.0, distToCam);
    float fade4 = 1.0 - smoothstep(22.0, 50.0, distToCam);
    float fade3 = 1.0 - smoothstep(38.0, 85.0, distToCam);
    float fadeMacro = 1.0 - smoothstep(60.0, 280.0, distToCam) * 0.45;

    vec2 waveGrad = d1 * (cos(w1) * k1 * a1) * fadeMacro +
                    d2 * (cos(w2) * k2 * a2) * fadeMacro +
                    d3 * (cos(w3) * k3 * a3) * fade3 +
                    d4 * (cos(w4) * k4 * a4) * fade4 +
                    d5 * (cos(w5) * k5 * a5) * fade5;

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
        // Looking down: F ~ 0.02 (crystal clear transparent waterbed)
        // Grazing angles: F -> 0.98 (brilliant reflective mirror)
        const float F0 = 0.020;
        float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

        // Trace scene reflection (sky dome, cumulus clouds, shore trees)
        vec3 R = reflect(-V, effN);
        vec3 reflectedScene = traceSceneReflection(fragWorldPos, R, L, isDay, sunIntensity, optWaterQuality, dither);

        // GGX Microfacet Specular Sun Reflection (Glittering Sun Trail)
        vec3 H = normalize(L + V);
        float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);

        float roughness = 0.075;
        float D = DistributionGGX(effN, H, roughness);
        float G = GeometrySmith(effN, V, L, roughness);
        float F_sun = F0 + (1.0 - F0) * pow(1.0 - max(dot(V, H), 0.0), 5.0);

        float NdotL = max(dot(effN, L), 0.0);
        float specularGGX = (D * F_sun * G) / (4.0 * max(NdotV, 0.001) * max(NdotL, 0.001) + 0.0001);

        float rtShadow = vibrant ? sampleRealtimeShadow(fragWorldPos, effN, L) : 1.0;
        vec3 sunGlintColor = mix(vec3(1.35, 1.25, 1.05), vec3(2.20, 1.45, 0.65), goldenHour);
        vec3 sunGlint = specularGGX * NdotL * sunIntensity * sunGlintColor * isDay * rtShadow;

        // Subsurface Water Scattering: emerald glow through wave crests
        vec3 sssColor = vec3(0.0);
        if (vibrant && optWaterQuality >= 2) {
            float sss = pow(clamp(dot(V, -L), 0.0, 1.0), 3.0) * (1.0 - depthFactor * 0.4) * 0.45 * isDay;
            sssColor = vec3(0.06, 0.48, 0.52) * sss * rtShadow;
        }

        // Shoreline Foam Wash along shallow banks
        float foamEdge = smoothstep(0.16, 0.02, depthFactor);
        float foamWave = 0.5 + 0.5 * sin(dot(pos, d1) * k1 * 1.5 - t * 1.8);
        float foam = foamEdge * smoothstep(0.40, 0.85, foamWave) * isDay;
        vec3 foamColor = vec3(0.92, 0.96, 1.0) * (foam * 0.45);

        // Composite water radiance
        waterSurfaceColor = mix(waterBedColor, reflectedScene, fresnel) + sunGlint + foamColor + sssColor;
        finalAlpha = clamp(mix(baseAlpha, 0.96, fresnel) + foam * 0.35, 0.0, 1.0);
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
