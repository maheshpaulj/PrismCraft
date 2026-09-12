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
layout(binding = 4) uniform sampler2D depthSampler;

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
// -------------------------------------------------------------
// High-Quality Multi-Octave Noise for Reflected Cumulus Clouds
// -------------------------------------------------------------
float hash2D(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    float a = hash2D(i);
    float b = hash2D(i + vec2(1.0, 0.0));
    float c = hash2D(i + vec2(0.0, 1.0));
    float d = hash2D(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float cloudFBM2D(vec2 p) {
    float f = 0.0;
    f += 0.500 * valueNoise2D(p); p = p * 2.02 + vec2(1.7, 3.2);
    f += 0.280 * valueNoise2D(p); p = p * 2.05 + vec2(2.3, 1.4);
    f += 0.140 * valueNoise2D(p); p = p * 2.08 + vec2(4.1, 2.7);
    f += 0.080 * valueNoise2D(p);
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
// Atmospheric Sky Palette & Off-Screen Dome Model (Matching sky.frag 1:1)
// -------------------------------------------------------------
vec3 computeProceduralSky(vec3 R, vec3 L, float isDay, float sunIntensity, float goldenHour) {
    vec3 zenithDay   = vec3(0.14, 0.36, 0.85);
    vec3 horizonDay  = vec3(0.60, 0.74, 0.90);
    vec3 zenithDusk  = vec3(0.12, 0.14, 0.36);
    vec3 horizonDusk = vec3(1.12, 0.58, 0.16);
    vec3 zenithNight = vec3(0.007, 0.012, 0.025);
    vec3 horizonNight= vec3(0.018, 0.025, 0.048);

    vec3 dayZenith   = mix(zenithDay, zenithDusk, goldenHour);
    vec3 dayHorizon  = mix(horizonDay, horizonDusk, goldenHour);

    vec3 activeZenith  = mix(zenithNight, dayZenith, isDay);
    vec3 activeHorizon = mix(horizonNight, dayHorizon, isDay);

    float elevation = clamp(max(R.y, 0.0), 0.0, 1.0);
    float rayleighFactor = exp(-elevation * 2.8);
    vec3 skyRgb = mix(activeZenith, activeHorizon, rayleighFactor);

    // Subtle atmospheric horizon aerosol haze band
    float hazeDist = abs(R.y - 0.015);
    float hazeBand = exp(-hazeDist * 14.0);
    vec3 hazeDay = mix(vec3(0.70, 0.80, 0.90), vec3(1.10, 0.68, 0.32), goldenHour);
    vec3 hazeColor = mix(vec3(0.014, 0.019, 0.032), hazeDay, isDay);
    skyRgb = mix(skyRgb, hazeColor, hazeBand * 0.45);

    // Solar atmospheric forward warmth
    float cosSun = dot(R, L);
    if (cosSun > 0.0 && isDay > 0.05) {
        float sunWarmth = pow(clamp(cosSun * 0.5 + 0.5, 0.0, 1.0), 2.5) * 0.55;
        vec3 warmthColor = mix(vec3(0.35, 0.30, 0.18), vec3(1.10, 0.60, 0.18), goldenHour);
        skyRgb += warmthColor * (sunWarmth * isDay);

        // Soft circumsolar corona glow
        float corona = pow(clamp(cosSun, 0.0, 1.0), 24.0) * 1.10 + pow(clamp(cosSun, 0.0, 1.0), 160.0) * 3.5;
        vec3 coronaCol = mix(vec3(2.2, 1.9, 1.4), vec3(4.2, 2.4, 0.8), goldenHour);
        skyRgb += coronaCol * (corona * isDay * sunIntensity);
    }

    return skyRgb;
}

// Linearize Vulkan [0, 1] perspective depth buffer into view-space meters
float linearizeDepth(float z_ndc) {
    float nearVal = 0.1;
    float farVal = 500.0;
    return (nearVal * farVal) / (farVal - z_ndc * (farVal - nearVal));
}

// -------------------------------------------------------------
// Robust Screen-Space Reflection (SSR) Ray Marcher
// Reprojects each march step through MVP into screen UV + linear depth
// -------------------------------------------------------------
vec4 traceSSR(vec3 rayOrigin, vec3 rayDir) {
    const float maxDist = 160.0;
    const int maxSteps = 48;
    float t = 0.6;
    float dt = 0.75;
    float tPrev = t;

    vec2 hitUV = vec2(0.0);
    bool hitFound = false;

    // Direct ray march along the reflection ray without perturbation jitter
    vec3 marchDir = normalize(rayDir);

    for (int i = 0; i < maxSteps && t < maxDist; ++i) {
        vec3 p = rayOrigin + marchDir * t;
        vec4 clip = pc.mvp * vec4(p, 1.0);

        if (clip.w <= 0.05) {
            tPrev = t;
            t += dt;
            dt *= 1.05;
            continue;
        }

        vec2 uv = vec2((clip.x / clip.w) * 0.5 + 0.5, 1.0 - ((clip.y / clip.w) * 0.5 + 0.5));
        float rayLinearDepth = clip.w;

        // Stop if ray exits screen bounds
        if (uv.x < 0.001 || uv.x > 0.999 || uv.y < 0.001 || uv.y > 0.999) {
            break;
        }

        float sceneRawDepth = texture(depthSampler, uv).r;

        // Skip sky / far plane: rays travelling through empty sky shouldn't falsely hit scene depth
        if (sceneRawDepth >= 0.9999) {
            tPrev = t;
            t += dt;
            dt *= 1.05;
            continue;
        }

        float sceneLinearDepth = linearizeDepth(sceneRawDepth);

        // RECONSTRUCT SCENE WORLD Y:
        // In the opaque depth pass, any pixel showing water has the LAKEBED depth.
        // A reflection ray in the air above water CANNOT hit underwater lakebed!
        // We reject any candidate whose reconstructed world Y is at or below the water surface.
        float sceneY = pc.camPos.y + (p.y - pc.camPos.y) * (sceneLinearDepth / rayLinearDepth);
        if (sceneY <= rayOrigin.y + 0.15) {
            tPrev = t;
            t += dt;
            dt *= 1.05;
            continue;
        }

        // Thickness tolerance in meters: expands with distance to avoid false pass-throughs
        float thickness = 0.90 + 0.05 * t;
        float depthDiff = rayLinearDepth - sceneLinearDepth;

        // Hit check: ray has entered the surface within the thickness envelope
        if (depthDiff >= 0.0 && depthDiff < thickness) {
            // Sub-step binary refinement between tPrev and t
            float t0 = tPrev;
            float t1 = t;
            for (int b = 0; b < 5; ++b) {
                float tMid = (t0 + t1) * 0.5;
                vec3 pMid = rayOrigin + marchDir * tMid;
                vec4 cMid = pc.mvp * vec4(pMid, 1.0);
                if (cMid.w > 0.05) {
                    vec2 uvMid = vec2((cMid.x / cMid.w) * 0.5 + 0.5, 1.0 - ((cMid.y / cMid.w) * 0.5 + 0.5));
                    float sDepth = linearizeDepth(texture(depthSampler, uvMid).r);
                    float sY = pc.camPos.y + (pMid.y - pc.camPos.y) * (sDepth / cMid.w);
                    if (sY > rayOrigin.y + 0.15 && cMid.w >= sDepth) {
                        t1 = tMid;
                        hitUV = uvMid;
                    } else {
                        t0 = tMid;
                    }
                }
            }
            hitFound = true;
            break;
        }

        tPrev = t;
        t += dt;
        dt *= 1.05;
    }

    if (!hitFound) {
        return vec4(0.0);
    }

    // Screen edge vignetting: smooth 3.5% falloff so reflections reach borders gracefully
    float edgeFade = smoothstep(0.0, 0.035, hitUV.x) *
                     smoothstep(0.0, 0.035, 1.0 - hitUV.x) *
                     smoothstep(0.0, 0.035, hitUV.y) *
                     smoothstep(0.0, 0.035, 1.0 - hitUV.y);

    // Distance fade as ray approaches maximum reach
    float distFade = 1.0 - smoothstep(maxDist * 0.70, maxDist, t);
    float totalFade = edgeFade * distFade;

    // Multi-tap soft reflection sample (liquid dispersion)
    vec2 texel = 1.0 / vec2(textureSize(ssrSampler, 0));
    vec3 s0 = texture(ssrSampler, hitUV).rgb;
    vec3 s1 = texture(ssrSampler, hitUV + vec2(texel.x, texel.y) * 1.5).rgb;
    vec3 s2 = texture(ssrSampler, hitUV - vec2(texel.x, texel.y) * 1.5).rgb;
    vec3 color = s0 * 0.5 + (s1 + s2) * 0.25;

    return vec4(color, totalFade);
}

// -------------------------------------------------------------
// Real-Time Screen-Space Reflection (SSR), Sky Dome & Clouds
// -------------------------------------------------------------
vec3 getReflectionColor(vec3 origin, vec3 R, vec3 L, float isDay, float sunIntensity, int waterQuality) {
    float goldenHour = smoothstep(0.40, 0.02, pc.dayInfo.y) * step(-0.06, pc.dayInfo.y);
    vec3 sky = computeProceduralSky(R, L, isDay, sunIntensity, goldenHour);

    // 1. Screen-Space Sky & 3D Volumetric Cloud Sampling:
    // If the reflected ray direction R points towards visible sky on screen,
    // directly sample the rendered sky dome and 3D volumetric clouds from ssrSampler!
    bool sampledScreenSky = false;
    vec3 pSky = pc.camPos.xyz + R * 350.0;
    vec4 cSky = pc.mvp * vec4(pSky, 1.0);
    if (cSky.w > 0.05) {
        vec2 uvSky = vec2((cSky.x / cSky.w) * 0.5 + 0.5, 1.0 - ((cSky.y / cSky.w) * 0.5 + 0.5));
        if (uvSky.x >= 0.012 && uvSky.x <= 0.988 && uvSky.y >= 0.012 && uvSky.y <= 0.988) {
            float sceneD = texture(depthSampler, uvSky).r;
            if (sceneD >= 0.9999) {
                // Ray points to unobstructed sky on screen!
                vec3 screenSkyColor = texture(ssrSampler, uvSky).rgb;
                float edgeW = smoothstep(0.012, 0.06, uvSky.x) * smoothstep(0.988, 0.94, uvSky.x) *
                              smoothstep(0.012, 0.06, uvSky.y) * smoothstep(0.988, 0.94, uvSky.y);
                sky = mix(sky, screenSkyColor, edgeW);
                sampledScreenSky = (edgeW > 0.85);
            }
        }
    }

    // 2. High-Contrast Billowy Cumulus Cloud Reflection (for off-screen sky directions or screen edge fill)
    if (!sampledScreenSky && R.y > 0.012 && waterQuality >= 1) {
        float tCloud = (196.0 - origin.y) / max(R.y, 0.015);
        if (tCloud > 0.0 && tCloud < 6500.0) {
            vec3 pCloud = origin + R * tCloud;
            vec2 wind = vec2(pc.camPos.w * 4.2, pc.camPos.w * 1.6);
            vec2 ws = (pCloud.xz + wind) * 0.0032;

            float macroNoise = cloudFBM2D(ws);
            if (macroNoise > 0.40) {
                float cDensity = smoothstep(0.40, 0.68, macroNoise);
                float cAlpha = clamp(cDensity * 1.8, 0.0, 0.96);

                // Silver lining & solar edge illumination
                float silver = pow(max(dot(R, L) * 0.5 + 0.5, 0.0), 3.2) * 1.10 + 0.90;
                vec3 midDayCloud = vec3(1.75, 1.72, 1.65);
                vec3 goldenCloud = vec3(2.65, 1.60, 0.70);
                vec3 cloudLitColor = mix(midDayCloud, goldenCloud, goldenHour);

                // Ambient underside (soft atmospheric blue, never pitch black)
                vec3 cloudShadeColor = mix(vec3(0.42, 0.48, 0.65), vec3(0.60, 0.45, 0.42), goldenHour);
                vec3 finalCloudLit = mix(cloudShadeColor, cloudLitColor * silver, isDay);

                sky = mix(sky, finalCloudLit, cAlpha);
            }
        }
    }

    // 3. Real-time Screen Space Reflections for nearby geometry (river banks, trees, terrain)
    if (waterQuality >= 2) {
        vec4 ssr = traceSSR(origin, R);
        if (ssr.a > 0.001) {
            sky = mix(sky, ssr.rgb, ssr.a);
        }
    }

    return sky;
}

// -------------------------------------------------------------
// Clean Horizon Distance Fog: Smoothly fades chunks into horizon at edge of render distance
// -------------------------------------------------------------
vec3 applyHorizonDistanceFog(vec3 surfaceColor, vec3 fragPos, vec3 camPos, vec3 L, vec3 skyFogColor, float isDay, float goldenHour, float fogEndDist) {
    vec3 rayDir = fragPos - camPos;
    float rayDist = length(rayDir);
    if (rayDist < 0.001) return surfaceColor;
    vec3 V = rayDir / rayDist;

    // Horizon fade start: 55% of render distance for rich aerial perspective while keeping foreground crisp
    float startRatio = 0.55;
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
    vec3 zenithSky = linearHorizonSky * vec3(0.72, 0.86, 1.18);
    vec3 baseAirLight = mix(linearHorizonSky, zenithSky, clamp(V.y * 0.55 + 0.15, 0.0, 1.0));

    // Smooth forward solar warming with Henyey-Greenstein Mie forward scattering
    float cosTheta = dot(V, L);
    if (isDay > 0.05) {
        // Broad forward scattering across the sun hemisphere + sharp circumsolar flare
        float broadPhase = pow(max(cosTheta * 0.5 + 0.5, 0.0), 2.5) * 0.50;
        float peakPhase  = pow(max(cosTheta, 0.0), 6.0) * 0.65;
        float forwardPhase = (broadPhase + peakPhase);

        vec3 goldenHaze = mix(vec3(1.18, 1.08, 0.92), vec3(1.95, 1.35, 0.55), goldenHour);
        baseAirLight = mix(baseAirLight, baseAirLight * goldenHaze, forwardPhase);
    }

    vec3 result = mix(surfaceColor, baseAirLight, fogFactor);

    // Subtle forward solar aerial haze on distant geometry facing the sun (AAA shader pack look)
    if (cosTheta > 0.0 && isDay > 0.05) {
        float forwardHaze = pow(cosTheta, 2.0) * isDay * clamp(rayDist / (fogEndDist * 0.75), 0.0, 1.0);
        vec3 forwardGlow = mix(vec3(0.12, 0.10, 0.06), vec3(0.45, 0.28, 0.10), goldenHour);
        result += forwardGlow * (forwardHaze * 0.20);
    }

    return result;
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
    // 1. Continuous Per-Pixel Scene Depth Buffer Reconstruction
    // -------------------------------------------------------------
    // Reconstruct view-space depth behind this water surface fragment (completely seamless, sub-pixel continuous!)
    vec2 screenUV = gl_FragCoord.xy / vec2(textureSize(depthSampler, 0));
    float sceneRawDepth = texture(depthSampler, screenUV).r;
    float sceneLinearDepth = (sceneRawDepth >= 0.9999) ? 500.0 : linearizeDepth(sceneRawDepth);
    float waterLinearDepth = linearizeDepth(gl_FragCoord.z);

    // Continuous view-space optical depth in meters through water along the camera ray
    float waterDepthMeters = max(0.0, sceneLinearDepth - waterLinearDepth);

    // -------------------------------------------------------------
    // 2. Physical Beer-Lambert Absorption (Red -> Green -> Blue)
    // -------------------------------------------------------------
    // Exponential extinction per color channel: red absorbs rapidly, green moderately, blue penetrates deeply
    vec3 extinctionCoeff = vec3(0.55, 0.22, 0.05);
    vec3 transmittance = exp(-waterDepthMeters * extinctionCoeff);

    // Color gradient: clear crystal shallows transitioning to deep oceanic dark navy
    vec3 shallowWaterColor = mix(vec3(0.015, 0.065, 0.10), vec3(0.020, 0.080, 0.13), isDay);
    vec3 deepWaterColor    = mix(vec3(0.001, 0.005, 0.015), vec3(0.002, 0.012, 0.035), isDay);

    // Deep water extinction: color = deepColor + (shallowColor - deepColor) * exp(-depth * extinctionCoeff)
    vec3 waterBedColor = deepWaterColor + (shallowWaterColor - deepWaterColor) * transmittance;

    // Smooth physical shoreline opacity: crystal clear at zero depth, smoothly building body opacity
    float baseAlpha = clamp(1.0 - exp(-waterDepthMeters * 0.45), 0.0, 0.92);

    // -------------------------------------------------------------
    // 3. Dynamic Wave Ripples & Surface Motion
    // -------------------------------------------------------------
    float t = pc.camPos.w * 1.05;
    vec2 pos = fragWorldPos.xz;
    float distToCam = length(fragWorldPos - pc.camPos.xyz);

    // Medium swells: wave ripples scrolling along (0.85, 0.52)
    vec2 uv1 = pos * 0.65 + vec2(t * 0.32, t * 0.20);
    float r1_x = cos(uv1.x * 3.14 + sin(uv1.y * 2.2)) * 3.14;
    float r1_y = cos(uv1.y * 3.14 + cos(uv1.x * 2.2)) * 3.14;
    vec2 rippleGrad1 = vec2(r1_x, r1_y) * 0.026;

    // Cross-ripples rotated ~45 deg, scrolling along (-0.6, 0.8)
    vec2 uv2 = vec2(pos.x * 0.707 - pos.y * 0.707, pos.x * 0.707 + pos.y * 0.707) * 1.35 - vec2(t * 0.25, -t * 0.32);
    float r2_x = sin(uv2.x * 3.14 + cos(uv2.y * 2.0)) * 3.14;
    float r2_y = cos(uv2.y * 3.14 + sin(uv2.x * 2.0)) * 3.14;
    vec2 rippleGrad2 = vec2(r2_x, r2_y) * 0.018;

    // Fine capillary ripples for glistening sun sparkles
    vec2 uv3 = pos * 2.8 + vec2(-t * 0.40, t * 0.35);
    float r3_x = sin(uv3.x * 3.14) * 3.14;
    float r3_y = cos(uv3.y * 3.14) * 3.14;
    vec2 fineGrad = vec2(r3_x, r3_y) * 0.012;

    // Distance fade to prevent shimmering at far horizon
    float rippleFade = 1.0 - smoothstep(40.0, 180.0, distToCam);
    vec2 totalRipples = (rippleGrad1 + rippleGrad2 + fineGrad) * rippleFade;

    // -------------------------------------------------------------
    // 4. Directional Flow Currents for Streams & Waterfalls
    // -------------------------------------------------------------
    bool isTopFace = (abs(fragNormal.y) > 0.6);
    vec2 flowDir = fragNormal.xz;
    float flowLen = length(flowDir);
    bool isFlowing = (flowLen > 0.02 && isTopFace);

    if (isFlowing) {
        vec2 fDir = normalize(flowDir);
        float fPhase0 = fract(t * 1.5);
        float fPhase1 = fract(t * 1.5 + 0.5);
        float fW0 = 1.0 - abs(fPhase0 - 0.5) * 2.0;
        float fW1 = 1.0 - abs(fPhase1 - 0.5) * 2.0;
        float r0 = sin(dot(pos - fDir * fPhase0 * 2.0, fDir) * 4.5) * 0.035;
        float r1 = sin(dot(pos - fDir * fPhase1 * 2.0, fDir) * 4.5) * 0.035;
        totalRipples += fDir * (r0 * fW0 + r1 * fW1);
    }

    vec3 rawNormal = normalize(fragNormal);

    // -------------------------------------------------------------
    // 5. DUAL-NORMAL PIPELINE:
    // a) smoothNormal: Carries rolling Gerstner swells + gentle wave tilt.
    //    Gives visible, organic undulation to reflections so reflected trees,
    //    mountains, and clouds ripple across the lake without breaking into noise!
    // b) detailNormal: Adds fine capillary ripples.
    //    Used for GGX specular glints and sun path sparkles.
    // -------------------------------------------------------------
    vec3 smoothNormal;
    vec3 detailNormal;

    if (isTopFace) {
        vec2 swellTilt = (rippleGrad1 + rippleGrad2 * 0.5) * rippleFade;
        smoothNormal = normalize(mix(vec3(0.0, 1.0, 0.0), rawNormal, 0.65) - vec3(swellTilt.x, 0.0, swellTilt.y) * 0.45);
        detailNormal = normalize(vec3(rawNormal.x - totalRipples.x, rawNormal.y, rawNormal.z - totalRipples.y));
    } else {
        float downRipple = sin((fragWorldPos.y + t * 2.5) * 6.0) * 0.05;
        smoothNormal = rawNormal;
        detailNormal = normalize(vec3(rawNormal.x, downRipple, rawNormal.z));
    }

    vec3 effSmoothN = viewingFromBelow ? -smoothNormal : smoothNormal;
    float NdotV_smooth = clamp(dot(effSmoothN, V), 0.0, 1.0);

    vec3 effDetailN = viewingFromBelow ? -detailNormal : detailNormal;
    float NdotV_detail = clamp(dot(effDetailN, V), 0.001, 1.0);

    vec3 L = normalize(pc.sunDir.xyz);

    vec3 waterSurfaceColor;
    float finalAlpha;

    if (viewingFromBelow || cameraUnderwater) {
        // -------------------------------------------------------------
        // UNDERWATER VIEW: Organic Snell's Window & Total Internal Reflection (TIR)
        // -------------------------------------------------------------
        vec3 N_down = -smoothNormal;
        float cosIncidence = clamp(dot(-V, N_down), 0.0, 1.0);

        // Snell's Law Critical Angle:
        // eta = 1.333 (water to air)
        const float eta = 1.333;
        float sin2_t = (eta * eta) * (1.0 - cosIncidence * cosIncidence);
        bool isTIR = (sin2_t >= 1.0);

        // Organic fluid caustic light webs (no rigid checkerboard!)
        vec2 cp1 = pos * 0.70 + vec2(t * 0.22, t * 0.15);
        vec2 cp2 = pos * 0.95 - vec2(t * 0.18, -t * 0.25);
        float cw1 = sin(cp1.x * 2.2 + sin(cp1.y * 1.8 + t * 0.8));
        float cw2 = cos(cp1.y * 2.4 + cos(cp1.x * 1.9 - t * 0.7));
        float cw3 = sin(cp2.x * 3.1 + cos(cp2.y * 2.5 + t * 0.9));
        float cw4 = cos(cp2.y * 2.8 + sin(cp2.x * 2.7 - t * 0.6));
        float caustic1 = 1.0 - abs(cw1 + cw2) * 0.5;
        float caustic2 = 1.0 - abs(cw3 + cw4) * 0.5;
        float fluidCaustic = pow(clamp(caustic1 * caustic2, 0.0, 1.0), 2.2);

        vec3 deepUnderwaterFog = vec3(0.008, 0.045, 0.080);
        vec3 shallowUnderwaterFog = vec3(0.015, 0.075, 0.120);
        vec3 underWaterTint = mix(shallowUnderwaterFog, deepUnderwaterFog, clamp(waterDepthMeters / 6.0, 0.0, 1.0));
        float goldenHour = smoothstep(0.40, 0.02, pc.dayInfo.y) * step(-0.06, pc.dayInfo.y);

        if (isTIR) {
            // Outside Snell's window: Total Internal Reflection (TIR)
            // Mirrors the underwater floor and depth in shimmering dark teal
            vec3 R_tir = reflect(-V, N_down);
            vec3 mirroredCeiling = mix(deepUnderwaterFog * 0.85, vec3(0.004, 0.025, 0.055), clamp(-R_tir.y, 0.0, 1.0));
            mirroredCeiling += vec3(0.02, 0.06, 0.08) * fluidCaustic * isDay;
            waterSurfaceColor = mirroredCeiling;
            finalAlpha = 0.88;
        } else {
            // Inside Snell's Window: Refraction into the outside sky, sun, and clouds
            // Realistic optical dispersion: red refracts less, blue refracts more (chromatic fringe)
            vec3 N_refractNormal = normalize(mix(vec3(0.0, -1.0, 0.0), N_down, 0.40));
            vec3 R_refractG = refract(-V, N_refractNormal, 1.333);
            vec3 R_refractR = refract(-V, N_refractNormal, 1.328);
            vec3 R_refractB = refract(-V, N_refractNormal, 1.339);

            if (length(R_refractG) < 0.01) R_refractG = vec3(0.0, 1.0, 0.0);
            if (length(R_refractR) < 0.01) R_refractR = R_refractG;
            if (length(R_refractB) < 0.01) R_refractB = R_refractG;

            vec3 skyRefractG = computeProceduralSky(R_refractG, L, isDay, sunIntensity, goldenHour);
            vec3 skyRefractR = computeProceduralSky(R_refractR, L, isDay, sunIntensity, goldenHour);
            vec3 skyRefractB = computeProceduralSky(R_refractB, L, isDay, sunIntensity, goldenHour);
            vec3 skyRefract = vec3(skyRefractR.r, skyRefractG.g, skyRefractB.b);

            // Refract 3D volumetric clouds through Snell's window
            if (R_refractG.y > 0.02) {
                float tCloud = (196.0 - fragWorldPos.y) / max(R_refractG.y, 0.02);
                if (tCloud > 0.0 && tCloud < 5000.0) {
                    vec3 pCloud = fragWorldPos + R_refractG * tCloud;
                    vec2 ws = (pCloud.xz + vec2(pc.camPos.w * 4.2, pc.camPos.w * 1.6)) * 0.0032;
                    float macroNoise = cloudFBM2D(ws);
                    if (macroNoise > 0.40) {
                        float cDensity = smoothstep(0.40, 0.68, macroNoise);
                        float cAlpha = clamp(cDensity * 1.8, 0.0, 0.96);
                        float silver = pow(max(dot(R_refractG, L) * 0.5 + 0.5, 0.0), 3.0) * 0.70 + 0.85;
                        vec3 goldenCloud = mix(vec3(1.50, 1.45, 1.35), vec3(2.40, 1.65, 0.85), goldenHour);
                        vec3 cloudLit = mix(vec3(0.25, 0.30, 0.42), goldenCloud * silver, isDay);
                        skyRefract = mix(skyRefract, cloudLit, cAlpha);
                    }
                }
            }

            // Smooth anti-aliased sun disc through Snell's window (no flashing pixel steps)
            float cosSun = dot(R_refractG, L);
            float sunDisc = smoothstep(0.996, 0.9995, cosSun);
            skyRefract += vec3(1.2, 1.1, 0.95) * (sunDisc * isDay * sunIntensity);

            // Subtle chromatic fringe at the rim of Snell's window
            float windowBorder = smoothstep(0.85, 0.99, sin2_t);
            vec3 fringeColor = mix(vec3(0.10, 0.22, 0.35), vec3(0.45, 0.35, 0.20), goldenHour);
            skyRefract = mix(skyRefract, fringeColor, windowBorder * 0.35);

            // Fluid caustic wash on surface
            skyRefract += vec3(0.05, 0.14, 0.18) * fluidCaustic * isDay;

            waterSurfaceColor = mix(skyRefract, underWaterTint, 0.18);
            finalAlpha = mix(0.28, 0.85, windowBorder);
        }

        // Volumetric forward-scattered sunlight beams penetrating the surface
        float cosSunBeam = dot(-V, L);
        float phaseHG = (1.0 - 0.4225) / pow(1.0 + 0.4225 - 2.0 * 0.65 * cosSunBeam, 1.5) * 0.079577;
        float sunBeams = fluidCaustic * (phaseHG * 2.8 + 0.25) * sunIntensity * isDay;
        vec3 beamColor = mix(vec3(0.12, 0.38, 0.52), vec3(0.95, 0.88, 0.60), goldenHour);
        waterSurfaceColor += beamColor * sunBeams;

    } else {
        // -------------------------------------------------------------
        // ABOVE WATER VIEW: Dual-Normal Coherent Reflections & GGX Sun Glints
        // -------------------------------------------------------------
        // 1. Schlick Fresnel evaluated with smooth macro normal (no micro noise!)
        float fresnel = 0.04 + 0.96 * pow(clamp(1.0 - NdotV_smooth, 0.0, 1.0), 5.0);

        // 2. Mirror-coherent Screen-Space Reflection using smooth normal
        vec3 R = reflect(-V, effSmoothN);
        vec3 reflectedScene = getReflectionColor(fragWorldPos, R, L, isDay, sunIntensity, optWaterQuality);

        // 3. GGX Microfacet Specular Sun Reflection using DETAIL NORMAL
        // Gives glistening sun sparkle across the surface without perturbing reflected trees/clouds
        vec3 H = normalize(L + V);
        float goldenHour = smoothstep(0.40, 0.02, pc.dayInfo.y) * step(-0.06, pc.dayInfo.y);

        float roughness = 0.038;
        float D = DistributionGGX(effDetailN, H, roughness);
        float G = GeometrySmith(effDetailN, V, L, roughness);
        float F_sun = 0.04 + 0.96 * pow(1.0 - max(dot(V, H), 0.0), 5.0);

        float NdotL = max(dot(effDetailN, L), 0.0);
        float specularGGX = (D * F_sun * G) / (4.0 * max(NdotV_detail, 0.001) * max(NdotL, 0.001) + 0.0001);

        float rtShadow = vibrant ? sampleRealtimeShadow(fragWorldPos, effDetailN, L) : 1.0;
        vec3 sunGlintColor = mix(vec3(2.80, 2.60, 2.30), vec3(4.20, 2.80, 1.20), goldenHour);
        vec3 sunGlint = specularGGX * NdotL * sunIntensity * sunGlintColor * isDay * rtShadow;

        // Subsurface Water Scattering: subtle deep oceanic glow through wave crests
        vec3 sssColor = vec3(0.0);
        if (vibrant && optWaterQuality >= 2) {
            float sss = pow(clamp(dot(V, -L), 0.0, 1.0), 3.5) * exp(-waterDepthMeters * 0.25) * 0.20 * isDay;
            sssColor = vec3(0.008, 0.045, 0.085) * sss * rtShadow;
        }

        // Shoreline Foam Wash where continuous water depth drops below 0.35m
        float shoreFoamMask = smoothstep(0.35, 0.02, waterDepthMeters);
        float foamNoise = sin(pos.x * 3.5 + t * 1.8) * cos(pos.y * 3.5 - t * 1.5) * 0.5 + 0.5;
        float foam = shoreFoamMask * smoothstep(0.30, 0.75, foamNoise) * isDay;
        vec3 foamColor = vec3(0.95, 0.98, 1.0) * (foam * 0.40);

        // Aquatic Reflection onto Deep Navy Ocean Water
        vec3 reflTint = mix(vec3(0.92, 0.94, 0.97), vec3(1.0, 1.0, 1.0), fresnel);
        vec3 reflColor = reflectedScene * reflTint;

        // Balanced Fresnel mix: looking down keeps rich dark navy body prominent, grazing angles transition to mirror reflection
        float reflFactor = clamp(fresnel * 0.80 + 0.03, 0.0, 0.88);

        waterSurfaceColor = mix(waterBedColor, reflColor, reflFactor) + sunGlint + foamColor + sssColor;
        finalAlpha = clamp(baseAlpha + fresnel * (1.0 - baseAlpha) + foam * 0.30, 0.0, 0.96);
    }


    // Physically-based specular point light reflections on water waves (Torches)
    vec3 torchSpecular = vec3(0.0);
    vec3 torchColor = vec3(1.2, 0.85, 0.40);

    if (pc.heldTorch.w > 0.001) {
        vec3 toHeld = pc.heldTorch.xyz - fragWorldPos;
        float dHeld = length(toHeld);
        if (dHeld < 16.0) {
            vec3 L_held = toHeld / dHeld;
            vec3 H_held = normalize(L_held + V);
            float NdotH = max(dot(effDetailN, H_held), 0.0);
            float spec = pow(NdotH, 64.0);
            float atten = clamp(1.0 - dHeld / 16.0, 0.0, 1.0);
            torchSpecular += torchColor * (spec * atten * atten * 1.8 * pc.heldTorch.w);
        }
    }

    if (pc.pointLight1.w > 0.001) {
        vec3 toLight = pc.pointLight1.xyz - fragWorldPos;
        float dLight = length(toLight);
        if (dLight < 16.0) {
            vec3 L_light = toLight / dLight;
            vec3 H_light = normalize(L_light + V);
            float NdotH = max(dot(effDetailN, H_light), 0.0);
            float spec = pow(NdotH, 64.0);
            float atten = clamp(1.0 - dLight / 16.0, 0.0, 1.0);
            torchSpecular += torchColor * (spec * atten * atten * 1.5 * pc.pointLight1.w);
        }
    }
    if (pc.pointLight2.w > 0.001) {
        vec3 toLight = pc.pointLight2.xyz - fragWorldPos;
        float dLight = length(toLight);
        if (dLight < 16.0) {
            vec3 L_light = toLight / dLight;
            vec3 H_light = normalize(L_light + V);
            float NdotH = max(dot(effDetailN, H_light), 0.0);
            float spec = pow(NdotH, 64.0);
            float atten = clamp(1.0 - dLight / 16.0, 0.0, 1.0);
            torchSpecular += torchColor * (spec * atten * atten * 1.5 * pc.pointLight2.w);
        }
    }

    waterSurfaceColor += torchSpecular;

    // Atmospheric Perspective & Distance Fog
    float fogEnd = abs(pc.skyFog.w);
    float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);

    if (cameraUnderwater) {
        float dist = length(fragWorldPos - pc.camPos.xyz);
        float uFactor = smoothstep(fogEnd * 0.40, fogEnd, dist);
        vec3 linearOceanFog = srgbToLinear(pc.skyFog.rgb);
        waterSurfaceColor = mix(waterSurfaceColor, linearOceanFog, uFactor);
        finalAlpha = mix(finalAlpha, 1.0, uFactor);
    } else {
        waterSurfaceColor = applyHorizonDistanceFog(
            waterSurfaceColor,
            fragWorldPos,
            pc.camPos.xyz,
            L,
            pc.skyFog.rgb,
            isDay,
            goldenHour,
            fogEnd
        );
    }


    // Output linear HDR radiance directly to HDR buffer (master tonemap handled in post-processing pass)
    outColor = vec4(waterSurfaceColor, finalAlpha);
}
