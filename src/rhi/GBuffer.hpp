#pragma once
#include "RHICommon.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace prismcraft {

class VulkanContext;

class GBuffer {
public:
    GBuffer(VulkanContext& context, uint32_t width, uint32_t height);
    ~GBuffer();

    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;

    void recreate(uint32_t width, uint32_t height);

    [[nodiscard]] VkImageView getAlbedoView() const { return m_albedoView; }
    [[nodiscard]] VkImageView getNormalRoughnessView() const { return m_normalRoughnessView; }
    [[nodiscard]] VkImageView getHdrSceneView() const { return m_hdrSceneView; }
    [[nodiscard]] VkImageView getDepthView() const { return m_depthView; }

    [[nodiscard]] VkFormat getAlbedoFormat() const { return m_albedoFormat; }
    [[nodiscard]] VkFormat getNormalRoughnessFormat() const { return m_normalRoughnessFormat; }
    [[nodiscard]] VkFormat getHdrSceneFormat() const { return m_hdrSceneFormat; }
    [[nodiscard]] VkFormat getDepthFormat() const { return m_depthFormat; }

    [[nodiscard]] VkSampler getSampler() const { return m_sampler; }
    [[nodiscard]] uint32_t getWidth() const { return m_width; }
    [[nodiscard]] uint32_t getHeight() const { return m_height; }

    void transitionForGBufferPass(VkCommandBuffer cmd);
    void transitionForLightingPass(VkCommandBuffer cmd);
    void transitionForForwardPass(VkCommandBuffer cmd);
    void transitionForPostProcess(VkCommandBuffer cmd);

private:
    void createResources(uint32_t width, uint32_t height);
    void cleanup();

    VulkanContext& m_context;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    VkFormat m_albedoFormat = VK_FORMAT_R8G8B8A8_SRGB;
    VkImage m_albedoImage = VK_NULL_HANDLE;
    VmaAllocation m_albedoAlloc = VK_NULL_HANDLE;
    VkImageView m_albedoView = VK_NULL_HANDLE;

    VkFormat m_normalRoughnessFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkImage m_normalRoughnessImage = VK_NULL_HANDLE;
    VmaAllocation m_normalRoughnessAlloc = VK_NULL_HANDLE;
    VkImageView m_normalRoughnessView = VK_NULL_HANDLE;

    VkFormat m_hdrSceneFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkImage m_hdrSceneImage = VK_NULL_HANDLE;
    VmaAllocation m_hdrSceneAlloc = VK_NULL_HANDLE;
    VkImageView m_hdrSceneView = VK_NULL_HANDLE;

    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthAlloc = VK_NULL_HANDLE;
    VkImageView m_depthView = VK_NULL_HANDLE;

    VkSampler m_sampler = VK_NULL_HANDLE;
};

} // namespace prismcraft
