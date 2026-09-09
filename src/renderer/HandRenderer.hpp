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

class HandRenderer {
public:
    HandRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                const Player& player,
                float aspectRatio);

private:
    void buildHandMesh();
    void buildHeldBlockMesh(BlockType type);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_handVertexBuffer;
    Buffer m_handIndexBuffer;
    uint32_t m_handIndexCount = 0;

    Buffer m_blockVertexBuffer;
    Buffer m_blockIndexBuffer;
    uint32_t m_blockIndexCount = 0;
    BlockType m_lastHeldBlock = BlockType::Air;
};

} // namespace prismcraft
