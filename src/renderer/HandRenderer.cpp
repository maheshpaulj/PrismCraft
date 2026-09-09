#include "HandRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "player/Player.hpp"
#include "world/ChunkMesher.hpp"
#include "TextureAtlas.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstring>

namespace prismcraft {

HandRenderer::HandRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
    buildHandMesh();
}

void HandRenderer::buildHandMesh() {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec3 sleeveColor(0.08f, 0.65f, 0.72f); // Steve's cyan/teal shirt
    glm::vec3 skinColor(0.88f, 0.68f, 0.54f);   // Steve's warm skin tone
    glm::vec4 shirtPx, skinPx;
    if (TextureAtlas::getTilePixel(135, 8, 6, shirtPx) && shirtPx.a > 0.5f) {
        sleeveColor = glm::vec3(shirtPx.r, shirtPx.g, shirtPx.b);
    }
    if (TextureAtlas::getTilePixel(141, 8, 8, skinPx) && skinPx.a > 0.5f) {
        skinColor = glm::vec3(skinPx.r, skinPx.g, skinPx.b);
    }
    glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE); // Solid white tile

    auto addQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& norm, const glm::vec3& color) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, glm::vec2(uv.x, uv.w), norm, color});
        vertices.push_back({v1, glm::vec2(uv.z, uv.w), norm, color});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, color});
        vertices.push_back({v3, glm::vec2(uv.x, uv.y), norm, color});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    float armW = 0.052f;
    float armH = 0.065f;
    float zBack = 0.25f;
    float zWrist = -0.12f;
    float zFingers = -0.32f;

    // Sleeve section
    addQuad({-armW, armH, zBack}, {armW, armH, zBack}, {armW, armH, zWrist}, {-armW, armH, zWrist}, glm::vec3(0, 1, 0), sleeveColor * 1.0f);
    addQuad({-armW, -armH, zWrist}, {armW, -armH, zWrist}, {armW, -armH, zBack}, {-armW, -armH, zBack}, glm::vec3(0, -1, 0), sleeveColor * 0.6f);
    addQuad({armW, -armH, zBack}, {armW, -armH, zWrist}, {armW, armH, zWrist}, {armW, armH, zBack}, glm::vec3(1, 0, 0), sleeveColor * 0.8f);
    addQuad({-armW, -armH, zWrist}, {-armW, -armH, zBack}, {-armW, armH, zBack}, {-armW, armH, zWrist}, glm::vec3(-1, 0, 0), sleeveColor * 0.75f);

    // Hand/Fingers section
    addQuad({-armW*0.9f, armH*0.9f, zWrist}, {armW*0.9f, armH*0.9f, zWrist}, {armW*0.9f, armH*0.9f, zFingers}, {-armW*0.9f, armH*0.9f, zFingers}, glm::vec3(0, 1, 0), skinColor * 1.0f);
    addQuad({-armW*0.9f, -armH*0.9f, zFingers}, {armW*0.9f, -armH*0.9f, zFingers}, {armW*0.9f, -armH*0.9f, zWrist}, {-armW*0.9f, -armH*0.9f, zWrist}, glm::vec3(0, -1, 0), skinColor * 0.6f);
    addQuad({armW*0.9f, -armH*0.9f, zWrist}, {armW*0.9f, -armH*0.9f, zFingers}, {armW*0.9f, armH*0.9f, zFingers}, {armW*0.9f, armH*0.9f, zWrist}, glm::vec3(1, 0, 0), skinColor * 0.8f);
    addQuad({-armW*0.9f, -armH*0.9f, zFingers}, {-armW*0.9f, -armH*0.9f, zWrist}, {-armW*0.9f, armH*0.9f, zWrist}, {-armW*0.9f, armH*0.9f, zFingers}, glm::vec3(-1, 0, 0), skinColor * 0.75f);
    addQuad({-armW*0.9f, -armH*0.9f, zFingers}, {armW*0.9f, -armH*0.9f, zFingers}, {armW*0.9f, armH*0.9f, zFingers}, {-armW*0.9f, armH*0.9f, zFingers}, glm::vec3(0, 0, -1), skinColor * 0.85f);

    m_handIndexCount = static_cast<uint32_t>(indices.size());
    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_handVertexBuffer = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_handVertexBuffer.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_handIndexBuffer = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_handIndexBuffer.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
}

void HandRenderer::buildHeldBlockMesh(BlockType type) {
    if (type == BlockType::Air) {
        m_blockIndexCount = 0;
        m_lastHeldBlock = type;
        return;
    }

    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    float s = 0.13f; // Miniature block scale

    auto addQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                       const glm::vec4& uv, const glm::vec3& norm, const glm::vec3& color) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, glm::vec2(uv.x, uv.w), norm, color});
        vertices.push_back({v1, glm::vec2(uv.z, uv.w), norm, color});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, color});
        vertices.push_back({v3, glm::vec2(uv.x, uv.y), norm, color});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    auto addTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                      const glm::vec4& uv, const glm::vec3& norm, const glm::vec3& color) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, glm::vec2(uv.x, uv.y), norm, color});
        vertices.push_back({v1, glm::vec2(uv.x, uv.w), norm, color});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, color});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    if (Cell{type}.isItem()) {
        uint32_t tileId = TextureAtlas::getTileForBlock(type, 0);

        bool occupied[16][16];
        glm::vec4 pixelColor[16][16];
        for (int py = 0; py < 16; ++py) {
            for (int px = 0; px < 16; ++px) {
                occupied[py][px] = TextureAtlas::getTilePixel(tileId, px, py, pixelColor[py][px]);
            }
        }

        float totalSize = 0.36f; // Big and good 3D tool model
        float voxelSize = totalSize / 16.0f;
        float voxelThick = 0.024f; // 3D extrusion thickness
        float zMin = -voxelThick * 0.5f;
        float zMax = voxelThick * 0.5f;

        int col = tileId % TextureAtlas::TILES_PER_ROW;
        int row = tileId / TextureAtlas::TILES_PER_ROW;

        for (int py = 0; py < 16; ++py) {
            for (int px = 0; px < 16; ++px) {
                if (!occupied[py][px]) continue;

                // Center coordinates so sprite center is at (0, 0)
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

                glm::vec3 cFront(1.0f);
                glm::vec3 cBack(0.85f);
                glm::vec3 cTop(0.95f);
                glm::vec3 cBot(0.65f);
                glm::vec3 cLeft(0.75f);
                glm::vec3 cRight(0.85f);

                // Front face (+Z)
                addQuad({x0, y0, zMax}, {x1, y0, zMax}, {x1, y1, zMax}, {x0, y1, zMax}, uv, glm::vec3(0, 0, 1), cFront);

                // Back face (-Z)
                uint32_t b = static_cast<uint32_t>(vertices.size());
                vertices.push_back({{x1, y0, zMin}, glm::vec2(uv.z, uv.w), glm::vec3(0, 0, -1), cBack});
                vertices.push_back({{x0, y0, zMin}, glm::vec2(uv.x, uv.w), glm::vec3(0, 0, -1), cBack});
                vertices.push_back({{x0, y1, zMin}, glm::vec2(uv.x, uv.y), glm::vec3(0, 0, -1), cBack});
                vertices.push_back({{x1, y1, zMin}, glm::vec2(uv.z, uv.y), glm::vec3(0, 0, -1), cBack});
                indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
                indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);

                // Top edge (+Y)
                if (py == 0 || !occupied[py - 1][px]) {
                    addQuad({x0, y1, zMax}, {x1, y1, zMax}, {x1, y1, zMin}, {x0, y1, zMin}, uv, glm::vec3(0, 1, 0), cTop);
                }
                // Bottom edge (-Y)
                if (py == 15 || !occupied[py + 1][px]) {
                    addQuad({x0, y0, zMin}, {x1, y0, zMin}, {x1, y0, zMax}, {x0, y0, zMax}, uv, glm::vec3(0, -1, 0), cBot);
                }
                // Left edge (-X)
                if (px == 0 || !occupied[py][px - 1]) {
                    addQuad({x0, y0, zMin}, {x0, y0, zMax}, {x0, y1, zMax}, {x0, y1, zMin}, uv, glm::vec3(-1, 0, 0), cLeft);
                }
                // Right edge (+X)
                if (px == 15 || !occupied[py][px + 1]) {
                    addQuad({x1, y0, zMax}, {x1, y0, zMin}, {x1, y1, zMin}, {x1, y1, zMax}, uv, glm::vec3(1, 0, 0), cRight);
                }
            }
        }

        m_blockIndexCount = static_cast<uint32_t>(indices.size());
        VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
        VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

        m_blockVertexBuffer = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_blockVertexBuffer.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

        m_blockIndexBuffer = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_blockIndexBuffer.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);

        m_lastHeldBlock = type;
        return;
    }

    glm::vec4 uvTop = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(type, 0));
    glm::vec4 uvBot = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(type, 1));
    glm::vec4 uvSide = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(type, 2));

    glm::vec3 cTop(1.0f);
    glm::vec3 cBot(0.55f);
    glm::vec3 cSide(0.80f);

    // Miniature triangular prism
    addTri({-s, s, -s}, {-s, s, s}, {s, s, -s}, uvTop, glm::vec3(0, 1, 0), cTop);
    addTri({-s, -s, -s}, {s, -s, -s}, {-s, -s, s}, uvBot, glm::vec3(0, -1, 0), cBot);
    addQuad({s, -s, -s}, {-s, -s, -s}, {-s, s, -s}, {s, s, -s}, uvSide, glm::vec3(0, 0, -1), cSide);
    addQuad({-s, -s, -s}, {-s, -s, s}, {-s, s, s}, {-s, s, -s}, uvSide, glm::vec3(-1, 0, 0), cSide);
    addQuad({-s, -s, s}, {s, -s, -s}, {s, s, -s}, {-s, s, s}, uvSide, glm::normalize(glm::vec3(1, 0, 1)), cSide);

    m_blockIndexCount = static_cast<uint32_t>(indices.size());
    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_blockVertexBuffer = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_blockVertexBuffer.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_blockIndexBuffer = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_blockIndexBuffer.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);

    m_lastHeldBlock = type;
}

void HandRenderer::render(VkCommandBuffer cmd,
                          const Pipeline& pipeline,
                          const Player& player,
                          float aspectRatio) {
    BlockType currentBlock = player.getSelectedBlock();
    if (currentBlock != m_lastHeldBlock) {
        buildHeldBlockMesh(currentBlock);
    }

    glm::mat4 proj = glm::perspective(glm::radians(70.0f), aspectRatio, 0.05f, 10.0f);

    float bobTime = player.getBobTime();
    float bobWeight = player.getBobWeight();
    float swayX = std::sin(bobTime) * 0.035f * bobWeight;
    float swayY = -std::abs(std::cos(bobTime)) * 0.035f * bobWeight;

    // Swing punch animation
    float swingProgress = player.getSwingProgress();
    float swing = 0.0f;
    if (swingProgress > 0.0f) {
        swing = std::sin(swingProgress * 3.14159265f);
    }

    // Place recoil animation
    float placeProgress = player.getPlaceProgress();
    float place = 0.0f;
    if (placeProgress > 0.0f) {
        place = std::sin(placeProgress * 3.14159265f);
    }

    bool isItem = Cell{currentBlock}.isItem();
    bool isSword = Cell{currentBlock}.isSword();
    bool isPickaxe = Cell{currentBlock}.isPickaxe();
    bool isAxe = Cell{currentBlock}.isAxe();
    bool isShovel = Cell{currentBlock}.isShovel();
    bool isBow = Cell{currentBlock}.isBow();
    bool isBlocking = player.isBlocking() && isSword;

    glm::vec3 handPos(0.35f + swayX - swing * 0.08f, 
                      -0.27f + swayY + swing * 0.05f - place * 0.06f, 
                      -0.52f - swing * 0.12f + place * 0.05f);

    glm::mat4 handModel = glm::mat4(1.0f);
    if (isBlocking) {
        // Authentic Minecraft sword blocking pose: angled defensively across the chest
        handModel = glm::translate(handModel, glm::vec3(0.12f + swayX * 0.2f, -0.20f + swayY * 0.2f, -0.42f));
        handModel = glm::rotate(handModel, glm::radians(-35.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        handModel = glm::rotate(handModel, glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        handModel = glm::rotate(handModel, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    } else {
        handModel = glm::translate(handModel, handPos);
        handModel = glm::rotate(handModel, glm::radians(-15.0f + swing * 40.0f - place * 15.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        handModel = glm::rotate(handModel, glm::radians(25.0f - swing * 30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        handModel = glm::rotate(handModel, glm::radians(-10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    PushConstants pc{};
    pc.sunDir[0] = 0.5f; pc.sunDir[1] = 0.8f; pc.sunDir[2] = 0.3f; pc.sunDir[3] = 1.0f;
    pc.lightColor[0] = 0.7f; pc.lightColor[1] = 0.7f; pc.lightColor[2] = 0.7f;
    pc.skyFog[3] = 1000.0f;

    // 1. Draw hand arm
    if (m_handIndexCount > 0 && m_handVertexBuffer.isValid()) {
        glm::mat4 mvp = proj * handModel;
        std::memcpy(pc.mvp, &mvp[0][0], sizeof(float) * 16);
        vkCmdPushConstants(cmd, pipeline.getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &pc);

        VkBuffer vbs[] = {m_handVertexBuffer.getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
        vkCmdBindIndexBuffer(cmd, m_handIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_handIndexCount, 1, 0, 0, 0);
    }

    // 2. Draw held block or 3D extruded tool at tip of hand
    if (m_blockIndexCount > 0 && m_blockVertexBuffer.isValid()) {
        glm::mat4 itemModel = handModel;

        if (isBlocking) {
            // Authentic Minecraft sword block stance
            itemModel = glm::translate(itemModel, glm::vec3(-0.02f, 0.10f, -0.28f));
            itemModel = glm::rotate(itemModel, glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            itemModel = glm::rotate(itemModel, glm::radians(-35.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        } else if (isBow) {
            float bowCharge = player.getBowCharge();
            if (bowCharge > 0.0f) {
                // Bow drawing / charge animation: pull back and center slightly with trembling
                float shake = (bowCharge >= 1.0f) ? (std::sin(player.getBobTime() * 45.0f) * 0.003f) : 0.0f;
                itemModel = glm::mat4(1.0f);
                itemModel = glm::translate(itemModel, glm::vec3(0.22f - bowCharge * 0.06f + shake, -0.19f + shake, -0.36f + bowCharge * 0.08f));
                itemModel = glm::rotate(itemModel, glm::radians(-12.0f - bowCharge * 15.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                itemModel = glm::rotate(itemModel, glm::radians(35.0f - bowCharge * 20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                itemModel = glm::rotate(itemModel, glm::radians(-15.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            } else {
                itemModel = glm::translate(itemModel, glm::vec3(0.0f, 0.06f, -0.30f));
                itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                itemModel = glm::rotate(itemModel, glm::radians(-20.0f + swing * 40.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            }
        } else if (isSword) {
            // Sword: wide circular slash arc sweeping across the screen
            itemModel = glm::translate(itemModel, glm::vec3(0.0f, 0.08f, -0.30f));
            itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            itemModel = glm::rotate(itemModel, glm::radians(-25.0f + swing * 75.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            itemModel = glm::rotate(itemModel, glm::radians(25.0f - swing * 50.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            itemModel = glm::rotate(itemModel, glm::radians(swing * 30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        } else if (isPickaxe) {
            // Pickaxe: steep overhead downward mining slam / hacking strike
            itemModel = glm::translate(itemModel, glm::vec3(0.0f, 0.08f + swing * 0.04f, -0.30f - swing * 0.08f));
            itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            itemModel = glm::rotate(itemModel, glm::radians(-35.0f + swing * 85.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            itemModel = glm::rotate(itemModel, glm::radians(15.0f - swing * 15.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        } else if (isAxe) {
            // Axe: heavy vertical downward chopping blow
            itemModel = glm::translate(itemModel, glm::vec3(-swing * 0.03f, 0.07f + swing * 0.03f, -0.30f - swing * 0.06f));
            itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            itemModel = glm::rotate(itemModel, glm::radians(-30.0f + swing * 70.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            itemModel = glm::rotate(itemModel, glm::radians(20.0f - swing * 20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        } else if (isShovel) {
            // Shovel: forward scooping thrust
            itemModel = glm::translate(itemModel, glm::vec3(0.0f, 0.06f - swing * 0.05f, -0.28f - swing * 0.10f));
            itemModel = glm::rotate(itemModel, glm::radians(40.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            itemModel = glm::rotate(itemModel, glm::radians(-15.0f + swing * 45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            itemModel = glm::rotate(itemModel, glm::radians(25.0f - swing * 15.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        } else if (isItem) {
            // Other items (stick, coal, iron ingot, diamond, etc.)
            itemModel = glm::translate(itemModel, glm::vec3(0.0f, 0.07f, -0.28f));
            itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            itemModel = glm::rotate(itemModel, glm::radians(-20.0f + swing * 55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            itemModel = glm::rotate(itemModel, glm::radians(20.0f - swing * 25.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        } else {
            // Miniature 3D triangular block
            itemModel = glm::translate(itemModel, glm::vec3(-0.04f, 0.05f, -0.28f));
            itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            itemModel = glm::rotate(itemModel, glm::radians(25.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        }

        glm::mat4 mvp = proj * itemModel;
        std::memcpy(pc.mvp, &mvp[0][0], sizeof(float) * 16);
        vkCmdPushConstants(cmd, pipeline.getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &pc);

        VkBuffer vbs[] = {m_blockVertexBuffer.getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
        vkCmdBindIndexBuffer(cmd, m_blockIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_blockIndexCount, 1, 0, 0, 0);
    }
}

} // namespace prismcraft
