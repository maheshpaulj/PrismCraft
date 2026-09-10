#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(binding = 0) uniform sampler2D texSampler;

void main() {
    float alpha = texture(texSampler, fragTexCoord).a;
    if (alpha < 0.35) {
        discard;
    }
}