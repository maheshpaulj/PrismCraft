#include "PBRGenerator.hpp"
#include <cmath>
#include <algorithm>

namespace prismcraft {

std::vector<uint8_t> PBRGenerator::generateNormalMap(const std::vector<uint8_t>& albedo,
                                                     uint32_t width, uint32_t height) {
    std::vector<uint8_t> normals(width * height * 4, 0);

    auto getLum = [&](int x, int y) -> float {
        x = std::clamp(x, 0, static_cast<int>(width) - 1);
        y = std::clamp(y, 0, static_cast<int>(height) - 1);
        size_t idx = (static_cast<size_t>(y) * width + x) * 4;
        return (albedo[idx] * 0.299f + albedo[idx + 1] * 0.587f + albedo[idx + 2] * 0.114f) / 255.0f;
    };

    const float bumpStrength = 2.4f;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float lC = getLum(static_cast<int>(x), static_cast<int>(y));
            float lL = getLum(static_cast<int>(x) - 1, static_cast<int>(y));
            float lR = getLum(static_cast<int>(x) + 1, static_cast<int>(y));
            float lU = getLum(static_cast<int>(x), static_cast<int>(y) - 1);
            float lD = getLum(static_cast<int>(x), static_cast<int>(y) + 1);

            float dx = (lR - lL) * bumpStrength;
            float dy = (lD - lU) * bumpStrength;

            // Normalized tangent normal vector
            float len = std::sqrt(dx * dx + dy * dy + 1.0f);
            float nx = -dx / len;
            float ny = -dy / len;
            float nz = 1.0f / len;

            size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            normals[idx + 0] = static_cast<uint8_t>(std::clamp((nx * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
            normals[idx + 1] = static_cast<uint8_t>(std::clamp((ny * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
            normals[idx + 2] = static_cast<uint8_t>(std::clamp((nz * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
            normals[idx + 3] = static_cast<uint8_t>(std::clamp(lC * 255.0f, 0.0f, 255.0f)); // Height in alpha
        }
    }
    return normals;
}

std::vector<uint8_t> PBRGenerator::generateORMMap(const std::vector<uint8_t>& albedo,
                                                  uint32_t width, uint32_t height) {
    std::vector<uint8_t> orm(width * height * 4, 0);

    const int tileSize = 16;
    const int tilesPerRow = static_cast<int>(width) / tileSize;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            int tileCol = static_cast<int>(x) / tileSize;
            int tileRow = static_cast<int>(y) / tileSize;
            int tileIndex = tileRow * tilesPerRow + tileCol;

            size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            uint8_t r = albedo[idx + 0];
            uint8_t g = albedo[idx + 1];
            uint8_t b = albedo[idx + 2];

            float lum = (r * 0.299f + g * 0.587f + b * 0.114f) / 255.0f;
            float colorDiff = std::max({std::abs(r - g), std::abs(g - b), std::abs(b - r)}) / 255.0f;

            // Default Dielectric Block values
            uint8_t ao = static_cast<uint8_t>(std::clamp(170.0f + lum * 85.0f, 0.0f, 255.0f));
            uint8_t roughness = 210; // ~0.82 rough matte
            uint8_t metallic = 0;    // dielectric
            uint8_t emissive = 0;    // inert

            // Metals (Iron: 22, Gold: 23, Diamond: 24, Emerald: 25)
            if (tileIndex == 22 || tileIndex == 23 || tileIndex == 24 || tileIndex == 25) {
                roughness = 22;  // 0.08 mirror polish
                metallic = 255; // 1.0 pure metal
            }
            // Emissive blocks (Torch: 80, Fire: 31, Lava: 255)
            else if (tileIndex == 80 || tileIndex == 31 || tileIndex == 255) {
                roughness = 100;
                emissive = 255; // full glow!
                ao = 255;
            }
            // Ores (Coal: 34, Iron: 33, Gold: 32, Diamond: 50, Redstone: 51, Emerald: 171, Quartz: 191)
            else if (tileIndex == 32 || tileIndex == 33 || tileIndex == 34 || tileIndex == 50 ||
                     tileIndex == 51 || tileIndex == 171 || tileIndex == 191) {
                if (colorDiff > 0.12f || (tileIndex == 34 && lum < 0.22f)) {
                    // Gemstone fleck pixel!
                    if (tileIndex == 32 || tileIndex == 33 || tileIndex == 50) {
                        roughness = 35;
                        metallic = 230;
                    } else if (tileIndex == 51) {
                        roughness = 60;
                        emissive = 200; // Redstone sparkles!
                    } else {
                        roughness = 30;
                    }
                } else {
                    roughness = 215; // surrounding stone
                }
            }
            // Obsidian (37)
            else if (tileIndex == 37) {
                roughness = 55;
                metallic = 35; // glossy dark mineral sheen
            }
            // Glass (49)
            else if (tileIndex == 49) {
                roughness = 15;
            }
            // Ice (67)
            else if (tileIndex == 67) {
                roughness = 25;
            }
            // Planks (4, 198, 199)
            else if (tileIndex == 4 || tileIndex == 198 || tileIndex == 199) {
                roughness = 160; // satin wood
            }
            // Smooth Stone Slab (5, 6)
            else if (tileIndex == 5 || tileIndex == 6) {
                roughness = 135;
            }
            // Sand (18), Gravel (19), Dirt (2)
            else if (tileIndex == 18 || tileIndex == 19 || tileIndex == 2) {
                roughness = 240;
            }

            orm[idx + 0] = ao;
            orm[idx + 1] = roughness;
            orm[idx + 2] = metallic;
            orm[idx + 3] = emissive;
        }
    }
    return orm;
}

} // namespace prismcraft
