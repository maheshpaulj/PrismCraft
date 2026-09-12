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
    vec4 shaderOptions;
    vec4 dayInfo;
    vec4 pointLight3;
    vec4 pointLight4;
} pc;

layout(location = 0) out vec4 outColor;

// =============================================================
// TUNABLE VOLUMETRIC CLOUD PARAMETERS (Live Hot-Reload Active)
// Adjust these parameters to tune density, scale, and lighting
// =============================================================

// 1. Coverage & Scale (Multi-Scale Puffy Cumulus Formations)
// CLOUD_SCALE_UV: Controls physical cloud size. ~5.5km atmospheric formations
const float CLOUD_SCALE_UV       = 0.00018;

// CLOUD_COVERAGE_BASE: Base threshold for negative space.
const float CLOUD_COVERAGE_BASE  = 0.30;
const float CLOUD_COVERAGE_TOP   = 0.50;     // Upward coverage ramp sculpting tall rounded dome tops
const float CLOUD_REMAP_WIDTH    = 0.320;    // Wide continuous cubic Hermite density remap eliminates sharp edges
const float CLOUD_DENSITY_MULT   = 1.50;     // Balanced, silky core density

// 2. Volumetric Lighting & Multiple Scattering Fill
const float BEER_OPTICAL_DENSITY = 1.60;     // Self-shadow optical depth
const float POWDER_STRENGTH      = 2.80;     // Powder edge brightening
const float AMBIENT_SHADOW_MULT  = 0.90;     // Atmospheric sky dome ambient fill
const float AMBIENT_FLOOR        = 0.30;     // Explicit scattering floor - shadows NEVER crush to black
const float MULTISCATTER_WEIGHT  = 0.45;     // Dual-octave Beer multiple scattering fill

// 3. Adaptive Uniform Raymarching Integration
const float STEP_COARSE          = 36.0;     // Smooth empty-air skipping step in meters
const float STEP_FINE            = 20.0;     // Dense intra-cloud volumetric step in meters
const int MAX_STEPS              = 36;       // Maximum raymarch steps (fast 100+ FPS)
const float EXTINCTION_MULT      = 0.032;    // View-ray extinction factor

// =============================================================
// Atmospheric Slab Dimensions (120 km Earth curvature)
// =============================================================
const float R_PLANET = 120000.0; // 120 km planetary radius
const float Y_BOT = 196.0;       // Cloud base altitude: 196m
const float Y_TOP = 340.0;       // Cloud top altitude: 340m (144m vertical thickness for lofty puffy domes)

// -------------------------------------------------------------
// Interleaved Gradient Noise (Jorge Jimenez)
// Optimal spatial distribution for raymarch dither; converts
// coherent banding stripes into imperceptible high-frequency dither
// -------------------------------------------------------------
float interleavedGradientNoise(vec2 screenPos) {
    return fract(52.9829189 * fract(0.06711056 * screenPos.x + 0.00583715 * screenPos.y));
}

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

// ACES Hill / Narkowicz Fitted tonemapper matching cell.frag
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

    // 1. Vertical density profile: flat defined base, tapering to lofty rounded dome tops
    float h = clamp((yCurved - Y_BOT) / (Y_TOP - Y_BOT), 0.0, 1.0);
    float baseTaper = smoothstep(0.0, 0.12, h);
    float topTaper  = smoothstep(1.0, 0.38, h);
    float heightGradient = baseTaper * topTaper;
    if (heightGradient <= 0.001) return 0.0;

    // 2. World-space UVs with gentle wind drift & vertical billow expansion
    vec2 wsUV = (p.xz + wind) * CLOUD_SCALE_UV;
    vec2 heightShear = vec2((h - 0.35) * 0.00010, (h - 0.35) * 0.00005);
    vec2 uv = wsUV + heightShear;

    // 3. Multi-octave smooth cloud texture sampling
    vec4 tBase = textureLod(cloudTexture, uv, 0.0);
    vec4 tMid  = textureLod(cloudTexture, uv * 2.2 + vec2(0.24, 0.58), 0.0);
    vec4 tSmall = textureLod(cloudTexture, uv * 4.6 + vec2(0.61, 0.37), 0.0);

    // Multi-scale cloud profiles: broad towering domes, companion billows, and soft atmospheric wisps
    float macroPuff = tBase.r * 0.60 + tBase.g * 0.40;
    float midPuff   = tMid.r * 0.50 + tMid.g * 0.50;
    float smallPuff = tSmall.b * 0.65 + tSmall.a * 0.35;

    float baseMacro = macroPuff * 0.58 + midPuff * 0.28 + smallPuff * (0.14 * (1.0 - h * 0.4));

    // Height-dependent coverage: sculpts lofty rounded cumulus dome tops
    float coverage = mix(CLOUD_COVERAGE_BASE, CLOUD_COVERAGE_TOP, smoothstep(0.10, 0.80, h));
    float denseRaw = baseMacro - coverage;

    if (denseRaw < -0.15) return 0.0;

    // Continuous cubic Hermite density remapping - produces silky, billowy, zero-stipple volume
    float density = smoothstep(-0.06, CLOUD_REMAP_WIDTH, denseRaw) * heightGradient * CLOUD_DENSITY_MULT;
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
    float isDay = pc.dayInfo.x;
    float actualSunY = pc.dayInfo.y;
    bool vibrant = (pc.sunDir.w > 0.0);
    vec2 wind = vec2(pc.camPos.w * 4.2, pc.camPos.w * 1.6);

    // -------------------------------------------------------------
    // 2D Flat Triangular Clouds (PrismCraft Non-Vibrant / Classic Mode)
    // -------------------------------------------------------------
    if (!vibrant) {
        if (rayDir.y <= 0.015) discard;
        float cloudH = 175.0;
        float t = (cloudH - cam.y) / rayDir.y;
        if (t <= 0.0 || t > 8500.0) discard;

        vec3 hitP = cam + rayDir * t;
        vec2 uv = (hitP.xz + wind * 3.0) * 0.005;

        // Equilateral Triangular Lattice
        mat2 toTri = mat2(1.0, 0.0, -0.57735027, 1.15470054);
        vec2 triCoord = toTri * uv;
        vec2 triId = floor(triCoord);
        vec2 triFrac = fract(triCoord);
        bool isUpper = (triFrac.x + triFrac.y > 1.0);
        vec2 triCenter = triId + (isUpper ? vec2(0.6667, 0.6667) : vec2(0.3333, 0.3333));

        vec2 noiseUV = triCenter * 0.05;
        float cNoise = texture(cloudTexture, noiseUV).r;
        if (cNoise < 0.44) discard;

        float edgeDist = min(min(triFrac.x, triFrac.y), abs(1.0 - (triFrac.x + triFrac.y)));
        float edgeOutline = smoothstep(0.015, 0.07, edgeDist);

        float goldenHour = smoothstep(0.40, 0.02, actualSunY) * step(-0.05, actualSunY);
        vec3 cloudColor = mix(vec3(0.32, 0.38, 0.52), mix(vec3(0.98, 0.98, 0.98), vec3(1.15, 0.82, 0.52), goldenHour), isDay);
        cloudColor *= (0.88 + 0.12 * edgeOutline);

        float horizDist = length(hitP.xz - cam.xz);
        float fade = 1.0 - smoothstep(2200.0, 7500.0, horizDist);
        float alpha = clamp((cNoise - 0.44) * 3.2, 0.0, 0.88) * fade;
        if (alpha < 0.02) discard;

        outColor = vec4(cloudColor, alpha);
        return;
    }

    // Interleaved Gradient Noise per-pixel dither offset [0, 1)
    float dither = interleavedGradientNoise(gl_FragCoord.xy);

    // -------------------------------------------------------------
    // Dynamic Time-of-Day Sun & Sky Color Palette
    // -------------------------------------------------------------
    float goldenHour = smoothstep(0.40, 0.02, actualSunY) * step(-0.05, actualSunY);

    // Warm Solar Spectrum (Direct Sunlight)
    // Midday: warm solar white (~5500K, not sterile flat RGB white)
    vec3 midDaySunColor = vec3(1.50, 1.42, 1.15);
    // Sunset / Golden Hour: rich fiery amber-gold
    vec3 sunsetSunColor = vec3(2.85, 1.55, 0.45);
    // Moon / Night: cool soft lunar silver
    vec3 nightSunColor  = vec3(0.35, 0.42, 0.65);
    vec3 sunColor = mix(midDaySunColor, sunsetSunColor, goldenHour);
    sunColor = mix(nightSunColor, sunColor, isDay);

    // Dual-lobe Henyey-Greenstein phase function (silver lining + forward bloom)
    float cosTheta = dot(rayDir, L);
    float g1 = 0.78;
    float g2 = -0.22;
    float hg1 = (1.0 - g1 * g1) / pow(max(1.0 + g1 * g1 - 2.0 * g1 * cosTheta, 0.001), 1.5);
    float hg2 = (1.0 - g2 * g2) / pow(max(1.0 + g2 * g2 - 2.0 * g2 * cosTheta, 0.001), 1.5);
    float silverLining = mix(hg1, hg2, 0.20) * 0.48 + 0.65;
    float forwardGlow = pow(max(cosTheta * 0.5 + 0.5, 0.0), 3.5) * 2.2;
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
    vec3 groundBounceMidday = vec3(0.32, 0.34, 0.30);
    vec3 groundBounceSunset = vec3(0.60, 0.38, 0.18);
    vec3 activeGroundBounce = mix(groundBounceMidday, groundBounceSunset, goldenHour);

    // Limit maximum raymarch distance inside cloud slab
    float maxDist = min(tExit, tEnter + 3800.0);
    float stepSize = STEP_COARSE;
    float tCurrent = tEnter + dither * 2.5;

    vec3 accumColor = vec3(0.0);
    float transmittance = 1.0;
    int zeroDensityStreak = 0;

    for (int i = 0; i < MAX_STEPS; ++i) {
        if (tCurrent >= maxDist || transmittance < 0.02) break;

        vec3 p = cam + rayDir * tCurrent;
        float density = sampleCloudDensity(p, wind);

        if (density > 0.002) {
            // Switch to fine steps inside cloud volume
            if (stepSize > STEP_FINE * 1.2) {
                tCurrent -= (stepSize - STEP_FINE);
                stepSize = STEP_FINE;
                zeroDensityStreak = 0;
                continue;
            }

            zeroDensityStreak = 0;

            // Direct sun ray sample along light direction L
            float dLight = sampleCloudDensity(p + L * 28.0, wind);
            float totalSunOpticalDepth = dLight * BEER_OPTICAL_DENSITY;

            // Dual-octave Beer's law multiple-scattering approximation
            float beer1 = exp(-totalSunOpticalDepth * 2.2);
            float beer2 = exp(-totalSunOpticalDepth * 0.40);
            float beerMultiscatter = mix(beer1, beer2, MULTISCATTER_WEIGHT);

            // Powder effect: edge brightening for forward-facing thin wisps
            float powderTerm = 1.0 - exp(-max(density, 0.001) * POWDER_STRENGTH);
            float directSunTransmittance = clamp(beerMultiscatter * (powderTerm * 1.6 + 0.35), 0.0, 1.0);

            // Dynamic Hemispherical Ambient Sky Fill
            float dHoriz = length(p.xz - cam.xz);
            float yCurved = p.y + (dHoriz * dHoriz) / (2.0 * R_PLANET);
            float h = clamp((yCurved - Y_BOT) / (Y_TOP - Y_BOT), 0.0, 1.0);

            vec3 ambientShade = mix(activeGroundBounce, activeZenithSky, smoothstep(0.05, 0.70, h));

            // Hard Ambient Floor: GUARANTEES shadows never crush to black!
            vec3 ambientFloorColor = max(activeZenithSky * 0.55, vec3(0.24, 0.28, 0.38) * isDay);
            ambientShade = max(ambientShade * AMBIENT_SHADOW_MULT, ambientFloorColor);

            vec3 inscatter = mix(ambientShade, litSurfaceColor, directSunTransmittance * isDay);

            // Soft Beer-Lambert extinction along view ray
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
    float sunProximity = pow(max(cosTheta * 0.5 + 0.5, 0.0), 3.6);
    vec3 crepuscularShafts = sunColor * (sunProximity * 1.8) * (0.35 + goldenHour * 0.65) * isDay;
    accumColor += crepuscularShafts * (1.0 - transmittance);

    float cloudAlpha = 1.0 - transmittance;

    // Atmospheric horizon distance blend:
    // Seamlessly fades clouds into the distant sky dome between 2,400m and 8,500m
    float dist = tEnter;
    float fogStart = 2400.0;
    float fogEnd   = 8500.0;
    float fogFactor = smoothstep(fogStart, fogEnd, dist);

    // Un-premultiply accumulated color so hardware SRC_ALPHA blend doesn't square alpha and crush shadows to black
    vec3 cloudRgb = accumColor / max(cloudAlpha, 0.0001);

    vec3 linearSkyFog = srgbToLinear(pc.skyFog.rgb);
    vec3 goldenFog = mix(linearSkyFog, vec3(1.60, 1.15, 0.55), goldenHour * 0.55);
    vec3 finalColor = mix(cloudRgb, goldenFog, fogFactor);
    float finalAlpha = cloudAlpha * (1.0 - fogFactor);

    if (finalAlpha < 0.002) {
        discard;
    }

    // Output linear HDR radiance directly to HDR buffer (master tonemapping handled in post-processing pass)
    outColor = vec4(finalColor, finalAlpha);
}