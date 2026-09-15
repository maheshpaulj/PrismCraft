#pragma once
#include <cstdint>
#include <string>
#include <FastNoiseLite.h>
#include "world/Biome.hpp"

namespace prismcraft {

class Chunk;

class TerrainGen {
public:
    TerrainGen(uint32_t seed = 12345);
    
    void generateChunk(Chunk& chunk) const;
    [[nodiscard]] int getHeight(int worldX, int worldZ) const;
    [[nodiscard]] BiomeType getBiome(int worldX, int worldZ) const;
    [[nodiscard]] uint32_t getSeed() const { return m_seed; }

    // Structure & Biome Locating for /locate command
    bool locateStructure(const std::string& type, int playerX, int playerZ, int& outX, int& outY, int& outZ) const;
    bool locateBiome(BiomeType biome, int playerX, int playerZ, int& outX, int& outZ) const;

private:
    uint32_t m_seed;
    FastNoiseLite m_continentalNoise;
    FastNoiseLite m_peaksNoise;
    FastNoiseLite m_detailNoise;
    FastNoiseLite m_tempNoise;
    FastNoiseLite m_humidityNoise;
    FastNoiseLite m_caveNoise1;
    FastNoiseLite m_caveNoise2;
    FastNoiseLite m_caveNoise3;

    // Trees & Vegetation
    void generateOakTree(Chunk& chunk, int localX, int baseY, int localZ) const;
    void generateBirchTree(Chunk& chunk, int localX, int baseY, int localZ) const;
    void generateSpruceTree(Chunk& chunk, int localX, int baseY, int localZ) const;
    void generateAcaciaTree(Chunk& chunk, int localX, int baseY, int localZ) const;
    void generateCactus(Chunk& chunk, int localX, int baseY, int localZ) const;

    // Structures
    void generateVillage(Chunk& chunk, int chunkX, int chunkZ) const;
    void generateDesertTemple(Chunk& chunk, int chunkX, int chunkZ) const;
    void generateWatchtower(Chunk& chunk, int chunkX, int chunkZ) const;
    void generateDungeon(Chunk& chunk, int chunkX, int chunkZ) const;

    // Helpers
    static bool isVillageChunk(int cx, int cz, uint32_t seed);
    static bool isTempleChunk(int cx, int cz, uint32_t seed);
    static bool isOutpostChunk(int cx, int cz, uint32_t seed);
    static bool isDungeonChunk(int cx, int cz, uint32_t seed);
};

} // namespace prismcraft
