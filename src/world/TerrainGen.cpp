#include "TerrainGen.hpp"
#include "Chunk.hpp"
#include "Cell.hpp"
#include "Coordinates.hpp"
#include <FastNoiseLite.h>
#include <functional>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace prismcraft {

TerrainGen::TerrainGen(uint32_t seed) : m_seed(seed) {
    // 1. Continental Macro Noise (determines land vs ocean & large terrain swells)
    m_continentalNoise.SetSeed(m_seed);
    m_continentalNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_continentalNoise.SetFrequency(0.0022f);

    // 2. Peaks & Valleys Noise (carves dramatic mountain ranges reaching Y = 85..118)
    m_peaksNoise.SetSeed(m_seed + 101);
    m_peaksNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_peaksNoise.SetFrequency(0.0045f);

    // 3. Local Detail Noise (surface hills, dunes, rock pockets)
    m_detailNoise.SetSeed(m_seed + 202);
    m_detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_detailNoise.SetFrequency(0.018f);

    // 4. Biome Temperature Noise (Cold to Hot)
    m_tempNoise.SetSeed(m_seed + 303);
    m_tempNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_tempNoise.SetFrequency(0.0016f);

    // 5. Biome Humidity Noise (Arid to Wet)
    m_humidityNoise.SetSeed(m_seed + 404);
    m_humidityNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_humidityNoise.SetFrequency(0.0016f);

    // 6. 3D Cave Noise 1 & 2 (Continuous winding worm tunnels)
    m_caveNoise1.SetSeed(m_seed + 505);
    m_caveNoise1.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_caveNoise1.SetFrequency(0.035f);

    m_caveNoise2.SetSeed(m_seed + 606);
    m_caveNoise2.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_caveNoise2.SetFrequency(0.035f);

    // 7. 3D Cave Noise 3 (Spacious cavern chambers & rooms)
    m_caveNoise3.SetSeed(m_seed + 707);
    m_caveNoise3.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_caveNoise3.SetFrequency(0.016f);
}

BiomeType TerrainGen::getBiome(int worldX, int worldZ) const {
    float cont = m_continentalNoise.GetNoise((float)worldX, (float)worldZ * TRI_HEIGHT);
    if (cont < -0.32f) {
        return BiomeType::Ocean;
    }

    float peaks = m_peaksNoise.GetNoise((float)worldX, (float)worldZ * TRI_HEIGHT);
    float temp = m_tempNoise.GetNoise((float)worldX, (float)worldZ * TRI_HEIGHT);
    float humid = m_humidityNoise.GetNoise((float)worldX, (float)worldZ * TRI_HEIGHT);

    // Tall Mountains trigger on high peaks noise when on solid continental land
    if (peaks > 0.42f && cont > 0.05f) {
        return BiomeType::Mountains;
    }

    // Temperature & humidity classification
    if (temp < -0.22f) {
        return BiomeType::Taiga;
    }
    if (temp > 0.30f && humid < -0.15f) {
        return BiomeType::Desert;
    }
    if (temp > 0.20f && humid < 0.20f) {
        return BiomeType::Savanna;
    }
    if (humid > 0.45f && temp > -0.10f) {
        return BiomeType::Swamp;
    }
    if (humid > 0.15f) {
        return (temp > 0.05f) ? BiomeType::BirchForest : BiomeType::Forest;
    }

    return BiomeType::Plains;
}

int TerrainGen::getHeight(int worldX, int worldZ) const {
    float cont = m_continentalNoise.GetNoise((float)worldX, (float)worldZ * TRI_HEIGHT);
    float det = m_detailNoise.GetNoise((float)worldX, (float)worldZ * TRI_HEIGHT);
    float peaks = m_peaksNoise.GetNoise((float)worldX, (float)worldZ * TRI_HEIGHT);

    // Ocean / Deep River Channel
    if (cont < -0.32f) {
        float t = (-cont - 0.32f) / 0.68f;
        float h = 42.0f - t * 14.0f + det * 2.0f;
        return std::clamp(static_cast<int>(std::round(h)), 10, CHUNK_SIZE_Y - 4);
    }

    // Sandy Shore / Coast
    if (cont < -0.20f) {
        float t = (cont + 0.32f) / 0.12f;
        float h = 43.0f + t * 7.0f + det * 1.5f;
        return std::clamp(static_cast<int>(std::round(h)), 10, CHUNK_SIZE_Y - 4);
    }

    BiomeType biome = getBiome(worldX, worldZ);
    float baseH = 52.0f;

    switch (biome) {
        case BiomeType::Desert: {
            // Rolling desert dunes
            baseH = 50.0f + det * 5.0f;
            break;
        }
        case BiomeType::Plains: {
            // Vast open plains, perfect flat building terrain
            baseH = 51.5f + det * 2.0f;
            break;
        }
        case BiomeType::Savanna: {
            // Rolling dry savanna with occasional plateau swells
            float plat = std::max(0.0f, peaks) * 12.0f;
            baseH = 53.0f + det * 3.0f + plat;
            break;
        }
        case BiomeType::Forest:
        case BiomeType::BirchForest: {
            // Gentle verdant hills
            baseH = 53.5f + det * 4.5f;
            break;
        }
        case BiomeType::Swamp: {
            // Low-lying murky wetland
            baseH = 45.0f + det * 1.5f;
            break;
        }
        case BiomeType::Taiga: {
            // Cold rugged hills
            float hill = std::max(0.0f, peaks) * 16.0f;
            baseH = 55.0f + det * 4.0f + hill;
            break;
        }
        case BiomeType::Mountains: {
            // Dramatic windswept peaks reaching Y = 85..118!
            float normPeak = (peaks - 0.42f) / 0.58f; // 0.0 to 1.0
            float mountainCurve = std::pow(std::clamp(normPeak, 0.0f, 1.0f), 1.25f);
            baseH = 70.0f + mountainCurve * 46.0f + det * 6.0f;
            break;
        }
        default:
            baseH = 52.0f + det * 3.0f;
            break;
    }

    return std::clamp(static_cast<int>(std::round(baseH)), 10, CHUNK_SIZE_Y - 4);
}

// Tree Generators
void TerrainGen::generateOakTree(Chunk& chunk, int localX, int baseY, int localZ) const {
    int height = 5 + (std::hash<int>{}(localX * 73856093 ^ localZ * 19349663) % 2);
    for (int y = baseY + 1; y <= baseY + height; ++y) {
        if (y < CHUNK_SIZE_Y) {
            chunk.setCell(localX, y, localZ, 0, Cell{BlockType::Wood});
            chunk.setCell(localX, y, localZ, 1, Cell{BlockType::Wood});
        }
    }

    int leafBase = baseY + height - 2;
    for (int ly = leafBase; ly <= leafBase + 1; ++ly) {
        if (ly >= CHUNK_SIZE_Y) continue;
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dz = -2; dz <= 2; ++dz) {
                if (std::abs(dx) == 2 && std::abs(dz) == 2) {
                    uint32_t cornerHash = static_cast<uint32_t>(std::hash<int>{}(dx * 31 ^ dz * 17 ^ ly * 53));
                    if (cornerHash % 2 == 0) continue;
                }
                int lx = localX + dx;
                int lz = localZ + dz;
                if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z) {
                    if (dx == 0 && dz == 0 && ly <= baseY + height) continue;
                    if (chunk.getCell(lx, ly, lz, 0).type == BlockType::Air) {
                        chunk.setCell(lx, ly, lz, 0, Cell{BlockType::Leaves});
                        chunk.setCell(lx, ly, lz, 1, Cell{BlockType::Leaves});
                    }
                }
            }
        }
    }

    int ly2 = leafBase + 2;
    if (ly2 < CHUNK_SIZE_Y) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                int lx = localX + dx;
                int lz = localZ + dz;
                if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z) {
                    if (dx == 0 && dz == 0 && ly2 <= baseY + height) continue;
                    if (chunk.getCell(lx, ly2, lz, 0).type == BlockType::Air) {
                        chunk.setCell(lx, ly2, lz, 0, Cell{BlockType::Leaves});
                        chunk.setCell(lx, ly2, lz, 1, Cell{BlockType::Leaves});
                    }
                }
            }
        }
    }

    int ly3 = leafBase + 3;
    if (ly3 < CHUNK_SIZE_Y) {
        const int crossOffsets[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& off : crossOffsets) {
            int lx = localX + off[0];
            int lz = localZ + off[1];
            if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z) {
                if (chunk.getCell(lx, ly3, lz, 0).type == BlockType::Air) {
                    chunk.setCell(lx, ly3, lz, 0, Cell{BlockType::Leaves});
                    chunk.setCell(lx, ly3, lz, 1, Cell{BlockType::Leaves});
                }
            }
        }
    }
}

void TerrainGen::generateBirchTree(Chunk& chunk, int localX, int baseY, int localZ) const {
    int height = 6 + (std::hash<int>{}(localX * 528391 ^ localZ * 892341) % 2);
    for (int y = baseY + 1; y <= baseY + height; ++y) {
        if (y < CHUNK_SIZE_Y) {
            chunk.setCell(localX, y, localZ, 0, Cell{BlockType::LogBirch});
            chunk.setCell(localX, y, localZ, 1, Cell{BlockType::LogBirch});
        }
    }

    int leafBase = baseY + height - 2;
    for (int ly = leafBase; ly <= leafBase + 1; ++ly) {
        if (ly >= CHUNK_SIZE_Y) continue;
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dz = -2; dz <= 2; ++dz) {
                if (std::abs(dx) == 2 && std::abs(dz) == 2) continue;
                int lx = localX + dx;
                int lz = localZ + dz;
                if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z) {
                    if (dx == 0 && dz == 0 && ly <= baseY + height) continue;
                    if (chunk.getCell(lx, ly, lz, 0).type == BlockType::Air) {
                        chunk.setCell(lx, ly, lz, 0, Cell{BlockType::LeavesBirch});
                        chunk.setCell(lx, ly, lz, 1, Cell{BlockType::LeavesBirch});
                    }
                }
            }
        }
    }

    int ly2 = leafBase + 2;
    if (ly2 < CHUNK_SIZE_Y) {
        const int crossOffsets[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& off : crossOffsets) {
            int lx = localX + off[0];
            int lz = localZ + off[1];
            if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z) {
                if (chunk.getCell(lx, ly2, lz, 0).type == BlockType::Air) {
                    chunk.setCell(lx, ly2, lz, 0, Cell{BlockType::LeavesBirch});
                    chunk.setCell(lx, ly2, lz, 1, Cell{BlockType::LeavesBirch});
                }
            }
        }
    }
}

void TerrainGen::generateSpruceTree(Chunk& chunk, int localX, int baseY, int localZ) const {
    int height = 7 + (std::hash<int>{}(localX * 912837 ^ localZ * 439812) % 3);
    for (int y = baseY + 1; y <= baseY + height; ++y) {
        if (y < CHUNK_SIZE_Y) {
            chunk.setCell(localX, y, localZ, 0, Cell{BlockType::LogSpruce});
            chunk.setCell(localX, y, localZ, 1, Cell{BlockType::LogSpruce});
        }
    }

    int leafStart = baseY + 3;
    for (int ly = leafStart; ly <= baseY + height + 1; ++ly) {
        if (ly >= CHUNK_SIZE_Y) continue;
        int distFromTop = (baseY + height + 1) - ly;
        int radius = (distFromTop == 0) ? 0 : ((distFromTop % 2 == 0) ? 2 : 1);

        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (radius == 2 && std::abs(dx) == 2 && std::abs(dz) == 2) continue;
                int lx = localX + dx;
                int lz = localZ + dz;
                if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z) {
                    if (dx == 0 && dz == 0 && ly <= baseY + height) continue;
                    if (chunk.getCell(lx, ly, lz, 0).type == BlockType::Air) {
                        chunk.setCell(lx, ly, lz, 0, Cell{BlockType::LeavesSpruce});
                        chunk.setCell(lx, ly, lz, 1, Cell{BlockType::LeavesSpruce});
                    }
                }
            }
        }
    }
}

void TerrainGen::generateAcaciaTree(Chunk& chunk, int localX, int baseY, int localZ) const {
    int height = 5 + (std::hash<int>{}(localX * 12347 ^ localZ * 76543) % 2);
    // Angled acacia trunk
    for (int y = baseY + 1; y <= baseY + height; ++y) {
        int xOff = (y > baseY + 2) ? 1 : 0;
        int lx = localX + xOff;
        if (lx < CHUNK_SIZE_X && y < CHUNK_SIZE_Y) {
            chunk.setCell(lx, y, localZ, 0, Cell{BlockType::LogAcacia});
            chunk.setCell(lx, y, localZ, 1, Cell{BlockType::LogAcacia});
        }
    }

    // Flat wide acacia canopy
    int ly = baseY + height;
    int cx = localX + 1;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            if (std::abs(dx) == 2 && std::abs(dz) == 2) continue;
            int lx = cx + dx;
            int lz = localZ + dz;
            if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z && ly < CHUNK_SIZE_Y) {
                if (chunk.getCell(lx, ly, lz, 0).type == BlockType::Air) {
                    chunk.setCell(lx, ly, lz, 0, Cell{BlockType::LeavesAcacia});
                    chunk.setCell(lx, ly, lz, 1, Cell{BlockType::LeavesAcacia});
                }
            }
        }
    }
}

void TerrainGen::generateCactus(Chunk& chunk, int localX, int baseY, int localZ) const {
    int height = 2 + (std::hash<int>{}(localX * 3331 ^ localZ * 7771) % 2);
    for (int y = baseY + 1; y <= baseY + height; ++y) {
        if (y < CHUNK_SIZE_Y) {
            chunk.setCell(localX, y, localZ, 0, Cell{BlockType::Cactus});
            chunk.setCell(localX, y, localZ, 1, Cell{BlockType::Cactus});
        }
    }
}

// Structure Predicates
bool TerrainGen::isVillageChunk(int cx, int cz, uint32_t seed) {
    int regionX = (cx >= 0) ? (cx / 22) : ((cx - 21) / 22);
    int regionZ = (cz >= 0) ? (cz / 22) : ((cz - 21) / 22);
    uint32_t h = static_cast<uint32_t>(std::hash<int>{}(regionX * 98765431 ^ regionZ * 12345679 ^ seed ^ 0x1111));
    int targetLocalCX = (h % 14) + 4;
    int targetLocalCZ = ((h / 14) % 14) + 4;
    int candCX = regionX * 22 + targetLocalCX;
    int candCZ = regionZ * 22 + targetLocalCZ;
    return (cx == candCX && cz == candCZ);
}

bool TerrainGen::isTempleChunk(int cx, int cz, uint32_t seed) {
    int regionX = (cx >= 0) ? (cx / 26) : ((cx - 25) / 26);
    int regionZ = (cz >= 0) ? (cz / 26) : ((cz - 25) / 26);
    uint32_t h = static_cast<uint32_t>(std::hash<int>{}(regionX * 55555557 ^ regionZ * 33333331 ^ seed ^ 0x2222));
    int targetLocalCX = (h % 16) + 5;
    int targetLocalCZ = ((h / 16) % 16) + 5;
    int candCX = regionX * 26 + targetLocalCX;
    int candCZ = regionZ * 26 + targetLocalCZ;
    return (cx == candCX && cz == candCZ);
}

bool TerrainGen::isOutpostChunk(int cx, int cz, uint32_t seed) {
    int regionX = (cx >= 0) ? (cx / 28) : ((cx - 27) / 28);
    int regionZ = (cz >= 0) ? (cz / 28) : ((cz - 27) / 28);
    uint32_t h = static_cast<uint32_t>(std::hash<int>{}(regionX * 77777771 ^ regionZ * 99999989 ^ seed ^ 0x3333));
    int targetLocalCX = (h % 18) + 5;
    int targetLocalCZ = ((h / 18) % 18) + 5;
    int candCX = regionX * 28 + targetLocalCX;
    int candCZ = regionZ * 28 + targetLocalCZ;
    return (cx == candCX && cz == candCZ);
}

bool TerrainGen::isDungeonChunk(int cx, int cz, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(std::hash<int>{}(cx * 88888881 ^ cz * 44444443 ^ seed ^ 0x4444));
    return (h % 11 == 0);
}

// Structure Generators
void TerrainGen::generateVillage(Chunk& chunk, int chunkX, int chunkZ) const {
    // Village center: well, paths, small furnished house, and a crop farm
    int wx = chunkX * CHUNK_SIZE_X + 8;
    int wz = chunkZ * CHUNK_SIZE_Z + 8;
    int centerY = getHeight(wx, wz);
    if (centerY < 46 || centerY > 65) return;

    // 1. Central Village Well at local (6..9, 6..9)
    for (int x = 6; x <= 9; ++x) {
        for (int z = 6; z <= 9; ++z) {
            bool isBorder = (x == 6 || x == 9 || z == 6 || z == 9);
            // Foundation
            for (int y = centerY - 3; y <= centerY; ++y) {
                chunk.setCell(x, y, z, 0, Cell{isBorder ? BlockType::CobbleStone : BlockType::Water});
                chunk.setCell(x, y, z, 1, Cell{isBorder ? BlockType::CobbleStone : BlockType::Water});
            }
            // Well rim
            if (isBorder) {
                chunk.setCell(x, centerY + 1, z, 0, Cell{BlockType::CobbleStone});
                chunk.setCell(x, centerY + 1, z, 1, Cell{BlockType::CobbleStone});
            } else {
                chunk.setCell(x, centerY + 1, z, 0, Cell{BlockType::Water});
                chunk.setCell(x, centerY + 1, z, 1, Cell{BlockType::Water});
            }
            // Well corner posts & roof
            if ((x == 6 || x == 9) && (z == 6 || z == 9)) {
                for (int py = centerY + 2; py <= centerY + 3; ++py) {
                    chunk.setCell(x, py, z, 0, Cell{BlockType::Wood});
                    chunk.setCell(x, py, z, 1, Cell{BlockType::Wood});
                }
            }
            // Roof
            chunk.setCell(x, centerY + 4, z, 0, Cell{BlockType::Planks});
            chunk.setCell(x, centerY + 4, z, 1, Cell{BlockType::Planks});
        }
    }

    // 2. Village House at local (1..5, 1..5)
    int houseY = getHeight(chunkX * CHUNK_SIZE_X + 3, chunkZ * CHUNK_SIZE_Z + 3);
    if (houseY >= 46 && houseY <= 65) {
        for (int x = 1; x <= 5; ++x) {
            for (int z = 1; z <= 5; ++z) {
                bool isCorner = ((x == 1 || x == 5) && (z == 1 || z == 5));
                bool isWall = (x == 1 || x == 5 || z == 1 || z == 5);

                // Cobblestone floor
                chunk.setCell(x, houseY, z, 0, Cell{BlockType::CobbleStone});
                chunk.setCell(x, houseY, z, 1, Cell{BlockType::CobbleStone});

                // Walls & Corner Posts
                for (int y = houseY + 1; y <= houseY + 3; ++y) {
                    if (isCorner) {
                        chunk.setCell(x, y, z, 0, Cell{BlockType::Wood});
                        chunk.setCell(x, y, z, 1, Cell{BlockType::Wood});
                    } else if (isWall) {
                        // Door at (3, houseY+1..2, 5)
                        if (x == 3 && z == 5 && y <= houseY + 2) {
                            chunk.setCell(x, y, z, 0, Cell{BlockType::Air});
                            chunk.setCell(x, y, z, 1, Cell{BlockType::Air});
                        }
                        // Window at (1, houseY+2, 3) and (5, houseY+2, 3)
                        else if ((x == 1 || x == 5) && z == 3 && y == houseY + 2) {
                            chunk.setCell(x, y, z, 0, Cell{BlockType::Glass});
                            chunk.setCell(x, y, z, 1, Cell{BlockType::Glass});
                        } else {
                            chunk.setCell(x, y, z, 0, Cell{BlockType::Planks});
                            chunk.setCell(x, y, z, 1, Cell{BlockType::Planks});
                        }
                    } else {
                        // Interior Air
                        chunk.setCell(x, y, z, 0, Cell{BlockType::Air});
                        chunk.setCell(x, y, z, 1, Cell{BlockType::Air});
                    }
                }

                // Plank Roof
                chunk.setCell(x, houseY + 4, z, 0, Cell{BlockType::Planks});
                chunk.setCell(x, houseY + 4, z, 1, Cell{BlockType::Planks});
            }
        }
        // Furniture inside house
        chunk.setCell(2, houseY + 1, 2, 0, Cell{BlockType::Bed});
        chunk.setCell(2, houseY + 1, 2, 1, Cell{BlockType::Bed});
        chunk.setCell(4, houseY + 1, 2, 0, Cell{BlockType::CraftingTable});
        chunk.setCell(4, houseY + 1, 2, 1, Cell{BlockType::CraftingTable});
        chunk.setCell(3, houseY + 3, 2, 0, Cell{BlockType::Torch}); // Wall torch
    }

    // 3. Crop Farm at local (10..14, 1..5)
    int farmY = getHeight(chunkX * CHUNK_SIZE_X + 12, chunkZ * CHUNK_SIZE_Z + 3);
    if (farmY >= 46 && farmY <= 65) {
        for (int x = 10; x <= 14; ++x) {
            for (int z = 1; z <= 5; ++z) {
                if (z == 3) {
                    // Central water canal
                    chunk.setCell(x, farmY, z, 0, Cell{BlockType::Water});
                    chunk.setCell(x, farmY, z, 1, Cell{BlockType::Water});
                } else {
                    // Farmland / Crops
                    chunk.setCell(x, farmY, z, 0, Cell{BlockType::Dirt});
                    chunk.setCell(x, farmY, z, 1, Cell{BlockType::Dirt});
                    // Crop plant on top
                    BlockType crop = ((x + z) % 3 == 0) ? BlockType::Melon : (((x + z) % 3 == 1) ? BlockType::Pumpkin : BlockType::TallGrass);
                    chunk.setCell(x, farmY + 1, z, 0, Cell{crop});
                    chunk.setCell(x, farmY + 1, z, 1, Cell{crop});
                }
            }
        }
    }

    // 4. Village Lamp Post at local (5, 11)
    int lampY = getHeight(chunkX * CHUNK_SIZE_X + 5, chunkZ * CHUNK_SIZE_Z + 11);
    if (lampY >= 46 && lampY <= 65) {
        chunk.setCell(5, lampY + 1, 11, 0, Cell{BlockType::CobbleStone});
        chunk.setCell(5, lampY + 1, 11, 1, Cell{BlockType::CobbleStone});
        chunk.setCell(5, lampY + 2, 11, 0, Cell{BlockType::Wood});
        chunk.setCell(5, lampY + 2, 11, 1, Cell{BlockType::Wood});
        chunk.setCell(5, lampY + 3, 11, 0, Cell{BlockType::Wood});
        chunk.setCell(5, lampY + 3, 11, 1, Cell{BlockType::Wood});
        chunk.setCell(5, lampY + 4, 11, 0, Cell{BlockType::Lantern});
        chunk.setCell(5, lampY + 4, 11, 1, Cell{BlockType::Lantern});
    }

    // 5. Village Interconnecting Paths
    for (int i = 0; i < CHUNK_SIZE_X; ++i) {
        int py = getHeight(chunkX * CHUNK_SIZE_X + i, chunkZ * CHUNK_SIZE_Z + 7);
        if (py >= 46 && py <= 65) {
            chunk.setCell(i, py, 7, 0, Cell{BlockType::CoarseDirt});
            chunk.setCell(i, py, 7, 1, Cell{BlockType::CoarseDirt});
        }
    }
}

void TerrainGen::generateDesertTemple(Chunk& chunk, int chunkX, int chunkZ) const {
    int wx = chunkX * CHUNK_SIZE_X + 8;
    int wz = chunkZ * CHUNK_SIZE_Z + 8;
    int baseY = getHeight(wx, wz);
    if (baseY < 46 || baseY > 65) return;

    // Stepped Sandstone Pyramid
    for (int tier = 0; tier < 4; ++tier) {
        int r = 7 - tier;
        int y = baseY + tier;
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                int lx = 8 + dx;
                int lz = 8 + dz;
                if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z && y < CHUNK_SIZE_Y) {
                    BlockType mat = (tier == 0) ? BlockType::Sandstone : BlockType::SmoothSandstone;
                    chunk.setCell(lx, y, lz, 0, Cell{mat});
                    chunk.setCell(lx, y, lz, 1, Cell{mat});
                }
            }
        }
    }

    // Inner chamber & Blue Terracotta seal
    for (int dy = 0; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dz = -2; dz <= 2; ++dz) {
                int lx = 8 + dx;
                int lz = 8 + dz;
                int y = baseY + 1 + dy;
                if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z && y < CHUNK_SIZE_Y) {
                    chunk.setCell(lx, y, lz, 0, Cell{BlockType::Air});
                    chunk.setCell(lx, y, lz, 1, Cell{BlockType::Air});
                }
            }
        }
    }
    // Decorative center emblem
    chunk.setCell(8, baseY + 1, 8, 0, Cell{BlockType::Terracotta});
    chunk.setCell(8, baseY + 1, 8, 1, Cell{BlockType::Terracotta});
}

void TerrainGen::generateWatchtower(Chunk& chunk, int chunkX, int chunkZ) const {
    int wx = chunkX * CHUNK_SIZE_X + 8;
    int wz = chunkZ * CHUNK_SIZE_Z + 8;
    int baseY = getHeight(wx, wz);
    if (baseY < 46 || baseY > 70) return;

    int towerH = 12;
    for (int y = baseY; y <= baseY + towerH; ++y) {
        if (y >= CHUNK_SIZE_Y) break;
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dz = -2; dz <= 2; ++dz) {
                int lx = 8 + dx;
                int lz = 8 + dz;
                if (lx < 0 || lx >= CHUNK_SIZE_X || lz < 0 || lz >= CHUNK_SIZE_Z) continue;

                bool isCorner = (std::abs(dx) == 2 && std::abs(dz) == 2);
                bool isOuter = (std::abs(dx) == 2 || std::abs(dz) == 2);

                if (y == baseY) {
                    chunk.setCell(lx, y, lz, 0, Cell{BlockType::CobbleStone});
                    chunk.setCell(lx, y, lz, 1, Cell{BlockType::CobbleStone});
                } else if (isCorner) {
                    chunk.setCell(lx, y, lz, 0, Cell{BlockType::LogDarkOak});
                    chunk.setCell(lx, y, lz, 1, Cell{BlockType::LogDarkOak});
                } else if (isOuter && (y <= baseY + 4 || y == baseY + towerH - 2)) {
                    chunk.setCell(lx, y, lz, 0, Cell{BlockType::PlanksDarkOak});
                    chunk.setCell(lx, y, lz, 1, Cell{BlockType::PlanksDarkOak});
                } else if (y == baseY + towerH) {
                    chunk.setCell(lx, y, lz, 0, Cell{BlockType::PlanksDarkOak});
                    chunk.setCell(lx, y, lz, 1, Cell{BlockType::PlanksDarkOak});
                } else {
                    chunk.setCell(lx, y, lz, 0, Cell{BlockType::Air});
                    chunk.setCell(lx, y, lz, 1, Cell{BlockType::Air});
                }
            }
        }
    }
    // Balcony chest/barrel
    chunk.setCell(8, baseY + towerH - 1, 8, 0, Cell{BlockType::Barrel});
    chunk.setCell(8, baseY + towerH - 1, 8, 1, Cell{BlockType::Barrel});
}

void TerrainGen::generateDungeon(Chunk& chunk, int chunkX, int chunkZ) const {
    uint32_t h = static_cast<uint32_t>(std::hash<int>{}(chunkX * 13579 ^ chunkZ * 24680 ^ m_seed));
    int dungeonY = 18 + (h % 16); // Y = 18..33 in deep stone layers

    for (int x = 5; x <= 10; ++x) {
        for (int z = 5; z <= 10; ++z) {
            for (int y = dungeonY; y <= dungeonY + 4; ++y) {
                if (y >= CHUNK_SIZE_Y) continue;
                bool isWall = (x == 5 || x == 10 || z == 5 || z == 10 || y == dungeonY || y == dungeonY + 4);
                if (isWall) {
                    BlockType stoneType = ((x * 7 + z * 13 + y * 19) % 3 == 0) ? BlockType::MossyCobble : BlockType::CobbleStone;
                    chunk.setCell(x, y, z, 0, Cell{stoneType});
                    chunk.setCell(x, y, z, 1, Cell{stoneType});
                } else {
                    chunk.setCell(x, y, z, 0, Cell{BlockType::Air});
                    chunk.setCell(x, y, z, 1, Cell{BlockType::Air});
                }
            }
        }
    }
    // Barrels & Torches in dungeon corners
    chunk.setCell(6, dungeonY + 1, 6, 0, Cell{BlockType::Barrel});
    chunk.setCell(6, dungeonY + 1, 6, 1, Cell{BlockType::Barrel});
    chunk.setCell(9, dungeonY + 1, 9, 0, Cell{BlockType::Barrel});
    chunk.setCell(9, dungeonY + 1, 9, 1, Cell{BlockType::Barrel});
    chunk.setCell(6, dungeonY + 2, 8, 0, Cell{BlockType::Torch});
}

// Full Chunk Generation Pipeline
void TerrainGen::generateChunk(Chunk& chunk) const {
    int chunkWorldX = chunk.getWorldX();
    int chunkWorldZ = chunk.getWorldZ();
    int cx = chunk.getCoord().cx;
    int cz = chunk.getCoord().cz;

    int columnHeights[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    BiomeType columnBiomes[CHUNK_SIZE_X][CHUNK_SIZE_Z];

    // PASS 1: Terrain Geometry, Strata, 3D Caves, Bedrock, and Ore Veins
    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            int worldX = chunkWorldX + x;
            int worldZ = chunkWorldZ + z;

            int height = getHeight(worldX, worldZ);
            columnHeights[x][z] = height;
            BiomeType biome = getBiome(worldX, worldZ);
            columnBiomes[x][z] = biome;

            int maxY = std::max(height, 44);

            for (int y = 0; y <= maxY; ++y) {
                BlockType type = BlockType::Air;

                // 1. Bedrock Floor Layering (Y = 0..3)
                if (y == 0) {
                    type = BlockType::Bedrock;
                } else if (y == 1 && ((worldX * 31 ^ worldZ * 17 ^ y * 53) % 4 != 0)) {
                    type = BlockType::Bedrock;
                } else if (y == 2 && ((worldX * 31 ^ worldZ * 17 ^ y * 53) % 2 == 0)) {
                    type = BlockType::Bedrock;
                } else if (y == 3 && ((worldX * 31 ^ worldZ * 17 ^ y * 53) % 4 == 0)) {
                    type = BlockType::Bedrock;
                }
                // 2. Subterranean Strata (Deepslate at Y < 16, Stone above)
                else if (y < height - 4) {
                    bool isDeepslate = (y < 16);
                    type = isDeepslate ? BlockType::Deepslate : BlockType::Stone;

                    // Stone variants
                    float rockNoise = m_detailNoise.GetNoise((float)worldX * 2.0f, (float)y * 2.0f, (float)worldZ * TRI_HEIGHT * 2.0f);
                    if (isDeepslate) {
                        if (rockNoise > 0.65f) type = BlockType::Tuff;
                    } else {
                        if (rockNoise > 0.72f) type = BlockType::Granite;
                        else if (rockNoise < -0.72f) type = BlockType::Diorite;
                        else if (rockNoise > 0.58f && y > 35) type = BlockType::Andesite;
                    }

                    // Depth & Biome Stratified Ores
                    float c = m_caveNoise1.GetNoise((float)worldX, (float)y, (float)worldZ * TRI_HEIGHT);
                    if (y <= 15 && c > 0.84f) {
                        type = isDeepslate ? BlockType::DeepslateDiamond : BlockType::OreDiamond;
                    } else if (y <= 16 && c < -0.83f) {
                        type = isDeepslate ? BlockType::DeepslateRedstone : BlockType::OreRedstone;
                    } else if (y <= 32 && c > 0.81f && c <= 0.84f) {
                        type = isDeepslate ? BlockType::DeepslateGold : BlockType::OreGold;
                    } else if (y >= 12 && y <= 36 && c < -0.80f && c >= -0.83f) {
                        type = BlockType::OreLapis;
                    } else if (y >= 24 && y <= 68 && c > 0.78f && c <= 0.81f) {
                        type = BlockType::OreCopper;
                    } else if (y <= 68 && c < -0.74f) {
                        type = isDeepslate ? BlockType::DeepslateIron : BlockType::OreIron;
                    } else if (y >= 32 && y <= 115 && c > 0.74f) {
                        type = isDeepslate ? BlockType::DeepslateCoal : BlockType::OreCoal;
                    } else if (biome == BiomeType::Mountains && y >= 65 && y <= 115 && c > 0.71f && c <= 0.74f) {
                        // Exclusive Mountain Emeralds!
                        type = BlockType::OreEmerald;
                    }

                    // 3. 3D Cave Carving (Worm tunnels & Cavern rooms)
                    if (y >= 4) {
                        float c1 = m_caveNoise1.GetNoise((float)worldX * 1.5f, (float)y * 2.2f, (float)worldZ * TRI_HEIGHT * 1.5f);
                        float c2 = m_caveNoise2.GetNoise((float)worldX * 1.5f, (float)y * 2.2f, (float)worldZ * TRI_HEIGHT * 1.5f);
                        float worm = c1 * c1 + c2 * c2;
                        float room = m_caveNoise3.GetNoise((float)worldX, (float)y * 1.5f, (float)worldZ * TRI_HEIGHT);

                        bool isCave = (worm < 0.0075f) || (room > 0.65f && y < 55);

                        // Protect water floor beds
                        if (isCave && !(height <= 45 && y >= height - 6)) {
                            if (y <= 10) {
                                type = BlockType::Water; // Deep underground lava/water pools
                            } else {
                                type = BlockType::Air; // Hollowed out cave tunnel
                            }
                        }
                    }
                }
                // 4. Subsurface Layer (Dirt / Sandstone)
                else if (y < height) {
                    type = getBiomeSubsurfaceBlock(biome);
                }
                // 5. Surface Block (Grass, Sand, Snow, Stone)
                else if (y == height) {
                    if (height <= 44) {
                        type = BlockType::Sand; // Shoreline
                    } else {
                        type = getBiomeSurfaceBlock(biome, height);
                    }
                }
                // 6. Water Fill (Sea level at Y = 44)
                else if (y <= 44 && y > height) {
                    type = BlockType::Water;
                }

                chunk.setCell(x, y, z, 0, Cell{type});
                chunk.setCell(x, y, z, 1, Cell{type});
            }
        }
    }

    // PASS 2: Foliage, Trees, and Biome Details
    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            int worldX = chunkWorldX + x;
            int worldZ = chunkWorldZ + z;
            int height = columnHeights[x][z];
            BiomeType biome = columnBiomes[x][z];

            if (height > 44 && height + 8 < CHUNK_SIZE_Y) {
                uint32_t h = static_cast<uint32_t>(std::hash<int>{}(worldX * 73856093 ^ worldZ * 19349663 ^ m_seed));

                // Tree placement based on biome
                if (biome == BiomeType::Desert) {
                    if (h % 90 == 0) {
                        generateCactus(chunk, x, height, z);
                    }
                } else if (biome == BiomeType::Savanna) {
                    if (h % 110 == 0) {
                        generateAcaciaTree(chunk, x, height, z);
                    }
                } else if (biome == BiomeType::Taiga) {
                    if (h % 45 == 0) {
                        generateSpruceTree(chunk, x, height, z);
                    }
                } else if (biome == BiomeType::Forest) {
                    if (h % 40 == 0) {
                        generateOakTree(chunk, x, height, z);
                    }
                } else if (biome == BiomeType::BirchForest) {
                    if (h % 45 == 0) {
                        generateBirchTree(chunk, x, height, z);
                    }
                } else if (biome == BiomeType::Plains) {
                    if (h % 130 == 0) {
                        generateOakTree(chunk, x, height, z);
                    }
                } else if (biome == BiomeType::Mountains) {
                    if (height < 85 && h % 95 == 0) {
                        generateSpruceTree(chunk, x, height, z);
                    }
                }

                // Surface wildflowers & tall grass
                if (biome != BiomeType::Desert && chunk.getCell(x, height, z, 0).type == BlockType::Grass) {
                    uint32_t f0 = static_cast<uint32_t>(std::hash<int>{}(worldX * 81237 ^ worldZ * 91238 ^ 101 ^ m_seed));
                    uint32_t roll = f0 % 100;
                    BlockType fol = BlockType::Air;
                    if (roll < 14) fol = BlockType::TallGrass;
                    else if (roll < 17) fol = BlockType::FlowerRose;
                    else if (roll < 20) fol = BlockType::FlowerDandelion;

                    if (fol != BlockType::Air && chunk.getCell(x, height + 1, z, 0).type == BlockType::Air) {
                        chunk.setCell(x, height + 1, z, 0, Cell{fol});
                        chunk.setCell(x, height + 1, z, 1, Cell{fol});
                    }
                }
            }
        }
    }

    // PASS 3: Procedural Structures (Villages, Temples, Outposts, Dungeons)
    BiomeType centerBiome = getBiome(chunkWorldX + 8, chunkWorldZ + 8);
    if (isVillageChunk(cx, cz, m_seed) && (centerBiome == BiomeType::Plains || centerBiome == BiomeType::Savanna || centerBiome == BiomeType::Desert)) {
        generateVillage(chunk, cx, cz);
    } else if (isTempleChunk(cx, cz, m_seed) && centerBiome == BiomeType::Desert) {
        generateDesertTemple(chunk, cx, cz);
    } else if (isOutpostChunk(cx, cz, m_seed) && (centerBiome == BiomeType::Plains || centerBiome == BiomeType::Savanna)) {
        generateWatchtower(chunk, cx, cz);
    } else if (isDungeonChunk(cx, cz, m_seed)) {
        generateDungeon(chunk, cx, cz);
    }
}

// Locating Functions for /locate command
bool TerrainGen::locateStructure(const std::string& type, int playerX, int playerZ, int& outX, int& outY, int& outZ) const {
    std::string q = type;
    std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

    int startCX = (playerX >= 0) ? (playerX / CHUNK_SIZE_X) : ((playerX - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X);
    int startCZ = (playerZ >= 0) ? (playerZ / CHUNK_SIZE_Z) : ((playerZ - CHUNK_SIZE_Z + 1) / CHUNK_SIZE_Z);

    int bestDistSq = INT_MAX;
    bool found = false;

    // Search outward up to radius 80 chunks (~1280 blocks)
    for (int r = 0; r <= 80; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::abs(dx) != r && std::abs(dz) != r) continue; // Ring perimeter only
                int cx = startCX + dx;
                int cz = startCZ + dz;

                bool matches = false;
                BiomeType b = getBiome(cx * CHUNK_SIZE_X + 8, cz * CHUNK_SIZE_Z + 8);

                if (q == "village") {
                    matches = isVillageChunk(cx, cz, m_seed) && (b == BiomeType::Plains || b == BiomeType::Savanna || b == BiomeType::Desert);
                } else if (q == "temple" || q == "desert_temple") {
                    matches = isTempleChunk(cx, cz, m_seed) && (b == BiomeType::Desert);
                } else if (q == "outpost" || q == "pillager_outpost") {
                    matches = isOutpostChunk(cx, cz, m_seed) && (b == BiomeType::Plains || b == BiomeType::Savanna);
                } else if (q == "dungeon") {
                    matches = isDungeonChunk(cx, cz, m_seed);
                }

                if (matches) {
                    int candX = cx * CHUNK_SIZE_X + 8;
                    int candZ = cz * CHUNK_SIZE_Z + 8;
                    int distSq = (candX - playerX) * (candX - playerX) + (candZ - playerZ) * (candZ - playerZ);
                    if (distSq < bestDistSq) {
                        bestDistSq = distSq;
                        outX = candX;
                        outZ = candZ;
                        outY = (q == "dungeon") ? 25 : getHeight(candX, candZ);
                        found = true;
                    }
                }
            }
        }
        if (found) return true;
    }

    return false;
}

bool TerrainGen::locateBiome(BiomeType biome, int playerX, int playerZ, int& outX, int& outZ) const {
    // Spiral outward in 16-block steps up to radius 1500 blocks
    for (int r = 1; r <= 100; ++r) {
        int step = r * 16;
        for (int dx = -step; dx <= step; dx += 16) {
            for (int dz = -step; dz <= step; dz += 16) {
                if (std::abs(dx) != step && std::abs(dz) != step) continue;
                int candX = playerX + dx;
                int candZ = playerZ + dz;
                if (getBiome(candX, candZ) == biome) {
                    outX = candX;
                    outZ = candZ;
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace prismcraft
