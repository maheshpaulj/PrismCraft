#pragma once
#include "world/Cell.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace prismcraft {

// Helper: converts a 16-stride legacy tile index (0..511) to its 64-stride position in 1024x1024 atlas
constexpr int toNewTile(int oldIdx) {
    return (oldIdx / 16) * 64 + (oldIdx % 16);
}

class TextureAtlas {
public:
    static constexpr int ATLAS_WIDTH = 1024;
    static constexpr int ATLAS_HEIGHT = 1024;
    static constexpr int TILE_SIZE = 16;
    static constexpr int TILES_PER_ROW = ATLAS_WIDTH / TILE_SIZE; // 64
    static constexpr int TOTAL_TILES = TILES_PER_ROW * (ATLAS_HEIGHT / TILE_SIZE); // 4096

    // Authentic Terrain Tiles for Doors & Bed (Rows 5..9)
    static constexpr int TILE_DOOR_WOOD_UPPER = toNewTile(81);  // 321
    static constexpr int TILE_DOOR_WOOD_LOWER = toNewTile(97);  // 385
    static constexpr int TILE_DOOR_IRON_UPPER = toNewTile(82);  // 322
    static constexpr int TILE_DOOR_IRON_LOWER = toNewTile(98);  // 386
    static constexpr int TILE_BED_HEAD_TOP    = toNewTile(133); // 517
    static constexpr int TILE_BED_FOOT_TOP    = toNewTile(134); // 518
    static constexpr int TILE_BED_HEAD_SIDE   = toNewTile(148); // 580
    static constexpr int TILE_BED_FOOT_SIDE   = toNewTile(149); // 581
    static constexpr int TILE_BED_FOOT_END    = toNewTile(150); // 582
    static constexpr int TILE_BED_HEAD_END    = toNewTile(151); // 583

    // HUD status icons (Row 16: tiles 256..265)
    static constexpr int TILE_HEART_EMPTY = toNewTile(256); // 1024
    static constexpr int TILE_HEART_FULL  = toNewTile(257); // 1025
    static constexpr int TILE_HEART_HALF  = toNewTile(258); // 1026
    static constexpr int TILE_HUNGER_FULL  = toNewTile(259); // 1027
    static constexpr int TILE_HUNGER_HALF  = toNewTile(260); // 1028
    static constexpr int TILE_HUNGER_EMPTY = toNewTile(261); // 1029
    static constexpr int TILE_BUBBLE       = toNewTile(262); // 1030
    static constexpr int TILE_BUBBLE_POP   = toNewTile(263); // 1031
    static constexpr int TILE_ARMOR_FULL   = toNewTile(264); // 1032
    static constexpr int TILE_ARMOR_HALF   = toNewTile(265); // 1033

    // Items & Tools (Rows 16-18: tiles 266..287, 296..301)
    static constexpr int TILE_ITEM_STICK          = toNewTile(266);
    static constexpr int TILE_ITEM_WOOD_PICK      = toNewTile(267);
    static constexpr int TILE_ITEM_STONE_PICK     = toNewTile(268);
    static constexpr int TILE_ITEM_IRON_PICK      = toNewTile(269);
    static constexpr int TILE_ITEM_DIAMOND_PICK   = toNewTile(270);
    static constexpr int TILE_ITEM_WOOD_SHOVEL    = toNewTile(271);
    static constexpr int TILE_ITEM_STONE_SHOVEL   = toNewTile(272);
    static constexpr int TILE_ITEM_IRON_SHOVEL    = toNewTile(273);
    static constexpr int TILE_ITEM_DIAMOND_SHOVEL = toNewTile(274);
    static constexpr int TILE_ITEM_WOOD_AXE       = toNewTile(275);
    static constexpr int TILE_ITEM_STONE_AXE      = toNewTile(276);
    static constexpr int TILE_ITEM_IRON_AXE       = toNewTile(277);
    static constexpr int TILE_ITEM_DIAMOND_AXE    = toNewTile(278);
    static constexpr int TILE_ITEM_WOOD_SWORD     = toNewTile(279);
    static constexpr int TILE_ITEM_STONE_SWORD    = toNewTile(280);
    static constexpr int TILE_ITEM_IRON_SWORD     = toNewTile(281);
    static constexpr int TILE_ITEM_DIAMOND_SWORD  = toNewTile(282);
    static constexpr int TILE_ITEM_WOOD_HOE       = toNewTile(283);
    static constexpr int TILE_ITEM_STONE_HOE      = toNewTile(284);
    static constexpr int TILE_ITEM_IRON_HOE       = toNewTile(285);
    static constexpr int TILE_ITEM_DIAMOND_HOE    = toNewTile(286);
    static constexpr int TILE_ITEM_BOW            = toNewTile(287);

    // Steve skin tiles (Row 18: tiles 288..295)
    static constexpr int TILE_STEVE_HEAD_FRONT  = toNewTile(288);
    static constexpr int TILE_STEVE_HEAD_SIDE   = toNewTile(289);
    static constexpr int TILE_STEVE_HEAD_TOP    = toNewTile(290);
    static constexpr int TILE_STEVE_SKIN_TONE   = toNewTile(291);
    static constexpr int TILE_STEVE_TORSO_FRONT = toNewTile(292);
    static constexpr int TILE_STEVE_TORSO_BACK  = toNewTile(293);
    static constexpr int TILE_STEVE_ARM         = toNewTile(294);
    static constexpr int TILE_STEVE_LEG         = toNewTile(295);

    // Remaining items (Row 18: tiles 296..315)
    static constexpr int TILE_ITEM_ARROW          = toNewTile(296);
    static constexpr int TILE_ITEM_COAL           = toNewTile(297);
    static constexpr int TILE_ITEM_IRON_INGOT     = toNewTile(298);
    static constexpr int TILE_ITEM_DIAMOND        = toNewTile(299);
    static constexpr int TILE_ITEM_STRING         = toNewTile(300);
    static constexpr int TILE_ITEM_FLINT          = toNewTile(301);
    static constexpr int TILE_SHADOW_DISC         = toNewTile(302);
    static constexpr int TILE_ITEM_GOLD_INGOT      = toNewTile(303);
    static constexpr int TILE_ITEM_GOLD_PICK       = toNewTile(304);
    static constexpr int TILE_ITEM_GOLD_SHOVEL     = toNewTile(305);
    static constexpr int TILE_ITEM_GOLD_AXE        = toNewTile(306);
    static constexpr int TILE_ITEM_GOLD_SWORD      = toNewTile(307);
    static constexpr int TILE_ITEM_GOLD_HOE        = toNewTile(308);
    static constexpr int TILE_ITEM_REDSTONE_DUST   = toNewTile(309);
    static constexpr int TILE_ITEM_EMERALD         = toNewTile(310);
    static constexpr int TILE_ITEM_QUARTZ          = toNewTile(311);
    static constexpr int TILE_ITEM_APPLE           = toNewTile(312);
    static constexpr int TILE_ITEM_BREAD           = toNewTile(313);
    static constexpr int TILE_LANTERN              = toNewTile(314);
    static constexpr int TILE_SMOOTH_STONE         = toNewTile(315);

    // Pixel-Art Font Glyphs for ASCII 32 to 126 (starts at row 20, col 0 in legacy -> row 20, col 0 in 1024x1024)
    static constexpr int TILE_FONT_BASE           = toNewTile(320); // 1280

    // Shading and Tint utilities
    static constexpr int TILE_TINT_RED            = toNewTile(509);
    static constexpr int TILE_TINT_DARK           = toNewTile(510);
    static constexpr int TILE_WHITE               = toNewTile(511);

    static inline int getFontTile(char c) {
        if (c < 32 || c > 126) c = '?';
        return toNewTile(320 + (static_cast<int>(c) - 32));
    }

    // Generates unified 1024x1024 RGBA texture atlas
    static std::vector<uint8_t> generateAtlasPixels(const std::string& assetsDir = "assets/");

    // Legacy 256x512 pixel buffer generator (called by TextureStitcher to embed legacy base)
    static std::vector<uint8_t> generateLegacyPixels();

    // UV coordinates for a given tile index (0 to 4095)
    // Returns {uMin, vMin, uMax, vMax}
    static glm::vec4 getTileUV(int tileIndex);

    // Get tile index for a block type and face index (0=top, 1=bottom, 2..4=sides)
    static int getTileForBlock(BlockType type, int faceIndex);

    // Legacy fallback mapping
    static int getLegacyTileForBlock(BlockType type, int faceIndex);

    // Get tile index for block destruction stage (0 to 9)
    static int getDestroyStageTile(int stage);

    // Get RGBA pixel for a specific tile (0..4095) and local pixel coordinate (0..15, 0..15)
    // Returns true if pixel is solid (alpha > 20)
    static bool getTilePixel(int tileIndex, int px, int py, glm::vec4& rgba);
};

} // namespace prismcraft
