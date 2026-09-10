#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec3 fragWorldPos;

// Baked 4-channel seeded periodic cloud noise texture
// R = Base Macro Worley, G = Mid Worley, B = High-Freq Perlin, A = Micro Wisps
layout(binding = 0) uniform sampler2D cloudTexture;

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

// =============================================================
// TUNABLE VOLUMETRIC CLOUD PARAMETERS
// Adjust these parameters to tune density, scale, and lighting
// =============================================================

// 1. Coverage & Scale (Multi-Scale Puffy Cumulus Formations)
const float CLOUD_SCALE_UV       = 0.00025;  // Slightly larger physical scale (~4.0 km period)
const float CLOUD_COVERAGE_BASE  = 0.44;     // Reduced spread between clouds (closer spacing, natural negative space)
const float CLOUD_COVERAGE_TOP   = 0.68;     // Upward coverage ramp sculpting tall rounded dome tops
const float CLOUD_REMAP_WIDTH    = 0.120;    // Soft density remap width
const float CLOUD_DENSITY_MULT   = 1.80;     // Balanced core density

// 2. Edge Erosion & Wispy Shredding
const float EROSION_STRENGTH     = 0.40;     // Detail noise erosion strength
const float EROSION_BOUNDARY_EXP = 3.20;     // Boundary confinement curve
const float DETAIL_WARP_STRENGTH = 0.00008;  // Micro curl silhouette warping

// 3. Volumetric Lighting & Multiple Scattering Fill
const float BEER_OPTICAL_DENSITY = 1.80;     // Self-shadow optical depth (macro contrast)
const float POWDER_STRENGTH      = 2.20;     // Powder edge brightening
const float AMBIENT_SHADOW_MULT  = 0.85;     // Strong sky dome ambient fill (raised from 0.72)
const float AMBIENT_FLOOR        = 0.28;     // Explicit scattering floor - shadows NEVER crush to black
const float MULTISCATTER_WEIGHT  = 0.50;     // Dual-octave Beer multiple scattering fill (50% low-extinction core glow)

// 4. Adaptive Raymarching Integration
const float STEP_COARSE          = 56.0;     // Empty air space-skipping step in meters
const float STEP_FINE            = 14.0;     // Dense intra-cloud volumetric step in meters (eliminates slicing)
const int MAX_STEPS              = 64;       // Maximum adaptive raymarch steps
const float EXTINCTION_MULT      = 0.035;    // View-ray extinction factor (multi-step depth)

// =============================================================
// Atmospheric Slab Dimensions (120 km Earth curvature)
// =============================================================
const float R_PLANET = 120000.0; // 120 km planetary radius
const float Y_BOT = 196.0;       // Cloud base altitude: reduced to 196m as requested!
const float Y_TOP = 320.0;       // Cloud top altitude (124m tall vertical layer for lofty puffy domes)

// -------------------------------------------------------------
// Interleaved Gradient Noise (Jorge Jimenez)
// Optimal spatial distribution for raymarch dither; converts
// coherent banding stripes into imperceptible high-frequency dither
// -------------------------------------------------------------
float interleavedGradientNoise(vec2 screenPos) {
    return fract(52.9829189 * fract(0.06711056 * screenPos.x + 0.00583715 * screenPos.y));
}

// -------------------------------------------------------------
// ACES Filmic Tonemapper matching scene lighting
// -------------------------------------------------------------
vec3 acesFilmicTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

bool solveQuad(float A, float B, float C, out float t1, out float t2) {
    if (abs(A) < 0.000001) {
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
    tExit = min(tExit, 12500.0); // Reach out to 12.5 km (780 chunks horizon)
    return true;
}

// -------------------------------------------------------------
// Realistic Volumetric Cloud Density Field
// -------------------------------------------------------------
float sampleCloudDensity(vec3 p, vec2 wind) {
    float dHoriz = length(p.xz - pc.camPos.xz);
    float yCurved = p.y + (dHoriz * dHoriz) / (2.0 * R_PLANET);
    if (yCurved < Y_BOT || yCurved > Y_TOP) return 0.0;

    // 1. Height-based density gradient:
    // Flat defined base (tapers quickly at h = 0.0 - 0.16), tapering smoothly to a compact dome top (h = 0.50 - 1.0)
    float h = clamp((yCurved - Y_BOT) / (Y_TOP - Y_BOT), 0.0, 1.0);
    float baseTaper = smoothstep(0.0, 0.16, h);
    float topTaper  = smoothstep(1.0, 0.50, h);
    float heightGradient = baseTaper * topTaper;
    if (heightGradient <= 0.001) return 0.0;

    // World-space UVs with animated wind drift
    vec2 wsUV = (p.xz + wind) * CLOUD_SCALE_UV;

    // Cauliflower billow vertical expansion with height
    vec2 heightShear = vec2((h - 0.4) * 0.00016, (h - 0.4) * 0.00008);
    vec2 uv = wsUV + heightShear;

    // 2. Sample baked 4-channel seeded periodic noise texture
    // Explicit textureLod(..., 0.0) bypasses quad derivatives, eliminating speckling in dynamic loops
    vec4 tBase = textureLod(cloudTexture, uv, 0.0);
    vec4 tMid  = textureLod(cloudTexture, uv * 2.3 + vec2(0.18, 0.42), 0.0);
    vec4 tSmall = textureLod(cloudTexture, uv * 5.2 + vec2(0.51, 0.79), 0.0);

    // Multi-scale cloud sizing: large towering domes, medium companion billows, and small fluffy puffs
    float macroPuff = tBase.r * 0.65 + tBase.g * 0.35;
    float midPuff   = tMid.r * 0.55 + tMid.g * 0.45;
    float smallPuff = tSmall.g * 0.60 + tSmall.b * 0.40;

    // Blended multi-scale profile: gives variety of sizes across the sky and fills in empty spread
    float baseMacro = macroPuff * 0.60 + midPuff * 0.28 + smallPuff * (0.12 * (1.0 - h * 0.5));

    // Height-dependent coverage: contracts perimeter inward with altitude to sculpt compact puffy domes
    float coverage = mix(CLOUD_COVERAGE_BASE, CLOUD_COVERAGE_TOP, smoothstep(0.08, 0.70, h));
    float denseRaw = baseMacro - coverage;

    // Negative space check: clear sky gaps
    if (denseRaw < -0.12) return 0.0;

    // 3. Edge erosion near density falloff boundary
    float boundary = clamp(1.0 - denseRaw * EROSION_BOUNDARY_EXP, 0.0, 1.0);

    // Detail noise sample (Channel B = Perlin FBM, Channel A = Micro curl wisps)
    vec2 detailUV = uv * 3.6 + vec2(0.23, 0.47) + (tBase.ba - vec2(0.5)) * (boundary * DETAIL_WARP_STRENGTH);
    vec4 tDetail = textureLod(cloudTexture, detailUV, 0.0);

    float erosionNoise = tDetail.b * 0.70 + tDetail.a * 0.30;
    float erodedShape = baseMacro - erosionNoise * boundary * EROSION_STRENGTH;

    // Soft density remap: wide transition band (0.120) produces continuous volumetric gradient
    float density = smoothstep(coverage, coverage + CLOUD_REMAP_WIDTH, erodedShape) * heightGradient * CLOUD_DENSITY_MULT;
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

    vec3 L = normalize(pc.sunDir.xyz);
    float isDay = clamp(pc.skyFog.b * 1.8 - 0.25, 0.0, 1.0);
    vec2 wind = vec2(pc.camPos.w * 1.6, 0.0);

    // Interleaved Gradient Noise per-pixel dither offset [0, 1)
    float dither = interleavedGradientNoise(gl_FragCoord.xy);

    // -------------------------------------------------------------
    // Dynamic Time-of-Day Sun & Sky Color Palette
    // -------------------------------------------------------------
    float sunElev = L.y;
    float goldenHour = smoothstep(0.48, 0.05, sunElev) * step(0.0, sunElev);

    // Warm Solar Spectrum (Direct Sunlight)
    // Midday: warm solar white (~5500K, not sterile flat RGB white)
    vec3 midDaySunColor = vec3(1.45, 1.38, 1.20);
    // Sunset / Golden Hour: rich fiery amber-gold
    vec3 sunsetSunColor = vec3(2.65, 1.45, 0.55);
    // Moon / Night: cool soft lunar silver
    vec3 nightSunColor  = vec3(0.35, 0.42, 0.65);
    vec3 sunColor = mix(midDaySunColor, sunsetSunColor, goldenHour);
    sunColor = mix(nightSunColor, sunColor, isDay);

    // Dual-lobe Henyey-Greenstein phase function (silver lining + forward bloom)
    float cosTheta = dot(rayDir, L);
    float g1 = 0.76;
    float g2 = -0.20;
    float hg1 = (1.0 - g1 * g1) / pow(max(1.0 + g1 * g1 - 2.0 * g1 * cosTheta, 0.001), 1.5);
    float hg2 = (1.0 - g2 * g2) / pow(max(1.0 + g2 * g2 - 2.0 * g2 * cosTheta, 0.001), 1.5);
    float silverLining = mix(hg1, hg2, 0.20) * 0.40 + 0.65;
    float forwardGlow = pow(max(cosTheta * 0.5 + 0.5, 0.0), 3.8) * 1.8;
    vec3 litSurfaceColor = sunColor * (silverLining + forwardGlow);

    // -------------------------------------------------------------
    // Dynamic Ambient Sky Tint (Shadowed Regions)
    // Directly samples and derives from pc.skyFog.rgb (actual dynamic sky color)
    // -------------------------------------------------------------
    // Midday zenith: cool, saturated periwinkle / atmospheric blue
    vec3 skyZenithBlue = pc.skyFog.rgb * vec3(0.68, 0.82, 1.28);
    // Sunset zenith: atmospheric mauve / lavender / violet opposite the sun
    vec3 sunsetZenithPurple = vec3(0.46, 0.34, 0.56);
    vec3 activeZenithSky = mix(skyZenithBlue, sunsetZenithPurple, goldenHour);

    // Ground bounce & horizon warmth (bottom of clouds)
    vec3 groundBounceMidday = vec3(0.30, 0.32, 0.30);
    vec3 groundBounceSunset = vec3(0.52, 0.34, 0.20);
    vec3 activeGroundBounce = mix(groundBounceMidday, groundBounceSunset, goldenHour);

    // Limit maximum raymarch distance inside cloud slab
    float maxDist = min(tExit, tEnter + 3800.0);
    float stepSize = STEP_COARSE;
    float tCurrent = tEnter + dither * STEP_COARSE;

    vec3 accumColor = vec3(0.0);
    float transmittance = 1.0;
    int zeroDensityStreak = 0;

    for (int i = 0; i < MAX_STEPS; ++i) {
        if (tCurrent >= maxDist || transmittance < 0.02) break;

        vec3 p = cam + rayDir * tCurrent;
        float density = sampleCloudDensity(p, wind);

        if (density > 0.003) {
            // If we were marching coarsely, back up half a coarse step and switch to fine 14m steps
            if (stepSize > STEP_FINE * 1.5) {
                tCurrent -= STEP_COARSE * 0.5;
                stepSize = STEP_FINE;
                tCurrent += stepSize;
                zeroDensityStreak = 0;
                continue;
            }

            zeroDensityStreak = 0;

            // Dual stratified sun rays (+16m and +48m) along light direction L
            float dLightNear = sampleCloudDensity(p + L * 16.0, wind);
            float dLightFar  = sampleCloudDensity(p + L * 48.0, wind);
            float totalSunOpticalDepth = dLightNear * 0.65 + dLightFar * 0.35;

            // Dual-octave Beer's law multiple-scattering approximation
            float beer1 = exp(-totalSunOpticalDepth * BEER_OPTICAL_DENSITY * 2.2);
            float beer2 = exp(-totalSunOpticalDepth * BEER_OPTICAL_DENSITY * 0.45);
            float beerMultiscatter = mix(beer1, beer2, MULTISCATTER_WEIGHT);

            // Powder effect: edge brightening for forward-facing thin wisps
            float powderTerm = 1.0 - exp(-max(density, 0.001) * POWDER_STRENGTH);
            float directSunTransmittance = clamp(beerMultiscatter * powderTerm * 2.2, 0.0, 1.0);

            // Dynamic Hemispherical Ambient Sky Fill
            float dHoriz = length(p.xz - cam.xz);
            float yCurved = p.y + (dHoriz * dHoriz) / (2.0 * R_PLANET);
            float h = clamp((yCurved - Y_BOT) / (Y_TOP - Y_BOT), 0.0, 1.0);

            vec3 ambientShade = mix(activeGroundBounce, activeZenithSky, smoothstep(0.05, 0.65, h));

            // Hard Ambient Floor: GUARANTEES shadows never crush to black!
            vec3 ambientFloorColor = max(activeZenithSky * 0.65, vec3(0.28, 0.32, 0.42) * isDay);
            ambientShade = max(ambientShade * AMBIENT_SHADOW_MULT, ambientFloorColor);

            vec3 inscatter = mix(ambientShade, litSurfaceColor, directSunTransmittance * isDay);

            // Soft Beer-Lambert extinction along view ray (stepSize is 14m here!)
            float stepAlpha = 1.0 - exp(-density * EXTINCTION_MULT * stepSize);
            accumColor += transmittance * inscatter * stepAlpha;
            transmittance *= (1.0 - stepAlpha);
        } else {
            zeroDensityStreak++;
            // If we have stepped in empty air for 2 consecutive steps, resume coarse space-skipping
            if (zeroDensityStreak >= 2) {
                stepSize = STEP_COARSE;
            }
        }

        tCurrent += stepSize;
    }

    // Crepuscular forward sun glow
    float sunProximity = pow(max(cosTheta * 0.5 + 0.5, 0.0), 4.2);
    vec3 crepuscularShafts = sunColor * (sunProximity * 1.5) * (0.35 + goldenHour * 0.65) * isDay;
    accumColor += crepuscularShafts * (1.0 - transmittance);

    float cloudAlpha = 1.0 - transmittance;
    if (cloudAlpha < 0.008) {
        discard;
    }

    // Atmospheric horizon distance blend:
    // Seamlessly fades clouds into the distant sky dome between 3,500m and 12,000m (750 chunks)
    float dist = tEnter;
    float fogStart = 3500.0;
    float fogEnd   = 12000.0;
    float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    fogFactor = fogFactor * fogFactor;

    // Un-premultiply accumulated color so hardware SRC_ALPHA blend doesn't square alpha and crush shadows to black
    vec3 cloudRgb = accumColor / max(cloudAlpha, 0.0001);

    vec3 goldenFog = mix(pc.skyFog.rgb, vec3(1.50, 1.10, 0.60), goldenHour * 0.40);
    vec3 finalColor = mix(cloudRgb, goldenFog, fogFactor);
    float finalAlpha = cloudAlpha * (1.0 - fogFactor);

    finalColor = acesFilmicTonemap(finalColor);

    outColor = vec4(finalColor, finalAlpha);
}