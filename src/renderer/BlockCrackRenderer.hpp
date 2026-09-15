#pragma once
#include "rhi/RHICommon.hpp"
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "world/Coordinates.hpp"
#include <glm/glm.hpp>
#include <optional>
#include <vector>

namespace prismcraft {

class VulkanContext;
class CommandQueue;
class World;
struct ChunkVertex;

class BlockCrackRenderer {
public:
    BlockCrackRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                VkDescriptorSet descSet,
                const World& world,
                const std::optional<CellCoord>& targetCell,
                int crackStage, // 0 to 9
                const glm::mat4& vpMatrix,
                const PushConstants& pc);

private:
    void rebuildBlockMesh(const World& world, const CellCoord& coord, int stage, uint32_t frameIndex);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo[MAX_FRAMES_IN_FLIGHT];
    Buffer m_ibo[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_indexCount[MAX_FRAMES_IN_FLIGHT] = { 0, 0 };

    int m_lastX[MAX_FRAMES_IN_FLIGHT] = { -999999, -999999 };
    int m_lastY[MAX_FRAMES_IN_FLIGHT] = { -999999, -999999 };
    int m_lastZ[MAX_FRAMES_IN_FLIGHT] = { -999999, -999999 };
    int m_lastS[MAX_FRAMES_IN_FLIGHT] = { -1, -1 };
    int m_lastStage[MAX_FRAMES_IN_FLIGHT] = { -1, -1 };
    uint8_t m_lastDoorState[MAX_FRAMES_IN_FLIGHT] = { 0, 0 };
};

} // namespace prismcraft
