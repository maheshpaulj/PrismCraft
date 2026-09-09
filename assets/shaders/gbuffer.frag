#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec3 fragWorldPos;

layout(binding = 0) uniform sampler2DArray blockTextureArray;

layout(location = 0) out vec4 outAlbedo;         // RGB: Albedo, A: Roughness
layout(location = 1) out vec4 outNormalMetallic; // XYZ: Normal, W: Metallic

vec3 getPerturbedNormal(vec3 worldNormal, vec3 tangentNormal) {
    vec3 up = abs(worldNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 tangent = normalize(cross(up, worldNormal));
    vec3 bitangent = cross(worldNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, worldNormal);
    return normalize(TBN * tangentNormal);
}

void main() {
    // Layer 0: Albedo
    vec4 texCol = texture(blockTextureArray, vec3(fragTexCoord, 0.0));
    if (texCol.a < 0.1) {
        discard;
    }

    vec3 albedo = texCol.rgb;

    // Foliage grass tint
    if (fragColor.r < 0.99 || fragColor.g < 0.99 || fragColor.b < 0.99) {
        if (fragColor.r > 0.05 && fragColor.g > 0.05) {
            albedo *= fragColor;
        }
    }

    // Layer 1: Normal map
    vec4 normalSample = texture(blockTextureArray, vec3(fragTexCoord, 1.0));
    vec3 tangentNormal = normalSample.rgb * 2.0 - 1.0;
    vec3 worldN = getPerturbedNormal(normalize(fragNormal), tangentNormal);

    // Layer 2: ORM (R=AO, G=Roughness, B=Metallic, A=Emissive)
    vec4 ormSample = texture(blockTextureArray, vec3(fragTexCoord, 2.0));
    float roughness = clamp(ormSample.g, 0.04, 1.0);
    float metallic = ormSample.b;
    float emissive = ormSample.a;

    if (emissive > 0.1) {
        albedo += albedo * emissive * 2.5;
    }

    outAlbedo = vec4(albedo, roughness);
    outNormalMetallic = vec4(worldN, metallic);
}
