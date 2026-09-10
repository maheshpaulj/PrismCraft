#pragma once
#include "RHICommon.hpp"
#include <cstdint>

namespace prismcraft {

class VulkanContext;
class CommandQueue;

class Texture {
public:
    Texture(VulkanContext& context, CommandQueue& cmdQueue, uint32_t width, uint32_t height, const uint8_t* rgbaPixels,
            bool linearFilter = false, bool generateMipmaps = false, uint32_t maxMipLevels = 0);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
    [[nodiscard]] VkImageView getImageView() const { return m_imageView; }
    [[nodiscard]] VkSampler getSampler() const { return m_sampler; }
    [[nodiscard]] uint32_t getMipLevels() const { return m_mipLevels; }

private:
    void cleanup();

    VulkanContext& m_context;
    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    uint32_t m_mipLevels = 1;
};

} // namespace prismcraft
