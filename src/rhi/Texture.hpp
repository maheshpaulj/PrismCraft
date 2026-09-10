#pragma once
#include "RHICommon.hpp"
#include <cstdint>

namespace prismcraft {

class VulkanContext;
class CommandQueue;

class Texture {
public:
    Texture(VulkanContext& context, CommandQueue& cmdQueue, uint32_t width, uint32_t height, const uint8_t* rgbaPixels, bool linearFilter = false);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
    [[nodiscard]] VkImageView getImageView() const { return m_imageView; }
    [[nodiscard]] VkSampler getSampler() const { return m_sampler; }

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
};

} // namespace prismcraft
