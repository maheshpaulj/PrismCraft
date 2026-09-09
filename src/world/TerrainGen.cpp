#include "TerrainGen.hpp"
#include "Chunk.hpp"
#include "Cell.hpp"
#include "Coordinates.hpp"
#include <FastNoiseLite.h>
#include <functional>

namespace prismcraft {

TerrainGen::TerrainGen(uint32_t seed) : m_seed(seed) {
    m_continentalNoise.SetSeed(m_seed);
    m_continentalNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_continentalNoise.SetFrequency(0.0035f); // Low-frequency continental scale

    m_detailNoise.SetSeed(m_seed + 101);
    m_detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_detailNoise.SetFrequency(0.015f);

    m_caveNoise.SetSeed(m_seed + 2);
    m_caveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_caveNoise.SetFrequency(0.05f);
}

int TerrainGen::getHeight(int worldX, int worldZ) const {
    float cont = m_continentalNoise.GetNoise((float)worldX, (float)worldZ * TRI_HEIGHT);
    float det = m_detailNoise.GetNoise((float)worldX, (float)worldZ * TRI_HEIGHT);

    float h = 52.0f;
    if (cont < -0.30f) {
        // Ocean / Lake / River channel
        float t = (-cont - 0.30f) / 0.70f;
        h = 42.0f - t * 14.0f + det * 2.0f;
    } else if (cont < -0.15f) {
        // Smooth sand beach sloping into water
        float t = (cont + 0.30f) / 0.15f;
        h = 43.0f + t * 7.0f + det * 1.5f;
    } else if (cont <= 0.45f) {
        // Vast Open Plains: Flat, spacious building space (height 50..53)
        float t = (cont + 0.15f) / 0.60f;
        h = 50.5f + t * 3.0f + det * 2.0f;
    } else if (cont <= 0.75f) {
        // Rolling Hills: Gentle elevation rise
        float t = (cont - 0.45f) / 0.30f;
        h = 54.0f + t * 16.0f + det * 4.0f;
    } else {
        // Majestic Distant Mountain Peaks
        float t = (cont - 0.75f) / 0.25f;
        h = 70.0f + t * 24.0f + det * 6.0f;
    }
    return std::clamp(static_cast<int>(std::round(h)), 10, CHUNK_SIZE_Y - 12);
}

void TerrainGen::generateTree(Chunk& chunk, int localX, int baseY, int localZ) const {
    // 5-6 blocks wood trunk
    int height = 5 + (std::hash<int>{}(localX * 73856093 ^ localZ * 19349663) % 2);
    for (int y = baseY + 1; y <= baseY + height; ++y) {
        if (y < CHUNK_SIZE_Y) {
            chunk.setCell(localX, y, localZ, 0, Cell{BlockType::Wood});
            chunk.setCell(localX, y, localZ, 1, Cell{BlockType::Wood});
        }
    }
    
    // Authentic Minecraft Oak Tree Leaf Canopy:
    // Layers leafBase to leafBase + 1: 5x5 leaves with corner cutouts
    int leafBase = baseY + height - 2;
    for (int ly = leafBase; ly <= leafBase + 1; ++ly) {
        if (ly >= CHUNK_SIZE_Y) continue;
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dz = -2; dz <= 2; ++dz) {
                // Cut corners for natural rounded canopy
                if (std::abs(dx) == 2 && std::abs(dz) == 2) {
                    uint32_t cornerHash = static_cast<uint32_t>(std::hash<int>{}(dx * 31 ^ dz * 17 ^ ly * 53));
                    if (cornerHash % 2 == 0) continue;
                }
                int lx = localX + dx;
                int lz = localZ + dz;
                if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z) {
                    if (dx == 0 && dz == 0 && ly <= baseY + height) continue; // Trunk
                    if (chunk.getCell(lx, ly, lz, 0).type == BlockType::Air) {
                        chunk.setCell(lx, ly, lz, 0, Cell{BlockType::Leaves});
                        chunk.setCell(lx, ly, lz, 1, Cell{BlockType::Leaves});
                    }
                }
            }
        }
    }

    // Layer leafBase + 2: 3x3 leaves
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

    // Layer leafBase + 3: Plus shape (cross) top cap
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

void TerrainGen::generateChunk(Chunk& chunk) const {
    int chunkWorldX = chunk.getWorldX();
    int chunkWorldZ = chunk.getWorldZ();

    int columnHeights[CHUNK_SIZE_X][CHUNK_SIZE_Z];

    // PASS 1: Base Terrain Generation (Bedrock, Stone, Dirt, Grass, Sand, Water)
    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            int worldX = chunkWorldX + x;
            int worldZ = chunkWorldZ + z;
            
            int height = getHeight(worldX, worldZ);
            columnHeights[x][z] = height;
            int maxY = std::max(height, 44);

            for (int y = 0; y <= maxY; ++y) {
                BlockType type = BlockType::Air;
                
                if (y == 0) {
                    type = BlockType::Bedrock;
                } else if (y < height - 4) {
                    type = BlockType::Stone;
                    // Cave / Ore noise
                    float c = m_caveNoise.GetNoise((float)worldX, (float)y, (float)worldZ * TRI_HEIGHT);
                    if (c > 0.8f) type = BlockType::OreCoal;
                    else if (c < -0.8f) type = BlockType::OreIron;
                } else if (y < height) {
                    type = BlockType::Dirt;
                } else if (y == height) {
                    if (height <= 45) {
                        type = BlockType::Sand; // Wide sandy beach around water
                    } else if (height > 84) {
                        type = BlockType::Snow; // High mountain snow peaks
                    } else if (height > 76) {
                        type = BlockType::Stone; // Mountain rock face
                    } else {
                        type = BlockType::Grass; // Vast verdant grassy plains & hills
                    }
                } else if (y <= 44 && y > height) {
                    type = BlockType::Water; // Water surface at Y=44
                }

                chunk.setCell(x, y, z, 0, Cell{type});
                chunk.setCell(x, y, z, 1, Cell{type});
            }
        }
    }

    // PASS 2: Features & Foliage Generation (Spacious Clearings, Trees and Wildflowers)
    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            int worldX = chunkWorldX + x;
            int worldZ = chunkWorldZ + z;
            int height = columnHeights[x][z];

            if (height > 45 && height <= 76 && height + 8 < CHUNK_SIZE_Y) {
                // Trees placed with spacious clearings so players have vast building room
                uint32_t h = static_cast<uint32_t>(std::hash<int>{}(worldX * 73856093 ^ worldZ * 19349663 ^ m_seed));
                int treeModulo = (height > 54) ? 75 : 120; // More open plains (1 in 120), groves in hills
                if (h % treeModulo == 0) {
                    generateTree(chunk, x, height, z);
                    continue; // No flowers directly under trunk
                }

                // Scatter wild tall grass and flowers on grass surface
                uint32_t f0 = static_cast<uint32_t>(std::hash<int>{}(worldX * 81237 ^ worldZ * 91238 ^ 101 ^ m_seed));
                uint32_t f1 = static_cast<uint32_t>(std::hash<int>{}(worldX * 81237 ^ worldZ * 91238 ^ 202 ^ m_seed));
                
                auto pickFoliage = [](uint32_t val) -> BlockType {
                    uint32_t roll = val % 100;
                    if (roll < 14) return BlockType::TallGrass;
                    if (roll < 17) return BlockType::FlowerRose;
                    if (roll < 20) return BlockType::FlowerDandelion;
                    return BlockType::Air;
                };

                BlockType fol0 = pickFoliage(f0);
                BlockType fol1 = pickFoliage(f1);
                if (fol0 != BlockType::Air && chunk.getCell(x, height + 1, z, 0).type == BlockType::Air) {
                    chunk.setCell(x, height + 1, z, 0, Cell{fol0});
                }
                if (fol1 != BlockType::Air && chunk.getCell(x, height + 1, z, 1).type == BlockType::Air) {
                    chunk.setCell(x, height + 1, z, 1, Cell{fol1});
                }
            }
        }
    }
}

} // namespace prismcraft
