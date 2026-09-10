#include "CloudRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/ChunkMesher.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstring>
#include <vector>

namespace prismcraft {

CloudRenderer::CloudRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
    buildSkyDome();
}

void CloudRenderer::buildSkyDome() {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    const float RADIUS = 450.0f; // Inside the 500m camera far plane to prevent clipping
    const int RINGS = 32;        // Latitude rings from below horizon to zenith
    const int SECTORS = 48;      // Longitude sectors (360 degrees)

    const float MIN_PHI = -0.26f;      // ~15 degrees below horizon
    const float MAX_PHI = 1.5707963f;  // Zenith (+90 degrees)

    for (int r = 0; r <= RINGS; ++r) {
        float phiFraction = static_cast<float>(r) / static_cast<float>(RINGS);
        float phi = MIN_PHI + phiFraction * (MAX_PHI - MIN_PHI);
        float cosPhi = std::cos(phi);
        float sinPhi = std::sin(phi);

        for (int s = 0; s <= SECTORS; ++s) {
            float theta = static_cast<float>(s) * 2.0f * 3.14159265f / static_cast<float>(SECTORS);
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);

            glm::vec3 pos(
                RADIUS * cosPhi * sinTheta,
                RADIUS * sinPhi,
                RADIUS * cosPhi * cosTheta
            );

            glm::vec3 norm = -glm::normalize(pos);

            vertices.push_back({ pos, glm::vec2(0.0f), norm, glm::vec3(1.0f) });
        }
    }

    for (int r = 0; r < RINGS; ++r) {
        for (int s = 0; s < SECTORS; ++s) {
            uint32_t i0 = r * (SECTORS + 1) + s;
            uint32_t i1 = (r + 1) * (SECTORS + 1) + s;
            uint32_t i2 = (r + 1) * (SECTORS + 1) + (s + 1);
            uint32_t i3 = r * (SECTORS + 1) + (s + 1);

            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    m_indexCount = static_cast<uint32_t>(indices.size());
    if (indices.empty()) return;

    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_vbo = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_vbo.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_ibo = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ibo.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
}

void CloudRenderer::render(VkCommandBuffer cmd,
                           const Pipeline& cloudPipeline,
                           const glm::vec3& camPos,
                           float time,
                           const glm::mat4& vpMatrix,
                           const glm::vec3& skyColor,
                           const glm::vec3& sunDir) {
    if (m_indexCount == 0 || !m_vbo.isValid()) return;

    PushConstants pc{};
    std::memcpy(pc.mvp, &vpMatrix[0][0], sizeof(float) * 16);
    pc.sunDir[0] = sunDir.x; pc.sunDir[1] = sunDir.y; pc.sunDir[2] = sunDir.z; pc.sunDir[3] = 1.0f;
    pc.skyFog[0] = skyColor.r; pc.skyFog[1] = skyColor.g; pc.skyFog[2] = skyColor.b; pc.skyFog[3] = 12000.0f;
    pc.camPos[0] = camPos.x; pc.camPos[1] = camPos.y; pc.camPos[2] = camPos.z; pc.camPos[3] = time;

    cloudPipeline.bind(cmd);
    vkCmdPushConstants(cmd, cloudPipeline.getLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pc);

    VkBuffer vbs[] = {m_vbo.getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}

} // namespace prismcraft
