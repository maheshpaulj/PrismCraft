#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "world/Cell.hpp"
#include "world/ChunkMesher.hpp"
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
                const glm::mat4& view,
                const glm::mat4& proj,
                const PushConstants& scenePC,
                float skylight,
                float torchlight);

private:
    void buildLocalHandMesh();
    void buildLocalHeldBlockMesh(BlockType type);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    // Cached local geometry
    std::vector<ChunkVertex> m_localHandVertices;
    std::vector<uint32_t> m_localHandIndices;

    std::vector<ChunkVertex> m_localItemVertices;
    std::vector<uint32_t> m_localItemIndices;
    BlockType m_lastHeldBlock = BlockType::Air;

    // Dynamic per-frame VBO and IBO for world-space hand rendering
    Buffer m_vbo[MAX_FRAMES_IN_FLIGHT];
    Buffer m_ibo[MAX_FRAMES_IN_FLIGHT];
};

} // namespace prismcraft
