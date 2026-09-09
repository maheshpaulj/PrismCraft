#pragma once
#include "world/Cell.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace prismcraft {

class TextureAtlas {
public:
    static constexpr int ATLAS_WIDTH = 256;
    static constexpr int ATLAS_HEIGHT = 512;
    static constexpr int TILE_SIZE = 16;
    static constexpr int TILES_PER_ROW = ATLAS_WIDTH / TILE_SIZE; // 16
    static constexpr int TOTAL_TILES = TILES_PER_ROW * (ATLAS_HEIGHT / TILE_SIZE); // 512

    // HUD status icons (Row 16: tiles 256..265)
    static constexpr int TILE_HEART_EMPTY = 256;
    static constexpr int TILE_HEART_FULL  = 257;
    static constexpr int TILE_HEART_HALF  = 258;
    static constexpr int TILE_HUNGER_FULL  = 259;
    static constexpr int TILE_HUNGER_HALF  = 260;
    static constexpr int TILE_HUNGER_EMPTY = 261;
    static constexpr int TILE_BUBBLE       = 262;
    static constexpr int TILE_BUBBLE_POP   = 263;
    static constexpr int TILE_ARMOR_FULL   = 264;
    static constexpr int TILE_ARMOR_HALF   = 265;

    // Items & Tools (Rows 16-18: tiles 266..287, 296..301)
    static constexpr int TILE_ITEM_STICK          = 266;
    static constexpr int TILE_ITEM_WOOD_PICK      = 267;
    static constexpr int TILE_ITEM_STONE_PICK     = 268;
    static constexpr int TILE_ITEM_IRON_PICK      = 269;
    static constexpr int TILE_ITEM_DIAMOND_PICK   = 270;
    static constexpr int TILE_ITEM_WOOD_SHOVEL    = 271;
    static constexpr int TILE_ITEM_STONE_SHOVEL   = 272;
    static constexpr int TILE_ITEM_IRON_SHOVEL    = 273;
    static constexpr int TILE_ITEM_DIAMOND_SHOVEL = 274;
    static constexpr int TILE_ITEM_WOOD_AXE       = 275;
    static constexpr int TILE_ITEM_STONE_AXE      = 276;
    static constexpr int TILE_ITEM_IRON_AXE       = 277;
    static constexpr int TILE_ITEM_DIAMOND_AXE    = 278;
    static constexpr int TILE_ITEM_WOOD_SWORD     = 279;
    static constexpr int TILE_ITEM_STONE_SWORD    = 280;
    static constexpr int TILE_ITEM_IRON_SWORD     = 281;
    static constexpr int TILE_ITEM_DIAMOND_SWORD  = 282;
    static constexpr int TILE_ITEM_WOOD_HOE       = 283;
    static constexpr int TILE_ITEM_STONE_HOE      = 284;
    static constexpr int TILE_ITEM_IRON_HOE       = 285;
    static constexpr int TILE_ITEM_DIAMOND_HOE    = 286;
    static constexpr int TILE_ITEM_BOW            = 287;

    // Steve skin tiles (Row 18: tiles 288..295)
    static constexpr int TILE_STEVE_HEAD_FRONT  = 288;
    static constexpr int TILE_STEVE_HEAD_SIDE   = 289;
    static constexpr int TILE_STEVE_HEAD_TOP    = 290;
    static constexpr int TILE_STEVE_SKIN_TONE   = 291;
    static constexpr int TILE_STEVE_TORSO_FRONT = 292;
    static constexpr int TILE_STEVE_TORSO_BACK  = 293;
    static constexpr int TILE_STEVE_ARM         = 294;
    static constexpr int TILE_STEVE_LEG         = 295;

    // Remaining items (Row 18: tiles 296..301)
    static constexpr int TILE_ITEM_ARROW          = 296;
    static constexpr int TILE_ITEM_COAL           = 297;
    static constexpr int TILE_ITEM_IRON_INGOT     = 298;
    static constexpr int TILE_ITEM_DIAMOND        = 299;
    static constexpr int TILE_ITEM_STRING         = 300;
    static constexpr int TILE_ITEM_FLINT          = 301;
    static constexpr int TILE_SHADOW_DISC         = 302; // Soft circular ground contact shadow disc

    // Pixel-Art Font Glyphs for ASCII 32 to 126 (Rows 20..25: tiles 320..414)
    static constexpr int TILE_FONT_BASE           = 320;

    // Shading and Tint utilities (Row 31: tiles 509..511)
    static constexpr int TILE_TINT_RED            = 509;
    static constexpr int TILE_TINT_DARK           = 510;
    static constexpr int TILE_WHITE               = 511;

    static inline int getFontTile(char c) {
        if (c < 32 || c > 126) c = '?';
        return TILE_FONT_BASE + (static_cast<int>(c) - 32);
    }

    // Generates RGBA 256x512 pixel-art texture atlas
    static std::vector<uint8_t> generateAtlasPixels();

    // UV coordinates for a given tile index (0 to 511)
    // Returns {uMin, vMin, uMax, vMax}
    static glm::vec4 getTileUV(int tileIndex);

    // Get tile index for a block type and face index (0=top, 1=bottom, 2..4=sides)
    static int getTileForBlock(BlockType type, int faceIndex);

    // Get RGBA pixel for a specific tile (0..511) and local pixel coordinate (0..15, 0..15)
    // Returns true if pixel is solid (alpha > 20)
    static bool getTilePixel(int tileIndex, int px, int py, glm::vec4& rgba);
};

} // namespace prismcraft
