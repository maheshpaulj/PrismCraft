#pragma once
#include "rhi/RHICommon.hpp"
#include "rhi/Pipeline.hpp"
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <string>

namespace prismcraft {

class VulkanContext;

struct TSRConstants {
    glm::mat4 currInvViewProj;
    glm::mat4 prevViewProj;
    glm::vec4 renderAndDisplaySize; // xy = renderW, renderH; zw = displayW, displayH
    glm::vec4 jitterAndParams;      // xy = jitterOffset (in render pixels), z = feedback (0.88-0.92), w = sharpness (0.0 .. 1.0)
    glm::vec4 modeAndFlags;         // x = upscaleMode (0=Native, 1=FSR 1.0 Spatial, 2=TSR Temporal), y = resetHistory (1.0 or 0.0)
};

class TSRRenderer {
public:
    TSRRenderer(VulkanContext& context, uint32_t displayWidth, uint32_t displayHeight,
                VkFormat swapchainFormat, const std::string& exeDir);
    ~TSRRenderer();

    TSRRenderer(const TSRRenderer&) = delete;
    TSRRenderer& operator=(const TSRRenderer&) = delete;

    void recreate(uint32_t displayWidth, uint32_t displayHeight);
    void resetHistory() { m_resetHistory = true; }

    void render(VkCommandBuffer cmd,
                VkImageView currentFrameView,
                VkImageView depthView,
                VkImageView swapchainImageView,
                VkExtent2D renderExtent,
                VkExtent2D displayExtent,
                const glm::mat4& currInvViewProj,
                const glm::mat4& prevViewProj,
                const glm::vec2& jitterOffset,
                int upscaleMode,
                float sharpness);

    [[nodiscard]] uint32_t getDisplayWidth() const { return m_displayWidth; }
    [[nodiscard]] uint32_t getDisplayHeight() const { return m_displayHeight; }

private:
    void createResources(uint32_t width, uint32_t height);
    void cleanupResources();
    void createDescriptors();
    void createPipeline(const std::string& exeDir);

    VulkanContext& m_context;
    uint32_t m_displayWidth = 0;
    uint32_t m_displayHeight = 0;
    VkFormat m_swapchainFormat;
    VkFormat m_historyFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    std::string m_exeDir;

    // Ping-pong history textures at display resolution
    VkImage m_historyImage[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VmaAllocation m_historyAllocation[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkImageView m_historyImageView[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkImageLayout m_historyLayout[2] = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED };

    VkSampler m_linearSampler = VK_NULL_HANDLE;
    VkSampler m_depthSampler = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_descLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descSets[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

    std::unique_ptr<Pipeline> m_pipeline;

    uint32_t m_currentHistoryIndex = 0;
    bool m_resetHistory = true;
};

} // namespace prismcraft
