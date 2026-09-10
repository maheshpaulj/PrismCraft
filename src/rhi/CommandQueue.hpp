#pragma once
#include "RHICommon.hpp"
#include <array>

namespace prismcraft {

class VulkanContext;
class Swapchain;

class CommandQueue {
public:
    CommandQueue(VulkanContext& context);
    ~CommandQueue();

    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;

    // Frame lifecycle. Returns the swapchain image index.
    // Returns UINT32_MAX if swapchain needs recreation.
    uint32_t beginFrame(Swapchain& swapchain);
    [[nodiscard]] VkCommandBuffer getCommandBuffer() const;
    // Returns false if swapchain needs recreation.
    bool endFrame(Swapchain& swapchain, uint32_t imageIndex);

    // One-shot command buffer for uploads
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    [[nodiscard]] uint32_t getCurrentFrame() const { return m_currentFrame; }

private:
    VulkanContext& m_context;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_commandBuffers{};
    static constexpr size_t MAX_RENDER_SEMAPHORES = 8;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};
    std::array<VkSemaphore, MAX_RENDER_SEMAPHORES> m_renderFinishedSemaphores{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};
    uint32_t m_currentFrame = 0;
};

} // namespace prismcraft
