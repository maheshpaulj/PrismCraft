#include "PlayerModelRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "player/Player.hpp"
#include "renderer/TextureAtlas.hpp"
#include "world/ChunkMesher.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace prismcraft {

PlayerModelRenderer::PlayerModelRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
}

static void addEquilateralPrism(
    std::vector<ChunkVertex>& vertices,
    std::vector<uint32_t>& indices,
    const glm::mat4& mat,
    float side,
    float height,
    uint32_t frontTile,
    uint32_t sideTile,
    uint32_t topTile,
    uint32_t botTile) {

    float zFront = side * 0.28867513f; // sqrt(3)/6
    float zBack  = -side * 0.57735027f; // -sqrt(3)/3
    float halfS  = side * 0.5f;

    glm::vec3 v0(-halfS, 0.0f, zFront);
    glm::vec3 v1( halfS, 0.0f, zFront);
    glm::vec3 v2( 0.0f,  0.0f, zBack);

    glm::vec3 v3(-halfS, height, zFront);
    glm::vec3 v4( halfS, height, zFront);
    glm::vec3 v5( 0.0f,  height, zBack);

    auto addQuad = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
                       const glm::vec3& localNorm, uint32_t tile) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 n = glm::normalize(glm::vec3(mat * glm::vec4(localNorm, 0.0f)));
        glm::vec4 uv = TextureAtlas::getTileUV(tile);
        glm::vec3 col(1.0f, 0.85f, 0.0f); // r = sunlight shadow, g = ambient/smooth light, b = torch light (0 = NO torch blast)

        glm::vec3 w0 = glm::vec3(mat * glm::vec4(p0, 1.0f));
        glm::vec3 w1 = glm::vec3(mat * glm::vec4(p1, 1.0f));
        glm::vec3 w2 = glm::vec3(mat * glm::vec4(p2, 1.0f));
        glm::vec3 w3 = glm::vec3(mat * glm::vec4(p3, 1.0f));

        vertices.push_back({w0, glm::vec2(uv.x, uv.w), n, col});
        vertices.push_back({w1, glm::vec2(uv.z, uv.w), n, col});
        vertices.push_back({w2, glm::vec2(uv.z, uv.y), n, col});
        vertices.push_back({w3, glm::vec2(uv.x, uv.y), n, col});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 0); indices.push_back(b + 2); indices.push_back(b + 3);
    };

    auto addTriangle = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                           const glm::vec3& localNorm, uint32_t tile) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 n = glm::normalize(glm::vec3(mat * glm::vec4(localNorm, 0.0f)));
        glm::vec4 uv = TextureAtlas::getTileUV(tile);
        glm::vec3 col(1.0f, 0.85f, 0.0f); // r = sunlight shadow, g = ambient, b = 0.0 torch light

        glm::vec3 w0 = glm::vec3(mat * glm::vec4(p0, 1.0f));
        glm::vec3 w1 = glm::vec3(mat * glm::vec4(p1, 1.0f));
        glm::vec3 w2 = glm::vec3(mat * glm::vec4(p2, 1.0f));

        vertices.push_back({w0, glm::vec2(uv.x, uv.w), n, col});
        vertices.push_back({w1, glm::vec2(uv.z, uv.w), n, col});
        vertices.push_back({w2, glm::vec2(uv.x + (uv.z - uv.x) * 0.5f, uv.y), n, col});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    // 1. Front Wall (facing +Z)
    addQuad(v0, v1, v4, v3, glm::vec3(0.0f, 0.0f, 1.0f), frontTile);

    // 2. Right Slanted Wall (normal = [sqrt(3)/2, 0, -0.5])
    addQuad(v1, v2, v5, v4, glm::vec3(0.8660254f, 0.0f, -0.5f), sideTile);

    // 3. Left Slanted Wall (normal = [-sqrt(3)/2, 0, -0.5])
    addQuad(v2, v0, v3, v5, glm::vec3(-0.8660254f, 0.0f, -0.5f), sideTile);

    // 4. Top Equilateral Triangle (facing +Y, CCW: v3 -> v4 -> v5)
    addTriangle(v3, v4, v5, glm::vec3(0.0f, 1.0f, 0.0f), topTile);

    // 5. Bottom Equilateral Triangle (facing -Y, CCW: v0 -> v2 -> v1)
    addTriangle(v0, v2, v1, glm::vec3(0.0f, -1.0f, 0.0f), botTile);
}

static void addItemQuad(
    std::vector<ChunkVertex>& vertices,
    std::vector<uint32_t>& indices,
    const glm::mat4& mat,
    float size,
    uint32_t tile) {

    float halfS = size * 0.5f;
    glm::vec4 uv = TextureAtlas::getTileUV(tile);
    glm::vec3 col(1.0f, 0.85f, 0.0f);

    glm::vec3 p0(-halfS, -halfS, 0.0f);
    glm::vec3 p1( halfS, -halfS, 0.0f);
    glm::vec3 p2( halfS,  halfS, 0.0f);
    glm::vec3 p3(-halfS,  halfS, 0.0f);

    glm::vec3 w0 = glm::vec3(mat * glm::vec4(p0, 1.0f));
    glm::vec3 w1 = glm::vec3(mat * glm::vec4(p1, 1.0f));
    glm::vec3 w2 = glm::vec3(mat * glm::vec4(p2, 1.0f));
    glm::vec3 w3 = glm::vec3(mat * glm::vec4(p3, 1.0f));

    glm::vec3 n = glm::normalize(glm::vec3(mat * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));

    // Front face
    uint32_t b = static_cast<uint32_t>(vertices.size());
    vertices.push_back({w0, glm::vec2(uv.x, uv.w), n, col});
    vertices.push_back({w1, glm::vec2(uv.z, uv.w), n, col});
    vertices.push_back({w2, glm::vec2(uv.z, uv.y), n, col});
    vertices.push_back({w3, glm::vec2(uv.x, uv.y), n, col});
    indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    indices.push_back(b + 0); indices.push_back(b + 2); indices.push_back(b + 3);

    // Back face
    uint32_t b2 = static_cast<uint32_t>(vertices.size());
    vertices.push_back({w1, glm::vec2(uv.z, uv.w), -n, col * 0.9f});
    vertices.push_back({w0, glm::vec2(uv.x, uv.w), -n, col * 0.9f});
    vertices.push_back({w3, glm::vec2(uv.x, uv.y), -n, col * 0.9f});
    vertices.push_back({w2, glm::vec2(uv.z, uv.y), -n, col * 0.9f});
    indices.push_back(b2 + 0); indices.push_back(b2 + 1); indices.push_back(b2 + 2);
    indices.push_back(b2 + 0); indices.push_back(b2 + 2); indices.push_back(b2 + 3);
}

void PlayerModelRenderer::render(VkCommandBuffer cmd,
                                 const Pipeline& pipeline,
                                 const Player& player,
                                 const glm::mat4& vp,
                                 const PushConstants& scenePC) {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec3& pos = player.getPosition();
    const glm::vec3& fwd = player.getCamera().getForward();

    glm::vec3 horizFwd = glm::vec3(fwd.x, 0.0f, fwd.z);
    if (glm::length(horizFwd) < 0.001f) {
        horizFwd = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        horizFwd = glm::normalize(horizFwd);
    }

    // Yaw rotation: character faces the camera's horizontal forward direction
    float yaw = std::atan2(horizFwd.x, horizFwd.z);
    
    // Crouch offset
    bool crouching = player.isCrouching();
    float crouchDrop = crouching ? 0.16f : 0.0f;
    glm::vec3 renderPos = pos - glm::vec3(0.0f, crouchDrop, 0.0f);

    glm::mat4 playerBase = glm::translate(glm::mat4(1.0f), renderPos) *
                           glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f));

    // Animation variables
    float bobTime = player.getBobTime();
    float bobWeight = player.getBobWeight();
    // Smooth natural human/Minecraft walking stride cadence (~1.8 strides/sec)
    float walkSwing = std::sin(bobTime) * 0.62f * bobWeight;

    // Hit / Mining Swing & Block Place animations
    float swingProgress = player.getSwingProgress();
    float swing = (swingProgress > 0.0f) ? std::sin(swingProgress * 3.14159265f) : 0.0f;

    float placeProgress = player.getPlaceProgress();
    float place = (placeProgress > 0.0f) ? std::sin(placeProgress * 3.14159265f) : 0.0f;

    // Pitch for head tilt
    float pitch = std::asin(std::clamp(fwd.y, -0.99f, 0.99f));

    // 1. Torso: Equilateral prism (S = 0.48m, H = 0.70m - Minecraft 8x12 pixels)
    // When crouching, torso leans forward ~22 degrees
    float torsoPitch = crouching ? glm::radians(22.0f) : 0.0f;
    glm::mat4 torsoMat = playerBase * 
                         glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.70f, crouching ? -0.06f : 0.0f)) *
                         glm::rotate(glm::mat4(1.0f), torsoPitch, glm::vec3(1.0f, 0.0f, 0.0f));
    addEquilateralPrism(vertices, indices, torsoMat, 0.48f, 0.70f,
                        TextureAtlas::TILE_STEVE_TORSO_FRONT,
                        TextureAtlas::TILE_STEVE_TORSO_BACK,
                        TextureAtlas::TILE_STEVE_TORSO_BACK,
                        TextureAtlas::TILE_STEVE_TORSO_BACK);

    // 2. Head: Equilateral prism (S = 0.48m, H = 0.44m - Minecraft 8x8 pixels) with face on FRONT ONLY
    glm::vec3 headPivot = crouching ? glm::vec3(0.0f, 1.34f, 0.12f) : glm::vec3(0.0f, 1.40f, 0.0f);
    glm::mat4 headMat = playerBase *
                        glm::translate(glm::mat4(1.0f), headPivot) *
                        glm::rotate(glm::mat4(1.0f), -pitch + (crouching ? glm::radians(-15.0f) : 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    addEquilateralPrism(vertices, indices, headMat, 0.48f, 0.44f,
                        TextureAtlas::TILE_STEVE_HEAD_FRONT,
                        TextureAtlas::TILE_STEVE_HEAD_SIDE,
                        TextureAtlas::TILE_STEVE_HEAD_TOP,
                        TextureAtlas::TILE_STEVE_SKIN_TONE);

    // 3. Left Arm: (S = 0.24m, H = 0.70m - Minecraft 4x12 pixels) placed flush against left shoulder (X = +0.34m in model local space)
    glm::vec3 lShoulderPivot = crouching ? glm::vec3(0.34f, 1.32f, 0.02f) : glm::vec3(0.34f, 1.36f, 0.04f);
    glm::mat4 lArmMat = playerBase *
                        glm::translate(glm::mat4(1.0f), lShoulderPivot) *
                        glm::rotate(glm::mat4(1.0f), -walkSwing + (crouching ? glm::radians(18.0f) : 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.70f, 0.0f));
    addEquilateralPrism(vertices, indices, lArmMat, 0.24f, 0.70f,
                        TextureAtlas::TILE_STEVE_ARM,
                        TextureAtlas::TILE_STEVE_ARM,
                        TextureAtlas::TILE_STEVE_TORSO_FRONT,
                        TextureAtlas::TILE_STEVE_SKIN_TONE);

    // 4. Right Arm: (S = 0.24m, H = 0.70m - Minecraft 4x12 pixels) placed on right shoulder (X = -0.34m in model local space)
    glm::vec3 rShoulderPivot = crouching ? glm::vec3(-0.34f, 1.32f, 0.02f) : glm::vec3(-0.34f, 1.36f, 0.04f);
    glm::mat4 rArmMat = playerBase *
                        glm::translate(glm::mat4(1.0f), rShoulderPivot);

    // Combine walk swing with hit punch and place gestures on Steve's Right Arm
    float rArmAngleX = walkSwing - swing * 1.35f - place * 0.45f + (crouching ? glm::radians(18.0f) : 0.0f);
    float rArmAngleY = -swing * 0.35f;
    float rArmAngleZ = -swing * 0.20f;
    rArmMat = glm::rotate(rArmMat, rArmAngleX, glm::vec3(1.0f, 0.0f, 0.0f));
    rArmMat = glm::rotate(rArmMat, rArmAngleY, glm::vec3(0.0f, 1.0f, 0.0f));
    rArmMat = glm::rotate(rArmMat, rArmAngleZ, glm::vec3(0.0f, 0.0f, 1.0f));
    rArmMat = glm::translate(rArmMat, glm::vec3(0.0f, -0.70f, 0.0f));

    addEquilateralPrism(vertices, indices, rArmMat, 0.24f, 0.70f,
                        TextureAtlas::TILE_STEVE_ARM,
                        TextureAtlas::TILE_STEVE_ARM,
                        TextureAtlas::TILE_STEVE_TORSO_FRONT,
                        TextureAtlas::TILE_STEVE_SKIN_TONE);

    // Miniature Held Block / 2.5D Tool in Right Hand
    BlockType heldBlock = player.getSelectedBlock();
    if (heldBlock != BlockType::Air) {
        uint32_t blockTile = TextureAtlas::getTileForBlock(heldBlock, 0);
        if (Cell{heldBlock}.isItem()) {
            glm::mat4 toolMat = rArmMat *
                                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.06f, 0.22f)) *
                                glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
                                glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            addItemQuad(vertices, indices, toolMat, 0.32f, blockTile);
        } else {
            glm::mat4 blockMat = rArmMat *
                                 glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.06f, 0.16f)) *
                                 glm::rotate(glm::mat4(1.0f), 0.35f, glm::vec3(0.0f, 1.0f, 0.0f));
            addEquilateralPrism(vertices, indices, blockMat, 0.20f, 0.20f, blockTile, blockTile, blockTile, blockTile);
        }
    }

    // 5. Left Leg: (S = 0.24m, H = 0.70m - Minecraft 4x12 pixels) at X = +0.12m
    glm::vec3 lHipPivot(0.12f, 0.70f, crouching ? -0.08f : 0.0f);
    glm::mat4 lLegMat = playerBase *
                        glm::translate(glm::mat4(1.0f), lHipPivot) *
                        glm::rotate(glm::mat4(1.0f), walkSwing, glm::vec3(1.0f, 0.0f, 0.0f)) *
                        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.70f, 0.0f));
    addEquilateralPrism(vertices, indices, lLegMat, 0.24f, 0.70f,
                        TextureAtlas::TILE_STEVE_LEG,
                        TextureAtlas::TILE_STEVE_LEG,
                        TextureAtlas::TILE_STEVE_LEG,
                        TextureAtlas::TILE_STEVE_LEG);

    // 6. Right Leg: (S = 0.24m, H = 0.70m - Minecraft 4x12 pixels) at X = -0.12m
    glm::vec3 rHipPivot(-0.12f, 0.70f, crouching ? -0.08f : 0.0f);
    glm::mat4 rLegMat = playerBase *
                        glm::translate(glm::mat4(1.0f), rHipPivot) *
                        glm::rotate(glm::mat4(1.0f), -walkSwing, glm::vec3(1.0f, 0.0f, 0.0f)) *
                        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.70f, 0.0f));
    addEquilateralPrism(vertices, indices, rLegMat, 0.24f, 0.70f,
                        TextureAtlas::TILE_STEVE_LEG,
                        TextureAtlas::TILE_STEVE_LEG,
                        TextureAtlas::TILE_STEVE_LEG,
                        TextureAtlas::TILE_STEVE_LEG);

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

    PushConstants pc = scenePC;
    std::memcpy(pc.mvp, &vp[0][0], sizeof(float) * 16);

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
