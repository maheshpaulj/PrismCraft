#include "CloudRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/ChunkMesher.hpp"
#include "world/Coordinates.hpp"
#include "renderer/TextureAtlas.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

namespace prismcraft {

CloudRenderer::CloudRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
}

static constexpr float CLOUD_SCALE = 5.5f;

static inline bool sampleCloudPattern(float x, float z) {
    float px = x * 0.036f;
    float pz = z * 0.036f;
    // Multi-octave cumulus pattern with smaller, noisy, crisp separated patches
    float n1 = std::sin(px * 1.6f + std::cos(pz * 1.3f)) * std::cos(pz * 1.7f);
    float n2 = std::sin(px * 3.6f - pz * 3.1f) * 0.55f;
    float n3 = std::cos(px * 7.8f + pz * 6.9f) * 0.35f;
    float n4 = std::sin(px * 15.6f - pz * 13.8f) * 0.22f;
    float density = n1 + n2 + n3 + n4;
    return density > 0.46f; // Smaller, separated puffy clusters with broken noisy edges
}

void CloudRenderer::rebuildCloudMesh(int anchorX, int anchorZ) {
    // Wait for in-flight GPU command buffers before destroying/recreating VBO and IBO
    m_context.waitIdle();

    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE); // Pure solid white tile
    float cloudScale = CLOUD_SCALE; // Fine scale for discrete puffy cloud blocks
    float yBottom = 160.0f;
    float yTop    = 168.0f;

    auto addTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                      const glm::vec3& norm, const glm::vec3& col) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, glm::vec2(uv.x, uv.w), norm, col});
        vertices.push_back({v1, glm::vec2(uv.z, uv.w), norm, col});
        vertices.push_back({v2, glm::vec2(uv.x + (uv.z - uv.x) * 0.5f, uv.y), norm, col});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    auto addQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                       const glm::vec3& norm, const glm::vec3& col) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, glm::vec2(uv.x, uv.w), norm, col});
        vertices.push_back({v1, glm::vec2(uv.z, uv.w), norm, col});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, col});
        vertices.push_back({v3, glm::vec2(uv.x, uv.y), norm, col});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    glm::vec3 colTop(1.0f, 1.0f, 1.0f);        // Fluffy pure white cloud top
    glm::vec3 colBot(0.72f, 0.72f, 0.76f);     // Soft shadowed cloud bottom
    glm::vec3 colSide(0.86f, 0.86f, 0.89f);    // Shaded vertical perimeter walls

    // Precompute discrete 2D boolean grid for 100% symmetric neighbor sharing
    // This guarantees that any internal wall between two adjacent cloud cells is completely eliminated!
    static constexpr int GRID_R = 64;
    static constexpr int GRID_DIM = GRID_R * 2 + 1;
    static constexpr int MESH_R = 60;

    std::vector<uint8_t> cloudGrid(GRID_DIM * GRID_DIM * 2, 0);

    auto getGridIdx = [&](int gx, int gz, int s) -> int {
        int ix = gx + GRID_R;
        int iz = gz + GRID_R;
        if (ix < 0 || ix >= GRID_DIM || iz < 0 || iz >= GRID_DIM || s < 0 || s >= 2) return -1;
        return (iz * GRID_DIM + ix) * 2 + s;
    };

    auto isCloudAt = [&](int gx, int gz, int s) -> bool {
        int idx = getGridIdx(gx, gz, s);
        if (idx < 0) return false;
        return cloudGrid[idx] != 0;
    };

    for (int gz = -GRID_R; gz <= GRID_R; ++gz) {
        int z = anchorZ + gz;
        for (int gx = -GRID_R; gx <= GRID_R; ++gx) {
            int x = anchorX + gx;
            for (int s = 0; s < 2; ++s) {
                glm::vec3 c = cellToWorldCenter(x, 0, z, s);
                bool cVal = sampleCloudPattern(c.x * cloudScale, c.z * cloudScale);
                int idx = getGridIdx(gx, gz, s);
                if (idx >= 0) cloudGrid[idx] = cVal ? 1 : 0;
            }
        }
    }

    for (int gz = -MESH_R; gz <= MESH_R; ++gz) {
        int z = anchorZ + gz;
        for (int gx = -MESH_R; gx <= MESH_R; ++gx) {
            int x = anchorX + gx;
            for (int s = 0; s < 2; ++s) {
                if (!isCloudAt(gx, gz, s)) continue;

                glm::vec2 vXZ[3];
                getPrismVerticesXZ(x, z, s, vXZ);

                glm::vec3 T0(vXZ[0].x * cloudScale, yTop, vXZ[0].y * cloudScale);
                glm::vec3 T1(vXZ[1].x * cloudScale, yTop, vXZ[1].y * cloudScale);
                glm::vec3 T2(vXZ[2].x * cloudScale, yTop, vXZ[2].y * cloudScale);

                glm::vec3 B0(vXZ[0].x * cloudScale, yBottom, vXZ[0].y * cloudScale);
                glm::vec3 B1(vXZ[1].x * cloudScale, yBottom, vXZ[1].y * cloudScale);
                glm::vec3 B2(vXZ[2].x * cloudScale, yBottom, vXZ[2].y * cloudScale);

                // Top face (+Y)
                addTri(T0, T2, T1, glm::vec3(0, 1, 0), colTop);

                // Bottom face (-Y)
                addTri(B0, B1, B2, glm::vec3(0, -1, 0), colBot);

                // Internal shared face culling:
                // Only emit lateral quad faces if the neighboring cell is NOT a cloud!
                CellCoord neighbors[5];
                getNeighbors(x, 0, z, s, neighbors);

                if (s == 0) {
                    // Base wall (along Z = z0, normal = (0, 0, -1))
                    int nGx = neighbors[2].x - anchorX;
                    int nGz = neighbors[2].z - anchorZ;
                    if (!isCloudAt(nGx, nGz, neighbors[2].s)) {
                        addQuad(B0, T0, T1, B1, glm::vec3(0.0f, 0.0f, -1.0f), colSide);
                    }
                    // Left slanted wall (normal = (-0.866, 0, 0.5))
                    nGx = neighbors[3].x - anchorX;
                    nGz = neighbors[3].z - anchorZ;
                    if (!isCloudAt(nGx, nGz, neighbors[3].s)) {
                        addQuad(B2, T2, T0, B0, glm::vec3(-SQRT_3_OVER_2, 0.0f, 0.5f), colSide);
                    }
                    // Right slanted wall (normal = (0.866, 0, 0.5))
                    nGx = neighbors[4].x - anchorX;
                    nGz = neighbors[4].z - anchorZ;
                    if (!isCloudAt(nGx, nGz, neighbors[4].s)) {
                        addQuad(B1, T1, T2, B2, glm::vec3(SQRT_3_OVER_2, 0.0f, 0.5f), colSide);
                    }
                } else {
                    // Base wall (along Z = z1, normal = (0, 0, 1))
                    int nGx = neighbors[2].x - anchorX;
                    int nGz = neighbors[2].z - anchorZ;
                    if (!isCloudAt(nGx, nGz, neighbors[2].s)) {
                        addQuad(B1, T1, T2, B2, glm::vec3(0.0f, 0.0f, 1.0f), colSide);
                    }
                    // Left slanted wall (normal = (-0.866, 0, -0.5))
                    nGx = neighbors[3].x - anchorX;
                    nGz = neighbors[3].z - anchorZ;
                    if (!isCloudAt(nGx, nGz, neighbors[3].s)) {
                        addQuad(B2, T2, T0, B0, glm::vec3(-SQRT_3_OVER_2, 0.0f, -0.5f), colSide);
                    }
                    // Right slanted wall (normal = (0.866, 0, -0.5))
                    nGx = neighbors[4].x - anchorX;
                    nGz = neighbors[4].z - anchorZ;
                    if (!isCloudAt(nGx, nGz, neighbors[4].s)) {
                        addQuad(B0, T0, T1, B1, glm::vec3(SQRT_3_OVER_2, 0.0f, -0.5f), colSide);
                    }
                }
            }
        }
    }

    m_indexCount[0] = static_cast<uint32_t>(indices.size());
    if (indices.empty()) return;

    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_vbo[0] = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_vbo[0].uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_ibo[0] = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ibo[0].uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
}

void CloudRenderer::render(VkCommandBuffer cmd,
                           const Pipeline& cloudPipeline,
                           const glm::vec3& camPos,
                           float time,
                           const glm::mat4& vpMatrix,
                           const glm::vec3& skyColor,
                           const glm::vec3& sunDir) {
    float cloudScale = CLOUD_SCALE;
    float driftSpeed = 2.4f;
    float totalDrift = std::fmod(time * driftSpeed, 50000.0f);

    // Anchor updates only when player travels across cells (in steps of 2 cells to preserve even row parity)
    int anchorX = static_cast<int>(std::floor(camPos.x / (cloudScale * 2.0f))) * 2;
    int anchorZ = static_cast<int>(std::floor(camPos.z / (cloudScale * TRI_HEIGHT * 2.0f))) * 2;

    if (m_indexCount[0] == 0 || anchorX != m_lastAnchorX || anchorZ != m_lastAnchorZ) {
        rebuildCloudMesh(anchorX, anchorZ);
        m_lastAnchorX = anchorX;
        m_lastAnchorZ = anchorZ;
    }

    if (m_indexCount[0] == 0 || !m_vbo[0].isValid()) return;

    // Smooth continuous drift via GPU model matrix
    glm::mat4 cloudModel = glm::translate(glm::mat4(1.0f), glm::vec3(totalDrift, 0.0f, 0.0f));
    glm::mat4 mvp = vpMatrix * cloudModel;

    PushConstants pc{};
    std::memcpy(pc.mvp, &mvp[0][0], sizeof(float) * 16);
    pc.sunDir[0] = sunDir.x; pc.sunDir[1] = sunDir.y; pc.sunDir[2] = sunDir.z; pc.sunDir[3] = 1.0f;
    pc.skyFog[0] = skyColor.r; pc.skyFog[1] = skyColor.g; pc.skyFog[2] = skyColor.b; pc.skyFog[3] = 1500.0f;
    pc.camPos[0] = camPos.x; pc.camPos[1] = camPos.y; pc.camPos[2] = camPos.z; pc.camPos[3] = time;

    cloudPipeline.bind(cmd);
    vkCmdPushConstants(cmd, cloudPipeline.getLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pc);

    VkBuffer vbs[] = {m_vbo[0].getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo[0].getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_indexCount[0], 1, 0, 0, 0);
}

} // namespace prismcraft
