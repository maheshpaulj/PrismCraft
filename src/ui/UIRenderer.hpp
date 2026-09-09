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
    void updateDynamicUI(const Player& player, uint32_t screenWidth, uint32_t screenHeight, float fps,
                         const GameOptions& options, const World* world,
                         bool isChatOpen, const std::string& chatInput,
                         const std::string& feedbackMsg, float feedbackTimer);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_chVbo;
    Buffer m_chIbo;
    uint32_t m_chIndexCount = 0;

    Buffer m_uiVertexBuffer[MAX_FRAMES_IN_FLIGHT];
    Buffer m_uiIndexBuffer[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_uiIndexCount[MAX_FRAMES_IN_FLIGHT]{0, 0};

    uint32_t m_lastWidth = 0;
    uint32_t m_lastHeight = 0;
    int m_lastSelectedSlot = -1;
    float m_lastHealth = -1.0f;
    float m_lastOxygen = -1.0f;
    int m_lastFPS = -1;
    bool m_lastDebugHUD = false;
    glm::vec3 m_lastPos{9999.0f};
    bool m_lastChatOpen = false;
    std::string m_lastChatInput = "";
    float m_lastFeedbackTimer = 0.0f;
    int m_lastCamMode = -1;
};

} // namespace prismcraft
