#include "MenuRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "player/Player.hpp"
#include "world/ChunkMesher.hpp"
#include "renderer/TextureAtlas.hpp"
#include "renderer/ItemDropRenderer.hpp"
#include "audio/AudioEngine.hpp"
#include "ui/FontData.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

namespace prismcraft {

MenuRenderer::MenuRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
}

struct InventoryLayout {
    float panW = 440.0f;
    float panH = 470.0f;
    float px, py;
    float S = 68.0f;
    float H;
    float gap = 3.5f;
    float stepX;
    float totalGridW;
    float startX;

    float hotbarY;
    float storageTopY;
    float rowGap = 3.0f;

    float craftMidY;
    float craftStartX;

    float arrowX, arrowY;
    float resX, resS, resH;

    InventoryLayout(float screenW, float screenH) {
        float cx = screenW * 0.5f;
        float cy = screenH * 0.5f;
        px = cx - panW * 0.5f;
        py = cy - panH * 0.5f;
        H = S * 0.8660254f;
        stepX = (S + gap) * 0.5f;
        totalGridW = 9.0f * stepX + S;
        startX = px + (panW - totalGridW) * 0.5f;

        hotbarY = py + panH - H - 18.0f;
        float storageBottomY = hotbarY - 14.0f;
        storageTopY = storageBottomY - (3.0f * H + 2.0f * rowGap);

        craftStartX = startX + 12.0f;
        craftMidY = py + 26.0f + H * 0.8f;

        arrowX = craftStartX + 2.0f * stepX + S + 22.0f;
        arrowY = craftMidY;

        resX = arrowX + 38.0f;
        resS = S * 1.15f;
        resH = resS * 0.8660254f;
    }

    void getHotbarTri(int i, glm::vec2& v0, glm::vec2& v1, glm::vec2& v2, bool& isUp) const {
        float sx = startX + i * stepX;
        isUp = (i % 2 == 0);
        if (isUp) {
            v0 = {sx + S * 0.5f, hotbarY};
            v1 = {sx, hotbarY + H};
            v2 = {sx + S, hotbarY + H};
        } else {
            v0 = {sx, hotbarY};
            v1 = {sx + S, hotbarY};
            v2 = {sx + S * 0.5f, hotbarY + H};
        }
    }

    void getStorageTri(int r, int c, glm::vec2& v0, glm::vec2& v1, glm::vec2& v2, bool& isUp) const {
        float rowY = storageTopY + r * (H + rowGap);
        float sx = startX + c * stepX;
        if (r % 2 == 0) {
            isUp = (c % 2 == 0);
        } else {
            isUp = (c % 2 != 0);
        }
        if (isUp) {
            v0 = {sx + S * 0.5f, rowY};
            v1 = {sx, rowY + H};
            v2 = {sx + S, rowY + H};
        } else {
            v0 = {sx, rowY};
            v1 = {sx + S, rowY};
            v2 = {sx + S * 0.5f, rowY + H};
        }
    }

    void getCraftingTri(int idx, glm::vec2& v0, glm::vec2& v1, glm::vec2& v2, bool& isUp) const {
        if (idx == 0) {
            isUp = true;
            v0 = {craftStartX + S * 0.5f, craftMidY - H * 0.8f};
            v1 = {craftStartX, craftMidY};
            v2 = {craftStartX + S, craftMidY};
        } else if (idx == 1) {
            isUp = false;
            float sx = craftStartX + stepX;
            v0 = {sx, craftMidY - H * 0.8f};
            v1 = {sx + S, craftMidY - H * 0.8f};
            v2 = {sx + S * 0.5f, craftMidY};
        } else if (idx == 2) {
            isUp = false;
            v0 = {craftStartX, craftMidY};
            v1 = {craftStartX + S, craftMidY};
            v2 = {craftStartX + S * 0.5f, craftMidY + H * 0.8f};
        } else {
            isUp = true;
            float sx = craftStartX + stepX;
            v0 = {sx + S * 0.5f, craftMidY};
            v1 = {sx, craftMidY + H * 0.8f};
            v2 = {sx + S, craftMidY + H * 0.8f};
        }
    }

    void getCraftingTableTri(int idx, glm::vec2& v0, glm::vec2& v1, glm::vec2& v2, bool& isUp) const {
        int r = idx / 3;
        int c = idx % 3;
        float rowY = py + 22.0f + r * (H * 0.72f);
        float sx = craftStartX + c * stepX;
        isUp = ((r + c) % 2 == 0);
        if (isUp) {
            v0 = {sx + S * 0.5f, rowY};
            v1 = {sx, rowY + H * 0.75f};
            v2 = {sx + S, rowY + H * 0.75f};
        } else {
            v0 = {sx, rowY};
            v1 = {sx + S, rowY};
            v2 = {sx + S * 0.5f, rowY + H * 0.75f};
        }
    }

    void getResultTri(glm::vec2& v0, glm::vec2& v1, glm::vec2& v2) const {
        v0 = {resX + resS * 0.5f, craftMidY - resH * 0.5f};
        v1 = {resX, craftMidY + resH * 0.5f};
        v2 = {resX + resS, craftMidY + resH * 0.5f};
    }
};

int MenuRenderer::handleClick(GameState& state, Player& player, glm::vec2 mousePos, uint32_t screenWidth, uint32_t screenHeight, uint32_t& worldSeed, GameOptions& options, bool isDown, bool isRightClick, ItemDropManager* itemDrops) {
    m_currentSeed = worldSeed;
    float cx = screenWidth * 0.5f;
    float cy = screenHeight * 0.5f;
    float btnW = 380.0f;
    float btnH = 48.0f;
    float x = cx - btnW * 0.5f;

    auto checkRightTri = [&](float y) {
        glm::vec2 v0(x, y);
        glm::vec2 v1(x, y + btnH);
        glm::vec2 v2(x + btnW, y + btnH * 0.5f);
        return pointInTriangle(mousePos, v0, v1, v2);
    };

    auto checkLeftTri = [&](float y) {
        glm::vec2 v0(x + btnW, y);
        glm::vec2 v1(x + btnW, y + btnH);
        glm::vec2 v2(x, y + btnH * 0.5f);
        return pointInTriangle(mousePos, v0, v1, v2);
    };

    if (state == GameState::MainMenu) {
        // [▶ PLAY WORLD] (y = cy - 40)
        if (checkRightTri(cy - 40.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Playing;
            return 1;
        }
        // [▶ CREATE NEW WORLD] (y = cy + 20)
        if (checkRightTri(cy + 20.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::WorldCreation;
            return 2;
        }
        // [▶ SETTINGS & OPTIONS] (y = cy + 80)
        if (checkRightTri(cy + 80.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            m_previousState = GameState::MainMenu;
            state = GameState::Options;
            return 6;
        }
        // [◀ QUIT GAME] (y = cy + 140)
        if (checkLeftTri(cy + 140.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            return 5;
        }
    } else if (state == GameState::WorldCreation) {
        // [▶ RANDOMIZE SEED] (y = cy - 25)
        if (checkRightTri(cy - 25.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            worldSeed = (worldSeed * 1664525u + 1013904223u) % 1000000u;
            m_currentSeed = worldSeed;
            return 0;
        }
        // [▶ BIOMES: PRISMATIC EXPANSION] (y = cy + 30)
        if (checkRightTri(cy + 30.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            worldSeed = (worldSeed + 777u) % 1000000u;
            m_currentSeed = worldSeed;
            return 0;
        }
        // [▶ LAUNCH WORLD] (y = cy + 85)
        if (checkRightTri(cy + 85.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Playing;
            return 3;
        }
        // [◀ BACK TO TITLE] (y = cy + 140)
        if (checkLeftTri(cy + 140.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::MainMenu;
            return 4;
        }
    } else if (state == GameState::Paused) {
        // [▶ RESUME GAME] (y = cy - 50)
        if (checkRightTri(cy - 50.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Playing;
            return 1;
        }
        // [▶ SETTINGS & OPTIONS] (y = cy + 10)
        if (checkRightTri(cy + 10.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            m_previousState = GameState::Paused;
            state = GameState::Options;
            return 6;
        }
        // [◀ SAVE & QUIT TO TITLE] (y = cy + 70)
        if (checkLeftTri(cy + 70.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::MainMenu;
            return 4;
        }
    } else if (state == GameState::Options) {
        float bW = 260.0f;
        float bH = 40.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 110.0f;
        float dy = 50.0f;

        auto inBox = [&](float bx, float by, float bw, float bh) {
            return (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
        };

        // Row 0: FOV (slider, left) | DEBUG HUD F3 (button, right)
        if (inBox(lx, y0, bW, bH)) {
            float t = std::clamp((mousePos.x - lx) / bW, 0.0f, 1.0f);
            options.fov = static_cast<int>(std::round(60.0f + t * 50.0f));
            return 7;
        }
        if (inBox(rx, y0, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.debugHUD = !options.debugHUD;
            return 23;
        }

        // Row 1: VIDEO SETTINGS... (left) | MUSIC & SOUNDS... (right)
        if (inBox(lx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::VideoSettings;
            return 6;
        }
        if (inBox(rx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::AudioSettings;
            return 6;
        }

        // Row 2: CONTROLS & KEYBINDS... (left) | LIGHT OVERLAY F7 (right)
        if (inBox(lx, y0 + dy * 2.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::ControlsSettings;
            return 6;
        }
        if (inBox(rx, y0 + dy * 2.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.lightOverlay = !options.lightOverlay;
            return 20;
        }

        // Row 3: DONE / BACK (center, width 320, height 40)
        float doneW = 320.0f;
        float doneH = 42.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 3.5f;
        if (inBox(doneX, doneY, doneW, doneH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = m_previousState;
            return 4;
        }
    } else if (state == GameState::VideoSettings) {
        float bW = 260.0f;
        float bH = 36.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 190.0f;
        float dy = 40.0f;

        auto inBox = [&](float bx, float by, float bw, float bh) {
            return (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
        };

        // Row 0: VIBRANT SHADERS (toggle, left) | SHADOW QUALITY (toggle, right)
        if (inBox(lx, y0, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.vibrantVisuals = !options.vibrantVisuals;
            return 19;
        }
        if (inBox(rx, y0, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.shadowQuality = (options.shadowQuality + 1) % 4;
            return 24;
        }

        // Row 1: RENDER DISTANCE (toggle, left) | MAX FPS (toggle, right)
        if (inBox(lx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            static const int rdList[] = {4, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256};
            int curIdx = 3;
            for (int k = 0; k < 12; ++k) { if (rdList[k] == options.renderDistance) { curIdx = k; break; } }
            options.renderDistance = rdList[(curIdx + 1) % 12];
            return 17;
        }
        if (inBox(rx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            static const int fpsList[] = {0, 30, 60, 90, 120, 144, 240};
            int curIdx = 0;
            for (int k = 0; k < 7; ++k) { if (fpsList[k] == options.maxFps) { curIdx = k; break; } }
            options.maxFps = fpsList[(curIdx + 1) % 7];
            return 15;
        }

        // Row 2: WATER & SSR (toggle, left) | COLOR GRADING (toggle, right)
        if (inBox(lx, y0 + dy * 2.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.waterQuality = (options.waterQuality + 1) % 3;
            return 25;
        }
        if (inBox(rx, y0 + dy * 2.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.colorGrading = (options.colorGrading + 1) % 5;
            return 26;
        }

        // Row 3: CLOUDS 3D (toggle, left) | CLOUD SHADOWS (toggle, right)
        if (inBox(lx, y0 + dy * 3.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.clouds = !options.clouds;
            options.cloudShadows = options.clouds;
            return 18;
        }
        if (inBox(rx, y0 + dy * 3.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.cloudShadows = !options.cloudShadows;
            return 27;
        }

        // Row 4: SMOOTH LIGHTING (toggle, left) | STEVE SHADOW (toggle, right)
        if (inBox(lx, y0 + dy * 4.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.smoothLighting = !options.smoothLighting;
            return 29;
        }
        if (inBox(rx, y0 + dy * 4.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.playerShadow = !options.playerShadow;
            return 28;
        }

        // Row 5: ATMOSPHERE FOG (toggle, left) | LOD PRESET (toggle, right)
        if (inBox(lx, y0 + dy * 5.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.atmosphericFog = (options.atmosphericFog + 1) % 3;
            return 30;
        }
        if (inBox(rx, y0 + dy * 5.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.lodPreset = (options.lodPreset + 1) % 4;
            return 21;
        }

        // Row 6: VSYNC (toggle, left) | UI SCALE (toggle, right)
        if (inBox(lx, y0 + dy * 6.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.vsync = !options.vsync;
            return 16;
        }
        if (inBox(rx, y0 + dy * 6.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            if (options.uiScale < 1.25f) options.uiScale = 1.5f;
            else if (options.uiScale < 1.75f) options.uiScale = 2.0f;
            else if (options.uiScale < 2.25f) options.uiScale = 2.5f;
            else if (options.uiScale < 2.75f) options.uiScale = 3.0f;
            else options.uiScale = 1.0f;
            return 12;
        }

        // Row 7: RESOLUTION (toggle, left) | WINDOW MODE (toggle, right)
        if (inBox(lx, y0 + dy * 7.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.resIndex = (options.resIndex + 1) % 4;
            return 14;
        }
        if (inBox(rx, y0 + dy * 7.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.windowMode = (options.windowMode + 1) % 3;
            return 13;
        }

        // Row 8: DONE / BACK (center)
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 8.0f + 10.0f;
        if (inBox(doneX, doneY, doneW, doneH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Options;
            return 6;
        }
    } else if (state == GameState::ControlsSettings) {
        float bW = 260.0f;
        float bH = 40.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 160.0f;

        auto inBox = [&](float bx, float by, float bw, float bh) {
            return (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
        };

        // Mouse Sensitivity Slider
        if (inBox(lx, y0, bW, bH)) {
            float t = std::clamp((mousePos.x - lx) / bW, 0.0f, 1.0f);
            options.mouseSens = 0.2f + t * 2.8f;
            return 8;
        }
        if (inBox(rx, y0, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            return 8;
        }

        // Done button
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = cy + 160.0f;
        if (inBox(doneX, doneY, doneW, doneH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Options;
            return 6;
        }
    } else if (state == GameState::AudioSettings) {
        float bW = 260.0f;
        float bH = 38.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 110.0f;
        float dy = 48.0f;

        auto inBox = [&](float bx, float by, float bw, float bh) {
            return (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
        };

        // Master Volume
        if (inBox(lx, y0, bW, bH)) {
            float t = std::clamp((mousePos.x - lx) / bW, 0.0f, 1.0f);
            options.masterVolume = t;
            options.audioVolume = t;
            AudioEngine::get().setMasterVolume(t);
            return 9;
        }
        // Music Volume
        if (inBox(rx, y0, bW, bH)) {
            float t = std::clamp((mousePos.x - rx) / bW, 0.0f, 1.0f);
            options.musicVolume = t;
            AudioEngine::get().setMusicVolume(t);
            return 9;
        }
        // Blocks Volume
        if (inBox(lx, y0 + dy, bW, bH)) {
            float t = std::clamp((mousePos.x - lx) / bW, 0.0f, 1.0f);
            options.blocksVolume = t;
            AudioEngine::get().setBlocksVolume(t);
            return 9;
        }
        // Player Volume
        if (inBox(rx, y0 + dy, bW, bH)) {
            float t = std::clamp((mousePos.x - rx) / bW, 0.0f, 1.0f);
            options.playerVolume = t;
            AudioEngine::get().setPlayerVolume(t);
            return 9;
        }
        // Ambient Volume
        if (inBox(lx, y0 + dy * 2.0f, bW, bH)) {
            float t = std::clamp((mousePos.x - lx) / bW, 0.0f, 1.0f);
            options.ambientVolume = t;
            AudioEngine::get().setAmbientVolume(t);
            return 9;
        }

        // Done button
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 3.5f;
        if (inBox(doneX, doneY, doneW, doneH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Options;
            return 6;
        }
    } else if (state == GameState::Inventory || state == GameState::CraftingTable) {
        if (isDown) return 0;
        InventoryLayout layout(static_cast<float>(screenWidth), static_cast<float>(screenHeight));

        auto handleTransfer = [&](BlockType& slotItem, int& slotCount) {
            if (isRightClick) {
                if (m_cursorItem == BlockType::Air && slotItem != BlockType::Air) {
                    int take = (slotCount + 1) / 2;
                    m_cursorItem = slotItem;
                    m_cursorCount = take;
                    slotCount -= take;
                    if (slotCount <= 0) slotItem = BlockType::Air;
                    AudioEngine::get().playSound(SoundEffect::Click);
                } else if (m_cursorItem != BlockType::Air && (slotItem == BlockType::Air || slotItem == m_cursorItem)) {
                    if (slotItem == BlockType::Air) {
                        slotItem = m_cursorItem;
                        slotCount = 1;
                    } else if (slotCount < 64) {
                        slotCount++;
                    }
                    m_cursorCount--;
                    if (m_cursorCount <= 0) m_cursorItem = BlockType::Air;
                    AudioEngine::get().playSound(SoundEffect::Click);
                }
            } else {
                if (m_cursorItem == slotItem && slotItem != BlockType::Air) {
                    int space = 64 - slotCount;
                    int amount = std::min(space, m_cursorCount);
                    slotCount += amount;
                    m_cursorCount -= amount;
                    if (m_cursorCount <= 0) m_cursorItem = BlockType::Air;
                } else {
                    std::swap(m_cursorItem, slotItem);
                    std::swap(m_cursorCount, slotCount);
                }
                AudioEngine::get().playSound(SoundEffect::Click);
            }
        };

        if (state == GameState::CraftingTable) {
            // 3x3 Result Slot
            glm::vec2 rv0, rv1, rv2;
            layout.getResultTri(rv0, rv1, rv2);
            if (pointInTriangle(mousePos, rv0, rv1, rv2)) {
                CraftingResult res = CraftingSystem::craft3x3(m_craftingTableGrid);
                if (res.item != BlockType::Air) {
                    if (m_cursorItem == BlockType::Air) {
                        m_cursorItem = res.item;
                        m_cursorCount = res.count;
                    } else if (m_cursorItem == res.item && m_cursorCount + res.count <= 64) {
                        m_cursorCount += res.count;
                    } else {
                        return 0;
                    }
                    for (int k = 0; k < 9; ++k) {
                        if (m_craftingTableGrid[k] != BlockType::Air) {
                            m_craftingTableCounts[k]--;
                            if (m_craftingTableCounts[k] <= 0) {
                                m_craftingTableGrid[k] = BlockType::Air;
                                m_craftingTableCounts[k] = 0;
                            }
                        }
                    }
                    AudioEngine::get().playSound(SoundEffect::ItemPop);
                    return 0;
                }
            }

            // 3x3 Crafting Grid (9 slots)
            for (int k = 0; k < 9; ++k) {
                glm::vec2 cv0, cv1, cv2; bool isUp;
                layout.getCraftingTableTri(k, cv0, cv1, cv2, isUp);
                if (pointInTriangle(mousePos, cv0, cv1, cv2)) {
                    handleTransfer(m_craftingTableGrid[k], m_craftingTableCounts[k]);
                    return 0;
                }
            }
        } else {
            // 2x2 Result Slot
            glm::vec2 rv0, rv1, rv2;
            layout.getResultTri(rv0, rv1, rv2);
            if (pointInTriangle(mousePos, rv0, rv1, rv2)) {
                CraftingResult res = CraftingSystem::craft2x2(m_craftingGrid);
                if (res.item != BlockType::Air) {
                    if (m_cursorItem == BlockType::Air) {
                        m_cursorItem = res.item;
                        m_cursorCount = res.count;
                    } else if (m_cursorItem == res.item && m_cursorCount + res.count <= 64) {
                        m_cursorCount += res.count;
                    } else {
                        return 0;
                    }
                    for (int k = 0; k < 4; ++k) {
                        if (m_craftingGrid[k] != BlockType::Air) {
                            m_craftingCounts[k]--;
                            if (m_craftingCounts[k] <= 0) {
                                m_craftingGrid[k] = BlockType::Air;
                                m_craftingCounts[k] = 0;
                            }
                        }
                    }
                    AudioEngine::get().playSound(SoundEffect::ItemPop);
                    return 0;
                }
            }

            // 2x2 Crafting Grid (4 slots)
            for (int k = 0; k < 4; ++k) {
                glm::vec2 cv0, cv1, cv2; bool isUp;
                layout.getCraftingTri(k, cv0, cv1, cv2, isUp);
                if (pointInTriangle(mousePos, cv0, cv1, cv2)) {
                    handleTransfer(m_craftingGrid[k], m_craftingCounts[k]);
                    return 0;
                }
            }
        }

        // 3. Storage Slots (30 slots: 3 rows x 10)
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 10; ++c) {
                int slotIdx = r * 10 + c;
                glm::vec2 sv0, sv1, sv2; bool isUp;
                layout.getStorageTri(r, c, sv0, sv1, sv2, isUp);
                if (pointInTriangle(mousePos, sv0, sv1, sv2)) {
                    BlockType curSlotBlock = player.getStorageBlock(slotIdx);
                    int curSlotCount = player.getStorageCount(slotIdx);
                    handleTransfer(curSlotBlock, curSlotCount);
                    player.setStorageSlot(slotIdx, curSlotBlock, curSlotCount);
                    return 0;
                }
            }
        }

        // 4. Hotbar Slots (10 slots)
        for (int i = 0; i < 10; ++i) {
            glm::vec2 hv0, hv1, hv2; bool isUp;
            layout.getHotbarTri(i, hv0, hv1, hv2, isUp);
            if (pointInTriangle(mousePos, hv0, hv1, hv2)) {
                BlockType curHotBlock = player.getHotbar()[i];
                int curHotCount = player.getHotbarCounts()[i];
                handleTransfer(curHotBlock, curHotCount);
                player.setHotbarBlock(i, curHotBlock, curHotCount);
                return 0;
            }
        }

        // 5. Click outside inventory dialog to drop held cursor item into world
        if (m_cursorItem != BlockType::Air && m_cursorCount > 0) {
            bool insidePanel = (mousePos.x >= layout.px && mousePos.x <= layout.px + layout.panW &&
                                mousePos.y >= layout.py && mousePos.y <= layout.py + layout.panH);
            if (!insidePanel) {
                int dropCount = isRightClick ? 1 : m_cursorCount;
                BlockType dropType = m_cursorItem;
                m_cursorCount -= dropCount;
                if (m_cursorCount <= 0) {
                    m_cursorItem = BlockType::Air;
                    m_cursorCount = 0;
                }
                if (itemDrops) {
                    glm::vec3 eye = player.getPosition() + glm::vec3(0.0f, 1.62f, 0.0f);
                    glm::vec3 fwd = player.getCamera().getForward();
                    glm::vec3 dropPos = eye + fwd * 0.8f;
                    glm::vec3 dropVel = fwd * 4.0f + glm::vec3(0.0f, 1.2f, 0.0f);
                    for (int k = 0; k < dropCount; ++k) {
                        itemDrops->spawnDrop(dropPos, dropType, dropVel);
                    }
                    AudioEngine::get().playSound(SoundEffect::ItemPop);
                }
                return 0;
            }
        }
    } else if (state == GameState::Death) {
        // [▶ RESPAWN] (y = cy + 20)
        if (checkRightTri(cy + 20.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            return 11;
        }
        // [◀ TITLE MENU] (y = cy + 85)
        if (checkLeftTri(cy + 85.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::MainMenu;
            return 4;
        }
    }

    return 0;
}

void MenuRenderer::rebuildMenuMesh(GameState state, const Player& player, uint32_t screenWidth, uint32_t screenHeight, glm::vec2 mousePos, const GameOptions& options, float loadingProgress, int loadedChunks, int totalChunks) {
    (void)player;

    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    auto addTexturedQuad = [&](float x, float y, float w, float h, const glm::vec4& uv, const glm::vec3& color) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 norm(0.0f, 0.0f, 1.0f);
        vertices.push_back({glm::vec3(x, y + h, 0.0f), glm::vec2(uv.x, uv.w), norm, color});
        vertices.push_back({glm::vec3(x + w, y + h, 0.0f), glm::vec2(uv.z, uv.w), norm, color});
        vertices.push_back({glm::vec3(x + w, y, 0.0f), glm::vec2(uv.z, uv.y), norm, color});
        vertices.push_back({glm::vec3(x, y, 0.0f), glm::vec2(uv.x, uv.y), norm, color});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    auto addSolidQuad = [&](float x, float y, float w, float h, const glm::vec3& color) {
        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);
        addTexturedQuad(x, y, w, h, uv, color);
    };

    auto addTintQuad = [&](float x, float y, float w, float h, const glm::vec3& color, float alpha) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);
        glm::vec3 norm(0.0f, 0.0f, alpha);
        vertices.push_back({glm::vec3(x, y + h, 0.0f), glm::vec2(uv.x, uv.w), norm, color});
        vertices.push_back({glm::vec3(x + w, y + h, 0.0f), glm::vec2(uv.z, uv.w), norm, color});
        vertices.push_back({glm::vec3(x + w, y, 0.0f), glm::vec2(uv.z, uv.y), norm, color});
        vertices.push_back({glm::vec3(x, y, 0.0f), glm::vec2(uv.x, uv.y), norm, color});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    auto addTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& color) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);
        glm::vec3 norm(0.0f, 0.0f, 1.0f);
        vertices.push_back({v0, glm::vec2(uv.x, uv.y), norm, color});
        vertices.push_back({v1, glm::vec2(uv.x, uv.w), norm, color});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, color});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    auto addThickLine = [&](glm::vec2 p1, glm::vec2 p2, float thickness, const glm::vec3& color) {
        glm::vec2 d = p2 - p1;
        float len = glm::length(d);
        if (len < 0.0001f) return;
        glm::vec2 n = glm::vec2(-d.y, d.x) * (thickness * 0.5f / len);

        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);
        glm::vec3 norm(0.0f, 0.0f, 1.0f);

        vertices.push_back({glm::vec3(p1 - n, 0.0f), glm::vec2(uv.x, uv.w), norm, color});
        vertices.push_back({glm::vec3(p1 + n, 0.0f), glm::vec2(uv.z, uv.w), norm, color});
        vertices.push_back({glm::vec3(p2 + n, 0.0f), glm::vec2(uv.z, uv.y), norm, color});
        vertices.push_back({glm::vec3(p2 - n, 0.0f), glm::vec2(uv.x, uv.y), norm, color});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    // Iconic PrismCraft Triangular Prism Button with 3D Bevels & Minecraft Pixel Font
    auto addPrismButton = [&](glm::vec2 v0, glm::vec2 v1, glm::vec2 v2,
                             const std::string& label, bool hovered,
                             bool pointRight = true, float fontSize = 16.0f) {
        // Base fill
        glm::vec3 baseCol = hovered ? glm::vec3(0.38f, 0.35f, 0.20f) : glm::vec3(0.24f, 0.25f, 0.28f);
        addTri(glm::vec3(v0, 0.0f), glm::vec3(v1, 0.0f), glm::vec3(v2, 0.0f), baseCol);

        // 3D Bevel borders
        glm::vec3 lightEdge = hovered ? glm::vec3(1.0f, 0.95f, 0.45f) : glm::vec3(0.60f, 0.62f, 0.68f);
        glm::vec3 darkEdge  = hovered ? glm::vec3(0.70f, 0.55f, 0.12f) : glm::vec3(0.10f, 0.11f, 0.13f);
        glm::vec3 baseEdge  = hovered ? glm::vec3(0.90f, 0.80f, 0.28f) : glm::vec3(0.45f, 0.47f, 0.52f);
        float edgeThick = hovered ? 3.5f : 2.5f;

        if (pointRight) {
            // v0: top-left, v1: bottom-left, v2: right apex
            addThickLine(v0, v1, edgeThick, baseEdge);
            addThickLine(v0, v2, edgeThick, lightEdge);
            addThickLine(v1, v2, edgeThick, darkEdge);

            // Glowing golden triangle pip
            float pipX = v0.x + 22.0f;
            float pipY = (v0.y + v1.y) * 0.5f;
            float pipS = 6.0f;
            glm::vec3 pipCol = hovered ? glm::vec3(1.0f, 0.92f, 0.25f) : glm::vec3(0.70f, 0.72f, 0.78f);
            addTri(glm::vec3(pipX + pipS, pipY, 0.0f),
                   glm::vec3(pipX - pipS * 0.5f, pipY - pipS, 0.0f),
                   glm::vec3(pipX - pipS * 0.5f, pipY + pipS, 0.0f), pipCol);

            // Pixelated button label
            float textX = v0.x + 40.0f;
            float textY = (v0.y + v1.y - fontSize) * 0.5f;
            glm::vec3 textCol = hovered ? glm::vec3(1.0f, 1.0f, 0.40f) : glm::vec3(0.92f, 0.92f, 0.92f);
            FontRenderer::drawText(vertices, indices, label, textX, textY, fontSize, textCol, true);
        } else {
            // v0: top-right, v1: bottom-right, v2: left apex
            addThickLine(v0, v1, edgeThick, baseEdge);
            addThickLine(v2, v0, edgeThick, lightEdge);
            addThickLine(v2, v1, edgeThick, darkEdge);

            // Pixelated button label aligned inside the wide right base
            float textW = FontRenderer::getTextWidth(label, fontSize);
            float textX = v0.x - textW - 35.0f;
            float textY = (v0.y + v1.y - fontSize) * 0.5f;
            glm::vec3 textCol = hovered ? glm::vec3(1.0f, 1.0f, 0.40f) : glm::vec3(0.92f, 0.92f, 0.92f);
            FontRenderer::drawText(vertices, indices, label, textX, textY, fontSize, textCol, true);

            // Glowing golden triangle pip pointing left to the left of the text
            float pipX = textX - 16.0f;
            float pipY = (v0.y + v1.y) * 0.5f;
            float pipS = 6.0f;
            glm::vec3 pipCol = hovered ? glm::vec3(1.0f, 0.92f, 0.25f) : glm::vec3(0.70f, 0.72f, 0.78f);
            addTri(glm::vec3(pipX - pipS, pipY, 0.0f),
                   glm::vec3(pipX + pipS * 0.5f, pipY + pipS, 0.0f),
                   glm::vec3(pipX + pipS * 0.5f, pipY - pipS, 0.0f), pipCol);
        }
    };

    // Sleek Interactive Slider with Triangular Handle
    auto addSlider = [&](float x, float y, float w, float h, float ratio, const std::string& label, bool hovered) {
        // Sunken dark track well
        addSolidQuad(x, y, w, h, glm::vec3(0.12f, 0.12f, 0.15f));

        // Beveled 3D frame
        glm::vec3 frameLight = hovered ? glm::vec3(0.65f, 0.65f, 0.70f) : glm::vec3(0.35f, 0.35f, 0.40f);
        glm::vec3 frameDark  = hovered ? glm::vec3(0.18f, 0.18f, 0.22f) : glm::vec3(0.08f, 0.08f, 0.10f);
        addSolidQuad(x, y, w, 2.5f, frameLight);
        addSolidQuad(x, y, 2.5f, h, frameLight);
        addSolidQuad(x, y + h - 2.5f, w, 2.5f, frameDark);
        addSolidQuad(x + w - 2.5f, y, 2.5f, h, frameDark);

        // Filled progress portion (from 0 to ratio)
        float fillW = std::clamp(ratio, 0.0f, 1.0f) * (w - 4.0f);
        if (fillW > 1.0f) {
            glm::vec3 fillCol = hovered ? glm::vec3(0.28f, 0.52f, 0.75f) : glm::vec3(0.20f, 0.38f, 0.58f);
            addSolidQuad(x + 2.0f, y + 2.0f, fillW, h - 4.0f, fillCol);
            addSolidQuad(x + 2.0f, y + 2.0f, fillW, 2.0f, fillCol * 1.3f);
        }

        // Triangular Slider Handle / Knob
        float knobX = x + 2.0f + fillW;
        float kw = 6.0f;
        glm::vec3 knobCol = hovered ? glm::vec3(1.0f, 0.95f, 0.40f) : glm::vec3(0.90f, 0.80f, 0.25f);

        // Top triangular apex ▲
        addTri(glm::vec3(knobX, y - 5.0f, 0.0f),
               glm::vec3(knobX - 7.0f, y + 4.0f, 0.0f),
               glm::vec3(knobX + 7.0f, y + 4.0f, 0.0f), knobCol);
        // Bottom triangular apex ▼
        addTri(glm::vec3(knobX, y + h + 5.0f, 0.0f),
               glm::vec3(knobX - 7.0f, y + h - 4.0f, 0.0f),
               glm::vec3(knobX + 7.0f, y + h - 4.0f, 0.0f), knobCol);
        // Vertical slider stem
        addSolidQuad(knobX - kw * 0.5f, y, kw, h, knobCol);

        // Centered label text in Minecraft pixel font
        float textW = FontRenderer::getTextWidth(label, 15.0f);
        glm::vec3 textCol = hovered ? glm::vec3(1.0f, 1.0f, 0.50f) : glm::vec3(1.0f, 1.0f, 1.0f);
        FontRenderer::drawText(vertices, indices, label, x + (w - textW) * 0.5f, y + (h - 15.0f) * 0.5f, 15.0f, textCol, true);
    };

    // Sleek Rectangular Toggle Button with 3D Bevel & PrismCraft Triangular Accents
    auto addRectButton = [&](float x, float y, float w, float h, const std::string& label, bool hovered, bool active) {
        glm::vec3 bgCol = hovered ? glm::vec3(0.24f, 0.25f, 0.30f) : (active ? glm::vec3(0.16f, 0.18f, 0.24f) : glm::vec3(0.12f, 0.12f, 0.15f));
        addSolidQuad(x, y, w, h, bgCol);

        glm::vec3 frameLight = hovered ? glm::vec3(0.85f, 0.85f, 0.95f) : (active ? glm::vec3(0.40f, 0.55f, 0.70f) : glm::vec3(0.30f, 0.30f, 0.35f));
        glm::vec3 frameDark  = hovered ? glm::vec3(0.18f, 0.18f, 0.22f) : glm::vec3(0.08f, 0.08f, 0.10f);
        addSolidQuad(x, y, w, 2.0f, frameLight);
        addSolidQuad(x, y, 2.0f, h, frameLight);
        addSolidQuad(x, y + h - 2.0f, w, 2.0f, frameDark);
        addSolidQuad(x + w - 2.0f, y, 2.0f, h, frameDark);

        // Triangular Chevron Accents (▶ on left, ◀ on right)
        glm::vec3 triAccent = hovered ? glm::vec3(1.0f, 0.88f, 0.30f) : (active ? glm::vec3(0.50f, 0.75f, 0.95f) : glm::vec3(0.35f, 0.38f, 0.45f));
        float triS = 4.5f;
        float midY = y + h * 0.5f;

        // Left triangle ▶
        addTri(glm::vec3(x + 13.0f, midY, 0.0f),
               glm::vec3(x + 7.0f, midY - triS, 0.0f),
               glm::vec3(x + 7.0f, midY + triS, 0.0f), triAccent);

        // Right triangle ◀
        addTri(glm::vec3(x + w - 13.0f, midY, 0.0f),
               glm::vec3(x + w - 7.0f, midY + triS, 0.0f),
               glm::vec3(x + w - 7.0f, midY - triS, 0.0f), triAccent);

        float textW = FontRenderer::getTextWidth(label, 13.5f);
        glm::vec3 textCol = hovered ? glm::vec3(1.0f, 1.0f, 0.40f) : (active ? glm::vec3(0.95f, 0.95f, 0.95f) : glm::vec3(0.60f, 0.60f, 0.60f));
        FontRenderer::drawText(vertices, indices, label, x + (w - textW) * 0.5f, y + (h - 13.5f) * 0.5f, 13.5f, textCol, true);
    };

    float sw = static_cast<float>(screenWidth);
    float sh = static_cast<float>(screenHeight);
    float cx = sw * 0.5f;
    float cy = sh * 0.5f;
    float btnW = 380.0f;
    float btnH = 48.0f;
    float bx = cx - btnW * 0.5f;

    if (state == GameState::MainMenu) {
        // Tiled Dirt/Stone Background
        glm::vec4 dirtUV = TextureAtlas::getTileUV(2);
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.40f));
            }
        }

        // Giant Minecraft-Style "PRISMCRAFT" Title Emblem
        float logoW = 520.0f;
        float logoH = 90.0f;
        addSolidQuad(cx - logoW * 0.5f, cy - 200.0f, logoW, logoH, glm::vec3(0.12f, 0.12f, 0.16f));
        addSolidQuad(cx - logoW * 0.5f, cy - 200.0f, logoW, 4.0f, glm::vec3(0.95f, 0.75f, 0.20f));
        addSolidQuad(cx - logoW * 0.5f, cy - 114.0f, logoW, 4.0f, glm::vec3(0.95f, 0.75f, 0.20f));

        // Decorative Diamond Prism Icons
        glm::vec4 diamondUV = TextureAtlas::getTileUV(16);
        addTexturedQuad(cx - 235.0f, cy - 180.0f, 44.0f, 44.0f, diamondUV, glm::vec3(1.0f));
        addTexturedQuad(cx + 191.0f, cy - 180.0f, 44.0f, 44.0f, diamondUV, glm::vec3(1.0f));

        // Center Gold Delta
        addTri(glm::vec3(cx, cy - 195.0f, 0.0f), glm::vec3(cx - 30.0f, cy - 145.0f, 0.0f), glm::vec3(cx + 30.0f, cy - 145.0f, 0.0f), glm::vec3(0.95f, 0.80f, 0.25f));
        addTri(glm::vec3(cx, cy - 187.0f, 0.0f), glm::vec3(cx - 20.0f, cy - 151.0f, 0.0f), glm::vec3(cx + 20.0f, cy - 151.0f, 0.0f), glm::vec3(0.12f, 0.12f, 0.16f));

        // Title text in bold golden Minecraft pixel font
        std::string titleStr = "PRISMCRAFT";
        float titleW = FontRenderer::getTextWidth(titleStr, 34.0f);
        FontRenderer::drawText(vertices, indices, titleStr, cx - titleW * 0.5f, cy - 188.0f, 34.0f, glm::vec3(1.0f, 0.85f, 0.15f), true);

        // Subtitle in cyan pixel font
        std::string subStr = "VOXEL TRIANGLES EDITION";
        float subW = FontRenderer::getTextWidth(subStr, 13.0f);
        FontRenderer::drawText(vertices, indices, subStr, cx - subW * 0.5f, cy - 142.0f, 13.0f, glm::vec3(0.45f, 0.90f, 1.0f), true);

        // Menu Triangular Buttons:
        // 1. PLAY WORLD
        glm::vec2 p1_0(bx, cy - 40.0f), p1_1(bx, cy - 40.0f + btnH), p1_2(bx + btnW, cy - 40.0f + btnH * 0.5f);
        bool h1 = pointInTriangle(mousePos, p1_0, p1_1, p1_2);
        addPrismButton(p1_0, p1_1, p1_2, "PLAY WORLD", h1, true, 16.0f);

        // 2. CREATE NEW WORLD
        glm::vec2 p2_0(bx, cy + 20.0f), p2_1(bx, cy + 20.0f + btnH), p2_2(bx + btnW, cy + 20.0f + btnH * 0.5f);
        bool h2 = pointInTriangle(mousePos, p2_0, p2_1, p2_2);
        addPrismButton(p2_0, p2_1, p2_2, "CREATE NEW WORLD", h2, true, 16.0f);

        // 3. SETTINGS & OPTIONS
        glm::vec2 p3_0(bx, cy + 80.0f), p3_1(bx, cy + 80.0f + btnH), p3_2(bx + btnW, cy + 80.0f + btnH * 0.5f);
        bool h3 = pointInTriangle(mousePos, p3_0, p3_1, p3_2);
        addPrismButton(p3_0, p3_1, p3_2, "SETTINGS & OPTIONS", h3, true, 16.0f);

        // 4. QUIT GAME (Points Left)
        glm::vec2 p4_0(bx + btnW, cy + 140.0f), p4_1(bx + btnW, cy + 140.0f + btnH), p4_2(bx, cy + 140.0f + btnH * 0.5f);
        bool h4 = pointInTriangle(mousePos, p4_0, p4_1, p4_2);
        addPrismButton(p4_0, p4_1, p4_2, "QUIT GAME", h4, false, 16.0f);

        // Footer copyright & version
        FontRenderer::drawText(vertices, indices, "PrismCraft v1.0 [Vulkan 1.3]", 12.0f, sh - 22.0f, 12.0f, glm::vec3(0.70f, 0.70f, 0.70f), true);
        std::string rightTag = "Pure Voxel Triangular Honeycomb Engine";
        float tagW = FontRenderer::getTextWidth(rightTag, 12.0f);
        FontRenderer::drawText(vertices, indices, rightTag, sw - tagW - 12.0f, sh - 22.0f, 12.0f, glm::vec3(0.55f, 0.55f, 0.55f), true);

    } else if (state == GameState::LoadingWorld) {
        // Tiled Dirt Background (Classic Minecraft Style)
        glm::vec4 dirtUV = TextureAtlas::getTileUV(2);
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.35f));
            }
        }

        // Giant Minecraft-Style "PRISMCRAFT" Title Emblem
        float logoW = 500.0f;
        float logoH = 80.0f;
        addSolidQuad(cx - logoW * 0.5f, cy - 180.0f, logoW, logoH, glm::vec3(0.12f, 0.12f, 0.16f));
        addSolidQuad(cx - logoW * 0.5f, cy - 180.0f, logoW, 4.0f, glm::vec3(0.95f, 0.75f, 0.20f));
        addSolidQuad(cx - logoW * 0.5f, cy - 104.0f, logoW, 4.0f, glm::vec3(0.95f, 0.75f, 0.20f));

        // Decorative Diamond Prism Icons
        glm::vec4 diamondUV = TextureAtlas::getTileUV(16);
        addTexturedQuad(cx - 225.0f, cy - 165.0f, 40.0f, 40.0f, diamondUV, glm::vec3(1.0f));
        addTexturedQuad(cx + 185.0f, cy - 165.0f, 40.0f, 40.0f, diamondUV, glm::vec3(1.0f));

        // Title text in bold golden Minecraft pixel font
        std::string titleStr = "PRISMCRAFT";
        float titleW = FontRenderer::getTextWidth(titleStr, 32.0f);
        FontRenderer::drawText(vertices, indices, titleStr, cx - titleW * 0.5f, cy - 170.0f, 32.0f, glm::vec3(1.0f, 0.85f, 0.15f), true);

        // Subtitle in cyan pixel font
        std::string subStr = "ENTERING PRISM REALM...";
        float subW = FontRenderer::getTextWidth(subStr, 13.0f);
        FontRenderer::drawText(vertices, indices, subStr, cx - subW * 0.5f, cy - 128.0f, 13.0f, glm::vec3(0.45f, 0.90f, 1.0f), true);

        // Dynamic Loading Stage Message
        std::string statusStr = "Generating terrain...";
        if (loadingProgress >= 0.85f) {
            statusStr = "Uploading spawn chunks to GPU...";
        } else if (loadingProgress >= 0.55f) {
            statusStr = "Meshing honeycomb geometry & foliage...";
        } else if (loadingProgress >= 0.25f) {
            statusStr = "Carving caverns & seeding biomes...";
        }
        float stW = FontRenderer::getTextWidth(statusStr, 16.0f);
        FontRenderer::drawText(vertices, indices, statusStr, cx - stW * 0.5f, cy - 25.0f, 16.0f, glm::vec3(0.95f, 0.95f, 0.95f), true);

        // Sleek Beveled Progress Bar
        float barW = 460.0f;
        float barH = 32.0f;
        float barX = cx - barW * 0.5f;
        float barY = cy + 10.0f;

        // Dark Sunken Well
        addSolidQuad(barX, barY, barW, barH, glm::vec3(0.08f, 0.08f, 0.10f));
        // Beveled Frame
        glm::vec3 fLight(0.40f, 0.40f, 0.45f);
        glm::vec3 fDark(0.05f, 0.05f, 0.06f);
        addSolidQuad(barX, barY, barW, 2.5f, fLight);
        addSolidQuad(barX, barY, 2.5f, barH, fLight);
        addSolidQuad(barX, barY + barH - 2.5f, barW, 2.5f, fDark);
        addSolidQuad(barX + barW - 2.5f, barY, 2.5f, barH, fDark);

        // Animated Golden/Amber Progress Fill
        float pct = std::clamp(loadingProgress, 0.0f, 1.0f);
        float fillW = pct * (barW - 6.0f);
        if (fillW > 2.0f) {
            addSolidQuad(barX + 3.0f, barY + 3.0f, fillW, barH - 6.0f, glm::vec3(0.85f, 0.68f, 0.15f));
            // Highlight shine strip on top half
            addSolidQuad(barX + 3.0f, barY + 3.0f, fillW, (barH - 6.0f) * 0.45f, glm::vec3(1.0f, 0.88f, 0.35f));
        }

        // Percentage & Chunk Counts inside progress bar
        char pctBuf[64];
        if (totalChunks > 0) {
            std::snprintf(pctBuf, sizeof(pctBuf), "%d%% (%d / %d CHUNKS)", static_cast<int>(pct * 100.0f), loadedChunks, totalChunks);
        } else {
            std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(pct * 100.0f));
        }
        std::string pctStr(pctBuf);
        float pctW = FontRenderer::getTextWidth(pctStr, 14.0f);
        FontRenderer::drawText(vertices, indices, pctStr, cx - pctW * 0.5f, barY + 9.0f, 14.0f, glm::vec3(1.0f), true);

        // Bottom Lore / Hints
        std::string tipStr = "Tip: Equilateral and right prisms join seamlessly on a 60-degree honeycomb grid!";
        float tipW = FontRenderer::getTextWidth(tipStr, 12.0f);
        FontRenderer::drawText(vertices, indices, tipStr, cx - tipW * 0.5f, sh - 35.0f, 12.0f, glm::vec3(0.65f, 0.65f, 0.70f), true);

    } else if (state == GameState::Options) {
        // Darkened tiled background
        glm::vec4 dirtUV = TextureAtlas::getTileUV(2);
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.25f));
            }
        }

        // Title Box "SETTINGS & OPTIONS"
        float titleBoxW = 460.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 180.0f, titleBoxW, 40.0f, glm::vec3(0.14f, 0.14f, 0.18f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 180.0f, titleBoxW, 3.0f, glm::vec3(0.95f, 0.75f, 0.20f));
        std::string optTitle = "SETTINGS & OPTIONS";
        float optW = FontRenderer::getTextWidth(optTitle, 22.0f);
        FontRenderer::drawText(vertices, indices, optTitle, cx - optW * 0.5f, cy - 171.0f, 22.0f, glm::vec3(1.0f, 0.85f, 0.20f), true);

        float bW = 260.0f;
        float bH = 40.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 110.0f;
        float dy = 50.0f;

        auto inBox = [&](float bx_, float by_, float bw_, float bh_) {
            return (mousePos.x >= bx_ && mousePos.x <= bx_ + bw_ && mousePos.y >= by_ && mousePos.y <= by_ + bh_);
        };

        // Row 0: FOV slider (left) | DEBUG HUD F3 (right)
        float fovRatio = std::clamp((static_cast<float>(options.fov) - 60.0f) / 50.0f, 0.0f, 1.0f);
        char fovBuf[64];
        std::snprintf(fovBuf, sizeof(fovBuf), "FOV: %d DEG", options.fov);
        addSlider(lx, y0, bW, bH, fovRatio, fovBuf, inBox(lx, y0, bW, bH));

        std::string debugStr = options.debugHUD ? "DEBUG HUD [F3]: ON" : "DEBUG HUD [F3]: OFF";
        addRectButton(rx, y0, bW, bH, debugStr, inBox(rx, y0, bW, bH), options.debugHUD);

        // Row 1: VIDEO SETTINGS... (left) | MUSIC & SOUNDS... (right)
        addRectButton(lx, y0 + dy, bW, bH, "VIDEO SETTINGS...", inBox(lx, y0 + dy, bW, bH), true);
        addRectButton(rx, y0 + dy, bW, bH, "MUSIC & SOUNDS...", inBox(rx, y0 + dy, bW, bH), true);

        // Row 2: CONTROLS & KEYBINDS... (left) | LIGHT OVERLAY F7 (right)
        addRectButton(lx, y0 + dy * 2.0f, bW, bH, "CONTROLS & KEYBINDS...", inBox(lx, y0 + dy * 2.0f, bW, bH), true);
        std::string lightStr = options.lightOverlay ? "LIGHT OVERLAY [F7]: ON" : "LIGHT OVERLAY [F7]: OFF";
        addRectButton(rx, y0 + dy * 2.0f, bW, bH, lightStr, inBox(rx, y0 + dy * 2.0f, bW, bH), options.lightOverlay);

        // Row 3: BACK / DONE (center)
        float doneW = 320.0f;
        float doneH = 42.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 3.5f;
        addRectButton(doneX, doneY, doneW, doneH, "DONE / BACK", inBox(doneX, doneY, doneW, doneH), true);

        // Footer info
        std::string infoStr = "PrismCraft Engine v1.3 | Triangular Prism Honeycomb Architecture";
        float infoW = FontRenderer::getTextWidth(infoStr, 12.0f);
        FontRenderer::drawText(vertices, indices, infoStr, cx - infoW * 0.5f, sh - 25.0f, 12.0f, glm::vec3(0.65f, 0.65f, 0.70f), true);

    } else if (state == GameState::VideoSettings) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(2);
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.24f));
            }
        }

        // Header "VIDEO & SHADER PACK SETTINGS"
        float titleBoxW = 460.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 235.0f, titleBoxW, 40.0f, glm::vec3(0.16f, 0.16f, 0.20f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 235.0f, titleBoxW, 2.5f, glm::vec3(0.40f, 0.65f, 0.95f));
        std::string title = "VIDEO & SHADER PACK SETTINGS";
        float titleW = FontRenderer::getTextWidth(title, 18.0f);
        FontRenderer::drawText(vertices, indices, title, cx - titleW * 0.5f, cy - 224.0f, 18.0f, glm::vec3(1.0f, 0.90f, 0.35f), true);

        float bW = 260.0f;
        float bH = 36.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 190.0f;
        float dy = 40.0f;

        auto inBox = [&](float bx_, float by_, float bw_, float bh_) {
            return (mousePos.x >= bx_ && mousePos.x <= bx_ + bw_ && mousePos.y >= by_ && mousePos.y <= by_ + bh_);
        };

        // Row 0: VIBRANT SHADERS | SHADOW QUALITY
        std::string vibStr = options.vibrantVisuals ? "VIBRANT SHADERS: ON" : "VIBRANT SHADERS: OFF";
        addRectButton(lx, y0, bW, bH, vibStr, inBox(lx, y0, bW, bH), options.vibrantVisuals);

        const char* shadowNames[] = {"SHADOWS: OFF", "SHADOWS: LOW", "SHADOWS: MEDIUM", "SHADOWS: HIGH"};
        addRectButton(rx, y0, bW, bH, shadowNames[std::clamp(options.shadowQuality, 0, 3)], inBox(rx, y0, bW, bH), options.shadowQuality > 0);

        // Row 1: RENDER DISTANCE | MAX FPS
        char rdBuf[64];
        std::snprintf(rdBuf, sizeof(rdBuf), "RENDER DIST: %d CHUNKS", options.renderDistance);
        addRectButton(lx, y0 + dy, bW, bH, rdBuf, inBox(lx, y0 + dy, bW, bH), true);

        char fpsBuf[64];
        if (options.maxFps <= 0) std::snprintf(fpsBuf, sizeof(fpsBuf), "MAX FPS: UNLIMITED");
        else std::snprintf(fpsBuf, sizeof(fpsBuf), "MAX FPS: %d FPS", options.maxFps);
        addRectButton(rx, y0 + dy, bW, bH, fpsBuf, inBox(rx, y0 + dy, bW, bH), true);

        // Row 2: WATER & SSR | COLOR GRADING
        const char* waterNames[] = {"WATER: FAST", "WATER: REALISTIC", "WATER: RTX SSR"};
        addRectButton(lx, y0 + dy * 2.0f, bW, bH, waterNames[std::clamp(options.waterQuality, 0, 2)], inBox(lx, y0 + dy * 2.0f, bW, bH), true);

        const char* gradeNames[] = {"COLOR: DEFAULT", "COLOR: CINEMATIC", "COLOR: VIBRANT", "COLOR: WARM", "COLOR: COOL"};
        addRectButton(rx, y0 + dy * 2.0f, bW, bH, gradeNames[std::clamp(options.colorGrading, 0, 4)], inBox(rx, y0 + dy * 2.0f, bW, bH), options.colorGrading > 0);

        // Row 3: CLOUDS 3D | CLOUD SHADOWS
        std::string cloudStr = options.clouds ? "CLOUDS: VOLUMETRIC 3D" : "CLOUDS: OFF";
        addRectButton(lx, y0 + dy * 3.0f, bW, bH, cloudStr, inBox(lx, y0 + dy * 3.0f, bW, bH), options.clouds);

        std::string cShadowStr = options.cloudShadows ? "CLOUD SHADOWS: ON" : "CLOUD SHADOWS: OFF";
        addRectButton(rx, y0 + dy * 3.0f, bW, bH, cShadowStr, inBox(rx, y0 + dy * 3.0f, bW, bH), options.cloudShadows);

        // Row 4: SMOOTH LIGHTING / SSAO | STEVE SHADOW
        std::string aoStr = options.smoothLighting ? "SMOOTH LIGHT / SSAO: ON" : "SMOOTH LIGHT: OFF";
        addRectButton(lx, y0 + dy * 4.0f, bW, bH, aoStr, inBox(lx, y0 + dy * 4.0f, bW, bH), options.smoothLighting);

        std::string pShadowStr = options.playerShadow ? "STEVE SHADOW: ON" : "STEVE SHADOW: OFF";
        addRectButton(rx, y0 + dy * 4.0f, bW, bH, pShadowStr, inBox(rx, y0 + dy * 4.0f, bW, bH), options.playerShadow);

        // Row 5: ATMOSPHERE FOG | LOD PRESET
        const char* atmosNames[] = {"ATMOSPHERE: OFF", "ATMOSPHERE: SUBTLE", "ATMOSPHERE: DENSE"};
        addRectButton(lx, y0 + dy * 5.0f, bW, bH, atmosNames[std::clamp(options.atmosphericFog, 0, 2)], inBox(lx, y0 + dy * 5.0f, bW, bH), options.atmosphericFog > 0);

        const char* lodNames[] = {"LOD: PERFORMANCE", "LOD: BALANCED", "LOD: QUALITY", "LOD: ULTRA"};
        addRectButton(rx, y0 + dy * 5.0f, bW, bH, lodNames[std::clamp(options.lodPreset, 0, 3)], inBox(rx, y0 + dy * 5.0f, bW, bH), true);

        // Row 6: VSYNC | UI SCALE
        std::string vsyncStr = options.vsync ? "VSYNC: ENABLED" : "VSYNC: DISABLED";
        addRectButton(lx, y0 + dy * 6.0f, bW, bH, vsyncStr, inBox(lx, y0 + dy * 6.0f, bW, bH), options.vsync);

        char uiBuf[64];
        std::snprintf(uiBuf, sizeof(uiBuf), "UI SCALE: %.1fx", options.uiScale);
        addRectButton(rx, y0 + dy * 6.0f, bW, bH, uiBuf, inBox(rx, y0 + dy * 6.0f, bW, bH), true);

        // Row 7: RESOLUTION | WINDOW MODE
        const char* resNames[] = {"RES: 1280x720", "RES: 1600x900", "RES: 1920x1080", "RES: 2560x1440"};
        addRectButton(lx, y0 + dy * 7.0f, bW, bH, resNames[std::clamp(options.resIndex, 0, 3)], inBox(lx, y0 + dy * 7.0f, bW, bH), true);

        const char* winModes[] = {"MODE: WINDOWED", "MODE: BORDERLESS", "MODE: FULLSCREEN"};
        addRectButton(rx, y0 + dy * 7.0f, bW, bH, winModes[std::clamp(options.windowMode, 0, 2)], inBox(rx, y0 + dy * 7.0f, bW, bH), true);

        // Row 8: DONE / BACK (center)
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 8.0f + 10.0f;
        addRectButton(doneX, doneY, doneW, doneH, "DONE / BACK", inBox(doneX, doneY, doneW, doneH), true);

    } else if (state == GameState::AudioSettings) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(2);
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.24f));
            }
        }

        // Header "MUSIC & SOUND OPTIONS"
        float titleBoxW = 440.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 180.0f, titleBoxW, 44.0f, glm::vec3(0.16f, 0.16f, 0.20f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 180.0f, titleBoxW, 3.0f, glm::vec3(0.95f, 0.80f, 0.25f));
        std::string soundTitle = "MUSIC & SOUND OPTIONS";
        float stW = FontRenderer::getTextWidth(soundTitle, 24.0f);
        FontRenderer::drawText(vertices, indices, soundTitle, cx - stW * 0.5f, cy - 170.0f, 24.0f, glm::vec3(1.0f, 0.85f, 0.20f), true);

        float bW = 260.0f;
        float bH = 38.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 110.0f;
        float dy = 48.0f;

        auto inBox = [&](float bx, float by, float bw, float bh) {
            return (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
        };

        // Row 0: Master Volume | Music Volume
        char masterBuf[64];
        if (options.masterVolume <= 0.01f) std::snprintf(masterBuf, sizeof(masterBuf), "MASTER VOLUME: OFF");
        else std::snprintf(masterBuf, sizeof(masterBuf), "MASTER VOLUME: %d%%", static_cast<int>(options.masterVolume * 100.0f));
        addSlider(lx, y0, bW, bH, options.masterVolume, masterBuf, inBox(lx, y0, bW, bH));

        char musicBuf[64];
        if (options.musicVolume <= 0.01f) std::snprintf(musicBuf, sizeof(musicBuf), "MUSIC: OFF");
        else std::snprintf(musicBuf, sizeof(musicBuf), "MUSIC: %d%%", static_cast<int>(options.musicVolume * 100.0f));
        addSlider(rx, y0, bW, bH, options.musicVolume, musicBuf, inBox(rx, y0, bW, bH));

        // Row 1: Blocks Volume | Player Volume
        char blocksBuf[64];
        if (options.blocksVolume <= 0.01f) std::snprintf(blocksBuf, sizeof(blocksBuf), "BLOCKS: OFF");
        else std::snprintf(blocksBuf, sizeof(blocksBuf), "BLOCKS: %d%%", static_cast<int>(options.blocksVolume * 100.0f));
        addSlider(lx, y0 + dy, bW, bH, options.blocksVolume, blocksBuf, inBox(lx, y0 + dy, bW, bH));

        char playerBuf[64];
        if (options.playerVolume <= 0.01f) std::snprintf(playerBuf, sizeof(playerBuf), "PLAYERS: OFF");
        else std::snprintf(playerBuf, sizeof(playerBuf), "PLAYERS: %d%%", static_cast<int>(options.playerVolume * 100.0f));
        addSlider(rx, y0 + dy, bW, bH, options.playerVolume, playerBuf, inBox(rx, y0 + dy, bW, bH));

        // Row 2: Ambient / Environment Volume
        char ambientBuf[64];
        if (options.ambientVolume <= 0.01f) std::snprintf(ambientBuf, sizeof(ambientBuf), "AMBIENT / WEATHER: %d%%", static_cast<int>(options.ambientVolume * 100.0f));
        else std::snprintf(ambientBuf, sizeof(ambientBuf), "AMBIENT / WEATHER: %d%%", static_cast<int>(options.ambientVolume * 100.0f));
        addSlider(lx, y0 + dy * 2.0f, bW, bH, options.ambientVolume, ambientBuf, inBox(lx, y0 + dy * 2.0f, bW, bH));

        // Done button
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 3.5f;
        addRectButton(doneX, doneY, doneW, doneH, "DONE", inBox(doneX, doneY, doneW, doneH), true);

    } else if (state == GameState::ControlsSettings) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(2);
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.24f));
            }
        }

        // Header "CONTROLS & KEYBINDINGS"
        float titleBoxW = 460.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 220.0f, titleBoxW, 40.0f, glm::vec3(0.16f, 0.16f, 0.20f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 220.0f, titleBoxW, 3.0f, glm::vec3(0.95f, 0.75f, 0.20f));
        std::string ctrlTitle = "CONTROLS & KEYBINDINGS";
        float ctW = FontRenderer::getTextWidth(ctrlTitle, 20.0f);
        FontRenderer::drawText(vertices, indices, ctrlTitle, cx - ctW * 0.5f, cy - 210.0f, 20.0f, glm::vec3(1.0f, 0.85f, 0.20f), true);

        float bW = 260.0f;
        float bH = 40.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 160.0f;

        auto inBox = [&](float bx_, float by_, float bw_, float bh_) {
            return (mousePos.x >= bx_ && mousePos.x <= bx_ + bw_ && mousePos.y >= by_ && mousePos.y <= by_ + bh_);
        };

        // Mouse Sensitivity Slider
        float sensRatio = std::clamp((options.mouseSens - 0.2f) / 2.8f, 0.0f, 1.0f);
        char sensBuf[64];
        std::snprintf(sensBuf, sizeof(sensBuf), "MOUSE SENS: %.2fx", options.mouseSens);
        addSlider(lx, y0, bW, bH, sensRatio, sensBuf, inBox(lx, y0, bW, bH));

        addRectButton(rx, y0, bW, bH, "INVERT MOUSE: OFF", inBox(rx, y0, bW, bH), false);

        // Keybinding Reference Card
        float cardW = 550.0f;
        float cardH = 240.0f;
        float cardX = cx - cardW * 0.5f;
        float cardY = y0 + 55.0f;
        addSolidQuad(cardX, cardY, cardW, cardH, glm::vec3(0.10f, 0.10f, 0.13f));
        addSolidQuad(cardX, cardY, cardW, 2.0f, glm::vec3(0.40f, 0.45f, 0.55f));
        addSolidQuad(cardX, cardY, 2.0f, cardH, glm::vec3(0.40f, 0.45f, 0.55f));
        addSolidQuad(cardX, cardY + cardH - 2.0f, cardW, 2.0f, glm::vec3(0.06f, 0.06f, 0.08f));
        addSolidQuad(cardX + cardW - 2.0f, cardY, 2.0f, cardH, glm::vec3(0.06f, 0.06f, 0.08f));

        struct KeyRow { const char* key; const char* action; };
        static const KeyRow rows[] = {
            {"W / A / S / D", "Move Forward / Strafe Left / Back / Right"},
            {"SPACEBAR", "Jump / Ascend Water"},
            {"LEFT SHIFT / CTRL", "Sneak / Sprint"},
            {"LEFT MOUSE CLICK", "Attack / Break Prism Block"},
            {"RIGHT MOUSE CLICK", "Place Block / Use Item / Sword Block"},
            {"MIDDLE MOUSE CLICK", "Pick Targeted Block"},
            {"KEY [E] / [Q]", "Open Inventory / Drop Selected Item"},
            {"KEY [F3] / [F5] / [F7]", "Debug Overlay / Camera View / Light Levels"}
        };

        for (int i = 0; i < 8; ++i) {
            float ry = cardY + 12.0f + static_cast<float>(i) * 28.0f;
            // Golden triangular pip
            addTri(glm::vec3(cardX + 18.0f, ry + 7.0f, 0.0f),
                   glm::vec3(cardX + 10.0f, ry + 2.0f, 0.0f),
                   glm::vec3(cardX + 10.0f, ry + 12.0f, 0.0f),
                   glm::vec3(0.95f, 0.80f, 0.25f));

            FontRenderer::drawText(vertices, indices, rows[i].key, cardX + 24.0f, ry, 13.0f, glm::vec3(0.45f, 0.85f, 1.0f), true);
            FontRenderer::drawText(vertices, indices, rows[i].action, cardX + 205.0f, ry, 13.0f, glm::vec3(0.90f, 0.90f, 0.90f), true);
        }

        // Done button
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = cardY + cardH + 16.0f;
        addRectButton(doneX, doneY, doneW, doneH, "DONE / BACK", inBox(doneX, doneY, doneW, doneH), true);

    } else if (state == GameState::Paused) {
        // Authentic semi-transparent dark tint overlay (50% black, world remains clearly visible!)
        addTintQuad(0.0f, 0.0f, sw, sh, glm::vec3(0.0f), 0.50f);

        // Header "GAME PAUSED"
        float titleBoxW = 340.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 140.0f, titleBoxW, 44.0f, glm::vec3(0.16f, 0.16f, 0.20f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 140.0f, titleBoxW, 3.0f, glm::vec3(0.95f, 0.80f, 0.25f));
        std::string pauseTitle = "GAME PAUSED";
        float pauseW = FontRenderer::getTextWidth(pauseTitle, 24.0f);
        FontRenderer::drawText(vertices, indices, pauseTitle, cx - pauseW * 0.5f, cy - 130.0f, 24.0f, glm::vec3(1.0f, 0.85f, 0.20f), true);

        // 1. RESUME GAME
        glm::vec2 p1_0(bx, cy - 50.0f), p1_1(bx, cy - 50.0f + btnH), p1_2(bx + btnW, cy - 50.0f + btnH * 0.5f);
        bool h1 = pointInTriangle(mousePos, p1_0, p1_1, p1_2);
        addPrismButton(p1_0, p1_1, p1_2, "RESUME GAME", h1, true, 16.0f);

        // 2. SETTINGS & OPTIONS
        glm::vec2 p2_0(bx, cy + 10.0f), p2_1(bx, cy + 10.0f + btnH), p2_2(bx + btnW, cy + 10.0f + btnH * 0.5f);
        bool h2 = pointInTriangle(mousePos, p2_0, p2_1, p2_2);
        addPrismButton(p2_0, p2_1, p2_2, "SETTINGS & OPTIONS", h2, true, 16.0f);

        // 3. SAVE & QUIT TO TITLE (Points Left)
        glm::vec2 p3_0(bx + btnW, cy + 70.0f), p3_1(bx + btnW, cy + 70.0f + btnH), p3_2(bx, cy + 70.0f + btnH * 0.5f);
        bool h3 = pointInTriangle(mousePos, p3_0, p3_1, p3_2);
        addPrismButton(p3_0, p3_1, p3_2, "SAVE & QUIT TO TITLE", h3, false, 15.0f);

    } else if (state == GameState::WorldCreation) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(2);
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.35f));
            }
        }

        // Dialog Panel
        float titleBoxW = 420.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 160.0f, titleBoxW, 44.0f, glm::vec3(0.18f, 0.18f, 0.22f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 160.0f, titleBoxW, 3.0f, glm::vec3(0.95f, 0.80f, 0.25f));
        std::string genTitle = "CREATE NEW WORLD";
        float genW = FontRenderer::getTextWidth(genTitle, 24.0f);
        FontRenderer::drawText(vertices, indices, genTitle, cx - genW * 0.5f, cy - 150.0f, 24.0f, glm::vec3(1.0f, 0.85f, 0.20f), true);

        // Seed Display Box
        float seedBoxW = 380.0f;
        float seedBoxH = 40.0f;
        addSolidQuad(cx - seedBoxW * 0.5f, cy - 85.0f, seedBoxW, seedBoxH, glm::vec3(0.12f, 0.12f, 0.15f));
        addSolidQuad(cx - seedBoxW * 0.5f, cy - 85.0f, seedBoxW, 2.0f, glm::vec3(0.35f, 0.35f, 0.40f));
        char seedBuf[64];
        std::snprintf(seedBuf, sizeof(seedBuf), "Seed: [ %u ]", m_currentSeed);
        FontRenderer::drawText(vertices, indices, seedBuf, cx - 170.0f, cy - 73.0f, 15.0f, glm::vec3(0.40f, 1.0f, 0.40f), true);

        // 1. RANDOMIZE SEED
        glm::vec2 p1_0(bx, cy - 25.0f), p1_1(bx, cy - 25.0f + btnH), p1_2(bx + btnW, cy - 25.0f + btnH * 0.5f);
        bool h1 = pointInTriangle(mousePos, p1_0, p1_1, p1_2);
        addPrismButton(p1_0, p1_1, p1_2, "RANDOMIZE SEED", h1, true, 16.0f);

        // 2. BIOMES PRESET
        glm::vec2 p2_0(bx, cy + 30.0f), p2_1(bx, cy + 30.0f + btnH), p2_2(bx + btnW, cy + 30.0f + btnH * 0.5f);
        bool h2 = pointInTriangle(mousePos, p2_0, p2_1, p2_2);
        addPrismButton(p2_0, p2_1, p2_2, "BIOMES: PRISMATIC EXPANSION", h2, true, 15.0f);

        // 3. LAUNCH WORLD
        glm::vec2 p3_0(bx, cy + 85.0f), p3_1(bx, cy + 85.0f + btnH), p3_2(bx + btnW, cy + 85.0f + btnH * 0.5f);
        bool h3 = pointInTriangle(mousePos, p3_0, p3_1, p3_2);
        addPrismButton(p3_0, p3_1, p3_2, "LAUNCH NEW WORLD", h3, true, 16.0f);

        // 4. BACK TO TITLE (Points Left)
        glm::vec2 p4_0(bx + btnW, cy + 140.0f), p4_1(bx + btnW, cy + 140.0f + btnH), p4_2(bx, cy + 140.0f + btnH * 0.5f);
        bool h4 = pointInTriangle(mousePos, p4_0, p4_1, p4_2);
        addPrismButton(p4_0, p4_1, p4_2, "BACK TO TITLE", h4, false, 15.0f);

    } else if (state == GameState::Inventory || state == GameState::CraftingTable) {
        // Transparent Dark Tint overlay behind GUI dialog
        glm::vec4 darkTintUV = TextureAtlas::getTileUV(127);
        addTexturedQuad(0.0f, 0.0f, sw, sh, darkTintUV, glm::vec3(1.0f));

        InventoryLayout layout(sw, sh);
        glm::vec4 whiteUV = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);

        // Gray dialog base (Classic Minecraft Light Gray)
        addSolidQuad(layout.px, layout.py, layout.panW, layout.panH, glm::vec3(0.776f, 0.776f, 0.776f));
        // Beveled 3D borders (Classic Minecraft GUI style)
        addSolidQuad(layout.px, layout.py, layout.panW, 3.5f, glm::vec3(1.0f));
        addSolidQuad(layout.px, layout.py, 3.5f, layout.panH, glm::vec3(1.0f));
        addSolidQuad(layout.px, layout.py + layout.panH - 3.5f, layout.panW, 3.5f, glm::vec3(0.25f, 0.25f, 0.28f));
        addSolidQuad(layout.px + layout.panW - 3.5f, layout.py, 3.5f, layout.panH, glm::vec3(0.25f, 0.25f, 0.28f));

        // Header Title
        std::string invTitle = (state == GameState::CraftingTable) ? "3x3 CRAFTING TABLE" : "2x2 CRAFTING";
        FontRenderer::drawText(vertices, indices, invTitle, layout.px + 14.0f, layout.py + 8.0f, 11.0f, glm::vec3(0.25f, 0.25f, 0.28f), false);

        // 3D Isometric Triangular Prism & 2.5D Item Icon Renderer
        auto draw3DItemIcon = [&](float cx, float cy, float s, BlockType item) {
            if (item == BlockType::Air) return;

            if (Cell{item}.isItem() || Cell{item}.isFoliage() || Cell{item}.isTorch()) {
                glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(item, 0));
                float sz = s * 0.88f;
                addTexturedQuad(cx - sz * 0.5f, cy - sz * 0.5f, sz, sz, uv, glm::vec3(1.0f));
            } else {
                // Authentic 3D Isometric Triangular Prism Block
                glm::vec2 tRL(cx - s * 0.44f, cy - s * 0.22f);
                glm::vec2 tRR(cx + s * 0.44f, cy - s * 0.22f);
                glm::vec2 tFC(cx, cy + s * 0.12f);

                glm::vec4 uvTop = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(item, 0));
                uint32_t bTop = static_cast<uint32_t>(vertices.size());
                glm::vec3 norm(0.0f, 0.0f, 1.0f);
                glm::vec3 cTop(1.0f);
                vertices.push_back({glm::vec3(tRL, 0.0f), glm::vec2(uvTop.x, uvTop.w), norm, cTop});
                vertices.push_back({glm::vec3(tRR, 0.0f), glm::vec2(uvTop.z, uvTop.w), norm, cTop});
                vertices.push_back({glm::vec3(tFC, 0.0f), glm::vec2(uvTop.x + (uvTop.z - uvTop.x) * 0.5f, uvTop.y), norm, cTop});
                indices.push_back(bTop + 0); indices.push_back(bTop + 1); indices.push_back(bTop + 2);

                float depth = s * 0.38f;
                glm::vec2 bRL = tRL + glm::vec2(0.0f, depth);
                glm::vec2 bRR = tRR + glm::vec2(0.0f, depth);
                glm::vec2 bFC = tFC + glm::vec2(0.0f, depth);

                glm::vec4 uvSideL = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(item, 2));
                glm::vec3 cSideL(0.72f);
                uint32_t bL = static_cast<uint32_t>(vertices.size());
                vertices.push_back({glm::vec3(tRL, 0.0f), glm::vec2(uvSideL.x, uvSideL.y), norm, cSideL});
                vertices.push_back({glm::vec3(tFC, 0.0f), glm::vec2(uvSideL.z, uvSideL.y), norm, cSideL});
                vertices.push_back({glm::vec3(bFC, 0.0f), glm::vec2(uvSideL.z, uvSideL.w), norm, cSideL});
                vertices.push_back({glm::vec3(bRL, 0.0f), glm::vec2(uvSideL.x, uvSideL.w), norm, cSideL});
                indices.push_back(bL + 0); indices.push_back(bL + 1); indices.push_back(bL + 2);
                indices.push_back(bL + 0); indices.push_back(bL + 2); indices.push_back(bL + 3);

                glm::vec4 uvSideR = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(item, 3));
                glm::vec3 cSideR(0.55f);
                uint32_t bR = static_cast<uint32_t>(vertices.size());
                vertices.push_back({glm::vec3(tFC, 0.0f), glm::vec2(uvSideR.x, uvSideR.y), norm, cSideR});
                vertices.push_back({glm::vec3(tRR, 0.0f), glm::vec2(uvSideR.z, uvSideR.y), norm, cSideR});
                vertices.push_back({glm::vec3(bRR, 0.0f), glm::vec2(uvSideR.z, uvSideR.w), norm, cSideR});
                vertices.push_back({glm::vec3(bFC, 0.0f), glm::vec2(uvSideR.x, uvSideR.w), norm, cSideR});
                indices.push_back(bR + 0); indices.push_back(bR + 1); indices.push_back(bR + 2);
                indices.push_back(bR + 0); indices.push_back(bR + 2); indices.push_back(bR + 3);
            }
        };

        BlockType hoveredItem = BlockType::Air;

        auto drawTriSlot = [&](glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, bool isUp,
                               BlockType item, int count, bool isHovered, bool isSelected) {
            glm::vec3 wellCol = isSelected ? glm::vec3(0.32f, 0.32f, 0.36f) : (isHovered ? glm::vec3(0.66f, 0.66f, 0.70f) : glm::vec3(0.55f, 0.55f, 0.57f));
            addTri(glm::vec3(p0, 0.0f), glm::vec3(p1, 0.0f), glm::vec3(p2, 0.0f), wellCol);

            float bw = isSelected ? 3.0f : 2.0f;
            glm::vec3 edgeLight = isSelected ? glm::vec3(1.0f, 0.95f, 0.40f) : (isHovered ? glm::vec3(0.88f, 0.88f, 0.92f) : glm::vec3(0.72f, 0.72f, 0.75f));
            glm::vec3 edgeDark  = isSelected ? glm::vec3(0.85f, 0.70f, 0.15f) : glm::vec3(0.25f, 0.25f, 0.28f);

            if (isUp) {
                addThickLine(glm::vec3(p1, 0.0f), glm::vec3(p0, 0.0f), bw, edgeLight);
                addThickLine(glm::vec3(p0, 0.0f), glm::vec3(p2, 0.0f), bw, edgeDark);
                addThickLine(glm::vec3(p1, 0.0f), glm::vec3(p2, 0.0f), bw, edgeDark);
            } else {
                addThickLine(glm::vec3(p0, 0.0f), glm::vec3(p1, 0.0f), bw, edgeDark);
                addThickLine(glm::vec3(p0, 0.0f), glm::vec3(p2, 0.0f), bw, edgeLight);
                addThickLine(glm::vec3(p1, 0.0f), glm::vec3(p2, 0.0f), bw, edgeDark);
            }

            if (isSelected) {
                glm::vec3 gold(1.0f, 0.90f, 0.20f);
                addThickLine(glm::vec3(p0, 0.0f), glm::vec3(p1, 0.0f), 1.5f, gold);
                addThickLine(glm::vec3(p1, 0.0f), glm::vec3(p2, 0.0f), 1.5f, gold);
                addThickLine(glm::vec3(p2, 0.0f), glm::vec3(p0, 0.0f), 1.5f, gold);
            }

            if (isHovered && item != BlockType::Air) {
                hoveredItem = item;
            }

            if (item != BlockType::Air) {
                glm::vec2 centroid = (p0 + p1 + p2) / 3.0f;
                draw3DItemIcon(centroid.x, centroid.y, layout.S * 0.52f, item);

                if (count > 0) {
                    char cBuf[16]; std::snprintf(cBuf, sizeof(cBuf), "%d", count);
                    float fontSize = 9.0f;
                    float textW = FontRenderer::getTextWidth(cBuf, fontSize);
                    float cx = isUp ? ((p1.x + p2.x) * 0.5f) : ((p0.x + p1.x) * 0.5f);
                    float tx = cx - textW * 0.5f;
                    float ty = isUp ? (p1.y - fontSize - 2.5f) : (p0.y + 2.5f);
                    addSolidQuad(tx - 2.0f, ty - 1.0f, textW + 4.0f, fontSize + 2.0f, glm::vec3(0.08f, 0.08f, 0.10f));
                    FontRenderer::drawText(vertices, indices, cBuf, tx, ty, fontSize, glm::vec3(1.0f), true);
                }
            }
        };

        // 1. Result Slot (Equilateral Triangle pointing UP)
        glm::vec2 rv0, rv1, rv2;
        layout.getResultTri(rv0, rv1, rv2);
        CraftingResult res = (state == GameState::CraftingTable) ?
            CraftingSystem::craft3x3(m_craftingTableGrid) :
            CraftingSystem::craft2x2(m_craftingGrid);
        bool rHov = pointInTriangle(mousePos, rv0, rv1, rv2);
        drawTriSlot(rv0, rv1, rv2, true, res.item, res.count, rHov, false);

        // 2. Pixel Arrow '->' between Crafting Grid and Result Slot
        float arrX = layout.arrowX;
        float arrY = layout.arrowY;
        glm::vec3 arrCol(0.25f, 0.25f, 0.28f);
        addSolidQuad(arrX, arrY - 2.0f, 18.0f, 4.0f, arrCol);
        addTri(glm::vec3(arrX + 26.0f, arrY, 0.0f), glm::vec3(arrX + 16.0f, arrY - 7.0f, 0.0f), glm::vec3(arrX + 16.0f, arrY + 7.0f, 0.0f), arrCol);

        // 3. Crafting Grid (4 slots for 2x2, 9 slots for 3x3)
        if (state == GameState::CraftingTable) {
            for (int k = 0; k < 9; ++k) {
                glm::vec2 cv0, cv1, cv2; bool isUp;
                layout.getCraftingTableTri(k, cv0, cv1, cv2, isUp);
                bool hov = pointInTriangle(mousePos, cv0, cv1, cv2);
                drawTriSlot(cv0, cv1, cv2, isUp, m_craftingTableGrid[k], m_craftingTableCounts[k], hov, false);
            }
        } else {
            for (int k = 0; k < 4; ++k) {
                glm::vec2 cv0, cv1, cv2; bool isUp;
                layout.getCraftingTri(k, cv0, cv1, cv2, isUp);
                bool hov = pointInTriangle(mousePos, cv0, cv1, cv2);
                drawTriSlot(cv0, cv1, cv2, isUp, m_craftingGrid[k], m_craftingCounts[k], hov, false);
            }
        }

        // 4. 30 Storage Slots (3 rows of 10 tessellating triangles)
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 10; ++c) {
                int slotIdx = r * 10 + c;
                glm::vec2 sv0, sv1, sv2; bool isUp;
                layout.getStorageTri(r, c, sv0, sv1, sv2, isUp);
                bool hov = pointInTriangle(mousePos, sv0, sv1, sv2);
                drawTriSlot(sv0, sv1, sv2, isUp, player.getStorageBlock(slotIdx), player.getStorageCount(slotIdx), hov, false);
            }
        }

        // 5. 10 Hotbar Slots (at bottom)
        for (int i = 0; i < 10; ++i) {
            glm::vec2 hv0, hv1, hv2; bool isUp;
            layout.getHotbarTri(i, hv0, hv1, hv2, isUp);
            bool hov = pointInTriangle(mousePos, hv0, hv1, hv2);
            bool isSel = (i == player.getSelectedSlot());
            drawTriSlot(hv0, hv1, hv2, isUp, player.getHotbar()[i], player.getHotbarCounts()[i], hov, isSel);
        }

        // 6. Floating Cursor Item attached to mousePos
        if (m_cursorItem != BlockType::Air) {
            draw3DItemIcon(mousePos.x, mousePos.y, 28.0f, m_cursorItem);
            if (m_cursorCount > 1) {
                char cBuf[16]; std::snprintf(cBuf, sizeof(cBuf), "%d", m_cursorCount);
                float fontSize = 9.0f;
                float textW = FontRenderer::getTextWidth(cBuf, fontSize);
                float tx = mousePos.x + 4.0f;
                float ty = mousePos.y + 4.0f;
                addSolidQuad(tx - 2.0f, ty - 1.0f, textW + 4.0f, fontSize + 2.0f, glm::vec3(0.08f, 0.08f, 0.10f));
                FontRenderer::drawText(vertices, indices, cBuf, tx, ty, fontSize, glm::vec3(1.0f), true);
            }
        }

        // 7. Floating Minecraft Item Name Tooltip on Mouse Hover
        if (hoveredItem != BlockType::Air && m_cursorItem == BlockType::Air) {
            std::string name(getItemDisplayName(hoveredItem));
            float textW = FontRenderer::getTextWidth(name, 12.0f);
            float padX = 7.0f;
            float padY = 5.0f;
            float boxW = textW + padX * 2.0f;
            float boxH = 12.0f + padY * 2.0f;

            float tx = mousePos.x + 12.0f;
            float ty = mousePos.y - 14.0f;

            if (tx + boxW > sw - 4.0f) tx = sw - boxW - 4.0f;
            if (ty < 4.0f) ty = 4.0f;

            addSolidQuad(tx, ty, boxW, boxH, glm::vec3(0.06f, 0.0f, 0.08f));
            addSolidQuad(tx - 1.0f, ty, 1.0f, boxH, glm::vec3(0.25f, 0.05f, 0.60f));
            addSolidQuad(tx + boxW, ty, 1.0f, boxH, glm::vec3(0.25f, 0.05f, 0.60f));
            addSolidQuad(tx, ty - 1.0f, boxW, 1.0f, glm::vec3(0.35f, 0.10f, 0.85f));
            addSolidQuad(tx, ty + boxH, boxW, 1.0f, glm::vec3(0.18f, 0.02f, 0.45f));

            FontRenderer::drawText(vertices, indices, name, tx + padX, ty + padY, 12.0f, glm::vec3(1.0f, 1.0f, 1.0f), true);
        }

    } else if (state == GameState::Death) {
        // Transparent Blood-Red Tint overlay covering the live world
        glm::vec4 redTintUV = TextureAtlas::getTileUV(128);
        addTexturedQuad(0.0f, 0.0f, sw, sh, redTintUV, glm::vec3(1.0f));

        // Giant "YOU DIED!" Title Box
        float titleBoxW = 440.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 140.0f, titleBoxW, 56.0f, glm::vec3(0.12f, 0.04f, 0.04f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 140.0f, titleBoxW, 3.5f, glm::vec3(0.85f, 0.15f, 0.15f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 87.5f, titleBoxW, 3.5f, glm::vec3(0.85f, 0.15f, 0.15f));

        std::string deathTitle = "YOU DIED!";
        float deathW = FontRenderer::getTextWidth(deathTitle, 36.0f);
        FontRenderer::drawText(vertices, indices, deathTitle, cx - deathW * 0.5f, cy - 130.0f, 36.0f, glm::vec3(1.0f, 0.20f, 0.20f), true);

        std::string scoreStr = "Score: 0";
        float scoreW = FontRenderer::getTextWidth(scoreStr, 16.0f);
        FontRenderer::drawText(vertices, indices, scoreStr, cx - scoreW * 0.5f, cy - 65.0f, 16.0f, glm::vec3(1.0f, 0.90f, 0.25f), true);

        std::string causeStr = "You fell victim to the triangular voxel depths";
        float causeW = FontRenderer::getTextWidth(causeStr, 13.0f);
        FontRenderer::drawText(vertices, indices, causeStr, cx - causeW * 0.5f, cy - 42.0f, 13.0f, glm::vec3(0.80f, 0.80f, 0.80f), true);

        // 1. RESPAWN (Points Right)
        glm::vec2 p1_0(bx, cy + 20.0f), p1_1(bx, cy + 20.0f + btnH), p1_2(bx + btnW, cy + 20.0f + btnH * 0.5f);
        bool h1 = pointInTriangle(mousePos, p1_0, p1_1, p1_2);
        addPrismButton(p1_0, p1_1, p1_2, "RESPAWN", h1, true, 16.0f);

        // 2. TITLE MENU (Points Left)
        glm::vec2 p2_0(bx + btnW, cy + 85.0f), p2_1(bx + btnW, cy + 85.0f + btnH), p2_2(bx, cy + 85.0f + btnH * 0.5f);
        bool h2 = pointInTriangle(mousePos, p2_0, p2_1, p2_2);
        addPrismButton(p2_0, p2_1, p2_2, "TITLE MENU", h2, false, 16.0f);
    }

    uint32_t f = m_cmdQueue.getCurrentFrame();
    m_indexCount[f] = static_cast<uint32_t>(indices.size());
    if (m_indexCount[f] == 0) return;

    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_vbo[f] = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_vbo[f].uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_ibo[f] = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ibo[f].uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);

    m_lastState = state;
    m_lastW = screenWidth;
    m_lastH = screenHeight;
}

void MenuRenderer::render(VkCommandBuffer cmd,
                          const Pipeline& uiPipeline,
                          GameState state,
                          const Player& player,
                          uint32_t screenWidth,
                          uint32_t screenHeight,
                          glm::vec2 mousePos,
                          const GameOptions& options,
                          float loadingProgress,
                          int loadedChunks,
                          int totalChunks) {
    if (state == GameState::Playing || screenWidth == 0 || screenHeight == 0) return;

    rebuildMenuMesh(state, player, screenWidth, screenHeight, mousePos, options, loadingProgress, loadedChunks, totalChunks);

    uint32_t f = m_cmdQueue.getCurrentFrame();
    if (m_indexCount[f] == 0 || !m_vbo[f].isValid()) return;

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight), 0.0f, -1.0f, 1.0f);

    PushConstants pc{};
    std::memcpy(pc.mvp, &proj[0][0], sizeof(float) * 16);
    pc.sunDir[3] = 1.0f;
    pc.lightColor[0] = 1.0f; pc.lightColor[1] = 1.0f; pc.lightColor[2] = 1.0f;
    pc.skyFog[3] = 1000.0f;

    vkCmdPushConstants(cmd, uiPipeline.getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &pc);

    VkBuffer vbs[] = {m_vbo[f].getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo[f].getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_indexCount[f], 1, 0, 0, 0);
}

void MenuRenderer::returnCraftingItems(Player& player) {
    if (m_cursorItem != BlockType::Air && m_cursorCount > 0) {
        player.pickupItem(m_cursorItem, m_cursorCount);
        m_cursorItem = BlockType::Air;
        m_cursorCount = 0;
    }
    for (int k = 0; k < 4; ++k) {
        if (m_craftingGrid[k] != BlockType::Air && m_craftingCounts[k] > 0) {
            player.pickupItem(m_craftingGrid[k], m_craftingCounts[k]);
            m_craftingGrid[k] = BlockType::Air;
            m_craftingCounts[k] = 0;
        }
    }
    for (int k = 0; k < 9; ++k) {
        if (m_craftingTableGrid[k] != BlockType::Air && m_craftingTableCounts[k] > 0) {
            player.pickupItem(m_craftingTableGrid[k], m_craftingTableCounts[k]);
            m_craftingTableGrid[k] = BlockType::Air;
            m_craftingTableCounts[k] = 0;
        }
    }
}

} // namespace prismcraft

