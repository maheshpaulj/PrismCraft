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
    vec3 zenithDay   = vec3(0.16, 0.40, 0.88);
    vec3 horizonDay  = vec3(0.55, 0.70, 0.92);
    vec3 zenithDusk  = vec3(0.14, 0.16, 0.38);
    vec3 horizonDusk = vec3(0.96, 0.52, 0.22);
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
    vec3 hazeDay = mix(vec3(0.68, 0.78, 0.90), vec3(0.98, 0.65, 0.36), goldenHour);
    vec3 hazeColor = mix(vec3(0.014, 0.019, 0.032), hazeDay, isDay);
    skyRgb = mix(skyRgb, hazeColor, hazeBand * 0.35);

    // Solar atmospheric forward warmth
    float cosSun = dot(R, L);
    if (cosSun > 0.0 && isDay > 0.05) {
        float sunWarmth = pow(clamp(cosSun * 0.5 + 0.5, 0.0, 1.0), 3.0) * 0.35;
        vec3 warmthColor = mix(vec3(0.28, 0.25, 0.16), vec3(0.70, 0.38, 0.12), goldenHour);
        skyRgb += warmthColor * (sunWarmth * isDay);

        // Soft circumsolar corona glow
        float corona = pow(clamp(cosSun, 0.0, 1.0), 24.0) * 0.70;
        vec3 coronaCol = mix(vec3(1.2, 1.1, 0.9), vec3(1.8, 1.2, 0.5), goldenHour);
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
vec4 traceSSR(vec3 rayOrigin, vec3 rayDir, vec2 waveGrad) {
    const float maxDist = 85.0;
    const int maxSteps = 40;
    float t = 0.25;
    float dt = 0.35;
    float tPrev = t;

    vec2 hitUV = vec2(0.0);
    bool hitFound = false;

    // Deflect ray in world space by local wave normals
    vec3 marchDir = normalize(rayDir + vec3(waveGrad.x, 0.0, waveGrad.y) * 0.12);

    for (int i = 0; i < maxSteps && t < maxDist; ++i) {
        vec3 p = rayOrigin + marchDir * t;
        vec4 clip = pc.mvp * vec4(p, 1.0);

        if (clip.w <= 0.05) {
            tPrev = t;
            t += dt;
            dt *= 1.05;
            continue;
        }

        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
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
            dt *= 1.06;
            continue;
        }

        float sceneLinearDepth = linearizeDepth(sceneRawDepth);

        // Thickness tolerance in meters: expands with distance to avoid false pass-throughs
        float thickness = 0.60 + 0.04 * t;
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
                    vec2 uvMid = (cMid.xy / cMid.w) * 0.5 + 0.5;
                    float sDepth = linearizeDepth(texture(depthSampler, uvMid).r);
                    if (cMid.w >= sDepth) {
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
        dt *= 1.06;
    }

    if (!hitFound) {
        return vec4(0.0);
    }

    // Screen edge vignetting: tight 1.5% falloff so reflections reach right to the borders
    float edgeFade = smoothstep(0.0, 0.015, hitUV.x) *
                     smoothstep(0.0, 0.015, 1.0 - hitUV.x) *
                     smoothstep(0.0, 0.015, hitUV.y) *
                     smoothstep(0.0, 0.015, 1.0 - hitUV.y);

    // Distance fade as ray approaches maximum reach
    float distFade = 1.0 - smoothstep(maxDist * 0.65, maxDist, t);
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
// Real-Time Screen-Space Reflection (SSR) & Sky Dome
// -------------------------------------------------------------
vec3 getReflectionColor(vec3 origin, vec3 R, vec3 L, float isDay, float sunIntensity, int waterQuality, vec2 waveGrad) {
    float goldenHour = smoothstep(0.40, 0.02, pc.dayInfo.y) * step(-0.06, pc.dayInfo.y);
    vec3 sky = computeProceduralSky(R, L, isDay, sunIntensity, goldenHour);

    // 1. Procedural 3D volumetric cumulus clouds (high-contrast, puffy clouds reflecting on water)
    if (R.y > 0.015 && waterQuality >= 1) {
        float tCloud = (196.0 - origin.y) / max(R.y, 0.02);
        if (tCloud > 0.0 && tCloud < 6000.0) {
            vec3 pCloud = origin + R * tCloud;
            vec2 wind = vec2(pc.camPos.w * 4.2, pc.camPos.w * 1.6);
            vec2 ws = pCloud.xz + wind;

            float macroNoise = cloudFBM(vec3(ws * 0.00028, 0.5));
            if (macroNoise > 0.22) {
                float cDensity = smoothstep(0.22, 0.48, macroNoise);
                float cAlpha = clamp(cDensity * 2.0, 0.0, 0.96);
                float silver = pow(max(dot(R, L) * 0.5 + 0.5, 0.0), 3.0) * 0.70 + 0.85;
                vec3 goldenCloud = mix(vec3(1.50, 1.45, 1.35), vec3(2.35, 1.55, 0.75), goldenHour);
                vec3 cloudLit = mix(vec3(0.25, 0.30, 0.42), goldenCloud * silver, isDay);
                sky = mix(sky, cloudLit, cAlpha);
            }
        }
    }

    // 2. Real-time Screen Space Reflections for nearby geometry (river banks, trees, terrain)
    if (waterQuality >= 2) {
        vec4 ssr = traceSSR(origin, R, waveGrad);
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

    // Horizon fade start: 70% of render distance (clean, crisp nearby world, smooth horizon edge fade)
    float startRatio = 0.70;
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
    vec3 sigmaA = vec3(0.38, 0.14, 0.04);
    float waterDepthMeters = depthFactor * 8.0;
    vec3 transmittance = exp(-waterDepthMeters * sigmaA);

    // Deep ocean bed: rich oceanic sapphire navy (sea dark blue)
    vec3 deepWaterRadiance = mix(vec3(0.004, 0.018, 0.070), vec3(0.015, 0.075, 0.260), isDay);
    // Shallow waterbed: luminous marine azure / cyan-blue
    vec3 shallowWaterRadiance = mix(vec3(0.010, 0.045, 0.110), vec3(0.045, 0.165, 0.380), isDay);

    vec3 waterBedColor = mix(deepWaterRadiance, shallowWaterRadiance, transmittance);

    // Soft shoreline edge blending: smooth transition at the immediate beach edge,
    // but deeper than 1 block is dark and opaque (eliminating see-through look)
    float shoreEdge = smoothstep(0.008, 0.08, depthFactor);
    float baseAlpha = mix(0.55, 0.95, smoothstep(0.02, 0.50, depthFactor)) * shoreEdge;

    // -------------------------------------------------------------
    // 2. Broad Ocean Swells & Dynamic Waves (Natural, Rolling Ocean Water)
    // -------------------------------------------------------------
    float t = pc.camPos.w * 0.95;
    vec2 pos = fragWorldPos.xz;
    float distToCam = length(fragWorldPos - pc.camPos.xyz);

    // Swell 1: Primary deep ocean swell (Wavelength = 32m)
    vec2  d1 = vec2(0.8, 0.6);
    float k1 = 0.1963;
    float a1 = 0.055;
    float w1 = dot(pos, d1) * k1 - t * 1.25;

    // Swell 2: Secondary diagonal roll (Wavelength = 19m)
    vec2  d2 = vec2(-0.6, 0.8);
    float k2 = 0.3307;
    float a2 = 0.036;
    float w2 = dot(pos, d2) * k2 + t * 1.45;

    // Swell 3: Soft ambient cross-swell (Wavelength = 11m)
    vec2  d3 = vec2(0.5, -0.86);
    float k3 = 0.5712;
    float a3 = 0.022;
    float w3 = dot(pos, d3) * k3 - t * 1.85;

    // Swell 4: Surface ripple roll (Wavelength = 5.5m)
    vec2  d4 = vec2(-0.7071, -0.7071);
    float k4 = 1.1424;
    float a4 = 0.012;
    float w4 = dot(pos, d4) * k4 + t * 2.30;

    // Distance LOD fade: smoothly calm ripples at distance to eliminate shimmering
    float fadeDist = 1.0 - smoothstep(60.0, 320.0, distToCam);

    // Dynamic wave gradient (normal slope) with clearly defined rolling waves and glistening crests
    vec2 waveGrad = (d1 * (cos(w1) * k1 * a1) +
                     d2 * (cos(w2) * k2 * a2) +
                     d3 * (cos(w3) * k3 * a3) +
                     d4 * (cos(w4) * k4 * a4) * fadeDist) * 1.10;

    // -------------------------------------------------------------
    // 3. Directional Flow Currents for Streams & Waterfalls
    // -------------------------------------------------------------
    bool isTopFace = (abs(fragNormal.y) > 0.7);
    vec2 flowDir = fragNormal.xz;
    float flowLen = length(flowDir);
    bool isFlowing = (flowLen > 0.02);

    if (isTopFace && isFlowing) {
        vec2 fDir = normalize(flowDir);
        float fPhase0 = fract(t * 1.3);
        float fPhase1 = fract(t * 1.3 + 0.5);
        float fW0 = 1.0 - abs(fPhase0 - 0.5) * 2.0;
        float fW1 = 1.0 - abs(fPhase1 - 0.5) * 2.0;
        float r0 = sin(dot(pos - fDir * fPhase0 * 1.6, fDir) * 4.5) * 0.04;
        float r1 = sin(dot(pos - fDir * fPhase1 * 1.6, fDir) * 4.5) * 0.04;
        waveGrad += fDir * (r0 * fW0 + r1 * fW1);
    }

    // Authentic Minecraft water texture tint from texture atlas
    vec3 waterTex = texture(texSampler, fragTexCoord).rgb;
    vec3 texModulation = mix(vec3(1.0), waterTex * 1.35 + 0.12, 0.22);
    waterBedColor *= texModulation;

    vec3 N = normalize(fragNormal);
    if (isTopFace) {
        N = normalize(vec3(-waveGrad.x, (N.y > 0.0 ? 1.0 : -1.0), -waveGrad.y));
    } else {
        // Lateral water wall (waterfalls / edge drops): downward water ripple
        float downRipple = sin((fragWorldPos.y + t * 2.5) * 6.0) * 0.05;
        N = normalize(vec3(fragNormal.x, downRipple, fragNormal.z));
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
        // Smooth progressive Fresnel curve (water builds reflection across the lake)
        float fresnel = 0.05 + 0.95 * pow(clamp(1.0 - NdotV, 0.0, 1.0), 2.2);

        // Trace screen-space planar reflection (trees, hills, shoreline, clouds)
        vec3 R = reflect(-V, effN);
        vec3 reflectedScene = getReflectionColor(fragWorldPos, R, L, isDay, sunIntensity, optWaterQuality, waveGrad);

        // GGX Microfacet Specular Sun Reflection (Glittering Sun Trail)
        vec3 H = normalize(L + V);
        float goldenHour = smoothstep(0.40, 0.02, pc.dayInfo.y) * step(-0.06, pc.dayInfo.y);

        float roughness = 0.052; // Smooth liquid surface for crisp, glistening sun reflections
        float D = DistributionGGX(effN, H, roughness);
        float G = GeometrySmith(effN, V, L, roughness);
        float F_sun = 0.04 + 0.96 * pow(1.0 - max(dot(V, H), 0.0), 5.0);

        float NdotL = max(dot(effN, L), 0.0);
        float specularGGX = (D * F_sun * G) / (4.0 * max(NdotV, 0.001) * max(NdotL, 0.001) + 0.0001);

        float rtShadow = vibrant ? sampleRealtimeShadow(fragWorldPos, effN, L) : 1.0;
        vec3 sunGlintColor = mix(vec3(2.40, 2.25, 2.05), vec3(3.40, 2.20, 0.95), goldenHour);
        vec3 sunGlint = specularGGX * NdotL * sunIntensity * sunGlintColor * isDay * rtShadow;

        // Subsurface Water Scattering: subtle emerald glow through wave crests
        vec3 sssColor = vec3(0.0);
        if (vibrant && optWaterQuality >= 2) {
            float sss = pow(clamp(dot(V, -L), 0.0, 1.0), 3.5) * (1.0 - depthFactor * 0.45) * 0.25 * isDay;
            sssColor = vec3(0.01, 0.10, 0.14) * sss * rtShadow;
        }

        // Shoreline Foam Wash along shallow banks
        float foamEdge = smoothstep(0.12, 0.015, depthFactor);
        float foamWave = 0.5 + 0.5 * sin(dot(pos, d1) * k1 * 1.5 - t * 1.6);
        float foam = foamEdge * smoothstep(0.42, 0.85, foamWave) * isDay;
        vec3 foamColor = vec3(0.92, 0.96, 1.0) * (foam * 0.40);

        // -------------------------------------------------------------
        // Vibrant Aquatic Reflection onto Rich Ocean Deep Blue Water Surface
        // -------------------------------------------------------------
        // Reflected clouds and sky are clearly visible on the surface with natural aquatic tinting.
        // The water retains its deep sapphire oceanic body and never looks like a flat silver mirror.
        vec3 reflTint = mix(vec3(0.70, 0.85, 1.05), vec3(0.95, 0.98, 1.0), fresnel);
        vec3 reflColor = reflectedScene * reflTint;

        // Balanced Fresnel mix:
        // - Steep angles (looking down): deep blue water bed dominates (~75%),
        //   while sky & cloud reflections are clearly visible (~25%) without washing out the blue.
        // - Grazing angles (looking across): reflections reach up to 88%.
        float reflFactor = clamp(fresnel * 0.65 + 0.22, 0.0, 0.88);

        waterSurfaceColor = mix(waterBedColor, reflColor, reflFactor) + sunGlint + foamColor + sssColor;
        finalAlpha = clamp(baseAlpha + fresnel * (1.0 - baseAlpha) + foam * 0.30, 0.40, 0.96);
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
            fogEnd
        );
    }


    // Output linear HDR radiance directly to HDR buffer (master tonemap handled in post-processing pass)
    outColor = vec4(waterSurfaceColor, finalAlpha);
}
