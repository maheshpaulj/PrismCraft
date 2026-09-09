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
    LOD0_Full = 0,     // 0 - 16 chunks: full per-block detail, caves, foliage, AO
    LOD1_Medium = 1,   // 16 - 48 chunks: 2x2 merged columns, surface shell + boundary skirts
    LOD2_Coarse = 2,   // 48 - 96 chunks: 4x4 merged columns, coarse surface shell + boundary skirts
    LOD3_Imposter = 3  // 96 - 256 chunks: procedural heightmap imposter
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

    static ChunkMesh generateImposterMesh(const ChunkCoord& coord, const TerrainGen& terrainGen);
};

} // namespace prismcraft
