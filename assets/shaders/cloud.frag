#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor; // Face brightness shading (top=1.0, sides=0.86, bot=0.72)
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

void main() {
    // Determine day / night ratio from sky fog color
    float isDay = clamp(pc.skyFog.b * 1.8 - 0.25, 0.0, 1.0);

    // Fluffy cloud colors: crisp white at day, midnight navy/grey at night
    vec3 dayCloud = vec3(1.0, 1.0, 1.0);
    vec3 nightCloud = vec3(0.06, 0.08, 0.14);
    vec3 baseCloudColor = mix(nightCloud, dayCloud, isDay);

    // Warm sunset / sunrise golden glow when sun is near horizon
    float sunAltitude = pc.sunDir.y;
    float sunsetFactor = smoothstep(0.40, 0.05, abs(sunAltitude)) * step(0.0, sunAltitude);
    vec3 sunsetColor = vec3(1.0, 0.65, 0.40);
    baseCloudColor = mix(baseCloudColor, sunsetColor, sunsetFactor * 0.65);

    // Direct sun illumination on cloud tops and sunward sides (matching Image 2)
    vec3 L = normalize(pc.sunDir.xyz);
    float NdotL = max(dot(fragNormal, L), 0.0);
    vec3 goldenSun = mix(vec3(0.5, 0.6, 0.8), vec3(1.18, 1.06, 0.82), isDay);
    vec3 cloudLitColor = baseCloudColor * fragColor + goldenSun * (NdotL * 0.38 * isDay);

    // Atmospheric distance fog to blend seamlessly into the sky horizon
    float dist = length(fragWorldPos - pc.camPos.xyz);
    float fogStart = 90.0;
    float fogEnd = pc.skyFog.w;
    float fogFactor = clamp((dist - fogStart) / max(fogEnd - fogStart, 1.0), 0.0, 1.0);
    fogFactor = fogFactor * fogFactor;
    vec3 finalColor = mix(cloudLitColor, pc.skyFog.rgb, fogFactor);

    float alpha = mix(0.85, 0.0, fogFactor);
    outColor = vec4(finalColor, alpha);
}
