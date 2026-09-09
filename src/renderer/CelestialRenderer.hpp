#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include <glm/glm.hpp>

namespace prismcraft {

class VulkanContext;
class CommandQueue;

class CelestialRenderer {
public:
    CelestialRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                const glm::vec3& camPos,
                const glm::vec3& sunDir,
                const glm::mat4& vpMatrix);

private:
    void buildSun();
    void buildMoon();

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_sunVbo, m_sunIbo;
    uint32_t m_sunIndexCount = 0;

    Buffer m_moonVbo, m_moonIbo;
    uint32_t m_moonIndexCount = 0;
};

} // namespace prismcraft
