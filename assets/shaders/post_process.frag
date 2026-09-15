#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D hdrSceneTexture;

layout(push_constant) uniform PushConstants {
    vec4 params;  // x = exposure, y = vibrance, z = bloomStrength, w = time
    vec4 options; // x = vibrantVisuals (1.0 or 0.0), y = sharpening (0.12), z = colorGrading, w = isUnderwater
    vec4 sunData; // x = sunScreenU, y = sunScreenV, z = sunIntensity*isDay, w = sunHeight
} pc;

// ACES Hill / Narkowicz Fitted Filmic Tonemapper (AP0 -> AP1 -> ACES curve -> sRGB)
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

// Accurate piecewise Linear to sRGB OETF
vec3 linearToSrgb(vec3 c) {
    bvec3 cutoff = lessThanEqual(c, vec3(0.0031308));
    vec3 higher = 1.055 * pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.4)) - 0.055;
    vec3 lower  = c * 12.92;
    return clamp(mix(higher, lower, cutoff), 0.0, 1.0);
}

// Triangular noise dither to eliminate 8-bit banding
vec3 triangularDither(vec3 color, vec2 uv) {
    float r = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
    float g = fract(sin(dot(uv, vec2(93.9898, 67.345))) * 24634.6345);
    float b = fract(sin(dot(uv, vec2(41.1234, 19.987))) * 58972.1234);
    return color + (vec3(r, g, b) - 0.5) * (1.0 / 255.0);
}

// Soft-knee threshold extraction for subtle highlight bloom (sun disc, glowing torches, water glints)
vec3 extractBloom(vec3 c) {
    float brightness = max(c.r, max(c.g, c.b));
    const float threshold = 1.45;
    const float knee = 0.45;
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    float weight = max(soft, brightness - threshold) / max(brightness, 0.0001);
    return c * clamp(weight, 0.0, 1.0);
}

void main() {
    vec2 texSize = vec2(textureSize(hdrSceneTexture, 0));
    vec2 texel = 1.0 / max(texSize, vec2(1.0));

    vec2 sampleUV = inUV;
    vec3 centerHdr;

    if (pc.options.w > 0.5) {
        float t = pc.params.w;
        // Animated dual-octave refractive liquid wobble to simulate underwater optical distortion
        float w1 = sin(inUV.y * 18.0 + t * 2.2) * cos(inUV.x * 12.0 + t * 1.5) * 0.0022;
        float w2 = cos(inUV.x * 22.0 - t * 1.8) * sin(inUV.y * 16.0 + t * 2.0) * 0.0018;
        sampleUV += vec2(w1, w2);

        // Subtle underwater chromatic dispersion at screen edges
        vec2 caOffset = (inUV - 0.5) * 0.0025;
        centerHdr.r = texture(hdrSceneTexture, sampleUV + caOffset).r;
        centerHdr.g = texture(hdrSceneTexture, sampleUV).g;
        centerHdr.b = texture(hdrSceneTexture, sampleUV - caOffset).b;
    } else {
        centerHdr = texture(hdrSceneTexture, sampleUV).rgb;
    }

    bool isVibrant = (pc.options.x > 0.5);
    float exposure = (pc.params.x > 0.001) ? pc.params.x : 0.95;

    // -------------------------------------------------------------
    // 1. Multi-Tap High-Threshold Bloom (Subtle glow on sun, torches, water glints)
    // -------------------------------------------------------------
    vec3 bloom = vec3(0.0);
    if (isVibrant) {
        // Dual-radius 9-tap kernel for natural soft scattering
        vec2 r1 = texel * 2.2;
        vec2 r2 = texel * 5.0;

        vec3 b0 = extractBloom(centerHdr) * 0.22;
        vec3 b1 = extractBloom(texture(hdrSceneTexture, sampleUV + vec2( r1.x,  0.0)).rgb) * 0.12;
        vec3 b2 = extractBloom(texture(hdrSceneTexture, sampleUV + vec2(-r1.x,  0.0)).rgb) * 0.12;
        vec3 b3 = extractBloom(texture(hdrSceneTexture, sampleUV + vec2( 0.0,  r1.y)).rgb) * 0.12;
        vec3 b4 = extractBloom(texture(hdrSceneTexture, sampleUV + vec2( 0.0, -r1.y)).rgb) * 0.12;

        vec3 b5 = extractBloom(texture(hdrSceneTexture, sampleUV + vec2( r2.x,  r2.y)).rgb) * 0.075;
        vec3 b6 = extractBloom(texture(hdrSceneTexture, sampleUV + vec2(-r2.x,  r2.y)).rgb) * 0.075;
        vec3 b7 = extractBloom(texture(hdrSceneTexture, sampleUV + vec2( r2.x, -r2.y)).rgb) * 0.075;
        vec3 b8 = extractBloom(texture(hdrSceneTexture, sampleUV + vec2(-r2.x, -r2.y)).rgb) * 0.075;

        bloom = b0 + b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8;
    }

    // Composite linear radiance with restrained bloom (zero sharpening artifacts on pixel art)
    float bloomWeight = (pc.params.z > 0.0) ? pc.params.z : 0.065;
    vec3 sceneRadiance = centerHdr + bloom * bloomWeight;

    // -------------------------------------------------------------
    // 2. Volumetric Screen-Space Sun Shafts / God Rays (Radial Light Blur)
    // -------------------------------------------------------------
    if (pc.sunData.z > 0.01) {
        vec2 sunUV = pc.sunData.xy;
        vec2 deltaUV = (sampleUV - sunUV);
        float distToSun = length(deltaUV);

        // 16 samples per pixel as configured for clean quality and high performance
        const int NUM_RAYS = 16;
        vec2 stepVec = deltaUV * (1.0 / float(NUM_RAYS));

        // Interleaved pseudo-random jitter to eliminate radial stepping bands
        float jitter = fract(sin(dot(sampleUV, vec2(12.9898, 78.233)) + pc.params.w * 0.1) * 43758.5453);

        vec3 godRayAccum = vec3(0.0);
        float rayWeight = 1.0;
        float totalWeight = 0.0001;

        for (int i = 0; i < NUM_RAYS; ++i) {
            vec2 marchUV = sampleUV - stepVec * (float(i) + jitter * 0.75);
            vec3 s = texture(hdrSceneTexture, clamp(marchUV, 0.001, 0.999)).rgb;

            // Extract intense celestial light pixels ONLY (sun disc & sky horizon); NEVER lit terrain
            float luma = max(s.r, max(s.g, s.b));
            float mask = smoothstep(4.0, 9.0, luma);

            godRayAccum += s * mask * rayWeight;
            totalWeight += rayWeight;
            rayWeight *= 0.88; // Physical exponential decay along light path (no circular boundary)
        }

        godRayAccum /= totalWeight;

        // Dynamic warm sunbeam tint: warm daylight -> rich fiery golden-amber at sunset
        float goldenHour = smoothstep(0.40, 0.02, pc.sunData.w) * step(-0.06, pc.sunData.w);
        vec3 rayColor = mix(vec3(1.02, 0.96, 0.88), vec3(1.15, 0.78, 0.38), goldenHour);

        // Soft screen border vignette so rays don't abruptly clip at viewport edges
        float borderFade = smoothstep(0.0, 0.08, sampleUV.x) * smoothstep(1.0, 0.92, sampleUV.x) *
                           smoothstep(0.0, 0.08, sampleUV.y) * smoothstep(1.0, 0.92, sampleUV.y);

        // Gentle central fade directly at sun center
        float centerFade = smoothstep(0.02, 0.12, distToSun);

        float godRayStrength = 0.15 * pc.sunData.z * borderFade * centerFade;
        sceneRadiance += godRayAccum * rayColor * godRayStrength;
    }

    // -------------------------------------------------------------
    // 3. Controlled Exposure & ACES Filmic Tone Mapping
    // -------------------------------------------------------------
    vec3 exposed = sceneRadiance * exposure;
    vec3 tonemapped = acesFilmicTonemap(exposed);

    // -------------------------------------------------------------
    // 4. Cinematic Color Grading (Subtle shadow lift & daylight warmth)
    // -------------------------------------------------------------
    if (isVibrant) {
        // Gentle shadow lift to prevent crushed blacks in foliage and caves
        tonemapped = max(tonemapped, vec3(0.008, 0.012, 0.020));

        // Balanced Film Vibrance: calm, natural saturation without radioactive colors
        float luma = dot(tonemapped, vec3(0.2126, 0.7152, 0.0722));
        vec3 vibranceBoost = mix(vec3(luma), tonemapped, 0.96);

        // Subtle solar warmth curve: gentle golden-amber highlight response at low sun angles
        float goldenHour = smoothstep(0.40, 0.02, pc.sunData.w) * step(-0.06, pc.sunData.w);
        vec3 warmTint = mix(vec3(1.01, 1.005, 0.985), vec3(1.04, 1.015, 0.95), goldenHour);
        tonemapped = mix(vibranceBoost, vibranceBoost * warmTint, clamp(luma * 0.50, 0.0, 1.0));
    }

    // -------------------------------------------------------------
    // 5. Final sRGB Gamma & Anti-Banding Dither
    // -------------------------------------------------------------
    vec3 srgb = linearToSrgb(tonemapped);
    srgb = triangularDither(srgb, inUV);

    outColor = vec4(clamp(srgb, 0.0, 1.0), 1.0);
}
