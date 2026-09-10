#pragma once
#include "RHICommon.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>

namespace prismcraft {

class VulkanContext;

class ShadowMap {
public:
    static constexpr uint32_t CASCADE_COUNT = 2;
    static constexpr uint32_t RESOLUTION = 2048;

    ShadowMap(VulkanContext& context);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    [[nodiscard]] VkImageView getLayerImageView(uint32_t cascadeIndex) const {
        return m_layerImageViews[cascadeIndex];
    }
    [[nodiscard]] VkImageView getArrayImageView() const {
        return m_arrayImageView;
    }
    [[nodiscard]] VkSampler getSampler() const {
        return m_sampler;
    }
    [[nodiscard]] VkFormat getFormat() const {
        return m_format;
    }
    [[nodiscard]] const std::array<glm::mat4, CASCADE_COUNT>& getCascadeViewProjections() const {
        return m_cascadeViewProjs;
    }
    [[nodiscard]] const std::array<glm::mat4, CASCADE_COUNT>& getCascadeShadowMatrices() const {
        return m_cascadeShadowMatrices;
    }
    [[nodiscard]] const std::array<float, CASCADE_COUNT>& getCascadeSplits() const {
        return m_cascadeSplits;
    }

    void updateCascades(const glm::mat4& viewMatrix, float fovRadians, float aspect,
                        float cameraNear, float cameraFar, const glm::vec3& lightDir,
                        int shadowDistanceOption = 1);

    void transitionForRendering(VkCommandBuffer cmd, uint32_t cascadeIndex);
    void transitionForSampling(VkCommandBuffer cmd);

private:
    void createResources();
    void cleanup();

    VulkanContext& m_context;
    VkFormat m_format = VK_FORMAT_D32_SFLOAT;
    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    std::array<VkImageView, CASCADE_COUNT> m_layerImageViews{VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkImageView m_arrayImageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    std::array<glm::mat4, CASCADE_COUNT> m_cascadeViewProjs{};
    std::array<glm::mat4, CASCADE_COUNT> m_cascadeShadowMatrices{};
    std::array<float, CASCADE_COUNT> m_cascadeSplits{24.0f, 80.0f};
    std::array<VkImageLayout, CASCADE_COUNT> m_currentLayouts{
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED
    };
};

} // namespace prismcraft
