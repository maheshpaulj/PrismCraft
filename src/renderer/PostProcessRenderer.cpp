#include "PostProcessRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include <iostream>

namespace prismcraft {

PostProcessRenderer::PostProcessRenderer(VulkanContext& context, uint32_t width, uint32_t height,
                                         VkFormat swapchainFormat, const std::string& exeDir)
    : m_context(context), m_width(width), m_height(height), m_swapchainFormat(swapchainFormat) {

    // 1. Create Linear Texture Sampler with Clamp-to-Edge
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    VK_CHECK(vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_hdrSampler),
             "Failed to create PostProcess HDR sampler!");

    // 2. Create Descriptor Set Layout (Binding 0 = Combined Image Sampler)
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    VK_CHECK(vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_descLayout),
             "Failed to create PostProcess descriptor set layout!");

    // 3. Create Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    VK_CHECK(vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descPool),
             "Failed to create PostProcess descriptor pool!");

    // 4. Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocSet{};
    allocSet.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSet.descriptorPool = m_descPool;
    allocSet.descriptorSetCount = 1;
    allocSet.pSetLayouts = &m_descLayout;
    VK_CHECK(vkAllocateDescriptorSets(m_context.getDevice(), &allocSet, &m_descSet),
             "Failed to allocate PostProcess descriptor set!");

    // 5. Create HDR Offscreen Image and Image View
    createResources(m_width, m_height);

    // 6. Create Fullscreen Post-Processing Graphics Pipeline
    createPipeline(exeDir);
}

PostProcessRenderer::~PostProcessRenderer() {
    cleanupResources();

    if (m_descPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.getDevice(), m_descPool, nullptr);
    }
    if (m_descLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context.getDevice(), m_descLayout, nullptr);
    }
    if (m_hdrSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_context.getDevice(), m_hdrSampler, nullptr);
    }
}

void PostProcessRenderer::createResources(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    // Create 16-bit Float Offscreen Image for high dynamic range linear radiance
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { m_width, m_height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_hdrFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VK_CHECK(vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo, &m_hdrImage, &m_hdrAllocation, nullptr),
             "Failed to allocate PostProcess HDR image!");

    // Create Image View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_hdrImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_hdrFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_hdrImageView),
             "Failed to create PostProcess HDR image view!");

    m_currentHDRLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Update Descriptor Set with new HDR Image View
    createDescriptorSet();
}

void PostProcessRenderer::cleanupResources() {
    if (m_hdrImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_context.getDevice(), m_hdrImageView, nullptr);
        m_hdrImageView = VK_NULL_HANDLE;
    }
    if (m_hdrImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_context.getAllocator(), m_hdrImage, m_hdrAllocation);
        m_hdrImage = VK_NULL_HANDLE;
        m_hdrAllocation = VK_NULL_HANDLE;
    }
}

void PostProcessRenderer::createDescriptorSet() {
    VkDescriptorImageInfo imageDesc{};
    imageDesc.sampler = m_hdrSampler;
    imageDesc.imageView = m_hdrImageView;
    imageDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageDesc;
    vkUpdateDescriptorSets(m_context.getDevice(), 1, &write, 0, nullptr);
}

void PostProcessRenderer::createPipeline(const std::string& exeDir) {
    std::vector<VkFormat> colorFormats = { m_swapchainFormat };
    m_pipeline = std::make_unique<Pipeline>(
        m_context,
        colorFormats,
        VK_FORMAT_UNDEFINED,
        exeDir + "assets/shaders/fullscreen.vert.spv",
        exeDir + "assets/shaders/post_process.frag.spv",
        m_descLayout,
        false,             // Depth test OFF
        false,             // Depth write OFF
        BlendMode::None,   // Opaque write into swapchain image
        VK_CULL_MODE_NONE, // No culling for fullscreen triangle
        false, 0.0f, 0.0f,
        false              // hasVertexInputs = false (vertices generated by gl_VertexIndex)
    );
}

void PostProcessRenderer::recreate(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    cleanupResources();
    createResources(width, height);
}

void PostProcessRenderer::transitionHDRForRendering(VkCommandBuffer cmd) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = (m_currentHDRLayout == VK_IMAGE_LAYOUT_UNDEFINED) ?
        VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.srcAccessMask = (m_currentHDRLayout == VK_IMAGE_LAYOUT_UNDEFINED) ?
        0 : VK_ACCESS_2_SHADER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = m_currentHDRLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.image = m_hdrImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    m_currentHDRLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

void PostProcessRenderer::transitionHDRForSampling(VkCommandBuffer cmd) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.image = m_hdrImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    m_currentHDRLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void PostProcessRenderer::render(VkCommandBuffer cmd, VkImageView swapchainImageView, VkExtent2D extent,
                                float exposure, float vibrance, float bloomStrength, float time,
                                bool vibrantVisuals, float sharpening) {
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapchainImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = { { 0, 0 }, extent };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.pDepthAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);

    renderQuad(cmd, extent, exposure, vibrance, bloomStrength, time, vibrantVisuals, sharpening);

    vkCmdEndRendering(cmd);
}

void PostProcessRenderer::renderQuad(VkCommandBuffer cmd, VkExtent2D extent,
                                     float exposure, float vibrance, float bloomStrength, float time,
                                     bool vibrantVisuals, float sharpening) {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{ { 0, 0 }, extent };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_pipeline->bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline->getLayout(), 0, 1,
                            &m_descSet, 0, nullptr);

    PostProcessPushConstants pc{};
    pc.params = glm::vec4(exposure, vibrance, bloomStrength, time);
    pc.options = glm::vec4(vibrantVisuals ? 1.0f : 0.0f, sharpening, 0.0f, 0.0f);

    vkCmdPushConstants(cmd, m_pipeline->getLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PostProcessPushConstants), &pc);

    // Draw full-screen triangle generated mathematically by fullscreen.vert
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

} // namespace prismcraft
