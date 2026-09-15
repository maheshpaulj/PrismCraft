#include "TextureStitcher.hpp"
#include "TextureAtlas.hpp"
#include "data/BlockRegistry.hpp"
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace prismcraft {

namespace fs = std::filesystem;

void TextureStitcher::blitTile(std::vector<uint8_t>& dst, int tileIndex, const uint8_t* src, int srcChannels) {
    if (tileIndex < 0 || tileIndex >= TOTAL_TILES || !src) return;
    int col = tileIndex % TILES_PER_ROW;
    int row = tileIndex / TILES_PER_ROW;
    int startX = col * TILE_SIZE;
    int startY = row * TILE_SIZE;

    for (int y = 0; y < TILE_SIZE; ++y) {
        for (int x = 0; x < TILE_SIZE; ++x) {
            int dstIdx = ((startY + y) * ATLAS_SIZE + (startX + x)) * 4;
            int srcIdx = (y * TILE_SIZE + x) * srcChannels;

            if (srcChannels == 4) {
                dst[dstIdx + 0] = src[srcIdx + 0];
                dst[dstIdx + 1] = src[srcIdx + 1];
                dst[dstIdx + 2] = src[srcIdx + 2];
                dst[dstIdx + 3] = src[srcIdx + 3];
            } else if (srcChannels == 3) {
                dst[dstIdx + 0] = src[srcIdx + 0];
                dst[dstIdx + 1] = src[srcIdx + 1];
                dst[dstIdx + 2] = src[srcIdx + 2];
                dst[dstIdx + 3] = 255;
            } else if (srcChannels == 1) {
                dst[dstIdx + 0] = src[srcIdx];
                dst[dstIdx + 1] = src[srcIdx];
                dst[dstIdx + 2] = src[srcIdx];
                dst[dstIdx + 3] = 255;
            }
        }
    }
}

void TextureStitcher::stitchLegacyBase(std::vector<uint8_t>& pixels, const std::string& assetsDir) {
    (void)assetsDir;
    // Generate the legacy 256x512 atlas pixels
    std::vector<uint8_t> legacy = TextureAtlas::generateLegacyPixels();
    if (legacy.size() != 256 * 512 * 4) return;

    // Blit the 256x512 rectangle into the top-left of the 1024x1024 atlas (rows 0..31, cols 0..15)
    for (int y = 0; y < 512; ++y) {
        for (int x = 0; x < 256; ++x) {
            int srcIdx = (y * 256 + x) * 4;
            int dstIdx = (y * ATLAS_SIZE + x) * 4;
            pixels[dstIdx + 0] = legacy[srcIdx + 0];
            pixels[dstIdx + 1] = legacy[srcIdx + 1];
            pixels[dstIdx + 2] = legacy[srcIdx + 2];
            pixels[dstIdx + 3] = legacy[srcIdx + 3];
        }
    }
}

void TextureStitcher::stitchBedTextures(std::vector<uint8_t>& pixels, const std::string& assetsDir, int& currentTile) {
    const std::vector<std::string> bedPaths = {
        assetsDir + "textures/textures/entity/bed/red.png",
        "assets/textures/textures/entity/bed/red.png",
        "../assets/textures/textures/entity/bed/red.png",
        "d:/Mahesh/Coding files/PrismCraft/PrismCraft/assets/textures/textures/entity/bed/red.png"
    };

    int w = 0, h = 0, ch = 0;
    stbi_uc* bedData = nullptr;
    for (const auto& p : bedPaths) {
        bedData = stbi_load(p.c_str(), &w, &h, &ch, 4);
        if (bedData) break;
    }

    if (!bedData || w < 64 || h < 64) {
        if (bedData) stbi_image_free(bedData);
        // Fallback: bed textures will use legacy tiles
        return;
    }

    auto extractSubTile = [&](int sx, int sy, int sw, int sh, const std::string& texName) {
        std::vector<uint8_t> tile(16 * 16 * 4, 0);
        for (int y = 0; y < std::min(sh, 16); ++y) {
            for (int x = 0; x < std::min(sw, 16); ++x) {
                int srcIdx = ((sy + y) * w + (sx + x)) * 4;
                int dstIdx = (y * 16 + x) * 4;
                tile[dstIdx + 0] = bedData[srcIdx + 0];
                tile[dstIdx + 1] = bedData[srcIdx + 1];
                tile[dstIdx + 2] = bedData[srcIdx + 2];
                tile[dstIdx + 3] = bedData[srcIdx + 3];
            }
        }
        int tileIdx = currentTile++;
        blitTile(pixels, tileIdx, tile.data(), 4);
        BlockRegistry::setTextureTile(texName, tileIdx);
    };

    // 1. Bed Head Top (Pillow at top y=0..6, Head Quilt at bottom y=7..15)
    extractSubTile(6, 6, 16, 16, "bed_head_top");

    // 2. Bed Foot Top (Red Quilt)
    extractSubTile(6, 28, 16, 16, "bed_foot_top");

    // 3. Bed Side (16 pixels long skirt scaled across 16 rows: top half red quilt, bottom half oak frame)
    {
        std::vector<uint8_t> sideTile(16 * 16 * 4, 0);
        for (int tx = 0; tx < 16; ++tx) {
            int sy = 28 + tx;
            for (int ty = 0; ty < 16; ++ty) {
                int skirtY = std::clamp(ty * 6 / 16, 0, 5);
                int sx = 5 - skirtY; // skirtY=0..2: red quilt, skirtY=3..5: wood frame
                int srcIdx = (sy * w + sx) * 4;
                int dstIdx = (ty * 16 + tx) * 4;
                sideTile[dstIdx + 0] = bedData[srcIdx + 0];
                sideTile[dstIdx + 1] = bedData[srcIdx + 1];
                sideTile[dstIdx + 2] = bedData[srcIdx + 2];
                sideTile[dstIdx + 3] = bedData[srcIdx + 3];
            }
        }
        int tileIdx = currentTile++;
        blitTile(pixels, tileIdx, sideTile.data(), 4);
        BlockRegistry::setTextureTile("bed_side", tileIdx);
    }

    // 4. Bed Head End (Headboard: Pillow top, Oak frame bottom)
    {
        std::vector<uint8_t> headEndTile(16 * 16 * 4, 0);
        for (int tx = 0; tx < 16; ++tx) {
            int sx = 6 + tx;
            for (int ty = 0; ty < 16; ++ty) {
                int skirtY = std::clamp(ty * 6 / 16, 0, 5);
                int sy = 5 - skirtY; // Pillow on top, wood on bottom
                int srcIdx = (sy * w + sx) * 4;
                int dstIdx = (ty * 16 + tx) * 4;
                headEndTile[dstIdx + 0] = bedData[srcIdx + 0];
                headEndTile[dstIdx + 1] = bedData[srcIdx + 1];
                headEndTile[dstIdx + 2] = bedData[srcIdx + 2];
                headEndTile[dstIdx + 3] = bedData[srcIdx + 3];
            }
        }
        int tileIdx = currentTile++;
        blitTile(pixels, tileIdx, headEndTile.data(), 4);
        BlockRegistry::setTextureTile("bed_head_end", tileIdx);
        BlockRegistry::setTextureTile("bed_end", tileIdx);
    }

    // 5. Bed Foot End (Footboard: Red quilt top, Oak frame bottom)
    {
        std::vector<uint8_t> footEndTile(16 * 16 * 4, 0);
        for (int tx = 0; tx < 16; ++tx) {
            int sx = 22 + tx;
            for (int ty = 0; ty < 16; ++ty) {
                int skirtY = std::clamp(ty * 6 / 16, 0, 5);
                int sy = 22 + (5 - skirtY); // Blanket on top, wood on bottom
                int srcIdx = (sy * w + sx) * 4;
                int dstIdx = (ty * 16 + tx) * 4;
                footEndTile[dstIdx + 0] = bedData[srcIdx + 0];
                footEndTile[dstIdx + 1] = bedData[srcIdx + 1];
                footEndTile[dstIdx + 2] = bedData[srcIdx + 2];
                footEndTile[dstIdx + 3] = bedData[srcIdx + 3];
            }
        }
        int tileIdx = currentTile++;
        blitTile(pixels, tileIdx, footEndTile.data(), 4);
        BlockRegistry::setTextureTile("bed_foot_end", tileIdx);
    }

    // 6. Bed Underside (16x16 planks at x=28..43, y=6..21)
    extractSubTile(28, 6, 16, 16, "bed_underside");

    // 7. Bed Leg Sides (3x3 pixels at x=50..52, y=3..5 scaled to 16x16)
    {
        std::vector<uint8_t> legTile(16 * 16 * 4, 0);
        for (int ty = 0; ty < 16; ++ty) {
            int sy = 3 + std::clamp(ty * 3 / 16, 0, 2);
            for (int tx = 0; tx < 16; ++tx) {
                int sx = 50 + std::clamp(tx * 3 / 16, 0, 2);
                int srcIdx = (sy * w + sx) * 4;
                int dstIdx = (ty * 16 + tx) * 4;
                legTile[dstIdx + 0] = bedData[srcIdx + 0];
                legTile[dstIdx + 1] = bedData[srcIdx + 1];
                legTile[dstIdx + 2] = bedData[srcIdx + 2];
                legTile[dstIdx + 3] = 255;
            }
        }
        int tileIdx = currentTile++;
        blitTile(pixels, tileIdx, legTile.data(), 4);
        BlockRegistry::setTextureTile("bed_leg", tileIdx);
    }

    // 8. Bed Leg Bottom Cap (3x3 pixels at x=53..55, y=0..2 scaled to 16x16)
    {
        std::vector<uint8_t> legBotTile(16 * 16 * 4, 0);
        for (int ty = 0; ty < 16; ++ty) {
            int sy = 0 + std::clamp(ty * 3 / 16, 0, 2);
            for (int tx = 0; tx < 16; ++tx) {
                int sx = 53 + std::clamp(tx * 3 / 16, 0, 2);
                int srcIdx = (sy * w + sx) * 4;
                int dstIdx = (ty * 16 + tx) * 4;
                legBotTile[dstIdx + 0] = bedData[srcIdx + 0];
                legBotTile[dstIdx + 1] = bedData[srcIdx + 1];
                legBotTile[dstIdx + 2] = bedData[srcIdx + 2];
                legBotTile[dstIdx + 3] = 255;
            }
        }
        int tileIdx = currentTile++;
        blitTile(pixels, tileIdx, legBotTile.data(), 4);
        BlockRegistry::setTextureTile("bed_leg_bottom", tileIdx);
    }

    // Bed Item Icon (registered as "bed" for inventory & drops)
    {
        std::vector<uint8_t> itemTile(16 * 16 * 4, 0);
        for (int y = 2; y < 14; ++y) {
            for (int x = 2; x < 14; ++x) {
                int dstIdx = (y * 16 + x) * 4;
                if (y >= 3 && y <= 5 && x >= 4 && x <= 11) {
                    // White fluffy pillow
                    uint8_t c = (y == 3 || x == 4) ? 245 : 220;
                    itemTile[dstIdx + 0] = c;
                    itemTile[dstIdx + 1] = c;
                    itemTile[dstIdx + 2] = c;
                    itemTile[dstIdx + 3] = 255;
                } else if (y >= 6 && y <= 12 && x >= 3 && x <= 12) {
                    // Red quilt blanket
                    bool hi = (x == 3 || y == 6);
                    bool sh = (x == 12 || y == 12);
                    itemTile[dstIdx + 0] = hi ? 205 : (sh ? 140 : 175);
                    itemTile[dstIdx + 1] = hi ? 45  : (sh ? 18  : 28);
                    itemTile[dstIdx + 2] = hi ? 45  : (sh ? 18  : 28);
                    itemTile[dstIdx + 3] = 255;
                } else {
                    // Oak wood frame
                    itemTile[dstIdx + 0] = 135;
                    itemTile[dstIdx + 1] = 95;
                    itemTile[dstIdx + 2] = 50;
                    itemTile[dstIdx + 3] = 255;
                }
            }
        }
        int tileIdx = currentTile++;
        blitTile(pixels, tileIdx, itemTile.data(), 4);
        BlockRegistry::setTextureTile("bed", tileIdx);
    }

    stbi_image_free(bedData);
}

void TextureStitcher::stitchDestroyStages(std::vector<uint8_t>& pixels, const std::string& validDir, int& currentTile) {
    for (int stage = 0; stage < 10; ++stage) {
        std::string name = "destroy_stage_" + std::to_string(stage);
        std::string filePath = validDir + name + ".png";

        int w = 0, h = 0, ch = 0;
        stbi_uc* data = nullptr;
        std::error_code ec;
        if (fs::exists(filePath, ec)) {
            data = stbi_load(filePath.c_str(), &w, &h, &ch, 4);
        }

        std::vector<uint8_t> tilePixels(16 * 16 * 4, 0);

        if (data && w > 0 && h > 0) {
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    int srcX = (w == 16) ? x : (x * w / 16);
                    int srcY = (h == 16) ? y : (y * h / 16);
                    int srcIdx = (srcY * w + srcX) * 4;
                    int dstIdx = (y * 16 + x) * 4;

                    uint8_t r = data[srcIdx + 0];
                    uint8_t g = data[srcIdx + 1];
                    uint8_t b = data[srcIdx + 2];
                    uint8_t a = data[srcIdx + 3];

                    // Minecraft destruction stages use low alpha (a=1) for background pixels
                    if (a <= 10) {
                        tilePixels[dstIdx + 0] = 0;
                        tilePixels[dstIdx + 1] = 0;
                        tilePixels[dstIdx + 2] = 0;
                        tilePixels[dstIdx + 3] = 0;
                    } else {
                        tilePixels[dstIdx + 0] = r;
                        tilePixels[dstIdx + 1] = g;
                        tilePixels[dstIdx + 2] = b;
                        tilePixels[dstIdx + 3] = 255;
                    }
                }
            }
            stbi_image_free(data);
        } else {
            // Procedural fallback if destroy_stage_*.png is not present
            int threshold = (stage + 1) * 3;
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    int dstIdx = (y * 16 + x) * 4;
                    bool crack = (std::abs(x - y) < (1 + stage / 3) && x > 2 && x < 14) ||
                                 (std::abs((15 - x) - y) < (1 + stage / 4) && y > 3 && y < 13) ||
                                 ((x == 8 || y == 8) && (x + y) % 2 == 0 && stage > 4);
                    if (crack && (x * 7 + y * 13) % 10 <= threshold) {
                        tilePixels[dstIdx + 0] = 61;
                        tilePixels[dstIdx + 1] = 61;
                        tilePixels[dstIdx + 2] = 61;
                        tilePixels[dstIdx + 3] = 255;
                    } else {
                        tilePixels[dstIdx + 0] = 0;
                        tilePixels[dstIdx + 1] = 0;
                        tilePixels[dstIdx + 2] = 0;
                        tilePixels[dstIdx + 3] = 0;
                    }
                }
            }
        }

        int assignedTile = currentTile++;
        blitTile(pixels, assignedTile, tilePixels.data(), 4);
        BlockRegistry::setTextureTile(name, assignedTile);
    }
}

void TextureStitcher::stitchModernBlocks(std::vector<uint8_t>& pixels, const std::string& assetsDir) {
    int currentTile = MODERN_BLOCKS_START_TILE; // Start at tile 2048 (row 32, col 0)

    // Stitch modern bed textures from entity/bed/red.png
    stitchBedTextures(pixels, assetsDir, currentTile);

    // Get all textures referenced by blocks.json
    std::vector<std::string> requested = BlockRegistry::getReferencedTextureNames();

    const std::vector<std::string> searchDirs = {
        assetsDir + "textures/textures/block/",
        "assets/textures/textures/block/",
        "../assets/textures/textures/block/",
        "d:/Mahesh/Coding files/PrismCraft/PrismCraft/assets/textures/textures/block/"
    };

    std::string validDir;
    for (const auto& d : searchDirs) {
        if (fs::exists(d)) {
            validDir = d;
            break;
        }
    }

    if (validDir.empty()) {
        std::cerr << "[TextureStitcher] Warning: Modern block textures directory not found." << std::endl;
        return;
    }

    // Stitch destroy stages 0-9 into modern atlas
    stitchDestroyStages(pixels, validDir, currentTile);

    int loadedCount = 0;
    for (const auto& name : requested) {
        if (name.empty()) continue;
        if (BlockRegistry::getTextureTile(name) >= 0) continue; // Already stitched (e.g. bed)

        std::string filePath = validDir + name + ".png";
        std::error_code ec;
        if (!fs::exists(filePath, ec)) {
            std::string itemPath = validDir + "../item/" + name + ".png";
            if (fs::exists(itemPath, ec)) {
                filePath = itemPath;
            } else {
                continue;
            }
        }

        int w = 0, h = 0, ch = 0;
        stbi_uc* data = stbi_load(filePath.c_str(), &w, &h, &ch, 4);
        if (!data || w <= 0 || h <= 0) {
            if (data) stbi_image_free(data);
            continue;
        }

        std::vector<uint8_t> tilePixels(16 * 16 * 4, 0);

        // Copy / scale to 16x16 if necessary
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                int srcX = (w == 16) ? x : (x * w / 16);
                int srcY = (w == 16) ? y : (y * w / 16); // Sample first square w x w frame (avoids squashing animation strips like lantern.png)
                int srcIdx = (srcY * w + srcX) * 4;
                int dstIdx = (y * 16 + x) * 4;

                uint8_t r = data[srcIdx + 0];
                uint8_t g = data[srcIdx + 1];
                uint8_t b = data[srcIdx + 2];
                uint8_t a = data[srcIdx + 3];

                // Biome Foliage / Grass Tinting for greyscale source textures
                if (name == "grass_block_top" || name == "short_grass" || name == "tall_grass_bottom" || name == "tall_grass_top") {
                    float grey = r / 255.0f;
                    r = static_cast<uint8_t>(std::clamp(grey * 110.0f, 0.0f, 255.0f));
                    g = static_cast<uint8_t>(std::clamp(grey * 190.0f, 0.0f, 255.0f));
                    b = static_cast<uint8_t>(std::clamp(grey * 65.0f, 0.0f, 255.0f));
                } else if (name == "oak_leaves" || name == "jungle_leaves" || name == "acacia_leaves") {
                    if (a > 20) {
                        float grey = r / 255.0f;
                        r = static_cast<uint8_t>(std::clamp(grey * 115.0f, 0.0f, 255.0f));
                        g = static_cast<uint8_t>(std::clamp(grey * 210.0f, 0.0f, 255.0f));
                        b = static_cast<uint8_t>(std::clamp(grey * 45.0f, 0.0f, 255.0f));
                    }
                } else if (name == "spruce_leaves") {
                    if (a > 20) {
                        float norm = std::min(1.0f, (r / 100.0f));
                        r = static_cast<uint8_t>(std::clamp(norm * 85.0f, 0.0f, 255.0f));
                        g = static_cast<uint8_t>(std::clamp(norm * 155.0f, 0.0f, 255.0f));
                        b = static_cast<uint8_t>(std::clamp(norm * 90.0f, 0.0f, 255.0f));
                    }
                } else if (name == "spruce_log" || name == "spruce_log_top") {
                    // Boost brightness of spruce bark so it shows authentic rich pine wood grain instead of pitch black
                    float factor = 1.55f;
                    r = static_cast<uint8_t>(std::clamp(r * factor + 14.0f, 0.0f, 255.0f));
                    g = static_cast<uint8_t>(std::clamp(g * factor + 10.0f, 0.0f, 255.0f));
                    b = static_cast<uint8_t>(std::clamp(b * factor + 7.0f, 0.0f, 255.0f));
                } else if (name == "birch_leaves") {
                    if (a > 20) {
                        float grey = r / 255.0f;
                        r = static_cast<uint8_t>(std::clamp(grey * 128.0f, 0.0f, 255.0f));
                        g = static_cast<uint8_t>(std::clamp(grey * 185.0f, 0.0f, 255.0f));
                        b = static_cast<uint8_t>(std::clamp(grey * 60.0f, 0.0f, 255.0f));
                    }
                }

                tilePixels[dstIdx + 0] = r;
                tilePixels[dstIdx + 1] = g;
                tilePixels[dstIdx + 2] = b;
                tilePixels[dstIdx + 3] = a;
            }
        }
        stbi_image_free(data);

        // Bleed outer 1-pixel transparent margins for cactus textures so bilinear sampling never hits alpha 0
        if (name == "cactus_side") {
            for (int y = 0; y < 16; ++y) {
                // Bleed x=1 into x=0
                int idx0 = (y * 16 + 0) * 4;
                int idx1 = (y * 16 + 1) * 4;
                tilePixels[idx0 + 0] = tilePixels[idx1 + 0];
                tilePixels[idx0 + 1] = tilePixels[idx1 + 1];
                tilePixels[idx0 + 2] = tilePixels[idx1 + 2];
                tilePixels[idx0 + 3] = 255;
                // Bleed x=14 into x=15
                int idx15 = (y * 16 + 15) * 4;
                int idx14 = (y * 16 + 14) * 4;
                tilePixels[idx15 + 0] = tilePixels[idx14 + 0];
                tilePixels[idx15 + 1] = tilePixels[idx14 + 1];
                tilePixels[idx15 + 2] = tilePixels[idx14 + 2];
                tilePixels[idx15 + 3] = 255;
            }
        } else if (name == "cactus_top" || name == "cactus_bottom") {
            // Bleed 1-pixel border from inner [1, 14] rectangle
            for (int y = 0; y < 16; ++y) {
                int clampY = std::clamp(y, 1, 14);
                for (int x = 0; x < 16; ++x) {
                    int clampX = std::clamp(x, 1, 14);
                    if (clampX != x || clampY != y) {
                        int srcIdx = (clampY * 16 + clampX) * 4;
                        int dstIdx = (y * 16 + x) * 4;
                        tilePixels[dstIdx + 0] = tilePixels[srcIdx + 0];
                        tilePixels[dstIdx + 1] = tilePixels[srcIdx + 1];
                        tilePixels[dstIdx + 2] = tilePixels[srcIdx + 2];
                        tilePixels[dstIdx + 3] = 255;
                    }
                }
            }
        }

        int assignedTile = currentTile++;
        blitTile(pixels, assignedTile, tilePixels.data(), 4);
        BlockRegistry::setTextureTile(name, assignedTile);
        loadedCount++;

        if (currentTile >= TOTAL_TILES) {
            std::cerr << "[TextureStitcher] Warning: Texture atlas is full!" << std::endl;
            break;
        }
    }

    // Stitch all item textures from textures/item/ (including stick, tools, materials)
    std::string itemDir = validDir + "../item/";
    std::error_code itemEc;
    if (fs::exists(itemDir, itemEc)) {
        for (const auto& entry : fs::directory_iterator(itemDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension().string() != ".png") continue;
            std::string itemName = entry.path().stem().string();
            if (BlockRegistry::getTextureTile(itemName) >= 0) continue;
            if (currentTile >= TOTAL_TILES) break;

            int iw = 0, ih = 0, ich = 0;
            stbi_uc* idata = stbi_load(entry.path().string().c_str(), &iw, &ih, &ich, 4);
            if (!idata || iw <= 0 || ih <= 0) {
                if (idata) stbi_image_free(idata);
                continue;
            }

            std::vector<uint8_t> itemPixels(16 * 16 * 4, 0);
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    int srcX = (iw == 16) ? x : (x * iw / 16);
                    int srcY = (iw == 16) ? y : (y * iw / 16);
                    int srcIdx = (srcY * iw + srcX) * 4;
                    int dstIdx = (y * 16 + x) * 4;
                    itemPixels[dstIdx + 0] = idata[srcIdx + 0];
                    itemPixels[dstIdx + 1] = idata[srcIdx + 1];
                    itemPixels[dstIdx + 2] = idata[srcIdx + 2];
                    itemPixels[dstIdx + 3] = idata[srcIdx + 3];
                }
            }
            stbi_image_free(idata);

            int assignedTile = currentTile++;
            blitTile(pixels, assignedTile, itemPixels.data(), 4);
            BlockRegistry::setTextureTile(itemName, assignedTile);
            BlockRegistry::setTextureTile("item_" + itemName, assignedTile);
            loadedCount++;
        }
    }

    std::cout << "[TextureStitcher] Stitched " << loadedCount << " modern block & item textures into 1024x1024 atlas." << std::endl;
}

std::vector<uint8_t> TextureStitcher::buildAtlas(const std::string& assetsDir) {
    std::cout << "[TextureStitcher] Building unified " << ATLAS_SIZE << "x" << ATLAS_SIZE << " Texture Atlas..." << std::endl;
    std::vector<uint8_t> pixels(ATLAS_SIZE * ATLAS_SIZE * 4, 0);

    // 1. Stitch legacy terrain base and procedural HUD/font/tool tiles into rows 0..31
    stitchLegacyBase(pixels, assetsDir);

    // 2. Stitch all requested modern block textures into rows 32..63
    stitchModernBlocks(pixels, assetsDir);

    std::cout << "[TextureStitcher] Atlas generation complete." << std::endl;
    stbi_write_png("atlas_stitched.png", ATLAS_SIZE, ATLAS_SIZE, 4, pixels.data(), ATLAS_SIZE * 4);
    return pixels;
}

} // namespace prismcraft
