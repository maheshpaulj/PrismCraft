#include "TSRRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include <iostream>
#include <algorithm>

namespace prismcraft {

TSRRenderer::TSRRenderer(VulkanContext& context, uint32_t displayWidth, uint32_t displayHeight,
                         VkFormat swapchainFormat, const std::string& exeDir)
    : m_context(context), m_displayWidth(displayWidth), m_displayHeight(displayHeight),
      m_swapchainFormat(swapchainFormat), m_exeDir(exeDir) {

    // 1. Create Linear Texture Sampler (Clamp to Edge)
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
    VK_CHECK(vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_linearSampler),
             "Failed to create TSR linear sampler!");

    // 2. Create Depth Sampler (Nearest, Clamp to Edge)
    VkSamplerCreateInfo depthSamplerInfo = samplerInfo;
    depthSamplerInfo.magFilter = VK_FILTER_NEAREST;
    depthSamplerInfo.minFilter = VK_FILTER_NEAREST;
    depthSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    VK_CHECK(vkCreateSampler(m_context.getDevice(), &depthSamplerInfo, nullptr, &m_depthSampler),
             "Failed to create TSR depth sampler!");

    // 3. Create Descriptor Set Layout
    // Binding 0 = Current Frame LDR scene
    // Binding 1 = Scene Depth buffer
    // Binding 2 = Previous History buffer
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_descLayout),
             "Failed to create TSR descriptor set layout!");

    // 4. Create Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 6; // 2 sets * 3 bindings

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2;
    VK_CHECK(vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descPool),
             "Failed to create TSR descriptor pool!");

    // 5. Allocate 2 Descriptor Sets for ping-pong
    std::array<VkDescriptorSetLayout, 2> layouts = { m_descLayout, m_descLayout };
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(m_context.getDevice(), &allocInfo, m_descSets),
             "Failed to allocate TSR descriptor sets!");

    // 6. Create History Resources and Pipeline
    createResources(m_displayWidth, m_displayHeight);
    createPipeline(m_exeDir);
}

TSRRenderer::~TSRRenderer() {
    cleanupResources();

    if (m_descPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.getDevice(), m_descPool, nullptr);
        m_descPool = VK_NULL_HANDLE;
    }
    if (m_descLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context.getDevice(), m_descLayout, nullptr);
        m_descLayout = VK_NULL_HANDLE;
    }
    if (m_linearSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_context.getDevice(), m_linearSampler, nullptr);
        m_linearSampler = VK_NULL_HANDLE;
    }
    if (m_depthSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_context.getDevice(), m_depthSampler, nullptr);
        m_depthSampler = VK_NULL_HANDLE;
    }
}

void TSRRenderer::createResources(uint32_t width, uint32_t height) {
    m_displayWidth = std::max(width, 1u);
    m_displayHeight = std::max(height, 1u);

    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = { m_displayWidth, m_displayHeight, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_historyFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VK_CHECK(vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo,
                                &m_historyImage[i], &m_historyAllocation[i], nullptr),
                 "Failed to allocate TSR history image!");

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_historyImage[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_historyFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_historyImageView[i]),
                 "Failed to create TSR history image view!");

        m_historyLayout[i] = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    m_resetHistory = true;
}

void TSRRenderer::cleanupResources() {
    for (int i = 0; i < 2; ++i) {
        if (m_historyImageView[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(m_context.getDevice(), m_historyImageView[i], nullptr);
            m_historyImageView[i] = VK_NULL_HANDLE;
        }
        if (m_historyImage[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_context.getAllocator(), m_historyImage[i], m_historyAllocation[i]);
            m_historyImage[i] = VK_NULL_HANDLE;
            m_historyAllocation[i] = VK_NULL_HANDLE;
        }
        m_historyLayout[i] = VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

void TSRRenderer::createPipeline(const std::string& exeDir) {
    std::vector<VkFormat> colorFormats = { m_swapchainFormat, m_historyFormat };
    m_pipeline = std::make_unique<Pipeline>(
        m_context,
        colorFormats,
        VK_FORMAT_UNDEFINED,
        exeDir + "assets/shaders/tsr_upscale.vert.spv",
        exeDir + "assets/shaders/tsr_upscale.frag.spv",
        m_descLayout,
        false,             // Depth test OFF
        false,             // Depth write OFF
        BlendMode::None,   // Opaque write
        VK_CULL_MODE_NONE, // Fullscreen triangle
        false, 0.0f, 0.0f,
        false              // hasVertexInputs = false
    );
}

void TSRRenderer::recreate(uint32_t displayWidth, uint32_t displayHeight) {
    if (displayWidth == 0 || displayHeight == 0) return;
    cleanupResources();
    createResources(displayWidth, displayHeight);
}

void TSRRenderer::render(VkCommandBuffer cmd,
                         VkImageView currentFrameView,
                         VkImageView depthView,
                         VkImageView swapchainImageView,
                         VkExtent2D renderExtent,
                         VkExtent2D displayExtent,
                         const glm::mat4& currInvViewProj,
                         const glm::mat4& prevViewProj,
                         const glm::vec2& jitterOffset,
                         int upscaleMode,
                         float sharpness) {

    uint32_t readIdx = 1 - m_currentHistoryIndex;
    uint32_t writeIdx = m_currentHistoryIndex;

    // 1. Transition History to Read Layout (for readIdx)
    if (m_historyLayout[readIdx] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = (m_historyLayout[readIdx] == VK_IMAGE_LAYOUT_UNDEFINED) ?
            VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = (m_historyLayout[readIdx] == VK_IMAGE_LAYOUT_UNDEFINED) ?
            0 : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        barrier.oldLayout = m_historyLayout[readIdx];
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.image = m_historyImage[readIdx];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dep{};
        dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &dep);

        m_historyLayout[readIdx] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // 2. Transition History to Write Layout (for writeIdx)
    if (m_historyLayout[writeIdx] != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = (m_historyLayout[writeIdx] == VK_IMAGE_LAYOUT_UNDEFINED) ?
            VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.srcAccessMask = (m_historyLayout[writeIdx] == VK_IMAGE_LAYOUT_UNDEFINED) ?
            0 : VK_ACCESS_2_SHADER_READ_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = m_historyLayout[writeIdx];
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.image = m_historyImage[writeIdx];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dep{};
        dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &dep);

        m_historyLayout[writeIdx] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // 3. Update Descriptor Set for this frame
    VkDescriptorImageInfo currentImgInfo{};
    currentImgInfo.sampler = m_linearSampler;
    currentImgInfo.imageView = currentFrameView;
    currentImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depthImgInfo{};
    depthImgInfo.sampler = m_depthSampler;
    depthImgInfo.imageView = depthView;
    depthImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo historyImgInfo{};
    historyImgInfo.sampler = m_linearSampler;
    historyImgInfo.imageView = m_historyImageView[readIdx];
    historyImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 3> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descSets[readIdx];
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &currentImgInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descSets[readIdx];
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &depthImgInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_descSets[readIdx];
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &historyImgInfo;

    vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    // 4. Begin Dynamic Rendering with Multiple Render Targets (Swapchain + History)
    VkRenderingAttachmentInfo colorAttachments[2]{};
    colorAttachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachments[0].imageView = swapchainImageView;
    colorAttachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachments[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachments[1].imageView = m_historyImageView[writeIdx];
    colorAttachments[1].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = { { 0, 0 }, displayExtent };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 2;
    renderInfo.pColorAttachments = colorAttachments;
    renderInfo.pDepthAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(displayExtent.width);
    viewport.height = static_cast<float>(displayExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{ { 0, 0 }, displayExtent };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_pipeline->bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline->getLayout(), 0, 1,
                            &m_descSets[readIdx], 0, nullptr);

    TSRConstants pc{};
    pc.currInvViewProj = currInvViewProj;
    pc.prevViewProj = prevViewProj;
    pc.renderAndDisplaySize = glm::vec4(renderExtent.width, renderExtent.height,
                                        displayExtent.width, displayExtent.height);
    pc.jitterAndParams = glm::vec4(jitterOffset.x, jitterOffset.y, 0.90f, sharpness);
    pc.modeAndFlags = glm::vec4(static_cast<float>(upscaleMode), m_resetHistory ? 1.0f : 0.0f, 0.0f, 0.0f);

    vkCmdPushConstants(cmd, m_pipeline->getLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(TSRConstants), &pc);

    // Fullscreen Triangle
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);

    // Advance ping-pong
    m_currentHistoryIndex = writeIdx;
    m_resetHistory = false;
}

} // namespace prismcraft
