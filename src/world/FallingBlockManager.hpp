#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "world/Cell.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace prismcraft {

class VulkanContext;
class CommandQueue;
class World;

struct FallingBlock {
    int x, z, s;
    float currentY;
    int targetY;
    float velY = 0.0f;
    BlockType type;
};

class FallingBlockManager {
public:
    FallingBlockManager(VulkanContext& context, CommandQueue& cmdQueue);

    void spawn(int x, int startY, int z, int s, int targetY, BlockType type);
    bool isFalling(int x, int z, int s) const;
    void update(float dt, World& world);
    void render(VkCommandBuffer cmd, const Pipeline& pipeline, const glm::mat4& vpMatrix);

    [[nodiscard]] bool hasActiveBlocks() const { return !m_blocks.empty(); }

private:
    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo[MAX_FRAMES_IN_FLIGHT];
    Buffer m_ibo[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_indexCount[MAX_FRAMES_IN_FLIGHT]{0, 0};

    std::vector<FallingBlock> m_blocks;
};

} // namespace prismcraft
