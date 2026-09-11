#pragma once
#include "world/Chunk.hpp"
#include "world/ChunkMesher.hpp"
#include "world/TerrainGen.hpp"
#include "world/Coordinates.hpp"
#include "world/WaterSimulator.hpp"
#include "rhi/Buffer.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>

#include "core/ThreadPool.hpp"
#include <shared_mutex>
#include <unordered_set>
#include <deque>

namespace prismcraft {

class VulkanContext;
class CommandQueue;

struct ChunkRenderData {
    Buffer opaqueVertexBuffer;
    Buffer opaqueIndexBuffer;
    uint32_t opaqueIndexCount = 0;
    Buffer waterVertexBuffer;
    Buffer waterIndexBuffer;
    uint32_t waterIndexCount = 0;
    ChunkCoord coord;
    LODLevel lod = LODLevel::LOD0_Full;
};

class FallingBlockManager;

struct StagedMeshResult {
    ChunkCoord coord;
    ChunkMesh mesh;
    LODLevel lod;
    bool valid = true;
};

class World {
public:
    World(VulkanContext& context, CommandQueue& cmdQueue, uint32_t seed = 12345);
    ~World();

    // Call each frame with the player's position
    void update(const glm::vec3& playerPos, float dt = 0.016f);
    
    // Get all renderable chunk data
    [[nodiscard]] const std::unordered_map<ChunkCoord, ChunkRenderData, ChunkCoordHash>& getMeshes() const { return m_meshes; }
    
    // Get a cell at world coordinates
    [[nodiscard]] Cell getCell(int worldX, int y, int worldZ, int s) const;
    void setCell(int worldX, int y, int worldZ, int s, Cell cell, bool notifyFluid = true);
    void setCellInstant(int worldX, int y, int worldZ, int s, Cell cell);

    // Water fluid simulation
    [[nodiscard]] WaterSimulator& getWaterSimulator() { return m_waterSimulator; }
    
    // Physics / Gravity simulation for falling blocks (Sand, Gravel)
    void checkGravity(int worldX, int y, int worldZ, int s, FallingBlockManager* fallingBlocks = nullptr);
    
    // Find highest solid ground level for safe spawn
    [[nodiscard]] float getHighestSolidY(float worldX, float worldZ) const;

    // Frustum culling check for chunk column
    [[nodiscard]] static bool isChunkInFrustum(const ChunkCoord& coord, const struct Frustum& frustum);

    // Call during loading screen to pre-warm and pre-mesh spawn chunks
    // Returns true when all target spawn chunks are uploaded to GPU
    bool updateLoading(const glm::vec3& spawnPos, int targetRadius, int& loadedCount, int& totalCount);

    // Light level calculation (0..15)
    void getLightLevels(int worldX, int y, int worldZ, int s, int& skyLight, int& blockLight) const;

    // Nearest placed torches for dynamic point light and shadow casting
    [[nodiscard]] std::vector<glm::vec3> getNearestPlacedTorches(const glm::vec3& refPos, size_t maxCount = 4, float maxDist = 24.0f) const;
    [[nodiscard]] std::optional<glm::vec3> getNearestPlacedTorch(const glm::vec3& refPos, float maxDist = 12.0f) const;
    
    int renderDistance = 8; // in chunks
    int lodPreset = 1;      // 0: Performance, 1: Balanced, 2: Quality, 3: Custom
    
private:
    void queueChunksAround(const ChunkCoord& center);
    void processStagedUploads(size_t maxUploads = 12);
    void uploadMeshesBatched(std::vector<StagedMeshResult>& items);
    void unloadDistantChunks(const ChunkCoord& center);
    void meshChunk(const ChunkCoord& coord, LODLevel lod);
    void uploadMesh(const ChunkCoord& coord, const ChunkMesh& mesh, LODLevel lod);
    Chunk* getChunk(const ChunkCoord& coord);
    const Chunk* getChunk(const ChunkCoord& coord) const;
    [[nodiscard]] LODLevel calculateTargetLOD(int distChunks, std::optional<LODLevel> currentLOD = std::nullopt) const;
    
    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;
    TerrainGen m_terrainGen;
    
    mutable std::shared_mutex m_chunksMutex;
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> m_chunks;
    std::unordered_map<ChunkCoord, ChunkRenderData, ChunkCoordHash> m_meshes;
    std::unordered_map<ChunkCoord, LODLevel, ChunkCoordHash> m_chunkLODs;

    std::mutex m_queueMutex;
    std::unordered_set<ChunkCoord, ChunkCoordHash> m_pendingTasks;
    std::deque<StagedMeshResult> m_stagedMeshes;
    
    ChunkCoord m_lastPlayerChunk{INT_MAX, INT_MAX};
    int m_lastRenderDistance = 0;

    mutable std::mutex m_torchesMutex;
    std::vector<glm::vec3> m_placedTorches;

    ThreadPool m_threadPool;
    WaterSimulator m_waterSimulator;
};

} // namespace prismcraft
