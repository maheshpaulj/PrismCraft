#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec3 fragWorldPos;

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 sunDir;     // xyz = normalized sun dir, w = sun intensity
    vec4 skyFog;     // xyz = fog color, w = fog distance
    vec4 camPos;     // xyz = camera pos, w = time
    vec4 playerPos;
    vec4 pointLight1;
    vec4 pointLight2;
    vec4 heldTorch;
} pc;

layout(location = 0) out vec4 outColor;

// -------------------------------------------------------------
// ACES Filmic Tonemapper matching cell.frag and water.frag
// -------------------------------------------------------------
vec3 acesFilmicTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// -------------------------------------------------------------
// Fast, Smooth 3D Procedural Noise
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

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);

    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);

    return mix(nxy0, nxy1, u.z);
}

// 3D Fractional Brownian Motion (fBm) for macro cumulus grouping
float cloudMacroFBM(vec3 p) {
    float f = 0.0;
    f += 0.54 * noise3D(p); p = p * 2.04 + vec3(1.7, 0.8, 2.3);
    f += 0.28 * noise3D(p); p = p * 2.08 + vec3(2.4, 1.5, 0.9);
    f += 0.18 * noise3D(p);
    return f;
}

// Multi-octave spherical cotton-candy billow noise: voluptuous, puffy lobes
float cloudBillows(vec3 p) {
    float b1 = 1.0 - 2.0 * abs(noise3D(p * 0.0022) - 0.5);
    float b2 = 1.0 - 2.0 * abs(noise3D(p * 0.0055 + vec3(1.4, 0.7, 2.3)) - 0.5);
    float b3 = 1.0 - 2.0 * abs(noise3D(p * 0.0115 + vec3(2.6, 1.9, 0.8)) - 0.5);
    float combined = b1 * 0.54 + b2 * 0.32 + b3 * 0.14;
    return pow(combined, 1.25); // Spherical puffiness
}

// -------------------------------------------------------------
// Curved Earth Atmosphere Projection
// -------------------------------------------------------------
const float R_PLANET = 90000.0; // 90 km planet radius for smooth horizon curve
const float Y_BOT = 235.0;       // Elevated cloud base altitude (well above mountains)
const float Y_TOP = 395.0;       // 160m thick majestic volumetric troposphere layer

bool solveQuad(float A, float B, float C, out float t1, out float t2) {
    if (A < 0.000001) {
        if (abs(B) < 0.0001) return false;
        t1 = -C / B;
        t2 = t1;
        return true;
    }
    float disc = B * B - 4.0 * A * C;
    if (disc < 0.0) return false;
    float s = sqrt(disc);
    t1 = (-B - s) / (2.0 * A);
    t2 = (-B + s) / (2.0 * A);
    return true;
}

// Intersects view ray with curved planetary cloud slab [Y_BOT, Y_TOP]
bool intersectCurvedSlab(vec3 cam, vec3 R, out float tEnter, out float tExit) {
    float horiz = max(1.0 - R.y * R.y, 0.0);
    float A = horiz / (2.0 * R_PLANET);
    float B = R.y;

    float tBot1, tBot2, tTop1, tTop2;
    bool hitBot = solveQuad(A, B, cam.y - Y_BOT, tBot1, tBot2);
    bool hitTop = solveQuad(A, B, cam.y - Y_TOP, tTop1, tTop2);

    if (!hitTop) return false;

    if (cam.y < Y_BOT) {
        if (!hitBot) return false;
        tEnter = max(tBot1 > 0.0 ? tBot1 : tBot2, 0.0);
        tExit  = max(tTop1 > 0.0 ? tTop1 : tTop2, 0.0);
    } else if (cam.y > Y_TOP) {
        tEnter = max(tTop1 > 0.0 ? tTop1 : tTop2, 0.0);
        tExit  = max(tBot1 > 0.0 ? tBot1 : tBot2, 0.0);
    } else {
        tEnter = 0.0;
        float exit1 = tTop1 > 0.0 ? tTop1 : tTop2;
        float exit2 = hitBot ? (tBot1 > 0.0 ? tBot1 : tBot2) : exit1;
        tExit = max(exit1, exit2);
    }

    if (tExit <= tEnter) return false;
    tEnter = max(tEnter, 0.0);
    tExit = min(tExit, 12500.0); // March up to 12.5 km (780 chunks render distance)
    return true;
}

// -------------------------------------------------------------
// 3D Volumetric Cumulus Cloud Density Field
// -------------------------------------------------------------
float sampleCloudDensity(vec3 p, vec2 wind) {
    float dHoriz = length(p.xz - pc.camPos.xz);
    // Effective curved altitude
    float yCurved = p.y + (dHoriz * dHoriz) / (2.0 * R_PLANET);
    if (yCurved < Y_BOT || yCurved > Y_TOP) return 0.0;

    // Asymmetric height envelope:
    // Soft puffy rounded base rising to towering cauliflower crowns at Y_TOP
    float h = (yCurved - Y_BOT) / (Y_TOP - Y_BOT);
    float baseCutoff = smoothstep(0.0, 0.16, h);
    float topCutoff  = smoothstep(1.0, 0.36, h);
    float heightEnvelope = baseCutoff * topCutoff;

    vec3 ws = p + vec3(wind.x, 0.0, wind.y);

    // 1. Macro cumulus cluster grouping: ~1.4 km wavelengths create distinct, grand cloud islands
    vec3 macroCoord = ws * 0.00072;
    float macroNoise = cloudMacroFBM(macroCoord);

    // Coverage threshold: creates large, clear open blue sky gaps between clouds
    float coverageThreshold = mix(0.48, 0.43, smoothstep(0.1, 0.5, h));
    if (macroNoise < coverageThreshold) return 0.0;

    float clusterMask = smoothstep(coverageThreshold, coverageThreshold + 0.16, macroNoise);

    // 2. Spherical cotton-candy billow shaping: rounded, voluminous lobes
    float billows = cloudBillows(ws);

    // Combined volumetric cotton candy shape
    float cloudShape = clusterMask * (billows * 0.74 + macroNoise * 0.26);

    // Smooth continuous density without any stepping artifacts
    float density = smoothstep(0.22, 0.58, cloudShape) * heightEnvelope * 3.2;
    return density;
}

void main() {
    vec3 cam = pc.camPos.xyz;
    vec3 rayDir = normalize(fragWorldPos - cam);

    // Intersect view ray with the curved atmosphere cloud slab
    float tEnter = 0.0;
    float tExit  = 0.0;
    if (!intersectCurvedSlab(cam, rayDir, tEnter, tExit)) {
        discard;
    }

    if (tExit <= tEnter) {
        discard;
    }

    // Interleaved screen-space dither jitter to eliminate stepping rings
    float dither = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);

    // 32 Raymarching steps inside the cloud volume
    const int STEPS = 32;

    vec3 L = normalize(pc.sunDir.xyz);
    float isDay = clamp(pc.skyFog.b * 1.8 - 0.25, 0.0, 1.0);
    vec2 wind = vec2(pc.camPos.w * 2.0, 0.0);

    // -------------------------------------------------------------
    // Golden-Hour Sun & Sky Lighting Palette matching reference image
    // -------------------------------------------------------------
    float goldenHour = smoothstep(0.55, 0.08, L.y) * step(0.0, L.y);
    vec3 midDaySun = vec3(1.22, 1.16, 1.05);
    vec3 goldenSun = vec3(1.95, 1.35, 0.72);
    vec3 activeSun = mix(midDaySun, goldenSun, goldenHour);

    // Ambient lighting: Pastel sky dome + warm ground bounce for cotton candy softness
    vec3 skyZenithAmbient = mix(vec3(0.04, 0.06, 0.12), mix(vec3(0.52, 0.62, 0.82), vec3(0.72, 0.62, 0.58), goldenHour), isDay);
    vec3 groundAmbient    = mix(vec3(0.02, 0.03, 0.05), mix(vec3(0.40, 0.44, 0.40), vec3(0.60, 0.48, 0.36), goldenHour), isDay);

    // Dual-lobe Henyey-Greenstein phase function (silver lining + golden bloom)
    float cosTheta = dot(rayDir, L);
    float g1 = 0.78; // Strong forward scattering peak
    float g2 = -0.22; // Gentle backscatter
    float hg1 = (1.0 - g1 * g1) / pow(max(1.0 + g1 * g1 - 2.0 * g1 * cosTheta, 0.001), 1.5);
    float hg2 = (1.0 - g2 * g2) / pow(max(1.0 + g2 * g2 - 2.0 * g2 * cosTheta, 0.001), 1.5);
    float silverLining = mix(hg1, hg2, 0.18) * 0.38 + 0.68;
    // Radiant forward sun glow around cloud edges when looking towards the sun
    float forwardGlow = pow(max(cosTheta * 0.5 + 0.5, 0.0), 3.2) * 1.6;
    vec3 cottonCandySun = mix(activeSun, vec3(2.05, 1.40, 0.95), goldenHour * 0.45);
    vec3 sunScatterColor = cottonCandySun * (silverLining + forwardGlow);

    // Limit maximum raymarch distance inside cloud slab to prevent performance drops at horizon
    float slabDist = min(tExit - tEnter, 4800.0);
    float stepSize = slabDist / float(STEPS);
    float tCurrent = tEnter + dither * stepSize;

    vec3 accumColor = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < STEPS; ++i) {
        if (tCurrent >= tExit || transmittance < 0.02) break;

        vec3 p = cam + rayDir * (tCurrent + stepSize * 0.5);
        tCurrent += stepSize;

        float density = sampleCloudDensity(p, wind);
        if (density > 0.002) {
            // Secondary light sample towards sun for volumetric self-shadowing
            vec3 pLight = p + L * 32.0;
            float lightDensity = sampleCloudDensity(pLight, wind);

            // Dual-octave multiple-scattering: light penetrates deep into the cloud without black soot
            float directLight = mix(exp(-lightDensity * 1.2), exp(-lightDensity * 0.24) * 0.72, 0.52);
            // Powder effect: darkens deep interior crevices while keeping outer billows radiant
            float powder = 1.0 - exp(-density * 2.8);
            directLight *= powder;

            float dHoriz = length(p.xz - cam.xz);
            float yCurved = p.y + (dHoriz * dHoriz) / (2.0 * R_PLANET);
            float h = clamp((yCurved - Y_BOT) / (Y_TOP - Y_BOT), 0.0, 1.0);
            vec3 ambient = mix(groundAmbient, skyZenithAmbient, h);

            // Multi-scattering inscattered radiance
            vec3 inscatter = ambient * 0.88 + sunScatterColor * (directLight * isDay);

            // Beer-Lambert extinction along view ray
            float stepExtinction = exp(-density * 0.20 * stepSize);
            accumColor += transmittance * inscatter * (1.0 - stepExtinction);
            transmittance *= stepExtinction;
        }
    }

    // Crepuscular sun shafts radiating through cloud edges towards observer
    float sunProximity = pow(max(cosTheta * 0.5 + 0.5, 0.0), 4.2);
    vec3 crepuscularShafts = activeSun * (sunProximity * 1.5) * (0.35 + goldenHour * 0.65) * isDay;
    accumColor += crepuscularShafts * (1.0 - transmittance);

    float cloudAlpha = 1.0 - transmittance;
    if (cloudAlpha < 0.008) {
        discard;
    }

    // Atmospheric horizon distance blend:
    // Seamlessly fades clouds into the distant sky dome between 3,500m and 12,000m (750 chunks)
    // Completely eliminates any sudden cloud cutoff or hard boundary
    float dist = tEnter;
    float fogStart = 3500.0;
    float fogEnd   = 12000.0;
    float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    fogFactor = fogFactor * fogFactor;

    vec3 goldenFog = mix(pc.skyFog.rgb, vec3(1.50, 1.10, 0.60), goldenHour * 0.40);
    vec3 finalColor = mix(accumColor, goldenFog, fogFactor);
    float finalAlpha = cloudAlpha * (1.0 - fogFactor);

    finalColor = acesFilmicTonemap(finalColor);

    outColor = vec4(finalColor, finalAlpha);
}
