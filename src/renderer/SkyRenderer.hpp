#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include <glm/glm.hpp>

namespace prismcraft {

class VulkanContext;
class CommandQueue;

class SkyRenderer {
public:
    SkyRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                const glm::vec3& camPos,
                const glm::vec3& sunDir,
                const glm::mat4& vpMatrix,
                float dayFactor,
                float sunHeight,
                float exposure = 0.95f);

private:
    void buildSkySphere();

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo;
    Buffer m_ibo;
    uint32_t m_indexCount = 0;
};

} // namespace prismcraft
