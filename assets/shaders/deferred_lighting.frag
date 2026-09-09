#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outHDRColor;

layout(binding = 0) uniform sampler2D gAlbedo;
layout(binding = 1) uniform sampler2D gNormal;
layout(binding = 2) uniform sampler2D gDepth;
layout(binding = 3) uniform sampler2DArrayShadow shadowMapArray;

layout(binding = 4) uniform LightingUBO {
    mat4 invViewProj;
    mat4 lightViewProj[3];
    vec4 cascadeSplits; // x=16.0, y=54.0, z=200.0
    vec4 sunDir;        // xyz = light dir, w = intensity
    vec4 camPos;        // xyz = cam pos, w = time
    vec4 skyColor;      // xyz = ambient sky color
    vec4 heldTorch;     // xyz = held torch pos, w = active
    vec4 pointLight1;   // xyz = dropped torch pos, w = intensity
    vec4 fogParams;     // x = fogStart, y = fogEnd, z = isUnderwater, w = fogFalloff
} ubo;

vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    vec4 world = ubo.invViewProj * clip;
    return world.xyz / world.w;
}

float sampleShadowPCF(vec3 worldPos, vec3 normal, vec3 lightDir, int cascade) {
    float texelSize = 1.0 / 2048.0;
    float slopeFactor = sqrt(1.0 - clamp(dot(normal, lightDir), 0.0, 1.0));
    // World-space normal-offset bias tailored for triangular prism hypotenuse faces
    vec3 biasedPos = worldPos + normal * (texelSize * 2.8 * (1.0 + slopeFactor * 2.2));

    vec4 lightSpace = ubo.lightViewProj[cascade] * vec4(biasedPos, 1.0);
    vec3 projCoords = lightSpace.xyz / lightSpace.w;
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = 1.0 - (projCoords.y * 0.5 + 0.5);

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec4 coord = vec4(projCoords.xy + vec2(x, y) * texelSize, float(cascade), projCoords.z - 0.0004);
            shadow += texture(shadowMapArray, coord);
        }
    }
    return shadow / 9.0;
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (3.14159265 * denom * denom + 0.000001);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k + 0.000001);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 evalPointLight(vec3 lightPos, vec3 lightColor, float range, vec3 worldPos, vec3 N) {
    vec3 toLight = lightPos - worldPos;
    float dist = length(toLight);
    if (dist < range && dist > 0.05) {
        vec3 lightDir = toLight / dist;
        float atten = clamp(1.0 - (dist / range), 0.0, 1.0);
        atten *= atten;
        float NdotPoint = max(dot(N, lightDir), 0.0);
        return lightColor * NdotPoint * atten;
    }
    return vec3(0.0);
}

float interleavedGradientNoise(vec2 screenPos) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

float henyeyGreenstein(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 / (4.0 * 3.14159265)) * ((1.0 - g2) / pow(max(1.0 + g2 - 2.0 * g * cosTheta, 0.001), 1.5));
}

vec3 calculateVolumetricGodRays(vec3 targetWorldPos, vec3 L) {
    if (ubo.sunDir.w <= 0.05 || ubo.sunDir.y <= -0.15 || ubo.fogParams.z > 0.5) {
        return vec3(0.0);
    }

    vec3 rayDir = targetWorldPos - ubo.camPos.xyz;
    float rayDist = length(rayDir);
    if (rayDist < 0.5) return vec3(0.0);
    rayDir /= rayDist;

    float cosTheta = dot(rayDir, L);
    if (cosTheta <= -0.15) return vec3(0.0);

    float phase = henyeyGreenstein(cosTheta, 0.72);
    float maxMarchDist = min(rayDist, 55.0);
    const int STEPS = 14;
    float stepSize = maxMarchDist / float(STEPS);
    float dither = interleavedGradientNoise(gl_FragCoord.xy);
    float curDist = stepSize * dither;

    float inScatter = 0.0;
    for (int i = 0; i < STEPS; ++i) {
        vec3 p = ubo.camPos.xyz + rayDir * curDist;
        int cascade = (curDist < ubo.cascadeSplits.x) ? 0 : ((curDist < ubo.cascadeSplits.y) ? 1 : 2);

        vec4 lightSpace = ubo.lightViewProj[cascade] * vec4(p, 1.0);
        vec3 projCoords = lightSpace.xyz / lightSpace.w;
        projCoords.x = projCoords.x * 0.5 + 0.5;
        projCoords.y = 1.0 - (projCoords.y * 0.5 + 0.5);

        if (projCoords.z >= 0.0 && projCoords.z <= 1.0 &&
            projCoords.x >= 0.0 && projCoords.x <= 1.0 &&
            projCoords.y >= 0.0 && projCoords.y <= 1.0) {
            float s = texture(shadowMapArray, vec4(projCoords.xy, float(cascade), projCoords.z - 0.0006));
            float heightFactor = clamp(1.0 - (p.y - 64.0) * 0.015, 0.3, 1.2);
            inScatter += s * stepSize * heightFactor;
        }
        curDist += stepSize;
    }

    float nearFade = smoothstep(1.0, 4.0, rayDist);
    vec3 sunRayColor = mix(vec3(0.85, 0.90, 1.0), vec3(1.15, 1.02, 0.80), clamp(ubo.sunDir.y * 2.0, 0.0, 1.0));
    return sunRayColor * (inScatter * phase * ubo.sunDir.w * 0.015 * nearFade);
}

void main() {
    float depth = texture(gDepth, inUV).r;
    vec3 L = normalize(ubo.sunDir.xyz);

    if (depth >= 1.0) {
        // Celestial background + atmospheric light shafts
        vec3 skyTarget = reconstructWorldPos(inUV, 0.9999);
        vec3 godRays = calculateVolumetricGodRays(skyTarget, L);
        outHDRColor = vec4(ubo.skyColor.rgb + godRays, 1.0);
        return;
    }

    vec4 albedoSample = texture(gAlbedo, inUV);
    vec4 normalSample = texture(gNormal, inUV);

    vec3 albedo = albedoSample.rgb;
    float roughness = clamp(albedoSample.a, 0.04, 1.0);
    vec3 N = normalize(normalSample.xyz);
    float metallic = clamp(normalSample.w, 0.0, 1.0);

    vec3 worldPos = reconstructWorldPos(inUV, depth);
    vec3 V = normalize(ubo.camPos.xyz - worldPos);
    vec3 H = normalize(L + V);

    float viewDist = length(worldPos - ubo.camPos.xyz);
    int cascade = (viewDist < ubo.cascadeSplits.x) ? 0 : (viewDist < ubo.cascadeSplits.y ? 1 : 2);

    float shadow = sampleShadowPCF(worldPos, N, L, cascade);

    // Bedrock RTX PBR Specular Reflection (Cook-Torrance)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    vec3 sunlightColor = vec3(1.0, 0.94, 0.85);
    vec3 directSun = (kD * albedo / 3.14159265 + specular) * sunlightColor * (NdotL * shadow * ubo.sunDir.w);

    // Ambient lighting with sky hemisphere bounce
    vec3 ambient = (0.10 + 0.08 * N.y) * ubo.skyColor.rgb * albedo;

    // Dynamic torch point lights
    vec3 torchLight = vec3(0.0);
    if (ubo.heldTorch.w > 0.5) {
        torchLight += evalPointLight(ubo.heldTorch.xyz, vec3(1.0, 0.65, 0.25) * 1.8, 14.0, worldPos, N) * albedo;
    }
    if (ubo.pointLight1.w > 0.05) {
        torchLight += evalPointLight(ubo.pointLight1.xyz, vec3(1.0, 0.65, 0.25) * ubo.pointLight1.w * 1.5, 12.0, worldPos, N) * albedo;
    }

    vec3 godRays = calculateVolumetricGodRays(worldPos, L);

    vec3 sceneColor = directSun + ambient + torchLight + godRays;

    // Atmospheric Distance Fog (Smooth horizon blend for distant LOD)
    float fogFactor = clamp((viewDist - ubo.fogParams.x) / max(1.0, ubo.fogParams.y - ubo.fogParams.x), 0.0, 1.0);
    fogFactor = fogFactor * fogFactor;
    vec3 finalColor = mix(sceneColor, ubo.skyColor.rgb, fogFactor);

    outHDRColor = vec4(finalColor, 1.0);
}
