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
    void buildSkyDome();

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo;
    Buffer m_ibo;
    uint32_t m_indexCount = 0;
};

} // namespace prismcraft
