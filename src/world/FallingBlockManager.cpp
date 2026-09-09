#include "FallingBlockManager.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/World.hpp"
#include "world/Coordinates.hpp"
#include "renderer/TextureAtlas.hpp"
#include "audio/AudioEngine.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>

namespace prismcraft {

FallingBlockManager::FallingBlockManager(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
}

void FallingBlockManager::spawn(int x, int startY, int z, int s, int targetY, BlockType type) {
    if (isFalling(x, z, s)) return;
    FallingBlock fb;
    fb.x = x;
    fb.z = z;
    fb.s = s;
    fb.currentY = static_cast<float>(startY);
    fb.targetY = targetY;
    fb.velY = 0.0f;
    fb.type = type;
    m_blocks.push_back(fb);
}

bool FallingBlockManager::isFalling(int x, int z, int s) const {
    for (const auto& b : m_blocks) {
        if (b.x == x && b.z == z && b.s == s) return true;
    }
    return false;
}

void FallingBlockManager::update(float dt, World& world) {
    for (auto& fb : m_blocks) {
        fb.velY -= 28.0f * dt;
        fb.currentY += fb.velY * dt;

        if (fb.currentY <= static_cast<float>(fb.targetY)) {
            world.setCellInstant(fb.x, fb.targetY, fb.z, fb.s, Cell{fb.type});
            AudioEngine::get().playSound(SoundEffect::BlockPlace, 0.7f);
            fb.currentY = -999.0f; // Mark completed
        }
    }

    m_blocks.erase(
        std::remove_if(m_blocks.begin(), m_blocks.end(),
            [](const FallingBlock& fb) { return fb.currentY < -900.0f; }),
        m_blocks.end()
    );
}

void FallingBlockManager::render(VkCommandBuffer cmd, const Pipeline& pipeline, const glm::mat4& vpMatrix) {
    if (m_blocks.empty()) return;

    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    for (const auto& fb : m_blocks) {
        glm::vec2 vXZ[3];
        getPrismVerticesXZ(fb.x, fb.z, fb.s, vXZ);

        float y0 = fb.currentY;
        float y1 = fb.currentY + 1.0f;

        glm::vec3 p0_bot(vXZ[0].x, y0, vXZ[0].y);
        glm::vec3 p1_bot(vXZ[1].x, y0, vXZ[1].y);
        glm::vec3 p2_bot(vXZ[2].x, y0, vXZ[2].y);

        glm::vec3 p0_top(vXZ[0].x, y1, vXZ[0].y);
        glm::vec3 p1_top(vXZ[1].x, y1, vXZ[1].y);
        glm::vec3 p2_top(vXZ[2].x, y1, vXZ[2].y);

        uint32_t sideTile = TextureAtlas::getTileForBlock(fb.type, 2);
        uint32_t topTile  = TextureAtlas::getTileForBlock(fb.type, 0);
        uint32_t botTile  = TextureAtlas::getTileForBlock(fb.type, 1);

        glm::vec4 uvSide = TextureAtlas::getTileUV(sideTile);
        glm::vec4 uvTop  = TextureAtlas::getTileUV(topTile);
        glm::vec4 uvBot  = TextureAtlas::getTileUV(botTile);

        auto addTri = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                          const glm::vec3& norm, const glm::vec4& uv) {
            uint32_t b = static_cast<uint32_t>(vertices.size());
            glm::vec3 col(1.0f);
            vertices.push_back({p0, glm::vec2(uv.x, uv.y), norm, col});
            vertices.push_back({p1, glm::vec2(uv.z, uv.y), norm, col});
            vertices.push_back({p2, glm::vec2(uv.x, uv.w), norm, col});
            indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        };

        auto addWallQuad = [&](const glm::vec3& a, const glm::vec3& b_pt, float h0, float h1) {
            // Edge is a -> b_pt. Viewed from outside, b_pt is left, a is right.
            glm::vec3 bl(b_pt.x, h0, b_pt.z);
            glm::vec3 br(a.x,    h0, a.z);
            glm::vec3 tr(a.x,    h1, a.z);
            glm::vec3 tl(b_pt.x, h1, b_pt.z);

            glm::vec3 edge = br - bl;
            glm::vec3 norm = glm::normalize(glm::cross(edge, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 col(1.0f);

            uint32_t bIdx = static_cast<uint32_t>(vertices.size());
            vertices.push_back({bl, glm::vec2(uvSide.x, uvSide.w), norm, col});
            vertices.push_back({br, glm::vec2(uvSide.z, uvSide.w), norm, col});
            vertices.push_back({tr, glm::vec2(uvSide.z, uvSide.y), norm, col});
            vertices.push_back({tl, glm::vec2(uvSide.x, uvSide.y), norm, col});

            indices.push_back(bIdx + 0); indices.push_back(bIdx + 1); indices.push_back(bIdx + 2);
            indices.push_back(bIdx + 0); indices.push_back(bIdx + 2); indices.push_back(bIdx + 3);
        };

        // Top face (CCW: p0 -> p1 -> p2)
        addTri(p0_top, p1_top, p2_top, glm::vec3(0.0f, 1.0f, 0.0f), uvTop);
        // Bottom face (CCW: p0 -> p2 -> p1)
        addTri(p0_bot, p2_bot, p1_bot, glm::vec3(0.0f, -1.0f, 0.0f), uvBot);

        // 3 side walls
        addWallQuad(p0_bot, p1_bot, y0, y1);
        addWallQuad(p1_bot, p2_bot, y0, y1);
        addWallQuad(p2_bot, p0_bot, y0, y1);
    }

    if (indices.empty()) return;

    uint32_t f = m_cmdQueue.getCurrentFrame();
    m_indexCount[f] = static_cast<uint32_t>(indices.size());

    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    if (!m_vbo[f].isValid() || m_vbo[f].getSize() < vSize) {
        m_vbo[f] = Buffer(m_context, std::max(vSize, (VkDeviceSize)8192), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    if (!m_ibo[f].isValid() || m_ibo[f].getSize() < iSize) {
        m_ibo[f] = Buffer(m_context, std::max(iSize, (VkDeviceSize)2048), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    m_vbo[f].upload(vertices.data(), vSize);
    m_ibo[f].upload(indices.data(), iSize);

    PushConstants pc{};
    std::memcpy(pc.mvp, &vpMatrix[0][0], sizeof(float) * 16);
    pc.sunDir[3] = 1.0f;
    pc.skyFog[3] = 1000.0f;

    vkCmdPushConstants(cmd, pipeline.getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &pc);

    VkBuffer vbs[] = {m_vbo[f].getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo[f].getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_indexCount[f], 1, 0, 0, 0);
}

} // namespace prismcraft
