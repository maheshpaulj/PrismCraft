#include "ArrowManager.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/World.hpp"
#include "world/Coordinates.hpp"
#include "player/Player.hpp"
#include "renderer/TextureAtlas.hpp"
#include "audio/AudioEngine.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace prismcraft {

ArrowManager::ArrowManager(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context), m_cmdQueue(cmdQueue) {
}

void ArrowManager::spawnArrow(const glm::vec3& pos, const glm::vec3& velocity) {
    Arrow arrow;
    arrow.pos = pos;
    arrow.vel = velocity;
    arrow.heading = (glm::length(velocity) > 0.001f) ? glm::normalize(velocity) : glm::vec3(0, 0, -1);
    arrow.life = 0.0f;
    arrow.inGround = false;
    m_arrows.push_back(arrow);
}

void ArrowManager::update(float dt, const World& world, Player& player) {
    const glm::vec3& pPos = player.getPosition();

    for (auto it = m_arrows.begin(); it != m_arrows.end(); ) {
        it->life += dt;
        if (it->life > 60.0f) {
            it = m_arrows.erase(it);
            continue;
        }

        if (!it->inGround) {
            it->vel.y -= 14.0f * dt; // Gravity
            if (glm::length(it->vel) > 0.1f) {
                it->heading = glm::normalize(it->vel);
            }

            glm::vec3 nextPos = it->pos + it->vel * dt;
            CellCoord cc = worldToCell(nextPos);
            Cell cell = world.getCell(cc.x, cc.y, cc.z, cc.s);

            if (cell.isSolid()) {
                it->inGround = true;
                it->vel = glm::vec3(0.0f);
                AudioEngine::get().playSound(SoundEffect::BlockPlace);
            } else {
                it->pos = nextPos;
            }
            ++it;
        } else {
            // Player pickup
            float dist = glm::distance(pPos + glm::vec3(0.0f, 0.9f, 0.0f), it->pos);
            if (dist < 1.4f) {
                player.pickupItem(BlockType::ItemArrow, 1);
                AudioEngine::get().playSound(SoundEffect::ItemPop);
                it = m_arrows.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void ArrowManager::render(VkCommandBuffer cmd,
                          const Pipeline& pipeline,
                          const glm::mat4& vpMatrix) {
    if (m_arrows.empty()) return;

    m_frameIndex = (m_frameIndex + 1) % 2;

    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec4 uv = TextureAtlas::getTileUV(161); // Arrow Tile
    glm::vec3 lightCol(1.0f, 1.0f, 0.0f);

    auto addQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                       const glm::vec3& norm) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, glm::vec2(uv.x, uv.w), norm, lightCol});
        vertices.push_back({v1, glm::vec2(uv.z, uv.w), norm, lightCol});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, lightCol});
        vertices.push_back({v3, glm::vec2(uv.x, uv.y), norm, lightCol});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
        // Double sided
        indices.push_back(b + 0); indices.push_back(b + 2); indices.push_back(b + 1);
        indices.push_back(b + 2); indices.push_back(b + 0); indices.push_back(b + 3);
    };

    for (const auto& arrow : m_arrows) {
        glm::vec3 fwd = arrow.heading;
        glm::vec3 up = std::abs(fwd.y) > 0.95f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        glm::vec3 right = glm::normalize(glm::cross(fwd, up));
        up = glm::normalize(glm::cross(right, fwd));

        float halfLen = 0.32f;
        float halfW = 0.045f;

        glm::vec3 tail = arrow.pos - fwd * halfLen;
        glm::vec3 head = arrow.pos + fwd * halfLen;

        // Quad 1: horizontal fin
        addQuad(tail - right * halfW, head - right * halfW, head + right * halfW, tail + right * halfW, up);
        // Quad 2: vertical fin (cross)
        addQuad(tail - up * halfW, head - up * halfW, head + up * halfW, tail + up * halfW, right);
    }

    if (vertices.empty()) return;

    m_indexCount[m_frameIndex] = static_cast<uint32_t>(indices.size());
    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_vbo[m_frameIndex] = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_vbo[m_frameIndex].uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_ibo[m_frameIndex] = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ibo[m_frameIndex].uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);

    PushConstants pc{};
    std::memcpy(pc.mvp, &vpMatrix[0][0], sizeof(float) * 16);
    pc.sunDir[3] = 1.0f;
    pc.skyFog[3] = 500.0f;

    vkCmdPushConstants(cmd, pipeline.getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &pc);

    VkBuffer vbs[] = {m_vbo[m_frameIndex].getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo[m_frameIndex].getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_indexCount[m_frameIndex], 1, 0, 0, 0);
}

} // namespace prismcraft
