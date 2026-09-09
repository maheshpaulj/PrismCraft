#include "DeferredRenderer.hpp"
#include "TextureAtlas.hpp"
#include "PBRGenerator.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include <cstring>
#include <stdexcept>

namespace prismcraft {

DeferredRenderer::DeferredRenderer(VulkanContext& context, CommandQueue& cmdQueue,
                                   uint32_t width, uint32_t height,
                                   VkFormat swapchainFormat, const std::string& shaderDir)
    : m_context(context), m_cmdQueue(cmdQueue),
      m_width(width), m_height(height),
      m_swapchainFormat(swapchainFormat), m_shaderDir(shaderDir) {

    // 1. Shadow Map (3-Cascade 2D Array)
    m_shadowMap = std::make_unique<ShadowMap>(m_context);

    // 2. Thin G-Buffer Targets
    m_gbuffer = std::make_unique<GBuffer>(m_context, m_width, m_height);

    // 3. PBR 2D Texture Array (Layer 0=Albedo, Layer 1=Normal, Layer 2=ORM)
    std::vector<uint8_t> albedoAtlas = TextureAtlas::generateAtlasPixels();
    std::vector<uint8_t> normalAtlas = PBRGenerator::generateNormalMap(albedoAtlas, TextureAtlas::ATLAS_WIDTH, TextureAtlas::ATLAS_HEIGHT);
    std::vector<uint8_t> ormAtlas = PBRGenerator::generateORMMap(albedoAtlas, TextureAtlas::ATLAS_WIDTH, TextureAtlas::ATLAS_HEIGHT);

    std::vector<const uint8_t*> layers = { albedoAtlas.data(), normalAtlas.data(), ormAtlas.data() };
    m_textureArray = std::make_unique<TextureArray>(m_context, m_cmdQueue,
                                                   TextureAtlas::ATLAS_WIDTH, TextureAtlas::ATLAS_HEIGHT,
                                                   3, layers);

    // 4. Lighting Uniform Buffer
    m_lightingUboBuffer = Buffer(m_context, sizeof(LightingUBO),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

    // 5. Descriptors & Pipelines
    try {
        createDescriptors();
        updateDescriptors();
        createPipelines();
    } catch (...) {
        cleanupDescriptors();
        throw;
    }
}

DeferredRenderer::~DeferredRenderer() {
    cleanupDescriptors();
}

void DeferredRenderer::recreate(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) return;
    m_width = width;
    m_height = height;

    m_gbuffer->recreate(m_width, m_height);
    updateDescriptors();
}

void DeferredRenderer::createDescriptors() {
    VkDevice device = m_context.getDevice();

    // 1. Deferred Lighting Descriptor Layout (Bindings: 0=Albedo, 1=Normal, 2=Depth, 3=ShadowMap, 4=UBO)
    std::array<VkDescriptorSetLayoutBinding, 5> lightingBindings{};
    for (uint32_t i = 0; i < 4; ++i) {
        lightingBindings[i].binding = i;
        lightingBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lightingBindings[i].descriptorCount = 1;
        lightingBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    lightingBindings[4].binding = 4;
    lightingBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingBindings[4].descriptorCount = 1;
    lightingBindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo lightingLayoutInfo{};
    lightingLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lightingLayoutInfo.bindingCount = static_cast<uint32_t>(lightingBindings.size());
    lightingLayoutInfo.pBindings = lightingBindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(device, &lightingLayoutInfo, nullptr, &m_lightingDescLayout),
             "Failed to create lighting descriptor set layout!");

    std::array<VkDescriptorPoolSize, 2> lightingPoolSizes{};
    lightingPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    lightingPoolSizes[0].descriptorCount = 4;
    lightingPoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingPoolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo lightingPoolInfo{};
    lightingPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    lightingPoolInfo.poolSizeCount = static_cast<uint32_t>(lightingPoolSizes.size());
    lightingPoolInfo.pPoolSizes = lightingPoolSizes.data();
    lightingPoolInfo.maxSets = 1;
    VK_CHECK(vkCreateDescriptorPool(device, &lightingPoolInfo, nullptr, &m_lightingDescPool),
             "Failed to create lighting descriptor pool!");

    VkDescriptorSetAllocateInfo lightingAllocInfo{};
    lightingAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    lightingAllocInfo.descriptorPool = m_lightingDescPool;
    lightingAllocInfo.descriptorSetCount = 1;
    lightingAllocInfo.pSetLayouts = &m_lightingDescLayout;
    VK_CHECK(vkAllocateDescriptorSets(device, &lightingAllocInfo, &m_lightingDescSet),
             "Failed to allocate lighting descriptor set!");

    // 2. Post-Process Descriptor Layout (Binding 0=HDR Scene Texture)
    VkDescriptorSetLayoutBinding postBinding{};
    postBinding.binding = 0;
    postBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postBinding.descriptorCount = 1;
    postBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo postLayoutInfo{};
    postLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    postLayoutInfo.bindingCount = 1;
    postLayoutInfo.pBindings = &postBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &postLayoutInfo, nullptr, &m_postDescLayout),
             "Failed to create post-process descriptor set layout!");

    VkDescriptorPoolSize postPoolSize{};
    postPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postPoolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo postPoolInfo{};
    postPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    postPoolInfo.poolSizeCount = 1;
    postPoolInfo.pPoolSizes = &postPoolSize;
    postPoolInfo.maxSets = 1;
    VK_CHECK(vkCreateDescriptorPool(device, &postPoolInfo, nullptr, &m_postDescPool),
             "Failed to create post-process descriptor pool!");

    VkDescriptorSetAllocateInfo postAllocInfo{};
    postAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    postAllocInfo.descriptorPool = m_postDescPool;
    postAllocInfo.descriptorSetCount = 1;
    postAllocInfo.pSetLayouts = &m_postDescLayout;
    VK_CHECK(vkAllocateDescriptorSets(device, &postAllocInfo, &m_postDescSet),
             "Failed to allocate post-process descriptor set!");
}

void DeferredRenderer::updateDescriptors() {
    VkDevice device = m_context.getDevice();

    // Lighting Descriptor Set writes
    VkDescriptorImageInfo albedoInfo{ m_gbuffer->getSampler(), m_gbuffer->getAlbedoView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo normalInfo{ m_gbuffer->getSampler(), m_gbuffer->getNormalRoughnessView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo depthInfo{ m_gbuffer->getSampler(), m_gbuffer->getDepthView(), VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo shadowInfo{ m_shadowMap->getSampler(), m_shadowMap->getArrayImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorBufferInfo uboInfo{ m_lightingUboBuffer.getBuffer(), 0, sizeof(LightingUBO) };

    std::array<VkWriteDescriptorSet, 5> writes{};
    writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_lightingDescSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &albedoInfo, nullptr, nullptr };
    writes[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_lightingDescSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, nullptr, nullptr };
    writes[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_lightingDescSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, nullptr, nullptr };
    writes[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_lightingDescSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowInfo, nullptr, nullptr };
    writes[4] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_lightingDescSet, 4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uboInfo, nullptr };

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    // Post-Process Descriptor Set writes
    VkDescriptorImageInfo hdrInfo{ m_gbuffer->getSampler(), m_gbuffer->getHdrSceneView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet postWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_postDescSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &hdrInfo, nullptr, nullptr };
    vkUpdateDescriptorSets(device, 1, &postWrite, 0, nullptr);
}

void DeferredRenderer::cleanupDescriptors() {
    VkDevice device = m_context.getDevice();
    if (m_lightingDescPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_lightingDescPool, nullptr);
        m_lightingDescPool = VK_NULL_HANDLE;
    }
    if (m_lightingDescLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_lightingDescLayout, nullptr);
        m_lightingDescLayout = VK_NULL_HANDLE;
    }
    if (m_postDescPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_postDescPool, nullptr);
        m_postDescPool = VK_NULL_HANDLE;
    }
    if (m_postDescLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_postDescLayout, nullptr);
        m_postDescLayout = VK_NULL_HANDLE;
    }
}

void DeferredRenderer::createPipelines() {
    // 1. CSM Depth-Only Shadow Pipeline (Hardware Depth Bias ON, Alpha Cutout ON for Fancy Leaves)
    m_csmPipeline = std::make_unique<Pipeline>(
        m_context, std::vector<VkFormat>{}, m_shadowMap->getFormat(),
        m_shaderDir + "assets/shaders/csm_depth.vert.spv",
        m_shaderDir + "assets/shaders/csm_depth.frag.spv",
        m_textureArray->getDescriptorSetLayout(),
        true, true, BlendMode::None, VK_CULL_MODE_BACK_BIT,
        true, 1.25f, 1.75f, true);

    // 2. G-Buffer Opaque Pipeline (MRT: RT0 Albedo/Roughness, RT1 Normal/Metallic)
    std::vector<VkFormat> gbufferFormats = { m_gbuffer->getAlbedoFormat(), m_gbuffer->getNormalRoughnessFormat() };
    m_gbufferPipeline = std::make_unique<Pipeline>(
        m_context, gbufferFormats, m_gbuffer->getDepthFormat(),
        m_shaderDir + "assets/shaders/gbuffer.vert.spv",
        m_shaderDir + "assets/shaders/gbuffer.frag.spv",
        m_textureArray->getDescriptorSetLayout(),
        true, true, BlendMode::None, VK_CULL_MODE_BACK_BIT,
        false, 0.0f, 0.0f, true);

    // 3. Deferred Lighting Pipeline (Fullscreen pass targeting HDR_SceneColor)
    std::vector<VkFormat> lightingFormats = { m_gbuffer->getHdrSceneFormat() };
    m_lightingPipeline = std::make_unique<Pipeline>(
        m_context, lightingFormats, VK_FORMAT_UNDEFINED,
        m_shaderDir + "assets/shaders/fullscreen.vert.spv",
        m_shaderDir + "assets/shaders/deferred_lighting.frag.spv",
        m_lightingDescLayout,
        false, false, BlendMode::None, VK_CULL_MODE_NONE,
        false, 0.0f, 0.0f, false);

    // 4. Post-Process Tonemapping Pipeline (Fullscreen pass targeting Swapchain format)
    std::vector<VkFormat> postFormats = { m_swapchainFormat };
    m_postProcessPipeline = std::make_unique<Pipeline>(
        m_context, postFormats, VK_FORMAT_UNDEFINED,
        m_shaderDir + "assets/shaders/fullscreen.vert.spv",
        m_shaderDir + "assets/shaders/post_process.frag.spv",
        m_postDescLayout,
        false, false, BlendMode::None, VK_CULL_MODE_NONE,
        false, 0.0f, 0.0f, false);
}

void DeferredRenderer::updateCascades(const glm::mat4& viewMatrix, float fov, float aspect,
                                     float nearClip, float farClip, const glm::vec3& lightDir) {
    m_shadowMap->updateCascades(viewMatrix, fov, aspect, nearClip, farClip, lightDir);
}

void DeferredRenderer::renderDeferredLighting(VkCommandBuffer cmd,
                                             const glm::mat4& invViewProj,
                                             const glm::vec3& sunDir, float sunIntensity,
                                             const glm::vec3& camPos, float time,
                                             const glm::vec3& skyColor,
                                             const glm::vec4& heldTorch,
                                             const glm::vec4& pointLight1,
                                             const glm::vec4& fogParams) {
    // 1. Update Lighting UBO
    LightingUBO ubo{};
    ubo.invViewProj = invViewProj;
    const auto& shadowMats = m_shadowMap->getCascadeViewProjections();
    ubo.lightViewProj[0] = shadowMats[0];
    ubo.lightViewProj[1] = shadowMats[1];
    ubo.lightViewProj[2] = shadowMats[2];
    const auto& splits = m_shadowMap->getCascadeSplits();
    ubo.cascadeSplits = glm::vec4(splits[0], splits[1], splits[2], 0.0f);
    ubo.sunDir = glm::vec4(sunDir, sunIntensity);
    ubo.camPos = glm::vec4(camPos, time);
    ubo.skyColor = glm::vec4(skyColor, 1.0f);
    ubo.heldTorch = heldTorch;
    ubo.pointLight1 = pointLight1;
    ubo.fogParams = fogParams;

    m_lightingUboBuffer.upload(&ubo, sizeof(LightingUBO));

    // 2. Barrier transitions for reading G-Buffer & Shadow map
    m_gbuffer->transitionForLightingPass(cmd);
    m_shadowMap->transitionForSampling(cmd);

    // 3. Dynamic Rendering pass targeting HDR_SceneColor
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_gbuffer->getHdrSceneView();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{ 0.0f, 0.0f, 0.0f, 1.0f }};

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = {{ 0, 0 }, { m_width, m_height }};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderInfo);

    VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{{ 0, 0 }, { m_width, m_height }};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_lightingPipeline->bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_lightingPipeline->getLayout(), 0, 1,
                            &m_lightingDescSet, 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0); // Fullscreen triangle without vertex buffer

    vkCmdEndRendering(cmd);
}

void DeferredRenderer::renderPostProcess(VkCommandBuffer cmd, VkImageView swapchainImageView, VkExtent2D extent) {
    // 1. Transition HDR_SceneColor to SHADER_READ_ONLY_OPTIMAL
    m_gbuffer->transitionForPostProcess(cmd);

    // 2. Dynamic Rendering pass targeting Swapchain image
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapchainImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = {{ 0, 0 }, extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderInfo);

    VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{{ 0, 0 }, extent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_postProcessPipeline->bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_postProcessPipeline->getLayout(), 0, 1,
                            &m_postDescSet, 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0); // Fullscreen triangle

    vkCmdEndRendering(cmd);
}

} // namespace prismcraft
