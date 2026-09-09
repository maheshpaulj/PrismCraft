#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace prismcraft {

class VulkanContext;
class CommandQueue;

class CloudRenderer {
public:
    CloudRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& cloudPipeline,
                const glm::vec3& camPos,
                float time,
                const glm::mat4& vpMatrix,
                const glm::vec3& skyColor,
                const glm::vec3& sunDir);

private:
    void rebuildCloudMesh(int anchorX, int anchorZ);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo[MAX_FRAMES_IN_FLIGHT];
    Buffer m_ibo[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_indexCount[MAX_FRAMES_IN_FLIGHT]{0, 0};
    int m_frameIndex = 0;
    int m_lastAnchorX = -999999;
    int m_lastAnchorZ = -999999;
};

} // namespace prismcraft
