#include "BlockCrackRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/ChunkMesher.hpp"
#include "TextureAtlas.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstring>
#include <algorithm>

namespace prismcraft {

BlockCrackRenderer::BlockCrackRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
}

void BlockCrackRenderer::rebuildStageMesh(int subIndex, int stage) {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    int clampedStage = std::clamp(stage, 0, 9);
    int tileIdx = 240 + clampedStage;
    glm::vec4 uv = TextureAtlas::getTileUV(tileIdx);
    glm::vec3 c(1.0f);

    auto addTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& norm) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, glm::vec2(uv.x, uv.y), norm, c});
        vertices.push_back({v1, glm::vec2(uv.x, uv.w), norm, c});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, c});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    auto addQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& norm) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, glm::vec2(uv.x, uv.w), norm, c});
        vertices.push_back({v1, glm::vec2(uv.z, uv.w), norm, c});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, c});
        vertices.push_back({v3, glm::vec2(uv.x, uv.y), norm, c});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    float eps = 0.003f; // Slight outwards offset

    if (subIndex == 0) {
        // s=0 equilateral vertices: (0,0), (1,0), (0.5, TRI_HEIGHT)
        glm::vec3 B0(-eps, -eps, -eps);
        glm::vec3 B1(1.0f + eps, -eps, -eps);
        glm::vec3 B2(0.5f, -eps, TRI_HEIGHT + eps);

        glm::vec3 T0(-eps, 1.0f + eps, -eps);
        glm::vec3 T1(1.0f + eps, 1.0f + eps, -eps);
        glm::vec3 T2(0.5f, 1.0f + eps, TRI_HEIGHT + eps);

        addTri(T0, T2, T1, glm::vec3(0, 1, 0));
        addTri(B0, B1, B2, glm::vec3(0, -1, 0));
        addQuad(B0, T0, T1, B1, glm::vec3(0.0f, 0.0f, -1.0f));
        addQuad(B2, T2, T0, B0, glm::vec3(-SQRT_3_OVER_2, 0.0f, 0.5f));
        addQuad(B1, T1, T2, B2, glm::vec3(SQRT_3_OVER_2, 0.0f, 0.5f));

        m_indexCount0 = static_cast<uint32_t>(indices.size());
        VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
        VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

        m_vbo0 = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_vbo0.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

        m_ibo0 = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_ibo0.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);

        m_lastStage0 = stage;
    } else {
        // s=1 equilateral vertices: (1,0), (1.5, TRI_HEIGHT), (0.5, TRI_HEIGHT)
        glm::vec3 B0(1.0f, -eps, -eps);
        glm::vec3 B1(1.5f + eps, -eps, TRI_HEIGHT + eps);
        glm::vec3 B2(0.5f - eps, -eps, TRI_HEIGHT + eps);

        glm::vec3 T0(1.0f, 1.0f + eps, -eps);
        glm::vec3 T1(1.5f + eps, 1.0f + eps, TRI_HEIGHT + eps);
        glm::vec3 T2(0.5f - eps, 1.0f + eps, TRI_HEIGHT + eps);

        addTri(T0, T2, T1, glm::vec3(0, 1, 0));
        addTri(B0, B1, B2, glm::vec3(0, -1, 0));
        addQuad(B1, T1, T2, B2, glm::vec3(0.0f, 0.0f, 1.0f));
        addQuad(B2, T2, T0, B0, glm::vec3(-SQRT_3_OVER_2, 0.0f, -0.5f));
        addQuad(B1, T1, T0, B0, glm::vec3(SQRT_3_OVER_2, 0.0f, -0.5f));

        m_indexCount1 = static_cast<uint32_t>(indices.size());
        VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
        VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

        m_vbo1 = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_vbo1.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

        m_ibo1 = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_ibo1.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);

        m_lastStage1 = stage;
    }
}

void BlockCrackRenderer::render(VkCommandBuffer cmd,
                                const Pipeline& pipeline,
                                const std::optional<CellCoord>& targetCell,
                                int crackStage,
                                const glm::mat4& vpMatrix) {
    if (!targetCell.has_value() || crackStage < 0) return;

    const CellCoord& c = targetCell.value();

    if (c.s == 0 && (crackStage != m_lastStage0 || !m_vbo0.isValid())) {
        rebuildStageMesh(0, crackStage);
    } else if (c.s == 1 && (crackStage != m_lastStage1 || !m_vbo1.isValid())) {
        rebuildStageMesh(1, crackStage);
    }

    float posX = static_cast<float>(c.x) + getRowXOffset(c.z);
    float posY = static_cast<float>(c.y);
    float posZ = static_cast<float>(c.z) * TRI_HEIGHT;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, posZ));
    glm::mat4 mvp = vpMatrix * model;

    PushConstants pc{};
    std::memcpy(pc.mvp, &mvp[0][0], sizeof(float) * 16);
    pc.sunDir[3] = 1.0f;
    pc.lightColor[0] = 1.0f; pc.lightColor[1] = 1.0f; pc.lightColor[2] = 1.0f;
    pc.skyFog[3] = 500.0f;

    vkCmdPushConstants(cmd, pipeline.getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &pc);

    if (c.s == 0 && m_indexCount0 > 0 && m_vbo0.isValid()) {
        VkBuffer vbs[] = {m_vbo0.getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
        vkCmdBindIndexBuffer(cmd, m_ibo0.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_indexCount0, 1, 0, 0, 0);
    } else if (c.s == 1 && m_indexCount1 > 0 && m_vbo1.isValid()) {
        VkBuffer vbs[] = {m_vbo1.getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
        vkCmdBindIndexBuffer(cmd, m_ibo1.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_indexCount1, 1, 0, 0, 0);
    }
}

} // namespace prismcraft
