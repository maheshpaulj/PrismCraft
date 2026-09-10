#include "CloudMapGenerator.hpp"
#include "rhi/Texture.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace prismcraft {

// Deterministic integer hash
static inline uint32_t hash32(uint32_t a, uint32_t b, uint32_t c, uint32_t seed) {
    uint32_t h = seed ^ (a * 73856093u) ^ (b * 19349663u) ^ (c * 83492791u);
    h ^= h >> 13;
    h *= 0x5bd1e995;
    h ^= h >> 15;
    return h;
}

// -------------------------------------------------------------
// Periodic Worley Noise Generator (Seamless Toroidal Wrap)
// Quadratic curve creates deep zero-valleys (open clear sky)
// and distinct, punchy rounded cloud puff centers.
// -------------------------------------------------------------
static float samplePeriodicWorley(float u, float v, int numCells, uint32_t seed, uint32_t salt) {
    float cellU = u * static_cast<float>(numCells);
    float cellV = v * static_cast<float>(numCells);

    int iU = static_cast<int>(std::floor(cellU));
    int iV = static_cast<int>(std::floor(cellV));

    float minDist = 999.0f;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = (iU + dx) % numCells;
            if (cx < 0) cx += numCells;
            int cy = (iV + dy) % numCells;
            if (cy < 0) cy += numCells;

            uint32_t h = hash32(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), salt, seed);
            float jx = (h & 0xFFFF) / 65535.0f * 0.74f + 0.13f;
            float jy = ((h >> 16) & 0xFFFF) / 65535.0f * 0.74f + 0.13f;

            float featureU = static_cast<float>(iU + dx) + jx;
            float featureV = static_cast<float>(iV + dy) + jy;

            float diffU = cellU - featureU;
            float diffV = cellV - featureV;
            float dist = std::sqrt(diffU * diffU + diffV * diffV);
            if (dist < minDist) {
                minDist = dist;
            }
        }
    }

    float normDist = std::clamp(minDist / 0.88f, 0.0f, 1.0f);
    float val = 1.0f - normDist;
    return val * val; // Quadratic punch
}

// -------------------------------------------------------------
// Periodic Perlin Noise Generator (Seamless Toroidal Wrap)
// -------------------------------------------------------------
static inline float quinticFade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float samplePeriodicPerlinSingle(float u, float v, int numCells, uint32_t seed, uint32_t salt) {
    float cellU = u * static_cast<float>(numCells);
    float cellV = v * static_cast<float>(numCells);

    int iU = static_cast<int>(std::floor(cellU));
    int iV = static_cast<int>(std::floor(cellV));

    float fracU = cellU - static_cast<float>(iU);
    float fracV = cellV - static_cast<float>(iV);

    int u0 = (iU) % numCells; if (u0 < 0) u0 += numCells;
    int u1 = (iU + 1) % numCells; if (u1 < 0) u1 += numCells;
    int v0 = (iV) % numCells; if (v0 < 0) v0 += numCells;
    int v1 = (iV + 1) % numCells; if (v1 < 0) v1 += numCells;

    auto getGrad = [&](int x, int y, float& gx, float& gy) {
        uint32_t h = hash32(static_cast<uint32_t>(x), static_cast<uint32_t>(y), salt, seed);
        float angle = (h & 0xFFFF) / 65535.0f * 6.2831853f;
        gx = std::cos(angle);
        gy = std::sin(angle);
    };

    float g00x, g00y, g10x, g10y, g01x, g01y, g11x, g11y;
    getGrad(u0, v0, g00x, g00y);
    getGrad(u1, v0, g10x, g10y);
    getGrad(u0, v1, g01x, g01y);
    getGrad(u1, v1, g11x, g11y);

    float d00 = g00x * (fracU)        + g00y * (fracV);
    float d10 = g10x * (fracU - 1.0f) + g10y * (fracV);
    float d01 = g01x * (fracU)        + g01y * (fracV - 1.0f);
    float d11 = g11x * (fracU - 1.0f) + g11y * (fracV - 1.0f);

    float fu = quinticFade(fracU);
    float fv = quinticFade(fracV);

    float nx0 = d00 + fu * (d10 - d00);
    float nx1 = d01 + fu * (d11 - d01);
    float val = nx0 + fv * (nx1 - nx0);

    return std::clamp(val * 0.707f + 0.5f, 0.0f, 1.0f);
}

static float samplePeriodicPerlinFBM(float u, float v, int baseCells, int octaves, uint32_t seed, uint32_t salt) {
    float total = 0.0f;
    float amp = 0.54f;
    float maxAmp = 0.0f;
    int cells = baseCells;

    for (int o = 0; o < octaves; ++o) {
        total += amp * samplePeriodicPerlinSingle(u, v, cells, seed, salt + static_cast<uint32_t>(o * 101));
        maxAmp += amp;
        amp *= 0.5f;
        cells *= 2;
    }
    return total / maxAmp;
}

std::vector<uint8_t> CloudMapGenerator::generateOrLoad(uint32_t seed, const std::string& cacheDir) {
    std::string cachePath = cacheDir + "cloud_seed_" + std::to_string(seed) + ".bin";
    const uint32_t MAGIC = 0x434C4F32; // 'CLO2' (v2 High Contrast Worley)
    const size_t pixelBytes = MAP_SIZE * MAP_SIZE * 4;

    // 1. Try reading disk cache
    std::ifstream inFile(cachePath, std::ios::binary);
    if (inFile.is_open()) {
        uint32_t header[4];
        inFile.read(reinterpret_cast<char*>(header), sizeof(header));
        if (inFile.gcount() == sizeof(header) &&
            header[0] == MAGIC && header[1] == seed &&
            header[2] == MAP_SIZE && header[3] == MAP_SIZE) {
            std::vector<uint8_t> pixels(pixelBytes);
            inFile.read(reinterpret_cast<char*>(pixels.data()), pixelBytes);
            if (inFile.gcount() == static_cast<std::streamsize>(pixelBytes)) {
                std::cout << "[CloudMap] Loaded cached v2 cloud map for seed " << seed << " (" << cachePath << ")" << std::endl;
                return pixels;
            }
        }
        inFile.close();
    }

    std::cout << "[CloudMap] Generating high-contrast periodic cloud map for seed " << seed << " (" << MAP_SIZE << "x" << MAP_SIZE << ")..." << std::endl;
    std::vector<uint8_t> pixels(pixelBytes);

    for (uint32_t y = 0; y < MAP_SIZE; ++y) {
        float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(MAP_SIZE);
        for (uint32_t x = 0; x < MAP_SIZE; ++x) {
            float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(MAP_SIZE);

            // Channel R: Base Macro Worley (4x4 cells, distinct rounded cumulus islands with deep zero valleys)
            float rVal = samplePeriodicWorley(u, v, 4, seed, 101);
            rVal = std::pow(rVal, 1.5f);

            // Channel G: Mid-Frequency Worley (10x10 cells, secondary billow domes)
            float gVal = samplePeriodicWorley(u, v, 10, seed, 202);

            // Channel B: High-Frequency Perlin FBM (base 20x20, 3 octaves, sharpened for edge erosion)
            float bVal = samplePeriodicPerlinFBM(u, v, 20, 3, seed, 303);
            bVal = std::clamp((bVal - 0.32f) / 0.68f, 0.0f, 1.0f);

            // Channel A: Micro Wisps / Curl (base 40x40, 2 octaves, feathered shreds)
            float aVal = samplePeriodicPerlinFBM(u, v, 40, 2, seed, 404);
            aVal = std::clamp((aVal - 0.28f) / 0.72f, 0.0f, 1.0f);

            size_t idx = (y * MAP_SIZE + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(std::clamp(rVal * 255.0f, 0.0f, 255.0f));
            pixels[idx + 1] = static_cast<uint8_t>(std::clamp(gVal * 255.0f, 0.0f, 255.0f));
            pixels[idx + 2] = static_cast<uint8_t>(std::clamp(bVal * 255.0f, 0.0f, 255.0f));
            pixels[idx + 3] = static_cast<uint8_t>(std::clamp(aVal * 255.0f, 0.0f, 255.0f));
        }
    }

    // 2. Write to disk cache
    std::ofstream outFile(cachePath, std::ios::binary);
    if (outFile.is_open()) {
        uint32_t header[4] = { MAGIC, seed, MAP_SIZE, MAP_SIZE };
        outFile.write(reinterpret_cast<const char*>(header), sizeof(header));
        outFile.write(reinterpret_cast<const char*>(pixels.data()), pixelBytes);
        outFile.close();
        std::cout << "[CloudMap] Cached v2 cloud map to " << cachePath << std::endl;
    }

    return pixels;
}

std::unique_ptr<Texture> CloudMapGenerator::createCloudTexture(VulkanContext& context,
                                                               CommandQueue& cmdQueue,
                                                               uint32_t seed,
                                                               const std::string& cacheDir) {
    std::vector<uint8_t> pixels = generateOrLoad(seed, cacheDir);
    // Construct Texture with linear filtering enabled for smooth hardware interpolation
    return std::make_unique<Texture>(context, cmdQueue, MAP_SIZE, MAP_SIZE, pixels.data(), true);
}

} // namespace prismcraft