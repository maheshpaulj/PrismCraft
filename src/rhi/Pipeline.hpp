#pragma once
#include "RHICommon.hpp"
#include <string>
#include <vector>

namespace prismcraft {

class VulkanContext;

struct PushConstants {
    float mvp[16];          // 64 bytes (mat4)
    float sunDir[4];        // 16 bytes (vec4: xyz = dir, w = intensity)
    float skyFog[4];        // 16 bytes (vec4: xyz = fog color, w = fog distance)
    float camPos[4];        // 16 bytes (vec4: xyz = cam pos, w = time)
    float lightColor[4];    // 16 bytes (vec4: xyz = player pos, w = packed yaw/swing/torch)
    float pointLight1[4];   // 16 bytes (vec4: xyz = dropped torch pos, w = intensity)
    float pointLight2[4];   // 16 bytes (vec4: xyz = placed torch pos, w = intensity)
    float heldTorch[4];     // 16 bytes (vec4: xyz = exact held torch world pos, w = active)
};

enum class BlendMode {
    None = 0,
    Alpha = 1,
    Invert = 2
};

class Pipeline {
public:
    // Multi-render-target and depth-only constructor with optional depth bias and no fragment shader
    Pipeline(VulkanContext& context,
             const std::vector<VkFormat>& colorFormats,
             VkFormat depthFormat,
             const std::string& vertShaderPath,
             const std::string& fragShaderPath = "",
             VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE,
             bool enableDepthTest = true,
             bool enableDepthWrite = true,
             BlendMode blendMode = BlendMode::None,
             VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
             bool enableDepthBias = false,
             float depthBiasConstant = 0.0f,
             float depthBiasSlope = 0.0f,
             bool hasVertexInputs = true);

    Pipeline(VulkanContext& context, VkFormat colorFormat, VkFormat depthFormat,
             const std::string& vertShaderPath, const std::string& fragShaderPath,
             VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE,
             bool enableDepthTest = true, bool enableDepthWrite = true,
             BlendMode blendMode = BlendMode::None, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT);

    Pipeline(VulkanContext& context, VkFormat colorFormat, VkFormat depthFormat,
             const std::string& vertShaderPath, const std::string& fragShaderPath,
             VkDescriptorSetLayout descriptorSetLayout,
             bool enableDepthTest, bool enableDepthWrite,
             bool enableBlend, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    void bind(VkCommandBuffer cmd) const;
    [[nodiscard]] VkPipelineLayout getLayout() const { return m_pipelineLayout; }
    [[nodiscard]] VkPipeline getPipeline() const { return m_pipeline; }

private:
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    VulkanContext& m_context;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
};

} // namespace prismcraft
