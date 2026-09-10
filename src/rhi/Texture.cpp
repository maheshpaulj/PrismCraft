#include "Texture.hpp"
#include "VulkanContext.hpp"
#include "CommandQueue.hpp"
#include "Buffer.hpp"
#include <cstring>
#include <stdexcept>

#include <cmath>
#include <algorithm>

namespace prismcraft {

Texture::Texture(VulkanContext& context, CommandQueue& cmdQueue, uint32_t width, uint32_t height, const uint8_t* rgbaPixels,
                 bool linearFilter, bool generateMipmaps, uint32_t maxMipLevels)
    : m_context(context) {

    VkDeviceSize imageSize = width * height * 4;

    m_mipLevels = 1;
    if (generateMipmaps) {
        m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
        if (maxMipLevels > 0 && maxMipLevels < m_mipLevels) {
            m_mipLevels = maxMipLevels;
        }
    }

    // 1. Create staging buffer and copy pixel data into it
    Buffer stagingBuffer(m_context, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stagingBuffer.upload(rgbaPixels, imageSize);

    // 2. Create GPU Image with VMA
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (m_mipLevels > 1) {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VK_CHECK(vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr),
             "Failed to create texture image!");

    // 3. Transition UNDEFINED -> TRANSFER_DST, copy buffer, generate mipmaps, transition -> SHADER_READ_ONLY
    VkCommandBuffer cmd = cmdQueue.beginSingleTimeCommands();

    // Barrier 1: mip 0 UNDEFINED -> TRANSFER_DST_OPTIMAL
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
    b1.subresourceRange.layerCount = 1;

    VkDependencyInfo dep1{};
    dep1.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep1.imageMemoryBarrierCount = 1;
    dep1.pImageMemoryBarriers = &b1;
    vkCmdPipelineBarrier2(cmd, &dep1);

    // Copy Buffer to Image (Mip 0)
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, stagingBuffer.getBuffer(), m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    if (m_mipLevels > 1) {
        int32_t mipWidth = static_cast<int32_t>(width);
        int32_t mipHeight = static_cast<int32_t>(height);

        for (uint32_t i = 1; i < m_mipLevels; ++i) {
            // Transition mip i - 1: TRANSFER_DST_OPTIMAL -> TRANSFER_SRC_OPTIMAL
            VkImageMemoryBarrier2 bSrc{};
            bSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            bSrc.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            bSrc.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            bSrc.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            bSrc.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            bSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            bSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            bSrc.image = m_image;
            bSrc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bSrc.subresourceRange.baseMipLevel = i - 1;
            bSrc.subresourceRange.levelCount = 1;
            bSrc.subresourceRange.baseArrayLayer = 0;
            bSrc.subresourceRange.layerCount = 1;

            // Transition mip i: UNDEFINED -> TRANSFER_DST_OPTIMAL
            VkImageMemoryBarrier2 bDst{};
            bDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            bDst.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            bDst.srcAccessMask = 0;
            bDst.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            bDst.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            bDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            bDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            bDst.image = m_image;
            bDst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bDst.subresourceRange.baseMipLevel = i;
            bDst.subresourceRange.levelCount = 1;
            bDst.subresourceRange.baseArrayLayer = 0;
            bDst.subresourceRange.layerCount = 1;

            VkImageMemoryBarrier2 barriers[2] = { bSrc, bDst };
            VkDependencyInfo depPreBlit{};
            depPreBlit.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            depPreBlit.imageMemoryBarrierCount = 2;
            depPreBlit.pImageMemoryBarriers = barriers;
            vkCmdPipelineBarrier2(cmd, &depPreBlit);

            int32_t nextWidth = std::max(1, mipWidth / 2);
            int32_t nextHeight = std::max(1, mipHeight / 2);

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;

            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { nextWidth, nextHeight, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmd,
                           m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit,
                           VK_FILTER_LINEAR);

            // Transition mip i - 1: TRANSFER_SRC_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
            VkImageMemoryBarrier2 bPostBlit{};
            bPostBlit.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            bPostBlit.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            bPostBlit.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            bPostBlit.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            bPostBlit.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            bPostBlit.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            bPostBlit.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            bPostBlit.image = m_image;
            bPostBlit.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bPostBlit.subresourceRange.baseMipLevel = i - 1;
            bPostBlit.subresourceRange.levelCount = 1;
            bPostBlit.subresourceRange.baseArrayLayer = 0;
            bPostBlit.subresourceRange.layerCount = 1;

            VkDependencyInfo depPostBlit{};
            depPostBlit.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            depPostBlit.imageMemoryBarrierCount = 1;
            depPostBlit.pImageMemoryBarriers = &bPostBlit;
            vkCmdPipelineBarrier2(cmd, &depPostBlit);

            mipWidth = nextWidth;
            mipHeight = nextHeight;
        }

        // Transition the final mip level: TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
        VkImageMemoryBarrier2 bLast{};
        bLast.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        bLast.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        bLast.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        bLast.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        bLast.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        bLast.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bLast.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bLast.image = m_image;
        bLast.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bLast.subresourceRange.baseMipLevel = m_mipLevels - 1;
        bLast.subresourceRange.levelCount = 1;
        bLast.subresourceRange.baseArrayLayer = 0;
        bLast.subresourceRange.layerCount = 1;

        VkDependencyInfo depLast{};
        depLast.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depLast.imageMemoryBarrierCount = 1;
        depLast.pImageMemoryBarriers = &bLast;
        vkCmdPipelineBarrier2(cmd, &depLast);
    } else {
        // Barrier 2: TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
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
        b2.subresourceRange.layerCount = 1;

        VkDependencyInfo dep2{};
        dep2.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep2.imageMemoryBarrierCount = 1;
        dep2.pImageMemoryBarriers = &b2;
        vkCmdPipelineBarrier2(cmd, &dep2);
    }

    cmdQueue.endSingleTimeCommands(cmd);

    // 4. Create Image View with all mip levels
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_imageView),
             "Failed to create texture image view!");

    // 5. Create Sampler (Strict Nearest filtering when linearFilter is false for crisp pixel art)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = linearFilter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.minFilter = linearFilter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    if (m_context.isAnisotropySupported() && linearFilter && m_mipLevels > 1) {
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = std::min(4.0f, m_context.getMaxAnisotropy());
    } else {
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
    }
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = linearFilter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(m_mipLevels);
    samplerInfo.mipLodBias = 0.0f;

    VK_CHECK(vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_sampler),
             "Failed to create texture sampler!");

    // 6. Create Descriptor Set Layout
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    VK_CHECK(vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout),
             "Failed to create descriptor set layout!");

    // 7. Create Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VK_CHECK(vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descriptorPool),
             "Failed to create descriptor pool!");

    // 8. Allocate and Update Descriptor Set
    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = m_descriptorPool;
    allocSetInfo.descriptorSetCount = 1;
    allocSetInfo.pSetLayouts = &m_descriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_context.getDevice(), &allocSetInfo, &m_descriptorSet),
             "Failed to allocate descriptor set!");

    VkDescriptorImageInfo descImageInfo{};
    descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descImageInfo.imageView = m_imageView;
    descImageInfo.sampler = m_sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &descImageInfo;

    vkUpdateDescriptorSets(m_context.getDevice(), 1, &descriptorWrite, 0, nullptr);
}

Texture::~Texture() {
    cleanup();
}

void Texture::cleanup() {
    VkDevice device = m_context.getDevice();
    if (device == VK_NULL_HANDLE) return;

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
