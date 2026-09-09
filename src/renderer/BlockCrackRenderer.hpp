#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "world/Coordinates.hpp"
#include <glm/glm.hpp>
#include <optional>

namespace prismcraft {

class VulkanContext;
class CommandQueue;

class BlockCrackRenderer {
public:
    BlockCrackRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                const std::optional<CellCoord>& targetCell,
                int crackStage, // 0 to 9
                const glm::mat4& vpMatrix);

private:
    void rebuildStageMesh(int subIndex, int stage);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo0, m_ibo0;
    uint32_t m_indexCount0 = 0;

    Buffer m_vbo1, m_ibo1;
    uint32_t m_indexCount1 = 0;

    int m_lastStage0 = -1;
    int m_lastStage1 = -1;
};

} // namespace prismcraft
