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
    buildLocalHandMesh();
}

void HandRenderer::buildLocalHandMesh() {
    m_localHandVertices.clear();
    m_localHandIndices.clear();

    glm::vec4 uvSleeve = TextureAtlas::getTileUV(TextureAtlas::TILE_STEVE_TORSO_FRONT);
    glm::vec4 uvArm    = TextureAtlas::getTileUV(TextureAtlas::TILE_STEVE_ARM);
    glm::vec4 uvSkin   = TextureAtlas::getTileUV(TextureAtlas::TILE_STEVE_SKIN_TONE);

    auto addQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                       const glm::vec3& norm, const glm::vec4& uv) {
        uint32_t b = static_cast<uint32_t>(m_localHandVertices.size());
        m_localHandVertices.push_back({v0, glm::vec2(uv.x, uv.w), norm, glm::vec3(1.0f)});
        m_localHandVertices.push_back({v1, glm::vec2(uv.z, uv.w), norm, glm::vec3(1.0f)});
        m_localHandVertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, glm::vec3(1.0f)});
        m_localHandVertices.push_back({v3, glm::vec2(uv.x, uv.y), norm, glm::vec3(1.0f)});
        m_localHandIndices.push_back(b + 0); m_localHandIndices.push_back(b + 1); m_localHandIndices.push_back(b + 2);
        m_localHandIndices.push_back(b + 2); m_localHandIndices.push_back(b + 3); m_localHandIndices.push_back(b + 0);
    };

    float armW = 0.052f;
    float armH = 0.065f;
    float zBack = 0.25f;
    float zWrist = -0.12f;
    float zFingers = -0.32f;

    // Sleeve section (Steve's cyan shirt sleeve)
    addQuad({-armW,  armH, zBack},  { armW,  armH, zBack},  { armW,  armH, zWrist}, {-armW,  armH, zWrist}, glm::vec3( 0,  1,  0), uvSleeve);
    addQuad({-armW, -armH, zWrist}, { armW, -armH, zWrist}, { armW, -armH, zBack},  {-armW, -armH, zBack},  glm::vec3( 0, -1,  0), uvSleeve);
    addQuad({ armW, -armH, zBack},  { armW, -armH, zWrist}, { armW,  armH, zWrist}, { armW,  armH, zBack},  glm::vec3( 1,  0,  0), uvArm);
    addQuad({-armW, -armH, zWrist}, {-armW, -armH, zBack},  {-armW,  armH, zBack},  {-armW,  armH, zWrist}, glm::vec3(-1,  0,  0), uvArm);

    // Hand/Fingers section (Steve's natural skin tone)
    addQuad({-armW*0.9f,  armH*0.9f, zWrist},   { armW*0.9f,  armH*0.9f, zWrist},   { armW*0.9f,  armH*0.9f, zFingers}, {-armW*0.9f,  armH*0.9f, zFingers}, glm::vec3( 0,  1,  0), uvSkin);
    addQuad({-armW*0.9f, -armH*0.9f, zFingers}, { armW*0.9f, -armH*0.9f, zFingers}, { armW*0.9f, -armH*0.9f, zWrist},   {-armW*0.9f, -armH*0.9f, zWrist},   glm::vec3( 0, -1,  0), uvSkin);
    addQuad({ armW*0.9f, -armH*0.9f, zWrist},   { armW*0.9f, -armH*0.9f, zFingers}, { armW*0.9f,  armH*0.9f, zFingers}, { armW*0.9f,  armH*0.9f, zWrist},   glm::vec3( 1,  0,  0), uvSkin);
    addQuad({-armW*0.9f, -armH*0.9f, zFingers}, {-armW*0.9f, -armH*0.9f, zWrist},   {-armW*0.9f,  armH*0.9f, zWrist},   {-armW*0.9f,  armH*0.9f, zFingers}, glm::vec3(-1,  0,  0), uvSkin);
    addQuad({-armW*0.9f, -armH*0.9f, zFingers}, { armW*0.9f, -armH*0.9f, zFingers}, { armW*0.9f,  armH*0.9f, zFingers}, {-armW*0.9f,  armH*0.9f, zFingers}, glm::vec3( 0,  0, -1), uvSkin);
}

void HandRenderer::buildLocalHeldBlockMesh(BlockType type) {
    m_localItemVertices.clear();
    m_localItemIndices.clear();
    m_lastHeldBlock = type;

    if (type == BlockType::Air) {
        return;
    }

    auto addQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                       const glm::vec4& uv, const glm::vec3& norm) {
        uint32_t b = static_cast<uint32_t>(m_localItemVertices.size());
        m_localItemVertices.push_back({v0, glm::vec2(uv.x, uv.w), norm, glm::vec3(1.0f)});
        m_localItemVertices.push_back({v1, glm::vec2(uv.z, uv.w), norm, glm::vec3(1.0f)});
        m_localItemVertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, glm::vec3(1.0f)});
        m_localItemVertices.push_back({v3, glm::vec2(uv.x, uv.y), norm, glm::vec3(1.0f)});
        m_localItemIndices.push_back(b + 0); m_localItemIndices.push_back(b + 1); m_localItemIndices.push_back(b + 2);
        m_localItemIndices.push_back(b + 2); m_localItemIndices.push_back(b + 3); m_localItemIndices.push_back(b + 0);
    };

    auto addTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                      const glm::vec4& uv, const glm::vec3& norm) {
        uint32_t b = static_cast<uint32_t>(m_localItemVertices.size());
        m_localItemVertices.push_back({v0, glm::vec2(uv.x, uv.y), norm, glm::vec3(1.0f)});
        m_localItemVertices.push_back({v1, glm::vec2(uv.x, uv.w), norm, glm::vec3(1.0f)});
        m_localItemVertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, glm::vec3(1.0f)});
        m_localItemIndices.push_back(b + 0); m_localItemIndices.push_back(b + 1); m_localItemIndices.push_back(b + 2);
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

        float totalSize = 0.36f; // Standard 3D tool dimensions
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

                // Front face (+Z)
                addQuad({x0, y0, zMax}, {x1, y0, zMax}, {x1, y1, zMax}, {x0, y1, zMax}, uv, glm::vec3(0, 0, 1));

                // Back face (-Z)
                uint32_t b = static_cast<uint32_t>(m_localItemVertices.size());
                m_localItemVertices.push_back({{x1, y0, zMin}, glm::vec2(uv.z, uv.w), glm::vec3(0, 0, -1), glm::vec3(1.0f)});
                m_localItemVertices.push_back({{x0, y0, zMin}, glm::vec2(uv.x, uv.w), glm::vec3(0, 0, -1), glm::vec3(1.0f)});
                m_localItemVertices.push_back({{x0, y1, zMin}, glm::vec2(uv.x, uv.y), glm::vec3(0, 0, -1), glm::vec3(1.0f)});
                m_localItemVertices.push_back({{x1, y1, zMin}, glm::vec2(uv.z, uv.y), glm::vec3(0, 0, -1), glm::vec3(1.0f)});
                m_localItemIndices.push_back(b + 0); m_localItemIndices.push_back(b + 1); m_localItemIndices.push_back(b + 2);
                m_localItemIndices.push_back(b + 2); m_localItemIndices.push_back(b + 3); m_localItemIndices.push_back(b + 0);

                // Top edge (+Y)
                if (py == 0 || !occupied[py - 1][px]) {
                    addQuad({x0, y1, zMax}, {x1, y1, zMax}, {x1, y1, zMin}, {x0, y1, zMin}, uv, glm::vec3(0, 1, 0));
                }
                // Bottom edge (-Y)
                if (py == 15 || !occupied[py + 1][px]) {
                    addQuad({x0, y0, zMin}, {x1, y0, zMin}, {x1, y0, zMax}, {x0, y0, zMax}, uv, glm::vec3(0, -1, 0));
                }
                // Left edge (-X)
                if (px == 0 || !occupied[py][px - 1]) {
                    addQuad({x0, y0, zMin}, {x0, y0, zMax}, {x0, y1, zMax}, {x0, y1, zMin}, uv, glm::vec3(-1, 0, 0));
                }
                // Right edge (+X)
                if (px == 15 || !occupied[py][px + 1]) {
                    addQuad({x1, y0, zMax}, {x1, y0, zMin}, {x1, y1, zMin}, {x1, y1, zMax}, uv, glm::vec3(1, 0, 0));
                }
            }
        }
        return;
    }

    float s = 0.13f; // Miniature block scale
    glm::vec4 uvTop  = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(type, 0));
    glm::vec4 uvBot  = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(type, 1));
    glm::vec4 uvSide = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(type, 2));

    // Miniature triangular prism
    addTri({-s,  s, -s}, {-s,  s,  s}, { s,  s, -s}, uvTop, glm::vec3(0, 1, 0));
    addTri({-s, -s, -s}, { s, -s, -s}, {-s, -s,  s}, uvBot, glm::vec3(0, -1, 0));
    addQuad({ s, -s, -s}, {-s, -s, -s}, {-s,  s, -s}, { s,  s, -s}, uvSide, glm::vec3(0, 0, -1));
    addQuad({-s, -s, -s}, {-s, -s,  s}, {-s,  s,  s}, {-s,  s, -s}, uvSide, glm::vec3(-1, 0, 0));
    addQuad({-s, -s,  s}, { s, -s, -s}, { s,  s, -s}, {-s,  s,  s}, uvSide, glm::normalize(glm::vec3(1, 0, 1)));
}

void HandRenderer::render(VkCommandBuffer cmd,
                          const Pipeline& pipeline,
                          const Player& player,
                          const glm::mat4& view,
                          const glm::mat4& proj,
                          const PushConstants& scenePC,
                          float skylight,
                          float torchlight) {
    BlockType currentBlock = player.getSelectedBlock();
    if (currentBlock != m_lastHeldBlock) {
        buildLocalHeldBlockMesh(currentBlock);
    }

    // Camera aspect ratio from projection matrix
    float aspect = std::abs(proj[1][1] / proj[0][0]);
    glm::mat4 handProj = glm::perspective(glm::radians(70.0f), aspect, 0.05f, 50.0f);

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

    glm::mat4 itemModel = handModel;
    if (isBlocking) {
        itemModel = glm::translate(itemModel, glm::vec3(-0.02f, 0.10f, -0.28f));
        itemModel = glm::rotate(itemModel, glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        itemModel = glm::rotate(itemModel, glm::radians(-35.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    } else if (isBow) {
        float bowCharge = player.getBowCharge();
        if (bowCharge > 0.0f) {
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
        itemModel = glm::translate(itemModel, glm::vec3(0.0f, 0.08f, -0.30f));
        itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        itemModel = glm::rotate(itemModel, glm::radians(-25.0f + swing * 75.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        itemModel = glm::rotate(itemModel, glm::radians(25.0f - swing * 50.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        itemModel = glm::rotate(itemModel, glm::radians(swing * 30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    } else if (isPickaxe) {
        itemModel = glm::translate(itemModel, glm::vec3(0.0f, 0.08f + swing * 0.04f, -0.30f - swing * 0.08f));
        itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        itemModel = glm::rotate(itemModel, glm::radians(-35.0f + swing * 85.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        itemModel = glm::rotate(itemModel, glm::radians(15.0f - swing * 15.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    } else if (isAxe) {
        itemModel = glm::translate(itemModel, glm::vec3(-swing * 0.03f, 0.07f + swing * 0.03f, -0.30f - swing * 0.06f));
        itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        itemModel = glm::rotate(itemModel, glm::radians(-30.0f + swing * 70.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        itemModel = glm::rotate(itemModel, glm::radians(20.0f - swing * 20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    } else if (isShovel) {
        itemModel = glm::translate(itemModel, glm::vec3(0.0f, 0.06f - swing * 0.05f, -0.28f - swing * 0.10f));
        itemModel = glm::rotate(itemModel, glm::radians(40.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        itemModel = glm::rotate(itemModel, glm::radians(-15.0f + swing * 45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        itemModel = glm::rotate(itemModel, glm::radians(25.0f - swing * 15.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    } else if (isItem) {
        itemModel = glm::translate(itemModel, glm::vec3(0.0f, 0.07f, -0.28f));
        itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        itemModel = glm::rotate(itemModel, glm::radians(-20.0f + swing * 55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        itemModel = glm::rotate(itemModel, glm::radians(20.0f - swing * 25.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    } else {
        itemModel = glm::translate(itemModel, glm::vec3(-0.04f, 0.05f, -0.28f));
        itemModel = glm::rotate(itemModel, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        itemModel = glm::rotate(itemModel, glm::radians(25.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    // World-Space Transformation: convert camera-space hand & tool into the real 3D world
    glm::mat4 invView = glm::inverse(view);
    glm::mat4 worldHand = invView * handModel;
    glm::mat4 worldItem = invView * itemModel;
    glm::mat3 normHand  = glm::mat3(worldHand);
    glm::mat3 normItem  = glm::mat3(worldItem);

    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(m_localHandVertices.size() + m_localItemVertices.size());
    indices.reserve(m_localHandIndices.size() + m_localItemIndices.size());

    // Shading parameter: skylight exposure (r), ambient occlusion (g), block torch level (b)
    glm::vec3 vertColor(skylight, 1.0f, torchlight);

    // 1. Transform Hand Vertices
    uint32_t handBase = static_cast<uint32_t>(vertices.size());
    for (const auto& v : m_localHandVertices) {
        glm::vec3 wPos  = glm::vec3(worldHand * glm::vec4(v.position, 1.0f));
        glm::vec3 wNorm = glm::normalize(normHand * v.normal);
        vertices.push_back({wPos, v.texCoord, wNorm, vertColor});
    }
    for (uint32_t idx : m_localHandIndices) {
        indices.push_back(handBase + idx);
    }

    // 2. Transform Held Block or 3D Tool Vertices
    if (!m_localItemVertices.empty()) {
        uint32_t itemBase = static_cast<uint32_t>(vertices.size());
        for (const auto& v : m_localItemVertices) {
            glm::vec3 wPos  = glm::vec3(worldItem * glm::vec4(v.position, 1.0f));
            glm::vec3 wNorm = glm::normalize(normItem * v.normal);
            vertices.push_back({wPos, v.texCoord, wNorm, vertColor});
        }
        for (uint32_t idx : m_localItemIndices) {
            indices.push_back(itemBase + idx);
        }
    }

    if (indices.empty()) return;

    // Upload dynamic world-space geometry to host-visible frame buffer
    uint32_t f = m_cmdQueue.getCurrentFrame();
    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    if (!m_vbo[f].isValid() || m_vbo[f].getSize() < vSize) {
        m_vbo[f] = Buffer(m_context, std::max(vSize, (VkDeviceSize)16384),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    if (!m_ibo[f].isValid() || m_ibo[f].getSize() < iSize) {
        m_ibo[f] = Buffer(m_context, std::max(iSize, (VkDeviceSize)8192),
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    m_vbo[f].upload(vertices.data(), vSize);
    m_ibo[f].upload(indices.data(), iSize);

    // Forward push constants with dedicated hand projection and full real-time scene lighting
    PushConstants pc = scenePC;
    glm::mat4 handVP = handProj * view;
    std::memcpy(pc.mvp, &handVP[0][0], sizeof(float) * 16);

    vkCmdPushConstants(cmd, pipeline.getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &pc);

    VkBuffer vbs[] = {m_vbo[f].getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo[f].getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}

} // namespace prismcraft
