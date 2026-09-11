#pragma once
#include "rhi/RHICommon.hpp"
#include "rhi/Pipeline.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace prismcraft {

class VulkanContext;

struct PostProcessPushConstants {
    glm::vec4 params;  // x = exposure, y = vibrance, z = bloomStrength, w = time
    glm::vec4 options; // x = vibrantVisuals (1.0 or 0.0), y = sharpening (0.12), z = colorGrading, w = reserved
};

class PostProcessRenderer {
public:
    PostProcessRenderer(VulkanContext& context, uint32_t width, uint32_t height,
                        VkFormat swapchainFormat, const std::string& exeDir);
    ~PostProcessRenderer();

    PostProcessRenderer(const PostProcessRenderer&) = delete;
    PostProcessRenderer& operator=(const PostProcessRenderer&) = delete;

    void recreate(uint32_t width, uint32_t height);

    void transitionHDRForRendering(VkCommandBuffer cmd);
    void transitionHDRForSampling(VkCommandBuffer cmd);

    void render(VkCommandBuffer cmd, VkImageView swapchainImageView, VkExtent2D extent,
                float exposure, float vibrance, float bloomStrength, float time,
                bool vibrantVisuals, float sharpening = 0.0f);

    void renderQuad(VkCommandBuffer cmd, VkExtent2D extent,
                    float exposure, float vibrance, float bloomStrength, float time,
                    bool vibrantVisuals, float sharpening = 0.0f);

    void copyHDRToSSR(VkCommandBuffer cmd, VkImage sceneDepthImage = VK_NULL_HANDLE);

    [[nodiscard]] VkImageView getHDRImageView() const { return m_hdrImageView; }
    [[nodiscard]] VkImage getHDRImage() const { return m_hdrImage; }
    [[nodiscard]] VkImageView getSSRImageView() const { return m_ssrImageView; }
    [[nodiscard]] VkSampler getSSRSampler() const { return m_ssrSampler; }
    [[nodiscard]] VkImageView getSSRDepthImageView() const { return m_ssrDepthImageView; }
    [[nodiscard]] VkSampler getSSRDepthSampler() const { return m_ssrDepthSampler; }
    [[nodiscard]] VkFormat getHDRFormat() const { return m_hdrFormat; }
    [[nodiscard]] uint32_t getWidth() const { return m_width; }
    [[nodiscard]] uint32_t getHeight() const { return m_height; }

private:
    void createResources(uint32_t width, uint32_t height);
    void cleanupResources();
    void createDescriptorSet();
    void createPipeline(const std::string& exeDir);

    VulkanContext& m_context;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    VkFormat m_swapchainFormat;
    VkFormat m_hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    VkImage m_hdrImage = VK_NULL_HANDLE;
    VmaAllocation m_hdrAllocation = VK_NULL_HANDLE;
    VkImageView m_hdrImageView = VK_NULL_HANDLE;
    VkSampler m_hdrSampler = VK_NULL_HANDLE;

    VkImage m_ssrImage = VK_NULL_HANDLE;
    VmaAllocation m_ssrAllocation = VK_NULL_HANDLE;
    VkImageView m_ssrImageView = VK_NULL_HANDLE;
    VkSampler m_ssrSampler = VK_NULL_HANDLE;

    VkImage m_ssrDepthImage = VK_NULL_HANDLE;
    VmaAllocation m_ssrDepthAllocation = VK_NULL_HANDLE;
    VkImageView m_ssrDepthImageView = VK_NULL_HANDLE;
    VkSampler m_ssrDepthSampler = VK_NULL_HANDLE;

    VkImageLayout m_currentHDRLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout m_currentSSRLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout m_currentSSRDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkDescriptorSetLayout m_descLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descSet = VK_NULL_HANDLE;

    std::unique_ptr<Pipeline> m_pipeline;
};

} // namespace prismcraft
