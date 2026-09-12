#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor; // r = skylight/shadow (0.08..1.0), g = smoothLighting (faceDir * vertexAO), b = torchLight (or torchLight + 2.0 if submerged)
layout(location = 3) in vec3 fragWorldPos;

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform sampler2DArrayShadow shadowMap;

layout(std140, binding = 2) uniform ShadowUBO {
    mat4 lightViewProj[2];
    vec4 cascadeSplits; // x = split0, y = split1
} shadowUBO;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 sunDir;        // xyz = normalized active light dir, w = signed intensity
    vec4 skyFog;        // xyz = fog color, w = fog distance (negative if underwater)
    vec4 camPos;        // xyz = camera pos, w = time
    vec4 playerPos;     // xyz = player pos, w = packed
    vec4 pointLight1;   // xyz = dynamic light 1 pos, w = intensity
    vec4 pointLight2;   // xyz = dynamic light 2 pos, w = intensity
    vec4 heldTorch;     // xyz = exact held torch world pos, w = active
    vec4 shaderOptions; // x = shadowQuality, y = waterQuality, z = colorGrading, w = packed settingsFlags
    vec4 dayInfo;       // x = isDay, y = sunHeight, z = 0, w = 0
    vec4 pointLight3;   // xyz = dynamic light 3 pos, w = intensity
    vec4 pointLight4;   // xyz = dynamic light 4 pos, w = intensity
} pc;

layout(location = 0) out vec4 outColor;

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

// Academy Color Encoding System (ACES) Hill / Narkowicz Fitted
// Transforms from linear sRGB to ACES AP1 wide gamut, evaluates RRT curve, and transforms back to sRGB.
// Ensures natural desaturation of highlights towards white without hue shifts or neon saturation burn.
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

// Physically-Based Windowed Dynamic Point Light (Solid 7 blocks illumination)
vec3 evalPointLight(vec3 lightPos, float intensity, float radius, vec3 fragPos, vec3 N, bool vibrant, bool optTorchColorBleed) {
    if (intensity <= 0.001) return vec3(0.0);
    vec3 toLight = lightPos - fragPos;
    float dist = length(toLight);
    if (dist >= radius || dist < 0.001) return vec3(0.0);

    vec3 L = toLight / dist;
    float rawNdotL = dot(N, L);
    // Faces pointing away from torch are in their own shadow (no light bleed through blocks)
    if (rawNdotL <= -0.02) return vec3(0.0);

    float NdotL = max(rawNdotL, 0.0);
    float diffuse = NdotL * 0.75 + 0.25 * smoothstep(0.0, 0.45, rawNdotL);

    // Physically-based windowed attenuation: solid light for 7 blocks, smoothly tapering to zero at radius (15m)
    float normDist = dist / radius;
    float window = clamp(1.0 - normDist * normDist, 0.0, 1.0);
    float eDist = max(dist, 1.15);
    float atten = (window * window) / (eDist * 0.35 + 0.65);

    // Dynamic torch incandescent color gradient (core warm white -> amber -> deep ember)
    vec3 coreCol = vec3(1.0, 0.92, 0.78);
    vec3 midCol  = vec3(1.0, 0.62, 0.18);
    vec3 edgeCol = vec3(0.95, 0.35, 0.06);
    vec3 lightCol = mix(mix(coreCol, midCol, smoothstep(0.0, 0.35, normDist)), edgeCol, smoothstep(0.35, 1.0, normDist));
    if (!vibrant || !optTorchColorBleed) {
        lightCol = vec3(1.0, 0.65, 0.22);
    }

    return lightCol * (diffuse * atten * intensity * 2.4);
}

// Real-Time Dynamic Light Shadow: Player body casts shadow from held torch
float computePlayerTorchOcclusion(vec3 fragPos, vec3 torchPos, vec3 playerPos) {
    // 1. Steve's own body is never self-occluded by the cylinder shadow!
    vec2 toFragXZ = fragPos.xz - playerPos.xz;
    if (dot(toFragXZ, toFragXZ) < 0.26 && fragPos.y >= (playerPos.y - 0.1) && fragPos.y <= (playerPos.y + 2.0)) {
        return 1.0;
    }
    if (length(fragPos - torchPos) < 0.75) return 1.0; // Held torch and player hand are never self-occluded!

    vec3 rayDir = fragPos - torchPos;
    float rayLen = length(rayDir);
    if (rayLen < 0.2) return 1.0;

    // Test ray against player cylinder (radius 0.38m, height [playerPos.y + 0.35m, playerPos.y + 1.85m])
    vec2 pXZ = playerPos.xz;
    vec2 dXZ = rayDir.xz;
    float dLen2 = dot(dXZ, dXZ);
    if (dLen2 > 0.0001) {
        float t = clamp(dot(pXZ - torchPos.xz, dXZ) / dLen2, 0.0, 1.0);
        if (t > 0.06 && t < 0.94) {
            vec2 closestXZ = torchPos.xz + dXZ * t;
            float distXZ = length(closestXZ - pXZ);
            float hitY = torchPos.y + rayDir.y * t;
            if (hitY >= (playerPos.y + 0.35) && hitY <= (playerPos.y + 1.85)) {
                float shadow = smoothstep(0.04, 0.58, distXZ);
                return mix(0.38, 1.0, shadow);
            }
        }
    }
    return 1.0;
}

// High-frequency screen-space dither noise (Jimenez / Next Generation Post Processing)
float interleavedGradientNoise(vec2 screenPos) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

// 16 Vogel disk sample offsets uniformly distributed in the unit circle
const vec2 VOGEL_16[16] = vec2[](
    vec2( 0.1705,  0.0384),
    vec2(-0.2229,  0.2238),
    vec2( 0.0537, -0.3926),
    vec2( 0.2312,  0.3957),
    vec2(-0.4851, -0.1689),
    vec2( 0.4727, -0.3204),
    vec2(-0.1689,  0.6148),
    vec2(-0.3168, -0.6033),
    vec2( 0.6970,  0.2520),
    vec2(-0.6934,  0.3340),
    vec2( 0.3015, -0.7511),
    vec2( 0.2921,  0.7937),
    vec2(-0.7904, -0.3703),
    vec2( 0.8659, -0.2878),
    vec2(-0.4735,  0.8174),
    vec2(-0.1983, -0.9634)
);

// Evaluates PCF shadow on a specific cascade with precision bias and rotated Vogel disk filter
float evaluateCascadeShadow(int cascade, vec3 worldPos, vec3 N, vec3 L, mat2 rotMat, int quality) {
    float worldTexelSize = (cascade == 0) ? 0.024 : 0.082;
    float texelSize = 1.0 / 2048.0;

    // Slope-scaled normal offset bias tied to cascade world texel footprint
    float cosAlpha = clamp(dot(N, L), 0.0, 1.0);
    float sinAlpha = sqrt(max(1.0 - cosAlpha * cosAlpha, 0.0));
    float tanAlpha = sinAlpha / max(cosAlpha, 0.08);
    float normalBias = worldTexelSize * clamp(tanAlpha, 0.0, 1.8);
    vec3 biasedPos = worldPos + N * normalBias;

    vec4 sc = shadowUBO.lightViewProj[cascade] * vec4(biasedPos, 1.0);
    vec3 shadowCoords = sc.xyz / sc.w;

    if (shadowCoords.x < 0.001 || shadowCoords.x > 0.999 ||
        shadowCoords.y < 0.001 || shadowCoords.y > 0.999 ||
        shadowCoords.z < 0.0 || shadowCoords.z > 1.0) {
        return 1.0;
    }

    // Minimized depth bias since hardware depth bias is already applied in rasterizer
    float depthBias = max(0.00030 * (1.0 - cosAlpha), 0.00008);
    float compareDepth = shadowCoords.z - depthBias;

    // Radius in shadow map UV coordinates
    float filterRadius = (cascade == 0) ? (2.4 * texelSize) : (1.9 * texelSize);

    float shadow = 0.0;
    if (quality >= 3) {
        // High Quality: 16-tap rotated Vogel disk PCF (64 bilinear depth comparisons)
        for (int i = 0; i < 16; ++i) {
            vec2 offset = rotMat * VOGEL_16[i] * filterRadius;
            shadow += texture(shadowMap, vec4(shadowCoords.xy + offset, float(cascade), compareDepth));
        }
        shadow *= (1.0 / 16.0);
    } else if (quality == 2) {
        // Medium Quality: 10-tap rotated Vogel disk PCF
        for (int i = 0; i < 10; ++i) {
            vec2 offset = rotMat * VOGEL_16[i] * (filterRadius * 1.25);
            shadow += texture(shadowMap, vec4(shadowCoords.xy + offset, float(cascade), compareDepth));
        }
        shadow *= (1.0 / 10.0);
    } else {
        // Low Quality: 4-tap Poisson PCF
        vec2 p4[4] = vec2[](
            vec2(-0.45, -0.45), vec2( 0.45, -0.45),
            vec2(-0.45,  0.45), vec2( 0.45,  0.45)
        );
        for (int i = 0; i < 4; ++i) {
            vec2 offset = rotMat * p4[i] * filterRadius;
            shadow += texture(shadowMap, vec4(shadowCoords.xy + offset, float(cascade), compareDepth));
        }
        shadow *= 0.25;
    }

    return shadow;
}

// Hardware PCF Real-Time Cascaded Shadow Sampling with Cross-Cascade Seamless Blending
float sampleRealtimeShadow(vec3 worldPos, vec3 N, vec3 L, int quality) {
    if (dot(N, L) <= -0.05) {
        return 0.0; // Back-facing surface is entirely in self-shadow
    }

    float viewDist = length(worldPos - pc.camPos.xyz);
    float split0 = shadowUBO.cascadeSplits.x;
    float split1 = shadowUBO.cascadeSplits.y;

    if (viewDist > split1) {
        return 1.0;
    }

    // Per-pixel interleaved gradient noise rotation matrix
    float rotAngle = interleavedGradientNoise(gl_FragCoord.xy) * 6.2831853;
    float sinR = sin(rotAngle);
    float cosR = cos(rotAngle);
    mat2 rotMat = mat2(cosR, -sinR, sinR, cosR);

    // Cascade 0 with cross-cascade transition blend
    if (viewDist < split0) {
        float shadow0 = evaluateCascadeShadow(0, worldPos, N, L, rotMat, quality);
        float blendStart = max(split0 - 6.0, 0.0);
        if (viewDist > blendStart) {
            float blendFactor = (viewDist - blendStart) / (split0 - blendStart);
            float shadow1 = evaluateCascadeShadow(1, worldPos, N, L, rotMat, quality);
            return mix(shadow0, shadow1, clamp(blendFactor, 0.0, 1.0));
        }
        return shadow0;
    }

    // Cascade 1
    float shadow1 = evaluateCascadeShadow(1, worldPos, N, L, rotMat, quality);
    float fadeStart = max(split1 - 16.0, split0);
    if (viewDist > fadeStart) {
        float fade = (viewDist - fadeStart) / (split1 - fadeStart);
        return mix(shadow1, 1.0, clamp(fade, 0.0, 1.0));
    }
    return shadow1;
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

    // Macro cluster coordinate matching cloud.frag: 0.00025
    vec2 pMacro = ws * 0.00025;
    float macro = sin(pMacro.x * 3.14 + cos(pMacro.y * 2.5)) * cos(pMacro.y * 3.14) * 0.5 + 0.5;
    float coverage = smoothstep(0.42, 0.65, macro);
    return 1.0 - coverage * 0.65;
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


void main() {
    // 1. Self-illuminated celestial bodies (Sun, Moon) & HUD overlays
    if (abs(pc.sunDir.w) >= 1.9) {
        vec3 emissive = fragColor * 3.5;
        outColor = vec4(emissive, 1.0);
        return;
    }

    vec4 texSample = texture(texSampler, fragTexCoord);
    if (texSample.a < 0.35) {
        discard;
    }

    // Convert authored sRGB texture albedo into true physical linear reflectance
    vec3 albedo = srgbToLinear(texSample.rgb);

    // Unpack Shader & Graphics Options
    bool vibrant = (pc.sunDir.w > 0.0);
    int optShadowQuality = int(pc.shaderOptions.x + 0.5);
    int optWaterQuality = int(pc.shaderOptions.y + 0.5);
    int optColorGrading = int(pc.shaderOptions.z + 0.5);
    int settingsFlags = int(pc.shaderOptions.w + 0.5);
    bool optPlayerShadow = (settingsFlags & 1) != 0;
    bool optClouds = (settingsFlags & 2) != 0;
    bool optCloudShadows = (settingsFlags & 4) != 0;
    bool optSmoothLighting = (settingsFlags & 8) != 0;
    bool optTorchColorBleed = (settingsFlags & 16) != 0;

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(pc.sunDir.xyz);
    vec3 V = normalize(pc.camPos.xyz - fragWorldPos);
    float NdotL = dot(N, L);

    float sunIntensity = abs(pc.sunDir.w);

    // Day / Night Factor: 1.0 = full daylight, 0.0 = dark night
    float isDay = pc.dayInfo.x;
    float actualSunY = pc.dayInfo.y;

    // 2. Direct Sunlight / Moonlight Shading with Real-Time & Block Shadows
    float blockShadow = fragColor.r;        // 1.0 = exposed to sky, 0.08 = beneath terrain/overhangs
    float smoothLighting = optSmoothLighting ? fragColor.g : 1.0;

    float viewDist = length(fragWorldPos - pc.camPos.xyz);
    float rtShadow = 1.0;
    if (vibrant && (optShadowQuality > 0)) {
        rtShadow = sampleRealtimeShadow(fragWorldPos, N, L, optShadowQuality);
    }

    // Seamless blend between real-time CSM and distant terrain static shadow
    float split1 = shadowUBO.cascadeSplits.y;
    float fadeToDistant = clamp((viewDist - (split1 - 16.0)) / 16.0, 0.0, 1.0);
    float combinedShadow = (vibrant && (optShadowQuality > 0))
                         ? mix(rtShadow, blockShadow, fadeToDistant)
                         : blockShadow;

    // Natural diffuse falloff with soft wrap at terminator to prevent harsh black pixel boundaries
    float diff = clamp((NdotL + 0.06) / 1.06, 0.0, 1.0);
    float directFactor = diff * combinedShadow * sunIntensity;

    // Dynamic Cloud Shadows on Terrain: Synchronized 1:1 with sky clouds
    if (vibrant && optCloudShadows && (optShadowQuality > 0) && isDay > 0.1 && L.y > 0.03) {
        float cShadow = sampleCloudShadow(fragWorldPos, L);
        directFactor *= cShadow;
    }

    // Golden-hour factor: peaks when actual sun is at horizon angles
    float goldenHour = smoothstep(0.40, 0.02, actualSunY) * step(-0.05, actualSunY);

    // Physical solar spectral irradiance in linear units
    // Midday: warm solar white (~5500K)
    vec3 midDaySun = vec3(2.80, 2.65, 2.25);
    // Sunset / Golden Hour: rich radiant amber gold
    vec3 goldenSun = vec3(3.50, 2.20, 0.80);
    vec3 sunColor  = mix(midDaySun, goldenSun, goldenHour);
    // Moonlight: soft lunar silver
    vec3 moonColor = vec3(0.28, 0.36, 0.52);
    vec3 celestialLightColor = mix(moonColor, sunColor, isDay) * directFactor;

    // 3. Foliage Translucency / Backlight (Subsurface transmission through leaves and grass)
    bool isFoliage = (texSample.g > texSample.r * 1.05 && texSample.g > texSample.b * 1.02) || (texSample.a < 0.95);
    vec3 foliageGlow = vec3(0.0);
    if (vibrant && isFoliage) {
        // Natural canopy hue and value variation per world chunk/tree position
        vec3 leafHash = sin(floor(fragWorldPos * 0.35) * 1.57 + vec3(0.0, 1.8, 3.4));
        float hueShift = leafHash.x * 0.05;
        float valShift = leafHash.y * 0.06;
        albedo = albedo * vec3(1.0 + valShift - hueShift * 0.5, 1.0 + hueShift, 0.94 - hueShift);

        // Chlorophyll forward scattering & two-sided wrap diffuse backlight
        float sss = pow(clamp(dot(V, -L), 0.0, 1.0), 3.0);
        float leafWrap = clamp((dot(N, L) + 0.4) / 1.4, 0.0, 1.0);
        vec3 chlorophyllTint = albedo * vec3(1.10, 1.18, 0.82);
        float foliageTransmission = (sss * 0.65 + leafWrap * 0.35) * combinedShadow * isDay * sunIntensity;
        foliageGlow = chlorophyllTint * foliageTransmission;
    }

    // 4. Ambient Skylight & Contact Occlusion in Linear Space
    float skyHemisphere = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambientGround = mix(vec3(0.22, 0.26, 0.18), vec3(0.30, 0.24, 0.14), goldenHour);
    vec3 ambientSky    = mix(vec3(0.46, 0.58, 0.78), vec3(0.55, 0.42, 0.28), goldenHour);
    vec3 daySkyAmbient = mix(ambientGround, ambientSky, skyHemisphere);
    vec3 nightSkyAmbient = mix(vec3(0.065, 0.080, 0.120), vec3(0.095, 0.120, 0.175), skyHemisphere);
    vec3 ambientLight = mix(nightSkyAmbient, daySkyAmbient, isDay);

    // Directional ambient response: vertical walls facing away from the sun receive cooler, darker ambient (~25% darker)
    vec2 Lxz = (length(L.xz) > 0.001) ? normalize(L.xz) : vec2(0.0, 1.0);
    vec2 Nxz = (length(N.xz) > 0.001) ? normalize(N.xz) : vec2(0.0, 0.0);
    float horizFacing = dot(Nxz, Lxz); // +1.0 facing sun, -1.0 facing away
    float wallWeight = 1.0 - abs(N.y); // 1.0 for vertical walls, 0.0 for horizontal floors/ceilings
    float dirAmbientMod = 1.0 + (horizFacing * 0.25) * wallWeight * isDay;
    ambientLight *= dirAmbientMod;

    // Calibrated contact AO for grounded corners, crevices, and wall seams
    float skyOcc = mix(0.40 * smoothstep(0.02, 0.18, blockShadow), 1.0, clamp(blockShadow * 1.15, 0.0, 1.0));
    float optAoStrength = (pc.shaderOptions.z > 0.001) ? pc.shaderOptions.z : 1.0;
    float rawAo = optSmoothLighting ? pow(clamp(smoothLighting, 0.0, 1.0), 1.35) : 1.0;
    float ao = mix(1.0, rawAo, optAoStrength);
    float contactAmbient = mix(0.32, 1.0, ao);
    float contactDirect  = mix(0.55, 1.0, ao);

    ambientLight = ambientLight * skyOcc * contactAmbient;
    celestialLightColor *= contactDirect;

    // 5. Dynamic Point Lights with Smooth Inverse-Square Falloff (Torches)
    vec3 torchLight = vec3(0.0);

    // A. Handheld Torch (in player's right hand)
    if (pc.heldTorch.w > 0.001) {
        float playerOcc = optPlayerShadow ? computePlayerTorchOcclusion(fragWorldPos, pc.heldTorch.xyz, pc.playerPos.xyz) : 1.0;
        torchLight += evalPointLight(pc.heldTorch.xyz, pc.heldTorch.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * (playerOcc * smoothLighting);
    }

    // B. Nearest Placed and Dropped Torches (dynamic per-pixel point lights 1 to 4)
    if (pc.pointLight1.w > 0.001) {
        torchLight += evalPointLight(pc.pointLight1.xyz, pc.pointLight1.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * smoothLighting;
    }
    if (pc.pointLight2.w > 0.001) {
        torchLight += evalPointLight(pc.pointLight2.xyz, pc.pointLight2.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * smoothLighting;
    }
    if (pc.pointLight3.w > 0.001) {
        torchLight += evalPointLight(pc.pointLight3.xyz, pc.pointLight3.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * smoothLighting;
    }
    if (pc.pointLight4.w > 0.001) {
        torchLight += evalPointLight(pc.pointLight4.xyz, pc.pointLight4.w, 15.0, fragWorldPos, N, vibrant, optTorchColorBleed) * smoothLighting;
    }

    // C. Distant Placed World Torches (baked into vertex color b-channel)
    bool isSubmerged = (fragColor.b >= 1.5);
    float placedTorch = isSubmerged ? (fragColor.b - 2.0) : fragColor.b;
    if (placedTorch > 0.01) {
        vec3 tCol = (vibrant && optTorchColorBleed) ? vec3(2.0, 1.25, 0.35) : vec3(1.8, 1.15, 0.30);
        torchLight += tCol * (placedTorch * 1.8 * smoothLighting);
    }

    // 6. Total Linear Illumination & Surface Radiance
    vec3 totalLight = ambientLight + celestialLightColor + torchLight;
    vec3 surfaceRadiance = albedo * totalLight + foliageGlow;

    // 7. Water Column Beer-Lambert Absorption & Seabed Caustics (Submerged surfaces under water)
    // 7. Water Column Beer-Lambert Absorption & Organic Seabed Caustics (Submerged surfaces under water)
    if (isSubmerged) {
        float waterDepth = max(44.5 - fragWorldPos.y, 0.4);
        vec3 waterAbsorption = vec3(0.28, 0.10, 0.035);
        vec3 beerTransmittance = exp(-waterDepth * waterAbsorption);
        surfaceRadiance *= beerTransmittance;

        vec3 deepWaterTint = mix(vec3(0.004, 0.015, 0.045), vec3(0.008, 0.045, 0.080), isDay);
        float depthFog = 1.0 - exp(-waterDepth * 0.18);
        surfaceRadiance = mix(surfaceRadiance, deepWaterTint, depthFog * 0.75);

        // Organic fluid caustic light webs on seabed (eliminates 2D checkerboard grid)
        float t = pc.camPos.w * 0.8;
        vec2 p = fragWorldPos.xz;
        vec2 cp1 = p * 0.70 + vec2(t * 0.22, t * 0.15);
        vec2 cp2 = p * 0.95 - vec2(t * 0.18, -t * 0.25);
        float cw1 = sin(cp1.x * 2.2 + sin(cp1.y * 1.8 + t * 0.8));
        float cw2 = cos(cp1.y * 2.4 + cos(cp1.x * 1.9 - t * 0.7));
        float cw3 = sin(cp2.x * 3.1 + cos(cp2.y * 2.5 + t * 0.9));
        float cw4 = cos(cp2.y * 2.8 + sin(cp2.x * 2.7 - t * 0.6));
        float caustic1 = 1.0 - abs(cw1 + cw2) * 0.5;
        float caustic2 = 1.0 - abs(cw3 + cw4) * 0.5;
        float fluidCaustic = pow(clamp(caustic1 * caustic2, 0.0, 1.0), 2.2);

        vec3 causticColor = mix(vec3(0.12, 0.38, 0.60), vec3(1.10, 1.02, 0.80), isDay);
        float causticFade = exp(-waterDepth * 0.24);
        surfaceRadiance += fluidCaustic * 0.35 * blockShadow * max(sunIntensity, 0.20) * causticColor * causticFade;
    }

    // 8. Atmospheric Aerial Perspective & AAA Volumetric Underwater In-Scattering
    bool cameraUnderwater = (pc.skyFog.w < 0.0);
    float fogEnd = abs(pc.skyFog.w);
    vec3 linearSceneColor = surfaceRadiance;

    if (cameraUnderwater) {
        float dist = length(fragWorldPos - pc.camPos.xyz);
        vec3 rayDir = normalize(fragWorldPos - pc.camPos.xyz);

        // 1. Spectral Wavelength-Dependent Beer-Lambert Extinction
        // Red extinguishes rapidly (3m), green moderately (12m), blue/cyan penetrates furthest (24m)
        vec3 extinctionCoeff = vec3(0.32, 0.11, 0.035);
        vec3 volumeTransmittance = exp(-dist * extinctionCoeff);
        linearSceneColor = surfaceRadiance * volumeTransmittance;

        // 2. Dual-Scatter Oceanic Ambient & Directional Forward Mie Scattering (g = 0.65)
        float cosTheta = dot(rayDir, L);
        float phaseHG = (1.0 - 0.4225) / pow(1.0 + 0.4225 - 2.0 * 0.65 * cosTheta, 1.5) * 0.079577;

        vec3 deepWaterAbyss = vec3(0.005, 0.025, 0.055);
        vec3 shallowWaterTeal = vec3(0.012, 0.070, 0.115);
        vec3 sunScatterGlow = mix(vec3(0.12, 0.45, 0.55), vec3(0.95, 0.80, 0.48), goldenHour) * (phaseHG * 2.8 * isDay * sunIntensity);

        vec3 ambientInscatter = mix(shallowWaterTeal, deepWaterAbyss, clamp(dist / 24.0, 0.0, 1.0)) + sunScatterGlow;
        linearSceneColor += ambientInscatter * (1.0 - volumeTransmittance);

        // 3. Volumetric Caustic Sun Shafts (God rays cutting through the water volume)
        if (isDay > 0.05) {
            vec3 midPoint = pc.camPos.xyz + rayDir * min(dist * 0.5, 14.0);
            vec2 shaftP = midPoint.xz + vec2(pc.camPos.w * 0.20, pc.camPos.w * 0.15);
            float s1 = sin(shaftP.x * 0.8 + shaftP.y * 0.4 + pc.camPos.w * 0.6);
            float s2 = cos(shaftP.x * 0.5 - shaftP.y * 0.9 - pc.camPos.w * 0.5);
            float godRay = pow(clamp(s1 * 0.5 + s2 * 0.5 + 0.5, 0.0, 1.0), 3.0) * max(cosTheta * 0.6 + 0.4, 0.0);
            vec3 godRayColor = mix(vec3(0.15, 0.45, 0.60), vec3(0.95, 0.88, 0.58), goldenHour);
            linearSceneColor += godRayColor * (godRay * 0.32 * sunIntensity * (1.0 - exp(-dist * 0.18)));
        }

        // 4. Complete oceanic fog extinction at fogEnd (16m) so chunks dissolve into the abyss seamlessly
        float uFogFactor = smoothstep(fogEnd * 0.35, fogEnd, dist);
        vec3 linearOceanFog = srgbToLinear(pc.skyFog.rgb);
        linearSceneColor = mix(linearSceneColor, linearOceanFog, uFogFactor);
    } else {
        linearSceneColor = applyHorizonDistanceFog(
            surfaceRadiance,
            fragWorldPos,
            pc.camPos.xyz,
            L,
            pc.skyFog.rgb,
            isDay,
            goldenHour,
            fogEnd
        );
    }


    // 9. Output linear HDR radiance directly to HDR buffer (master tonemapping handled in post-processing pass)
    outColor = vec4(linearSceneColor, texSample.a);
}
