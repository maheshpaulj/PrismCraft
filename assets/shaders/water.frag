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
    vec4 pointLight1; // xyz = dropped torch pos, w = intensity
    vec4 pointLight2; // xyz = placed torch pos, w = intensity (y = waterQuality: 0=Fast, 1=Fancy, 2=RTX)
    vec4 heldTorch;   // xyz = held torch pos, w = active
} pc;

layout(location = 0) out vec4 outColor;

// ACES Filmic Tonemapper matching cell.frag
vec3 acesFilmicTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
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
    float tCloud = (235.0 - worldPos.y) / L.y;
    if (tCloud <= 0.0) return 1.0;
    vec2 cloudHit = worldPos.xz + L.xz * tCloud;
    vec2 wind = vec2(pc.camPos.w * 2.0, 0.0);
    vec2 ws = cloudHit + wind;

    // Macro cluster coordinate matching cloud.frag: 0.00078
    vec2 pMacro = ws * 0.00078;
    float macro = sin(pMacro.x * 3.14 + cos(pMacro.y * 2.5)) * cos(pMacro.y * 3.14) * 0.5 + 0.5;
    float coverage = smoothstep(0.44, 0.62, macro);
    return 1.0 - coverage * 0.65;
}

// -------------------------------------------------------------
// Volumetric Crepuscular God Rays (Light Shafts through clouds & over water)
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

    // 1. Sun Occlusion by Distant Mountains / LOD Hills
    float sunOcclusion = 1.0;
    if (L.y > 0.02) {
        for (int s = 1; s <= 5; ++s) {
            float testDist = float(s) * 48.0;
            vec3 testP = rayOrigin + L * testDist;
            float mountainY = 56.0 + 34.0 * sin(testP.x * 0.0075) * cos(testP.z * 0.0075 + 0.4) + 12.0 * sin(testP.x * 0.016);
            if (testP.y < mountainY) {
                float diff = mountainY - testP.y;
                sunOcclusion = min(sunOcclusion, clamp(1.0 - diff * 0.18, 0.0, 1.0));
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

// -------------------------------------------------------------
// Real-Time Scene Reflection (Sky, 3D Clouds & Distant Mountains)
// -------------------------------------------------------------
vec3 traceSceneReflection(vec3 origin, vec3 R, vec3 L, float isDay, float sunIntensity, int waterQuality, float dither) {
    float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);

    // 1. Atmospheric Sky Dome Gradient matching golden-hour reference image
    vec3 skyZenith  = mix(vec3(0.03, 0.06, 0.15), vec3(0.14, 0.38, 0.78), isDay);
    vec3 skyHorizon = mix(pc.skyFog.rgb, mix(vec3(0.60, 0.76, 0.92), vec3(1.50, 1.10, 0.60), goldenHour * 0.45), isDay);
    vec3 skyColor   = mix(skyHorizon, skyZenith, clamp(R.y * 1.8, 0.0, 1.0));

    // 2. Reflected 3D Volumetric Cotton Candy Clouds in the sky
    if (R.y > 0.015 && waterQuality >= 1) {
        float tCloud = (235.0 - origin.y) / max(R.y, 0.02);
        if (tCloud > 0.0 && tCloud < 6500.0) {
            vec3 pCloud = origin + R * tCloud;
            vec2 wind = vec2(pc.camPos.w * 2.0, 0.0);
            vec3 ws = pCloud + vec3(wind.x, 0.0, wind.y);

            // Planetary curved altitude
            float dH = length(pCloud.xz - pc.camPos.xz);
            float yCurv = pCloud.y + (dH * dH) / (2.0 * 90000.0);

            if (yCurv >= 230.0 && yCurv <= 395.0) {
                float macroNoise = cloudFBM(ws * 0.00072);
                if (macroNoise > 0.43) {
                    float cDensity = smoothstep(0.43, 0.65, macroNoise);
                    if (cDensity > 0.0) {
                        float cAlpha = clamp(cDensity * 1.6, 0.0, 1.0);
                        float cosTh = dot(R, L);
                        float silver = pow(max(cosTh * 0.5 + 0.5, 0.0), 3.0) * 0.55 + 0.85;
                        vec3 goldenCloud = mix(vec3(1.30, 1.20, 1.02), vec3(1.95, 1.35, 0.72), goldenHour);
                        vec3 cloudLit = mix(vec3(0.40, 0.46, 0.58), goldenCloud * silver, isDay);
                        skyColor = mix(skyColor, cloudLit, cAlpha * 0.92);
                    }
                }
            }
        }
    }

    // 3. Shoreline, Far Mountain & Tree Reflection
    vec3 terrainReflection = vec3(0.0);
    float terrainWeight = 0.0;

    if (R.y < 0.42) {
        // A. Extended Raymarching against Cascade 0 & Cascade 1 (up to 250m)
        if (waterQuality >= 1) {
            int maxSteps = (waterQuality >= 2) ? 26 : 16;
            float dist = 0.8 + dither * 0.6;
            float maxDist = (waterQuality >= 2) ? 250.0 : 160.0;

            for (int i = 0; i < maxSteps; ++i) {
                if (dist >= maxDist) break;

                vec3 P = origin + R * dist;

                // 1. Cascaded shadow map hit check (near and mid-distance blocks and trees)
                int cascade = (dist > shadowUBO.cascadeSplits.x) ? 1 : 0;
                vec4 sc = shadowUBO.lightViewProj[cascade] * vec4(P, 1.0);
                vec3 coords = sc.xyz / sc.w;
                if (coords.x >= 0.01 && coords.x <= 0.99 &&
                    coords.y >= 0.01 && coords.y <= 0.99 &&
                    coords.z >= 0.01 && coords.z <= 0.99) {
                    float shadowZ = texture(shadowMap, vec4(coords.xy, float(cascade), coords.z));
                    if (shadowZ < 0.5) {
                        float hAbove = P.y - origin.y;
                        vec3 foliageGreen = vec3(0.05, 0.16, 0.04) * mix(0.40, 1.25, goldenHour);
                        vec3 bankEarth    = vec3(0.22, 0.16, 0.10) * mix(0.40, 1.05, goldenHour);
                        vec3 hitColor     = (hAbove > 0.8) ? foliageGreen : bankEarth;

                        float sunDiffuse = clamp(dot(vec3(-R.x, 0.6, -R.z), L), 0.25, 1.0);
                        hitColor *= mix(0.45, 1.10, sunDiffuse * isDay);

                        float fade = clamp(dist / maxDist, 0.0, 1.0);
                        terrainReflection = mix(hitColor, pc.skyFog.rgb, fade * 0.45);
                        terrainWeight = (1.0 - fade * 0.35);
                        break;
                    }
                }

                // 2. Distant mountain silhouette reflection (for terrain beyond cascades, up to 350m)
                if (dist > 35.0) {
                    float hillHeight = 56.0 + 34.0 * sin(P.x * 0.0075) * cos(P.z * 0.0075 + 0.4) + 12.0 * sin(P.x * 0.016);
                    if (P.y <= hillHeight) {
                        vec3 mountainGreen = vec3(0.06, 0.15, 0.05) * mix(0.45, 1.15, goldenHour);
                        float distFade = clamp(dist / 350.0, 0.0, 0.75);
                        terrainReflection = mix(mountainGreen, pc.skyFog.rgb, distFade);
                        terrainWeight = 0.90 * (1.0 - distFade * 0.4);
                        break;
                    }
                }

                float stepSize = 0.8 + 0.15 * dist;
                dist += stepSize;
            }
        }

        // B. Shoreline presence fallback: for grazing angles towards the bank, ensure dark tree reflection
        if (terrainWeight < 0.15 && R.y < 0.22) {
            float shoreDir = clamp(dot(R.xz, normalize(vec2(1.0, 0.5))), 0.0, 1.0);
            float shoreFactor = smoothstep(0.22, 0.02, R.y) * shoreDir;
            vec3 bankGreen = vec3(0.06, 0.16, 0.05) * mix(0.50, 1.20, goldenHour);
            terrainReflection = mix(terrainReflection, bankGreen, shoreFactor);
            terrainWeight = max(terrainWeight, shoreFactor * 0.85);
        }
    }

    return mix(skyColor, terrainReflection, terrainWeight * isDay);
}

void main() {
    bool vibrant = (pc.sunDir.w > 0.0);
    float sunIntensity = abs(pc.sunDir.w);
    bool cameraUnderwater = (pc.skyFog.w < 0.0);
    float depthFactor = fragColor.r; // 0.0 = shore, 1.0 = deep
    float isDay = clamp(pc.skyFog.b * 1.8 - 0.25, 0.0, 1.0);

    vec3 V = normalize(pc.camPos.xyz - fragWorldPos);
    bool viewingFromBelow = (fragWorldPos.y > pc.camPos.y) || (dot(fragNormal, V) < 0.0);

    // -------------------------------------------------------------
    // 1. Natural Deep Pond Water Color Palette matching reference image
    // -------------------------------------------------------------
    // Deep open water: rich, glassy dark obsidian navy
    vec3 deepWaterColor = mix(vec3(0.008, 0.018, 0.040), vec3(0.012, 0.038, 0.075), isDay);

    // Shallow water: clear aquatic mossy-slate with warm amber undertones near the sandy bank
    vec3 shallowWaterColor = mix(vec3(0.015, 0.035, 0.055), vec3(0.035, 0.095, 0.110), isDay);

    float dSmooth = smoothstep(0.0, 0.85, depthFactor);
    vec3 waterBaseColor = mix(shallowWaterColor, deepWaterColor, dSmooth);

    // Crystal clear shallow transparency (see-through to riverbed) transitioning to opaque deeps
    float baseAlpha = mix(0.28, 0.88, dSmooth);

    // -------------------------------------------------------------
    // 2. Animated Directional Wind Wave Harmonics (AAA Wavy Liquid Surface)
    // -------------------------------------------------------------
    float t = pc.camPos.w * 1.6;
    vec2 pos = fragWorldPos.xz;

    // Multi-octave wave directions with distinct spatial frequencies & wave speeds
    vec2 d1 = normalize(vec2(0.86, 0.50));
    vec2 d2 = normalize(vec2(-0.48, 0.88));
    vec2 d3 = normalize(vec2(0.92, -0.38));
    vec2 d4 = normalize(vec2(-0.35, -0.94));
    vec2 d5 = normalize(vec2(0.65, 0.76));

    float w1 = dot(pos, d1) * 0.45 - t * 1.3;
    float w2 = dot(pos, d2) * 0.82 + t * 1.7;
    float w3 = dot(pos, d3) * 1.80 - t * 2.5;
    float w4 = dot(pos, d4) * 4.50 + t * 3.8;
    float w5 = dot(pos, d5) * 12.0 - t * 6.2;

    // Gerstner trochoidal wave steepness sharpening: sharp crests, broad rounded troughs
    float s1 = sin(w1); float c1 = cos(w1);
    float s2 = sin(w2); float c2 = cos(w2);
    float s3 = sin(w3); float c3 = cos(w3);
    float s4 = sin(w4); float c4 = cos(w4);
    float c5 = cos(w5);

    // Dynamic wave amplitude modulated by graphics profile
    float waveAmp = vibrant ? 1.0 : 0.75;

    // Gradient computation (wave slope vector) with Gerstner crest sharpening
    vec2 waveGrad = (
        d1 * (c1 * (1.0 + s1 * 0.4) * 0.135) +
        d2 * (c2 * (1.0 + s2 * 0.35) * 0.095) +
        d3 * (c3 * (1.0 + s3 * 0.3) * 0.065) +
        d4 * (c4 * 0.035) +
        d5 * (c5 * 0.018)
    ) * waveAmp;

    vec3 N = normalize(fragNormal);
    if (abs(N.y) > 0.5) {
        N = normalize(vec3(-waveGrad.x, (N.y > 0.0 ? 1.0 : -1.0), -waveGrad.y));
    }

    vec3 effN = N;
    if (viewingFromBelow) {
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

        vec3 underWaterTint = mix(vec3(0.08, 0.35, 0.52), vec3(0.03, 0.16, 0.32), dSmooth);
        waterSurfaceColor = mix(underWaterTint, vec3(0.02, 0.08, 0.22), internalReflection * 0.7) + sunBeams;

    } else {
        // -------------------------------------------------------------
        // ABOVE WATER VIEW: Schlick Fresnel Reflection & Golden Sun Trail
        // -------------------------------------------------------------
        int optWaterQuality = int(pc.pointLight2.y + 0.5);
        bool isRTX = vibrant && (optWaterQuality >= 2);

        // Screen-space dither jitter for smooth raymarched reflections
        float dither = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);

        // Schlick Fresnel approximation:
        // Normal incidence (looking down): F = 0.025 (clear, transparent, see riverbed)
        // Grazing angles (across pond): F -> 0.92 (rich reflective mirror of sky, trees, and golden sun)
        float fresnel = 0.025 + 0.895 * pow(1.0 - NdotV, 4.2);

        // Reflection ray
        vec3 R = reflect(-V, effN);
        vec3 reflectedScene = traceSceneReflection(fragWorldPos, R, L, isDay, sunIntensity, optWaterQuality, dither);

        // Dual-Lobe Golden Sun Specular Trail:
        // 1. Sharp core glint
        // 2. Wide shimmering golden-hour sun path across wave slopes
        vec3 H = normalize(L + V);
        float NdotH = max(dot(effN, H), 0.0);
        float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);
        vec3 midDaySun = vec3(1.15, 1.08, 0.95);
        vec3 goldenSun = vec3(1.68, 1.20, 0.58);
        vec3 activeSun = mix(midDaySun, goldenSun, goldenHour);

        float specSharp = pow(NdotH, 180.0) * 4.5;
        float specWide  = pow(NdotH, 14.0)  * 1.8 * goldenHour; // Radiant golden sun trail towards sun
        float rtShadow  = vibrant ? sampleRealtimeShadow(fragWorldPos, effN, L) : 1.0;
        vec3 sunGlint   = (specSharp + specWide) * sunIntensity * activeSun * rtShadow;

        // Subsurface Water Scattering (RTX Mode)
        vec3 sssColor = vec3(0.0);
        if (isRTX) {
            float sss = pow(clamp(dot(V, -L), 0.0, 1.0), 3.5) * (1.0 - dSmooth * 0.6) * 0.35 * isDay;
            sssColor = vec3(0.08, 0.65, 0.70) * sss * rtShadow;
        }

        // Shoreline foam wave along shallow banks
        float foamEdge = smoothstep(0.22, 0.02, depthFactor);
        float foamWave = 0.5 + 0.5 * sin(pos.x * 5.5 + pos.y * 5.5 + t * 3.0);
        float foamNoise = isRTX ? (sin(pos.x * 11.0 + t * 3.8) * cos(pos.y * 11.0 - t * 2.8) * 0.18) : 0.0;
        float foam = foamEdge * smoothstep(0.38, 0.78, foamWave + foamNoise) * isDay;
        vec3 foamColor = vec3(0.85, 0.94, 1.0) * (foam * 0.65);

        // Composite water color
        waterSurfaceColor = mix(waterBaseColor, reflectedScene, fresnel) + sunGlint + foamColor + sssColor;
        finalAlpha = clamp(mix(baseAlpha, 0.95, fresnel) + foam * 0.3, 0.0, 1.0);
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
    float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);
    vec3 goldenFog = mix(pc.skyFog.rgb, vec3(1.50, 1.10, 0.60), goldenHour * 0.40);
    vec3 fogCol = cameraUnderwater ? vec3(0.02, 0.14, 0.40) : goldenFog;
    waterSurfaceColor = mix(waterSurfaceColor, fogCol, fogFactor);

    // Volumetric Crepuscular God Rays over Water
    if (vibrant && !cameraUnderwater) {
        vec3 godRays = computeGodRays(pc.camPos.xyz, fragWorldPos, L, isDay, goldenHour);
        waterSurfaceColor += godRays;
    }

    waterSurfaceColor = acesFilmicTonemap(waterSurfaceColor);

    outColor = vec4(waterSurfaceColor, finalAlpha);
}
