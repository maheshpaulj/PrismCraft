#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace prismcraft {

class TextureStitcher {
public:
    static constexpr int ATLAS_SIZE = 1024;
    static constexpr int TILE_SIZE = 16;
    static constexpr int TILES_PER_ROW = ATLAS_SIZE / TILE_SIZE; // 64
    static constexpr int TOTAL_TILES = TILES_PER_ROW * TILES_PER_ROW; // 4096
    static constexpr int MODERN_BLOCKS_START_TILE = 2048; // Row 32, col 0

    // Converts an old 256x512 (16 tiles/row) tile index to the 1024x1024 (64 tiles/row) equivalent
    static inline int legacyToNewTile(int legacyIdx) {
        int col = legacyIdx % 16;
        int row = legacyIdx / 16;
        return row * TILES_PER_ROW + col;
    }

    // Builds the unified 1024x1024 RGBA atlas and registers all modern block textures with BlockRegistry
    static std::vector<uint8_t> buildAtlas(const std::string& assetsDir = "assets/");

private:
    static void stitchLegacyBase(std::vector<uint8_t>& pixels, const std::string& assetsDir);
    static void stitchModernBlocks(std::vector<uint8_t>& pixels, const std::string& assetsDir);
    static void stitchBedTextures(std::vector<uint8_t>& pixels, const std::string& assetsDir, int& currentTile);
    static void stitchDestroyStages(std::vector<uint8_t>& pixels, const std::string& validDir, int& currentTile);
    static void blitTile(std::vector<uint8_t>& dst, int tileIndex, const uint8_t* src, int srcChannels = 4);
};

} // namespace prismcraft
