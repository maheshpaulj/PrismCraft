#include "SkyRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/ChunkMesher.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstring>
#include <vector>

namespace prismcraft {

SkyRenderer::SkyRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
    buildSkySphere();
}

void SkyRenderer::buildSkySphere() {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    const float RADIUS = 480.0f; // Sits right at the far horizon
    const int RINGS = 32;        // Latitude rings from south pole to north pole
    const int SECTORS = 48;      // Longitude sectors (360 degrees)

    const float MIN_PHI = -1.5707963f; // -90 degrees (Nadir)
    const float MAX_PHI =  1.5707963f; // +90 degrees (Zenith)

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

            // Inward-facing winding for inside of sphere
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i0);
            indices.push_back(i3);
            indices.push_back(i2);
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

void SkyRenderer::render(VkCommandBuffer cmd,
                         const Pipeline& pipeline,
                         const glm::vec3& camPos,
                         const glm::vec3& sunDir,
                         const glm::mat4& vpMatrix,
                         float dayFactor,
                         float sunHeight,
                         float exposure) {
    if (m_indexCount == 0 || !m_vbo.isValid()) return;

    PushConstants pc{};
    std::memcpy(pc.mvp, &vpMatrix[0][0], sizeof(float) * 16);
    pc.sunDir[0] = sunDir.x; pc.sunDir[1] = sunDir.y; pc.sunDir[2] = sunDir.z;
    pc.sunDir[3] = 1.0f; // Full intensity
    pc.camPos[0] = camPos.x; pc.camPos[1] = camPos.y; pc.camPos[2] = camPos.z; pc.camPos[3] = 0.0f;
    pc.dayInfo[0] = dayFactor;
    pc.dayInfo[1] = sunHeight;
    pc.dayInfo[2] = exposure;
    pc.dayInfo[3] = 0.0f;

    pipeline.bind(cmd);
    vkCmdPushConstants(cmd, pipeline.getLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pc);

    VkBuffer vbs[] = { m_vbo.getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}

} // namespace prismcraft
