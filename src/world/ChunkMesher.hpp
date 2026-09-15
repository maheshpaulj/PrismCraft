#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace prismcraft {

class Chunk;
class TerrainGen;
struct ChunkCoord;
enum class BlockType : uint8_t;

enum class LODLevel : uint8_t {
    LOD0_Full = 0,     // 16x16: full per-block detail, caves, foliage, AO (0 - 8/16 chunks)
    LOD1_Medium = 1,   // 8x8: 2x2 merged columns (step = 2)
    LOD2_Coarse = 2,   // 4x4: 4x4 merged columns (step = 4)
    LOD3_Imposter = 3, // 2x2: 8x8 merged columns (step = 8)
    LOD4_Extreme = 4   // 1x1: 16x16 merged macro-voxel (step = 16, extreme horizons up to 256 chunks)
};

// Textured, lit vertex for world and model rendering
struct ChunkVertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec3 normal;
    glm::vec3 color;
};

struct ChunkMesh {
    std::vector<ChunkVertex> opaqueVertices;
    std::vector<uint32_t> opaqueIndices;
    std::vector<ChunkVertex> waterVertices;
    std::vector<uint32_t> waterIndices;
    bool empty() const { return opaqueIndices.empty() && waterIndices.empty(); }
};

class ChunkMesher {
public:
    static ChunkMesh generateMesh(const Chunk& chunk,
        const Chunk* northNeighbor = nullptr,  // z-1
        const Chunk* southNeighbor = nullptr,  // z+1
        const Chunk* westNeighbor = nullptr,   // x-1
        const Chunk* eastNeighbor = nullptr,   // x+1
        const class World* world = nullptr);

    static ChunkMesh generateLODMesh(const Chunk& chunk, LODLevel lod);

    static ChunkMesh generateImposterMesh(const ChunkCoord& coord, const TerrainGen& terrainGen, int step = 8);
};

} // namespace prismcraft
