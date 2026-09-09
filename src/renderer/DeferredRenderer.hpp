#pragma once
#include "rhi/RHICommon.hpp"
#include "rhi/Pipeline.hpp"
#include "rhi/Buffer.hpp"
#include "rhi/ShadowMap.hpp"
#include "rhi/GBuffer.hpp"
#include "rhi/TextureArray.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace prismcraft {

class VulkanContext;
class CommandQueue;

struct LightingUBO {
    glm::mat4 invViewProj;
    glm::mat4 lightViewProj[3];
    glm::vec4 cascadeSplits; // x=16.0, y=54.0, z=200.0
    glm::vec4 sunDir;        // xyz = light dir, w = intensity
    glm::vec4 camPos;        // xyz = cam pos, w = time
    glm::vec4 skyColor;      // xyz = ambient sky color
    glm::vec4 heldTorch;     // xyz = held torch pos, w = active
    glm::vec4 pointLight1;   // xyz = dropped torch pos, w = intensity
    glm::vec4 fogParams;     // x = fogStart, y = fogEnd, z = isUnderwater, w = fogFalloff
};

class DeferredRenderer {
public:
    DeferredRenderer(VulkanContext& context, CommandQueue& cmdQueue,
                     uint32_t width, uint32_t height,
                     VkFormat swapchainFormat, const std::string& shaderDir);
    ~DeferredRenderer();

    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;

    void recreate(uint32_t width, uint32_t height);

    [[nodiscard]] ShadowMap& getShadowMap() { return *m_shadowMap; }
    [[nodiscard]] GBuffer& getGBuffer() { return *m_gbuffer; }
    [[nodiscard]] TextureArray& getTextureArray() { return *m_textureArray; }

    [[nodiscard]] const Pipeline& getGBufferPipeline() const { return *m_gbufferPipeline; }
    [[nodiscard]] const Pipeline& getCsmPipeline() const { return *m_csmPipeline; }

    void updateCascades(const glm::mat4& viewMatrix, float fov, float aspect,
                        float nearClip, float farClip, const glm::vec3& lightDir);

    void renderDeferredLighting(VkCommandBuffer cmd,
                                const glm::mat4& invViewProj,
                                const glm::vec3& sunDir, float sunIntensity,
                                const glm::vec3& camPos, float time,
                                const glm::vec3& skyColor,
                                const glm::vec4& heldTorch,
                                const glm::vec4& pointLight1,
                                const glm::vec4& fogParams = glm::vec4(60.0f, 250.0f, 0.0f, 0.85f));

    void renderPostProcess(VkCommandBuffer cmd, VkImageView swapchainImageView, VkExtent2D extent);

private:
    void createPipelines();
    void createDescriptors();
    void updateDescriptors();
    void cleanupDescriptors();

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;
    uint32_t m_width;
    uint32_t m_height;
    VkFormat m_swapchainFormat;
    std::string m_shaderDir;

    std::unique_ptr<ShadowMap> m_shadowMap;
    std::unique_ptr<GBuffer> m_gbuffer;
    std::unique_ptr<TextureArray> m_textureArray;

    Buffer m_lightingUboBuffer;

    std::unique_ptr<Pipeline> m_csmPipeline;
    std::unique_ptr<Pipeline> m_gbufferPipeline;
    std::unique_ptr<Pipeline> m_lightingPipeline;
    std::unique_ptr<Pipeline> m_postProcessPipeline;

    VkDescriptorSetLayout m_lightingDescLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_lightingDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_lightingDescSet = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_postDescLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_postDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_postDescSet = VK_NULL_HANDLE;
};

} // namespace prismcraft
