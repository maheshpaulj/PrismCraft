#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace prismcraft {

class VulkanContext;
class CommandQueue;
class Texture;

class CloudMapGenerator {
public:
    static constexpr uint32_t MAP_SIZE = 512;

    // Generates or loads cached 512x512 RGBA noise map for clouds
    // Channel R: Base Macro Worley (low-frequency cumulus lobes)
    // Channel G: Mid Worley (medium-frequency billowy puffs)
    // Channel B: High-Frequency Perlin (wispy edge erosion)
    // Channel A: Micro Wisps / Curl (feathered edge shredding)
    static std::vector<uint8_t> generateOrLoad(uint32_t seed, const std::string& cacheDir);

    // Creates GPU Texture with linear filtering and repeat addressing
    static std::unique_ptr<Texture> createCloudTexture(VulkanContext& context,
                                                       CommandQueue& cmdQueue,
                                                       uint32_t seed,
                                                       const std::string& cacheDir);
};

} // namespace prismcraft
