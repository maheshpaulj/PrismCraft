#include "world/World.hpp"
#include "world/FallingBlockManager.hpp"
#include "player/Camera.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include <cmath>
#include <algorithm>

namespace prismcraft {

World::World(VulkanContext& context, CommandQueue& cmdQueue, uint32_t seed)
    : m_context(context)
    , m_cmdQueue(cmdQueue)
    , m_terrainGen(seed)
{
    // Synchronously generate initial spawn area (3x3 chunks) so basic height queries work
    for (int cx = -1; cx <= 1; ++cx) {
        for (int cz = -1; cz <= 1; ++cz) {
            ChunkCoord coord{cx, cz};
            auto chunk = std::make_unique<Chunk>(coord);
            m_terrainGen.generateChunk(*chunk);
            m_chunks[coord] = std::move(chunk);
        }
    }
}

World::~World() {
    m_threadPool.shutdown();
}

void World::update(const glm::vec3& playerPos) {
    CellCoord playerCell = worldToCell(playerPos);
    ChunkCoord currentChunk = worldToChunk(playerCell.x, playerCell.z);

    // 1. Process bounded GPU staging uploads (up to 12 per frame in a single GPU submission)
    processStagedUploads(12);

    // 2. Queue chunk generation/meshing if player moved chunks or renderDistance changed or queue low
    bool playerMoved = (currentChunk.cx != m_lastPlayerChunk.cx || currentChunk.cz != m_lastPlayerChunk.cz);
    bool rdChanged = (renderDistance != m_lastRenderDistance);

    bool queueLow = false;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        queueLow = (m_pendingTasks.size() < 16);
    }

    if (playerMoved || rdChanged || queueLow) {
        m_lastPlayerChunk = currentChunk;
        m_lastRenderDistance = renderDistance;
        queueChunksAround(currentChunk);
    }

    // 3. Unload distant chunks periodically or when moving
    if (playerMoved || rdChanged) {
        unloadDistantChunks(currentChunk);
    }

    // 4. Remesh dirty chunks within close radius immediately for responsive mining/building
    std::vector<ChunkCoord> dirtyToMesh;
    {
        std::shared_lock<std::shared_mutex> lock(m_chunksMutex);
        for (const auto& [coord, chunk] : m_chunks) {
            if (chunk->isDirty()) {
                int dist = std::max(std::abs(coord.cx - currentChunk.cx), std::abs(coord.cz - currentChunk.cz));
                if (dist <= 4) {
                    dirtyToMesh.push_back(coord);
                }
            }
        }
    }

    for (const auto& coord : dirtyToMesh) {
        meshChunk(coord, LODLevel::LOD0_Full);
    }
}

LODLevel World::calculateTargetLOD(int distChunks, std::optional<LODLevel> currentLOD) const {
    int t0 = 16, t1 = 48, t2 = 96;
    if (lodPreset == 0) { // Performance: aggressive LOD
        t0 = 8; t1 = 24; t2 = 64;
    } else if (lodPreset == 2) { // Quality
        t0 = 24; t1 = 64; t2 = 128;
    } else if (lodPreset == 3) { // Custom / Ultra
        t0 = 32; t1 = 80; t2 = 160;
    }

    // If no existing mesh, assign LOD tier directly without hysteresis offsets
    if (!currentLOD.has_value()) {
        if (distChunks <= t0) return LODLevel::LOD0_Full;
        if (distChunks <= t1) return LODLevel::LOD1_Medium;
        if (distChunks <= t2) return LODLevel::LOD2_Coarse;
        return LODLevel::LOD3_Imposter;
    }

    // Apply Hysteresis: prevent flickering between LOD tiers for existing meshes
    LODLevel cur = currentLOD.value();
    if (cur == LODLevel::LOD0_Full) {
        if (distChunks > t0 + 2) {
            return (distChunks <= t1) ? LODLevel::LOD1_Medium : ((distChunks <= t2) ? LODLevel::LOD2_Coarse : LODLevel::LOD3_Imposter);
        }
        return LODLevel::LOD0_Full;
    } else if (cur == LODLevel::LOD1_Medium) {
        if (distChunks <= t0 - 2) return LODLevel::LOD0_Full;
        if (distChunks > t1 + 3) {
            return (distChunks <= t2) ? LODLevel::LOD2_Coarse : LODLevel::LOD3_Imposter;
        }
        return LODLevel::LOD1_Medium;
    } else if (cur == LODLevel::LOD2_Coarse) {
        if (distChunks <= t1 - 3) return LODLevel::LOD1_Medium;
        if (distChunks > t2 + 4) return LODLevel::LOD3_Imposter;
        return LODLevel::LOD2_Coarse;
    } else { // LOD3_Imposter
        if (distChunks <= t2 - 4) return LODLevel::LOD2_Coarse;
        return LODLevel::LOD3_Imposter;
    }
}

void World::queueChunksAround(const ChunkCoord& center) {
    size_t enqueuedThisCall = 0;
    const size_t maxEnqueuePerFrame = 64;

    for (int r = 0; r <= renderDistance; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;

                ChunkCoord coord{center.cx + dx, center.cz + dz};
                int dist = r;

                LODLevel curLOD = LODLevel::LOD0_Full;
                bool hasMesh = false;
                auto itMesh = m_meshes.find(coord);
                if (itMesh != m_meshes.end()) {
                    hasMesh = true;
                    curLOD = itMesh->second.lod;
                }

                LODLevel targetLOD = calculateTargetLOD(dist, hasMesh ? std::optional<LODLevel>(curLOD) : std::nullopt);
                if (hasMesh && curLOD == targetLOD) {
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    if (m_pendingTasks.find(coord) != m_pendingTasks.end()) {
                        continue;
                    }
                    m_pendingTasks.insert(coord);
                }

                if (targetLOD == LODLevel::LOD3_Imposter) {
                    m_threadPool.enqueueTask([this, coord, targetLOD]() {
                        ChunkMesh mesh = ChunkMesher::generateImposterMesh(coord, this->m_terrainGen);
                        {
                            std::lock_guard<std::mutex> lock(this->m_queueMutex);
                            this->m_stagedMeshes.push_back({coord, std::move(mesh), targetLOD, true});
                        }
                    });
                } else if (dist > 32) {
                    m_threadPool.enqueueTask([this, coord, targetLOD]() {
                        Chunk chunk(coord);
                        this->m_terrainGen.generateChunk(chunk);
                        ChunkMesh mesh = ChunkMesher::generateLODMesh(chunk, targetLOD);
                        {
                            std::lock_guard<std::mutex> lock(this->m_queueMutex);
                            this->m_stagedMeshes.push_back({coord, std::move(mesh), targetLOD, true});
                        }
                    });
                } else {
                    m_threadPool.enqueueTask([this, coord, targetLOD]() {
                        // 1. Check which chunks need generation under a fast shared lock
                        bool needCoord = false;
                        std::vector<ChunkCoord> missingNeighbors;
                        {
                            std::shared_lock<std::shared_mutex> rlock(this->m_chunksMutex);
                            if (this->m_chunks.find(coord) == this->m_chunks.end()) {
                                needCoord = true;
                            }
                            if (targetLOD == LODLevel::LOD0_Full) {
                                ChunkCoord neighbors[4] = {
                                    {coord.cx, coord.cz - 1}, {coord.cx, coord.cz + 1},
                                    {coord.cx - 1, coord.cz}, {coord.cx + 1, coord.cz}
                                };
                                for (const auto& nc : neighbors) {
                                    if (this->m_chunks.find(nc) == this->m_chunks.end()) {
                                        missingNeighbors.push_back(nc);
                                    }
                                }
                            }
                        }

                        // 2. Generate missing chunks in parallel OUTSIDE any lock
                        std::unique_ptr<Chunk> newCoordChunk;
                        if (needCoord) {
                            newCoordChunk = std::make_unique<Chunk>(coord);
                            this->m_terrainGen.generateChunk(*newCoordChunk);
                        }

                        std::vector<std::pair<ChunkCoord, std::unique_ptr<Chunk>>> newNeighbors;
                        for (const auto& nc : missingNeighbors) {
                            auto nChunk = std::make_unique<Chunk>(nc);
                            this->m_terrainGen.generateChunk(*nChunk);
                            newNeighbors.emplace_back(nc, std::move(nChunk));
                        }

                        // 3. Insert generated chunks under a brief exclusive write lock
                        if (newCoordChunk || !newNeighbors.empty()) {
                            std::unique_lock<std::shared_mutex> wlock(this->m_chunksMutex);
                            if (newCoordChunk && this->m_chunks.find(coord) == this->m_chunks.end()) {
                                this->m_chunks[coord] = std::move(newCoordChunk);
                            }
                            for (auto& pair : newNeighbors) {
                                if (this->m_chunks.find(pair.first) == this->m_chunks.end()) {
                                    this->m_chunks[pair.first] = std::move(pair.second);
                                }
                            }
                        }

                        const Chunk* chunk = nullptr;
                        const Chunk* north = nullptr;
                        const Chunk* south = nullptr;
                        const Chunk* west = nullptr;
                        const Chunk* east = nullptr;
                        {
                            std::shared_lock<std::shared_mutex> lock(this->m_chunksMutex);
                            chunk = this->getChunk(coord);
                            if (targetLOD == LODLevel::LOD0_Full) {
                                north = this->getChunk({coord.cx, coord.cz - 1});
                                south = this->getChunk({coord.cx, coord.cz + 1});
                                west = this->getChunk({coord.cx - 1, coord.cz});
                                east = this->getChunk({coord.cx + 1, coord.cz});
                            }
                        }

                        ChunkMesh mesh;
                        bool valid = false;
                        if (targetLOD == LODLevel::LOD0_Full) {
                            if (chunk && north && south && west && east) {
                                mesh = ChunkMesher::generateMesh(*chunk, north, south, west, east, this);
                                valid = true;
                            }
                        } else {
                            if (chunk) {
                                mesh = ChunkMesher::generateLODMesh(*chunk, targetLOD);
                                valid = true;
                            }
                        }

                        {
                            std::lock_guard<std::mutex> lock(this->m_queueMutex);
                            this->m_stagedMeshes.push_back({coord, std::move(mesh), targetLOD, valid});
                        }
                    });
                }

                enqueuedThisCall++;
                if (enqueuedThisCall >= maxEnqueuePerFrame) {
                    return;
                }
            }
        }
    }
}

void World::processStagedUploads(size_t maxUploads) {
    std::vector<StagedMeshResult> toUpload;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_stagedMeshes.empty()) return;
        size_t count = std::min(m_stagedMeshes.size(), maxUploads);
        for (size_t i = 0; i < count; ++i) {
            toUpload.push_back(std::move(m_stagedMeshes.front()));
            m_stagedMeshes.pop_front();
        }
    }

    uploadMeshesBatched(toUpload);
}

void World::uploadMeshesBatched(std::vector<StagedMeshResult>& items) {
    if (items.empty()) return;

    // Record all buffer copies into ONE single command buffer (1 GPU submit, 0 repeated stalls)
    VkCommandBuffer cmd = m_cmdQueue.beginSingleTimeCommands();

    std::vector<Buffer> stagingBuffers;
    stagingBuffers.reserve(items.size() * 4);

    struct UploadEntry {
        ChunkCoord coord;
        LODLevel lod;
        ChunkRenderData renderData;
    };
    std::vector<UploadEntry> uploadedData;
    uploadedData.reserve(items.size());

    for (auto& item : items) {
        if (!item.valid) {
            std::lock_guard<std::mutex> qlock(m_queueMutex);
            m_pendingTasks.erase(item.coord);
            continue;
        }

        ChunkRenderData data;
        data.coord = item.coord;
        data.lod = item.lod;

        // 1. Opaque Mesh
        if (!item.mesh.opaqueIndices.empty()) {
            VkDeviceSize vSize = item.mesh.opaqueVertices.size() * sizeof(ChunkVertex);
            VkDeviceSize iSize = item.mesh.opaqueIndices.size() * sizeof(uint32_t);

            Buffer stagingVbo(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
            stagingVbo.upload(item.mesh.opaqueVertices.data(), vSize);

            Buffer devVbo(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
            devVbo.recordCopy(cmd, stagingVbo, vSize);

            Buffer stagingIbo(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
            stagingIbo.upload(item.mesh.opaqueIndices.data(), iSize);

            Buffer devIbo(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
            devIbo.recordCopy(cmd, stagingIbo, iSize);

            data.opaqueVertexBuffer = std::move(devVbo);
            data.opaqueIndexBuffer = std::move(devIbo);
            data.opaqueIndexCount = static_cast<uint32_t>(item.mesh.opaqueIndices.size());

            stagingBuffers.push_back(std::move(stagingVbo));
            stagingBuffers.push_back(std::move(stagingIbo));
        }

        // 2. Translucent Water Mesh
        if (!item.mesh.waterIndices.empty()) {
            VkDeviceSize vSize = item.mesh.waterVertices.size() * sizeof(ChunkVertex);
            VkDeviceSize iSize = item.mesh.waterIndices.size() * sizeof(uint32_t);

            Buffer stagingVbo(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
            stagingVbo.upload(item.mesh.waterVertices.data(), vSize);

            Buffer devVbo(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
            devVbo.recordCopy(cmd, stagingVbo, vSize);

            Buffer stagingIbo(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
            stagingIbo.upload(item.mesh.waterIndices.data(), iSize);

            Buffer devIbo(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
            devIbo.recordCopy(cmd, stagingIbo, iSize);

            data.waterVertexBuffer = std::move(devVbo);
            data.waterIndexBuffer = std::move(devIbo);
            data.waterIndexCount = static_cast<uint32_t>(item.mesh.waterIndices.size());

            stagingBuffers.push_back(std::move(stagingVbo));
            stagingBuffers.push_back(std::move(stagingIbo));
        }

        uploadedData.push_back({item.coord, item.lod, std::move(data)});
    }

    // Submit all copies in a single unified command buffer!
    m_cmdQueue.endSingleTimeCommands(cmd);

    // Apply to mesh dictionary and clear from pending tasks
    {
        std::lock_guard<std::mutex> qlock(m_queueMutex);
        for (auto& entry : uploadedData) {
            m_chunkLODs[entry.coord] = entry.lod;
            m_meshes[entry.coord] = std::move(entry.renderData);
            m_pendingTasks.erase(entry.coord);
        }
    }
}

void World::unloadDistantChunks(const ChunkCoord& center) {
    int maxDist = renderDistance + 6;

    // 1. Unload GPU meshes beyond renderDistance + 6
    for (auto it = m_meshes.begin(); it != m_meshes.end(); ) {
        int dist = std::max(std::abs(it->first.cx - center.cx), std::abs(it->first.cz - center.cz));
        if (dist > maxDist) {
            m_chunkLODs.erase(it->first);
            it = m_meshes.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Unload voxel block RAM for chunks outside active radius (scales with render distance)
    int maxVoxelDist = renderDistance + 4;
    {
        std::unique_lock<std::shared_mutex> lock(m_chunksMutex);
        std::lock_guard<std::mutex> qlock(m_queueMutex);
        for (auto it = m_chunks.begin(); it != m_chunks.end(); ) {
            int dist = std::max(std::abs(it->first.cx - center.cx), std::abs(it->first.cz - center.cz));
            if (dist > maxVoxelDist && m_pendingTasks.find(it->first) == m_pendingTasks.end()) {
                it = m_chunks.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void World::meshChunk(const ChunkCoord& coord, LODLevel lod) {
    Chunk* chunk = nullptr;
    const Chunk* north = nullptr;
    const Chunk* south = nullptr;
    const Chunk* west = nullptr;
    const Chunk* east = nullptr;

    {
        std::shared_lock<std::shared_mutex> lock(m_chunksMutex);
        chunk = getChunk(coord);
        north = getChunk({coord.cx, coord.cz - 1});
        south = getChunk({coord.cx, coord.cz + 1});
        west = getChunk({coord.cx - 1, coord.cz});
        east = getChunk({coord.cx + 1, coord.cz});
    }
    
    if (!chunk) return;
    
    ChunkMesh mesh;
    if (lod == LODLevel::LOD0_Full) {
        if (!north || !south || !west || !east) return;
        mesh = ChunkMesher::generateMesh(*chunk, north, south, west, east, this);
    } else {
        mesh = ChunkMesher::generateLODMesh(*chunk, lod);
    }

    uploadMesh(coord, mesh, lod);
    chunk->clearDirty();
}

void World::uploadMesh(const ChunkCoord& coord, const ChunkMesh& mesh, LODLevel lod) {
    if (mesh.empty()) {
        m_meshes.erase(coord);
        m_chunkLODs.erase(coord);
        return;
    }

    ChunkRenderData data;
    data.coord = coord;
    data.lod = lod;

    // 1. Upload Opaque Mesh
    if (!mesh.opaqueIndices.empty()) {
        VkDeviceSize vertexSize = mesh.opaqueVertices.size() * sizeof(ChunkVertex);
        VkDeviceSize indexSize = mesh.opaqueIndices.size() * sizeof(uint32_t);
        
        Buffer vbo(m_context, vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        vbo.uploadStaged(m_context, m_cmdQueue, mesh.opaqueVertices.data(), vertexSize);
        
        Buffer ibo(m_context, indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        ibo.uploadStaged(m_context, m_cmdQueue, mesh.opaqueIndices.data(), indexSize);

        data.opaqueVertexBuffer = std::move(vbo);
        data.opaqueIndexBuffer = std::move(ibo);
        data.opaqueIndexCount = static_cast<uint32_t>(mesh.opaqueIndices.size());
    }

    // 2. Upload Translucent Water Mesh
    if (!mesh.waterIndices.empty()) {
        VkDeviceSize vertexSize = mesh.waterVertices.size() * sizeof(ChunkVertex);
        VkDeviceSize indexSize = mesh.waterIndices.size() * sizeof(uint32_t);
        
        Buffer vbo(m_context, vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        vbo.uploadStaged(m_context, m_cmdQueue, mesh.waterVertices.data(), vertexSize);
        
        Buffer ibo(m_context, indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        ibo.uploadStaged(m_context, m_cmdQueue, mesh.waterIndices.data(), indexSize);

        data.waterVertexBuffer = std::move(vbo);
        data.waterIndexBuffer = std::move(ibo);
        data.waterIndexCount = static_cast<uint32_t>(mesh.waterIndices.size());
    }
    
    m_chunkLODs[coord] = lod;
    m_meshes[coord] = std::move(data);
}

Chunk* World::getChunk(const ChunkCoord& coord) {
    auto it = m_chunks.find(coord);
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

const Chunk* World::getChunk(const ChunkCoord& coord) const {
    auto it = m_chunks.find(coord);
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

Cell World::getCell(int worldX, int y, int worldZ, int s) const {
    ChunkCoord coord = worldToChunk(worldX, worldZ);
    std::shared_lock<std::shared_mutex> lock(m_chunksMutex);
    const Chunk* chunk = getChunk(coord);
    if (!chunk) return Cell{BlockType::Air};
    
    int localX, localZ;
    worldToLocal(worldX, worldZ, localX, localZ);
    return chunk->getCell(localX, y, localZ, s);
}

void World::setCell(int worldX, int y, int worldZ, int s, Cell cell) {
    ChunkCoord coord = worldToChunk(worldX, worldZ);
    {
        std::unique_lock<std::shared_mutex> lock(m_chunksMutex);
        Chunk* chunk = getChunk(coord);
        if (!chunk) return;
        
        int localX, localZ;
        worldToLocal(worldX, worldZ, localX, localZ);
        
        chunk->setCell(localX, y, localZ, s, cell);
        chunk->markDirty();
        
        if (localX == 0) {
            if (Chunk* n = getChunk({coord.cx - 1, coord.cz})) n->markDirty();
        } else if (localX == CHUNK_SIZE_X - 1) {
            if (Chunk* n = getChunk({coord.cx + 1, coord.cz})) n->markDirty();
        }
        
        if (localZ == 0) {
            if (Chunk* n = getChunk({coord.cx, coord.cz - 1})) n->markDirty();
        } else if (localZ == CHUNK_SIZE_Z - 1) {
            if (Chunk* n = getChunk({coord.cx, coord.cz + 1})) n->markDirty();
        }
    }

    // Track placed torches for instant O(1) point-light lookups
    glm::vec3 torchWorldPos = cellToWorldCenter(worldX, y, worldZ, s);
    {
        std::lock_guard<std::mutex> tLock(m_torchesMutex);
        if (cell.isTorch()) {
            bool exists = false;
            for (const auto& p : m_placedTorches) {
                if (glm::distance(p, torchWorldPos) < 0.1f) { exists = true; break; }
            }
            if (!exists) m_placedTorches.push_back(torchWorldPos);
        } else {
            m_placedTorches.erase(std::remove_if(m_placedTorches.begin(), m_placedTorches.end(), [&](const glm::vec3& p) {
                return glm::distance(p, torchWorldPos) < 0.1f;
            }), m_placedTorches.end());
        }
    }
}

void World::setCellInstant(int worldX, int y, int worldZ, int s, Cell cell) {
    setCell(worldX, y, worldZ, s, cell);
    
    ChunkCoord coord = worldToChunk(worldX, worldZ);
    meshChunk(coord, LODLevel::LOD0_Full);
    
    int localX, localZ;
    worldToLocal(worldX, worldZ, localX, localZ);
    if (localX == 0) {
        meshChunk({coord.cx - 1, coord.cz}, LODLevel::LOD0_Full);
    } else if (localX == CHUNK_SIZE_X - 1) {
        meshChunk({coord.cx + 1, coord.cz}, LODLevel::LOD0_Full);
    }
    
    if (localZ == 0) {
        meshChunk({coord.cx, coord.cz - 1}, LODLevel::LOD0_Full);
    } else if (localZ == CHUNK_SIZE_Z - 1) {
        meshChunk({coord.cx, coord.cz + 1}, LODLevel::LOD0_Full);
    }
}

void World::checkGravity(int worldX, int y, int worldZ, int s, FallingBlockManager* fallingBlocks) {
    if (y < 0 || y >= CHUNK_SIZE_Y) return;
    for (int checkY = std::max(0, y); checkY < CHUNK_SIZE_Y; ++checkY) {
        Cell c = getCell(worldX, checkY, worldZ, s);
        if (c.type == BlockType::Sand || c.type == BlockType::Gravel) {
            int fallToY = checkY;
            while (fallToY > 0) {
                Cell below = getCell(worldX, fallToY - 1, worldZ, s);
                if (below.type == BlockType::Air || below.type == BlockType::Water) {
                    fallToY--;
                } else {
                    break;
                }
            }
            if (fallToY != checkY) {
                setCellInstant(worldX, checkY, worldZ, s, Cell{BlockType::Air});
                if (fallingBlocks) {
                    fallingBlocks->spawn(worldX, checkY, worldZ, s, fallToY, c.type);
                } else {
                    setCellInstant(worldX, fallToY, worldZ, s, c);
                }
            }
        }
    }
}

float World::getHighestSolidY(float worldX, float worldZ) const {
    CellCoord c = worldToCell(glm::vec3(worldX, 0.0f, worldZ));
    for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
        for (int s = 0; s < 2; ++s) {
            Cell cell = getCell(c.x, y, c.z, s);
            if (cell.isSolid()) {
                return static_cast<float>(y + 1);
            }
        }
    }
    return 60.0f;
}

bool World::isChunkInFrustum(const ChunkCoord& coord, const Frustum& frustum) {
    float margin = 6.0f;
    float minX = static_cast<float>(coord.cx * CHUNK_SIZE_X) - margin;
    float maxX = static_cast<float>((coord.cx + 1) * CHUNK_SIZE_X) + 1.0f + margin;
    float minY = 0.0f - margin;
    float maxY = static_cast<float>(CHUNK_SIZE_Y) + margin;
    float minZ = static_cast<float>(coord.cz * CHUNK_SIZE_Z) * TRI_HEIGHT - margin;
    float maxZ = static_cast<float>((coord.cz + 1) * CHUNK_SIZE_Z) * TRI_HEIGHT + margin;

    for (const auto& plane : frustum.planes) {
        float px = (plane.x > 0.0f) ? maxX : minX;
        float py = (plane.y > 0.0f) ? maxY : minY;
        float pz = (plane.z > 0.0f) ? maxZ : minZ;

        if (plane.x * px + plane.y * py + plane.z * pz + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

void World::getLightLevels(int worldX, int y, int worldZ, int s, int& skyLight, int& blockLight) const {
    (void)s;
    skyLight = 15;
    blockLight = 0;

    ChunkCoord coord = worldToChunk(worldX, worldZ);
    int localX, localZ;
    worldToLocal(worldX, worldZ, localX, localZ);
    {
        std::shared_lock<std::shared_mutex> lock(m_chunksMutex);
        const Chunk* chunk = getChunk(coord);
        if (chunk) {
            for (int sy = y + 1; sy < CHUNK_SIZE_Y; ++sy) {
                Cell c0 = chunk->getCell(localX, sy, localZ, 0);
                Cell c1 = chunk->getCell(localX, sy, localZ, 1);
                bool isSolid = (c0.isOpaque() && c0.type != BlockType::Leaves) || (c1.isOpaque() && c1.type != BlockType::Leaves);
                bool isLeaf = (c0.type == BlockType::Leaves || c1.type == BlockType::Leaves);
                if (isSolid) {
                    int depth = sy - y;
                    skyLight = std::max(0, 15 - depth * 2);
                    break;
                } else if (isLeaf) {
                    int depth = sy - y;
                    skyLight = std::max(12, 15 - depth); // Leaves allow abundant daylight through
                    break;
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> tLock(m_torchesMutex);
        for (const auto& tp : m_placedTorches) {
            int dx = std::abs(static_cast<int>(std::floor(tp.x)) - worldX);
            int dy = std::abs(static_cast<int>(std::floor(tp.y)) - y);
            int dz = std::abs(static_cast<int>(std::floor(tp.z)) - worldZ);
            if (dx <= 10 && dz <= 10 && dy <= 6) {
                int dist = dx + dy + dz;
                int bl = std::max(0, 14 - dist);
                if (bl > blockLight) blockLight = bl;
            }
        }
    }
}

std::vector<glm::vec3> World::getNearestPlacedTorches(const glm::vec3& refPos, size_t maxCount, float maxDist) const {
    std::lock_guard<std::mutex> tLock(m_torchesMutex);
    if (m_placedTorches.empty()) return {};

    struct DistTorch {
        float distSq;
        glm::vec3 pos;
    };
    std::vector<DistTorch> candidates;
    float maxDistSq = maxDist * maxDist;

    for (const auto& pos : m_placedTorches) {
        glm::vec3 diff = pos - refPos;
        float dSq = glm::dot(diff, diff);
        if (dSq <= maxDistSq) {
            candidates.push_back({dSq, pos});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const DistTorch& a, const DistTorch& b) {
        return a.distSq < b.distSq;
    });

    size_t count = std::min(maxCount, candidates.size());
    std::vector<glm::vec3> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        result.push_back(candidates[i].pos);
    }
    return result;
}

std::optional<glm::vec3> World::getNearestPlacedTorch(const glm::vec3& refPos, float maxDist) const {
    std::lock_guard<std::mutex> tLock(m_torchesMutex);
    float nearestDistSq = maxDist * maxDist;
    std::optional<glm::vec3> nearestPos;

    for (const auto& pos : m_placedTorches) {
        glm::vec3 diff = pos - refPos;
        float dSq = glm::dot(diff, diff);
        if (dSq < nearestDistSq) {
            nearestDistSq = dSq;
            nearestPos = pos;
        }
    }
    return nearestPos;
}

bool World::updateLoading(const glm::vec3& spawnPos, int targetRadius, int& loadedCount, int& totalCount) {
    CellCoord playerCell = worldToCell(spawnPos);
    ChunkCoord center = worldToChunk(playerCell.x, playerCell.z);

    totalCount = (2 * targetRadius + 1) * (2 * targetRadius + 1);

    // 1. Process staged GPU uploads aggressively during loading screen (up to 24 chunks per frame)
    processStagedUploads(24);

    // 2. Check which target chunks are already in m_meshes
    loadedCount = 0;
    std::vector<ChunkCoord> missingChunks;
    for (int dx = -targetRadius; dx <= targetRadius; ++dx) {
        for (int dz = -targetRadius; dz <= targetRadius; ++dz) {
            ChunkCoord c{center.cx + dx, center.cz + dz};
            if (m_meshes.find(c) != m_meshes.end()) {
                loadedCount++;
            } else {
                missingChunks.push_back(c);
            }
        }
    }

    if (loadedCount >= totalCount) {
        return true; // 100% of spawn chunks are meshed and uploaded to the GPU!
    }

    // 3. Sort missing chunks by distance to center so closest chunks generate and mesh first
    std::sort(missingChunks.begin(), missingChunks.end(), [center](const ChunkCoord& a, const ChunkCoord& b) {
        int distA = (a.cx - center.cx) * (a.cx - center.cx) + (a.cz - center.cz) * (a.cz - center.cz);
        int distB = (b.cx - center.cx) * (b.cx - center.cx) + (b.cz - center.cz) * (b.cz - center.cz);
        return distA < distB;
    });

    // 4. Enqueue missing spawn chunks (LOD0_Full)
    for (const auto& coord : missingChunks) {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_pendingTasks.find(coord) != m_pendingTasks.end()) {
                continue;
            }
            m_pendingTasks.insert(coord);
        }

        m_threadPool.enqueueTask([this, coord]() {
            // Generate coord chunk if missing
            bool needCoord = false;
            std::vector<ChunkCoord> missingNeighbors;
            {
                std::shared_lock<std::shared_mutex> rlock(this->m_chunksMutex);
                if (this->m_chunks.find(coord) == this->m_chunks.end()) {
                    needCoord = true;
                }
                ChunkCoord neighbors[4] = {
                    {coord.cx, coord.cz - 1}, {coord.cx, coord.cz + 1},
                    {coord.cx - 1, coord.cz}, {coord.cx + 1, coord.cz}
                };
                for (const auto& nc : neighbors) {
                    if (this->m_chunks.find(nc) == this->m_chunks.end()) {
                        missingNeighbors.push_back(nc);
                    }
                }
            }

            std::unique_ptr<Chunk> newCoordChunk;
            if (needCoord) {
                newCoordChunk = std::make_unique<Chunk>(coord);
                this->m_terrainGen.generateChunk(*newCoordChunk);
            }

            std::vector<std::pair<ChunkCoord, std::unique_ptr<Chunk>>> newNeighbors;
            for (const auto& nc : missingNeighbors) {
                auto nChunk = std::make_unique<Chunk>(nc);
                this->m_terrainGen.generateChunk(*nChunk);
                newNeighbors.emplace_back(nc, std::move(nChunk));
            }

            if (newCoordChunk || !newNeighbors.empty()) {
                std::unique_lock<std::shared_mutex> wlock(this->m_chunksMutex);
                if (newCoordChunk && this->m_chunks.find(coord) == this->m_chunks.end()) {
                    this->m_chunks[coord] = std::move(newCoordChunk);
                }
                for (auto& pair : newNeighbors) {
                    if (this->m_chunks.find(pair.first) == this->m_chunks.end()) {
                        this->m_chunks[pair.first] = std::move(pair.second);
                    }
                }
            }

            const Chunk* chunk = nullptr;
            const Chunk* north = nullptr;
            const Chunk* south = nullptr;
            const Chunk* west = nullptr;
            const Chunk* east = nullptr;
            {
                std::shared_lock<std::shared_mutex> lock(this->m_chunksMutex);
                chunk = this->getChunk(coord);
                north = this->getChunk({coord.cx, coord.cz - 1});
                south = this->getChunk({coord.cx, coord.cz + 1});
                west = this->getChunk({coord.cx - 1, coord.cz});
                east = this->getChunk({coord.cx + 1, coord.cz});
            }

            ChunkMesh mesh;
            bool valid = false;
            if (chunk && north && south && west && east) {
                mesh = ChunkMesher::generateMesh(*chunk, north, south, west, east, this);
                valid = true;
            }

            {
                std::lock_guard<std::mutex> lock(this->m_queueMutex);
                this->m_stagedMeshes.push_back({coord, std::move(mesh), LODLevel::LOD0_Full, valid});
            }
        });
    }

    return false;
}

} // namespace prismcraft
