#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "world/Coordinates.hpp"
#include "world/World.hpp"
#include <glm/glm.hpp>
#include <optional>

namespace prismcraft {

class VulkanContext;
class CommandQueue;

class BlockOutlineRenderer {
public:
    BlockOutlineRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                const std::optional<CellCoord>& targetCell,
                const glm::mat4& vpMatrix,
                const World& world);

private:
    void buildPrismWireframe(int subIndex);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo0, m_ibo0;
    uint32_t m_indexCount0 = 0;

    Buffer m_vbo1, m_ibo1;
    uint32_t m_indexCount1 = 0;

    Buffer m_dynVbo[MAX_FRAMES_IN_FLIGHT];
    Buffer m_dynIbo[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_dynIndexCount[MAX_FRAMES_IN_FLIGHT]{0, 0};
};

} // namespace prismcraft
