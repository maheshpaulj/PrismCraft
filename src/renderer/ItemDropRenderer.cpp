#include "ItemDropRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/World.hpp"
#include "world/ChunkMesher.hpp"
#include "player/Player.hpp"
#include "audio/AudioEngine.hpp"
#include "TextureAtlas.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstring>

namespace prismcraft {

ItemDropManager::ItemDropManager(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
}

void ItemDropManager::spawnDrop(const glm::vec3& pos, BlockType type, const glm::vec3& vel) {
    if (type == BlockType::Air) return;
    ItemDrop drop;
    drop.pos = pos;
    drop.vel = vel;
    drop.type = type;
    m_drops.push_back(drop);
}

void ItemDropManager::update(float dt, const World& world, Player& player) {
    glm::vec3 playerPos = player.getPosition() + glm::vec3(0.0f, 0.9f, 0.0f);

    for (auto& drop : m_drops) {
        if (drop.collected) continue;

        drop.vel.y -= 16.0f * dt;
        drop.vel.x *= std::max(0.0f, 1.0f - dt * 2.0f);
        drop.vel.z *= std::max(0.0f, 1.0f - dt * 2.0f);
        drop.pos += drop.vel * dt;

        float gy = world.getHighestSolidY(drop.pos.x, drop.pos.z);
        drop.groundY = gy;
        drop.inSunlight = (drop.pos.y >= gy - 0.2f);
        if (drop.pos.y < gy + 0.15f) {
            drop.pos.y = gy + 0.15f;
            drop.vel = glm::vec3(0.0f);
        }

        drop.age += dt;
        drop.rotAngle += dt * 1.8f;

        if (glm::distance(drop.pos, playerPos) < 1.5f && drop.age > 0.6f) {
            if (player.pickupItem(drop.type, 1)) {
                drop.collected = true;
                AudioEngine::get().playSound(SoundEffect::ItemPop);
            }
        }
    }

    m_drops.erase(std::remove_if(m_drops.begin(), m_drops.end(),
        [](const ItemDrop& d) { return d.collected || d.age > 300.0f; }),
        m_drops.end());
}

std::optional<std::pair<glm::vec3, float>> ItemDropManager::getNearestTorchDrop(const glm::vec3& refPos, float maxDist) const {
    float nearestDistSq = maxDist * maxDist;
    const ItemDrop* nearestDrop = nullptr;

    for (const auto& drop : m_drops) {
        if (drop.collected || drop.type != BlockType::Torch) continue;
        glm::vec3 diff = drop.pos - refPos;
        float dSq = glm::dot(diff, diff);
        if (dSq < nearestDistSq) {
            nearestDistSq = dSq;
            nearestDrop = &drop;
        }
    }

    if (nearestDrop) {
        float flicker = 1.0f + 0.07f * std::sin(nearestDrop->age * 11.0f) + 0.04f * std::cos(nearestDrop->age * 19.0f);
        return std::make_pair(nearestDrop->pos, flicker);
    }
    return std::nullopt;
}

void ItemDropManager::render(VkCommandBuffer cmd,
                             const Pipeline& pipeline,
                             const glm::mat4& vpMatrix,
                             const PushConstants& scenePC) {
    if (m_drops.empty()) return;

    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    auto addTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                      const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                      const glm::vec3& norm, const glm::vec3& litCol) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, uv0, norm, litCol});
        vertices.push_back({v1, uv1, norm, litCol});
        vertices.push_back({v2, uv2, norm, litCol});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    auto addQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                       const glm::vec4& uv, const glm::vec3& norm, const glm::vec3& litCol) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        // v0: bottom-left, v1: top-left, v2: top-right, v3: bottom-right
        vertices.push_back({v0, glm::vec2(uv.x, uv.w), norm, litCol});
        vertices.push_back({v1, glm::vec2(uv.x, uv.y), norm, litCol});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, litCol});
        vertices.push_back({v3, glm::vec2(uv.z, uv.w), norm, litCol});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    for (const auto& drop : m_drops) {
        if (drop.collected) continue;

        float bob = std::sin(drop.age * 4.0f) * 0.08f;
        glm::vec3 renderPos = drop.pos + glm::vec3(0.0f, bob, 0.0f);

        // 1. Dynamic Contact Shadow on the Ground beneath the floating drop
        float groundY = (drop.groundY > 0.0f) ? drop.groundY : (drop.pos.y - 0.15f);
        glm::vec3 shadowCenter = drop.pos;
        shadowCenter.y = groundY + 0.015f; // Sits right above top of ground block

        // Offset shadow slightly along light ray opposite direction
        glm::vec3 L(scenePC.sunDir[0], scenePC.sunDir[1], scenePC.sunDir[2]);
        if (L.y > 0.08f) {
            float hAboveGround = std::max(0.02f, renderPos.y - groundY);
            shadowCenter.x -= (L.x / L.y) * hAboveGround * 0.35f;
            shadowCenter.z -= (L.z / L.y) * hAboveGround * 0.35f;
        }

        float sRad = std::clamp(0.18f - bob * 0.03f, 0.12f, 0.24f);
        glm::vec4 uvShadow = TextureAtlas::getTileUV(TextureAtlas::TILE_SHADOW_DISC);
        glm::vec3 shadowCol(drop.inSunlight ? 0.30f : 0.08f, 0.85f, 0.0f);
        glm::vec3 nUp(0.0f, 1.0f, 0.0f);

        uint32_t sb = static_cast<uint32_t>(vertices.size());
        vertices.push_back({shadowCenter + glm::vec3(-sRad, 0.0f, -sRad), glm::vec2(uvShadow.x, uvShadow.w), nUp, shadowCol});
        vertices.push_back({shadowCenter + glm::vec3(-sRad, 0.0f,  sRad), glm::vec2(uvShadow.x, uvShadow.y), nUp, shadowCol});
        vertices.push_back({shadowCenter + glm::vec3( sRad, 0.0f,  sRad), glm::vec2(uvShadow.z, uvShadow.y), nUp, shadowCol});
        vertices.push_back({shadowCenter + glm::vec3( sRad, 0.0f, -sRad), glm::vec2(uvShadow.z, uvShadow.w), nUp, shadowCol});
        indices.push_back(sb + 0); indices.push_back(sb + 1); indices.push_back(sb + 2);
        indices.push_back(sb + 2); indices.push_back(sb + 3); indices.push_back(sb + 0);

        // 2. Render 3D Bobbing Drop Entity
        glm::vec3 itemLitCol = (drop.type == BlockType::Torch)
            ? glm::vec3(1.0f, 1.0f, 1.0f) // Emissive torch flame glows in darkness
            : glm::vec3(drop.inSunlight ? 1.0f : 0.20f, 0.95f, 0.0f);
        
        glm::mat4 model = glm::translate(glm::mat4(1.0f), renderPos);
        model = glm::rotate(model, drop.rotAngle, glm::vec3(0.0f, 1.0f, 0.0f));

        bool is2D = Cell{drop.type}.isItem() || Cell{drop.type}.isFoliage() || Cell{drop.type}.isTorch();
        if (is2D) {
            float hs = 0.22f;
            glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 0));
            
            // Quad 1: Front and Back faces
            glm::vec3 f0 = glm::vec3(model * glm::vec4(-hs, -hs, 0.0f, 1.0f));
            glm::vec3 f1 = glm::vec3(model * glm::vec4(-hs,  hs, 0.0f, 1.0f));
            glm::vec3 f2 = glm::vec3(model * glm::vec4( hs,  hs, 0.0f, 1.0f));
            glm::vec3 f3 = glm::vec3(model * glm::vec4( hs, -hs, 0.0f, 1.0f));
            glm::vec3 nFront = glm::normalize(glm::vec3(model * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
            addQuad(f0, f1, f2, f3, uv, nFront, itemLitCol);
            addQuad(f3, f2, f1, f0, uv, -nFront, itemLitCol);

            // Crossed Quad 2 (rotated 90 degrees for plants/torches)
            glm::mat4 m90 = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 c0 = glm::vec3(m90 * glm::vec4(-hs, -hs, 0.0f, 1.0f));
            glm::vec3 c1 = glm::vec3(m90 * glm::vec4(-hs,  hs, 0.0f, 1.0f));
            glm::vec3 c2 = glm::vec3(m90 * glm::vec4( hs,  hs, 0.0f, 1.0f));
            glm::vec3 c3 = glm::vec3(m90 * glm::vec4( hs, -hs, 0.0f, 1.0f));
            glm::vec3 nSide = glm::normalize(glm::vec3(m90 * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
            addQuad(c0, c1, c2, c3, uv, nSide, itemLitCol);
            addQuad(c3, c2, c1, c0, uv, -nSide, itemLitCol);
        } else {
            // 3D miniature equilateral triangular prism block
            float R = 0.16f;
            float H = 0.16f;
            float cos30 = 0.8660254f;
            glm::vec3 l0(0.0f, 0.0f, R);
            glm::vec3 l1(R * cos30, 0.0f, -R * 0.5f);
            glm::vec3 l2(-R * cos30, 0.0f, -R * 0.5f);

            auto xf = [&](const glm::vec3& p) {
                return glm::vec3(model * glm::vec4(p, 1.0f));
            };

            glm::vec3 t0 = xf(l0 + glm::vec3(0, H, 0));
            glm::vec3 t1 = xf(l1 + glm::vec3(0, H, 0));
            glm::vec3 t2 = xf(l2 + glm::vec3(0, H, 0));

            glm::vec3 b0 = xf(l0 - glm::vec3(0, H, 0));
            glm::vec3 b1 = xf(l1 - glm::vec3(0, H, 0));
            glm::vec3 b2 = xf(l2 - glm::vec3(0, H, 0));

            // Top Face (face 0): CCW winding (t0 -> t2 -> t1) viewed from outside/above
            glm::vec4 uvTop = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 0));
            float uMidTop = uvTop.x + (uvTop.z - uvTop.x) * 0.5f;
            glm::vec3 nTop = glm::normalize(glm::vec3(model * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
            addTri(t0, t2, t1,
                   glm::vec2(uMidTop, uvTop.y),
                   glm::vec2(uvTop.x, uvTop.w),
                   glm::vec2(uvTop.z, uvTop.w),
                   nTop, itemLitCol);

            // Bottom Face (face 1): CCW winding (b0 -> b1 -> b2) viewed from outside/below
            glm::vec4 uvBot = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 1));
            float uMidBot = uvBot.x + (uvBot.z - uvBot.x) * 0.5f;
            glm::vec3 nBot = glm::normalize(glm::vec3(model * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));
            addTri(b0, b1, b2,
                   glm::vec2(uMidBot, uvBot.w),
                   glm::vec2(uvBot.z, uvBot.y),
                   glm::vec2(uvBot.x, uvBot.y),
                   nBot, itemLitCol);

            // Side Wall 0 (between l0 and l1)
            glm::vec4 uvSide0 = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 2));
            glm::vec3 n0 = glm::normalize(glm::cross(t1 - t0, b0 - t0));
            addQuad(b0, t0, t1, b1, uvSide0, n0, itemLitCol);

            // Side Wall 1 (between l1 and l2)
            glm::vec4 uvSide1 = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 3));
            glm::vec3 n1 = glm::normalize(glm::cross(t2 - t1, b1 - t1));
            addQuad(b1, t1, t2, b2, uvSide1, n1, itemLitCol);

            // Side Wall 2 (between l2 and l0)
            glm::vec4 uvSide2 = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 4));
            glm::vec3 n2 = glm::normalize(glm::cross(t0 - t2, b2 - t2));
            addQuad(b2, t2, t0, b0, uvSide2, n2, itemLitCol);
        }
    }

    if (vertices.empty()) return;

    uint32_t frameIndex = m_cmdQueue.getCurrentFrame();
    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    if (!m_vbo[frameIndex].isValid() || m_vbo[frameIndex].getSize() < vSize) {
        m_vbo[frameIndex] = Buffer(m_context, std::max(vSize, (VkDeviceSize)8192), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    if (!m_ibo[frameIndex].isValid() || m_ibo[frameIndex].getSize() < iSize) {
        m_ibo[frameIndex] = Buffer(m_context, std::max(iSize, (VkDeviceSize)2048), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    m_vbo[frameIndex].upload(vertices.data(), vSize);
    m_ibo[frameIndex].upload(indices.data(), iSize);

    m_indexCount[frameIndex] = static_cast<uint32_t>(indices.size());

    VkBuffer vbs[] = {m_vbo[frameIndex].getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo[frameIndex].getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    PushConstants pc = scenePC;
    std::memcpy(pc.mvp, &vpMatrix[0][0], sizeof(float) * 16);
    pc.skyFog[3] = 400.0f; // Prevent dropped items from ever getting black distance fog
    vkCmdPushConstants(cmd, pipeline.getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

    vkCmdDrawIndexed(cmd, m_indexCount[frameIndex], 1, 0, 0, 0);
}

} // namespace prismcraft
