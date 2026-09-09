#include "TextureArray.hpp"
#include "VulkanContext.hpp"
#include "CommandQueue.hpp"
#include "Buffer.hpp"
#include <cstring>
#include <stdexcept>

namespace prismcraft {

TextureArray::TextureArray(VulkanContext& context, CommandQueue& cmdQueue,
                           uint32_t width, uint32_t height, uint32_t layerCount,
                           const std::vector<const uint8_t*>& layerPixelPointers)
    : m_context(context) {

    VkDeviceSize layerSize = width * height * 4;
    VkDeviceSize totalSize = layerSize * layerCount;

    // 1. Pack all layer data sequentially into host memory and upload to staging buffer
    std::vector<uint8_t> packedLayers(totalSize, 0);
    for (uint32_t i = 0; i < layerCount; ++i) {
        if (i < layerPixelPointers.size() && layerPixelPointers[i] != nullptr) {
            std::memcpy(packedLayers.data() + (i * layerSize), layerPixelPointers[i], layerSize);
        }
    }

    Buffer stagingBuffer(m_context, totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stagingBuffer.upload(packedLayers.data(), totalSize);

    // 2. Create GPU 2D Array Image with VMA
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = layerCount;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VK_CHECK(vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr),
             "Failed to create texture array image!");

    // 3. Upload data via one-shot command buffer
    VkCommandBuffer cmd = cmdQueue.beginSingleTimeCommands();

    // Transition UNDEFINED -> TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier2 b1{};
    b1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b1.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    b1.srcAccessMask = 0;
    b1.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    b1.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    b1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b1.image = m_image;
    b1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b1.subresourceRange.baseMipLevel = 0;
    b1.subresourceRange.levelCount = 1;
    b1.subresourceRange.baseArrayLayer = 0;
    b1.subresourceRange.layerCount = layerCount;

    VkDependencyInfo dep1{};
    dep1.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep1.imageMemoryBarrierCount = 1;
    dep1.pImageMemoryBarriers = &b1;
    vkCmdPipelineBarrier2(cmd, &dep1);

    // Copy each layer
    std::vector<VkBufferImageCopy> regions(layerCount);
    for (uint32_t i = 0; i < layerCount; ++i) {
        regions[i].bufferOffset = i * layerSize;
        regions[i].bufferRowLength = 0;
        regions[i].bufferImageHeight = 0;
        regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel = 0;
        regions[i].imageSubresource.baseArrayLayer = i;
        regions[i].imageSubresource.layerCount = 1;
        regions[i].imageOffset = {0, 0, 0};
        regions[i].imageExtent = {width, height, 1};
    }
    vkCmdCopyBufferToImage(cmd, stagingBuffer.getBuffer(), m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<uint32_t>(regions.size()), regions.data());

    // Transition TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
    VkImageMemoryBarrier2 b2{};
    b2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b2.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    b2.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    b2.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    b2.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b2.image = m_image;
    b2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b2.subresourceRange.baseMipLevel = 0;
    b2.subresourceRange.levelCount = 1;
    b2.subresourceRange.baseArrayLayer = 0;
    b2.subresourceRange.layerCount = layerCount;

    VkDependencyInfo dep2{};
    dep2.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep2.imageMemoryBarrierCount = 1;
    dep2.pImageMemoryBarriers = &b2;
    vkCmdPipelineBarrier2(cmd, &dep2);

    cmdQueue.endSingleTimeCommands(cmd);

    // 4. Create 2D Array Image View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = layerCount;

    VK_CHECK(vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_imageView),
             "Failed to create texture array image view!");

    // 5. Sampler (Nearest for crisp voxel pixel-art)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;

    VK_CHECK(vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_sampler),
             "Failed to create texture array sampler!");

    // 6. Descriptor Set Layout & Pool
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    VK_CHECK(vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout),
             "Failed to create descriptor set layout for texture array!");

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VK_CHECK(vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descriptorPool),
             "Failed to create descriptor pool for texture array!");

    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = m_descriptorPool;
    allocSetInfo.descriptorSetCount = 1;
    allocSetInfo.pSetLayouts = &m_descriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_context.getDevice(), &allocSetInfo, &m_descriptorSet),
             "Failed to allocate descriptor set for texture array!");

    VkDescriptorImageInfo descImageInfo{};
    descImageInfo.sampler = m_sampler;
    descImageInfo.imageView = m_imageView;
    descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &descImageInfo;

    vkUpdateDescriptorSets(m_context.getDevice(), 1, &write, 0, nullptr);
}

TextureArray::~TextureArray() {
    cleanup();
}

void TextureArray::cleanup() {
    VkDevice device = m_context.getDevice();
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_context.getAllocator(), m_image, m_allocation);
        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

} // namespace prismcraft
