#include "MenuRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "player/Player.hpp"
#include "world/ChunkMesher.hpp"
#include "renderer/TextureAtlas.hpp"
#include "renderer/ItemDropRenderer.hpp"
#include "audio/AudioEngine.hpp"
#include "ui/FontData.hpp"
#include "core/Input.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <thread>

namespace prismcraft {

MenuRenderer::MenuRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_vbo[i] = Buffer(m_context, MAX_MENU_VBO_SIZE,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        m_ibo[i] = Buffer(m_context, MAX_MENU_IBO_SIZE,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        m_indexCount[i] = 0;
    }
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

    auto checkTriRight = [&](float bx, float by, float bw, float bh) {
        glm::vec2 v0(bx, by);
        glm::vec2 v1(bx, by + bh);
        glm::vec2 v2(bx + bw, by + bh * 0.5f);
        return pointInTriangle(mousePos, v0, v1, v2);
    };

    auto checkTriLeft = [&](float bx, float by, float bw, float bh) {
        glm::vec2 v0(bx + bw, by);
        glm::vec2 v1(bx + bw, by + bh);
        glm::vec2 v2(bx, by + bh * 0.5f);
        return pointInTriangle(mousePos, v0, v1, v2);
    };

    auto checkTriUp = [&](float bx, float by, float bw, float bh) {
        glm::vec2 v0(bx + bw * 0.5f, by);
        glm::vec2 v1(bx, by + bh);
        glm::vec2 v2(bx + bw, by + bh);
        return pointInTriangle(mousePos, v0, v1, v2);
    };

    if (state == GameState::MainMenu) {
        // [▶ PLAY GAME / SELECT WORLD] (y = cy - 40)
        if (checkRightTri(cy - 40.0f)) {
            AudioEngine::get().playSound(SoundEffect::Click);
            refreshWorldList();
            state = GameState::WorldSelect;
            return 12; // Open World Select
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
    } else if (state == GameState::WorldSelect) {
        float listW = 500.0f;
        float cardH = 50.0f;
        float listY = cy - 130.0f;
        float listX = cx - listW * 0.5f;

        // Click on world card
        int maxShown = std::min(4, static_cast<int>(m_worldList.size()));
        for (int i = 0; i < maxShown; ++i) {
            int worldIdx = m_worldListScroll + i;
            if (worldIdx >= static_cast<int>(m_worldList.size())) break;

            float cardY = listY + i * (cardH + 8.0f);
            if (mousePos.x >= listX && mousePos.x <= listX + listW &&
                mousePos.y >= cardY && mousePos.y <= cardY + cardH) {
                if (m_selectedWorldIndex == worldIdx && !isDown) {
                    AudioEngine::get().playSound(SoundEffect::Click);
                    return 13; // Play Selected World on double click
                }
                AudioEngine::get().playSound(SoundEffect::Click);
                m_selectedWorldIndex = worldIdx;
                return 0;
            }
        }

        // Row 1: [PLAY SELECTED WORLD] (left) | [CREATE NEW WORLD] (right)
        float bW = 240.0f;
        float bH = 38.0f;
        float bY1 = cy + 115.0f;
        float bY2 = cy + 165.0f;

        // [PLAY SELECTED WORLD]
        if (checkTriRight(cx - 250.0f, bY1, bW, bH) && !isDown) {
            if (!m_worldList.empty() && m_selectedWorldIndex >= 0 && m_selectedWorldIndex < static_cast<int>(m_worldList.size())) {
                AudioEngine::get().playSound(SoundEffect::Click);
                return 13; // Play Selected World
            }
        }
        // [CREATE NEW WORLD]
        if (checkTriRight(cx + 10.0f, bY1, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::WorldCreation;
            return 2;
        }
        // [DELETE WORLD]
        if (checkTriLeft(cx - 250.0f, bY2, bW, bH) && !isDown) {
            if (!m_worldList.empty() && m_selectedWorldIndex >= 0 && m_selectedWorldIndex < static_cast<int>(m_worldList.size())) {
                AudioEngine::get().playSound(SoundEffect::Click);
                SaveManager::deleteWorld(m_worldList[m_selectedWorldIndex].folderName);
                refreshWorldList();
                return 14;
            }
        }
        // [BACK TO TITLE]
        if (checkTriLeft(cx + 10.0f, bY2, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::MainMenu;
            return 4;
        }
    } else if (state == GameState::WorldCreation) {
        float boxW = 340.0f;
        float boxH = 36.0f;
        float boxX = cx - boxW * 0.5f;

        // Focus World Name box (cy - 65)
        if (mousePos.x >= boxX && mousePos.x <= boxX + boxW && mousePos.y >= cy - 65.0f && mousePos.y <= cy - 65.0f + boxH) {
            m_creationFieldFocus = 0;
            AudioEngine::get().playSound(SoundEffect::Click);
            return 0;
        }

        // Focus Seed box (cy - 10)
        if (mousePos.x >= boxX && mousePos.x <= boxX + boxW && mousePos.y >= cy - 10.0f && mousePos.y <= cy - 10.0f + boxH) {
            m_creationFieldFocus = 1;
            AudioEngine::get().playSound(SoundEffect::Click);
            return 0;
        }

        // [▶ RANDOMIZE SEED] (y = cy + 35)
        if (checkTriRight(cx - 160.0f, cy + 35.0f, 320.0f, 34.0f) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            worldSeed = (worldSeed * 1664525u + 1013904223u) % 1000000u;
            m_newWorldSeedStr = std::to_string(worldSeed);
            m_currentSeed = worldSeed;
            return 0;
        }

        // [▶ MODE: SURVIVAL / CREATIVE] (y = cy + 78)
        if (checkTriRight(cx - 160.0f, cy + 78.0f, 320.0f, 34.0f) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            m_startInCreative = !m_startInCreative;
            return 0;
        }

        // [▶ CREATE WORLD] (y = cy + 130, left)
        if (checkTriRight(cx - 220.0f, cy + 130.0f, 210.0f, 38.0f) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            if (!m_newWorldSeedStr.empty()) {
                try {
                    worldSeed = static_cast<uint32_t>(std::stoul(m_newWorldSeedStr));
                } catch (...) {}
            }
            return 3; // Create & Launch
        }

        // [◀ CANCEL] (y = cy + 130, right)
        if (checkTriLeft(cx + 10.0f, cy + 130.0f, 210.0f, 38.0f) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::WorldSelect;
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
        float bH = 38.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 145.0f;
        float dy = 44.0f;

        auto inBox = [&](float bx, float by, float bw, float bh) {
            return (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
        };

        // Row 0: FOV (slider, left) | DEBUG HUD F3 (button, right)
        if (inBox(lx, y0, bW, bH)) {
            float t = std::clamp((mousePos.x - lx) / bW, 0.0f, 1.0f);
            options.fov = static_cast<int>(std::round(60.0f + t * 50.0f));
            return 7;
        }
        if (checkTriLeft(rx, y0, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.debugHUD = !options.debugHUD;
            return 23;
        }

        // Row 1: VIDEO SETTINGS... (left) | VIBRANT VISUALS... (right)
        if (checkTriRight(lx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::VideoSettings;
            return 6;
        }
        if (checkTriLeft(rx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::VibrantVisualsSettings;
            return 6;
        }

        // Row 2: VOXEL LOD SETTINGS... (left) | MUSIC & SOUNDS... (right)
        if (checkTriRight(lx, y0 + dy * 2.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::LODSettings;
            return 6;
        }
        if (checkTriLeft(rx, y0 + dy * 2.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::AudioSettings;
            return 6;
        }

        // Row 3: CONTROLS & KEYBINDS... (left) | LIGHT OVERLAY F7 (right)
        if (checkTriRight(lx, y0 + dy * 3.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::ControlsSettings;
            return 6;
        }
        if (checkTriLeft(rx, y0 + dy * 3.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.lightOverlay = !options.lightOverlay;
            return 20;
        }

        // Row 4: SHOW FPS (left) | SHOW XYZ (right)
        if (checkTriRight(lx, y0 + dy * 4.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.showFPS = !options.showFPS;
            return 24;
        }
        if (checkTriLeft(rx, y0 + dy * 4.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.showXYZ = !options.showXYZ;
            return 25;
        }

        // Row 5: DONE / BACK (center)
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 5.2f;
        if (checkTriUp(doneX, doneY, doneW, doneH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = m_previousState;
            return 4;
        }
    } else if (state == GameState::LODSettings) {
        float bW = 260.0f;
        float bH = 36.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 180.0f;
        float dy = 42.0f;

        auto inBox = [&](float bx, float by, float bw, float bh) {
            return (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
        };

        // Row 0: Active Chunks slider (left) | Distant Horizons LOD Slider (right)
        if (inBox(lx, y0, bW, bH)) {
            float t = std::clamp((mousePos.x - lx) / bW, 0.0f, 1.0f);
            int targetRD = static_cast<int>(std::round(4.0f + t * 20.0f));
            targetRD = std::clamp((targetRD / 2) * 2, 4, 24);
            if (targetRD != options.renderDistance) {
                options.renderDistance = targetRD;
                return 17;
            }
        }
        if (inBox(rx, y0, bW, bH)) {
            float t = std::clamp((mousePos.x - rx) / bW, 0.0f, 1.0f);
            int targetLOD = static_cast<int>(std::round(16.0f + t * 240.0f));
            targetLOD = std::clamp((targetLOD / 16) * 16, 16, 256);
            if (targetLOD != options.lodDistance) {
                options.lodDistance = targetLOD;
                return 26;
            }
        }

        // Row 1: LOD Quality Preset (left)
        if (checkTriRight(lx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.lodPreset = (options.lodPreset + 1) % 4;
            return 21;
        }

        // Done / Back
        float infoBoxY = y0 + dy * 3.2f;
        float infoBoxH = 120.0f;
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = infoBoxY + infoBoxH + 18.0f;
        if (checkTriUp(doneX, doneY, doneW, doneH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Options;
            return 4;
        }
    } else if (state == GameState::VibrantVisualsSettings) {
        float bW = 260.0f;
        float bH = 36.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 180.0f;
        float dy = 42.0f;

        // Row 0: Vibrant Visuals Master (left) | Shaders / Post-FX (right)
        if (checkTriRight(lx, y0, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.vibrantVisuals = !options.vibrantVisuals;
            return 19;
        }
        if (checkTriLeft(rx, y0, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.shadersEnabled = !options.shadersEnabled;
            return 30;
        }

        // Row 1: Volumetric Clouds (left) | Cloud Shadows (right)
        if (checkTriRight(lx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.clouds = !options.clouds;
            return 18;
        }
        if (checkTriLeft(rx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.cloudShadows = !options.cloudShadows;
            return 18;
        }

        // Row 2: Water Quality (left) | SSAO Ambient Occlusion (right)
        if (checkTriRight(lx, y0 + dy * 2.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.waterQuality = (options.waterQuality + 1) % 3;
            return 25;
        }
        if (checkTriLeft(rx, y0 + dy * 2.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            if (options.aoStrength <= 0.01f) {
                options.smoothLighting = true;
                options.aoStrength = 0.8f;
            } else if (options.aoStrength < 1.0f) {
                options.smoothLighting = true;
                options.aoStrength = 1.2f;
            } else if (options.aoStrength < 1.4f) {
                options.smoothLighting = true;
                options.aoStrength = 1.6f;
            } else {
                options.smoothLighting = false;
                options.aoStrength = 0.0f;
            }
            return 29;
        }

        // Row 3: Color Grading (left) | Torch Color Bleed (right)
        if (checkTriRight(lx, y0 + dy * 3.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.colorGrading = (options.colorGrading + 1) % 5;
            return 27;
        }
        if (checkTriLeft(rx, y0 + dy * 3.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.torchColorBleed = !options.torchColorBleed;
            return 28;
        }

        // Done / Back
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 4.5f;
        if (checkTriUp(doneX, doneY, doneW, doneH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Options;
            return 4;
        }
    } else if (state == GameState::VideoSettings) {
        float bW = 260.0f;
        float bH = 32.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 200.0f;
        float dy = 37.0f;

        auto inBox = [&](float bx, float by, float bw, float bh) {
            return (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
        };

        // Row 0: VIBRANT VISUALS (toggle, left) | RENDER DISTANCE (slider 4..48, right)
        if (checkTriRight(lx, y0, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.vibrantVisuals = !options.vibrantVisuals;
            return 19;
        }
        if (inBox(rx, y0, bW, bH)) {
            float t = std::clamp((mousePos.x - rx) / bW, 0.0f, 1.0f);
            int targetRD = static_cast<int>(std::round(4.0f + t * 44.0f));
            targetRD = std::clamp((targetRD / 2) * 2, 4, 48);
            if (targetRD != options.renderDistance) {
                options.renderDistance = targetRD;
                return 17;
            }
        }

        // Row 1: MAX FPS (toggle, left) | VSYNC (toggle, right)
        if (checkTriRight(lx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            static const int fpsList[] = {0, 30, 60, 90, 120, 144, 240};
            int curIdx = 0;
            for (int k = 0; k < 7; ++k) { if (fpsList[k] == options.maxFps) { curIdx = k; break; } }
            options.maxFps = fpsList[(curIdx + 1) % 7];
            return 15;
        }
        if (checkTriLeft(rx, y0 + dy, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.vsync = !options.vsync;
            return 16;
        }

        // Row 2 (Vibrant only): WATER (Vanilla / RTX, left) | SHADOW QUALITY (toggle, right)
        if (checkTriRight(lx, y0 + dy * 2.0f, bW, bH) && !isDown && options.vibrantVisuals) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.waterQuality = (options.waterQuality >= 2) ? 0 : 2;
            return 25;
        }
        if (checkTriLeft(rx, y0 + dy * 2.0f, bW, bH) && !isDown && options.vibrantVisuals) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.shadowQuality = (options.shadowQuality + 1) % 4;
            return 24;
        }

        // Row 3 (Vibrant only): CLOUDS (3D toggle, left) | CLOUD SEED (cycle, right)
        if (checkTriRight(lx, y0 + dy * 3.0f, bW, bH) && !isDown && options.vibrantVisuals) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.clouds = !options.clouds;
            options.cloudShadows = options.clouds;
            return 18;
        }
        if (checkTriLeft(rx, y0 + dy * 3.0f, bW, bH) && !isDown && options.vibrantVisuals) {
            AudioEngine::get().playSound(SoundEffect::Click);
            static const int seedList[] = {1337, 42, 101, 777, 2026, 9999};
            int curIdx = 0;
            for (int k = 0; k < 6; ++k) { if (seedList[k] == options.cloudSeed) { curIdx = k; break; } }
            options.cloudSeed = seedList[(curIdx + 1) % 6];
            return 27;
        }

        // Row 4: CONTACT SHADING (cycle: REALISTIC -> ENHANCED -> OFF) | STEVE SHADOW (toggle, right)
        if (checkTriRight(lx, y0 + dy * 4.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            if (!options.smoothLighting || options.aoStrength <= 0.01f) {
                options.smoothLighting = true;
                options.aoStrength = 1.0f;
            } else if (options.aoStrength < 1.2f) {
                options.smoothLighting = true;
                options.aoStrength = 1.4f;
            } else {
                options.smoothLighting = false;
                options.aoStrength = 0.0f;
            }
            return 29;
        }
        if (checkTriLeft(rx, y0 + dy * 4.0f, bW, bH) && !isDown && options.vibrantVisuals) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.playerShadow = !options.playerShadow;
            return 28;
        }

        // Row 5: LOD PRESET (toggle, left) | WINDOW MODE (toggle, right)
        if (checkTriRight(lx, y0 + dy * 5.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.lodPreset = (options.lodPreset + 1) % 4;
            return 21;
        }
        if (checkTriLeft(rx, y0 + dy * 5.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.windowMode = (options.windowMode + 1) % 3;
            return 13;
        }

        // Row 6: UPSCALER (toggle, left) | QUALITY PRESET (toggle, right)
        if (checkTriRight(lx, y0 + dy * 6.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.upscalerMode = (options.upscalerMode + 1) % 3;
            return 30;
        }
        if (checkTriLeft(rx, y0 + dy * 6.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.upscalerQuality = (options.upscalerQuality + 1) % 4;
            return 30;
        }

        // Row 7: RCAS SHARPNESS (slider, left) | RAM CACHE (toggle, right)
        if (inBox(lx, y0 + dy * 7.0f, bW, bH)) {
            float t = std::clamp((mousePos.x - lx) / bW, 0.0f, 1.0f);
            options.upscalerSharpness = t;
            return 31;
        }
        if (checkTriLeft(rx, y0 + dy * 7.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.ramCacheSize = (options.ramCacheSize + 1) % 4;
            return 32;
        }

        // Row 8: UI SCALE (toggle, left) | RESOLUTION (toggle, right)
        if (checkTriRight(lx, y0 + dy * 8.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            if (options.uiScale < 1.25f) options.uiScale = 1.5f;
            else if (options.uiScale < 1.75f) options.uiScale = 2.0f;
            else if (options.uiScale < 2.25f) options.uiScale = 2.5f;
            else if (options.uiScale < 2.75f) options.uiScale = 3.0f;
            else options.uiScale = 1.0f;
            return 12;
        }
        if (checkTriLeft(rx, y0 + dy * 8.0f, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            options.resIndex = (options.resIndex + 1) % 5;
            return 14;
        }

        // Row 9: DONE / BACK (center)
        float doneW = 320.0f;
        float doneH = 36.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 9.0f + 6.0f;
        if (checkTriUp(doneX, doneY, doneW, doneH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Options;
            return 2;
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
        if (checkTriLeft(rx, y0, bW, bH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            return 8;
        }

        // Done button
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = cy + 160.0f;
        if (checkTriUp(doneX, doneY, doneW, doneH) && !isDown) {
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
        if (checkTriUp(doneX, doneY, doneW, doneH) && !isDown) {
            AudioEngine::get().playSound(SoundEffect::Click);
            state = GameState::Options;
            return 6;
        }
    } else if (state == GameState::Inventory || state == GameState::CraftingTable) {
        if (isDown) return 0;

        if (state == GameState::Inventory && player.isCreative()) {
            float panW = 440.0f;
            float panH = 370.0f;
            float px = (static_cast<float>(screenWidth) - panW) * 0.5f;
            float py = (static_cast<float>(screenHeight) - panH) * 0.5f;

            float searchX = px + 14.0f;
            float searchY = py + 24.0f;
            float searchW = 346.0f;
            float searchH = 22.0f;

            float clearX = searchX + searchW + 8.0f;
            float clearY = searchY;
            float clearW = 22.0f;
            float clearH = 22.0f;

            // 1. Clear search button click
            if (mousePos.x >= clearX && mousePos.x <= clearX + clearW && mousePos.y >= clearY && mousePos.y <= clearY + clearH) {
                m_creativeSearchQuery.clear();
                m_creativeScrollRow = 0;
                AudioEngine::get().playSound(SoundEffect::Click);
                return 0;
            }

            // Click inside search box
            if (mousePos.x >= searchX && mousePos.x <= searchX + searchW && mousePos.y >= searchY && mousePos.y <= searchY + searchH) {
                return 0;
            }

            std::vector<BlockType> catalog = getCreativeCatalog(m_creativeSearchQuery);
            int totalRows = (static_cast<int>(catalog.size()) + 8) / 9;
            int maxScroll = std::max(0, totalRows - 5);
            if (m_creativeScrollRow > maxScroll) m_creativeScrollRow = maxScroll;
            if (m_creativeScrollRow < 0) m_creativeScrollRow = 0;

            float gridX = px + 14.0f;
            float gridY = py + 52.0f;
            float slotStep = 38.0f;
            float slotW = 34.0f;
            float slotH = 34.0f;

            // 2. Scrollbar track & arrows
            float sbX = gridX + 9.0f * slotStep + 6.0f;
            float sbY = gridY;
            float sbW = 16.0f;
            float sbH = 5.0f * slotStep - 4.0f;

            if (mousePos.x >= sbX && mousePos.x <= sbX + sbW && mousePos.y >= sbY && mousePos.y <= sbY + sbH) {
                if (mousePos.y <= sbY + 16.0f) {
                    m_creativeScrollRow = std::max(0, m_creativeScrollRow - 1);
                } else if (mousePos.y >= sbY + sbH - 16.0f) {
                    m_creativeScrollRow = std::min(maxScroll, m_creativeScrollRow + 1);
                } else if (maxScroll > 0) {
                    float rel = (mousePos.y - sbY - 16.0f) / std::max(1.0f, sbH - 32.0f);
                    m_creativeScrollRow = std::clamp(static_cast<int>(rel * maxScroll + 0.5f), 0, maxScroll);
                }
                AudioEngine::get().playSound(SoundEffect::Click);
                return 0;
            }

            // 3. Catalog Grid click
            for (int r = 0; r < 5; ++r) {
                for (int c = 0; c < 9; ++c) {
                    float sx = gridX + c * slotStep;
                    float sy = gridY + r * slotStep;
                    if (mousePos.x >= sx && mousePos.x <= sx + slotW && mousePos.y >= sy && mousePos.y <= sy + slotH) {
                        int idx = (m_creativeScrollRow + r) * 9 + c;
                        if (idx < static_cast<int>(catalog.size())) {
                            BlockType clicked = catalog[idx];
                            if (Input::isKeyDown(GLFW_KEY_LEFT_SHIFT)) {
                                bool placed = false;
                                for (int h = 0; h < 10; ++h) {
                                    if (player.getHotbar()[h] == BlockType::Air || player.getHotbar()[h] == clicked) {
                                        player.setHotbarBlock(h, clicked, 64);
                                        placed = true;
                                        break;
                                    }
                                }
                                if (!placed) {
                                    player.setHotbarBlock(player.getSelectedSlot(), clicked, 64);
                                }
                                AudioEngine::get().playSound(SoundEffect::ItemPop);
                                return 0;
                            } else {
                                m_cursorItem = clicked;
                                m_cursorCount = 64;
                                AudioEngine::get().playSound(SoundEffect::Click);
                                return 0;
                            }
                        }
                    }
                }
            }

            // 4. Hotbar slots click
            float hbX = gridX;
            float hbY = py + 266.0f;
            float hbStep = 35.0f;
            float hbW = 32.0f; float hbH = 32.0f;

            for (int i = 0; i < 10; ++i) {
                float hsx = hbX + i * hbStep;
                float hsy = hbY;
                if (mousePos.x >= hsx && mousePos.x <= hsx + hbW && mousePos.y >= hsy && mousePos.y <= hsy + hbH) {
                    if (isRightClick) {
                        if (m_cursorItem != BlockType::Air) {
                            if (player.getHotbar()[i] == BlockType::Air) {
                                player.setHotbarBlock(i, m_cursorItem, 1);
                            } else if (player.getHotbar()[i] == m_cursorItem && player.getHotbarCounts()[i] < 64) {
                                player.setHotbarBlock(i, m_cursorItem, player.getHotbarCounts()[i] + 1);
                            }
                            AudioEngine::get().playSound(SoundEffect::Click);
                            return 0;
                        } else if (player.getHotbar()[i] != BlockType::Air) {
                            int count = player.getHotbarCounts()[i];
                            int take = (count + 1) / 2;
                            m_cursorItem = player.getHotbar()[i];
                            m_cursorCount = take;
                            player.setHotbarBlock(i, (count - take > 0) ? player.getHotbar()[i] : BlockType::Air, count - take);
                            AudioEngine::get().playSound(SoundEffect::Click);
                            return 0;
                        }
                    } else {
                        // Left click
                        if (m_cursorItem != BlockType::Air) {
                            if (player.getHotbar()[i] == m_cursorItem) {
                                player.setHotbarBlock(i, m_cursorItem, 64);
                            } else {
                                BlockType oldItem = player.getHotbar()[i];
                                int oldCount = player.getHotbarCounts()[i];
                                player.setHotbarBlock(i, m_cursorItem, m_cursorCount);
                                m_cursorItem = oldItem;
                                m_cursorCount = oldCount;
                            }
                            AudioEngine::get().playSound(SoundEffect::Click);
                            return 0;
                        } else {
                            if (player.getHotbar()[i] != BlockType::Air) {
                                m_cursorItem = player.getHotbar()[i];
                                m_cursorCount = player.getHotbarCounts()[i];
                                player.setHotbarBlock(i, BlockType::Air, 0);
                                AudioEngine::get().playSound(SoundEffect::Click);
                                return 0;
                            }
                        }
                    }
                }
            }

            // 5. Trash slot click
            float trashX = px + panW - 48.0f;
            float trashY = hbY;
            float trashW = 34.0f; float trashH = 32.0f;

            if (mousePos.x >= trashX && mousePos.x <= trashX + trashW && mousePos.y >= trashY && mousePos.y <= trashY + trashH) {
                if (Input::isKeyDown(GLFW_KEY_LEFT_SHIFT)) {
                    for (int i = 0; i < 10; ++i) player.setHotbarBlock(i, BlockType::Air, 0);
                    m_cursorItem = BlockType::Air;
                    m_cursorCount = 0;
                    AudioEngine::get().playSound(SoundEffect::ItemPop);
                    return 0;
                } else {
                    m_cursorItem = BlockType::Air;
                    m_cursorCount = 0;
                    AudioEngine::get().playSound(SoundEffect::ItemPop);
                    return 0;
                }
            }

            // 6. Click outside dialog panel
            bool insidePanel = (mousePos.x >= px && mousePos.x <= px + panW &&
                                mousePos.y >= py && mousePos.y <= py + panH);
            if (!insidePanel && m_cursorItem != BlockType::Air) {
                if (itemDrops) {
                    glm::vec3 eye = player.getPosition() + glm::vec3(0.0f, 1.62f, 0.0f);
                    glm::vec3 fwd = player.getCamera().getForward();
                    glm::vec3 dropPos = eye + fwd * 0.8f;
                    glm::vec3 dropVel = fwd * 4.0f + glm::vec3(0.0f, 1.2f, 0.0f);
                    itemDrops->spawnDrop(dropPos, m_cursorItem, dropVel);
                }
                m_cursorItem = BlockType::Air;
                m_cursorCount = 0;
                AudioEngine::get().playSound(SoundEffect::ItemPop);
                return 0;
            }

            return 0;
        }

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

    auto checkTriRight = [&](float bx_, float by_, float bw_, float bh_) {
        glm::vec2 v0(bx_, by_);
        glm::vec2 v1(bx_, by_ + bh_);
        glm::vec2 v2(bx_ + bw_, by_ + bh_ * 0.5f);
        return pointInTriangle(mousePos, v0, v1, v2);
    };

    auto checkTriLeft = [&](float bx_, float by_, float bw_, float bh_) {
        glm::vec2 v0(bx_ + bw_, by_);
        glm::vec2 v1(bx_ + bw_, by_ + bh_);
        glm::vec2 v2(bx_, by_ + bh_ * 0.5f);
        return pointInTriangle(mousePos, v0, v1, v2);
    };

    auto checkTriUp = [&](float bx_, float by_, float bw_, float bh_) {
        glm::vec2 v0(bx_ + bw_ * 0.5f, by_);
        glm::vec2 v1(bx_, by_ + bh_);
        glm::vec2 v2(bx_ + bw_, by_ + bh_);
        return pointInTriangle(mousePos, v0, v1, v2);
    };

    // Directional Triangular Button (0: Point Right ▶, 1: Point Left ◀, 2: Point Up ▲, 3: Point Down ▼)
    auto addTriButton = [&](float x, float y, float w, float h, const std::string& label, bool hovered, bool active, int dir = 0, float fontSize = 13.5f) {
        glm::vec2 v0, v1, v2;
        if (dir == 0) { // Point Right ▶
            v0 = {x, y};
            v1 = {x, y + h};
            v2 = {x + w, y + h * 0.5f};
        } else if (dir == 1) { // Point Left ◀
            v0 = {x + w, y};
            v1 = {x + w, y + h};
            v2 = {x, y + h * 0.5f};
        } else if (dir == 2) { // Point Up ▲
            v0 = {x + w * 0.5f, y};
            v1 = {x, y + h};
            v2 = {x + w, y + h};
        } else { // Point Down ▼
            v0 = {x, y};
            v1 = {x + w, y};
            v2 = {x + w * 0.5f, y + h};
        }

        glm::vec3 bgCol = hovered ? glm::vec3(0.35f, 0.32f, 0.20f) : (active ? glm::vec3(0.18f, 0.20f, 0.28f) : glm::vec3(0.12f, 0.12f, 0.15f));
        addTri(glm::vec3(v0, 0.0f), glm::vec3(v1, 0.0f), glm::vec3(v2, 0.0f), bgCol);

        glm::vec3 lightEdge = hovered ? glm::vec3(1.0f, 0.95f, 0.45f) : (active ? glm::vec3(0.50f, 0.70f, 0.90f) : glm::vec3(0.40f, 0.42f, 0.48f));
        glm::vec3 darkEdge  = hovered ? glm::vec3(0.65f, 0.50f, 0.10f) : glm::vec3(0.08f, 0.09f, 0.11f);
        glm::vec3 baseEdge  = hovered ? glm::vec3(0.85f, 0.75f, 0.25f) : glm::vec3(0.28f, 0.30f, 0.35f);
        float edgeThick = hovered ? 3.0f : 2.0f;

        if (dir == 0) {
            addThickLine(v0, v1, edgeThick, baseEdge);
            addThickLine(v0, v2, edgeThick, lightEdge);
            addThickLine(v1, v2, edgeThick, darkEdge);

            float pipX = x + 16.0f;
            float pipY = y + h * 0.5f;
            float pipS = 4.5f;
            glm::vec3 pipCol = hovered ? glm::vec3(1.0f, 0.92f, 0.25f) : (active ? glm::vec3(0.50f, 0.75f, 0.95f) : glm::vec3(0.55f, 0.58f, 0.65f));
            addTri(glm::vec3(pipX + pipS, pipY, 0.0f),
                   glm::vec3(pipX - pipS * 0.5f, pipY - pipS, 0.0f),
                   glm::vec3(pipX - pipS * 0.5f, pipY + pipS, 0.0f), pipCol);

            float textX = x + 28.0f;
            float textY = y + (h - fontSize) * 0.5f;
            glm::vec3 textCol = hovered ? glm::vec3(1.0f, 1.0f, 0.40f) : (active ? glm::vec3(0.95f, 0.95f, 0.95f) : glm::vec3(0.60f, 0.60f, 0.60f));
            FontRenderer::drawText(vertices, indices, label, textX, textY, fontSize, textCol, true);
        } else if (dir == 1) {
            addThickLine(v0, v1, edgeThick, baseEdge);
            addThickLine(v2, v0, edgeThick, lightEdge);
            addThickLine(v2, v1, edgeThick, darkEdge);

            float textW = FontRenderer::getTextWidth(label, fontSize);
            float textX = x + w - textW - 28.0f;
            float textY = y + (h - fontSize) * 0.5f;
            glm::vec3 textCol = hovered ? glm::vec3(1.0f, 1.0f, 0.40f) : (active ? glm::vec3(0.95f, 0.95f, 0.95f) : glm::vec3(0.60f, 0.60f, 0.60f));
            FontRenderer::drawText(vertices, indices, label, textX, textY, fontSize, textCol, true);

            float pipX = textX - 12.0f;
            float pipY = y + h * 0.5f;
            float pipS = 4.5f;
            glm::vec3 pipCol = hovered ? glm::vec3(1.0f, 0.92f, 0.25f) : (active ? glm::vec3(0.50f, 0.75f, 0.95f) : glm::vec3(0.55f, 0.58f, 0.65f));
            addTri(glm::vec3(pipX - pipS, pipY, 0.0f),
                   glm::vec3(pipX + pipS * 0.5f, pipY + pipS, 0.0f),
                   glm::vec3(pipX + pipS * 0.5f, pipY - pipS, 0.0f), pipCol);
        } else if (dir == 2) { // Point Up ▲
            addThickLine(v1, v2, edgeThick, baseEdge);
            addThickLine(v1, v0, edgeThick, lightEdge);
            addThickLine(v2, v0, edgeThick, darkEdge);

            float textW = FontRenderer::getTextWidth(label, fontSize);
            float textX = x + (w - textW) * 0.5f;
            float textY = y + h * 0.52f;
            glm::vec3 textCol = hovered ? glm::vec3(1.0f, 1.0f, 0.40f) : (active ? glm::vec3(0.95f, 0.95f, 0.95f) : glm::vec3(0.60f, 0.60f, 0.60f));
            FontRenderer::drawText(vertices, indices, label, textX, textY, fontSize, textCol, true);

            float pipX = x + w * 0.5f;
            float pipY = y + 10.0f;
            float pipS = 5.0f;
            glm::vec3 pipCol = hovered ? glm::vec3(1.0f, 0.92f, 0.25f) : (active ? glm::vec3(0.50f, 0.75f, 0.95f) : glm::vec3(0.55f, 0.58f, 0.65f));
            addTri(glm::vec3(pipX, pipY - pipS, 0.0f),
                   glm::vec3(pipX - pipS, pipY + pipS * 0.5f, 0.0f),
                   glm::vec3(pipX + pipS, pipY + pipS * 0.5f, 0.0f), pipCol);
        } else {
            addThickLine(v0, v1, edgeThick, baseEdge);
            addThickLine(v0, v2, edgeThick, lightEdge);
            addThickLine(v1, v2, edgeThick, darkEdge);

            float textW = FontRenderer::getTextWidth(label, fontSize);
            float textX = x + (w - textW) * 0.5f;
            float textY = y + h * 0.25f;
            glm::vec3 textCol = hovered ? glm::vec3(1.0f, 1.0f, 0.40f) : (active ? glm::vec3(0.95f, 0.95f, 0.95f) : glm::vec3(0.60f, 0.60f, 0.60f));
            FontRenderer::drawText(vertices, indices, label, textX, textY, fontSize, textCol, true);
        }
    };

    auto addRectButton = [&](float x, float y, float w, float h, const std::string& label, bool hovered, bool active) {
        addTriButton(x, y, w, h, label, hovered, active, 0);
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
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
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
        glm::vec4 diamondUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::BlockDiamond, 0));
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
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
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
        glm::vec4 diamondUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::BlockDiamond, 0));
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
        FontRenderer::drawText(vertices, indices, statusStr, cx - stW * 0.5f, cy - 80.0f, 16.0f, glm::vec3(0.95f, 0.95f, 0.95f), true);

        // Triangular Progress Vessel (Upright Triangle filling base to apex)
        float triBaseW = 280.0f;
        float triH = 190.0f;
        float apexY = cy - 50.0f;
        float baseY = apexY + triH;

        glm::vec2 tApex(cx, apexY);
        glm::vec2 tLeft(cx - triBaseW * 0.5f, baseY);
        glm::vec2 tRight(cx + triBaseW * 0.5f, baseY);

        // 1. Dark Sunken Background Triangle
        addTri(glm::vec3(tApex, 0.0f), glm::vec3(tLeft, 0.0f), glm::vec3(tRight, 0.0f), glm::vec3(0.08f, 0.08f, 0.11f));

        // 2. Liquid Molten Amber / Gold Progress Fill (fills from baseY upwards to apexY)
        float pct = std::clamp(loadingProgress, 0.0f, 1.0f);
        if (pct > 0.005f) {
            float fillY = baseY - pct * triH;
            float fillW = triBaseW * (1.0f - pct);
            glm::vec2 fLeft(cx - fillW * 0.5f, fillY);
            glm::vec2 fRight(cx + fillW * 0.5f, fillY);

            glm::vec3 fillCol(0.90f, 0.70f, 0.16f);
            glm::vec3 fillHighlight(1.0f, 0.90f, 0.40f);

            // Trapezoid fill from baseY up to fillY
            addTri(glm::vec3(tLeft, 0.0f), glm::vec3(tRight, 0.0f), glm::vec3(fRight, 0.0f), fillCol * 0.85f);
            addTri(glm::vec3(tLeft, 0.0f), glm::vec3(fRight, 0.0f), glm::vec3(fLeft, 0.0f), fillCol);

            // Glowing Molten Horizon line on the liquid surface
            addThickLine(fLeft, fRight, 3.5f, fillHighlight);
        }

        // 3. Thick 3D Beveled Triangular Frame
        glm::vec3 fLight(0.95f, 0.85f, 0.35f);
        glm::vec3 fDark(0.10f, 0.10f, 0.14f);
        glm::vec3 fBase(0.35f, 0.35f, 0.42f);
        addThickLine(tLeft, tApex, 4.0f, fLight);
        addThickLine(tApex, tRight, 4.0f, fDark);
        addThickLine(tRight, tLeft, 4.0f, fBase);

        // Percentage & Chunk Counts inside progress triangle
        char pctBuf[64];
        if (totalChunks > 0) {
            std::snprintf(pctBuf, sizeof(pctBuf), "%d%% (%d / %d CHUNKS)", static_cast<int>(pct * 100.0f), loadedChunks, totalChunks);
        } else {
            std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(pct * 100.0f));
        }
        std::string pctStr(pctBuf);
        float pctW = FontRenderer::getTextWidth(pctStr, 15.0f);
        FontRenderer::drawText(vertices, indices, pctStr, cx - pctW * 0.5f, baseY + 18.0f, 15.0f, glm::vec3(1.0f, 0.95f, 0.40f), true);

        // Bottom Lore / Hints
        std::string tipStr = "Tip: Equilateral and right prisms join seamlessly on a 60-degree honeycomb grid!";
        float tipW = FontRenderer::getTextWidth(tipStr, 12.0f);
        FontRenderer::drawText(vertices, indices, tipStr, cx - tipW * 0.5f, sh - 35.0f, 12.0f, glm::vec3(0.65f, 0.65f, 0.70f), true);

    } else if (state == GameState::Options) {
        // Darkened tiled background
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
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
        float bH = 38.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 145.0f;
        float dy = 44.0f;

        auto inBox = [&](float bx_, float by_, float bw_, float bh_) {
            return (mousePos.x >= bx_ && mousePos.x <= bx_ + bw_ && mousePos.y >= by_ && mousePos.y <= by_ + bh_);
        };

        // Row 0: FOV slider (left) | DEBUG HUD F3 (right ◀)
        float fovRatio = std::clamp((static_cast<float>(options.fov) - 60.0f) / 50.0f, 0.0f, 1.0f);
        char fovBuf[64];
        std::snprintf(fovBuf, sizeof(fovBuf), "FOV: %d DEG", options.fov);
        addSlider(lx, y0, bW, bH, fovRatio, fovBuf, inBox(lx, y0, bW, bH));

        std::string debugStr = options.debugHUD ? "DEBUG HUD [F3]: ON" : "DEBUG HUD [F3]: OFF";
        addTriButton(rx, y0, bW, bH, debugStr, checkTriLeft(rx, y0, bW, bH), options.debugHUD, 1);

        // Row 1: VIDEO SETTINGS... (left ▶) | VIBRANT VISUALS... (right ◀)
        addTriButton(lx, y0 + dy, bW, bH, "VIDEO SETTINGS...", checkTriRight(lx, y0 + dy, bW, bH), true, 0);
        addTriButton(rx, y0 + dy, bW, bH, "VIBRANT VISUALS...", checkTriLeft(rx, y0 + dy, bW, bH), true, 1);

        // Row 2: VOXEL LOD SETTINGS... (left ▶) | MUSIC & SOUNDS... (right ◀)
        addTriButton(lx, y0 + dy * 2.0f, bW, bH, "VOXEL LOD SETTINGS...", checkTriRight(lx, y0 + dy * 2.0f, bW, bH), true, 0);
        addTriButton(rx, y0 + dy * 2.0f, bW, bH, "MUSIC & SOUNDS...", checkTriLeft(rx, y0 + dy * 2.0f, bW, bH), true, 1);

        // Row 3: CONTROLS & KEYBINDS... (left ▶) | LIGHT OVERLAY F7 (right ◀)
        addTriButton(lx, y0 + dy * 3.0f, bW, bH, "CONTROLS & KEYBINDS...", checkTriRight(lx, y0 + dy * 3.0f, bW, bH), true, 0);
        std::string lightStr = options.lightOverlay ? "LIGHT OVERLAY [F7]: ON" : "LIGHT OVERLAY [F7]: OFF";
        addTriButton(rx, y0 + dy * 3.0f, bW, bH, lightStr, checkTriLeft(rx, y0 + dy * 3.0f, bW, bH), options.lightOverlay, 1);

        // Row 4: SHOW FPS (left ▶) | SHOW XYZ (right ◀)
        std::string fpsStr = options.showFPS ? "SHOW FPS: ON" : "SHOW FPS: OFF";
        addTriButton(lx, y0 + dy * 4.0f, bW, bH, fpsStr, checkTriRight(lx, y0 + dy * 4.0f, bW, bH), options.showFPS, 0);
        std::string xyzStr = options.showXYZ ? "SHOW XYZ: ON" : "SHOW XYZ: OFF";
        addTriButton(rx, y0 + dy * 4.0f, bW, bH, xyzStr, checkTriLeft(rx, y0 + dy * 4.0f, bW, bH), options.showXYZ, 1);

        // Row 5: BACK / DONE (center ▲)
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 5.2f;
        addTriButton(doneX, doneY, doneW, doneH, "DONE / BACK", checkTriUp(doneX, doneY, doneW, doneH), true, 2);

        // Footer info
        std::string infoStr = "PrismCraft Engine v1.3 | Triangular Prism Honeycomb Architecture";
        float infoW = FontRenderer::getTextWidth(infoStr, 12.0f);
        FontRenderer::drawText(vertices, indices, infoStr, cx - infoW * 0.5f, sh - 25.0f, 12.0f, glm::vec3(0.65f, 0.65f, 0.70f), true);

    } else if (state == GameState::LODSettings) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.24f));
            }
        }

        // Header "VOXEL LOD & DISTANT HORIZONS"
        float titleBoxW = 460.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 235.0f, titleBoxW, 40.0f, glm::vec3(0.16f, 0.16f, 0.20f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 235.0f, titleBoxW, 2.5f, glm::vec3(0.40f, 0.85f, 0.55f));
        std::string title = "VOXEL LOD & DISTANT HORIZONS";
        float titleW = FontRenderer::getTextWidth(title, 18.0f);
        FontRenderer::drawText(vertices, indices, title, cx - titleW * 0.5f, cy - 224.0f, 18.0f, glm::vec3(1.0f, 0.90f, 0.35f), true);

        float bW = 260.0f;
        float bH = 36.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 180.0f;
        float dy = 42.0f;

        auto inBox = [&](float bx_, float by_, float bw_, float bh_) {
            return (mousePos.x >= bx_ && mousePos.x <= bx_ + bw_ && mousePos.y >= by_ && mousePos.y <= by_ + bh_);
        };

        // Row 0: Active Render Distance slider (left) | Distant Horizons LOD Slider (right)
        float rdRatio = std::clamp((static_cast<float>(options.renderDistance) - 4.0f) / 20.0f, 0.0f, 1.0f);
        char rdBuf[64];
        std::snprintf(rdBuf, sizeof(rdBuf), "ACTIVE CHUNKS: %d", options.renderDistance);
        addSlider(lx, y0, bW, bH, rdRatio, rdBuf, inBox(lx, y0, bW, bH));

        float lodRatio = std::clamp((static_cast<float>(options.lodDistance) - 16.0f) / 240.0f, 0.0f, 1.0f);
        char lodBuf[64];
        std::snprintf(lodBuf, sizeof(lodBuf), "LOD DISTANCE: %d CHUNKS", options.lodDistance);
        addSlider(rx, y0, bW, bH, lodRatio, lodBuf, inBox(rx, y0, bW, bH));

        // Row 1: LOD Quality Preset (left) | LOD Stepping Mode (right)
        static const char* lodPresetNames[] = {"LOD PRESET: PERFORMANCE", "LOD PRESET: BALANCED", "LOD PRESET: QUALITY", "LOD PRESET: EXTREME"};
        addTriButton(lx, y0 + dy, bW, bH, lodPresetNames[std::clamp(options.lodPreset, 0, 3)], checkTriRight(lx, y0 + dy, bW, bH), true, 0);

        std::string stepStr = "VOXEL STEP: 16 -> 8 -> 4 -> 2 -> 1";
        addTriButton(rx, y0 + dy, bW, bH, stepStr, checkTriLeft(rx, y0 + dy, bW, bH), true, 1);

        // Row 2: Thread Pool & Hyper-Threading
        unsigned int hwThreads = std::thread::hardware_concurrency();
        char threadBuf[64];
        std::snprintf(threadBuf, sizeof(threadBuf), "WORKERS: %u (HYPER-THREAD)", (hwThreads > 1) ? (hwThreads - 1) : 1);
        addTriButton(lx, y0 + dy * 2.0f, bW, bH, threadBuf, checkTriRight(lx, y0 + dy * 2.0f, bW, bH), true, 0);

        char fogBuf[64];
        std::snprintf(fogBuf, sizeof(fogBuf), "HORIZON RADIUS: %d M", options.lodDistance * 16);
        addTriButton(rx, y0 + dy * 2.0f, bW, bH, fogBuf, checkTriLeft(rx, y0 + dy * 2.0f, bW, bH), true, 1);

        // Info box describing the 5 LOD Tiers
        float infoBoxW = 550.0f;
        float infoBoxH = 120.0f;
        float infoBoxX = cx - infoBoxW * 0.5f;
        float infoBoxY = y0 + dy * 3.2f;
        addSolidQuad(infoBoxX, infoBoxY, infoBoxW, infoBoxH, glm::vec3(0.10f, 0.10f, 0.14f));
        addSolidQuad(infoBoxX, infoBoxY, infoBoxW, 2.0f, glm::vec3(0.35f, 0.65f, 0.95f));

        FontRenderer::drawText(vertices, indices, "DISTANT HORIZONS 5-TIER VOXEL LOD ARCHITECTURE:", infoBoxX + 12.0f, infoBoxY + 10.0f, 13.0f, glm::vec3(1.0f, 0.90f, 0.35f), true);
        FontRenderer::drawText(vertices, indices, " - LOD 0 [16x16 Full] : Near active terrain & player interaction (0..8 chunks)", infoBoxX + 16.0f, infoBoxY + 28.0f, 11.5f, glm::vec3(0.65f, 0.95f, 0.65f), true);
        FontRenderer::drawText(vertices, indices, " - LOD 1 [8x8 Cells]  : Medium distance smooth transitions (8..24 chunks)", infoBoxX + 16.0f, infoBoxY + 44.0f, 11.5f, glm::vec3(0.85f, 0.95f, 0.65f), true);
        FontRenderer::drawText(vertices, indices, " - LOD 2 [4x4 Cells]  : Mid-horizon terrain contours (24..48 chunks)", infoBoxX + 16.0f, infoBoxY + 60.0f, 11.5f, glm::vec3(0.95f, 0.85f, 0.55f), true);
        FontRenderer::drawText(vertices, indices, " - LOD 3 [2x2 Quads]  : Distant horizon mountain ranges (48..96 chunks)", infoBoxX + 16.0f, infoBoxY + 76.0f, 11.5f, glm::vec3(0.95f, 0.70f, 0.45f), true);
        FontRenderer::drawText(vertices, indices, " - LOD 4 [1x1 Macro]  : Extreme distant skyline voxels (96..256 chunks)", infoBoxX + 16.0f, infoBoxY + 92.0f, 11.5f, glm::vec3(0.95f, 0.50f, 0.45f), true);

        // Done button
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = infoBoxY + infoBoxH + 18.0f;
        addTriButton(doneX, doneY, doneW, doneH, "DONE / BACK", checkTriUp(doneX, doneY, doneW, doneH), true, 2);

    } else if (state == GameState::VibrantVisualsSettings) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.24f));
            }
        }

        // Header "VIBRANT VISUALS & SHADER SUITE"
        float titleBoxW = 460.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 235.0f, titleBoxW, 40.0f, glm::vec3(0.16f, 0.16f, 0.20f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 235.0f, titleBoxW, 2.5f, glm::vec3(0.95f, 0.55f, 0.35f));
        std::string title = "VIBRANT VISUALS & SHADER SUITE";
        float titleW = FontRenderer::getTextWidth(title, 18.0f);
        FontRenderer::drawText(vertices, indices, title, cx - titleW * 0.5f, cy - 224.0f, 18.0f, glm::vec3(1.0f, 0.90f, 0.35f), true);

        float bW = 260.0f;
        float bH = 36.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 180.0f;
        float dy = 42.0f;

        // Row 0: Vibrant Visuals Master toggle | Shaders / Post-FX
        std::string vvStr = options.vibrantVisuals ? "VIBRANT VISUALS: ON" : "VIBRANT VISUALS: OFF";
        addTriButton(lx, y0, bW, bH, vvStr, checkTriRight(lx, y0, bW, bH), options.vibrantVisuals, 0);

        std::string shdStr = options.shadersEnabled ? "SHADERS / POST-FX: ON" : "SHADERS / POST-FX: OFF";
        addTriButton(rx, y0, bW, bH, shdStr, checkTriLeft(rx, y0, bW, bH), options.shadersEnabled, 1);

        // Row 1: Volumetric Clouds | Cloud Shadows
        std::string cldStr = options.clouds ? "VOLUMETRIC CLOUDS: ON" : "VOLUMETRIC CLOUDS: OFF";
        addTriButton(lx, y0 + dy, bW, bH, cldStr, checkTriRight(lx, y0 + dy, bW, bH), options.clouds, 0);

        std::string cshStr = options.cloudShadows ? "CLOUD SHADOWS: ON" : "CLOUD SHADOWS: OFF";
        addTriButton(rx, y0 + dy, bW, bH, cshStr, checkTriLeft(rx, y0 + dy, bW, bH), options.cloudShadows, 1);

        // Row 2: Water Quality | SSAO Ambient Occlusion
        static const char* waterNames[] = {"WATER: FAST", "WATER: FANCY", "WATER: RTX REFLECTION"};
        addTriButton(lx, y0 + dy * 2.0f, bW, bH, waterNames[std::clamp(options.waterQuality, 0, 2)], checkTriRight(lx, y0 + dy * 2.0f, bW, bH), true, 0);

        const char* ssaoStr = (options.aoStrength <= 0.01f) ? "SSAO: OFF" :
                              ((options.aoStrength < 1.0f) ? "SSAO: SUBTLE" :
                              ((options.aoStrength < 1.4f) ? "SSAO: ENHANCED" : "SSAO: ULTRA"));
        addTriButton(rx, y0 + dy * 2.0f, bW, bH, ssaoStr, checkTriLeft(rx, y0 + dy * 2.0f, bW, bH), options.aoStrength > 0.01f, 1);

        // Row 3: Color Grading | Torch Color Bleed
        static const char* gradingNames[] = {"COLOR GRADING: OFF", "COLOR GRADING: CINEMATIC", "COLOR GRADING: VIBRANT", "COLOR GRADING: WARM", "COLOR GRADING: COOL"};
        addTriButton(lx, y0 + dy * 3.0f, bW, bH, gradingNames[std::clamp(options.colorGrading, 0, 4)], checkTriRight(lx, y0 + dy * 3.0f, bW, bH), true, 0);

        std::string torchStr = options.torchColorBleed ? "TORCH COLOR BLEED: ON" : "TORCH COLOR BLEED: OFF";
        addTriButton(rx, y0 + dy * 3.0f, bW, bH, torchStr, checkTriLeft(rx, y0 + dy * 3.0f, bW, bH), options.torchColorBleed, 1);

        // Done button
        float doneW = 320.0f;
        float doneH = 40.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 4.5f;
        addTriButton(doneX, doneY, doneW, doneH, "DONE / BACK", checkTriUp(doneX, doneY, doneW, doneH), true, 2);

        // Footer info
        std::string infoStr = "PrismCraft Engine v1.3 | Triangular Prism Honeycomb Architecture";
        float infoW = FontRenderer::getTextWidth(infoStr, 12.0f);
        FontRenderer::drawText(vertices, indices, infoStr, cx - infoW * 0.5f, sh - 25.0f, 12.0f, glm::vec3(0.65f, 0.65f, 0.70f), true);

    } else if (state == GameState::VideoSettings) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
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
        float bH = 32.0f;
        float lx = cx - 275.0f;
        float rx = cx + 15.0f;
        float y0 = cy - 200.0f;
        float dy = 37.0f;

        auto inBox = [&](float bx_, float by_, float bw_, float bh_) {
            return (mousePos.x >= bx_ && mousePos.x <= bx_ + bw_ && mousePos.y >= by_ && mousePos.y <= by_ + bh_);
        };

        // Row 0: VIBRANT VISUALS (left ▶) | RENDER DISTANCE (SLIDER)
        std::string vibStr = options.vibrantVisuals ? "VIBRANT VISUALS: ON" : "VIBRANT VISUALS: OFF";
        addTriButton(lx, y0, bW, bH, vibStr, checkTriRight(lx, y0, bW, bH), options.vibrantVisuals, 0);

        float rdRatio = std::clamp(static_cast<float>(options.renderDistance - 4) / 44.0f, 0.0f, 1.0f);
        char rdBuf[64];
        std::snprintf(rdBuf, sizeof(rdBuf), "RENDER DISTANCE: %d CHUNKS", options.renderDistance);
        addSlider(rx, y0, bW, bH, rdRatio, rdBuf, inBox(rx, y0, bW, bH));

        // Row 1: MAX FPS (left ▶) | VSYNC (right ◀)
        char fpsBuf[64];
        if (options.maxFps <= 0) std::snprintf(fpsBuf, sizeof(fpsBuf), "MAX FPS: UNLIMITED");
        else std::snprintf(fpsBuf, sizeof(fpsBuf), "MAX FPS: %d FPS", options.maxFps);
        addTriButton(lx, y0 + dy, bW, bH, fpsBuf, checkTriRight(lx, y0 + dy, bW, bH), true, 0);

        std::string vsyncStr = options.vsync ? "VSYNC: ENABLED" : "VSYNC: DISABLED";
        addTriButton(rx, y0 + dy, bW, bH, vsyncStr, checkTriLeft(rx, y0 + dy, bW, bH), options.vsync, 1);

        // Row 2 (Vibrant only): WATER (left ▶) | SHADOW QUALITY (right ◀)
        std::string waterStr = options.vibrantVisuals
            ? (options.waterQuality >= 2 ? "WATER: RTX" : "WATER: VANILLA")
            : "WATER: VANILLA (LOCKED)";
        addTriButton(lx, y0 + dy * 2.0f, bW, bH, waterStr, checkTriRight(lx, y0 + dy * 2.0f, bW, bH), options.vibrantVisuals && (options.waterQuality >= 2), 0);

        const char* shadowNames[] = {"SHADOWS: OFF", "SHADOWS: LOW", "SHADOWS: MEDIUM", "SHADOWS: HIGH"};
        std::string shadowStr = options.vibrantVisuals
            ? shadowNames[std::clamp(options.shadowQuality, 0, 3)]
            : "SHADOWS: BASIC (LOCKED)";
        addTriButton(rx, y0 + dy * 2.0f, bW, bH, shadowStr, checkTriLeft(rx, y0 + dy * 2.0f, bW, bH), options.vibrantVisuals && (options.shadowQuality > 0), 1);

        // Row 3 (Vibrant only): CLOUDS (left ▶) | CLOUD SEED (right ◀)
        std::string cloudStr = options.vibrantVisuals
            ? (options.clouds ? "CLOUDS: VOLUMETRIC 3D" : "CLOUDS: OFF")
            : "CLOUDS: 2D PRISM (LOCKED)";
        addTriButton(lx, y0 + dy * 3.0f, bW, bH, cloudStr, checkTriRight(lx, y0 + dy * 3.0f, bW, bH), options.vibrantVisuals && options.clouds, 0);

        char cSeedBuf[64];
        std::snprintf(cSeedBuf, sizeof(cSeedBuf), "CLOUD SEED: %d", options.cloudSeed);
        std::string seedStr = options.vibrantVisuals ? cSeedBuf : "CLOUD SEED: OFF";
        addTriButton(rx, y0 + dy * 3.0f, bW, bH, seedStr, checkTriLeft(rx, y0 + dy * 3.0f, bW, bH), options.vibrantVisuals, 1);

        // Row 4: CONTACT SHADING (left ▶) | STEVE SHADOW (right ◀)
        std::string aoStr;
        if (!options.smoothLighting || options.aoStrength <= 0.01f) {
            aoStr = "CONTACT SHADING: OFF";
        } else if (options.aoStrength > 1.2f) {
            aoStr = "CONTACT SHADING: ENHANCED";
        } else {
            aoStr = "CONTACT SHADING: REALISTIC";
        }
        addTriButton(lx, y0 + dy * 4.0f, bW, bH, aoStr, checkTriRight(lx, y0 + dy * 4.0f, bW, bH), options.smoothLighting && options.aoStrength > 0.01f, 0);

        std::string pShadowStr = options.vibrantVisuals
            ? (options.playerShadow ? "STEVE SHADOW: ON" : "STEVE SHADOW: OFF")
            : "STEVE SHADOW: OFF (LOCKED)";
        addTriButton(rx, y0 + dy * 4.0f, bW, bH, pShadowStr, checkTriLeft(rx, y0 + dy * 4.0f, bW, bH), options.vibrantVisuals && options.playerShadow, 1);

        // Row 5: LOD PRESET (left ▶) | WINDOW MODE (right ◀)
        const char* lodNames[] = {"LOD: PERFORMANCE", "LOD: BALANCED", "LOD: QUALITY", "LOD: ULTRA"};
        addTriButton(lx, y0 + dy * 5.0f, bW, bH, lodNames[std::clamp(options.lodPreset, 0, 3)], checkTriRight(lx, y0 + dy * 5.0f, bW, bH), true, 0);

        const char* winModes[] = {"MODE: WINDOWED", "MODE: BORDERLESS", "MODE: FULLSCREEN"};
        addTriButton(rx, y0 + dy * 5.0f, bW, bH, winModes[std::clamp(options.windowMode, 0, 2)], checkTriLeft(rx, y0 + dy * 5.0f, bW, bH), true, 1);

        // Row 6: UPSCALER (left ▶) | QUALITY (right ◀)
        const char* upscalerNames[] = {"UPSCALER: OFF (NATIVE)", "UPSCALER: FSR SPATIAL", "UPSCALER: TSR TEMPORAL"};
        std::string upStr = upscalerNames[std::clamp(options.upscalerMode, 0, 2)];
        addTriButton(lx, y0 + dy * 6.0f, bW, bH, upStr, checkTriRight(lx, y0 + dy * 6.0f, bW, bH), options.upscalerMode > 0, 0);

        const char* qualityNames[] = {"QUALITY: ULTRA (77%)", "QUALITY: QUALITY (67%)", "QUALITY: BALANCED (59%)", "QUALITY: PERFORMANCE (50%)"};
        std::string qStr = (options.upscalerMode == 0) ? "UPSCALER QUALITY: N/A" : qualityNames[std::clamp(options.upscalerQuality, 0, 3)];
        addTriButton(rx, y0 + dy * 6.0f, bW, bH, qStr, checkTriLeft(rx, y0 + dy * 6.0f, bW, bH), options.upscalerMode > 0, 1);

        // Row 7: RCAS SHARPNESS (slider, left) | RAM CACHE (right ◀)
        char sharpBuf[64];
        std::snprintf(sharpBuf, sizeof(sharpBuf), "RCAS SHARPNESS: %d%%", static_cast<int>(std::round(options.upscalerSharpness * 100.0f)));
        addSlider(lx, y0 + dy * 7.0f, bW, bH, options.upscalerSharpness, sharpBuf, inBox(lx, y0 + dy * 7.0f, bW, bH));

        const char* ramNames[] = {"RAM CACHE: 512 MB", "RAM CACHE: 1 GB", "RAM CACHE: 2 GB (REC)", "RAM CACHE: 4 GB (HIGH)"};
        std::string ramStr = ramNames[std::clamp(options.ramCacheSize, 0, 3)];
        addTriButton(rx, y0 + dy * 7.0f, bW, bH, ramStr, checkTriLeft(rx, y0 + dy * 7.0f, bW, bH), true, 1);

        // Row 8: UI SCALE (left ▶) | RESOLUTION (right ◀)
        char uiBuf[64];
        std::snprintf(uiBuf, sizeof(uiBuf), "UI SCALE: %.1fx", options.uiScale);
        addTriButton(lx, y0 + dy * 8.0f, bW, bH, uiBuf, checkTriRight(lx, y0 + dy * 8.0f, bW, bH), true, 0);

        const char* resNames[] = {"RENDER: 1280x720", "RENDER: 1600x900", "RENDER: 1920x1080", "RENDER: 2560x1440", "RENDER: NATIVE"};
        addTriButton(rx, y0 + dy * 8.0f, bW, bH, resNames[std::clamp(options.resIndex, 0, 4)], checkTriLeft(rx, y0 + dy * 8.0f, bW, bH), true, 1);

        // Row 9: DONE / BACK (center ▲)
        float doneW = 320.0f;
        float doneH = 36.0f;
        float doneX = cx - doneW * 0.5f;
        float doneY = y0 + dy * 9.0f + 6.0f;
        addTriButton(doneX, doneY, doneW, doneH, "DONE / BACK", checkTriUp(doneX, doneY, doneW, doneH), true, 2);

    } else if (state == GameState::AudioSettings) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
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
        addTriButton(doneX, doneY, doneW, doneH, "DONE", checkTriUp(doneX, doneY, doneW, doneH), true, 2);

    } else if (state == GameState::ControlsSettings) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
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

        addTriButton(rx, y0, bW, bH, "INVERT MOUSE: OFF", checkTriLeft(rx, y0, bW, bH), false, 1);

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
        addTriButton(doneX, doneY, doneW, doneH, "DONE / BACK", checkTriUp(doneX, doneY, doneW, doneH), true, 2);

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

    } else if (state == GameState::WorldSelect) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.28f));
            }
        }

        // Header Title: SELECT WORLD
        float titleBoxW = 460.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 190.0f, titleBoxW, 44.0f, glm::vec3(0.14f, 0.14f, 0.18f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 190.0f, titleBoxW, 3.0f, glm::vec3(0.95f, 0.80f, 0.25f));
        std::string selTitle = "SELECT WORLD";
        float selW = FontRenderer::getTextWidth(selTitle, 24.0f);
        FontRenderer::drawText(vertices, indices, selTitle, cx - selW * 0.5f, cy - 180.0f, 24.0f, glm::vec3(1.0f, 0.85f, 0.20f), true);

        // World Cards List
        float cardW = 500.0f;
        float cardH = 50.0f;
        float listY = cy - 130.0f;
        float listX = cx - cardW * 0.5f;

        if (m_worldList.empty()) {
            std::string emptyStr = "(No saved worlds found. Click 'Create New World' to begin!)";
            float ew = FontRenderer::getTextWidth(emptyStr, 14.0f);
            FontRenderer::drawText(vertices, indices, emptyStr, cx - ew * 0.5f, cy - 40.0f, 14.0f, glm::vec3(0.70f, 0.70f, 0.75f), true);
        } else {
            int maxShown = std::min(4, static_cast<int>(m_worldList.size()));
            for (int i = 0; i < maxShown; ++i) {
                int worldIdx = m_worldListScroll + i;
                if (worldIdx >= static_cast<int>(m_worldList.size())) break;

                const auto& meta = m_worldList[worldIdx];
                float cY = listY + i * (cardH + 8.0f);
                bool isSelected = (worldIdx == m_selectedWorldIndex);
                bool isHovered = (mousePos.x >= listX && mousePos.x <= listX + cardW &&
                                  mousePos.y >= cY && mousePos.y <= cY + cardH);

                glm::vec3 bgCol = isSelected ? glm::vec3(0.18f, 0.18f, 0.25f) : (isHovered ? glm::vec3(0.14f, 0.14f, 0.19f) : glm::vec3(0.10f, 0.10f, 0.14f));
                glm::vec3 borderCol = isSelected ? glm::vec3(0.95f, 0.80f, 0.25f) : (isHovered ? glm::vec3(0.50f, 0.50f, 0.60f) : glm::vec3(0.25f, 0.25f, 0.30f));

                addSolidQuad(listX, cY, cardW, cardH, bgCol);
                addSolidQuad(listX, cY, cardW, 2.0f, borderCol);
                addSolidQuad(listX, cY + cardH - 2.0f, cardW, 2.0f, borderCol);
                addSolidQuad(listX, cY, 2.0f, cardH, borderCol);
                addSolidQuad(listX + cardW - 2.0f, cY, 2.0f, cardH, borderCol);

                // World Name
                FontRenderer::drawText(vertices, indices, meta.name, listX + 14.0f, cY + 8.0f, 16.0f, isSelected ? glm::vec3(1.0f, 0.95f, 0.30f) : glm::vec3(0.95f, 0.95f, 0.95f), true);

                // Subtitle Info
                std::string modeTag = (meta.gameMode == 1) ? "Creative" : "Survival";
                std::string subInfo = meta.folderName + " (" + meta.lastPlayedFormatted + ") - " + modeTag + " - Seed: " + std::to_string(meta.seed);
                glm::vec3 subCol = (meta.gameMode == 1) ? glm::vec3(0.40f, 0.85f, 1.0f) : glm::vec3(0.85f, 0.85f, 0.40f);
                FontRenderer::drawText(vertices, indices, subInfo, listX + 14.0f, cY + 28.0f, 11.5f, subCol, true);
            }
        }

        // Action Buttons:
        // Row 1: [PLAY SELECTED WORLD] | [CREATE NEW WORLD]
        float bW = 240.0f;
        float bH = 38.0f;
        float bY1 = cy + 115.0f;
        float bY2 = cy + 165.0f;

        // [PLAY SELECTED WORLD]
        glm::vec2 p1_0(cx - 250.0f, bY1), p1_1(cx - 250.0f, bY1 + bH), p1_2(cx - 250.0f + bW, bY1 + bH * 0.5f);
        bool h1 = pointInTriangle(mousePos, p1_0, p1_1, p1_2);
        addPrismButton(p1_0, p1_1, p1_2, "PLAY SELECTED WORLD", h1, true, 14.0f);

        // [CREATE NEW WORLD]
        glm::vec2 p2_0(cx + 10.0f, bY1), p2_1(cx + 10.0f, bY1 + bH), p2_2(cx + 10.0f + bW, bY1 + bH * 0.5f);
        bool h2 = pointInTriangle(mousePos, p2_0, p2_1, p2_2);
        addPrismButton(p2_0, p2_1, p2_2, "CREATE NEW WORLD", h2, true, 14.0f);

        // [DELETE WORLD]
        glm::vec2 p3_0(cx - 250.0f + bW, bY2), p3_1(cx - 250.0f + bW, bY2 + bH), p3_2(cx - 250.0f, bY2 + bH * 0.5f);
        bool h3 = pointInTriangle(mousePos, p3_0, p3_1, p3_2);
        addPrismButton(p3_0, p3_1, p3_2, "DELETE WORLD", h3, false, 14.0f);

        // [BACK TO TITLE]
        glm::vec2 p4_0(cx + 10.0f + bW, bY2), p4_1(cx + 10.0f + bW, bY2 + bH), p4_2(cx + 10.0f, bY2 + bH * 0.5f);
        bool h4 = pointInTriangle(mousePos, p4_0, p4_1, p4_2);
        addPrismButton(p4_0, p4_1, p4_2, "BACK TO TITLE", h4, false, 14.0f);

    } else if (state == GameState::WorldCreation) {
        glm::vec4 dirtUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Dirt, 0));
        for (float by = 0; by < sh; by += 48.0f) {
            for (float bpx = 0; bpx < sw; bpx += 48.0f) {
                addTexturedQuad(bpx, by, 48.0f, 48.0f, dirtUV, glm::vec3(0.32f));
            }
        }

        // Header Title: CREATE NEW WORLD
        float titleBoxW = 440.0f;
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 180.0f, titleBoxW, 44.0f, glm::vec3(0.16f, 0.16f, 0.20f));
        addSolidQuad(cx - titleBoxW * 0.5f, cy - 180.0f, titleBoxW, 3.0f, glm::vec3(0.95f, 0.80f, 0.25f));
        std::string genTitle = "CREATE NEW WORLD";
        float genW = FontRenderer::getTextWidth(genTitle, 24.0f);
        FontRenderer::drawText(vertices, indices, genTitle, cx - genW * 0.5f, cy - 170.0f, 24.0f, glm::vec3(1.0f, 0.85f, 0.20f), true);

        // Field 1: World Name Box
        float boxW = 340.0f;
        float boxH = 36.0f;
        float boxX = cx - boxW * 0.5f;

        FontRenderer::drawText(vertices, indices, "World Name:", boxX, cy - 90.0f, 13.5f, glm::vec3(0.85f, 0.85f, 0.85f), true);
        glm::vec3 nameBorder = (m_creationFieldFocus == 0) ? glm::vec3(0.40f, 0.85f, 1.0f) : glm::vec3(0.30f, 0.30f, 0.35f);
        addSolidQuad(boxX, cy - 70.0f, boxW, boxH, glm::vec3(0.12f, 0.12f, 0.15f));
        addSolidQuad(boxX, cy - 70.0f, boxW, 2.0f, nameBorder);
        addSolidQuad(boxX, cy - 70.0f + boxH - 2.0f, boxW, 2.0f, nameBorder);
        addSolidQuad(boxX, cy - 70.0f, 2.0f, boxH, nameBorder);
        addSolidQuad(boxX + boxW - 2.0f, cy - 70.0f, 2.0f, boxH, nameBorder);
        std::string displayName = m_newWorldName + ((m_creationFieldFocus == 0) ? "_" : "");
        FontRenderer::drawText(vertices, indices, displayName, boxX + 10.0f, cy - 58.0f, 14.5f, glm::vec3(1.0f, 1.0f, 1.0f), true);

        // Field 2: Seed Box
        FontRenderer::drawText(vertices, indices, "Seed (leave blank for random):", boxX, cy - 26.0f, 13.5f, glm::vec3(0.85f, 0.85f, 0.85f), true);
        glm::vec3 seedBorder = (m_creationFieldFocus == 1) ? glm::vec3(0.40f, 0.85f, 1.0f) : glm::vec3(0.30f, 0.30f, 0.35f);
        addSolidQuad(boxX, cy - 6.0f, boxW, boxH, glm::vec3(0.12f, 0.12f, 0.15f));
        addSolidQuad(boxX, cy - 6.0f, boxW, 2.0f, seedBorder);
        addSolidQuad(boxX, cy - 6.0f + boxH - 2.0f, boxW, 2.0f, seedBorder);
        addSolidQuad(boxX, cy - 6.0f, 2.0f, boxH, seedBorder);
        addSolidQuad(boxX + boxW - 2.0f, cy - 6.0f, 2.0f, boxH, seedBorder);
        std::string displaySeed = m_newWorldSeedStr.empty() ? (m_creationFieldFocus == 1 ? "_" : "[ Random ]") : (m_newWorldSeedStr + (m_creationFieldFocus == 1 ? "_" : ""));
        glm::vec3 seedCol = m_newWorldSeedStr.empty() ? glm::vec3(0.55f, 0.55f, 0.60f) : glm::vec3(0.40f, 1.0f, 0.40f);
        FontRenderer::drawText(vertices, indices, displaySeed, boxX + 10.0f, cy + 6.0f, 14.5f, seedCol, true);

        // Button: RANDOMIZE SEED
        float btnW3 = 320.0f;
        float btnH3 = 34.0f;
        glm::vec2 p1_0(cx - 160.0f, cy + 35.0f), p1_1(cx - 160.0f, cy + 35.0f + btnH3), p1_2(cx - 160.0f + btnW3, cy + 35.0f + btnH3 * 0.5f);
        bool h1 = pointInTriangle(mousePos, p1_0, p1_1, p1_2);
        addPrismButton(p1_0, p1_1, p1_2, "RANDOMIZE SEED", h1, true, 14.0f);

        // Button: MODE: SURVIVAL / CREATIVE
        glm::vec2 p2_0(cx - 160.0f, cy + 78.0f), p2_1(cx - 160.0f, cy + 78.0f + btnH3), p2_2(cx - 160.0f + btnW3, cy + 78.0f + btnH3 * 0.5f);
        bool h2 = pointInTriangle(mousePos, p2_0, p2_1, p2_2);
        std::string modeStr = m_startInCreative ? "MODE: CREATIVE" : "MODE: SURVIVAL";
        addPrismButton(p2_0, p2_1, p2_2, modeStr, h2, true, 14.0f);

        // Bottom row: [CREATE WORLD] | [CANCEL]
        float bW_bot = 210.0f;
        float bH_bot = 38.0f;
        glm::vec2 p3_0(cx - 220.0f, cy + 130.0f), p3_1(cx - 220.0f, cy + 130.0f + bH_bot), p3_2(cx - 220.0f + bW_bot, cy + 130.0f + bH_bot * 0.5f);
        bool h3 = pointInTriangle(mousePos, p3_0, p3_1, p3_2);
        addPrismButton(p3_0, p3_1, p3_2, "CREATE WORLD", h3, true, 14.0f);

        glm::vec2 p4_0(cx + 10.0f + bW_bot, cy + 130.0f), p4_1(cx + 10.0f + bW_bot, cy + 130.0f + bH_bot), p4_2(cx + 10.0f, cy + 130.0f + bH_bot * 0.5f);
        bool h4 = pointInTriangle(mousePos, p4_0, p4_1, p4_2);
        addPrismButton(p4_0, p4_1, p4_2, "CANCEL", h4, false, 14.0f);

    } else if (state == GameState::Inventory || state == GameState::CraftingTable) {
        // Transparent Dark Tint overlay behind GUI dialog
        glm::vec4 darkTintUV = TextureAtlas::getTileUV(TextureAtlas::TILE_TINT_DARK);
        addTexturedQuad(0.0f, 0.0f, sw, sh, darkTintUV, glm::vec3(1.0f));

        // 3D Isometric Triangular Prism & 2.5D Item Icon Renderer
        auto draw3DItemIcon = [&](float cx, float cy, float s, BlockType item) {
            if (item == BlockType::Air) return;

            if (Cell{item}.isFlatItem()) {
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
                vertices.push_back({glm::vec3(tRL, 0.0f), glm::vec2(uvTop.x, uvTop.y), norm, cTop});
                vertices.push_back({glm::vec3(tRR, 0.0f), glm::vec2(uvTop.z, uvTop.y), norm, cTop});
                vertices.push_back({glm::vec3(tFC, 0.0f), glm::vec2(uvTop.x, uvTop.w), norm, cTop});
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
        std::string hoveredTooltip = "";

        if (state == GameState::Inventory && player.isCreative()) {
            float panW = 440.0f;
            float panH = 370.0f;
            float px = (sw - panW) * 0.5f;
            float py = (sh - panH) * 0.5f;

            // Gray dialog base (Classic Minecraft Light Gray)
            addSolidQuad(px, py, panW, panH, glm::vec3(0.776f, 0.776f, 0.776f));
            addSolidQuad(px, py, panW, 3.5f, glm::vec3(1.0f));
            addSolidQuad(px, py, 3.5f, panH, glm::vec3(1.0f));
            addSolidQuad(px, py + panH - 3.5f, panW, 3.5f, glm::vec3(0.25f, 0.25f, 0.28f));
            addSolidQuad(px + panW - 3.5f, py, 3.5f, panH, glm::vec3(0.25f, 0.25f, 0.28f));

            // Title
            FontRenderer::drawText(vertices, indices, "CREATIVE INVENTORY", px + 14.0f, py + 8.0f, 11.0f, glm::vec3(0.25f, 0.25f, 0.28f), false);

            // Search Bar
            float searchX = px + 14.0f;
            float searchY = py + 24.0f;
            float searchW = 346.0f;
            float searchH = 22.0f;
            addSolidQuad(searchX, searchY, searchW, searchH, glm::vec3(0.12f, 0.12f, 0.14f));
            addSolidQuad(searchX, searchY, searchW, 1.5f, glm::vec3(0.25f, 0.25f, 0.28f));
            addSolidQuad(searchX, searchY, 1.5f, searchH, glm::vec3(0.25f, 0.25f, 0.28f));
            addSolidQuad(searchX, searchY + searchH - 1.5f, searchW, 1.5f, glm::vec3(0.9f, 0.9f, 0.9f));
            addSolidQuad(searchX + searchW - 1.5f, searchY, 1.5f, searchH, glm::vec3(0.9f, 0.9f, 0.9f));

            if (m_creativeSearchQuery.empty()) {
                FontRenderer::drawText(vertices, indices, "Search items...", searchX + 6.0f, searchY + 5.0f, 11.0f, glm::vec3(0.55f, 0.55f, 0.58f), false);
            } else {
                std::string disp = m_creativeSearchQuery + "_";
                FontRenderer::drawText(vertices, indices, disp, searchX + 6.0f, searchY + 5.0f, 11.0f, glm::vec3(1.0f, 1.0f, 1.0f), false);
            }

            // Clear button ('X')
            float clearX = searchX + searchW + 8.0f;
            float clearY = searchY;
            float clearW = 22.0f; float clearH = 22.0f;
            bool clearHov = (mousePos.x >= clearX && mousePos.x <= clearX + clearW && mousePos.y >= clearY && mousePos.y <= clearY + clearH);
            addSolidQuad(clearX, clearY, clearW, clearH, clearHov ? glm::vec3(0.85f, 0.35f, 0.35f) : glm::vec3(0.70f, 0.25f, 0.25f));
            addSolidQuad(clearX, clearY, clearW, 1.5f, glm::vec3(1.0f));
            addSolidQuad(clearX, clearY, 1.5f, clearH, glm::vec3(1.0f));
            addSolidQuad(clearX, clearY + clearH - 1.5f, clearW, 1.5f, glm::vec3(0.35f, 0.1f, 0.1f));
            addSolidQuad(clearX + clearW - 1.5f, clearY, 1.5f, clearH, glm::vec3(0.35f, 0.1f, 0.1f));
            FontRenderer::drawText(vertices, indices, "X", clearX + 7.0f, clearY + 5.0f, 11.0f, glm::vec3(1.0f), true);

            // Catalog Grid
            std::vector<BlockType> catalog = getCreativeCatalog(m_creativeSearchQuery);
            int totalRows = (static_cast<int>(catalog.size()) + 8) / 9;
            int maxScroll = std::max(0, totalRows - 5);
            if (m_creativeScrollRow > maxScroll) m_creativeScrollRow = maxScroll;
            if (m_creativeScrollRow < 0) m_creativeScrollRow = 0;

            float gridX = px + 14.0f;
            float gridY = py + 52.0f;
            float slotStep = 38.0f;
            float slotW = 34.0f;
            float slotH = 34.0f;

            for (int r = 0; r < 5; ++r) {
                for (int c = 0; c < 9; ++c) {
                    int idx = (m_creativeScrollRow + r) * 9 + c;
                    float sx = gridX + c * slotStep;
                    float sy = gridY + r * slotStep;
                    bool isHovered = (mousePos.x >= sx && mousePos.x <= sx + slotW && mousePos.y >= sy && mousePos.y <= sy + slotH);

                    glm::vec3 wellCol = isHovered ? glm::vec3(0.66f, 0.66f, 0.70f) : glm::vec3(0.55f, 0.55f, 0.57f);
                    addSolidQuad(sx, sy, slotW, slotH, wellCol);
                    addSolidQuad(sx, sy, slotW, 1.5f, glm::vec3(0.25f, 0.25f, 0.28f));
                    addSolidQuad(sx, sy, 1.5f, slotH, glm::vec3(0.25f, 0.25f, 0.28f));
                    addSolidQuad(sx, sy + slotH - 1.5f, slotW, 1.5f, glm::vec3(1.0f));
                    addSolidQuad(sx + slotW - 1.5f, sy, 1.5f, slotH, glm::vec3(1.0f));

                    if (idx < static_cast<int>(catalog.size())) {
                        BlockType it = catalog[idx];
                        draw3DItemIcon(sx + slotW * 0.5f, sy + slotH * 0.5f, 26.0f, it);
                        if (isHovered) {
                            hoveredItem = it;
                        }
                    }
                }
            }

            // Scrollbar
            float sbX = gridX + 9.0f * slotStep + 6.0f;
            float sbY = gridY;
            float sbW = 16.0f;
            float sbH = 5.0f * slotStep - 4.0f;
            addSolidQuad(sbX, sbY, sbW, sbH, glm::vec3(0.38f, 0.38f, 0.40f));
            addSolidQuad(sbX, sbY, sbW, 1.5f, glm::vec3(0.20f, 0.20f, 0.22f));
            addSolidQuad(sbX, sbY, 1.5f, sbH, glm::vec3(0.20f, 0.20f, 0.22f));
            addSolidQuad(sbX, sbY + sbH - 1.5f, sbW, 1.5f, glm::vec3(0.60f, 0.60f, 0.62f));
            addSolidQuad(sbX + sbW - 1.5f, sbY, 1.5f, sbH, glm::vec3(0.60f, 0.60f, 0.62f));

            if (maxScroll > 0) {
                float thumbH = std::max(22.0f, sbH * (5.0f / totalRows));
                float thumbY = sbY + (sbH - thumbH) * (static_cast<float>(m_creativeScrollRow) / maxScroll);
                addSolidQuad(sbX + 1.0f, thumbY, sbW - 2.0f, thumbH, glm::vec3(0.78f, 0.78f, 0.78f));
                addSolidQuad(sbX + 1.0f, thumbY, sbW - 2.0f, 1.5f, glm::vec3(1.0f));
                addSolidQuad(sbX + 1.0f, thumbY, 1.5f, thumbH, glm::vec3(1.0f));
                addSolidQuad(sbX + 1.0f, thumbY + thumbH - 1.5f, sbW - 2.0f, 1.5f, glm::vec3(0.35f, 0.35f, 0.38f));
                addSolidQuad(sbX + sbW - 2.5f, thumbY, 1.5f, thumbH, glm::vec3(0.35f, 0.35f, 0.38f));
            }

            // Hotbar label & slots
            FontRenderer::drawText(vertices, indices, "HOTBAR", gridX, py + 250.0f, 10.0f, glm::vec3(0.35f, 0.35f, 0.38f), false);
            float hbX = gridX;
            float hbY = py + 266.0f;
            float hbStep = 35.0f;
            float hbW = 32.0f; float hbH = 32.0f;

            for (int i = 0; i < 10; ++i) {
                float hsx = hbX + i * hbStep;
                float hsy = hbY;
                bool isHovered = (mousePos.x >= hsx && mousePos.x <= hsx + hbW && mousePos.y >= hsy && mousePos.y <= hsy + hbH);
                bool isSelected = (i == player.getSelectedSlot());

                glm::vec3 wellCol = isHovered ? glm::vec3(0.66f, 0.66f, 0.70f) : glm::vec3(0.55f, 0.55f, 0.57f);
                addSolidQuad(hsx, hsy, hbW, hbH, wellCol);
                addSolidQuad(hsx, hsy, hbW, 1.5f, glm::vec3(0.25f, 0.25f, 0.28f));
                addSolidQuad(hsx, hsy, 1.5f, hbH, glm::vec3(0.25f, 0.25f, 0.28f));
                addSolidQuad(hsx, hsy + hbH - 1.5f, hbW, 1.5f, glm::vec3(1.0f));
                addSolidQuad(hsx + hbW - 1.5f, hsy, 1.5f, hbH, glm::vec3(1.0f));

                if (isSelected) {
                    glm::vec3 gold(1.0f, 0.90f, 0.20f);
                    addSolidQuad(hsx - 1.0f, hsy - 1.0f, hbW + 2.0f, 2.0f, gold);
                    addSolidQuad(hsx - 1.0f, hsy - 1.0f, 2.0f, hbH + 2.0f, gold);
                    addSolidQuad(hsx - 1.0f, hsy + hbH - 1.0f, hbW + 2.0f, 2.0f, gold);
                    addSolidQuad(hsx + hbW - 1.0f, hsy - 1.0f, 2.0f, hbH + 2.0f, gold);
                }

                BlockType it = player.getHotbar()[i];
                int cnt = player.getHotbarCounts()[i];
                if (it != BlockType::Air) {
                    draw3DItemIcon(hsx + hbW * 0.5f, hsy + hbH * 0.5f, 24.0f, it);
                    if (cnt > 1) {
                        char cBuf[16]; std::snprintf(cBuf, sizeof(cBuf), "%d", cnt);
                        float fontSize = 9.0f;
                        float textW = FontRenderer::getTextWidth(cBuf, fontSize);
                        FontRenderer::drawText(vertices, indices, cBuf, hsx + hbW - textW - 2.0f, hsy + hbH - fontSize - 2.0f, fontSize, glm::vec3(1.0f), true);
                    }
                    if (isHovered) hoveredItem = it;
                }
            }

            // Trash Slot (Destroy Item)
            float trashX = px + panW - 48.0f;
            float trashY = hbY;
            float trashW = 34.0f; float trashH = 32.0f;
            bool isTrashHov = (mousePos.x >= trashX && mousePos.x <= trashX + trashW && mousePos.y >= trashY && mousePos.y <= trashY + trashH);

            addSolidQuad(trashX, trashY, trashW, trashH, isTrashHov ? glm::vec3(0.75f, 0.35f, 0.35f) : glm::vec3(0.60f, 0.28f, 0.28f));
            addSolidQuad(trashX, trashY, trashW, 1.5f, glm::vec3(0.35f, 0.12f, 0.12f));
            addSolidQuad(trashX, trashY, 1.5f, trashH, glm::vec3(0.35f, 0.12f, 0.12f));
            addSolidQuad(trashX, trashY + trashH - 1.5f, trashW, 1.5f, glm::vec3(0.95f, 0.55f, 0.55f));
            addSolidQuad(trashX + trashW - 1.5f, trashY, 1.5f, trashH, glm::vec3(0.95f, 0.55f, 0.55f));

            float xW = FontRenderer::getTextWidth("X", 16.0f);
            FontRenderer::drawText(vertices, indices, "X", trashX + (trashW - xW) * 0.5f, trashY + 8.0f, 16.0f, glm::vec3(1.0f, 0.25f, 0.25f), true);

            if (isTrashHov) {
                hoveredTooltip = "Destroy Item (Shift-Click: Clear All)";
            }
        } else {
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
        if (!hoveredTooltip.empty()) {
            float textW = FontRenderer::getTextWidth(hoveredTooltip, 12.0f);
            float padX = 7.0f;
            float padY = 5.0f;
            float boxW = textW + padX * 2.0f;
            float boxH = 12.0f + padY * 2.0f;

            float tx = mousePos.x + 12.0f;
            float ty = mousePos.y - 14.0f;

            if (tx + boxW > sw - 4.0f) tx = sw - boxW - 4.0f;
            if (ty < 4.0f) ty = 4.0f;

            addSolidQuad(tx, ty, boxW, boxH, glm::vec3(0.06f, 0.0f, 0.08f));
            addSolidQuad(tx - 1.0f, ty, 1.0f, boxH, glm::vec3(0.60f, 0.10f, 0.15f));
            addSolidQuad(tx + boxW, ty, 1.0f, boxH, glm::vec3(0.60f, 0.10f, 0.15f));
            addSolidQuad(tx, ty - 1.0f, boxW, 1.0f, glm::vec3(0.85f, 0.20f, 0.25f));
            addSolidQuad(tx, ty + boxH, boxW, 1.0f, glm::vec3(0.45f, 0.05f, 0.10f));

            FontRenderer::drawText(vertices, indices, hoveredTooltip, tx + padX, ty + padY, 12.0f, glm::vec3(1.0f, 0.90f, 0.90f), true);
        } else if (hoveredItem != BlockType::Air && m_cursorItem == BlockType::Air) {
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
        glm::vec4 redTintUV = TextureAtlas::getTileUV(TextureAtlas::TILE_TINT_RED);
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

    if (vSize > m_vbo[f].getSize()) {
        VkDeviceSize newSize = std::max(vSize * 2, static_cast<VkDeviceSize>(MAX_MENU_VBO_SIZE));
        m_vbo[f] = Buffer(m_context, newSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    if (iSize > m_ibo[f].getSize()) {
        VkDeviceSize newSize = std::max(iSize * 2, static_cast<VkDeviceSize>(MAX_MENU_IBO_SIZE));
        m_ibo[f] = Buffer(m_context, newSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    m_vbo[f].upload(vertices.data(), vSize);
    m_ibo[f].upload(indices.data(), iSize);

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
                          int totalChunks,
                          VkDescriptorSet descSet) {
    if (state == GameState::Playing || screenWidth == 0 || screenHeight == 0) return;

    rebuildMenuMesh(state, player, screenWidth, screenHeight, mousePos, options, loadingProgress, loadedChunks, totalChunks);

    uint32_t f = m_cmdQueue.getCurrentFrame();
    if (m_indexCount[f] == 0 || !m_vbo[f].isValid()) return;

    uiPipeline.bind(cmd);
    if (descSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline.getLayout(), 0, 1, &descSet, 0, nullptr);
    }

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
    if (player.isCreative()) {
        m_cursorItem = BlockType::Air;
        m_cursorCount = 0;
        return;
    }
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

void MenuRenderer::handleCreativeChar(char c) {
    if (m_creativeSearchQuery.size() < 32 && (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '_' || c == '-')) {
        m_creativeSearchQuery += c;
        m_creativeScrollRow = 0;
    }
}

void MenuRenderer::handleCreativeBackspace() {
    if (!m_creativeSearchQuery.empty()) {
        m_creativeSearchQuery.pop_back();
        m_creativeScrollRow = 0;
    }
}

void MenuRenderer::handleCreativeScroll(int delta) {
    m_creativeScrollRow += delta;
    if (m_creativeScrollRow < 0) m_creativeScrollRow = 0;
}

void MenuRenderer::clearCreativeSearch() {
    m_creativeSearchQuery.clear();
    m_creativeScrollRow = 0;
}

std::vector<BlockType> MenuRenderer::getCreativeCatalog(const std::string& query) const {
    std::vector<BlockType> items;
    items.reserve(180);

    std::string lowerQuery = query;
    for (char& c : lowerQuery) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    for (int t = 1; t < static_cast<int>(BlockType::COUNT); ++t) {
        BlockType b = static_cast<BlockType>(t);
        std::string_view nameView = getItemDisplayName(b);
        if (nameView.empty()) continue;

        if (lowerQuery.empty()) {
            items.push_back(b);
        } else {
            std::string lowerName(nameView);
            for (char& c : lowerName) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lowerName.find(lowerQuery) != std::string::npos) {
                items.push_back(b);
            }
        }
    }
    return items;
}

void MenuRenderer::refreshWorldList() {
    m_worldList = SaveManager::listWorlds();
    if (m_selectedWorldIndex >= static_cast<int>(m_worldList.size())) {
        m_selectedWorldIndex = std::max(0, static_cast<int>(m_worldList.size()) - 1);
    }
}

void MenuRenderer::handleWorldCreationChar(char c) {
    if (m_creationFieldFocus == 0) {
        if (m_newWorldName.size() < 24 && (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '_' || c == '-')) {
            m_newWorldName += c;
        }
    } else if (m_creationFieldFocus == 1) {
        if (m_newWorldSeedStr.size() < 10 && std::isdigit(static_cast<unsigned char>(c))) {
            m_newWorldSeedStr += c;
        }
    }
}

void MenuRenderer::handleWorldCreationBackspace() {
    if (m_creationFieldFocus == 0) {
        if (!m_newWorldName.empty()) m_newWorldName.pop_back();
    } else if (m_creationFieldFocus == 1) {
        if (!m_newWorldSeedStr.empty()) m_newWorldSeedStr.pop_back();
    }
}

void MenuRenderer::handleWorldListScroll(int delta) {
    m_worldListScroll = std::max(0, m_worldListScroll - delta);
}

} // namespace prismcraft

