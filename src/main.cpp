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
#include "renderer/LightOverlayRenderer.hpp"
#include "ui/UIRenderer.hpp"
#include "ui/MenuRenderer.hpp"
#include "renderer/PlayerModelRenderer.hpp"
#include "game/ArrowManager.hpp"

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

namespace prismcraft {

static float calculateBreakDuration(BlockType block, BlockType tool) {
    if (Cell{block}.isFoliage() || Cell{block}.isTorch()) {
        return 0.05f; // Instant break for plants, flowers, tall grass, torches
    }

    float baseTime = 1.0f;
    bool requiresPickaxe = false;
    bool isWoodBlock = (block == BlockType::Wood || block == BlockType::Planks || block == BlockType::CraftingTable);
    bool isStoneBlock = (block == BlockType::Stone || block == BlockType::CobbleStone || block == BlockType::Furnace);
    bool isOreBlock = (block == BlockType::OreCoal || block == BlockType::OreIron || block == BlockType::OreGold || block == BlockType::OreDiamond);
    bool isDirtBlock = (block == BlockType::Grass || block == BlockType::Dirt || block == BlockType::Sand || block == BlockType::Gravel || block == BlockType::Snow);

    if (isWoodBlock) {
        baseTime = 1.8f;
    } else if (isStoneBlock) {
        baseTime = 3.2f;
        requiresPickaxe = true;
    } else if (isOreBlock) {
        baseTime = 4.8f;
        requiresPickaxe = true;
    } else if (isDirtBlock) {
        baseTime = 0.65f;
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
        case 4: tierMultiplier = 8.5f; break;  // Diamond
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
    Window window("PrismCraft", 1280, 720);
    Input::init(window.getHandle());
    
    // Initialize procedural Audio Engine
    if (AudioEngine::get().init()) {
        std::cout << "[Audio] AudioEngine initialized." << std::endl;
    }
    
    std::string exeDir = getExeDir();
    GameOptions options;
    ConfigManager::load(options, exeDir + "options.txt");

    VulkanContext context(window.getHandle());
    Swapchain swapchain(context, window.getWidth(), window.getHeight(), options.vsync);
    CommandQueue commandQueue(context);
    
    // Generate & Upload 256x256 Pixel-Art Texture Atlas
    std::vector<uint8_t> atlasPixels = TextureAtlas::generateAtlasPixels();
    Texture textureAtlas(context, commandQueue, TextureAtlas::ATLAS_WIDTH, TextureAtlas::ATLAS_HEIGHT, atlasPixels.data());
    VkDescriptorSetLayout descLayout = textureAtlas.getDescriptorSetLayout();
    VkDescriptorSet descSet = textureAtlas.getDescriptorSet();

    // 3D Opaque World Pipeline (Back-face culling, Depth test & write ON, Alpha Blending OFF)
    Pipeline worldPipeline(context, swapchain.getImageFormat(), swapchain.getDepthFormat(),
                           exeDir + "assets/shaders/cell.vert.spv", exeDir + "assets/shaders/cell.frag.spv",
                           descLayout, true, true, BlendMode::None, VK_CULL_MODE_BACK_BIT);

    // 3D Translucent Water Pipeline (Double-sided, Depth test ON, Depth write OFF, Alpha Blending ON)
    Pipeline waterPipeline(context, swapchain.getImageFormat(), swapchain.getDepthFormat(),
                           exeDir + "assets/shaders/cell.vert.spv", exeDir + "assets/shaders/water.frag.spv",
                           descLayout, true, false, BlendMode::Alpha, VK_CULL_MODE_NONE);

    // 3D Volumetric Cloud Pipeline (Back-face culling ON, Depth test ON, Depth write OFF, Alpha Blending ON)
    Pipeline cloudPipeline(context, swapchain.getImageFormat(), swapchain.getDepthFormat(),
                           exeDir + "assets/shaders/cell.vert.spv", exeDir + "assets/shaders/cloud.frag.spv",
                           descLayout, true, false, BlendMode::Alpha, VK_CULL_MODE_BACK_BIT);

    // 3D Invert Pipeline for Block Wireframe (Depth test ON, Depth write OFF, BlendMode::Invert)
    Pipeline outlineInvertPipeline(context, swapchain.getImageFormat(), swapchain.getDepthFormat(),
                                   exeDir + "assets/shaders/cell.vert.spv", exeDir + "assets/shaders/cell.frag.spv",
                                   descLayout, true, false, BlendMode::Invert, VK_CULL_MODE_NONE);

    // 2D UI Pipeline (Targeting Swapchain sRGB image directly, Depth test/write OFF)
    Pipeline uiPipeline(context, swapchain.getImageFormat(), swapchain.getDepthFormat(),
                        exeDir + "assets/shaders/ui.vert.spv", exeDir + "assets/shaders/ui.frag.spv",
                        descLayout, false, false, BlendMode::Alpha, VK_CULL_MODE_NONE);

    // 2D Invert Pipeline for Crosshair (No culling, Depth test & write OFF, BlendMode::Invert)
    Pipeline invertPipeline(context, swapchain.getImageFormat(), swapchain.getDepthFormat(),
                            exeDir + "assets/shaders/cell.vert.spv", exeDir + "assets/shaders/cell.frag.spv",
                            descLayout, false, false, BlendMode::Invert, VK_CULL_MODE_NONE);
    
    std::cout << "[Main] Pipelines created, creating World..." << std::endl;
    uint32_t currentSeed = 42;
    std::unique_ptr<World> world = std::make_unique<World>(context, commandQueue, currentSeed);
    world->renderDistance = options.renderDistance;
    world->lodPreset = options.lodPreset;
    
    std::cout << "[Main] World created, placing player..." << std::endl;
    float spawnY = world->getHighestSolidY(0.0f, 0.0f);
    Player player(glm::vec3(0.5f, spawnY + 2.5f, 0.5f));
    player.getCamera().fov = static_cast<float>(options.fov);
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
    LightOverlayRenderer lightOverlayRenderer(context, commandQueue);
    
    std::cout << "[Main] All renderers initialized, entering main game loop!" << std::endl;
    GameState state = GameState::MainMenu;
    window.setCursorMode(false);
    
    Timer timer;
    float fpsTimer = 0.0f;
    int fpsFrameCount = 0;
    float displayedFPS = 60.0f;
    float menuCamAngle = 0.0f;
    float timeOfDay = 0.0f; // Start at Noon
    
    bool isChatOpen = false;
    std::string chatInput = "";
    std::string chatFeedback = "";
    float chatFeedbackTimer = 0.0f;
    
    float miningTimer = 0.0f;
    std::optional<CellCoord> currentMiningCell = std::nullopt;
    int crackStage = -1;

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
        
        uint32_t screenW = static_cast<uint32_t>(window.getWidth());
        uint32_t screenH = static_cast<uint32_t>(window.getHeight());
        
        if (screenW == 0 || screenH == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        glm::vec2 mousePos = Input::getMousePosition();
        uint32_t uiW = static_cast<uint32_t>(static_cast<float>(screenW) / options.uiScale);
        uint32_t uiH = static_cast<uint32_t>(static_cast<float>(screenH) / options.uiScale);
        glm::vec2 uiMousePos = mousePos / options.uiScale;

        // Advance Day / Night Cycle (720s / 12 min full cycle)
        timeOfDay += dt / 720.0f;
        if (timeOfDay > 1.0f) timeOfDay -= 1.0f;

        float celestialAngle = timeOfDay * glm::two_pi<float>();
        glm::vec3 sunDir(std::sin(celestialAngle), std::cos(celestialAngle), 0.35f);
        sunDir = glm::normalize(sunDir);

        float sunHeight = sunDir.y;
        float sunIntensity = std::clamp((sunHeight + 0.15f) / 0.65f, 0.0f, 1.0f);

        // Dynamic atmospheric sky color
        glm::vec3 daySky(0.40f, 0.65f, 0.94f);      // Crisp Minecraft sky blue
        glm::vec3 duskSky(0.92f, 0.44f, 0.18f);     // Vibrant warm sunset
        glm::vec3 nightSky(0.012f, 0.016f, 0.035f); // Deep dark night sky

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
            skyColor = glm::vec3(0.02f, 0.16f, 0.44f); // Deep oceanic blue matching Image 3
        }
        float fogDistance = isUnderwater ? 24.0f : (static_cast<float>(world->renderDistance * 16) * 1.02f);

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
                        if (cmd == "/time set day" || cmd == "/time set 1000") {
                            timeOfDay = 0.05f;
                            chatFeedback = "Set the time to 1000 (Day)";
                        } else if (cmd == "/time set noon" || cmd == "/time set 6000") {
                            timeOfDay = 0.0f;
                            chatFeedback = "Set the time to 6000 (Noon)";
                        } else if (cmd == "/time set sunset" || cmd == "/time set dusk" || cmd == "/time set 12000") {
                            timeOfDay = 0.23f;
                            chatFeedback = "Set the time to 12000 (Sunset)";
                        } else if (cmd == "/time set night" || cmd == "/time set 13000") {
                            timeOfDay = 0.35f;
                            chatFeedback = "Set the time to 13000 (Night)";
                        } else if (cmd == "/time set midnight" || cmd == "/time set 18000") {
                            timeOfDay = 0.50f;
                            chatFeedback = "Set the time to 18000 (Midnight)";
                        } else if (cmd == "/time set sunrise" || cmd == "/time set dawn" || cmd == "/time set 0") {
                            timeOfDay = 0.78f;
                            chatFeedback = "Set the time to 0 (Sunrise)";
                        } else if (cmd == "/gamemode creative" || cmd == "/gamemode c" || cmd == "/gamemode 1") {
                            player.setFlying(true);
                            chatFeedback = "Set game mode to Creative Mode";
                        } else if (cmd == "/gamemode survival" || cmd == "/gamemode s" || cmd == "/gamemode 0") {
                            player.setFlying(false);
                            if (player.getCamera().getMode() == CameraMode::FreeCam) player.getCamera().toggleFreeCam();
                            chatFeedback = "Set game mode to Survival Mode";
                        } else if (cmd == "/gamemode spectator" || cmd == "/gamemode sp" || cmd == "/gamemode 3") {
                            if (player.getCamera().getMode() != CameraMode::FreeCam) player.getCamera().toggleFreeCam();
                            chatFeedback = "Set game mode to Spectator Mode";
                        } else if (cmd == "/clear") {
                            player.clearInventory();
                            chatFeedback = "Cleared the inventory";
                        } else if (cmd == "/help") {
                            chatFeedback = "Commands: /time set <day|night|noon>, /gamemode <c|s|sp>, /clear";
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

                player.handleInput(dt);
                player.update(dt, *world);
                world->update(player.getPosition());
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

                // Left click ALWAYS triggers punch/hit animation (air or block)
                if (Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
                    player.triggerSwing();
                }

                // Mining / Breaking Blocks (with suited tool speed advantages & crack stages 0-9)
                if (Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) && targetHit.has_value()) {
                    const CellCoord& hit = targetHit->hitCell;

                    if (!currentMiningCell.has_value() ||
                        currentMiningCell->x != hit.x || currentMiningCell->y != hit.y ||
                        currentMiningCell->z != hit.z || currentMiningCell->s != hit.s) {
                        currentMiningCell = hit;
                        miningTimer = 0.0f;
                    }

                    miningTimer += dt;
                    Cell targetCell = world->getCell(hit.x, hit.y, hit.z, hit.s);
                    float breakDuration = calculateBreakDuration(targetCell.type, player.getSelectedBlock());
                    crackStage = std::clamp(static_cast<int>((miningTimer / breakDuration) * 10.0f), 0, 9);

                    if (miningTimer >= breakDuration) {
                        Cell broken = world->getCell(hit.x, hit.y, hit.z, hit.s);
                        world->setCellInstant(hit.x, hit.y, hit.z, hit.s, Cell{BlockType::Air});
                        AudioEngine::get().playBlockDig(static_cast<int>(broken.type), 0.9f);

                        // Spawn miniature 3D drop entity
                        glm::vec3 dropPos = cellToWorldCenter(hit.x, hit.y, hit.z, hit.s);
                        itemDropManager.spawnDrop(dropPos, broken.type);

                        // Trigger gravity physics for Sand and Gravel blocks above the broken block
                        world->checkGravity(hit.x, hit.y + 1, hit.z, hit.s, &fallingBlockManager);

                        miningTimer = 0.0f;
                        currentMiningCell = std::nullopt;
                        crackStage = -1;
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
                        if (targetC.type == BlockType::CraftingTable) canBlock = false;
                    }
                    player.setBlocking(canBlock);
                } else {
                    player.setBlocking(false);
                }

                // Right Click Handling (Crafting Table interaction or Block/Flower/Torch Placement)
                if (heldItem != BlockType::ItemBow && !player.isBlocking() && Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) && targetHit.has_value()) {
                    CellCoord hitCell = targetHit->hitCell;
                    Cell hitBlock = world->getCell(hitCell.x, hitCell.y, hitCell.z, hitCell.s);

                    // 1. If right-clicked a Crafting Table block, open 3x3 Crafting Table GUI!
                    if (hitBlock.type == BlockType::CraftingTable) {
                        state = GameState::CraftingTable;
                        window.setCursorMode(false);
                        AudioEngine::get().playSound(SoundEffect::Click);
                    } else {
                        CellCoord place = targetHit->placeCell;
                        BlockType b = player.getSelectedBlock();
                        if (b != BlockType::Air && !Cell{b}.isItem()) {
                            // Foliage placement restriction (must be placed on grass or dirt)
                            bool canPlace = true;
                            if (b == BlockType::FlowerRose || b == BlockType::FlowerDandelion || b == BlockType::TallGrass) {
                                Cell below = world->getCell(place.x, place.y - 1, place.z, place.s);
                                if (below.type != BlockType::Grass && below.type != BlockType::Dirt) {
                                    canPlace = false;
                                }
                            }

                            if (canPlace) {
                                world->setCellInstant(place.x, place.y, place.z, place.s, Cell{b});
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
        } else if (state == GameState::MainMenu) {
            if (Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options);
                if (action == 1) { // Play / Singleplayer
                    state = GameState::LoadingWorld;
                    loadingProgress = 0.0f;
                    loadingLoadedChunks = 0;
                    loadingTotalChunks = 81;
                    window.setCursorMode(false);
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 2) { // Open World Creation
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 5) { // Quit
                    AudioEngine::get().playSound(SoundEffect::Click);
                    break;
                } else if (action == 6) { // Options
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
            }
        } else if (state == GameState::WorldCreation) {
            if (Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                int action = menuRenderer.handleClick(state, player, uiMousePos, uiW, uiH, currentSeed, options);
                if (action == 3) { // Generate & Start World
                    world = std::make_unique<World>(context, commandQueue, currentSeed);
                    world->renderDistance = options.renderDistance;
                    world->lodPreset = options.lodPreset;
                    state = GameState::LoadingWorld;
                    loadingProgress = 0.0f;
                    loadingLoadedChunks = 0;
                    loadingTotalChunks = 81;
                    window.setCursorMode(false);
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 4) { // Back to Main Menu
                    state = GameState::MainMenu;
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
                spawnY = world->getHighestSolidY(0.5f, 0.5f);
                player.respawn(glm::vec3(0.5f, spawnY + 2.5f, 0.5f));
                state = GameState::Playing;
                window.setCursorMode(true);
                lastLoggedLoaded = -1;
                std::cout << "[Loading] World loaded! Entering gameplay at spawn Y=" << spawnY << std::endl;
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
                } else if (action == 4) { // Title screen
                    state = GameState::MainMenu;
                    window.setCursorMode(false);
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (action == 5) { // Quit
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
                if (action >= 7 && action <= 25) {
                    player.getCamera().fov = static_cast<float>(options.fov);
                    player.mouseSensitivity = options.mouseSens;
                    AudioEngine::get().setMasterVolume(options.audioVolume);
                    world->renderDistance = options.renderDistance;
                    world->lodPreset = options.lodPreset;
                    if (swapchain.isVSyncEnabled() != options.vsync) {
                        swapchain.setVSync(options.vsync, window.getWidth(), window.getHeight());
                    }
                    if (action == 13 || action == 14) {
                        static const int resList[4][2] = {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}};
                        int targetW = resList[options.resIndex][0];
                        int targetH = resList[options.resIndex][1];
                        window.setWindowMode(options.windowMode, targetW, targetH, window.getRefreshRate());
                        swapchain.recreate(window.getWidth(), window.getHeight());
                    }
                    ConfigManager::save(options, exeDir + "options.txt");
                } else if (action == 4) {
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
        } else if (state == GameState::Inventory) {
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
            continue;
        }

        VkCommandBuffer cmd = commandQueue.getCommandBuffer();

        float aspect = static_cast<float>(swapchain.getExtent().width) / static_cast<float>(swapchain.getExtent().height);

        glm::mat4 view, proj, vp;
        glm::vec3 camPos;
        Frustum frustum;
        if (state == GameState::MainMenu) {
            float camRadius = 45.0f;
            camPos = glm::vec3(std::cos(menuCamAngle) * camRadius, 85.0f, std::sin(menuCamAngle) * camRadius);
            view = glm::lookAt(camPos, glm::vec3(0.0f, 65.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            proj = glm::perspective(glm::radians(75.0f), aspect, 0.1f, 500.0f);
            frustum = player.getCamera().getFrustum(aspect);
        } else {
            camPos = player.getCamera().getRenderPosition();
            view = player.getCamera().getViewMatrix();
            proj = player.getCamera().getProjectionMatrix(aspect);
            frustum = player.getCamera().getFrustum(aspect);
        }
        vp = proj * view;

        PushConstants pc{};
        std::memcpy(pc.mvp, &vp[0][0], sizeof(float) * 16);

        // Sun & Moon Directional Light: Moon takes over when sun dips below horizon
        glm::vec3 activeLightDir = sunDir;
        float activeIntensity = sunIntensity;
        if (sunDir.y <= 0.0f) {
            activeLightDir = -sunDir; // Moon is on opposite celestial pole!
            activeIntensity = 0.22f;   // Soft silver moonlight intensity for authentic dark nights
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

        // Point Light 1: Nearest active dropped torch (flickering dynamic light & shadow caster)
        auto droppedTorch = itemDropManager.getNearestTorchDrop(pPos, 22.0f);
        if (droppedTorch.has_value()) {
            pc.pointLight1[0] = droppedTorch->first.x;
            pc.pointLight1[1] = droppedTorch->first.y;
            pc.pointLight1[2] = droppedTorch->first.z;
            pc.pointLight1[3] = droppedTorch->second;
        } else {
            pc.pointLight1[0] = 0.0f; pc.pointLight1[1] = 0.0f; pc.pointLight1[2] = 0.0f; pc.pointLight1[3] = 0.0f;
        }

        // Point Light 2: Nearest placed world torch
        auto placedTorch = world->getNearestPlacedTorch(pPos, 12.0f);
        if (placedTorch.has_value()) {
            pc.pointLight2[0] = placedTorch->x;
            pc.pointLight2[1] = placedTorch->y;
            pc.pointLight2[2] = placedTorch->z;
            pc.pointLight2[3] = 1.0f;
        } else {
            pc.pointLight2[0] = 0.0f; pc.pointLight2[1] = 0.0f; pc.pointLight2[2] = 0.0f; pc.pointLight2[3] = 0.0f;
        }

        // Point Light 3: Exact Handheld Torch in player's right hand
        if (hasHeldTorch) {
            glm::vec3 fwd = player.getCamera().getForward();
            glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 torchWorldPos;
            if (player.getCamera().getMode() == CameraMode::FirstPerson) {
                torchWorldPos = camPos + fwd * 0.42f + right * 0.32f - glm::vec3(0.0f, 0.20f, 0.0f);
            } else {
                torchWorldPos = pPos + glm::vec3(0.0f, 1.15f, 0.0f) + fwd * 0.38f + right * 0.34f;
            }
            pc.heldTorch[0] = torchWorldPos.x;
            pc.heldTorch[1] = torchWorldPos.y;
            pc.heldTorch[2] = torchWorldPos.z;
            pc.heldTorch[3] = 1.0f;
        } else {
            pc.heldTorch[0] = 0.0f; pc.heldTorch[1] = 0.0f; pc.heldTorch[2] = 0.0f; pc.heldTorch[3] = 0.0f;
        }

        // =============================================================
        // Unified Forward Dynamic Rendering Pass
        // =============================================================
        swapchain.transitionToColorAttachment(cmd, imageIndex);

        // Clear swapchain color attachment with dynamic sky/fog color
        VkRenderingAttachmentInfo colorAtt{};
        colorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAtt.imageView = swapchain.getImageView(imageIndex);
        colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.clearValue.color = {{ skyColor.r, skyColor.g, skyColor.b, 1.0f }};

        VkRenderingAttachmentInfo depthAtt{};
        depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAtt.imageView = swapchain.getDepthImageView();
        depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.renderArea = {{ 0, 0 }, swapchain.getExtent()};
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAtt;
        renderInfo.pDepthAttachment = &depthAtt;

        vkCmdBeginRendering(cmd, &renderInfo);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = static_cast<float>(swapchain.getExtent().height);
        viewport.width = static_cast<float>(swapchain.getExtent().width);
        viewport.height = -static_cast<float>(swapchain.getExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{{ 0, 0 }, swapchain.getExtent()};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Only render 3D scene when Playing or in game menus (Pause, Inventory, CraftingTable, Death)
        bool render3D = (state == GameState::Playing || state == GameState::Paused ||
                         state == GameState::Inventory || state == GameState::CraftingTable ||
                         state == GameState::Death);

        if (render3D) {
            // Bind World Pipeline & Atlas Descriptor Set
            worldPipeline.bind(cmd);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    worldPipeline.getLayout(), 0, 1,
                                    &descSet, 0, nullptr);

            // 1. Render Triangular Sun & Moon
            celestialRenderer.render(cmd, worldPipeline, camPos, sunDir, vp);

            // 2. Render Opaque World Chunks
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
                crackRenderer.render(cmd, worldPipeline, currentMiningCell, crackStage, vp);
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
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, outlineInvertPipeline.getLayout(), 0, 1, &descSet, 0, nullptr);
                outlineRenderer.render(cmd, outlineInvertPipeline, targetHit->hitCell, vp);
                worldPipeline.bind(cmd);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline.getLayout(), 0, 1, &descSet, 0, nullptr);
            }

            // Render First-Person Hand or Third-Person Triangular Player Character Model
            if (state == GameState::Playing || state == GameState::Paused || state == GameState::Inventory || state == GameState::CraftingTable) {
                if (player.getCamera().getMode() == CameraMode::FirstPerson) {
                    handRenderer.render(cmd, worldPipeline, player, aspect);
                } else {
                    playerModelRenderer.render(cmd, worldPipeline, player, vp, pc);
                }
            }

            // 3. Render Translucent Water Chunks
            waterPipeline.bind(cmd);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, waterPipeline.getLayout(), 0, 1, &descSet, 0, nullptr);

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

            // 4. Render 3D Drifting Volumetric Clouds
            if (options.clouds && !isUnderwater) {
                cloudPipeline.bind(cmd);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cloudPipeline.getLayout(), 0, 1, &descSet, 0, nullptr);
                cloudRenderer.render(cmd, cloudPipeline, camPos, timer.getElapsedTime(), vp, skyColor, sunDir);
            }
        }

        // 5. Render 2D UI Overlay (HUD, Crosshair, Menus, Loading Screen)
        uiPipeline.bind(cmd);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline.getLayout(), 0, 1, &descSet, 0, nullptr);

        if (state == GameState::Playing) {
            uiRenderer.render(cmd, uiPipeline, &invertPipeline, player, uiW, uiH, displayedFPS, options, world.get(),
                              isChatOpen, chatInput, chatFeedback, chatFeedbackTimer);
        }

        if (state != GameState::Playing) {
            menuRenderer.render(cmd, uiPipeline, state, player, uiW, uiH, uiMousePos, options, loadingProgress, loadingLoadedChunks, loadingTotalChunks);
        }

        vkCmdEndRendering(cmd);

        // Transition to Present & Swap Buffers
        swapchain.transitionToPresent(cmd, imageIndex);

        if (!commandQueue.endFrame(swapchain, imageIndex)) {
            swapchain.recreate(window.getWidth(), window.getHeight());
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

    context.waitIdle();
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
