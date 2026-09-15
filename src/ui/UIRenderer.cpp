#include "UIRenderer.hpp"
#include "MenuRenderer.hpp"
#include "world/World.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "player/Player.hpp"
#include "world/ChunkMesher.hpp"
#include "world/Biome.hpp"
#include "world/TerrainGen.hpp"
#include "renderer/TextureAtlas.hpp"
#include "ui/FontData.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <string>

namespace prismcraft {

UIRenderer::UIRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
    buildCrosshairMesh();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_uiVertexBuffer[i] = Buffer(m_context, MAX_UI_VBO_SIZE,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        m_uiIndexBuffer[i] = Buffer(m_context, MAX_UI_IBO_SIZE,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        m_uiIndexCount[i] = 0;
    }
}

void UIRenderer::buildCrosshairMesh() {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);
    glm::vec3 c(1.0f, 1.0f, 1.0f); // Pure white for mathematical negative color inversion (ONE_MINUS_DST_COLOR)

    auto addLineQuad = [&](const glm::vec2& p0, const glm::vec2& p1, float thickness) {
        glm::vec2 diff = p1 - p0;
        float len = glm::length(diff);
        if (len < 0.001f) return;
        glm::vec2 dir = diff / len;
        glm::vec2 perp(-dir.y, dir.x);
        glm::vec2 h = perp * (thickness * 0.5f);

        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 norm(0.0f, 0.0f, 1.0f);
        vertices.push_back({glm::vec3(p0 - h, 0.0f), glm::vec2(uv.x, uv.w), norm, c});
        vertices.push_back({glm::vec3(p1 - h, 0.0f), glm::vec2(uv.z, uv.w), norm, c});
        vertices.push_back({glm::vec3(p1 + h, 0.0f), glm::vec2(uv.z, uv.y), norm, c});
        vertices.push_back({glm::vec3(p0 + h, 0.0f), glm::vec2(uv.x, uv.y), norm, c});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    // Thematic equilateral 3-spoke star matching reference image media_1788876421683.png:
    // Radiating at:
    // Spoke 1: 90 deg (Up: (0, -1) in screen space)
    // Spoke 2: 210 deg (Down-Left: (-sqrt(3)/2, 0.5))
    // Spoke 3: 330 deg (Down-Right: (sqrt(3)/2, 0.5))
    float rInner = 3.5f;
    float rOuter = 11.5f;
    float thick = 2.0f;

    glm::vec2 dir1(0.0f, -1.0f);
    glm::vec2 dir2(-0.8660254f, 0.5f);
    glm::vec2 dir3(0.8660254f, 0.5f);

    // 3 Radial Spokes
    addLineQuad(dir1 * rInner, dir1 * rOuter, thick);
    addLineQuad(dir2 * rInner, dir2 * rOuter, thick);
    addLineQuad(dir3 * rInner, dir3 * rOuter, thick);

    // Central hollow triangular hub
    addLineQuad(dir1 * rInner, dir2 * rInner, thick * 0.85f);
    addLineQuad(dir2 * rInner, dir3 * rInner, thick * 0.85f);
    addLineQuad(dir3 * rInner, dir1 * rInner, thick * 0.85f);

    m_chIndexCount = static_cast<uint32_t>(indices.size());
    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_chVbo = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_chVbo.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_chIbo = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_chIbo.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
}

void UIRenderer::updateDynamicUI(uint32_t f,
                                 const Player& player, uint32_t screenWidth, uint32_t screenHeight, float fps,
                                 const GameOptions& options, const World* world,
                                 bool isChatOpen, const std::string& chatInput,
                                 const std::string& feedbackMsg, float feedbackTimer,
                                 float sleepFadeAlpha) {
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
        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE); // Solid white tile
        addTexturedQuad(x, y, w, h, uv, color);
    };

    auto addTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& uv, const glm::vec3& color) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 norm(0.0f, 0.0f, 1.0f);
        vertices.push_back({v0, glm::vec2(uv.x, uv.y), norm, color});
        vertices.push_back({v1, glm::vec2(uv.x, uv.w), norm, color});
        vertices.push_back({v2, glm::vec2(uv.z, uv.y), norm, color});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    const glm::vec3& pos = player.getPosition();
    CellCoord curCell = worldToCell(pos);
    BlockType sel = player.getSelectedBlock();
    std::string bName(getItemDisplayName(sel));

    // 0. Top-Left HUD (Standard compact or F3 Full Debug HUD)
    float hudX = 12.0f;
    float hudY = 12.0f;

    if (options.debugHUD) {
        float fontSize = 13.5f;
        float lineH = 16.0f;

        // F3 Dark semi-transparent background box
        addSolidQuad(hudX - 4.0f, hudY - 4.0f, 320.0f, lineH * 7.5f, glm::vec3(0.08f, 0.08f, 0.10f));

        // Line 1: Build & FPS
        char l1[128];
        std::snprintf(l1, sizeof(l1), "PrismCraft v0.1.0 (%d FPS)", static_cast<int>(fps));
        FontRenderer::drawText(vertices, indices, l1, hudX, hudY, fontSize, glm::vec3(1.0f, 0.95f, 0.35f), true);

        // Line 2: XYZ Coordinates
        char l2[128];
        std::snprintf(l2, sizeof(l2), "XYZ: %.3f / %.3f / %.3f", pos.x, pos.y, pos.z);
        FontRenderer::drawText(vertices, indices, l2, hudX, hudY + lineH * 1.0f, fontSize, glm::vec3(1.0f, 1.0f, 1.0f), true);

        // Line 3: Block & Subprism
        char l3[128];
        std::snprintf(l3, sizeof(l3), "Block: %d %d %d [s=%d] (%s)", curCell.x, curCell.y, curCell.z, curCell.s, bName.empty() ? "Air" : bName.c_str());
        FontRenderer::drawText(vertices, indices, l3, hudX, hudY + lineH * 2.0f, fontSize, glm::vec3(0.45f, 1.0f, 0.45f), true);

        // Line 4: Chunk Coordinates
        ChunkCoord chk = worldToChunk(curCell.x, curCell.z);
        int lx, lz;
        worldToLocal(curCell.x, curCell.z, lx, lz);
        char l4[128];
        std::snprintf(l4, sizeof(l4), "Chunk: [%d, %d] in [%d, %d]", chk.cx, chk.cz, lx, lz);
        FontRenderer::drawText(vertices, indices, l4, hudX, hudY + lineH * 3.0f, fontSize, glm::vec3(0.85f, 0.85f, 0.85f), true);

        // Line 5: Facing direction
        glm::vec3 fwd = player.getCamera().getForward();
        const char* facingStr = "North (Towards -Z)";
        if (std::abs(fwd.x) > std::abs(fwd.z)) {
            facingStr = (fwd.x > 0.0f) ? "East (Towards +X)" : "West (Towards -X)";
        } else {
            facingStr = (fwd.z > 0.0f) ? "South (Towards +Z)" : "North (Towards -Z)";
        }
        char l5[128];
        std::snprintf(l5, sizeof(l5), "Facing: %s", facingStr);
        FontRenderer::drawText(vertices, indices, l5, hudX, hudY + lineH * 4.0f, fontSize, glm::vec3(0.70f, 0.85f, 1.0f), true);

        // Line 6: Light level
        int skyLight = 15, blockLight = 0;
        if (world) {
            world->getLightLevels(curCell.x, curCell.y, curCell.z, curCell.s, skyLight, blockLight);
        }
        int totalLight = std::max(skyLight, blockLight);
        char l6[128];
        std::snprintf(l6, sizeof(l6), "Light: %d (%d sky, %d block)", totalLight, skyLight, blockLight);
        glm::vec3 lightCol = (totalLight <= 7) ? glm::vec3(1.0f, 0.35f, 0.35f) : glm::vec3(0.35f, 1.0f, 0.45f);
        FontRenderer::drawText(vertices, indices, l6, hudX, hudY + lineH * 5.0f, fontSize, lightCol, true);

        // Line 7: Biome
        std::string_view biomeSv = "Plains";
        if (world) {
            BiomeType bType = world->getTerrainGen().getBiome(curCell.x, curCell.z);
            biomeSv = getBiomeName(bType);
        }
        char l7[128];
        std::snprintf(l7, sizeof(l7), "Biome: %.*s", static_cast<int>(biomeSv.size()), biomeSv.data());
        FontRenderer::drawText(vertices, indices, l7, hudX, hudY + lineH * 6.0f, fontSize, glm::vec3(0.95f, 0.85f, 0.40f), true);
    } else {
        float fontSize = 14.5f;
        float curY = hudY;

        if (options.showFPS) {
            char fpsBuf[64];
            std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %d", static_cast<int>(fps));
            FontRenderer::drawText(vertices, indices, fpsBuf, hudX, curY, fontSize, glm::vec3(1.0f, 1.0f, 0.35f), true);
            curY += 18.0f;
        }

        if (options.showXYZ) {
            char posBuf[128];
            std::snprintf(posBuf, sizeof(posBuf), "XYZ: %.1f / %.1f / %.1f", pos.x, pos.y, pos.z);
            FontRenderer::drawText(vertices, indices, posBuf, hudX, curY, fontSize, glm::vec3(1.0f, 1.0f, 1.0f), true);
            curY += 18.0f;

            if (world) {
                BiomeType bType = world->getTerrainGen().getBiome(curCell.x, curCell.z);
                std::string_view bNameSv = getBiomeName(bType);
                char bioBuf[64];
                std::snprintf(bioBuf, sizeof(bioBuf), "Biome: %.*s", static_cast<int>(bNameSv.size()), bNameSv.data());
                FontRenderer::drawText(vertices, indices, bioBuf, hudX, curY, fontSize, glm::vec3(0.95f, 0.85f, 0.40f), true);
                curY += 18.0f;
            }
        }

        if (options.showFPS || options.showXYZ) {
            char selBuf[128];
            std::snprintf(selBuf, sizeof(selBuf), "Block: %s", bName.empty() ? "Air" : bName.c_str());
            FontRenderer::drawText(vertices, indices, selBuf, hudX, curY, fontSize, glm::vec3(0.40f, 1.0f, 0.40f), true);
        }
    }

    auto addThickLine = [&](const glm::vec3& p0, const glm::vec3& p1, float thickness, const glm::vec3& color) {
        glm::vec2 diff(p1.x - p0.x, p1.y - p0.y);
        float len = glm::length(diff);
        if (len < 0.001f) return;
        glm::vec2 dir = diff / len;
        glm::vec2 perp(-dir.y, dir.x);
        glm::vec2 h = perp * (thickness * 0.5f);
        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 norm(0.0f, 0.0f, 1.0f);
        vertices.push_back({glm::vec3(p0.x - h.x, p0.y - h.y, 0.0f), glm::vec2(uv.x, uv.y), norm, color});
        vertices.push_back({glm::vec3(p1.x - h.x, p1.y - h.y, 0.0f), glm::vec2(uv.z, uv.y), norm, color});
        vertices.push_back({glm::vec3(p1.x + h.x, p1.y + h.y, 0.0f), glm::vec2(uv.z, uv.w), norm, color});
        vertices.push_back({glm::vec3(p0.x + h.x, p0.y + h.y, 0.0f), glm::vec2(uv.x, uv.w), norm, color});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 0); indices.push_back(b + 2); indices.push_back(b + 3);
    };

    // 2. Health & Oxygen Bars above Hotbar
    float triSide = 48.0f;
    float triHeight = triSide * 0.8660254f;
    float stepX = (triSide + 3.0f) * 0.5f;
    float totalHotbarW = 9.0f * stepX + triSide;
    float startX = (screenWidth - totalHotbarW) * 0.5f;
    float startY = screenHeight - triHeight - 16.0f;

    // Health & Hunger & Oxygen & Armor Bars above Hotbar (Survival only)
    if (!player.isCreative()) {
        float iconW = 12.0f;
        float iconStep = 13.0f;
        float statusY = startY - 20.0f;

        // 1. Health Bar (10 Minecraft hearts on the left)
        float health = player.getHealth();
        for (int i = 0; i < 10; ++i) {
            float hx = startX + i * iconStep;
            float hy = statusY;
            if (health <= 4.0f) {
                hy += (i % 2 == 0) ? 1.5f : -1.5f; // Low health jitter
            }
            // Background empty container
            glm::vec4 uvEmpty = TextureAtlas::getTileUV(TextureAtlas::TILE_HEART_EMPTY);
            addTexturedQuad(hx, hy, iconW, iconW, uvEmpty, glm::vec3(1.0f));

            if (health >= (i + 1) * 2.0f) {
                glm::vec4 uvFull = TextureAtlas::getTileUV(TextureAtlas::TILE_HEART_FULL);
                addTexturedQuad(hx, hy, iconW, iconW, uvFull, glm::vec3(1.0f));
            } else if (health >= i * 2.0f + 1.0f) {
                glm::vec4 uvHalf = TextureAtlas::getTileUV(TextureAtlas::TILE_HEART_HALF);
                addTexturedQuad(hx, hy, iconW, iconW, uvHalf, glm::vec3(1.0f));
            }
        }

        // 2. Hunger Bar (10 Minecraft hunger shanks on the right)
        float hunger = player.getHunger();
        float hungerStartX = startX + totalHotbarW - 10.0f * iconStep;
        for (int i = 0; i < 10; ++i) {
            float hx = hungerStartX + (9 - i) * iconStep;
            float hy = statusY;
            // Background empty drumstick
            glm::vec4 uvEmpty = TextureAtlas::getTileUV(TextureAtlas::TILE_HUNGER_EMPTY);
            addTexturedQuad(hx, hy, iconW, iconW, uvEmpty, glm::vec3(1.0f));

            if (hunger >= (i + 1) * 2.0f) {
                glm::vec4 uvFull = TextureAtlas::getTileUV(TextureAtlas::TILE_HUNGER_FULL);
                addTexturedQuad(hx, hy, iconW, iconW, uvFull, glm::vec3(1.0f));
            } else if (hunger >= i * 2.0f + 1.0f) {
                glm::vec4 uvHalf = TextureAtlas::getTileUV(TextureAtlas::TILE_HUNGER_HALF);
                addTexturedQuad(hx, hy, iconW, iconW, uvHalf, glm::vec3(1.0f));
            }
        }

        // 3. Oxygen Bar (10 Bubbles above Hunger Bar when underwater)
        float oxygen = player.getOxygen();
        if (player.isUnderwater() || oxygen < 10.0f) {
            float bubbleY = statusY - 14.0f;
            for (int i = 0; i < 10; ++i) {
                float bx = hungerStartX + (9 - i) * iconStep;
                float by = bubbleY;
                if (oxygen >= (i + 1) * 1.0f) {
                    glm::vec4 uvBubble = TextureAtlas::getTileUV(TextureAtlas::TILE_BUBBLE);
                    addTexturedQuad(bx, by, iconW, iconW, uvBubble, glm::vec3(1.0f));
                } else if (oxygen >= i * 1.0f + 0.5f) {
                    glm::vec4 uvPop = TextureAtlas::getTileUV(TextureAtlas::TILE_BUBBLE_POP);
                    addTexturedQuad(bx, by, iconW, iconW, uvPop, glm::vec3(1.0f));
                }
            }
        }

        // 4. Armor Bar (above Health Bar when armor > 0)
        float armor = player.getArmor();
        if (armor > 0.0f) {
            float armorY = statusY - 14.0f;
            for (int i = 0; i < 10; ++i) {
                float ax = startX + i * iconStep;
                float ay = armorY;
                if (armor >= (i + 1) * 2.0f) {
                    glm::vec4 uvArmor = TextureAtlas::getTileUV(TextureAtlas::TILE_ARMOR_FULL);
                    addTexturedQuad(ax, ay, iconW, iconW, uvArmor, glm::vec3(1.0f));
                } else if (armor >= i * 2.0f + 1.0f) {
                    glm::vec4 uvHalfArmor = TextureAtlas::getTileUV(TextureAtlas::TILE_ARMOR_HALF);
                    addTexturedQuad(ax, ay, iconW, iconW, uvHalfArmor, glm::vec3(1.0f));
                }
            }
        }
    }

    // 3. Hotbar: 10 Alternating Equilateral Triangles
    int activeSlot = player.getSelectedSlot();
    const auto& hotbar = player.getHotbar();
    const auto& counts = player.getHotbarCounts();

    for (int i = 0; i < 10; ++i) {
        float sx = startX + i * stepX;
        bool isUp = (i % 2 == 0);
        bool isSelected = (i == activeSlot);

        glm::vec3 v0, v1, v2;
        if (isUp) {
            v0 = {sx + triSide * 0.5f, startY, 0.0f};
            v1 = {sx, startY + triHeight, 0.0f};
            v2 = {sx + triSide, startY + triHeight, 0.0f};
        } else {
            v0 = {sx, startY, 0.0f};
            v1 = {sx + triSide, startY, 0.0f};
            v2 = {sx + triSide * 0.5f, startY + triHeight, 0.0f};
        }

        glm::vec4 slotUV = TextureAtlas::getTileUV(TextureAtlas::TILE_TINT_DARK);
        glm::vec3 slotColor(0.20f, 0.20f, 0.22f);
        addTri(v0, v1, v2, slotUV, slotColor);

        // Slot Border
        float borderThickness = isSelected ? 3.5f : 1.5f;
        glm::vec3 borderColor = isSelected ? glm::vec3(1.0f, 0.95f, 0.30f) : glm::vec3(0.40f, 0.40f, 0.45f);
        addThickLine(v0, v1, borderThickness, borderColor);
        addThickLine(v1, v2, borderThickness, borderColor);
        addThickLine(v2, v0, borderThickness, borderColor);

        auto draw3DItemIcon = [&](float icx, float icy, float iconS, BlockType blockType) {
            if (Cell{blockType}.isFlatItem()) {
                uint32_t itemTile = TextureAtlas::getTileForBlock(blockType, 0);
                glm::vec4 itemUV = TextureAtlas::getTileUV(itemTile);
                addTexturedQuad(icx - iconS * 0.5f, icy - iconS * 0.5f, iconS, iconS, itemUV, glm::vec3(1.0f));
            } else {
                glm::vec4 uvTop = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(blockType, 0));
                glm::vec4 uvSide = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(blockType, 2));

                float r = iconS * 0.5f;
                float hr = r * 0.8660254f;
                float topY = icy - hr * 0.45f;
                float sideH = r * 0.85f;

                glm::vec3 t0(icx, topY - hr * 0.6f, 0.0f);
                glm::vec3 t1(icx - r, topY + hr * 0.4f, 0.0f);
                glm::vec3 t2(icx + r, topY + hr * 0.4f, 0.0f);
                addTri(t0, t1, t2, uvTop, glm::vec3(1.0f));

                uint32_t bL = static_cast<uint32_t>(vertices.size());
                glm::vec3 nL(0.0f, 0.0f, 1.0f);
                vertices.push_back({t1, glm::vec2(uvSide.x, uvSide.y), nL, glm::vec3(0.70f)});
                vertices.push_back({glm::vec3(t1.x, t1.y + sideH, 0.0f), glm::vec2(uvSide.x, uvSide.w), nL, glm::vec3(0.70f)});
                vertices.push_back({glm::vec3(icx, topY + hr * 0.4f + sideH, 0.0f), glm::vec2(uvSide.z, uvSide.w), nL, glm::vec3(0.70f)});
                vertices.push_back({glm::vec3(icx, topY + hr * 0.4f, 0.0f), glm::vec2(uvSide.z, uvSide.y), nL, glm::vec3(0.70f)});
                indices.push_back(bL + 0); indices.push_back(bL + 1); indices.push_back(bL + 2);
                indices.push_back(bL + 0); indices.push_back(bL + 2); indices.push_back(bL + 3);

                uint32_t bR = static_cast<uint32_t>(vertices.size());
                glm::vec3 nR(0.0f, 0.0f, 1.0f);
                vertices.push_back({glm::vec3(icx, topY + hr * 0.4f, 0.0f), glm::vec2(uvSide.x, uvSide.y), nR, glm::vec3(0.55f)});
                vertices.push_back({glm::vec3(icx, topY + hr * 0.4f + sideH, 0.0f), glm::vec2(uvSide.x, uvSide.w), nR, glm::vec3(0.55f)});
                vertices.push_back({glm::vec3(t2.x, t2.y + sideH, 0.0f), glm::vec2(uvSide.z, uvSide.w), nR, glm::vec3(0.55f)});
                vertices.push_back({t2, glm::vec2(uvSide.z, uvSide.y), nR, glm::vec3(0.55f)});
                indices.push_back(bR + 0); indices.push_back(bR + 1); indices.push_back(bR + 2);
                indices.push_back(bR + 0); indices.push_back(bR + 2); indices.push_back(bR + 3);
            }
        };

        // Block item icon
        BlockType block = hotbar[i];
        if (block != BlockType::Air) {
            float iconS = 24.0f;
            float icx = sx + triSide * 0.5f;
            float icy = isUp ? (startY + triHeight * 0.58f) : (startY + triHeight * 0.42f);
            draw3DItemIcon(icx, icy, iconS, block);

            // Item count in pixel font centered inside equilateral triangle
            int count = counts[i];
            if (count > 1) {
                char countBuf[16];
                std::snprintf(countBuf, sizeof(countBuf), "%d", count);
                float fontSize = 9.0f;
                float textW = FontRenderer::getTextWidth(countBuf, fontSize);
                float cx = sx + triSide * 0.5f;
                float countX = cx - textW * 0.5f;
                float countY = isUp ? (startY + triHeight - fontSize - 3.0f) : (startY + 3.0f);
                addSolidQuad(countX - 2.0f, countY - 1.0f, textW + 4.0f, fontSize + 2.0f, glm::vec3(0.08f, 0.08f, 0.10f));
                FontRenderer::drawText(vertices, indices, countBuf, countX, countY, fontSize, glm::vec3(1.0f, 1.0f, 1.0f), true);
            }
        }
    }

    // 4. Spectator / Free Cam Banner
    if (player.getCamera().getMode() == CameraMode::FreeCam) {
        std::string specBanner = "[SPECTATOR / FREE CAM MODE] (Press F6 to exit)";
        float bW = FontRenderer::getTextWidth(specBanner, 13.0f);
        float bx = (static_cast<float>(screenWidth) - bW) * 0.5f;
        addTexturedQuad(bx - 10.0f, 16.0f, bW + 20.0f, 22.0f, TextureAtlas::getTileUV(TextureAtlas::TILE_TINT_DARK), glm::vec3(0.08f, 0.08f, 0.12f));
        FontRenderer::drawText(vertices, indices, specBanner, bx, 20.0f, 13.0f, glm::vec3(1.0f, 0.85f, 0.20f), true);
    }

    // 5. In-Game Chat / Command Feedback Notification
    if (feedbackTimer > 0.0f && !feedbackMsg.empty()) {
        float fbY = static_cast<float>(screenHeight) - (isChatOpen ? 72.0f : 45.0f);
        float fbW = FontRenderer::getTextWidth(feedbackMsg, 13.0f) + 16.0f;
        addTexturedQuad(10.0f, fbY - 2.0f, fbW, 20.0f, TextureAtlas::getTileUV(TextureAtlas::TILE_TINT_DARK), glm::vec3(0.08f, 0.08f, 0.12f));
        FontRenderer::drawText(vertices, indices, feedbackMsg, 16.0f, fbY + 2.0f, 13.0f, glm::vec3(1.0f, 0.95f, 0.35f), true);
    }

    // 6. Interactive Command / Chat Bar
    if (isChatOpen) {
        float chatY = static_cast<float>(screenHeight) - 38.0f;
        float chatW = std::min(static_cast<float>(screenWidth) - 20.0f, 600.0f);
        addTexturedQuad(10.0f, chatY, chatW, 26.0f, TextureAtlas::getTileUV(TextureAtlas::TILE_TINT_DARK), glm::vec3(0.05f, 0.05f, 0.08f));
        addTexturedQuad(10.0f, chatY, chatW, 1.5f, TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE), glm::vec3(0.45f, 0.45f, 0.55f));
        addTexturedQuad(10.0f, chatY + 25.0f, chatW, 1.5f, TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE), glm::vec3(0.45f, 0.45f, 0.55f));
        std::string displayText = "> " + chatInput + "_";
        FontRenderer::drawText(vertices, indices, displayText, 16.0f, chatY + 6.0f, 13.0f, glm::vec3(1.0f, 1.0f, 1.0f), true);
    }

    // 7. Sleeping Vignette & Fade Overlay
    if (sleepFadeAlpha > 0.005f) {
        float alpha = std::clamp(sleepFadeAlpha, 0.01f, 1.0f);
        float sw = static_cast<float>(screenWidth);
        float sh = static_cast<float>(screenHeight);
        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);

        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 norm(0.0f, 0.0f, alpha);
        glm::vec3 black(0.0f, 0.0f, 0.0f);
        vertices.push_back({glm::vec3(0.0f, sh, 0.0f), glm::vec2(uv.x, uv.w), norm, black});
        vertices.push_back({glm::vec3(sw, sh, 0.0f), glm::vec2(uv.z, uv.w), norm, black});
        vertices.push_back({glm::vec3(sw, 0.0f, 0.0f), glm::vec2(uv.z, uv.y), norm, black});
        vertices.push_back({glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2(uv.x, uv.y), norm, black});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);

        if (alpha > 0.35f) {
            float textAlpha = std::clamp((alpha - 0.35f) / 0.65f, 0.01f, 1.0f);
            std::string msg1 = "Sleeping through the night...";
            float w1 = FontRenderer::getTextWidth(msg1, 20.0f);
            FontRenderer::drawText(vertices, indices, msg1, (sw - w1) * 0.5f, sh * 0.44f, 20.0f, glm::vec3(1.0f, 1.0f, 1.0f), true, textAlpha);

            std::string msg2 = "Press [SHIFT] to leave bed";
            float w2 = FontRenderer::getTextWidth(msg2, 13.0f);
            FontRenderer::drawText(vertices, indices, msg2, (sw - w2) * 0.5f, sh * 0.52f, 13.0f, glm::vec3(0.85f, 0.85f, 0.85f), true, textAlpha);
        }
    }

    m_uiIndexCount[f] = static_cast<uint32_t>(indices.size());
    if (m_uiIndexCount[f] == 0) return;

    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    if (vSize > m_uiVertexBuffer[f].getSize()) {
        VkDeviceSize newSize = std::max(vSize * 2, static_cast<VkDeviceSize>(MAX_UI_VBO_SIZE));
        m_uiVertexBuffer[f] = Buffer(m_context, newSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    if (iSize > m_uiIndexBuffer[f].getSize()) {
        VkDeviceSize newSize = std::max(iSize * 2, static_cast<VkDeviceSize>(MAX_UI_IBO_SIZE));
        m_uiIndexBuffer[f] = Buffer(m_context, newSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    m_uiVertexBuffer[f].upload(vertices.data(), vSize);
    m_uiIndexBuffer[f].upload(indices.data(), iSize);
}

void UIRenderer::render(VkCommandBuffer cmd,
                        const Pipeline& uiPipeline,
                        const Pipeline* invertPipeline,
                        const Player& player,
                        uint32_t screenWidth,
                        uint32_t screenHeight,
                        float fps,
                        const GameOptions& options,
                        const World* world,
                        bool isChatOpen,
                        const std::string& chatInput,
                        const std::string& feedbackMsg,
                        float feedbackTimer,
                        VkDescriptorSet descSet,
                        float sleepFadeAlpha) {
    if (screenWidth == 0 || screenHeight == 0) return;

    uint32_t f = m_cmdQueue.getCurrentFrame();
    updateDynamicUI(f, player, screenWidth, screenHeight, fps, options, world, isChatOpen, chatInput, feedbackMsg, feedbackTimer, sleepFadeAlpha);

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight), 0.0f, -1.0f, 1.0f);

    PushConstants pc{};
    std::memcpy(pc.mvp, &proj[0][0], sizeof(float) * 16);
    pc.sunDir[3] = 2.0f; // Pure unshaded
    pc.lightColor[0] = 1.0f; pc.lightColor[1] = 1.0f; pc.lightColor[2] = 1.0f;
    pc.skyFog[3] = 1000.0f;

    // 1. Draw Thematic Inverted 3-Spoke Star Crosshair at center (except in spectator freecam or when sleeping)
    if (player.getCamera().getMode() != CameraMode::FreeCam && m_chIndexCount > 0 && m_chVbo.isValid() && sleepFadeAlpha <= 0.005f) {
        const Pipeline& chPipeline = invertPipeline ? *invertPipeline : uiPipeline;
        chPipeline.bind(cmd);

        if (descSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, chPipeline.getLayout(), 0, 1, &descSet, 0, nullptr);
        }

        float cx = static_cast<float>(screenWidth) * 0.5f;
        float cy = static_cast<float>(screenHeight) * 0.5f;
        glm::mat4 chModel = glm::translate(proj, glm::vec3(cx, cy, 0.0f));

        PushConstants chPC{};
        std::memcpy(chPC.mvp, &chModel[0][0], sizeof(float) * 16);
        chPC.sunDir[3] = 2.0f;
        chPC.lightColor[0] = 1.0f; chPC.lightColor[1] = 1.0f; chPC.lightColor[2] = 1.0f;
        chPC.skyFog[3] = 1000.0f;

        vkCmdPushConstants(cmd, chPipeline.getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &chPC);

        VkBuffer vbs[] = {m_chVbo.getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
        vkCmdBindIndexBuffer(cmd, m_chIbo.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_chIndexCount, 1, 0, 0, 0);
    }

    // 2. Draw Dynamic UI (HUD, Hearts, Oxygen, Hotbar)
    if (m_uiIndexCount[f] > 0 && m_uiVertexBuffer[f].isValid()) {
        uiPipeline.bind(cmd);

        if (descSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline.getLayout(), 0, 1, &descSet, 0, nullptr);
        }

        vkCmdPushConstants(cmd, uiPipeline.getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &pc);

        VkBuffer vbs[] = {m_uiVertexBuffer[f].getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
        vkCmdBindIndexBuffer(cmd, m_uiIndexBuffer[f].getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_uiIndexCount[f], 1, 0, 0, 0);
    }
}

} // namespace prismcraft
