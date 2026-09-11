#pragma once
#include "RHICommon.hpp"
#include <vector>

namespace prismcraft {

class VulkanContext;

class Swapchain {
public:
    Swapchain(VulkanContext& context, uint32_t width, uint32_t height, bool vsync = true);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void recreate(uint32_t width, uint32_t height);
    void recreate(uint32_t width, uint32_t height, bool vsync);
    void setVSync(bool vsync, uint32_t width, uint32_t height);
    [[nodiscard]] bool isVSyncEnabled() const { return m_vsync; }

    [[nodiscard]] VkSwapchainKHR getSwapchain() const { return m_swapchain; }
    [[nodiscard]] VkFormat getImageFormat() const { return m_imageFormat; }
    [[nodiscard]] VkFormat getDepthFormat() const { return m_depthFormat; }
    [[nodiscard]] VkExtent2D getExtent() const { return m_extent; }
    [[nodiscard]] uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }
    [[nodiscard]] VkImageView getImageView(uint32_t index) const { return m_imageViews[index]; }
    [[nodiscard]] VkImageView getDepthImageView() const { return m_depthImageView; }
    [[nodiscard]] VkImage getDepthImage() const { return m_depthImage; }

    // Returns UINT32_MAX if swapchain needs recreation
    uint32_t acquireNextImage(VkSemaphore imageAvailableSemaphore);

    // Transition helpers for dynamic rendering
    void transitionDepthAttachment(VkCommandBuffer cmd);
    void transitionToColorAttachment(VkCommandBuffer cmd, uint32_t imageIndex);
    void transitionToPresent(VkCommandBuffer cmd, uint32_t imageIndex);

private:
    void create(uint32_t width, uint32_t height);
    void createImageViews();
    void createDepthResources();
    void cleanup();

    bool m_vsync = true;
    VulkanContext& m_context;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
};

} // namespace prismcraft
