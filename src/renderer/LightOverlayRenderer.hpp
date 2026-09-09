#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace prismcraft {

class VulkanContext;
class CommandQueue;
class World;

class LightOverlayRenderer {
public:
    LightOverlayRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                const World& world,
                const glm::vec3& playerPos,
                const glm::vec3& camPos,
                const glm::mat4& vpMatrix);

private:
    void rebuildMesh(const World& world, const glm::vec3& playerPos, const glm::vec3& camPos);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo[MAX_FRAMES_IN_FLIGHT];
    Buffer m_ibo[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_indexCount[MAX_FRAMES_IN_FLIGHT]{0, 0};
    int m_frameIndex = 0;
    glm::vec3 m_lastPlayerPos{9999.0f};
};

} // namespace prismcraft
