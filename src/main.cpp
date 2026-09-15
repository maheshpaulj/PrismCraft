#include "core/Window.hpp"
#include "core/Input.hpp"
#include "core/Timer.hpp"
#include "core/ConfigManager.hpp"
#include "audio/AudioEngine.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/Swapchain.hpp"
#include "rhi/CommandQueue.hpp"
#include "rhi/Pipeline.hpp"
#include "rhi/Buffer.hpp"
#include "rhi/Texture.hpp"
#include "rhi/ShadowMap.hpp"
#include "player/Player.hpp"
#include "player/Raycast.hpp"
#include "world/World.hpp"
#include "world/FallingBlockManager.hpp"
#include "renderer/TextureAtlas.hpp"
#include "renderer/HandRenderer.hpp"
#include "renderer/BlockOutlineRenderer.hpp"
#include "renderer/CelestialRenderer.hpp"
#include "renderer/ItemDropRenderer.hpp"
#include "renderer/BlockCrackRenderer.hpp"
#include "renderer/CloudRenderer.hpp"
#include "renderer/SkyRenderer.hpp"
#include "renderer/CloudMapGenerator.hpp"
#include "renderer/LightOverlayRenderer.hpp"
#include "ui/UIRenderer.hpp"
#include "ui/MenuRenderer.hpp"
#include "renderer/PlayerModelRenderer.hpp"
#include "renderer/PostProcessRenderer.hpp"
#include "renderer/TSRRenderer.hpp"
#include "game/ArrowManager.hpp"
#include "data/BlockRegistry.hpp"
#include "world/SaveManager.hpp"
#include "world/Biome.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <filesystem>
#include <memory>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

static std::string getExeDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path().string() + "/";
}

static void checkAndAutoRecompileShaders(const std::string& exeDir) {
    std::string glslc = "D:\\Softwares\\Vulkan SDK\\Bin\\glslc.exe";
    const char* vksdk = getenv("VULKAN_SDK");
    if (vksdk && std::filesystem::exists(std::string(vksdk) + "\\Bin\\glslc.exe")) {
        glslc = std::string(vksdk) + "\\Bin\\glslc.exe";
    } else if (!std::filesystem::exists(glslc)) {
        glslc = "glslc.exe";
    }

    std::filesystem::path exeDirPath(exeDir);
    if (exeDirPath.filename().empty()) {
        exeDirPath = exeDirPath.parent_path();
    }

    std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path() / "assets" / "shaders",
        exeDirPath / "assets" / "shaders",
        exeDirPath.parent_path() / "assets" / "shaders",
        exeDirPath.parent_path().parent_path() / "assets" / "shaders",
        exeDirPath.parent_path().parent_path().parent_path() / "assets" / "shaders"
    };

    std::filesystem::path projShaders;
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c) && std::filesystem::exists(c / "cloud.frag")) {
            projShaders = c;
            break;
        }
    }

    if (projShaders.empty() || !std::filesystem::exists(projShaders)) {
        return;
    }

    std::filesystem::path exeShaders = std::filesystem::path(exeDir) / "assets" / "shaders";
    if (!std::filesystem::exists(exeShaders)) {
        try { std::filesystem::create_directories(exeShaders); } catch (...) {}
    }

    bool anyRecompiled = false;
    for (const auto& entry : std::filesystem::directory_iterator(projShaders)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext == ".frag" || ext == ".vert" || ext == ".comp") {
            std::filesystem::path srcFile = entry.path();
            std::filesystem::path spvProj = srcFile.string() + ".spv";
            std::filesystem::path spvExe = exeShaders / (srcFile.filename().string() + ".spv");

            bool needsRecompile = false;
            auto srcTime = std::filesystem::last_write_time(srcFile);

            if (!std::filesystem::exists(spvProj) || srcTime > std::filesystem::last_write_time(spvProj)) {
                needsRecompile = true;
            }
            if (!std::filesystem::exists(spvExe) || srcTime > std::filesystem::last_write_time(spvExe)) {
                needsRecompile = true;
            }

            if (needsRecompile) {
                // Windows cmd.exe /c strips outermost quotes when multiple quoted args exist;
                // wrapping in an extra outer pair ensures "glslc with spaces" executes correctly.
                std::string cmd = "\"\"" + glslc + "\" \"" + srcFile.string() + "\" -o \"" + spvProj.string() + "\"\"";
                int ret = std::system(cmd.c_str());
                if (ret == 0) {
                    try {
                        std::filesystem::copy_file(spvProj, spvExe, std::filesystem::copy_options::overwrite_existing);
                    } catch (...) {}
                    std::cout << "[ShaderAutoCompiler] Recompiled " << srcFile.filename().string() << " -> .spv successfully!" << std::endl;
                    anyRecompiled = true;
                } else {
                    std::cerr << "[ShaderAutoCompiler] ERROR: glslc failed on " << srcFile.filename().string() << std::endl;
                }
            } else if (std::filesystem::exists(spvProj)) {
                if (!std::filesystem::exists(spvExe) || std::filesystem::last_write_time(spvProj) > std::filesystem::last_write_time(spvExe)) {
                    try {
                        std::filesystem::copy_file(spvProj, spvExe, std::filesystem::copy_options::overwrite_existing);
                        std::cout << "[ShaderAutoCompiler] Synced " << spvProj.filename().string() << " to exe directory." << std::endl;
                    } catch (...) {}
                }
            }
        }
    }
    if (anyRecompiled) {
        std::cout << "[ShaderAutoCompiler] All modified shaders recompiled and updated live." << std::endl;
    }
}

namespace prismcraft {

static float calculateBreakDuration(BlockType block, BlockType tool) {
    if (Cell{block}.isFoliage() || Cell{block}.isTorch() || Cell{block}.isLantern()) {
        return 0.05f; // Instant break for plants, flowers, tall grass, torches, lanterns
    }

    float baseTime = 1.0f;
    bool requiresPickaxe = false;
    bool isWoodBlock = (block == BlockType::Wood || block == BlockType::Planks || block == BlockType::CraftingTable ||
                        block == BlockType::DoorWood || block == BlockType::Bed ||
                        (block >= BlockType::PlanksPine && block <= BlockType::PlanksJungle) ||
                        (block >= BlockType::LogSpruce && block <= BlockType::LogAcacia) ||
                        block == BlockType::PlanksDarkOak || block == BlockType::PlanksAcacia ||
                        Cell{block}.isTrapdoor() || (block >= BlockType::DoorSpruce && block <= BlockType::DoorAcacia) ||
                        block == BlockType::Barrel);
    bool isStoneBlock = (block == BlockType::Stone || block == BlockType::CobbleStone || block == BlockType::Furnace ||
                         block == BlockType::SmoothStone || block == BlockType::DoorIron ||
                         (block >= BlockType::Andesite && block <= BlockType::MudBricks) ||
                         block == BlockType::TrapdoorIron || block == BlockType::Smoker || block == BlockType::BlastFurnace);
    bool isOreBlock = (block == BlockType::OreCoal || block == BlockType::OreIron || block == BlockType::OreGold ||
                       block == BlockType::OreDiamond || block == BlockType::OreRedstone || block == BlockType::OreEmerald ||
                       block == BlockType::OreNetherQuartz || block == BlockType::OreCopper || block == BlockType::OreLapis ||
                       (block >= BlockType::DeepslateCoal && block <= BlockType::DeepslateCopper));
    bool isDirtBlock = (block == BlockType::Grass || block == BlockType::Dirt || block == BlockType::Sand ||
                        block == BlockType::Gravel || block == BlockType::Snow || block == BlockType::CoarseDirt);

    if (isWoodBlock) {
        baseTime = (block == BlockType::DoorWood || block == BlockType::Bed) ? 0.75f : 1.8f;
    } else if (isStoneBlock) {
        baseTime = (block == BlockType::DoorIron) ? 3.0f : 3.2f;
        requiresPickaxe = true;
    } else if (isOreBlock) {
        baseTime = 4.8f;
        requiresPickaxe = true;
    } else if (isDirtBlock) {
        baseTime = 0.65f;
    } else if (block == BlockType::Torch || block == BlockType::Lantern || Cell{block}.isFoliage()) {
        baseTime = 0.12f;
    } else if (block == BlockType::Bedrock) {
        return 999999.0f;
    }

    float speedMultiplier = 1.0f;
    int tier = Cell{tool}.getToolTier();
    float tierMultiplier = 1.0f;
    switch (tier) {
        case 1: tierMultiplier = 2.0f; break;  // Wood
        case 2: tierMultiplier = 4.0f; break;  // Stone
        case 3: tierMultiplier = 6.0f; break;  // Iron
        case 4: tierMultiplier = 12.0f; break; // Gold (ultra fast)
        case 5: tierMultiplier = 8.5f; break;  // Diamond
        default: tierMultiplier = 1.0f; break;
    }

    if (Cell{tool}.isAxe() && isWoodBlock) {
        speedMultiplier = tierMultiplier;
    } else if (Cell{tool}.isPickaxe() && (isStoneBlock || isOreBlock)) {
        speedMultiplier = tierMultiplier;
    } else if (Cell{tool}.isShovel() && isDirtBlock) {
        speedMultiplier = tierMultiplier;
    } else if (Cell{tool}.isSword()) {
        if (block == BlockType::Leaves) speedMultiplier = 4.0f;
        else speedMultiplier = 1.5f;
    } else if (Cell{tool}.isHoe()) {
        if (block == BlockType::Leaves || Cell{block}.isFoliage()) speedMultiplier = 5.0f;
    }

    if (requiresPickaxe && !Cell{tool}.isPickaxe()) {
        speedMultiplier = 0.5f;
    }

    return std::max(0.05f, baseTime / speedMultiplier);
}

void run() {
    std::string exeDir = getExeDir();
    checkAndAutoRecompileShaders(exeDir);
    GameOptions options;
    ConfigManager::load(options, exeDir + "options.txt");

    // Clamp terrain chunk streaming distance to realistic bounds [4, 48]
    options.renderDistance = std::clamp(options.renderDistance, 4, 48);

    static const int resList[4][2] = {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}};
    int initialW = (options.resIndex < 4) ? resList[options.resIndex][0] : 1920;
    int initialH = (options.resIndex < 4) ? resList[options.resIndex][1] : 1080;

    auto computeInternalResolution = [](const GameOptions& opt, int winW, int winH) -> std::pair<uint32_t, uint32_t> {
        uint32_t baseW = static_cast<uint32_t>(std::max(winW, 1));
        uint32_t baseH = static_cast<uint32_t>(std::max(winH, 1));

        if (opt.upscalerMode == 0) { // Off / Native
            static const int resTable[4][2] = {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}};
            if (opt.resIndex >= 0 && opt.resIndex < 4) {
                return { static_cast<uint32_t>(resTable[opt.resIndex][0]), static_cast<uint32_t>(resTable[opt.resIndex][1]) };
            }
            return { baseW, baseH };
        }

        // Upscaler Quality Presets (FSR 1.0 Spatial & TSR Temporal):
        // 0: Ultra Quality (1.3x scale -> ~77% render scale)
        // 1: Quality (1.5x scale -> ~67% render scale)
        // 2: Balanced (1.7x scale -> ~59% render scale)
        // 3: Performance (2.0x scale -> ~50% render scale)
        float scale = 0.67f;
        switch (opt.upscalerQuality) {
            case 0: scale = 0.77f; break;
            case 1: scale = 0.67f; break;
            case 2: scale = 0.59f; break;
            case 3: scale = 0.50f; break;
            default: scale = 0.67f; break;
        }

        uint32_t rw = static_cast<uint32_t>(std::round(baseW * scale));
        uint32_t rh = static_cast<uint32_t>(std::round(baseH * scale));
        if (rw % 2 != 0) rw++;
        if (rh % 2 != 0) rh++;
        rw = std::max(rw, 320u);
        rh = std::max(rh, 240u);
        return { rw, rh };
    };

    Window window("PrismCraft", initialW, initialH);
    window.setWindowMode(options.windowMode, initialW, initialH, window.getRefreshRate());
    Input::init(window.getHandle());
    
    // Initialize procedural Audio Engine
    if (AudioEngine::get().init()) {
        std::cout << "[Audio] AudioEngine initialized." << std::endl;
    }

    VulkanContext context(window.getHandle());
    Swapchain swapchain(context, window.getWidth(), window.getHeight(), options.vsync);
    CommandQueue commandQueue(context);

    auto [initialRenderW, initialRenderH] = computeInternalResolution(options, window.getWidth(), window.getHeight());
    PostProcessRenderer postProcessRenderer(context, initialRenderW, initialRenderH, swapchain.getImageFormat(), exeDir);
    TSRRenderer tsrRenderer(context, window.getWidth(), window.getHeight(), swapchain.getImageFormat(), exeDir);
    
    // Initialize BlockRegistry and generate unified 1024x1024 Pixel-Art Texture Atlas
    BlockRegistry::init(exeDir + "assets/blocks.json");
    std::vector<uint8_t> atlasPixels = TextureAtlas::generateAtlasPixels(exeDir + "assets/");
    Texture textureAtlas(context, commandQueue, TextureAtlas::ATLAS_WIDTH, TextureAtlas::ATLAS_HEIGHT, atlasPixels.data(), false, false, 0);
    VkDescriptorSetLayout descLayout = textureAtlas.getDescriptorSetLayout();
    VkDescriptorSet descSet = textureAtlas.getDescriptorSet();

    // Generate or Load Cached 512x512 Seamless Cloud Noise Map (Worley + Perlin seeded)
    std::unique_ptr<Texture> cloudTexture = CloudMapGenerator::createCloudTexture(context, commandQueue, static_cast<uint32_t>(options.cloudSeed), exeDir);
    VkDescriptorSet cloudDescSet = cloudTexture->getDescriptorSet();
    int currentCloudSeed = options.cloudSeed;

    struct ShadowUBO {
        glm::mat4 lightViewProj[2];
        glm::vec4 cascadeSplits;
    };

    ShadowMap shadowMap(context);

    // Initial transition of shadow map to shader read-only so descriptor sets and samplers are always valid
    {
        VkCommandBuffer initCmd = commandQueue.beginSingleTimeCommands();
        shadowMap.transitionForSampling(initCmd);
        commandQueue.endSingleTimeCommands(initCmd);
    }

    Buffer shadowUboBuffers[MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        shadowUboBuffers[i] = Buffer(context, sizeof(ShadowUBO),
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    // Scene Descriptor Set Layout: binding 0 = Atlas, binding 1 = ShadowMap array, binding 2 = ShadowUBO
    VkDescriptorSetLayoutBinding sceneBindings[3]{};
    sceneBindings[0].binding = 0;
    sceneBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sceneBindings[0].descriptorCount = 1;
    sceneBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    sceneBindings[1].binding = 1;
    sceneBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sceneBindings[1].descriptorCount = 1;
    sceneBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    sceneBindings[2].binding = 2;
    sceneBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneBindings[2].descriptorCount = 1;
    sceneBindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo sceneLayoutInfo{};
    sceneLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sceneLayoutInfo.bindingCount = 3;
    sceneLayoutInfo.pBindings = sceneBindings;

    VkDescriptorSetLayout sceneDescLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(context.getDevice(), &sceneLayoutInfo, nullptr, &sceneDescLayout),
             "Failed to create scene descriptor set layout!");

    // Descriptor pool for scene descriptor sets
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 2 * MAX_FRAMES_IN_FLIGHT;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPool sceneDescPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(context.getDevice(), &poolInfo, nullptr, &sceneDescPool),
             "Failed to create scene descriptor pool!");

    std::vector<VkDescriptorSetLayout> sceneLayouts(MAX_FRAMES_IN_FLIGHT, sceneDescLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = sceneDescPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = sceneLayouts.data();

    VkDescriptorSet sceneDescSets[MAX_FRAMES_IN_FLIGHT]{};
    VK_CHECK(vkAllocateDescriptorSets(context.getDevice(), &allocInfo, sceneDescSets),
             "Failed to allocate scene descriptor sets!");

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorImageInfo atlasImageInfo{};
        atlasImageInfo.sampler = textureAtlas.getSampler();
        atlasImageInfo.imageView = textureAtlas.getImageView();
        atlasImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo shadowImageInfo{};
        shadowImageInfo.sampler = shadowMap.getSampler();
        shadowImageInfo.imageView = shadowMap.getArrayImageView();
        shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = shadowUboBuffers[i].getBuffer();
        uboInfo.offset = 0;
        uboInfo.range = sizeof(ShadowUBO);

        VkWriteDescriptorSet writes[3]{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = sceneDescSets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &atlasImageInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = sceneDescSets[i];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &shadowImageInfo;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = sceneDescSets[i];
        writes[2].dstBinding = 2;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[2].descriptorCount = 1;
        writes[2].pBufferInfo = &uboInfo;

        vkUpdateDescriptorSets(context.getDevice(), 3, writes, 0, nullptr);
    }

    // -------------------------------------------------------------
    // Water Descriptor Set Layout (Binding 0=Atlas, 1=ShadowMap, 2=ShadowUBO, 3=SSRTexture)
    // -------------------------------------------------------------
    VkDescriptorSetLayoutBinding waterBindings[5]{};
    waterBindings[0] = sceneBindings[0];
    waterBindings[1] = sceneBindings[1];
    waterBindings[2] = sceneBindings[2];
    waterBindings[3].binding = 3;
    waterBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    waterBindings[3].descriptorCount = 1;
    waterBindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    waterBindings[4].binding = 4;
    waterBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    waterBindings[4].descriptorCount = 1;
    waterBindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo waterLayoutInfo{};
    waterLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    waterLayoutInfo.bindingCount = 5;
    waterLayoutInfo.pBindings = waterBindings;

    VkDescriptorSetLayout waterDescLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(context.getDevice(), &waterLayoutInfo, nullptr, &waterDescLayout),
             "Failed to create water descriptor set layout!");

    VkDescriptorPoolSize waterPoolSizes[2]{};
    waterPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    waterPoolSizes[0].descriptorCount = 4 * MAX_FRAMES_IN_FLIGHT;
    waterPoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    waterPoolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo waterPoolInfo{};
    waterPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    waterPoolInfo.poolSizeCount = 2;
    waterPoolInfo.pPoolSizes = waterPoolSizes;
    waterPoolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPool waterDescPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(context.getDevice(), &waterPoolInfo, nullptr, &waterDescPool),
             "Failed to create water descriptor pool!");

    std::vector<VkDescriptorSetLayout> waterLayouts(MAX_FRAMES_IN_FLIGHT, waterDescLayout);
    VkDescriptorSetAllocateInfo waterAllocInfo{};
    waterAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    waterAllocInfo.descriptorPool = waterDescPool;
    waterAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    waterAllocInfo.pSetLayouts = waterLayouts.data();

    VkDescriptorSet waterDescSets[MAX_FRAMES_IN_FLIGHT]{};
    VK_CHECK(vkAllocateDescriptorSets(context.getDevice(), &waterAllocInfo, waterDescSets),
             "Failed to allocate water descriptor sets!");

    auto updateWaterDescriptorSets = [&]() {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo atlasImageInfo{};
            atlasImageInfo.sampler = textureAtlas.getSampler();
            atlasImageInfo.imageView = textureAtlas.getImageView();
            atlasImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo shadowImageInfo{};
            shadowImageInfo.sampler = shadowMap.getSampler();
            shadowImageInfo.imageView = shadowMap.getArrayImageView();
            shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorBufferInfo uboInfo{};
            uboInfo.buffer = shadowUboBuffers[i].getBuffer();
            uboInfo.offset = 0;
            uboInfo.range = sizeof(ShadowUBO);

            VkDescriptorImageInfo ssrImageInfo{};
            ssrImageInfo.sampler = postProcessRenderer.getSSRSampler();
            ssrImageInfo.imageView = postProcessRenderer.getSSRImageView();
            ssrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo depthImageInfo{};
            depthImageInfo.sampler = postProcessRenderer.getSSRDepthSampler();
            depthImageInfo.imageView = postProcessRenderer.getSSRDepthImageView();
            depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet writes[5]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = waterDescSets[i];
            writes[0].dstBinding = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].descriptorCount = 1;
            writes[0].pImageInfo = &atlasImageInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = waterDescSets[i];
            writes[1].dstBinding = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &shadowImageInfo;

            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = waterDescSets[i];
            writes[2].dstBinding = 2;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[2].descriptorCount = 1;
            writes[2].pBufferInfo = &uboInfo;

            writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet = waterDescSets[i];
            writes[3].dstBinding = 3;
            writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[3].descriptorCount = 1;
            writes[3].pImageInfo = &ssrImageInfo;

            writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[4].dstSet = waterDescSets[i];
            writes[4].dstBinding = 4;
            writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[4].descriptorCount = 1;
            writes[4].pImageInfo = &depthImageInfo;

            vkUpdateDescriptorSets(context.getDevice(), 5, writes, 0, nullptr);
        }
    };

    updateWaterDescriptorSets();

    // 3D Directional CSM Shadow Pipeline (Depth only, No color attachments, Double-sided for foliage shadow cutout)
    Pipeline csmPipeline(context, {}, shadowMap.getFormat(),
                         exeDir + "assets/shaders/csm_depth.vert.spv",
                         exeDir + "assets/shaders/csm_depth.frag.spv",
                         descLayout, true, true, BlendMode::None,
                         VK_CULL_MODE_NONE, true, 1.25f, 1.75f);

    // 3D Opaque World Pipeline (Back-face culling, Depth test & write ON, Alpha Blending OFF)
    Pipeline worldPipeline(context, postProcessRenderer.getHDRFormat(), swapchain.getDepthFormat(),
                           exeDir + "assets/shaders/cell.vert.spv", exeDir + "assets/shaders/cell.frag.spv",
                           sceneDescLayout, true, true, BlendMode::None, VK_CULL_MODE_BACK_BIT);

    // 3D Translucent Water Pipeline (Double-sided, Depth test ON, Depth write OFF, Alpha Blending ON)
    Pipeline waterPipeline(context, postProcessRenderer.getHDRFormat(), swapchain.getDepthFormat(),
                           exeDir + "assets/shaders/water.vert.spv", exeDir + "assets/shaders/water.frag.spv",
                           waterDescLayout, true, false, BlendMode::Alpha, VK_CULL_MODE_NONE);

    // 3D Volumetric Cloud Pipeline (Sky dome, Depth test ON, Depth write OFF, Alpha Blending ON)
    Pipeline cloudPipeline(context, postProcessRenderer.getHDRFormat(), swapchain.getDepthFormat(),
                           exeDir + "assets/shaders/cloud.vert.spv", exeDir + "assets/shaders/cloud.frag.spv",
                           descLayout, true, false, BlendMode::Alpha, VK_CULL_MODE_NONE);

    // 3D Procedural Atmospheric Sky Pipeline (Sky sphere, Depth test ON, Depth write OFF, Blend None, Cull None)
    Pipeline skyPipeline(context, postProcessRenderer.getHDRFormat(), swapchain.getDepthFormat(),
                         exeDir + "assets/shaders/sky.vert.spv", exeDir + "assets/shaders/sky.frag.spv",
                         descLayout, true, false, BlendMode::None, VK_CULL_MODE_NONE);

    // 3D Invert Pipeline for Block Wireframe (Depth test ON, Depth write OFF, BlendMode::Invert)
    Pipeline outlineInvertPipeline(context, postProcessRenderer.getHDRFormat(), swapchain.getDepthFormat(),
                                   exeDir + "assets/shaders/cell.vert.spv", exeDir + "assets/shaders/cell.frag.spv",
                                   sceneDescLayout, true, false, BlendMode::Invert, VK_CULL_MODE_NONE);

    // 3D Block Crack Mining Overlay Pipeline (Depth test ON, Depth write OFF, Multiplicative Blending ON, Depth Bias enabled)
    Pipeline crackPipeline(context,
                           std::vector<VkFormat>{ postProcessRenderer.getHDRFormat() },
                           swapchain.getDepthFormat(),
                           exeDir + "assets/shaders/cell.vert.spv",
                           exeDir + "assets/shaders/cell.frag.spv",
                           sceneDescLayout,
                           true, false, BlendMode::Multiply, VK_CULL_MODE_NONE,
                           true, -2.0f, -2.0f, true);

    // 2D UI Pipeline (Targeting Swapchain sRGB image directly, Depth test/write OFF)
    Pipeline uiPipeline(context, swapchain.getImageFormat(), swapchain.getDepthFormat(),
                        exeDir + "assets/shaders/ui.vert.spv", exeDir + "assets/shaders/ui.frag.spv",
                        descLayout, false, false, BlendMode::Alpha, VK_CULL_MODE_NONE);

    // 2D Invert Pipeline for Crosshair (No culling, Depth test & write OFF, BlendMode::Invert)
    Pipeline invertPipeline(context, swapchain.getImageFormat(), swapchain.getDepthFormat(),
                            exeDir + "assets/shaders/ui.vert.spv", exeDir + "assets/shaders/ui.frag.spv",
                            descLayout, false, false, BlendMode::Invert, VK_CULL_MODE_NONE);
    
    std::cout << "[Main] Pipelines created, creating World..." << std::endl;
    uint32_t currentSeed = 42;
    std::string currentWorldFolder = "";
    std::unique_ptr<World> world = std::make_unique<World>(context, commandQueue, currentSeed, currentWorldFolder);
    world->renderDistance = options.renderDistance;
    world->lodDistance = options.lodDistance;
    world->lodPreset = options.lodPreset;
    world->setRamCacheSize(options.ramCacheSize);
    
    std::cout << "[Main] World created, placing player..." << std::endl;
    float spawnY = world->getHighestSolidY(0.0f, 0.0f);
    Player player(glm::vec3(0.5f, spawnY + 2.5f, 0.5f));
    if (player.getHotbar()[3] == BlockType::Air) {
        player.setHotbarBlock(3, BlockType::DoorWood, 4);
    }
    if (player.getHotbar()[4] == BlockType::Air) {
        player.setHotbarBlock(4, BlockType::Bed, 2);
    }
    player.getCamera().fov = static_cast<float>(options.fov);
    player.getCamera().farPlane = std::max(500.0f, static_cast<float>(options.lodDistance) * 16.0f * 1.42f);
    player.mouseSensitivity = options.mouseSens;
    AudioEngine::get().setMasterVolume(options.audioVolume);
    AudioEngine::get().setMusicVolume(options.musicVolume);
    AudioEngine::get().setBlocksVolume(options.blocksVolume);
    AudioEngine::get().setPlayerVolume(options.playerVolume);
    AudioEngine::get().setAmbientVolume(options.ambientVolume);
    
    std::cout << "[Main] Creating auxiliary renderers..." << std::endl;
    HandRenderer handRenderer(context, commandQueue);
    PlayerModelRenderer playerModelRenderer(context, commandQueue);
    UIRenderer uiRenderer(context, commandQueue);
    BlockOutlineRenderer outlineRenderer(context, commandQueue);
    MenuRenderer menuRenderer(context, commandQueue);
    CelestialRenderer celestialRenderer(context, commandQueue);
    ItemDropManager itemDropManager(context, commandQueue);
    BlockCrackRenderer crackRenderer(context, commandQueue);
    FallingBlockManager fallingBlockManager(context, commandQueue);
    ArrowManager arrowManager(context, commandQueue);
    CloudRenderer cloudRenderer(context, commandQueue);
    SkyRenderer skyRenderer(context, commandQueue);
    LightOverlayRenderer lightOverlayRenderer(context, commandQueue);
    
    std::cout << "[Main] All renderers initialized, entering main game loop!" << std::endl;
    GameState state = GameState::MainMenu;
    window.setCursorMode(false);
    
    Timer timer;
    float fpsTimer = 0.0f;
    int fpsFrameCount = 0;
    float displayedFPS = 60.0f;
    float menuCamAngle = 0.0f;
    float timeOfDay = 0.0416667f; // Start at morning (1000 ticks)
    bool isTimePaused = false;
    uint32_t gameDay = 0;
    bool isSleeping = false;
    float sleepTimer = 0.0f;
    float sleepFadeAlpha = 0.0f;
    glm::vec3 sleepingBedPos{0.0f};
    glm::vec3 preSleepPlayerPos{0.0f};
    float preSleepPitch = 0.0f;
    float preSleepYaw = 0.0f;
    
    bool isChatOpen = false;
    std::string chatInput = "";
    std::string chatFeedback = "";
    float chatFeedbackTimer = 0.0f;
    
    float miningTimer = 0.0f;
    std::optional<CellCoord> currentMiningCell = std::nullopt;
    int crackStage = -1;
    float breakCooldown = 0.0f;

    float loadingProgress = 0.0f;
    int loadingLoadedChunks = 0;
    int loadingTotalChunks = 81;

    while (!window.shouldClose()) {
        Input::update();
        window.pollEvents();
        float dt = timer.tick();
        AudioEngine::get().update(dt);
        fpsTimer += dt;
        fpsFrameCount++;
        
        if (fpsTimer >= 1.0f) {
            displayedFPS = static_cast<float>(fpsFrameCount) / fpsTimer;
            fpsFrameCount = 0;
            fpsTimer = 0.0f;
            std::string title = "PrismCraft - FPS: " + std::to_string(static_cast<int>(displayedFPS));
            std::cout << title << std::endl;
        }

        if (chatFeedbackTimer > 0.0f) {
            chatFeedbackTimer -= dt;
            if (chatFeedbackTimer <= 0.0f) {
                chatFeedbackTimer = 0.0f;
                chatFeedback.clear();
            }
        }
        
        // Handle iconified / minimized state: pause render loop without calling Vulkan APIs
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window.getHandle(), &fbW, &fbH);
        if (window.isIconified() || fbW <= 0 || fbH <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        uint32_t screenW = static_cast<uint32_t>(fbW);
        uint32_t screenH = static_cast<uint32_t>(fbH);

        if (window.wasResized() || screenW != swapchain.getExtent().width || screenH != swapchain.getExtent().height) {
            window.resetResizedFlag();
            swapchain.recreate(screenW, screenH);
            tsrRenderer.recreate(screenW, screenH);
            auto [targetRenderW, targetRenderH] = computeInternalResolution(options, screenW, screenH);
            if (targetRenderW != postProcessRenderer.getWidth() || targetRenderH != postProcessRenderer.getHeight()) {
                postProcessRenderer.recreate(targetRenderW, targetRenderH);
                updateWaterDescriptorSets();
            }
        }
        
        glm::vec2 mousePos = Input::getMousePosition();
        uint32_t uiW = static_cast<uint32_t>(static_cast<float>(screenW) / options.uiScale);
        uint32_t uiH = static_cast<uint32_t>(static_cast<float>(screenH) / options.uiScale);
        glm::vec2 uiMousePos = mousePos / options.uiScale;

        // Advance Day / Night Cycle (1200s / 20 min full cycle, exact Minecraft parity)
        if (state == GameState::Playing && !isTimePaused) {
            timeOfDay += dt / 1200.0f;
            while (timeOfDay >= 1.0f) {
                timeOfDay -= 1.0f;
                gameDay++;
            }
            while (timeOfDay < 0.0f) {
                timeOfDay += 1.0f;
                if (gameDay > 0) gameDay--;
            }
        }

        // Calibrated celestial angle: 0.0=Sunrise (east), 0.25=Noon (zenith), 0.50=Sunset (west), 0.75=Midnight (nadir)
        float celestialAngle = (timeOfDay - 0.25f) * glm::two_pi<float>();
        glm::vec3 sunDir(std::sin(celestialAngle), std::cos(celestialAngle), 0.35f);
        sunDir = glm::normalize(sunDir);

        float sunHeight = sunDir.y;
        float sunIntensity = std::clamp((sunHeight + 0.15f) / 0.65f, 0.0f, 1.0f);

        // Dynamic atmospheric horizon sky color (calibrated to the linear horizon in sky.frag)
        glm::vec3 daySky(0.80f, 0.85f, 0.92f);      // Slightly warmer luminous atmospheric horizon sky
        glm::vec3 duskSky(1.00f, 0.76f, 0.44f);     // Intense golden-amber horizon
        glm::vec3 nightSky(0.13f, 0.15f, 0.22f);    // Soft atmospheric indigo night horizon

        glm::vec3 skyColor;
        if (sunHeight > 0.1f) {
            skyColor = glm::mix(duskSky, daySky, std::clamp((sunHeight - 0.1f) / 0.3f, 0.0f, 1.0f));
        } else if (sunHeight > -0.2f) {
            skyColor = glm::mix(nightSky, duskSky, std::clamp((sunHeight + 0.2f) / 0.3f, 0.0f, 1.0f));
        } else {
            skyColor = nightSky;
        }

        glm::vec3 checkCamPos = player.getCamera().getPosition();
        CellCoord camCell = worldToCell(checkCamPos);
        Cell camBlock = world->getCell(camCell.x, camCell.y, camCell.z, camCell.s);
        bool isUnderwater = (camBlock.type == BlockType::Water);
        if (isUnderwater) {
            skyColor = glm::vec3(0.012f, 0.065f, 0.110f); // Deep oceanic slate-blue underwater fog
        }
        float maxVisibleDist = static_cast<float>(std::max(world->renderDistance, world->lodDistance) * 16);
        float fogDistance = isUnderwater ? 16.0f : (maxVisibleDist * 0.92f);

        // -------------------------------------------------------------
        // State-Specific Logic & Input Handling
        // -------------------------------------------------------------
        std::optional<RaycastHit> targetHit = std::nullopt;

        if (state == GameState::Playing) {
            if (isChatOpen) {
                for (char c : Input::getTypedChars()) {
                    if (chatInput.size() < 60) chatInput += c;
                }
                if (Input::isKeyPressed(GLFW_KEY_BACKSPACE) && !chatInput.empty()) {
                    chatInput.pop_back();
                }
                if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                    isChatOpen = false;
                    chatInput.clear();
                }
                if (Input::isKeyPressed(GLFW_KEY_ENTER)) {
                    std::string cmd = chatInput;
                    while (!cmd.empty() && cmd.front() == ' ') cmd.erase(cmd.begin());
                    if (!cmd.empty()) {
                        if (cmd.rfind("/time", 0) == 0) {
                            std::string args = (cmd.size() > 5) ? cmd.substr(5) : "";
                            while (!args.empty() && args.front() == ' ') args.erase(0, 1);

                            if (args.rfind("set ", 0) == 0) {
                                std::string param = args.substr(4);
                                while (!param.empty() && param.front() == ' ') param.erase(0, 1);

                                if (param == "day") {
                                    timeOfDay = 1000.0f / 24000.0f;
                                    chatFeedback = "Set the time to 1000 (Day)";
                                } else if (param == "noon") {
                                    timeOfDay = 6000.0f / 24000.0f;
                                    chatFeedback = "Set the time to 6000 (Noon)";
                                } else if (param == "sunset" || param == "dusk") {
                                    timeOfDay = 12000.0f / 24000.0f;
                                    chatFeedback = "Set the time to 12000 (Sunset)";
                                } else if (param == "night") {
                                    timeOfDay = 13000.0f / 24000.0f;
                                    chatFeedback = "Set the time to 13000 (Night)";
                                } else if (param == "midnight") {
                                    timeOfDay = 18000.0f / 24000.0f;
                                    chatFeedback = "Set the time to 18000 (Midnight)";
                                } else if (param == "sunrise" || param == "dawn") {
                                    timeOfDay = 0.0f;
                                    chatFeedback = "Set the time to 0 (Sunrise)";
                                } else {
                                    try {
                                        int ticks = std::stoi(param);
                                        if (ticks < 0) ticks = 0;
                                        timeOfDay = static_cast<float>(ticks % 24000) / 24000.0f;
                                        chatFeedback = "Set the time to " + std::to_string(ticks % 24000);
                                    } catch (...) {
                                        chatFeedback = "Unknown time value: " + param;
                                    }
                                }
                                chatFeedbackTimer = 3.0f;
                            } else if (args.rfind("add ", 0) == 0) {
                                std::string param = args.substr(4);
                                while (!param.empty() && param.front() == ' ') param.erase(0, 1);
                                try {
                                    int ticksToAdd = std::stoi(param);
                                    timeOfDay += static_cast<float>(ticksToAdd) / 24000.0f;
                                    while (timeOfDay >= 1.0f) { timeOfDay -= 1.0f; gameDay++; }
                                    while (timeOfDay < 0.0f) { timeOfDay += 1.0f; if (gameDay > 0) gameDay--; }
                                    int currentTicks = static_cast<int>(timeOfDay * 24000.0f) % 24000;
                                    chatFeedback = "Added " + std::to_string(ticksToAdd) + " to the time (Current: " + std::to_string(currentTicks) + ")";
                                } catch (...) {
                                    chatFeedback = "Invalid tick amount: " + param;
                                }
                                chatFeedbackTimer = 3.0f;
                            } else if (args.rfind("query ", 0) == 0) {
                                std::string param = args.substr(6);
                                while (!param.empty() && param.front() == ' ') param.erase(0, 1);
                                if (param == "daytime") {
                                    int currentTicks = static_cast<int>(timeOfDay * 24000.0f) % 24000;
                                    chatFeedback = "The time is " + std::to_string(currentTicks);
                                } else if (param == "day") {
                                    chatFeedback = "The time is " + std::to_string(gameDay) + " days";
                                } else if (param == "gametime" || param == "time") {
                                    uint64_t totalTicks = static_cast<uint64_t>(gameDay) * 24000ULL + static_cast<uint64_t>(timeOfDay * 24000.0f);
                                    chatFeedback = "The time is " + std::to_string(totalTicks);
                                } else {
                                    chatFeedback = "Usage: /time query <daytime|gametime|day>";
                                }
                                chatFeedbackTimer = 3.0f;
                            } else if (args == "pause" || args == "stop") {
                                isTimePaused = true;
                                chatFeedback = "Day-night cycle paused";
                                chatFeedbackTimer = 3.0f;
                            } else if (args == "resume" || args == "start" || args == "unpause") {
                                isTimePaused = false;
                                chatFeedback = "Day-night cycle resumed";
                                chatFeedbackTimer = 3.0f;
                            } else {
                                chatFeedback = "Usage: /time <set|add|query|pause|resume>";
                                chatFeedbackTimer = 3.0f;
                            }
                        } else if (cmd == "/gamemode creative" || cmd == "/gamemode c" || cmd == "/gamemode 1") {
                            player.setCreative(true);
                            player.setFlying(true);
                            chatFeedback = "Set game mode to Creative Mode";
                        } else if (cmd == "/gamemode survival" || cmd == "/gamemode s" || cmd == "/gamemode 0") {
                            player.setCreative(false);
                            player.setFlying(false);
                            if (player.getCamera().getMode() == CameraMode::FreeCam) player.getCamera().toggleFreeCam();
                            chatFeedback = "Set game mode to Survival Mode";
                        } else if (cmd == "/gamemode spectator" || cmd == "/gamemode sp" || cmd == "/gamemode 3") {
                            if (player.getCamera().getMode() != CameraMode::FreeCam) player.getCamera().toggleFreeCam();
                            chatFeedback = "Set game mode to Spectator Mode";
                        } else if (cmd.rfind("/locate", 0) == 0) {
                            std::string target = (cmd.size() > 7) ? cmd.substr(7) : "";
                            while (!target.empty() && target.front() == ' ') target.erase(0, 1);

                            if (target.rfind("structure ", 0) == 0) {
                                target = target.substr(10);
                            }
                            while (!target.empty() && target.front() == ' ') target.erase(0, 1);

                            if (target.rfind("biome ", 0) == 0) {
                                std::string bName = target.substr(6);
                                while (!bName.empty() && bName.front() == ' ') bName.erase(0, 1);
                                BiomeType targetBiome = BiomeType::Plains;
                                bool validBiome = true;
                                if (bName == "mountains" || bName == "mountain" || bName == "peaks") targetBiome = BiomeType::Mountains;
                                else if (bName == "desert") targetBiome = BiomeType::Desert;
                                else if (bName == "savanna") targetBiome = BiomeType::Savanna;
                                else if (bName == "taiga") targetBiome = BiomeType::Taiga;
                                else if (bName == "forest") targetBiome = BiomeType::Forest;
                                else if (bName == "birch_forest" || bName == "birch") targetBiome = BiomeType::BirchForest;
                                else if (bName == "swamp") targetBiome = BiomeType::Swamp;
                                else if (bName == "ocean") targetBiome = BiomeType::Ocean;
                                else if (bName == "plains") targetBiome = BiomeType::Plains;
                                else validBiome = false;

                                if (validBiome && world) {
                                    int bx = 0, bz = 0;
                                    int px = static_cast<int>(std::floor(player.getPosition().x));
                                    int pz = static_cast<int>(std::floor(player.getPosition().z));
                                    if (world->getTerrainGen().locateBiome(targetBiome, px, pz, bx, bz)) {
                                        int dist = static_cast<int>(std::round(std::sqrt((bx - px) * (bx - px) + (bz - pz) * (bz - pz))));
                                        int by = world->getTerrainGen().getHeight(bx, bz);
                                        chatFeedback = "Nearest " + std::string(getBiomeName(targetBiome)) + " is at [" +
                                                       std::to_string(bx) + ", " + std::to_string(by) + ", " + std::to_string(bz) +
                                                       "] (" + std::to_string(dist) + " blocks away)";
                                    } else {
                                        chatFeedback = "Could not find biome '" + bName + "' within search distance";
                                    }
                                } else {
                                    chatFeedback = "Unknown biome: " + bName + " (Options: plains, desert, mountains, taiga, forest, birch, savanna, swamp, ocean)";
                                }
                            } else if (!target.empty() && world) {
                                int sx = 0, sy = 0, sz = 0;
                                int px = static_cast<int>(std::floor(player.getPosition().x));
                                int pz = static_cast<int>(std::floor(player.getPosition().z));
                                if (world->getTerrainGen().locateStructure(target, px, pz, sx, sy, sz)) {
                                    int dist = static_cast<int>(std::round(std::sqrt((sx - px) * (sx - px) + (sz - pz) * (sz - pz))));
                                    chatFeedback = "Nearest " + target + " is at [" +
                                                   std::to_string(sx) + ", " + std::to_string(sy) + ", " + std::to_string(sz) +
                                                   "] (" + std::to_string(dist) + " blocks away)";
                                } else {
                                    chatFeedback = "Could not find structure '" + target + "' within 1280 blocks";
                                }
                            } else {
                                chatFeedback = "Usage: /locate <village|temple|outpost|dungeon|biome <name>>";
                            }
                        } else if (cmd == "/save") {
                            if (world) {
                                world->saveAll(player, timeOfDay);
                                chatFeedback = "Saved the game";
                            }
                        } else if (cmd == "/clear") {
                            player.clearInventory();
                            chatFeedback = "Cleared the inventory";
                        } else if (cmd == "/help") {
                            chatFeedback = "Commands: /locate <village|temple|outpost|biome>, /time set <day|night>, /gamemode <c|s>, /save, /clear";
                        } else {
                            chatFeedback = "Unknown command: " + cmd;
                        }
                        chatFeedbackTimer = 5.0f;
                        AudioEngine::get().playSound(SoundEffect::Click);
                    }
                    isChatOpen = false;
                    chatInput.clear();
                }
            } else if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                if (isSleeping) {
                    isSleeping = false;
                    sleepTimer = 0.0f;
                    sleepFadeAlpha = 0.0f;
                    player.setPosition(preSleepPlayerPos);
                    player.getCamera().setPitch(0.0f);
                }
                state = GameState::Paused;
                window.setCursorMode(false);
                AudioEngine::get().playSound(SoundEffect::Click);
            } else if (Input::isKeyPressed(GLFW_KEY_E)) {
                state = GameState::Inventory;
                window.setCursorMode(false);
                AudioEngine::get().playSound(SoundEffect::Click);
            } else if (Input::isKeyPressed(GLFW_KEY_SLASH)) {
                isChatOpen = true;
                chatInput = "/";
            } else if (Input::isKeyPressed(GLFW_KEY_T)) {
                isChatOpen = true;
                chatInput = "";
            } else {
                if (isSleeping) {
                    // Shift, Space, or Escape cancels / leaves bed early
                    if (Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::isKeyPressed(GLFW_KEY_RIGHT_SHIFT) ||
                        Input::isKeyPressed(GLFW_KEY_SPACE) || Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                        isSleeping = false;
                        sleepTimer = 0.0f;
                        sleepFadeAlpha = 0.0f;
                        player.setPosition(preSleepPlayerPos);
                        player.getCamera().setPitch(preSleepPitch);
                        player.getCamera().setYaw(preSleepYaw);
                        chatFeedback = "Left the bed";
                        chatFeedbackTimer = 2.0f;
                        AudioEngine::get().playSound(SoundEffect::WoodStep, 0.8f);
                    } else {
                        sleepTimer += dt;

                        // Minecraft-style sleeping animation sequence:
                        // 0.0s - 0.8s: Smoothly ease camera onto bed and pitch upward, fade to black
                        // 0.8s - 1.6s: Full black, accelerate time to dawn, restore health and hunger
                        // 1.6s - 2.5s: Fade in morning sunlight, tilt camera back
                        // 2.5s+: Wake up standing beside bed

                        glm::vec3 bedTargetCam = sleepingBedPos + glm::vec3(0.0f, 0.55f, 0.0f);
                        glm::vec3 headStartCam = preSleepPlayerPos + glm::vec3(0.0f, player.getEyeHeight(), 0.0f);

                        if (sleepTimer < 0.8f) {
                            float u = sleepTimer / 0.8f;
                            u = u * u * (3.0f - 2.0f * u); // Smoothstep
                            glm::vec3 curCamPos = glm::mix(headStartCam, bedTargetCam, u);
                            float curPitch = glm::mix(preSleepPitch, 0.80f, u); // Look up while lying down
                            player.getCamera().setPosition(curCamPos);
                            player.getCamera().setPitch(curPitch);
                            sleepFadeAlpha = std::clamp((sleepTimer - 0.15f) / 0.65f, 0.0f, 1.0f);
                        } else if (sleepTimer < 1.6f) {
                            player.getCamera().setPosition(bedTargetCam);
                            player.getCamera().setPitch(0.80f);
                            sleepFadeAlpha = 1.0f;

                            // Transition world to morning dawn at midpoint
                            int curTicks = static_cast<int>(timeOfDay * 24000.0f) % 24000;
                            if (sleepTimer >= 1.2f && (curTicks >= 12000 || curTicks <= 1000)) {
                                timeOfDay = 1000.0f / 24000.0f; // Morning dawn (1000 ticks)
                                if (curTicks >= 12000) gameDay++;
                                player.setHealth(20.0f);
                                player.setHunger(20.0f);
                            }
                        } else if (sleepTimer < 2.5f) {
                            float u = (sleepTimer - 1.6f) / 0.9f;
                            u = u * u * (3.0f - 2.0f * u);
                            glm::vec3 curCamPos = glm::mix(bedTargetCam, headStartCam, u);
                            float curPitch = glm::mix(0.80f, 0.0f, u);
                            player.getCamera().setPosition(curCamPos);
                            player.getCamera().setPitch(curPitch);
                            sleepFadeAlpha = 1.0f - u;
                        } else {
                            // Awakening complete
                            isSleeping = false;
                            sleepTimer = 0.0f;
                            sleepFadeAlpha = 0.0f;
                            player.setPosition(preSleepPlayerPos);
                            player.getCamera().setPosition(headStartCam);
                            player.getCamera().setPitch(0.0f);
                            chatFeedback = "Good morning!";
                            chatFeedbackTimer = 3.0f;
                            AudioEngine::get().playSound(SoundEffect::Click, 1.0f);
                        }

                        // Keep world chunks and entities updating
                        world->update(sleepingBedPos, dt);
                        itemDropManager.update(dt, *world, player);
                        fallingBlockManager.update(dt, *world);
                    }
                } else {
                    // F6 to toggle Spectator Free Cam
                if (Input::isKeyPressed(GLFW_KEY_F6)) {
                    player.getCamera().toggleFreeCam();
                    if (player.getCamera().getMode() != CameraMode::FreeCam) {
                        player.getCamera().setPosition(player.getPosition() + glm::vec3(0.0f, player.getEyeHeight(), 0.0f));
                    }
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
                // F3 to toggle Debug HUD
                if (Input::isKeyPressed(GLFW_KEY_F3)) {
                    options.debugHUD = !options.debugHUD;
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
                // F7 to toggle Light Level Overlay
                if (Input::isKeyPressed(GLFW_KEY_F7)) {
                    options.lightOverlay = !options.lightOverlay;
                    AudioEngine::get().playSound(SoundEffect::Click);
                }

                // F5 to cycle Camera Mode (First Person -> Third Person Back -> Third Person Front)
                if (Input::isKeyPressed(GLFW_KEY_F5)) {
                    player.getCamera().cycleMode();
                }

                // Number keys 1-9 and 0 to select hotbar slots (10 slots)
                for (int k = GLFW_KEY_1; k <= GLFW_KEY_9; ++k) {
                    if (Input::isKeyPressed(k)) {
                        player.setSelectedSlot(k - GLFW_KEY_1);
                    }
                }
                if (Input::isKeyPressed(GLFW_KEY_0)) {
                    player.setSelectedSlot(9);
                }

                // Mouse scroll wheel for hotbar selection (10 slots)
                float scroll = Input::getScrollDelta();
                if (scroll != 0.0f) {
                    int cur = player.getSelectedSlot();
                    if (scroll > 0) cur = (cur + 9) % 10;
                    else cur = (cur + 1) % 10;
                    player.setSelectedSlot(cur);
                }

                // Q key to drop 1 item from currently selected hotbar slot
                if (Input::isKeyPressed(GLFW_KEY_Q)) {
                    BlockType sel = player.getSelectedBlock();
                    if (sel != BlockType::Air) {
                        glm::vec3 eye = player.getPosition() + glm::vec3(0.0f, 1.62f, 0.0f);
                        glm::vec3 fwd = player.getCamera().getForward();
                        glm::vec3 dropPos = eye + fwd * 0.8f;
                        glm::vec3 dropVel = fwd * 4.5f + glm::vec3(0.0f, 1.5f, 0.0f);
                        itemDropManager.spawnDrop(dropPos, sel, dropVel);
                        player.consumeSelectedItem();
                        AudioEngine::get().playSound(SoundEffect::ItemPop);
                    }
                }

                // OptiFine 5x Dynamic Zoom (Hold 'C')
                bool isZooming = (!isChatOpen && Input::isKeyDown(GLFW_KEY_C));
                float targetFov = isZooming ? (static_cast<float>(options.fov) / 5.0f) : static_cast<float>(options.fov);
                player.getCamera().fov += (targetFov - player.getCamera().fov) * std::min(1.0f, dt * 22.0f);
                float zoomRatio = player.getCamera().fov / static_cast<float>(options.fov);
                player.mouseSensitivity = options.mouseSens * zoomRatio;

                player.handleInput(dt);
                player.update(dt, *world);
                world->update(player.getPosition(), dt);
                itemDropManager.update(dt, *world, player);
                fallingBlockManager.update(dt, *world);
                arrowManager.update(dt, *world, player);

                // Void damage check & Death Screen trigger
                if (player.getPosition().y < -10.0f) {
                    player.takeDamage(100.0f);
                }
                if (player.getHealth() <= 0.0f) {
                    state = GameState::Death;
                    window.setCursorMode(false);
                }

                // Raycast for targeted block
                targetHit = Raycast::cast(*world, player.getCamera().getPosition(), player.getCamera().getForward(), 6.0f);

                // Middle click to pick block
                if (Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE) && targetHit.has_value()) {
                    Cell picked = world->getCell(targetHit->hitCell.x, targetHit->hitCell.y, targetHit->hitCell.z, targetHit->hitCell.s);
                    if (picked.type != BlockType::Air) {
                        player.setHotbarBlock(player.getSelectedSlot(), picked.type, 64);
                        AudioEngine::get().playSound(SoundEffect::ItemPop);
                    }
                }

                if (breakCooldown > 0.0f) {
                    breakCooldown = std::max(0.0f, breakCooldown - dt);
                }

                // Left click ALWAYS triggers punch/hit animation (air or block)
                if (Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
                    player.triggerSwing();
                }

                // Mining / Breaking Blocks (instant in creative, suited tool speeds in survival)
                if (Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) && targetHit.has_value()) {
                    const CellCoord& hit = targetHit->hitCell;

                    if (!player.isCreative() && breakCooldown > 0.0f) {
                        miningTimer = 0.0f;
                        currentMiningCell = std::nullopt;
                        crackStage = -1;
                    } else {
                        if (!currentMiningCell.has_value() ||
                            currentMiningCell->x != hit.x || currentMiningCell->y != hit.y ||
                            currentMiningCell->z != hit.z || currentMiningCell->s != hit.s) {
                            currentMiningCell = hit;
                            miningTimer = 0.0f;
                        }

                        miningTimer += dt;
                        Cell targetCell = world->getCell(hit.x, hit.y, hit.z, hit.s);
                        float breakDuration = player.isCreative() ? 0.0f : calculateBreakDuration(targetCell.type, player.getSelectedBlock());
                        crackStage = (breakDuration > 0.0f) ? std::clamp(static_cast<int>((miningTimer / breakDuration) * 10.0f), 0, 9) : 0;

                        if (miningTimer >= breakDuration) {
                        Cell broken = world->getCell(hit.x, hit.y, hit.z, hit.s);
                        world->setCellInstant(hit.x, hit.y, hit.z, hit.s, Cell{BlockType::Air});
                        AudioEngine::get().playBlockDig(static_cast<int>(broken.type), 0.9f);

                        // Spawn miniature 3D drop entity (Survival only)
                        if (!player.isCreative()) {
                            glm::vec3 dropPos = cellToWorldCenter(hit.x, hit.y, hit.z, hit.s);
                            itemDropManager.spawnDrop(dropPos, broken.type);
                        }

                        // Trigger gravity physics for Sand and Gravel blocks above the broken block
                        world->checkGravity(hit.x, hit.y + 1, hit.z, hit.s, &fallingBlockManager);

                        // If broken block was a Door, break the partner half
                        if (broken.isDoor()) {
                            bool isUpper = broken.isDoorUpper();
                            int partnerY = hit.y + (isUpper ? -1 : 1);
                            Cell partner = world->getCell(hit.x, partnerY, hit.z, hit.s);
                            if (partner.isDoor()) {
                                world->setCellInstant(hit.x, partnerY, hit.z, hit.s, Cell{BlockType::Air});
                            }
                        }
                        // If broken block was a Bed, break all 4 cells of the bed
                        if (broken.isBed()) {
                            CellCoord bedCells[4];
                            getBedAllCells(hit.x, hit.y, hit.z, hit.s, broken.level, bedCells);
                            for (int i = 0; i < 4; ++i) {
                                if (world->getCell(bedCells[i].x, bedCells[i].y, bedCells[i].z, bedCells[i].s).isBed()) {
                                    world->setCellInstant(bedCells[i].x, bedCells[i].y, bedCells[i].z, bedCells[i].s, Cell{BlockType::Air});
                                }
                            }
                        }

                        // Pop off dependent blocks supported by this broken block
                        // 1. Blocks resting directly on top of this block
                        Cell aboveCell = world->getCell(hit.x, hit.y + 1, hit.z, hit.s);
                        if (aboveCell.isTorch() && aboveCell.level == 0) {
                            world->setCellInstant(hit.x, hit.y + 1, hit.z, hit.s, Cell{BlockType::Air});
                            itemDropManager.spawnDrop(cellToWorldCenter(hit.x, hit.y + 1, hit.z, hit.s), aboveCell.type);
                        } else if (aboveCell.isLantern() && aboveCell.level == 0) {
                            world->setCellInstant(hit.x, hit.y + 1, hit.z, hit.s, Cell{BlockType::Air});
                            itemDropManager.spawnDrop(cellToWorldCenter(hit.x, hit.y + 1, hit.z, hit.s), aboveCell.type);
                        } else if (aboveCell.isTrapdoor() && !aboveCell.isTrapdoorOpen()) {
                            world->setCellInstant(hit.x, hit.y + 1, hit.z, hit.s, Cell{BlockType::Air});
                            itemDropManager.spawnDrop(cellToWorldCenter(hit.x, hit.y + 1, hit.z, hit.s), aboveCell.type);
                        } else if (aboveCell.isDoor() && !aboveCell.isDoorUpper()) {
                            world->setCellInstant(hit.x, hit.y + 1, hit.z, hit.s, Cell{BlockType::Air});
                            Cell topHalf = world->getCell(hit.x, hit.y + 2, hit.z, hit.s);
                            if (topHalf.isDoor()) {
                                world->setCellInstant(hit.x, hit.y + 2, hit.z, hit.s, Cell{BlockType::Air});
                            }
                            itemDropManager.spawnDrop(cellToWorldCenter(hit.x, hit.y + 1, hit.z, hit.s), aboveCell.type);
                        } else if (aboveCell.isBed()) {
                            CellCoord bedCells[4];
                            getBedAllCells(hit.x, hit.y + 1, hit.z, hit.s, aboveCell.level, bedCells);
                            for (int i = 0; i < 4; ++i) {
                                if (world->getCell(bedCells[i].x, bedCells[i].y, bedCells[i].z, bedCells[i].s).isBed()) {
                                    world->setCellInstant(bedCells[i].x, bedCells[i].y, bedCells[i].z, bedCells[i].s, Cell{BlockType::Air});
                                }
                            }
                            itemDropManager.spawnDrop(cellToWorldCenter(hit.x, hit.y + 1, hit.z, hit.s), BlockType::Bed);
                        }
                        // 2. Wall torches mounted on lateral faces of this block
                        CellCoord bNeighbors[5];
                        getNeighbors(hit.x, hit.y, hit.z, hit.s, bNeighbors);
                        // Neighbor 2: Base wall torch (mount level 1)
                        Cell n2 = world->getCell(bNeighbors[2].x, bNeighbors[2].y, bNeighbors[2].z, bNeighbors[2].s);
                        if (n2.isTorch() && n2.level == 1) {
                            world->setCellInstant(bNeighbors[2].x, bNeighbors[2].y, bNeighbors[2].z, bNeighbors[2].s, Cell{BlockType::Air});
                            itemDropManager.spawnDrop(cellToWorldCenter(bNeighbors[2].x, bNeighbors[2].y, bNeighbors[2].z, bNeighbors[2].s), n2.type);
                        }
                        // Neighbor 3: Left slanted wall torch (mount level 2)
                        Cell n3 = world->getCell(bNeighbors[3].x, bNeighbors[3].y, bNeighbors[3].z, bNeighbors[3].s);
                        if (n3.isTorch() && n3.level == 2) {
                            world->setCellInstant(bNeighbors[3].x, bNeighbors[3].y, bNeighbors[3].z, bNeighbors[3].s, Cell{BlockType::Air});
                            itemDropManager.spawnDrop(cellToWorldCenter(bNeighbors[3].x, bNeighbors[3].y, bNeighbors[3].z, bNeighbors[3].s), n3.type);
                        }
                        // Neighbor 4: Right slanted wall torch (mount level 3)
                        Cell n4 = world->getCell(bNeighbors[4].x, bNeighbors[4].y, bNeighbors[4].z, bNeighbors[4].s);
                        if (n4.isTorch() && n4.level == 3) {
                            world->setCellInstant(bNeighbors[4].x, bNeighbors[4].y, bNeighbors[4].z, bNeighbors[4].s, Cell{BlockType::Air});
                            itemDropManager.spawnDrop(cellToWorldCenter(bNeighbors[4].x, bNeighbors[4].y, bNeighbors[4].z, bNeighbors[4].s), n4.type);
                        }

                        miningTimer = 0.0f;
                        currentMiningCell = std::nullopt;
                        crackStage = -1;
                        if (!player.isCreative()) {
                            breakCooldown = 0.25f; // Minecraft 5-tick (0.25s) hit delay
                        }
                    }
                    }
                } else {
                    miningTimer = 0.0f;
                    currentMiningCell = std::nullopt;
                    crackStage = -1;
                }

                // Bow charging & shooting mechanics
                BlockType heldItem = player.getSelectedBlock();
                if (heldItem == BlockType::ItemBow) {
                    if (Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT) && (player.hasItem(BlockType::ItemArrow) || player.isDrawingBow())) {
                        player.setDrawingBow(true);
                        float newCharge = std::min(1.0f, player.getBowCharge() + dt * 1.5f);
                        player.setBowCharge(newCharge);
                    } else if (player.isDrawingBow()) {
                        if (player.getBowCharge() >= 0.15f && player.hasItem(BlockType::ItemArrow)) {
                            player.consumeItem(BlockType::ItemArrow, 1);
                            float charge = player.getBowCharge();
                            float speed = 15.0f + 25.0f * charge;
                            glm::vec3 spawnPos = player.getCamera().getPosition() + player.getCamera().getForward() * 0.4f;
                            arrowManager.spawnArrow(spawnPos, player.getCamera().getForward() * speed);
                            AudioEngine::get().playSound(SoundEffect::BowShoot);
                            player.triggerPlace();
                        }
                        player.setDrawingBow(false);
                        player.setBowCharge(0.0f);
                    }
                } else {
                    if (player.isDrawingBow()) {
                        player.setDrawingBow(false);
                        player.setBowCharge(0.0f);
                    }
                }

                // Sword blocking stance (holding RMB while holding any sword)
                if (heldItem != BlockType::ItemBow && Cell{heldItem}.isSword()) {
                    bool canBlock = Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);
                    if (canBlock && targetHit.has_value()) {
                        Cell targetC = world->getCell(targetHit->hitCell.x, targetHit->hitCell.y, targetHit->hitCell.z, targetHit->hitCell.s);
                        if (targetC.type == BlockType::CraftingTable || targetC.isDoor() || targetC.isBed()) canBlock = false;
                    }
                    player.setBlocking(canBlock);
                } else {
                    player.setBlocking(false);
                }

                // Right Click Handling (Crafting Table, Door, Bed interaction, or Block Placement)
                if (heldItem != BlockType::ItemBow && !player.isBlocking() && Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) && targetHit.has_value()) {
                    CellCoord hitCell = targetHit->hitCell;
                    Cell hitBlock = world->getCell(hitCell.x, hitCell.y, hitCell.z, hitCell.s);

                    // 1. If right-clicked a Crafting Table block, open 3x3 Crafting Table GUI!
                    if (hitBlock.type == BlockType::CraftingTable) {
                        state = GameState::CraftingTable;
                        window.setCursorMode(false);
                        AudioEngine::get().playSound(SoundEffect::Click);
                    } else if (hitBlock.isDoor()) {
                        // Toggle door open/closed state on both vertical halves
                        bool isUpper = hitBlock.isDoorUpper();
                        int partnerY = hitCell.y + (isUpper ? -1 : 1);
                        Cell partner = world->getCell(hitCell.x, partnerY, hitCell.z, hitCell.s);

                        uint8_t newLevel = hitBlock.level ^ 8; // toggle open bit
                        world->setCellInstant(hitCell.x, hitCell.y, hitCell.z, hitCell.s, Cell{hitBlock.type, newLevel});

                        if (partner.isDoor()) {
                            uint8_t partnerNewLevel = partner.level ^ 8;
                            world->setCellInstant(hitCell.x, partnerY, hitCell.z, hitCell.s, Cell{partner.type, partnerNewLevel});
                        }
                        AudioEngine::get().playSound(SoundEffect::WoodStep, 1.0f);
                        player.triggerPlace();
                    } else if (hitBlock.isTrapdoor()) {
                        // Toggle trapdoor open/closed state
                        uint8_t newLevel = hitBlock.level ^ 8;
                        world->setCellInstant(hitCell.x, hitCell.y, hitCell.z, hitCell.s, Cell{hitBlock.type, newLevel});
                        AudioEngine::get().playSound(SoundEffect::WoodStep, 1.0f);
                        player.triggerPlace();
                    } else if (hitBlock.isBed()) {
                        // Set spawn point to this bed
                        glm::vec3 spawnPos = cellToWorldCenter(hitCell.x, hitCell.y, hitCell.z, hitCell.s) + glm::vec3(0.0f, 0.6f, 0.0f);
                        player.setSpawnPoint(spawnPos);

                        // Check night time (12500 to 23500 ticks)
                        int curTicks = static_cast<int>(timeOfDay * 24000.0f) % 24000;
                        if (curTicks >= 12500 && curTicks <= 23500) {
                            isSleeping = true;
                            sleepTimer = 0.0f;
                            sleepFadeAlpha = 0.0f;
                            sleepingBedPos = cellToWorldCenter(hitCell.x, hitCell.y, hitCell.z, hitCell.s);
                            preSleepPlayerPos = player.getPosition();
                            preSleepPitch = player.getCamera().getPitch();
                            preSleepYaw = player.getCamera().getYaw();
                            chatFeedback = "Spawn point set. Sleeping...";
                            chatFeedbackTimer = 2.0f;
                            AudioEngine::get().playSound(SoundEffect::WoodStep, 0.9f);
                        } else {
                            chatFeedback = "Spawn point set. You can only sleep at night.";
                            chatFeedbackTimer = 3.0f;
                            AudioEngine::get().playSound(SoundEffect::Click, 0.8f);
                        }
                        player.triggerPlace();
                    } else {
                        CellCoord place = targetHit->placeCell;
                        BlockType b = player.getSelectedBlock();
                        if (b != BlockType::Air && !Cell{b}.isItem()) {
                            bool canPlace = true;
                            uint8_t placeLevel = 0;

                            if (Cell{b}.isDoor()) {
                                Cell below = world->getCell(place.x, place.y - 1, place.z, place.s);
                                if (!below.isSolid() || place.y + 1 >= CHUNK_SIZE_Y) {
                                    canPlace = false;
                                } else {
                                    Cell above = world->getCell(place.x, place.y + 1, place.z, place.s);
                                    if (above.type != BlockType::Air) {
                                        canPlace = false;
                                    }
                                }

                                if (canPlace) {
                                    glm::vec3 fwd = player.getCamera().getForward();
                                    glm::vec2 fwd2(fwd.x, fwd.z);
                                    if (glm::length(fwd2) > 0.001f) fwd2 = glm::normalize(fwd2);

                                    glm::vec2 wallInNormals[3];
                                    if (place.s == 0) {
                                        wallInNormals[0] = glm::vec2(0.0f, 1.0f);            // Base wall
                                        wallInNormals[1] = glm::vec2(SQRT_3_OVER_2, -0.5f);  // Left wall
                                        wallInNormals[2] = glm::vec2(-SQRT_3_OVER_2, -0.5f); // Right wall
                                    } else {
                                        wallInNormals[0] = glm::vec2(0.0f, -1.0f);           // Base wall
                                        wallInNormals[1] = glm::vec2(SQRT_3_OVER_2, 0.5f);   // Left wall
                                        wallInNormals[2] = glm::vec2(-SQRT_3_OVER_2, 0.5f);  // Right wall
                                    }

                                    float maxDot = -999.0f;
                                    int bestFacing = 0;
                                    for (int i = 0; i < 3; ++i) {
                                        float d = glm::dot(fwd2, wallInNormals[i]);
                                        if (d > maxDot) {
                                            maxDot = d;
                                            bestFacing = i;
                                        }
                                    }

                                    uint8_t lowerLevel = static_cast<uint8_t>(bestFacing & 3);
                                    uint8_t upperLevel = static_cast<uint8_t>((bestFacing & 3) | 4);

                                    world->setCellInstant(place.x, place.y, place.z, place.s, Cell{b, lowerLevel});
                                    world->setCellInstant(place.x, place.y + 1, place.z, place.s, Cell{b, upperLevel});
                                    player.consumeSelectedItem();
                                    player.triggerPlace();
                                    AudioEngine::get().playBlockStep(static_cast<int>(b), 0.9f);
                                    continue;
                                }
                            } else if (b == BlockType::Bed) {
                                int xF = place.x, zF = place.z, yF = place.y;
                                glm::vec3 fwd = player.getCamera().getForward();
                                float ax = std::abs(fwd.x);
                                float az = std::abs(fwd.z);
                                uint8_t facing = 0;
                                int rowParity = floorMod(zF, 2);
                                int xH = xF, zH = zF;

                                if (ax >= az) {
                                    if (fwd.x >= 0.0f) { facing = 0; xH = xF + 1; zH = zF; }
                                    else               { facing = 1; xH = xF - 1; zH = zF; }
                                } else {
                                    if (fwd.z >= 0.0f) { facing = 2; xH = (rowParity == 0 ? xF : xF + 1); zH = zF + 1; }
                                    else               { facing = 3; xH = (rowParity == 0 ? xF - 1 : xF); zH = zF - 1; }
                                }

                                // Check all 4 cells are Air and support below is solid
                                Cell f0 = world->getCell(xF, yF, zF, 0);
                                Cell f1 = world->getCell(xF, yF, zF, 1);
                                Cell h0 = world->getCell(xH, yF, zH, 0);
                                Cell h1 = world->getCell(xH, yF, zH, 1);

                                Cell f0_below = world->getCell(xF, yF - 1, zF, 0);
                                Cell f1_below = world->getCell(xF, yF - 1, zF, 1);
                                Cell h0_below = world->getCell(xH, yF - 1, zH, 0);
                                Cell h1_below = world->getCell(xH, yF - 1, zH, 1);

                                if (f0.type != BlockType::Air || f1.type != BlockType::Air ||
                                    h0.type != BlockType::Air || h1.type != BlockType::Air ||
                                    !f0_below.isSolid() || !f1_below.isSolid() ||
                                    !h0_below.isSolid() || !h1_below.isSolid()) {
                                    canPlace = false;
                                } else {
                                    uint8_t footLevel0 = static_cast<uint8_t>((facing << 2) | (0 << 1) | 0);
                                    uint8_t footLevel1 = static_cast<uint8_t>((facing << 2) | (1 << 1) | 0);
                                    uint8_t headLevel0 = static_cast<uint8_t>((facing << 2) | (0 << 1) | 1);
                                    uint8_t headLevel1 = static_cast<uint8_t>((facing << 2) | (1 << 1) | 1);

                                    world->setCellInstant(xF, yF, zF, 0, Cell{b, footLevel0});
                                    world->setCellInstant(xF, yF, zF, 1, Cell{b, footLevel1});
                                    world->setCellInstant(xH, yF, zH, 0, Cell{b, headLevel0});
                                    world->setCellInstant(xH, yF, zH, 1, Cell{b, headLevel1});

                                    player.consumeSelectedItem();
                                    player.triggerPlace();
                                    AudioEngine::get().playBlockStep(static_cast<int>(b), 0.9f);
                                    continue;
                                }
                            } else if (Cell{b}.isTorch()) {
                                CellCoord placeNeighbors[5];
                                getNeighbors(place.x, place.y, place.z, place.s, placeNeighbors);
                                Cell hitBlockCell = world->getCell(hitCell.x, hitCell.y, hitCell.z, hitCell.s);

                                if (!hitBlockCell.isSolid()) {
                                    canPlace = false;
                                } else if (placeNeighbors[1] == hitCell) {
                                    // Placed on top face of hitCell (Floor torch)
                                    placeLevel = 0;
                                } else if (placeNeighbors[2] == hitCell) {
                                    // Attached to Base wall
                                    placeLevel = 1;
                                } else if (placeNeighbors[3] == hitCell) {
                                    // Attached to Left slanted wall
                                    placeLevel = 2;
                                } else if (placeNeighbors[4] == hitCell) {
                                    // Attached to Right slanted wall
                                    placeLevel = 3;
                                } else {
                                    // Ceiling or non-adjacent face
                                    canPlace = false;
                                }
                            } else if (Cell{b}.isTrapdoor()) {
                                // Determine facing wall (0 = Base, 1 = Left, 2 = Right)
                                CellCoord placeNeighbors[5];
                                getNeighbors(place.x, place.y, place.z, place.s, placeNeighbors);
                                uint8_t facing = 0;
                                if (placeNeighbors[2] == hitCell) facing = 0;
                                else if (placeNeighbors[3] == hitCell) facing = 1;
                                else if (placeNeighbors[4] == hitCell) facing = 2;
                                else {
                                    glm::vec3 fwd = player.getCamera().getForward();
                                    glm::vec2 vXZ[3];
                                    getPrismVerticesXZ(place.x, place.z, place.s, vXZ);
                                    glm::vec2 cent = (vXZ[0] + vXZ[1] + vXZ[2]) / 3.0f;
                                    glm::vec2 m0 = (vXZ[0] + vXZ[1]) * 0.5f - cent;
                                    glm::vec2 m1 = (vXZ[0] + vXZ[2]) * 0.5f - cent;
                                    glm::vec2 m2 = (vXZ[1] + vXZ[2]) * 0.5f - cent;
                                    float d0 = glm::dot(glm::vec2(fwd.x, fwd.z), m0);
                                    float d1 = glm::dot(glm::vec2(fwd.x, fwd.z), m1);
                                    float d2 = glm::dot(glm::vec2(fwd.x, fwd.z), m2);
                                    if (d0 >= d1 && d0 >= d2) facing = 0;
                                    else if (d1 >= d0 && d1 >= d2) facing = 1;
                                    else facing = 2;
                                }
                                placeLevel = facing; // Closed by default, facing in bits 0..1
                            } else if (Cell{b}.isLantern()) {
                                Cell below = world->getCell(place.x, place.y - 1, place.z, place.s);
                                Cell above = world->getCell(place.x, place.y + 1, place.z, place.s);
                                if (below.isSolid()) {
                                    placeLevel = 0; // Sitting on floor
                                } else if (above.isSolid()) {
                                    placeLevel = 1; // Hanging from ceiling
                                } else {
                                    canPlace = false;
                                }
                            } else if (Cell{b}.isFoliage()) {
                                // Foliage placement restriction (must be placed on grass or dirt)
                                Cell below = world->getCell(place.x, place.y - 1, place.z, place.s);
                                if (below.type != BlockType::Grass && below.type != BlockType::Dirt) {
                                    canPlace = false;
                                }
                            }

                            if (canPlace) {
                                world->setCellInstant(place.x, place.y, place.z, place.s, Cell{b, placeLevel});
                                player.consumeSelectedItem();
                                player.triggerPlace();
                                AudioEngine::get().playBlockStep(static_cast<int>(b), 0.9f);

                                // If placed block is Sand or Gravel, trigger falling gravity
                                if (b == BlockType::Sand || b == BlockType::Gravel) {
                                    world->checkGravity(place.x, place.y, place.z, place.s, &fallingBlockManager);
                                }
                            }
                        }
                    }
                }
            }
        }
    } else if (state == GameState::MainMenu) {
            if (Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options);
                if (action == 12) { // Open World Select
                    menuRenderer.refreshWorldList();
                    state = GameState::WorldSelect;
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 2) { // Open World Creation
                    menuRenderer.setCreationFieldFocus(0);
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 5) { // Quit
                    AudioEngine::get().playSound(SoundEffect::Click);
                    break;
                } else if (action == 6) { // Options
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
            }
        } else if (state == GameState::WorldSelect) {
            float scroll = Input::getScrollDelta();
            if (scroll > 0.1f) menuRenderer.handleWorldListScroll(1);
            else if (scroll < -0.1f) menuRenderer.handleWorldListScroll(-1);
            if (Input::isKeyPressed(GLFW_KEY_UP)) menuRenderer.handleWorldListScroll(1);
            if (Input::isKeyPressed(GLFW_KEY_DOWN)) menuRenderer.handleWorldListScroll(-1);
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                state = GameState::MainMenu;
                AudioEngine::get().playSound(SoundEffect::Click);
            }

            if (Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options);
                if (action == 13) { // Play Selected World
                    const auto& worlds = menuRenderer.getWorldList();
                    int idx = menuRenderer.getSelectedWorldIndex();
                    if (idx >= 0 && idx < static_cast<int>(worlds.size())) {
                        const auto& meta = worlds[idx];
                        currentWorldFolder = meta.folderName;
                        currentSeed = meta.seed;
                        world = std::make_unique<World>(context, commandQueue, currentSeed, currentWorldFolder);
                        world->renderDistance = options.renderDistance;
                        world->lodDistance = options.lodDistance;
                        world->lodPreset = options.lodPreset;
                        world->setRamCacheSize(options.ramCacheSize);

                        // Restore player state
                        player.setPosition(meta.playerPos);
                        player.getCamera().setYaw(meta.yaw);
                        player.getCamera().setPitch(meta.pitch);
                        player.getCamera().farPlane = std::max(500.0f, static_cast<float>(options.lodDistance) * 16.0f * 1.42f);
                        player.setHealth(meta.health);
                        player.setHunger(meta.hunger);
                        player.setSelectedSlot(meta.selectedSlot);
                        player.setCreative(meta.gameMode == 1);
                        timeOfDay = meta.timeOfDay;

                        player.clearInventory();
                        for (const auto& s : meta.inventory) {
                            if (s.slot >= 0 && s.slot < 10) {
                                player.setHotbarBlock(s.slot, s.type, s.count);
                            } else if (s.slot >= 10 && s.slot < 40) {
                                player.setStorageSlot(s.slot - 10, s.type, s.count);
                            }
                        }

                        state = GameState::LoadingWorld;
                        loadingProgress = 0.0f;
                        loadingLoadedChunks = 0;
                        loadingTotalChunks = 81;
                        window.setCursorMode(false);
                        AudioEngine::get().playSound(SoundEffect::Click);
                    }
                } else if (action == 2) { // Create New World
                    menuRenderer.setCreationFieldFocus(0);
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 4) { // Back to title
                    state = GameState::MainMenu;
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
            }
        } else if (state == GameState::WorldCreation) {
            for (char c : Input::getTypedChars()) {
                menuRenderer.handleWorldCreationChar(c);
            }
            if (Input::isKeyPressed(GLFW_KEY_BACKSPACE)) {
                menuRenderer.handleWorldCreationBackspace();
            }
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                state = GameState::WorldSelect;
                AudioEngine::get().playSound(SoundEffect::Click);
            }

            if (Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options);
                if (action == 3) { // Generate & Start World
                    WorldMetadata outMeta;
                    SaveManager::createWorld(menuRenderer.getNewWorldName(), currentSeed, menuRenderer.isStartInCreative(), outMeta);
                    currentWorldFolder = outMeta.folderName;
                    currentSeed = outMeta.seed;

                    world = std::make_unique<World>(context, commandQueue, currentSeed, currentWorldFolder);
                    world->renderDistance = options.renderDistance;
                    world->lodDistance = options.lodDistance;
                    world->lodPreset = options.lodPreset;
                    world->setRamCacheSize(options.ramCacheSize);
                    player.getCamera().farPlane = std::max(500.0f, static_cast<float>(options.lodDistance) * 16.0f * 1.42f);
                    player.resetToStarterInventory();
                    player.setCreative(menuRenderer.isStartInCreative());
                    state = GameState::LoadingWorld;
                    loadingProgress = 0.0f;
                    loadingLoadedChunks = 0;
                    loadingTotalChunks = 81;
                    window.setCursorMode(false);
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 4) { // Back to WorldSelect
                    state = GameState::WorldSelect;
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
            }
        } else if (state == GameState::LoadingWorld) {
            int loaded = 0;
            int total = 0;
            // Warm up 9x9 spawn area (radius = 4)
            bool ready = world->updateLoading(glm::vec3(0.0f, 64.0f, 0.0f), 4, loaded, total);
            loadingLoadedChunks = loaded;
            loadingTotalChunks = total;
            float targetPct = (total > 0) ? static_cast<float>(loaded) / static_cast<float>(total) : 1.0f;
            loadingProgress = std::max(loadingProgress, targetPct);

            static int lastLoggedLoaded = -1;
            if (loaded != lastLoggedLoaded) {
                std::cout << "[Loading] " << loaded << " / " << total << " chunks loaded ("
                          << static_cast<int>(loadingProgress * 100.0f) << "%)" << std::endl;
                lastLoggedLoaded = loaded;
            }

            if (ready) {
                float highY = world->getHighestSolidY(player.getPosition().x, player.getPosition().z);
                if (player.getPosition().y < highY || player.getPosition().y > highY + 20.0f) {
                    player.setPosition(glm::vec3(player.getPosition().x, highY + 2.5f, player.getPosition().z));
                }
                state = GameState::Playing;
                window.setCursorMode(true);
                lastLoggedLoaded = -1;
                std::cout << "[Loading] World loaded! Entering gameplay at ("
                          << player.getPosition().x << ", " << player.getPosition().y << ", " << player.getPosition().z << ")" << std::endl;
            }
        } else if (state == GameState::Paused) {
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                state = GameState::Playing;
                window.setCursorMode(true);
                AudioEngine::get().playSound(SoundEffect::Click);
            }
            if (Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options);
                if (action == 1) { // Resume
                    state = GameState::Playing;
                    window.setCursorMode(true);
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 4) { // Save & Title screen
                    if (world) {
                        world->saveAll(player, timeOfDay);
                    }
                    menuRenderer.refreshWorldList();
                    state = GameState::MainMenu;
                    window.setCursorMode(false);
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 5) { // Quit
                    if (world) {
                        world->saveAll(player, timeOfDay);
                    }
                    AudioEngine::get().playSound(SoundEffect::Click);
                    break;
                } else if (action == 6) { // Options
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
            }
        } else if (state == GameState::Options) {
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                state = menuRenderer.getPreviousState();
                AudioEngine::get().playSound(SoundEffect::Click);
                ConfigManager::save(options, exeDir + "options.txt");
            }
            bool isDown = Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
            bool isPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
            if (isDown) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options, !isPressed);
                if (action >= 7 && action <= 30) {
                    player.getCamera().fov = static_cast<float>(options.fov);
                    player.mouseSensitivity = options.mouseSens;
                    AudioEngine::get().setMasterVolume(options.audioVolume);
                    world->renderDistance = options.renderDistance;
                    world->lodDistance = options.lodDistance;
                    world->lodPreset = options.lodPreset;
                    player.getCamera().farPlane = std::max(500.0f, static_cast<float>(options.lodDistance) * 16.0f * 1.42f);
                    if (swapchain.isVSyncEnabled() != options.vsync) {
                        swapchain.setVSync(options.vsync, window.getWidth(), window.getHeight());
                    }
                    if (action == 13 || action == 14) {
                        if (action == 13) {
                            int targetW = (options.resIndex < 4) ? resList[options.resIndex][0] : window.getWidth();
                            int targetH = (options.resIndex < 4) ? resList[options.resIndex][1] : window.getHeight();
                            window.setWindowMode(options.windowMode, targetW, targetH, window.getRefreshRate());
                            swapchain.recreate(window.getWidth(), window.getHeight());
                        } else if (action == 14) {
                            if (options.windowMode == 0) {
                                int targetW = (options.resIndex < 4) ? resList[options.resIndex][0] : window.getWidth();
                                int targetH = (options.resIndex < 4) ? resList[options.resIndex][1] : window.getHeight();
                                window.setWindowMode(0, targetW, targetH, window.getRefreshRate());
                                swapchain.recreate(window.getWidth(), window.getHeight());
                            }
                        }
                        auto [targetRenderW, targetRenderH] = computeInternalResolution(options, window.getWidth(), window.getHeight());
                        if (targetRenderW != postProcessRenderer.getWidth() || targetRenderH != postProcessRenderer.getHeight()) {
                            postProcessRenderer.recreate(targetRenderW, targetRenderH);
                            updateWaterDescriptorSets();
                        }
                        if (window.getWidth() != tsrRenderer.getDisplayWidth() || window.getHeight() != tsrRenderer.getDisplayHeight()) {
                            tsrRenderer.recreate(window.getWidth(), window.getHeight());
                        }
                    }
                    ConfigManager::save(options, exeDir + "options.txt");
                } else if (action == 4) {
                    ConfigManager::save(options, exeDir + "options.txt");
                }
            }
        } else if (state == GameState::VideoSettings) {
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                state = GameState::Options;
                AudioEngine::get().playSound(SoundEffect::Click);
                ConfigManager::save(options, exeDir + "options.txt");
            }
            bool isDown = Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
            bool isPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
            if (isDown) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options, !isPressed);
                if (action != 0) {
                    if (action == 13 || action == 14 || action == 30) {
                        if (action == 13) {
                            int targetW = (options.resIndex < 4) ? resList[options.resIndex][0] : window.getWidth();
                            int targetH = (options.resIndex < 4) ? resList[options.resIndex][1] : window.getHeight();
                            window.setWindowMode(options.windowMode, targetW, targetH, window.getRefreshRate());
                            swapchain.recreate(window.getWidth(), window.getHeight());
                            tsrRenderer.recreate(window.getWidth(), window.getHeight());
                        } else if (action == 14) {
                            if (options.windowMode == 0) {
                                int targetW = (options.resIndex < 4) ? resList[options.resIndex][0] : window.getWidth();
                                int targetH = (options.resIndex < 4) ? resList[options.resIndex][1] : window.getHeight();
                                window.setWindowMode(0, targetW, targetH, window.getRefreshRate());
                                swapchain.recreate(window.getWidth(), window.getHeight());
                                tsrRenderer.recreate(window.getWidth(), window.getHeight());
                            }
                        }
                        auto [targetRenderW, targetRenderH] = computeInternalResolution(options, window.getWidth(), window.getHeight());
                        if (targetRenderW != postProcessRenderer.getWidth() || targetRenderH != postProcessRenderer.getHeight()) {
                            postProcessRenderer.recreate(targetRenderW, targetRenderH);
                            updateWaterDescriptorSets();
                        }
                        tsrRenderer.resetHistory();
                    } else if (action == 32) {
                        world->setRamCacheSize(options.ramCacheSize);
                    }
                    if (swapchain.isVSyncEnabled() != options.vsync) {
                        swapchain.setVSync(options.vsync, window.getWidth(), window.getHeight());
                    }
                    if (options.cloudSeed != currentCloudSeed) {
                        context.waitIdle();
                        cloudTexture = CloudMapGenerator::createCloudTexture(context, commandQueue, static_cast<uint32_t>(options.cloudSeed), exeDir);
                        cloudDescSet = cloudTexture->getDescriptorSet();
                        currentCloudSeed = options.cloudSeed;
                    }
                    world->renderDistance = options.renderDistance;
                    world->lodDistance = options.lodDistance;
                    world->lodPreset = options.lodPreset;
                    player.getCamera().farPlane = std::max(500.0f, static_cast<float>(options.lodDistance) * 16.0f * 1.42f);
                    ConfigManager::save(options, exeDir + "options.txt");
                }
            }
        } else if (state == GameState::LODSettings) {
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                state = GameState::Options;
                AudioEngine::get().playSound(SoundEffect::Click);
                ConfigManager::save(options, exeDir + "options.txt");
            }
            bool isDown = Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
            bool isPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
            if (isDown) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options, !isPressed);
                if (action != 0) {
                    world->renderDistance = options.renderDistance;
                    world->lodDistance = options.lodDistance;
                    world->lodPreset = options.lodPreset;
                    player.getCamera().farPlane = std::max(500.0f, static_cast<float>(options.lodDistance) * 16.0f * 1.42f);
                    ConfigManager::save(options, exeDir + "options.txt");
                }
            }
        } else if (state == GameState::VibrantVisualsSettings) {
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                state = GameState::Options;
                AudioEngine::get().playSound(SoundEffect::Click);
                ConfigManager::save(options, exeDir + "options.txt");
            }
            bool isDown = Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
            bool isPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
            if (isDown) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options, !isPressed);
                if (action != 0) {
                    ConfigManager::save(options, exeDir + "options.txt");
                }
            }
        } else if (state == GameState::AudioSettings) {
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                state = GameState::Options;
                AudioEngine::get().playSound(SoundEffect::Click);
                ConfigManager::save(options, exeDir + "options.txt");
            }
            bool isDown = Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
            bool isPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
            if (isDown) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options, !isPressed);
                if (action == 6 || action == 9) {
                    ConfigManager::save(options, exeDir + "options.txt");
                }
            }
        } else if (state == GameState::ControlsSettings) {
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                state = GameState::Options;
                AudioEngine::get().playSound(SoundEffect::Click);
                ConfigManager::save(options, exeDir + "options.txt");
            }
            bool isDown = Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
            bool isPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
            if (isDown) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options, !isPressed);
                if (action == 6 || action == 8) {
                    player.mouseSensitivity = options.mouseSens;
                    ConfigManager::save(options, exeDir + "options.txt");
                }
            }
        } else if (state == GameState::Inventory) {
            if (player.isCreative()) {
                for (char c : Input::getTypedChars()) {
                    menuRenderer.handleCreativeChar(c);
                }
                if (Input::isKeyPressed(GLFW_KEY_BACKSPACE)) {
                    menuRenderer.handleCreativeBackspace();
                }
                float scroll = Input::getScrollDelta();
                if (scroll > 0.1f) menuRenderer.handleCreativeScroll(-1);
                else if (scroll < -0.1f) menuRenderer.handleCreativeScroll(1);
                if (Input::isKeyPressed(GLFW_KEY_UP)) menuRenderer.handleCreativeScroll(-1);
                if (Input::isKeyPressed(GLFW_KEY_DOWN)) menuRenderer.handleCreativeScroll(1);

                if (Input::isKeyPressed(GLFW_KEY_ESCAPE) || (menuRenderer.getCreativeSearchQuery().empty() && Input::isKeyPressed(GLFW_KEY_E))) {
                    menuRenderer.returnCraftingItems(player);
                    state = GameState::Playing;
                    window.setCursorMode(true);
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
            } else {
                if (Input::isKeyPressed(GLFW_KEY_E) || Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                    menuRenderer.returnCraftingItems(player);
                    state = GameState::Playing;
                    window.setCursorMode(true);
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
            }
            bool leftPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
            bool rightPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
            if (leftPressed || rightPressed) {
                menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options, false, rightPressed, &itemDropManager);
            }
        } else if (state == GameState::CraftingTable) {
            if (Input::isKeyPressed(GLFW_KEY_E) || Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                menuRenderer.returnCraftingItems(player);
                state = GameState::Playing;
                window.setCursorMode(true);
                AudioEngine::get().playSound(SoundEffect::Click);
            }
            bool leftPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
            bool rightPressed = Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
            if (leftPressed || rightPressed) {
                menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options, false, rightPressed, &itemDropManager);
            }
        } else if (state == GameState::Death) {
            if (Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options);
                if (action == 11) { // Respawn on ground
                    state = GameState::LoadingWorld;
                    loadingProgress = 0.0f;
                    loadingLoadedChunks = 0;
                    loadingTotalChunks = 81;
                    window.setCursorMode(false);
                } else if (action == 4) { // Back to Main Menu
                    float gy = world->getHighestSolidY(0.0f, 0.0f);
                    player.respawn(glm::vec3(0.5f, gy + 0.1f, 0.5f));
                    state = GameState::MainMenu;
                    window.setCursorMode(false);
                }
            }
        }

        // -------------------------------------------------------------
        // Frame Rendering Pass
        // -------------------------------------------------------------
        uint32_t imageIndex = commandQueue.beginFrame(swapchain);
        if (imageIndex == UINT32_MAX) {
            swapchain.recreate(window.getWidth(), window.getHeight());
            tsrRenderer.recreate(window.getWidth(), window.getHeight());
            auto [targetRenderW, targetRenderH] = computeInternalResolution(options, window.getWidth(), window.getHeight());
            if (targetRenderW != postProcessRenderer.getWidth() || targetRenderH != postProcessRenderer.getHeight()) {
                postProcessRenderer.recreate(targetRenderW, targetRenderH);
                updateWaterDescriptorSets();
            }
            continue;
        }

        VkCommandBuffer cmd = commandQueue.getCommandBuffer();

        float aspect = static_cast<float>(swapchain.getExtent().width) / static_cast<float>(swapchain.getExtent().height);

        static uint32_t tsrFrameIndex = 0;
        static glm::mat4 prevViewProj = glm::mat4(1.0f);
        static bool firstTsrFrame = true;

        glm::vec2 jitterOffset(0.0f, 0.0f);
        glm::mat4 view, proj, unjitteredProj, vp, unjitteredVp;
        glm::vec3 camPos;
        Frustum frustum;
        if (state == GameState::MainMenu) {
            float camRadius = 45.0f;
            camPos = glm::vec3(std::cos(menuCamAngle) * camRadius, 85.0f, std::sin(menuCamAngle) * camRadius);
            view = glm::lookAt(camPos, glm::vec3(0.0f, 65.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            unjitteredProj = glm::perspective(glm::radians(75.0f), aspect, 0.1f, 500.0f);
            proj = unjitteredProj;
            frustum = player.getCamera().getFrustum(aspect);
        } else {
            camPos = player.getCamera().getRenderPosition();
            view = player.getCamera().getViewMatrix();
            unjitteredProj = player.getCamera().getProjectionMatrix(aspect);
            frustum = player.getCamera().getFrustum(aspect);

            if (options.upscalerMode == 2) { // TSR Temporal Super Resolution
                jitterOffset = Camera::getHaltonJitter(tsrFrameIndex++);
                glm::vec2 renderRes(postProcessRenderer.getWidth(), postProcessRenderer.getHeight());
                proj = player.getCamera().getJitteredProjectionMatrix(aspect, jitterOffset, renderRes);
            } else {
                proj = unjitteredProj;
            }
        }
        vp = proj * view;
        unjitteredVp = unjitteredProj * view;

        if (firstTsrFrame) {
            prevViewProj = unjitteredVp;
            firstTsrFrame = false;
        }

        PushConstants pc{};
        std::memcpy(pc.mvp, &vp[0][0], sizeof(float) * 16);

        // Sun & Moon Directional Light: Moon takes over when sun dips below horizon
        glm::vec3 activeLightDir = sunDir;
        float activeIntensity = sunIntensity;
        if (sunDir.y <= 0.0f) {
            activeLightDir = -sunDir; // Moon is on opposite celestial pole!
            activeIntensity = 0.42f;   // Luminous silver moonlight intensity for authentic visible nights
        }
        float signedIntensity = options.vibrantVisuals ? activeIntensity : -activeIntensity;
        pc.sunDir[0] = activeLightDir.x; pc.sunDir[1] = activeLightDir.y; pc.sunDir[2] = activeLightDir.z; pc.sunDir[3] = signedIntensity;

        pc.skyFog[0] = skyColor.r; pc.skyFog[1] = skyColor.g; pc.skyFog[2] = skyColor.b;
        pc.skyFog[3] = isUnderwater ? -fogDistance : fogDistance;
        pc.camPos[0] = camPos.x; pc.camPos[1] = camPos.y; pc.camPos[2] = camPos.z; pc.camPos[3] = timer.getElapsedTime();

        const glm::vec3& pPos = player.getPosition();
        bool hasHeldTorch = (player.getSelectedBlock() == BlockType::Torch);
        float yaw = player.getCamera().getYaw();
        float walkSwing = std::sin(player.getBobTime()) * 0.62f * player.getBobWeight();

        // Pack walkSwing and yaw cleanly into IEEE-754 float
        float swingNorm = std::clamp((walkSwing + 0.7f) / 1.4f, 0.0f, 1.0f);
        float normYaw = std::fmod(yaw, glm::two_pi<float>());
        if (normYaw < 0.0f) normYaw += glm::two_pi<float>();
        int yawInt = static_cast<int>(std::round(normYaw * 1000.0f));
        int swingInt = static_cast<int>(std::round(swingNorm * 1000.0f));
        float packedVal = static_cast<float>(yawInt * 1001 + swingInt);
        if (hasHeldTorch) packedVal = -packedVal - 1.0f;
        pc.lightColor[0] = pPos.x; pc.lightColor[1] = pPos.y; pc.lightColor[2] = pPos.z; pc.lightColor[3] = packedVal;

        // Gather dynamic light sources (placed torches in world & dropped torch items)
        struct DynamicLightCandidate {
            glm::vec3 pos;
            float intensity;
            float distSq;
        };
        std::vector<DynamicLightCandidate> lightCandidates;

        auto placedTorches = world->getNearestPlacedTorches(pPos, 4, 28.0f);
        for (const auto& pt : placedTorches) {
            float flicker = 0.95f + 0.05f * std::sin(timer.getElapsedTime() * 9.7f + pt.x * 3.1f + pt.z * 1.7f);
            glm::vec3 diff = pPos - pt;
            float dSq = glm::dot(diff, diff);
            lightCandidates.push_back({pt, 1.0f * flicker, dSq});
        }

        auto droppedTorch = itemDropManager.getNearestTorchDrop(pPos, 22.0f);
        if (droppedTorch.has_value()) {
            glm::vec3 diff = pPos - droppedTorch->first;
            float dSq = glm::dot(diff, diff);
            lightCandidates.push_back({droppedTorch->first, droppedTorch->second, dSq});
        }

        std::sort(lightCandidates.begin(), lightCandidates.end(), [](const DynamicLightCandidate& a, const DynamicLightCandidate& b) {
            return a.distSq < b.distSq;
        });

        auto assignPointLight = [&](float* dest, size_t index) {
            if (index < lightCandidates.size()) {
                dest[0] = lightCandidates[index].pos.x;
                dest[1] = lightCandidates[index].pos.y;
                dest[2] = lightCandidates[index].pos.z;
                dest[3] = lightCandidates[index].intensity;
            } else {
                dest[0] = 0.0f; dest[1] = 0.0f; dest[2] = 0.0f; dest[3] = 0.0f;
            }
        };

        assignPointLight(pc.pointLight1, 0);
        assignPointLight(pc.pointLight2, 1);
        assignPointLight(pc.pointLight3, 2);
        assignPointLight(pc.pointLight4, 3);

        // Exact Handheld Torch in player's right hand (with organic subtle flicker)
        if (hasHeldTorch) {
            glm::vec3 fwd = player.getCamera().getForward();
            glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 torchWorldPos;
            if (player.getCamera().getMode() == CameraMode::FirstPerson) {
                torchWorldPos = camPos + fwd * 0.42f + right * 0.32f - glm::vec3(0.0f, 0.20f, 0.0f);
            } else {
                torchWorldPos = pPos + glm::vec3(0.0f, 1.15f, 0.0f) + fwd * 0.38f + right * 0.34f;
            }
            float torchFlicker = 0.95f + 0.05f * std::sin(timer.getElapsedTime() * 11.5f);
            pc.heldTorch[0] = torchWorldPos.x;
            pc.heldTorch[1] = torchWorldPos.y;
            pc.heldTorch[2] = torchWorldPos.z;
            pc.heldTorch[3] = 1.0f * torchFlicker;
        } else {
            pc.heldTorch[0] = 0.0f; pc.heldTorch[1] = 0.0f; pc.heldTorch[2] = 0.0f; pc.heldTorch[3] = 0.0f;
        }

        // Shader & Graphics Pack Options packed into pc.shaderOptions
        int effectiveShadowQ = options.vibrantVisuals ? options.shadowQuality : 0;
        int effectiveWaterQ  = options.vibrantVisuals ? options.waterQuality : 0;
        pc.shaderOptions[0] = static_cast<float>(effectiveShadowQ);
        pc.shaderOptions[1] = static_cast<float>(effectiveWaterQ);
        pc.shaderOptions[2] = options.smoothLighting ? options.aoStrength : 0.0f;
        int settingsFlags = ((options.playerShadow && options.vibrantVisuals) ? 1 : 0)
                          | ((options.clouds ? 1 : 0) << 1)
                          | (((options.cloudShadows && options.vibrantVisuals) ? 1 : 0) << 2)
                          | ((options.smoothLighting ? 1 : 0) << 3)
                          | ((options.torchColorBleed ? 1 : 0) << 4);
        pc.shaderOptions[3] = static_cast<float>(settingsFlags);

        float dayFactor = std::clamp((sunHeight + 0.10f) / 0.35f, 0.0f, 1.0f);
        pc.dayInfo[0] = dayFactor;
        pc.dayInfo[1] = sunHeight;
        pc.dayInfo[2] = options.exposure;
        pc.dayInfo[3] = options.fogDensity;

        // =============================================================
        // Cascaded Shadow Map Depth Pre-Pass & Shadow UBO Upload
        // =============================================================
        bool render3D = (state == GameState::Playing || state == GameState::Paused ||
                         state == GameState::Inventory || state == GameState::CraftingTable ||
                         state == GameState::Death);

        uint32_t currentFrame = commandQueue.getCurrentFrame();
        VkDescriptorSet currentSceneDescSet = sceneDescSets[currentFrame];
        playerModelRenderer.resetFrame();

        float normPlayerSky = 1.0f;
        float normPlayerTorch = 0.0f;

        if (render3D) {
            // Update Cascades based on camera and active light direction (sun during day, moon at night)
            shadowMap.updateCascades(player.getCamera().getViewMatrix(),
                                     glm::radians(player.getCamera().fov),
                                     aspect,
                                     0.1f,
                                     500.0f,
                                     activeLightDir,
                                     options.shadowDistance);

            ShadowUBO shadowUBOData{};
            const auto& shadowMats = shadowMap.getCascadeShadowMatrices();
            shadowUBOData.lightViewProj[0] = shadowMats[0];
            shadowUBOData.lightViewProj[1] = shadowMats[1];
            const auto& splits = shadowMap.getCascadeSplits();
            shadowUBOData.cascadeSplits = glm::vec4(splits[0], splits[1], 0.0f, 0.0f);
            shadowUboBuffers[currentFrame].upload(&shadowUBOData, sizeof(ShadowUBO));

            // Query Steve's ambient skylight and torchlight at player position
            int playerSky = 15, playerBlock = 0;
            int ppx = static_cast<int>(std::floor(pPos.x));
            int ppy = static_cast<int>(std::floor(pPos.y));
            int ppz = static_cast<int>(std::floor(pPos.z));
            world->getLightLevels(ppx, ppy, ppz, 0, playerSky, playerBlock);
            normPlayerSky = static_cast<float>(playerSky) / 15.0f;
            normPlayerTorch = static_cast<float>(playerBlock) / 15.0f;

            // Pre-build Steve's mesh so both shadow depth pass and 3D forward pass use identical vertices
            playerModelRenderer.buildModelMesh(player, normPlayerSky, normPlayerTorch);

            if (options.shadowQuality > 0) {
                for (uint32_t cIdx = 0; cIdx < ShadowMap::CASCADE_COUNT; ++cIdx) {
                    shadowMap.transitionForRendering(cmd, cIdx);

                    VkRenderingAttachmentInfo shadowDepthAtt{};
                    shadowDepthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    shadowDepthAtt.imageView = shadowMap.getLayerImageView(cIdx);
                    shadowDepthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                    shadowDepthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    shadowDepthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                    shadowDepthAtt.clearValue.depthStencil = { 1.0f, 0 };

                    VkRenderingInfo shadowRenderInfo{};
                    shadowRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                    shadowRenderInfo.renderArea = { { 0, 0 }, { ShadowMap::RESOLUTION, ShadowMap::RESOLUTION } };
                    shadowRenderInfo.layerCount = 1;
                    shadowRenderInfo.colorAttachmentCount = 0;
                    shadowRenderInfo.pColorAttachments = nullptr;
                    shadowRenderInfo.pDepthAttachment = &shadowDepthAtt;

                    vkCmdBeginRendering(cmd, &shadowRenderInfo);

                    VkViewport shadowViewport{};
                    shadowViewport.x = 0.0f;
                    shadowViewport.y = static_cast<float>(ShadowMap::RESOLUTION);
                    shadowViewport.width = static_cast<float>(ShadowMap::RESOLUTION);
                    shadowViewport.height = -static_cast<float>(ShadowMap::RESOLUTION);
                    shadowViewport.minDepth = 0.0f;
                    shadowViewport.maxDepth = 1.0f;
                    vkCmdSetViewport(cmd, 0, 1, &shadowViewport);

                    VkRect2D shadowScissor{ { 0, 0 }, { ShadowMap::RESOLUTION, ShadowMap::RESOLUTION } };
                    vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

                    csmPipeline.bind(cmd);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            csmPipeline.getLayout(), 0, 1,
                                            &descSet, 0, nullptr);

                    const glm::mat4& lightVP = shadowMap.getCascadeViewProjections()[cIdx];

                    // Render Steve's 3D model into shadow map
                    if (options.playerShadow) {
                        playerModelRenderer.renderShadow(cmd, csmPipeline, lightVP);
                    }

                    // Render nearby terrain chunks into shadow map (with margin for casters towards the sun)
                    float maxChunkDist = (cIdx == 0) ? (splits[0] + 36.0f) : (splits[1] + 48.0f);
                    float maxChunkDistSq = maxChunkDist * maxChunkDist;

                    for (const auto& [coord, chunk] : world->getMeshes()) {
                        if (chunk.opaqueIndexCount == 0 || !chunk.opaqueVertexBuffer.isValid()) continue;

                        float chunkCenterX = static_cast<float>(coord.cx * CHUNK_SIZE_X + CHUNK_SIZE_X / 2);
                        float chunkCenterZ = static_cast<float>(coord.cz * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2);
                        float cdx = chunkCenterX - pPos.x;
                        float cdz = chunkCenterZ - pPos.z;
                        if (cdx * cdx + cdz * cdz > maxChunkDistSq) continue;

                        vkCmdPushConstants(cmd, csmPipeline.getLayout(),
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(float) * 16, &lightVP[0][0]);

                        VkBuffer vbs[] = { chunk.opaqueVertexBuffer.getBuffer() };
                        VkDeviceSize offsets[] = { 0 };
                        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
                        vkCmdBindIndexBuffer(cmd, chunk.opaqueIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
                        vkCmdDrawIndexed(cmd, chunk.opaqueIndexCount, 1, 0, 0, 0);
                    }

                    vkCmdEndRendering(cmd);
                }

                // Transition shadow map array to shader read-only for forward lighting pass
                shadowMap.transitionForSampling(cmd);
            }
        }

        // =============================================================
        // Pass 1: 3D Scene HDR Dynamic Rendering Pass
        // =============================================================
        postProcessRenderer.transitionHDRForRendering(cmd);
        postProcessRenderer.transitionDepthForRendering(cmd);

        auto srgbToLinear = [](float c) {
            return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
        };
        glm::vec3 linearSky(srgbToLinear(skyColor.r), srgbToLinear(skyColor.g), srgbToLinear(skyColor.b));

        VkExtent2D renderExtent = { postProcessRenderer.getWidth(), postProcessRenderer.getHeight() };

        VkRenderingAttachmentInfo hdrColorAtt{};
        hdrColorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        hdrColorAtt.imageView = postProcessRenderer.getHDRImageView();
        hdrColorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        hdrColorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        hdrColorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        hdrColorAtt.clearValue.color = {{ linearSky.r, linearSky.g, linearSky.b, 1.0f }};

        VkRenderingAttachmentInfo depthAtt{};
        depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAtt.imageView = postProcessRenderer.getSceneDepthImageView();
        depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAtt.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo hdrRenderInfo{};
        hdrRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        hdrRenderInfo.renderArea = {{ 0, 0 }, renderExtent};
        hdrRenderInfo.layerCount = 1;
        hdrRenderInfo.colorAttachmentCount = 1;
        hdrRenderInfo.pColorAttachments = &hdrColorAtt;
        hdrRenderInfo.pDepthAttachment = &depthAtt;

        vkCmdBeginRendering(cmd, &hdrRenderInfo);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = static_cast<float>(renderExtent.height);
        viewport.width = static_cast<float>(renderExtent.width);
        viewport.height = -static_cast<float>(renderExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{{ 0, 0 }, renderExtent};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        if (render3D) {
            // 0. Render Procedural Atmospheric Sky Dome at Far Plane (Depth = 1.0)
            skyRenderer.render(cmd, skyPipeline, camPos, sunDir, vp, dayFactor, sunHeight, options.exposure);

            // Bind World Pipeline & Scene Descriptor Set (Atlas + Cascaded Shadows + Shadow UBO)
            worldPipeline.bind(cmd);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    worldPipeline.getLayout(), 0, 1,
                                    &currentSceneDescSet, 0, nullptr);

            // 1. Render Triangular Sun & Moon
            celestialRenderer.render(cmd, worldPipeline, camPos, sunDir, vp);

            // 2. Render 3D Drifting Volumetric Clouds (rendered before terrain so chunks occlude them, and SSR captures them)
            if (options.clouds && !isUnderwater) {
                cloudPipeline.bind(cmd);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cloudPipeline.getLayout(), 0, 1, &cloudDescSet, 0, nullptr);
                cloudRenderer.render(cmd, cloudPipeline, camPos, timer.getElapsedTime(), vp, skyColor, sunDir, dayFactor, sunHeight, options.vibrantVisuals, options.exposure);
                worldPipeline.bind(cmd);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline.getLayout(), 0, 1, &currentSceneDescSet, 0, nullptr);
            }

            // 3. Render Opaque World Chunks
            for (const auto& [coord, chunk] : world->getMeshes()) {
                if (chunk.opaqueIndexCount == 0 || !chunk.opaqueVertexBuffer.isValid()) continue;
                if (!World::isChunkInFrustum(coord, frustum)) continue;

                vkCmdPushConstants(cmd, worldPipeline.getLayout(),
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0, sizeof(PushConstants), &pc);

                VkBuffer vbs[] = { chunk.opaqueVertexBuffer.getBuffer() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
                vkCmdBindIndexBuffer(cmd, chunk.opaqueIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, chunk.opaqueIndexCount, 1, 0, 0, 0);
            }

            // Render Block Cracks (mining overlay stages 0-9)
            if (state == GameState::Playing && crackStage >= 0 && currentMiningCell.has_value()) {
                crackRenderer.render(cmd, crackPipeline, currentSceneDescSet, *world, currentMiningCell, crackStage, vp, pc);
                worldPipeline.bind(cmd);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline.getLayout(), 0, 1, &currentSceneDescSet, 0, nullptr);
            }

            // Render 3D Floating Item Drops
            itemDropManager.render(cmd, worldPipeline, vp, pc);

            // Render Animated Falling Sand & Gravel
            fallingBlockManager.render(cmd, worldPipeline, vp);

            // Render In-Flight & Embedded 3D Arrows
            arrowManager.render(cmd, worldPipeline, vp);

            // Render Minecraft F7 Discrete Light Level Overlay
            if (options.lightOverlay && state == GameState::Playing) {
                lightOverlayRenderer.render(cmd, worldPipeline, *world, player.getPosition(), camPos, vp);
            }

            // Render Targeted Prism Outline with Mathematical Color Inversion
            if (state == GameState::Playing && targetHit.has_value()) {
                outlineInvertPipeline.bind(cmd);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, outlineInvertPipeline.getLayout(), 0, 1, &currentSceneDescSet, 0, nullptr);
                outlineRenderer.render(cmd, outlineInvertPipeline, targetHit->hitCell, vp, *world);
                worldPipeline.bind(cmd);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline.getLayout(), 0, 1, &currentSceneDescSet, 0, nullptr);
            }

            // Render Third-Person Triangular Player Character Model (Steve in world)
            if (state == GameState::Playing || state == GameState::Paused || state == GameState::Inventory || state == GameState::CraftingTable) {
                if (player.getCamera().getMode() != CameraMode::FirstPerson) {
                    playerModelRenderer.render(cmd, worldPipeline, player, vp, pc, normPlayerSky, normPlayerTorch);
                }
            }

            // 4. Water Dynamic Render Pass (with SSR snapshot)
            vkCmdEndRendering(cmd);

            postProcessRenderer.copyHDRToSSR(cmd);

            VkRenderingAttachmentInfo waterColorAtt = hdrColorAtt;
            waterColorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            waterColorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            VkRenderingAttachmentInfo waterDepthAtt = depthAtt;
            waterDepthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            waterDepthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

            VkRenderingInfo waterRenderInfo = hdrRenderInfo;
            waterRenderInfo.pColorAttachments = &waterColorAtt;
            waterRenderInfo.pDepthAttachment = &waterDepthAtt;

            vkCmdBeginRendering(cmd, &waterRenderInfo);

            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkDescriptorSet currentWaterDescSet = waterDescSets[currentFrame];
            waterPipeline.bind(cmd);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, waterPipeline.getLayout(), 0, 1, &currentWaterDescSet, 0, nullptr);

            for (const auto& [coord, chunk] : world->getMeshes()) {
                if (chunk.waterIndexCount == 0 || !chunk.waterVertexBuffer.isValid()) continue;
                if (!World::isChunkInFrustum(coord, frustum)) continue;

                vkCmdPushConstants(cmd, waterPipeline.getLayout(),
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0, sizeof(PushConstants), &pc);

                VkBuffer vbs[] = { chunk.waterVertexBuffer.getBuffer() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
                vkCmdBindIndexBuffer(cmd, chunk.waterIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, chunk.waterIndexCount, 1, 0, 0, 0);
            }

            // 5. Render First-Person Hand (after water and SSR copy so it is NEVER reflected in water)
            if (state == GameState::Playing || state == GameState::Paused || state == GameState::Inventory || state == GameState::CraftingTable) {
                if (player.getCamera().getMode() == CameraMode::FirstPerson) {
                    // Clear depth attachment so view-model hand and held items never clip through blocks/walls
                    VkClearAttachment clearDepthAtt{};
                    clearDepthAtt.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    clearDepthAtt.clearValue.depthStencil = { 1.0f, 0 };
                    VkClearRect clearDepthRect{};
                    clearDepthRect.rect.offset = { 0, 0 };
                    clearDepthRect.rect.extent = swapchain.getExtent();
                    clearDepthRect.baseArrayLayer = 0;
                    clearDepthRect.layerCount = 1;
                    vkCmdClearAttachments(cmd, 1, &clearDepthAtt, 1, &clearDepthRect);

                    worldPipeline.bind(cmd);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline.getLayout(), 0, 1, &currentSceneDescSet, 0, nullptr);
                    handRenderer.render(cmd, worldPipeline, player, view, proj, pc, normPlayerSky, normPlayerTorch);
                }
            }
        }

        vkCmdEndRendering(cmd);

        // =============================================================
        // Pass 2: Tonemap & Composite 3D Scene into LDR Target (render resolution)
        // =============================================================
        // Compute Sun Screen UV for Volumetric God Rays in Post-Processing
        float sunScreenU = 0.5f;
        float sunScreenV = 0.5f;
        float godRaySunIntensity = 0.0f;
        glm::vec3 camForward = (state == GameState::Playing) ? player.getCamera().getForward() : glm::normalize(glm::vec3(0.0f, 65.0f, 0.0f) - camPos);
        float sunDotForward = glm::dot(sunDir, camForward);
        // God rays occur selectively at lower sun angles (sunrise, morning, afternoon, sunset, golden hour)
        // when looking generally towards the sun, and NEVER when looking down at the ground
        float sunElevationWeight = std::clamp((0.48f - sunHeight) / 0.28f, 0.0f, 1.0f) * std::clamp((sunHeight + 0.06f) / 0.16f, 0.0f, 1.0f);
        float pitchWeight = std::clamp((camForward.y + 0.15f) / 0.30f, 0.0f, 1.0f);
        if (options.vibrantVisuals && !isUnderwater && sunDotForward > 0.45f && sunElevationWeight > 0.01f && pitchWeight > 0.01f && sunIntensity > 0.01f) {
            glm::vec4 sunClip = vp * glm::vec4(camPos + sunDir * 500.0f, 1.0f);
            if (sunClip.w > 0.1f) {
                float u = (sunClip.x / sunClip.w) * 0.5f + 0.5f;
                float v = 1.0f - ((sunClip.y / sunClip.w) * 0.5f + 0.5f);
                if (u >= -0.15f && u <= 1.15f && v >= -0.15f && v <= 1.15f) {
                    sunScreenU = u;
                    sunScreenV = v;
                    float forwardFade = std::clamp((sunDotForward - 0.45f) / 0.35f, 0.0f, 1.0f);
                    godRaySunIntensity = sunIntensity * dayFactor * forwardFade * sunElevationWeight * pitchWeight;
                }
            }
        }

        // 1. Tonemap 3D Scene to LDR at internal render resolution
        postProcessRenderer.renderToLDR(cmd,
                                        options.exposure, 1.0f, 0.05f, timer.getElapsedTime(),
                                        (options.vibrantVisuals && options.shadersEnabled), 0.0f,
                                        isUnderwater ? 1.0f : 0.0f,
                                        sunScreenU, sunScreenV, godRaySunIntensity, sunHeight);

        // =============================================================
        // Pass 3: TSR Temporal Super Resolution / FSR 1.0 Spatial Upscale into Swapchain
        // =============================================================
        swapchain.transitionToColorAttachment(cmd, imageIndex);

        glm::mat4 currInvViewProj = glm::inverse(unjitteredVp);

        tsrRenderer.render(cmd,
                           postProcessRenderer.getLDRImageView(),
                           postProcessRenderer.getSceneDepthImageView(),
                           swapchain.getImageView(imageIndex),
                           { postProcessRenderer.getWidth(), postProcessRenderer.getHeight() },
                           swapchain.getExtent(),
                           currInvViewProj,
                           prevViewProj,
                           jitterOffset,
                           options.upscalerMode,
                           options.upscalerSharpness);

        prevViewProj = unjitteredVp;

        // =============================================================
        // Pass 4: Crisp 2D UI & Menus Overlay on top at 100% Native Resolution
        // =============================================================
        VkRenderingAttachmentInfo uiColorAtt{};
        uiColorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        uiColorAtt.imageView = swapchain.getImageView(imageIndex);
        uiColorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        uiColorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Preserve upscaled 3D scene!
        uiColorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo uiRenderInfo{};
        uiRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        uiRenderInfo.renderArea = {{ 0, 0 }, swapchain.getExtent()};
        uiRenderInfo.layerCount = 1;
        uiRenderInfo.colorAttachmentCount = 1;
        uiRenderInfo.pColorAttachments = &uiColorAtt;
        uiRenderInfo.pDepthAttachment = nullptr;

        vkCmdBeginRendering(cmd, &uiRenderInfo);

        VkViewport uiViewport{};
        uiViewport.x = 0.0f;
        uiViewport.y = static_cast<float>(swapchain.getExtent().height);
        uiViewport.width = static_cast<float>(swapchain.getExtent().width);
        uiViewport.height = -static_cast<float>(swapchain.getExtent().height);
        uiViewport.minDepth = 0.0f;
        uiViewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &uiViewport);

        VkRect2D uiScissor{{ 0, 0 }, swapchain.getExtent()};
        vkCmdSetScissor(cmd, 0, 1, &uiScissor);

        uiPipeline.bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline.getLayout(), 0, 1, &descSet, 0, nullptr);

        if (state == GameState::Playing) {
            uiRenderer.render(cmd, uiPipeline, &invertPipeline, player, uiW, uiH, displayedFPS, options, world.get(),
                              isChatOpen, chatInput, chatFeedback, chatFeedbackTimer, descSet, sleepFadeAlpha);
        }

        if (state != GameState::Playing) {
            menuRenderer.render(cmd, uiPipeline, state, player, uiW, uiH, uiMousePos, options, loadingProgress, loadingLoadedChunks, loadingTotalChunks, descSet);
        }

        vkCmdEndRendering(cmd);

        // Transition to Present & Swap Buffers
        swapchain.transitionToPresent(cmd, imageIndex);

        if (!commandQueue.endFrame(swapchain, imageIndex)) {
            swapchain.recreate(window.getWidth(), window.getHeight());
            tsrRenderer.recreate(window.getWidth(), window.getHeight());
            auto [targetRenderW, targetRenderH] = computeInternalResolution(options, window.getWidth(), window.getHeight());
            if (targetRenderW != postProcessRenderer.getWidth() || targetRenderH != postProcessRenderer.getHeight()) {
                postProcessRenderer.recreate(targetRenderW, targetRenderH);
                updateWaterDescriptorSets();
            }
        }

        // Max FPS Framerate Limiter
        if (options.maxFps > 0) {
            float targetFrameTime = 1.0f / static_cast<float>(options.maxFps);
            float frameElapsed = timer.getDeltaTime();
            if (frameElapsed < targetFrameTime) {
                float sleepSec = targetFrameTime - frameElapsed;
                std::this_thread::sleep_for(std::chrono::duration<float>(sleepSec));
            }
        }
    }

    if (world) {
        world->saveAll(player, timeOfDay);
    }

    context.waitIdle();
    vkDestroyDescriptorPool(context.getDevice(), sceneDescPool, nullptr);
    vkDestroyDescriptorSetLayout(context.getDevice(), sceneDescLayout, nullptr);
    vkDestroyDescriptorPool(context.getDevice(), waterDescPool, nullptr);
    vkDestroyDescriptorSetLayout(context.getDevice(), waterDescLayout, nullptr);
    AudioEngine::get().shutdown();
}

} // namespace prismcraft

int main() {
    try {
        prismcraft::run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
