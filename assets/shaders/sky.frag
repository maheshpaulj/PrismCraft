#version 450

layout(location = 0) in vec3 fragViewDir;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 sunDir;     // xyz = normalized sun dir, w = sun intensity
    vec4 skyFog;     // xyz = fog color, w = fog distance
    vec4 camPos;     // xyz = camera pos, w = time
    vec4 lightColor;
    vec4 pointLight1;
    vec4 pointLight2;
    vec4 heldTorch;
    vec4 shaderOptions;
    vec4 dayInfo;    // x = dayFactor, y = sunHeight, z = exposure, w = fogDensity
    vec4 pointLight3;
    vec4 pointLight4;
} pc;

layout(location = 0) out vec4 outColor;

// High-frequency screen-space dither noise (Jorge Jimenez / IGN)
// Eliminates 8-bit Mach banding and contour steps on smooth gradients
float interleavedGradientNoise(vec2 screenPos) {
    return fract(52.9829189 * fract(0.06711056 * screenPos.x + 0.00583715 * screenPos.y));
}

// Accurate sRGB to Linear helper
vec3 srgbToLinear(vec3 c) {
    return pow(clamp(c, 0.0, 1.0), vec3(2.2));
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

// Procedural pseudo-random hash for night stellar field
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec3 V = normalize(fragViewDir);
    vec3 L = normalize(pc.sunDir.xyz);

    float isDay = pc.dayInfo.x;
    float sunHeight = pc.dayInfo.y;
    float sunIntensity = abs(pc.sunDir.w);

    float goldenHour = smoothstep(0.40, 0.02, sunHeight) * step(-0.06, sunHeight);

    // Underwater Sky Background: Eliminate bright daytime sky dome when camera is submerged
    bool cameraUnderwater = (pc.skyFog.w < 0.0);
    if (cameraUnderwater) {
        vec3 linearOcean = srgbToLinear(pc.skyFog.rgb);
        float cosTheta = dot(V, L);
        float forwardScatter = pow(max(cosTheta, 0.0), 3.5) * isDay * smoothstep(-0.1, 0.5, V.y);
        vec3 oceanSky = linearOcean + vec3(0.04, 0.16, 0.22) * forwardScatter;
        outColor = vec4(oceanSky, 1.0);
        return;
    }

    // -------------------------------------------------------------
    // 1. Physically-Calibrated Atmospheric Sky Palettes (Linear Space)
    // -------------------------------------------------------------
    // Midday: Rich deep atmospheric azure at zenith -> soft luminous sky blue at horizon
    vec3 zenithDay   = vec3(0.14, 0.36, 0.85);
    vec3 horizonDay  = vec3(0.60, 0.74, 0.90);
    vec3 groundDay   = vec3(0.24, 0.28, 0.22);

    // Sunset / Golden Hour: Deep twilight indigo at zenith -> intense fiery golden-amber at horizon
    vec3 zenithDusk  = vec3(0.12, 0.14, 0.36);
    vec3 horizonDusk = vec3(1.12, 0.58, 0.16);
    vec3 groundDusk  = vec3(0.20, 0.14, 0.10);

    // Night: Deep celestial navy at zenith -> subtle ambient indigo above horizon
    vec3 zenithNight = vec3(0.007, 0.012, 0.025);
    vec3 horizonNight= vec3(0.018, 0.025, 0.048);
    vec3 groundNight = vec3(0.008, 0.012, 0.016);

    // Interpolate palettes across the celestial time cycle
    vec3 dayZenith   = mix(zenithDay, zenithDusk, goldenHour);
    vec3 dayHorizon  = mix(horizonDay, horizonDusk, goldenHour);
    vec3 dayGround   = mix(groundDay, groundDusk, goldenHour);

    vec3 activeZenith  = mix(zenithNight, dayZenith, isDay);
    vec3 activeHorizon = mix(horizonNight, dayHorizon, isDay);
    vec3 activeGround  = mix(groundNight, dayGround, isDay);

    // -------------------------------------------------------------
    // 2. Continuous Rayleigh Zenith-to-Horizon Gradient
    // -------------------------------------------------------------
    vec3 skyRgb;
    if (V.y >= 0.0) {
        // Upper hemisphere: exponential optical thickness ramp towards horizon
        float elevation = clamp(V.y, 0.0, 1.0);
        float rayleighFactor = exp(-elevation * 2.8);
        skyRgb = mix(activeZenith, activeHorizon, rayleighFactor);
    } else {
        // Lower hemisphere (below horizon): smooth cubic hermite blend into ground ambient
        float nadirFactor = smoothstep(0.0, -0.28, V.y);
        skyRgb = mix(activeHorizon, activeGround, nadirFactor);
    }

    // -------------------------------------------------------------
    // 3. Subtle Atmospheric Horizon Haze Layer
    // Ground-level boundary aerosol layer (Mie scattering peak near Vy = 0)
    // Seamlessly grounds the distant chunk fog with the horizon sky
    // -------------------------------------------------------------
    float hazeDist = abs(V.y - 0.015);
    float hazeBand = exp(-hazeDist * 14.0);
    vec3 hazeDay = mix(vec3(0.70, 0.80, 0.90), vec3(1.10, 0.68, 0.32), goldenHour);
    vec3 hazeColor = mix(vec3(0.014, 0.019, 0.032), hazeDay, isDay);
    skyRgb = mix(skyRgb, hazeColor, hazeBand * 0.52);

    // -------------------------------------------------------------
    // 4. Solar Influence (Forward Mie Scattering & Halo)
    // -------------------------------------------------------------
    float cosTheta = dot(V, L);

    // A. Broad atmospheric solar warmth across the sun-facing hemisphere
    float sunWarmth = pow(clamp(cosTheta * 0.5 + 0.5, 0.0, 1.0), 2.5) * 0.55;
    vec3 warmthColor = mix(vec3(0.35, 0.30, 0.18), vec3(1.10, 0.60, 0.18), goldenHour);
    skyRgb += warmthColor * (sunWarmth * isDay);

    // B. Circumsolar corona / solar aureole around the sun disc
    float corona = pow(clamp(cosTheta, 0.0, 1.0), 24.0) * 1.10 + pow(clamp(cosTheta, 0.0, 1.0), 160.0) * 3.5;
    vec3 coronaColor = mix(vec3(2.2, 1.9, 1.4), vec3(4.2, 2.4, 0.8), goldenHour);
    skyRgb += coronaColor * (corona * isDay * sunIntensity);

    // C. Anti-solar horizon arch (Belt of Venus) opposite setting sun
    if (goldenHour > 0.05 && V.y > -0.02) {
        float antiSolar = pow(clamp(-cosTheta, 0.0, 1.0), 2.2) * smoothstep(-0.02, 0.12, V.y) * smoothstep(0.35, 0.08, V.y);
        skyRgb += vec3(0.18, 0.08, 0.12) * (antiSolar * goldenHour);
    }

    // -------------------------------------------------------------
    // 5. Procedural Night Stellar Field
    // -------------------------------------------------------------
    if (isDay < 0.75 && V.y > 0.06) {
        // Fixed spherical grid for stars
        vec2 starGrid = vec2(atan(V.z, V.x) * 48.0, acos(clamp(V.y, -1.0, 1.0)) * 48.0);
        vec2 cellId = floor(starGrid);
        float h = hash21(cellId);
        if (h > 0.982) {
            vec2 cellF = fract(starGrid) - 0.5;
            float starDist = length(cellF);
            float starInt = smoothstep(0.28, 0.02, starDist);

            // Subtle twinkling
            float twinkle = sin(pc.camPos.w * 3.5 + h * 6.28) * 0.25 + 0.75;
            float nightFade = (1.0 - isDay / 0.75) * smoothstep(0.06, 0.22, V.y);

            vec3 starTint = mix(vec3(0.8, 0.9, 1.0), vec3(1.0, 0.85, 0.7), fract(h * 37.0));
            skyRgb += starTint * (starInt * twinkle * nightFade * 0.85);
        }
    }

    // -------------------------------------------------------------
    // 6. Output linear HDR radiance directly to HDR buffer (master tonemapping handled in post-processing pass)
    outColor = vec4(skyRgb, 1.0);
}
