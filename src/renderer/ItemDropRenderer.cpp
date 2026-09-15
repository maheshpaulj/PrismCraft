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
    // Add small randomized horizontal drift if default upward velocity was passed
    if (std::abs(vel.x) < 0.001f && std::abs(vel.z) < 0.001f && vel.y > 0.1f) {
        float rx = ((rand() % 100) / 50.0f - 1.0f) * 0.35f;
        float rz = ((rand() % 100) / 50.0f - 1.0f) * 0.35f;
        drop.vel = glm::vec3(rx, vel.y, rz);
    } else {
        drop.vel = vel;
    }
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

        CellCoord c = worldToCell(glm::vec3(drop.pos.x, 0.0f, drop.pos.z));

        // 1. Ceiling collision: bounce down if colliding with solid block above when rising
        if (drop.vel.y > 0.0f) {
            int ceilY = static_cast<int>(std::floor(drop.pos.y + 0.25f));
            if (ceilY < CHUNK_SIZE_Y) {
                Cell ceilCell = world.getCell(c.x, ceilY, c.z, c.s);
                if (ceilCell.isSolid()) {
                    drop.pos.y = static_cast<float>(ceilY) - 0.26f;
                    drop.vel.y = -0.4f; // bounce downwards
                }
            }
        }

        // 2. Floor collision: search strictly downwards from current item level
        // Items will NEVER check blocks above them, preventing teleportation to tree canopies or cave roofs!
        int checkStartY = std::clamp(static_cast<int>(std::floor(drop.pos.y)), 0, CHUNK_SIZE_Y - 1);
        float localGroundY = 0.0f;
        for (int y = checkStartY; y >= 0; --y) {
            Cell cell = world.getCell(c.x, y, c.z, c.s);
            if (cell.isSolid()) {
                localGroundY = static_cast<float>(y + 1);
                break;
            }
        }

        drop.groundY = localGroundY;
        float skyGroundY = world.getHighestSolidY(drop.pos.x, drop.pos.z);
        drop.inSunlight = (drop.pos.y >= skyGroundY - 0.2f);

        if (drop.pos.y < localGroundY + 0.15f) {
            drop.pos.y = localGroundY + 0.15f;
            drop.vel.y = 0.0f;
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
        // Outward-facing counter-clockwise winding (BL -> TL -> TR -> BR -> BL)
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
        glm::vec3 itemLitCol = Cell{drop.type}.isTorch()
            ? glm::vec3(1.0f, 1.0f, 1.0f) // Emissive torch flame glows in darkness
            : glm::vec3(drop.inSunlight ? 1.0f : 0.20f, 0.95f, 0.0f);
        
        glm::mat4 model = glm::translate(glm::mat4(1.0f), renderPos);
        model = glm::rotate(model, drop.rotAngle, glm::vec3(0.0f, 1.0f, 0.0f));

        if (Cell{drop.type}.isTorch()) {
            // 3D miniature standing/spinning torch cuboid
            float hw = 0.026f;
            float hTotal = 0.28f;
            float yBase = -0.14f;
            glm::vec4 uvTorch = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 0));
            float uSpan = (uvTorch.z - uvTorch.x);
            float vSpan = (uvTorch.w - uvTorch.y);

            glm::vec4 uvSide(uvTorch.x + uSpan * (7.0f / 16.0f),
                             uvTorch.y + vSpan * (6.0f / 16.0f),
                             uvTorch.x + uSpan * (9.0f / 16.0f),
                             uvTorch.w);
            glm::vec4 uvTop(uvTorch.x + uSpan * (7.0f / 16.0f),
                            uvTorch.y + vSpan * (6.0f / 16.0f),
                            uvTorch.x + uSpan * (9.0f / 16.0f),
                            uvTorch.y + vSpan * (8.0f / 16.0f));
            glm::vec4 uvBot(uvTorch.x + uSpan * (7.0f / 16.0f),
                            uvTorch.y + vSpan * (14.0f / 16.0f),
                            uvTorch.x + uSpan * (9.0f / 16.0f),
                            uvTorch.w);

            auto xf = [&](const glm::vec3& p) { return glm::vec3(model * glm::vec4(p, 1.0f)); };
            glm::vec3 flameCol(1.0f, 1.0f, 1.0f);
            if (drop.type == BlockType::TorchSoul) {
                flameCol = glm::vec3(0.3f, 0.95f, 1.0f);
            } else if (drop.type == BlockType::TorchRedstone) {
                flameCol = glm::vec3(1.0f, 0.2f, 0.2f);
            }

            // Bottom cap (-Y)
            addQuad(xf({-hw, yBase,  hw}), xf({-hw, yBase, -hw}), xf({ hw, yBase, -hw}), xf({ hw, yBase,  hw}), uvBot, glm::vec3(model * glm::vec4(0, -1, 0, 0)), flameCol);
            addQuad(xf({-hw, yBase, -hw}), xf({-hw, yBase,  hw}), xf({ hw, yBase,  hw}), xf({ hw, yBase, -hw}), uvBot, glm::vec3(model * glm::vec4(0, 1, 0, 0)), flameCol);
            // Top cap (+Y)
            addQuad(xf({-hw, yBase + hTotal, -hw}), xf({-hw, yBase + hTotal,  hw}), xf({ hw, yBase + hTotal,  hw}), xf({ hw, yBase + hTotal, -hw}), uvTop, glm::vec3(model * glm::vec4(0, 1, 0, 0)), flameCol);
            addQuad(xf({-hw, yBase + hTotal,  hw}), xf({-hw, yBase + hTotal, -hw}), xf({ hw, yBase + hTotal, -hw}), xf({ hw, yBase + hTotal,  hw}), uvTop, glm::vec3(model * glm::vec4(0, -1, 0, 0)), flameCol);
            // Front face (+Z)
            addQuad(xf({ hw, yBase,  hw}), xf({ hw, yBase + hTotal,  hw}), xf({-hw, yBase + hTotal,  hw}), xf({-hw, yBase,  hw}), uvSide, glm::vec3(model * glm::vec4(0, 0, 1, 0)), flameCol);
            // Back face (-Z)
            addQuad(xf({-hw, yBase, -hw}), xf({-hw, yBase + hTotal, -hw}), xf({ hw, yBase + hTotal, -hw}), xf({ hw, yBase, -hw}), uvSide, glm::vec3(model * glm::vec4(0, 0, -1, 0)), flameCol);
            // Right face (+X)
            addQuad(xf({ hw, yBase, -hw}), xf({ hw, yBase + hTotal, -hw}), xf({ hw, yBase + hTotal,  hw}), xf({ hw, yBase,  hw}), uvSide, glm::vec3(model * glm::vec4(1, 0, 0, 0)), flameCol);
            // Left face (-X)
            addQuad(xf({-hw, yBase,  hw}), xf({-hw, yBase + hTotal,  hw}), xf({-hw, yBase + hTotal, -hw}), xf({-hw, yBase, -hw}), uvSide, glm::vec3(model * glm::vec4(-1, 0, 0, 0)), flameCol);
        } else if (Cell{drop.type}.isFlatItem()) {
            // True 3D Voxel Extruded Item Drop
            uint32_t tileId = TextureAtlas::getTileForBlock(drop.type, 0);
            bool occupied[16][16];
            glm::vec4 pixelColor[16][16];
            for (int py = 0; py < 16; ++py) {
                for (int px = 0; px < 16; ++px) {
                    occupied[py][px] = TextureAtlas::getTilePixel(tileId, px, py, pixelColor[py][px]);
                }
            }

            float totalSize = 0.32f;
            float voxelSize = totalSize / 16.0f;
            float voxelThick = 0.024f;
            float zMin = -voxelThick * 0.5f;
            float zMax =  voxelThick * 0.5f;

            int col = tileId % TextureAtlas::TILES_PER_ROW;
            int row = tileId / TextureAtlas::TILES_PER_ROW;

            auto xf = [&](const glm::vec3& p) { return glm::vec3(model * glm::vec4(p, 1.0f)); };

            for (int py = 0; py < 16; ++py) {
                for (int px = 0; px < 16; ++px) {
                    if (!occupied[py][px]) continue;

                    float x0 = (static_cast<float>(px) - 8.0f) * voxelSize;
                    float x1 = x0 + voxelSize;
                    float y1 = (8.0f - static_cast<float>(py)) * voxelSize;
                    float y0 = y1 - voxelSize;

                    int atlasX = col * TextureAtlas::TILE_SIZE + px;
                    int atlasY = row * TextureAtlas::TILE_SIZE + py;
                    float u0 = static_cast<float>(atlasX) / static_cast<float>(TextureAtlas::ATLAS_WIDTH);
                    float v0 = static_cast<float>(atlasY) / static_cast<float>(TextureAtlas::ATLAS_HEIGHT);
                    float u1 = static_cast<float>(atlasX + 1) / static_cast<float>(TextureAtlas::ATLAS_WIDTH);
                    float v1 = static_cast<float>(atlasY + 1) / static_cast<float>(TextureAtlas::ATLAS_HEIGHT);
                    glm::vec4 uv(u0, v0, u1, v1);

                    // Front face (+Z) - CCW: BL -> TL -> TR -> BR
                    addQuad(xf({x0, y0, zMax}), xf({x0, y1, zMax}), xf({x1, y1, zMax}), xf({x1, y0, zMax}), uv, glm::vec3(model * glm::vec4(0, 0, 1, 0)), itemLitCol);

                    // Back face (-Z) - CCW: BL -> TL -> TR -> BR
                    addQuad(xf({x1, y0, zMin}), xf({x1, y1, zMin}), xf({x0, y1, zMin}), xf({x0, y0, zMin}), uv, glm::vec3(model * glm::vec4(0, 0, -1, 0)), itemLitCol);

                    // Top edge (+Y)
                    if (py == 0 || !occupied[py - 1][px]) {
                        addQuad(xf({x0, y1, zMax}), xf({x0, y1, zMin}), xf({x1, y1, zMin}), xf({x1, y1, zMax}), uv, glm::vec3(model * glm::vec4(0, 1, 0, 0)), itemLitCol);
                    }
                    // Bottom edge (-Y)
                    if (py == 15 || !occupied[py + 1][px]) {
                        addQuad(xf({x0, y0, zMin}), xf({x0, y0, zMax}), xf({x1, y0, zMax}), xf({x1, y0, zMin}), uv, glm::vec3(model * glm::vec4(0, -1, 0, 0)), itemLitCol);
                    }
                    // Left edge (-X)
                    if (px == 0 || !occupied[py][px - 1]) {
                        addQuad(xf({x0, y0, zMin}), xf({x0, y1, zMin}), xf({x0, y1, zMax}), xf({x0, y0, zMax}), uv, glm::vec3(model * glm::vec4(-1, 0, 0, 0)), itemLitCol);
                    }
                    // Right edge (+X)
                    if (px == 15 || !occupied[py][px + 1]) {
                        addQuad(xf({x1, y0, zMax}), xf({x1, y1, zMax}), xf({x1, y1, zMin}), xf({x1, y0, zMin}), uv, glm::vec3(model * glm::vec4(1, 0, 0, 0)), itemLitCol);
                    }
                }
            }
        } else if (Cell{drop.type}.isFoliage()) {
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

            // Crossed Quad 2 (rotated 90 degrees for plants/foliage)
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

            // Top Face (face 0): Double-sided for complete robustness against culling and viewport flips
            glm::vec4 uvTop = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 0));
            glm::vec3 nTop = glm::normalize(glm::vec3(model * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
            glm::vec2 uvTop0((uvTop.x + uvTop.z) * 0.5f, uvTop.y);
            glm::vec2 uvTop1(uvTop.z, uvTop.w);
            glm::vec2 uvTop2(uvTop.x, uvTop.w);
            addTri(t0, t1, t2, uvTop0, uvTop1, uvTop2, nTop, itemLitCol);
            addTri(t0, t2, t1, uvTop0, uvTop2, uvTop1, nTop, itemLitCol);

            // Bottom Face (face 1): Double-sided for complete robustness against culling and viewport flips
            glm::vec4 uvBot = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 1));
            glm::vec3 nBot = glm::normalize(glm::vec3(model * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));
            glm::vec2 uvBot0((uvBot.x + uvBot.z) * 0.5f, uvBot.y);
            glm::vec2 uvBot1(uvBot.z, uvBot.w);
            glm::vec2 uvBot2(uvBot.x, uvBot.w);
            addTri(b0, b2, b1, uvBot0, uvBot2, uvBot1, nBot, itemLitCol);
            addTri(b0, b1, b2, uvBot0, uvBot1, uvBot2, nBot, itemLitCol);

            // Side Wall 0 (between l1 and l0): Left is l1, Right is l0. CCW from outside: b1 -> t1 -> t0 -> b0
            glm::vec4 uvSide0 = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 2));
            glm::vec3 n0 = glm::normalize(glm::cross(b0 - b1, t1 - b1));
            addQuad(b1, t1, t0, b0, uvSide0, n0, itemLitCol);

            // Side Wall 1 (between l2 and l1): Left is l2, Right is l1. CCW from outside: b2 -> t2 -> t1 -> b1
            glm::vec4 uvSide1 = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 3));
            glm::vec3 n1 = glm::normalize(glm::cross(b1 - b2, t2 - b2));
            addQuad(b2, t2, t1, b1, uvSide1, n1, itemLitCol);

            // Side Wall 2 (between l0 and l2): Left is l0, Right is l2. CCW from outside: b0 -> t0 -> t2 -> b2
            glm::vec4 uvSide2 = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(drop.type, 4));
            glm::vec3 n2 = glm::normalize(glm::cross(b2 - b0, t0 - b0));
            addQuad(b0, t0, t2, b2, uvSide2, n2, itemLitCol);
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
