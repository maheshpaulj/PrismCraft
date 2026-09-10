#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "world/Cell.hpp"
#include "core/GameOptions.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace prismcraft {

class VulkanContext;
class CommandQueue;
class Player;
class World;

class UIRenderer {
public:
    UIRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& uiPipeline,
                const Pipeline* invertPipeline,
                const Player& player,
                uint32_t screenWidth,
                uint32_t screenHeight,
                float fps,
                const GameOptions& options,
                const World* world = nullptr,
                bool isChatOpen = false,
                const std::string& chatInput = "",
                const std::string& feedbackMsg = "",
                float feedbackTimer = 0.0f);

private:
    void buildCrosshairMesh();
    void updateDynamicUI(uint32_t frameIndex,
                         const Player& player, uint32_t screenWidth, uint32_t screenHeight, float fps,
                         const GameOptions& options, const World* world,
                         bool isChatOpen, const std::string& chatInput,
                         const std::string& feedbackMsg, float feedbackTimer);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_chVbo;
    Buffer m_chIbo;
    uint32_t m_chIndexCount = 0;

    static constexpr VkDeviceSize MAX_UI_VBO_SIZE = 1024 * 1024; // 1 MB
    static constexpr VkDeviceSize MAX_UI_IBO_SIZE = 512 * 1024;  // 512 KB

    Buffer m_uiVertexBuffer[MAX_FRAMES_IN_FLIGHT];
    Buffer m_uiIndexBuffer[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_uiIndexCount[MAX_FRAMES_IN_FLIGHT]{0, 0};
};

} // namespace prismcraft
