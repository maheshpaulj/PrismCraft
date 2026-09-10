#include "CommandQueue.hpp"
#include "VulkanContext.hpp"
#include "Swapchain.hpp"

namespace prismcraft {

CommandQueue::CommandQueue(VulkanContext& context) : m_context(context) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_context.getGraphicsQueueFamily();

    VK_CHECK(vkCreateCommandPool(m_context.getDevice(), &poolInfo, nullptr, &m_commandPool), "Failed to create command pool!");

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    VK_CHECK(vkAllocateCommandBuffers(m_context.getDevice(), &allocInfo, m_commandBuffers.data()), "Failed to allocate command buffers!");

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(m_context.getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]), "Failed to create semaphore!");
        VK_CHECK(vkCreateFence(m_context.getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]), "Failed to create fence!");
    }
    for (size_t i = 0; i < MAX_RENDER_SEMAPHORES; i++) {
        VK_CHECK(vkCreateSemaphore(m_context.getDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]), "Failed to create semaphore!");
    }
}

CommandQueue::~CommandQueue() {
    VkDevice device = m_context.getDevice();
    for (size_t i = 0; i < MAX_RENDER_SEMAPHORES; i++) {
        vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, m_inFlightFences[i], nullptr);
    }
    vkDestroyCommandPool(device, m_commandPool, nullptr);
}

uint32_t CommandQueue::beginFrame(Swapchain& swapchain) {
    vkWaitForFences(m_context.getDevice(), 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = swapchain.acquireNextImage(m_imageAvailableSemaphores[m_currentFrame]);
    if (imageIndex == UINT32_MAX) {
        return UINT32_MAX;
    }

    vkResetFences(m_context.getDevice(), 1, &m_inFlightFences[m_currentFrame]);
    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo), "Failed to begin recording command buffer!");

    return imageIndex;
}

VkCommandBuffer CommandQueue::getCommandBuffer() const {
    return m_commandBuffers[m_currentFrame];
}

bool CommandQueue::endFrame(Swapchain& swapchain, uint32_t imageIndex) {
    VK_CHECK(vkEndCommandBuffer(m_commandBuffers[m_currentFrame]), "Failed to record command buffer!");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

    uint32_t renderSemIndex = imageIndex % MAX_RENDER_SEMAPHORES;
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[renderSemIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK(vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]), "Failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {swapchain.getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(m_context.getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        return false;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

VkCommandBuffer CommandQueue::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_context.getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void CommandQueue::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context.getGraphicsQueue());

    vkFreeCommandBuffers(m_context.getDevice(), m_commandPool, 1, &commandBuffer);
}

} // namespace prismcraft
