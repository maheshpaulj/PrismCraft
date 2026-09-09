#include "BlockOutlineRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/ChunkMesher.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstring>

namespace prismcraft {

BlockOutlineRenderer::BlockOutlineRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
    buildPrismWireframe(0);
    buildPrismWireframe(1);
}

void BlockOutlineRenderer::buildPrismWireframe(int subIndex) {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec3 wireColor(1.0f, 1.0f, 1.0f); // White for mathematical color inversion (ONE_MINUS_DST_COLOR)

    auto addLineQuad = [&](const glm::vec3& p0, const glm::vec3& p1, float thickness) {
        glm::vec3 dir = p1 - p0;
        float len = glm::length(dir);
        if (len < 0.0001f) return;
        dir /= len;

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        if (std::abs(dir.y) > 0.9f) up = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 side = glm::normalize(glm::cross(dir, up)) * (thickness * 0.5f);

        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec2 uv(0.0f, 0.0f);
        glm::vec3 norm(0.0f, 1.0f, 0.0f);
        vertices.push_back({p0 - side, uv, norm, wireColor});
        vertices.push_back({p0 + side, uv, norm, wireColor});
        vertices.push_back({p1 + side, uv, norm, wireColor});
        vertices.push_back({p1 - side, uv, norm, wireColor});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    float th = 0.02f; // Outline line thickness
    float eps = 0.002f; // Slight expansion so it doesn't z-fight with the block surface

    if (subIndex == 0) {
        // s=0 equilateral vertices: (0,0), (1,0), (0.5, TRI_HEIGHT)
        glm::vec3 v0(-eps, -eps, -eps);
        glm::vec3 v1(1.0f + eps, -eps, -eps);
        glm::vec3 v2(0.5f, -eps, TRI_HEIGHT + eps);

        glm::vec3 v0_top(-eps, 1.0f + eps, -eps);
        glm::vec3 v1_top(1.0f + eps, 1.0f + eps, -eps);
        glm::vec3 v2_top(0.5f, 1.0f + eps, TRI_HEIGHT + eps);

        // Bottom 3 edges
        addLineQuad(v0, v1, th);
        addLineQuad(v1, v2, th);
        addLineQuad(v2, v0, th);

        // Top 3 edges
        addLineQuad(v0_top, v1_top, th);
        addLineQuad(v1_top, v2_top, th);
        addLineQuad(v2_top, v0_top, th);

        // 3 Vertical edges
        addLineQuad(v0, v0_top, th);
        addLineQuad(v1, v1_top, th);
        addLineQuad(v2, v2_top, th);

        m_indexCount0 = static_cast<uint32_t>(indices.size());
        VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
        VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

        m_vbo0 = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_vbo0.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

        m_ibo0 = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_ibo0.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
    } else {
        // s=1 equilateral vertices: (1,0), (1.5, TRI_HEIGHT), (0.5, TRI_HEIGHT)
        glm::vec3 v0(1.0f, -eps, -eps);
        glm::vec3 v1(1.5f + eps, -eps, TRI_HEIGHT + eps);
        glm::vec3 v2(0.5f - eps, -eps, TRI_HEIGHT + eps);

        glm::vec3 v0_top(1.0f, 1.0f + eps, -eps);
        glm::vec3 v1_top(1.5f + eps, 1.0f + eps, TRI_HEIGHT + eps);
        glm::vec3 v2_top(0.5f - eps, 1.0f + eps, TRI_HEIGHT + eps);

        // Bottom 3 edges
        addLineQuad(v0, v1, th);
        addLineQuad(v1, v2, th);
        addLineQuad(v2, v0, th);

        // Top 3 edges
        addLineQuad(v0_top, v1_top, th);
        addLineQuad(v1_top, v2_top, th);
        addLineQuad(v2_top, v0_top, th);

        // 3 Vertical edges
        addLineQuad(v0, v0_top, th);
        addLineQuad(v1, v1_top, th);
        addLineQuad(v2, v2_top, th);

        m_indexCount1 = static_cast<uint32_t>(indices.size());
        VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
        VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

        m_vbo1 = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_vbo1.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

        m_ibo1 = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_ibo1.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
    }
}

void BlockOutlineRenderer::render(VkCommandBuffer cmd,
                                  const Pipeline& pipeline,
                                  const std::optional<CellCoord>& targetCell,
                                  const glm::mat4& vpMatrix) {
    if (!targetCell.has_value()) return;

    const CellCoord& c = targetCell.value();
    float posX = static_cast<float>(c.x) + getRowXOffset(c.z);
    float posY = static_cast<float>(c.y);
    float posZ = static_cast<float>(c.z) * TRI_HEIGHT;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, posZ));
    glm::mat4 mvp = vpMatrix * model;

    PushConstants pc{};
    std::memcpy(pc.mvp, &mvp[0][0], sizeof(float) * 16);
    pc.sunDir[3] = 2.0f; // Self-illuminated pure white for negative inversion
    pc.lightColor[0] = 1.0f; pc.lightColor[1] = 1.0f; pc.lightColor[2] = 1.0f;
    pc.skyFog[3] = 1000.0f;

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
