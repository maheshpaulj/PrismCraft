#pragma once
#include <cstdint>
#include <FastNoiseLite.h>

namespace prismcraft {

class Chunk;

class TerrainGen {
public:
    TerrainGen(uint32_t seed = 12345);
    
    void generateChunk(Chunk& chunk) const;
    [[nodiscard]] int getHeight(int worldX, int worldZ) const;
    
private:
    uint32_t m_seed;
    FastNoiseLite m_continentalNoise;
    FastNoiseLite m_detailNoise;
    FastNoiseLite m_caveNoise;

    void generateTree(Chunk& chunk, int localX, int baseY, int localZ) const;
};

} // namespace prismcraft
