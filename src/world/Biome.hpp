#pragma once
#include <cstdint>
#include <string_view>
#include "Cell.hpp"

namespace prismcraft {

enum class BiomeType : uint8_t {
    Plains = 0,
    Forest,
    BirchForest,
    Taiga,
    Mountains,
    Desert,
    Savanna,
    Swamp,
    Ocean,
    COUNT
};

inline std::string_view getBiomeName(BiomeType type) {
    switch (type) {
        case BiomeType::Plains:      return "Plains";
        case BiomeType::Forest:      return "Forest";
        case BiomeType::BirchForest: return "Birch Forest";
        case BiomeType::Taiga:       return "Taiga";
        case BiomeType::Mountains:   return "Mountains";
        case BiomeType::Desert:      return "Desert";
        case BiomeType::Savanna:     return "Savanna";
        case BiomeType::Swamp:       return "Swamp";
        case BiomeType::Ocean:       return "Ocean";
        default:                     return "Plains";
    }
}

inline bool isColdBiome(BiomeType type) {
    return type == BiomeType::Taiga || type == BiomeType::Mountains;
}

inline bool isDryBiome(BiomeType type) {
    return type == BiomeType::Desert || type == BiomeType::Savanna;
}

inline BlockType getBiomeSurfaceBlock(BiomeType type, int height) {
    switch (type) {
        case BiomeType::Desert:
            return BlockType::Sand;
        case BiomeType::Mountains:
            if (height >= 85) return BlockType::Snow;
            if (height >= 75) return BlockType::Stone;
            return BlockType::Grass;
        case BiomeType::Taiga:
            if (height >= 78) return BlockType::Snow;
            return BlockType::Grass;
        case BiomeType::Swamp:
            return BlockType::Grass;
        case BiomeType::Ocean:
            return BlockType::Sand;
        default:
            return BlockType::Grass;
    }
}

inline BlockType getBiomeSubsurfaceBlock(BiomeType type) {
    switch (type) {
        case BiomeType::Desert:
            return BlockType::Sandstone;
        case BiomeType::Ocean:
            return BlockType::Gravel;
        default:
            return BlockType::Dirt;
    }
}

} // namespace prismcraft
