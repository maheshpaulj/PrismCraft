#include "ShadowMap.hpp"
#include "VulkanContext.hpp"
#include <algorithm>
#include <cmath>

namespace prismcraft {

ShadowMap::ShadowMap(VulkanContext& context)
    : m_context(context) {
    createResources();
}

ShadowMap::~ShadowMap() {
    cleanup();
}

void ShadowMap::createResources() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { RESOLUTION, RESOLUTION, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = CASCADE_COUNT;
    imageInfo.format = m_format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VK_CHECK(vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr),
             "Failed to create shadow map 2D array image!");

    // 3 Individual 2D views for rendering each cascade
    for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_layerImageViews[i]),
                 "Failed to create cascade layer image view!");
    }

    // 2D Array view for shader sampling
    VkImageViewCreateInfo arrayViewInfo{};
    arrayViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    arrayViewInfo.image = m_image;
    arrayViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    arrayViewInfo.format = m_format;
    arrayViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    arrayViewInfo.subresourceRange.baseMipLevel = 0;
    arrayViewInfo.subresourceRange.levelCount = 1;
    arrayViewInfo.subresourceRange.baseArrayLayer = 0;
    arrayViewInfo.subresourceRange.layerCount = CASCADE_COUNT;
    VK_CHECK(vkCreateImageView(m_context.getDevice(), &arrayViewInfo, nullptr, &m_arrayImageView),
             "Failed to create cascade array image view!");

    // Hardware depth comparison sampler with white border clamp (points outside cascade are lit)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    VK_CHECK(vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_sampler),
             "Failed to create shadow comparison sampler!");
}

void ShadowMap::cleanup() {
    VkDevice device = m_context.getDevice();
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_arrayImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_arrayImageView, nullptr);
        m_arrayImageView = VK_NULL_HANDLE;
    }
    for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
        if (m_layerImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_layerImageViews[i], nullptr);
            m_layerImageViews[i] = VK_NULL_HANDLE;
        }
    }
    if (m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_context.getAllocator(), m_image, m_allocation);
        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

void ShadowMap::updateCascades(const glm::mat4& viewMatrix, float fovRadians, float aspect,
                              float cameraNear, float cameraFar, const glm::vec3& lightDir,
                              int shadowDistanceOption) {
    if (shadowDistanceOption == 0) {
        m_cascadeSplits[0] = 20.0f;
        m_cascadeSplits[1] = std::min(cameraFar, 52.0f);
    } else if (shadowDistanceOption == 1) {
        m_cascadeSplits[0] = 26.0f;
        m_cascadeSplits[1] = std::min(cameraFar, 90.0f);
    } else if (shadowDistanceOption == 2) {
        m_cascadeSplits[0] = 34.0f;
        m_cascadeSplits[1] = std::min(cameraFar, 130.0f);
    } else {
        m_cascadeSplits[0] = 42.0f;
        m_cascadeSplits[1] = std::min(cameraFar, 175.0f);
    }

    glm::vec3 normalizedLightDir = glm::normalize(lightDir);

    const glm::mat4 biasMat(
        0.5f,  0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.5f,  0.5f, 0.0f, 1.0f
    );

    for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
        float splitNear = (i == 0) ? cameraNear : m_cascadeSplits[i - 1];
        float splitFar = m_cascadeSplits[i];

        // Construct subfrustum projection matrix
        glm::mat4 subProj = glm::perspective(fovRadians, aspect, splitNear, splitFar);
        glm::mat4 invSubVP = glm::inverse(subProj * viewMatrix);

        // Calculate world space corners of subfrustum
        std::array<glm::vec3, 8> frustumCorners{};
        int cornerIdx = 0;
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                for (int z = 0; z < 2; ++z) {
                    glm::vec4 pt = invSubVP * glm::vec4(
                        2.0f * static_cast<float>(x) - 1.0f,
                        2.0f * static_cast<float>(y) - 1.0f,
                        static_cast<float>(z), // Vulkan depth in [0, 1]
                        1.0f
                    );
                    frustumCorners[cornerIdx++] = glm::vec3(pt) / pt.w;
                }
            }
        }

        // Subfrustum centroid
        glm::vec3 center(0.0f);
        for (const auto& c : frustumCorners) center += c;
        center /= 8.0f;

        // Bounding sphere radius: invariant under camera rotation!
        float radius = 0.0f;
        for (const auto& c : frustumCorners) {
            radius = std::max(radius, glm::length(c - center));
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // Up vector perpendicular to light direction
        glm::vec3 lightUp = (std::abs(normalizedLightDir.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        // Light camera eye sits along +normalizedLightDir (at the light) looking towards center
        glm::mat4 lightView = glm::lookAt(center + normalizedLightDir * radius * 2.0f, center, lightUp);

        // Texel stabilization: snap sphere center to world-space texel grid
        float worldTexelSize = (2.0f * radius) / static_cast<float>(RESOLUTION);
        glm::vec4 centerLight = lightView * glm::vec4(center, 1.0f);
        centerLight.x = std::floor(centerLight.x / worldTexelSize) * worldTexelSize;
        centerLight.y = std::floor(centerLight.y / worldTexelSize) * worldTexelSize;

        glm::vec3 snappedCenter = glm::vec3(glm::inverse(lightView) * centerLight);
        lightView = glm::lookAt(snappedCenter + normalizedLightDir * radius * 2.0f, snappedCenter, lightUp);

        // Symmetric orthographic projection with tight bounds
        // (Rasterized with negative viewport in main.cpp, matching Vulkan CCW winding)
        glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, 0.0f, radius * 4.0f);

        m_cascadeViewProjs[i] = lightProj * lightView;
        m_cascadeShadowMatrices[i] = biasMat * m_cascadeViewProjs[i];
    }
}

void ShadowMap::transitionForRendering(VkCommandBuffer cmd, uint32_t cascadeIndex) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = m_currentLayouts[cascadeIndex];
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = cascadeIndex;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
    m_currentLayouts[cascadeIndex] = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
}

void ShadowMap::transitionForSampling(VkCommandBuffer cmd) {
    for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        barrier.oldLayout = m_currentLayouts[i];
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.image = m_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = i;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);
        m_currentLayouts[i] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

} // namespace prismcraft
