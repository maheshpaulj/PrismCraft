#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "world/Cell.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace prismcraft {

class VulkanContext;
class CommandQueue;
class Player;

class PlayerModelRenderer {
public:
    PlayerModelRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                const Player& player,
                const glm::mat4& vp,
                const PushConstants& scenePC);

private:
    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo[MAX_FRAMES_IN_FLIGHT];
    Buffer m_ibo[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_indexCount[MAX_FRAMES_IN_FLIGHT]{0, 0};
};

} // namespace prismcraft
