#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(binding = 0) uniform sampler2DArray blockTextureArray;

void main() {
    float alpha = texture(blockTextureArray, vec3(fragTexCoord, 0.0)).a;
    if (alpha < 0.35) {
        discard;
    }
}