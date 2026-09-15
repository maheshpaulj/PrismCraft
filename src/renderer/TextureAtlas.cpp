#include "TextureAtlas.hpp"
#include "TextureStitcher.hpp"
#include "data/BlockRegistry.hpp"
#include "ui/FontData.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace prismcraft {

static std::vector<uint8_t> s_cachedAtlasPixels;

glm::vec4 TextureAtlas::getTileUV(int tileIndex) {
    int col = tileIndex % TILES_PER_ROW;
    int row = tileIndex / TILES_PER_ROW;

    float uMin = static_cast<float>(col * TILE_SIZE) / static_cast<float>(ATLAS_WIDTH);
    float vMin = static_cast<float>(row * TILE_SIZE) / static_cast<float>(ATLAS_HEIGHT);
    float uMax = static_cast<float>((col + 1) * TILE_SIZE) / static_cast<float>(ATLAS_WIDTH);
    float vMax = static_cast<float>((row + 1) * TILE_SIZE) / static_cast<float>(ATLAS_HEIGHT);

    return glm::vec4(uMin, vMin, uMax, vMax);
}

int TextureAtlas::getTileForBlock(BlockType type, int faceIndex) {
    if (BlockRegistry::isInitialized()) {
        int tile = BlockRegistry::getTileForBlock(type, faceIndex);
        if (tile >= 0) return tile;
    }
    int legacy = getLegacyTileForBlock(type, faceIndex);
    if (legacy >= 1024) return legacy;
    return TextureStitcher::legacyToNewTile(legacy);
}

int TextureAtlas::getDestroyStageTile(int stage) {
    if (stage < 0) return -1;
    int clamped = std::clamp(stage, 0, 9);
    if (BlockRegistry::isInitialized()) {
        std::string name = "destroy_stage_" + std::to_string(clamped);
        int tile = BlockRegistry::getTextureTile(name);
        if (tile >= 0) return tile;
    }
    return TextureStitcher::legacyToNewTile(240 + clamped);
}

int TextureAtlas::getLegacyTileForBlock(BlockType type, int faceIndex) {
    switch (type) {
        case BlockType::Grass:
            if (faceIndex == 0) return 0;   // Grass Top (0,0) greyscale tinted vibrant green
            if (faceIndex == 1) return 2;   // Dirt (0,2)
            return 3;                       // Grass Side (0,3)
        case BlockType::Dirt:        return 2;   // Dirt (0,2)
        case BlockType::Stone:       return 1;   // Stone (0,1)
        case BlockType::CobbleStone: return 16;  // Cobblestone (1,0)
        case BlockType::Bedrock:     return 17;  // Bedrock (1,1)
        case BlockType::Sand:        return 18;  // Sand (1,2)
        case BlockType::Gravel:      return 19;  // Gravel (1,3)
        case BlockType::Wood:
            if (faceIndex == 0 || faceIndex == 1) return 21; // Wood Log Top (1,5)
            return 20;                                       // Oak Wood Log Side (1,4)
        case BlockType::Planks:       return 4;   // Oak Wood Planks (0,4)
        case BlockType::PlanksPine:   return 198; // Spruce Wood Planks (12,6)
        case BlockType::PlanksBirch:  return 199; // Birch Wood Planks (12,7)
        case BlockType::PlanksJungle: return 4;   // Oak Planks
        case BlockType::Leaves:       return 52;  // Leaves Oak/Jungle Fancy transparent (3,4)
        case BlockType::Glass:        return 49;  // Glass (3,1)
        case BlockType::Water:        return 14;  // Water Still Placeholder (0,14)
        case BlockType::Ice:          return 67;  // Ice (4,3)
        case BlockType::Snow:         return 66;  // Snow Block (4,2)
        case BlockType::TallGrass:    return 39;  // Tall Grass (2,7)
        case BlockType::FlowerRose:   return 12;  // Rose / Poppy (0,12)
        case BlockType::FlowerDandelion: return 13; // Dandelion (0,13)
        case BlockType::Torch:        return 80;  // Torch (5,0)

        // Ores
        case BlockType::OreCoal:         return 34;  // Coal Ore (2,2)
        case BlockType::OreIron:         return 33;  // Iron Ore (2,1)
        case BlockType::OreGold:         return 32;  // Gold Ore (2,0)
        case BlockType::OreDiamond:      return 50;  // Diamond Ore (3,2)
        case BlockType::OreRedstone:     return 51;  // Redstone Ore (3,3)
        case BlockType::OreEmerald:      return 171; // Emerald Ore (10,11)
        case BlockType::OreNetherQuartz: return 191; // Nether Quartz Ore (11,15)

        // Mineral Blocks
        case BlockType::BlockIron:     return 22;  // Block of Iron (1,6)
        case BlockType::BlockGold:     return 23;  // Block of Gold (1,7)
        case BlockType::BlockDiamond:  return 24;  // Block of Diamond (1,8)
        case BlockType::BlockEmerald:  return 25;  // Block of Emerald (1,9)
        case BlockType::BlockRedstone: return 26;  // Redstone Dust Dot (1,10)

        // Building & Masonry
        case BlockType::Bookshelf:   return 35;  // Bookshelf (2,3)
        case BlockType::MossyCobble: return 36;  // Mossy Cobblestone (2,4)
        case BlockType::Obsidian:    return 37;  // Obsidian (2,5)
        case BlockType::Sponge:      return 48;  // Sponge (3,0)
        case BlockType::StoneBricks: return 54;  // Stone Bricks (3,6)
        case BlockType::Brick:       return 7;   // Bricks (0,7)

        // Utility
        case BlockType::CraftingTable:
            if (faceIndex == 0) return 43; // Crafting Table Top (2,11)
            if (faceIndex == 1) return 4;  // Oak Wood Planks Bottom (0,4)
            if (faceIndex == 2) return 59; // Crafting Table Front (3,11)
            return 60;                     // Crafting Table Side (3,12)
        case BlockType::Furnace:
            if (faceIndex == 2) return 44; // Furnace Front Inactive (2,12)
            if (faceIndex == 0) return 62; // Furnace Top (3,14)
            if (faceIndex == 1) return 1;  // Stone Bottom (0,1)
            return 45;                     // Furnace Side (2,13)

        // Nature & Plants
        case BlockType::Cactus:
            if (faceIndex == 0) return 69; // Cactus Top (4,5)
            if (faceIndex == 1) return 71; // Cactus Bottom (4,7)
            return 70;                     // Cactus Side (4,6)
        case BlockType::Clay:        return 72;  // Clay Block (4,8)
        case BlockType::SugarCane:   return 73;  // Sugar Canes (4,9)
        case BlockType::Pumpkin:
            if (faceIndex == 0 || faceIndex == 1) return 102; // Pumpkin Top (6,6)
            return 118;                                       // Pumpkin Side (7,6)
        case BlockType::JackOLantern:
            if (faceIndex == 0 || faceIndex == 1) return 102; // Pumpkin Top (6,6)
            if (faceIndex == 2) return 120;                  // Jack o'Lantern Face Lit (7,8)
            return 118;                                       // Pumpkin Side (7,6)
        case BlockType::Melon:
            if (faceIndex == 0 || faceIndex == 1) return 136; // Melon Top (8,8)
            return 135;                                       // Melon Side (8,7)
        case BlockType::Cake:
            if (faceIndex == 0) return 121; // Cake Top (7,9)
            if (faceIndex == 1) return 124; // Cake Bottom (7,12)
            return 122;                     // Cake Side (7,10)

        // Nether
        case BlockType::Netherrack:  return 103; // Netherrack (6,7)
        case BlockType::SoulSand:    return 104; // Soul Sand (6,8)
        case BlockType::Glowstone:   return 105; // Glowstone (6,9)
        case BlockType::NetherBrick: return 224; // Nether Bricks (14,0)

        case BlockType::Bed:
            if (faceIndex == 0) {
                int t = BlockRegistry::getTextureTile("bed_head_top");
                if (t >= 0) return t;
                return 133;
            }
            if (faceIndex == 1) {
                int t = BlockRegistry::getTextureTile("bed_underside");
                if (t >= 0) return t;
                return 4;   // Oak Planks bottom fallback
            }
            if (faceIndex == 2) {
                int t = BlockRegistry::getTextureTile("bed_side");
                if (t >= 0) return t;
                return 148;
            }
            {
                int t = BlockRegistry::getTextureTile("bed_head_end");
                if (t >= 0) return t;
                return 151;
            }
        case BlockType::DoorWood:
            if (faceIndex == 0 || faceIndex == 1) return 4;
            return 81;                      // Wooden Door Upper (5,1)
        case BlockType::DoorIron:
            if (faceIndex == 0 || faceIndex == 1) return 22;
            return 82;                      // Iron Door Upper (5,2)
        case BlockType::Sandstone:
            if (faceIndex == 0) return 176; // Sandstone Top (11,0)
            if (faceIndex == 1) return 208; // Sandstone Bottom (13,0)
            return 192;                     // Sandstone Side (12,0)
        case BlockType::TNT:
            if (faceIndex == 0) {
                int t = BlockRegistry::getTextureTile("tnt_top");
                if (t >= 0) return t;
                return 9;
            }
            if (faceIndex == 1) {
                int t = BlockRegistry::getTextureTile("tnt_bottom");
                if (t >= 0) return t;
                return 10;
            }
            {
                int t = BlockRegistry::getTextureTile("tnt_side");
                if (t >= 0) return t;
                return 8;
            }
        case BlockType::Lantern:            return TILE_LANTERN;
        case BlockType::SmoothStone:        return TILE_SMOOTH_STONE;

        // Items & Tools (Cleanly isolated in Rows 16..18: tiles 266..287, 296..315)
        case BlockType::ItemStick:          return TILE_ITEM_STICK;
        case BlockType::ItemWoodenPickaxe:  return TILE_ITEM_WOOD_PICK;
        case BlockType::ItemStonePickaxe:   return TILE_ITEM_STONE_PICK;
        case BlockType::ItemIronPickaxe:    return TILE_ITEM_IRON_PICK;
        case BlockType::ItemGoldenPickaxe:  return TILE_ITEM_GOLD_PICK;
        case BlockType::ItemDiamondPickaxe: return TILE_ITEM_DIAMOND_PICK;
        case BlockType::ItemWoodenShovel:   return TILE_ITEM_WOOD_SHOVEL;
        case BlockType::ItemStoneShovel:    return TILE_ITEM_STONE_SHOVEL;
        case BlockType::ItemIronShovel:     return TILE_ITEM_IRON_SHOVEL;
        case BlockType::ItemGoldenShovel:   return TILE_ITEM_GOLD_SHOVEL;
        case BlockType::ItemDiamondShovel:  return TILE_ITEM_DIAMOND_SHOVEL;
        case BlockType::ItemWoodenAxe:      return TILE_ITEM_WOOD_AXE;
        case BlockType::ItemStoneAxe:       return TILE_ITEM_STONE_AXE;
        case BlockType::ItemIronAxe:        return TILE_ITEM_IRON_AXE;
        case BlockType::ItemGoldenAxe:      return TILE_ITEM_GOLD_AXE;
        case BlockType::ItemDiamondAxe:     return TILE_ITEM_DIAMOND_AXE;
        case BlockType::ItemWoodenSword:    return TILE_ITEM_WOOD_SWORD;
        case BlockType::ItemStoneSword:     return TILE_ITEM_STONE_SWORD;
        case BlockType::ItemIronSword:      return TILE_ITEM_IRON_SWORD;
        case BlockType::ItemGoldenSword:    return TILE_ITEM_GOLD_SWORD;
        case BlockType::ItemDiamondSword:   return TILE_ITEM_DIAMOND_SWORD;
        case BlockType::ItemWoodenHoe:      return TILE_ITEM_WOOD_HOE;
        case BlockType::ItemStoneHoe:       return TILE_ITEM_STONE_HOE;
        case BlockType::ItemIronHoe:        return TILE_ITEM_IRON_HOE;
        case BlockType::ItemGoldenHoe:      return TILE_ITEM_GOLD_HOE;
        case BlockType::ItemDiamondHoe:     return TILE_ITEM_DIAMOND_HOE;
        case BlockType::ItemBow:            return TILE_ITEM_BOW;
        case BlockType::ItemArrow:          return TILE_ITEM_ARROW;
        case BlockType::ItemCoal:           return TILE_ITEM_COAL;
        case BlockType::ItemIronIngot:      return TILE_ITEM_IRON_INGOT;
        case BlockType::ItemGoldIngot:      return TILE_ITEM_GOLD_INGOT;
        case BlockType::ItemDiamond:        return TILE_ITEM_DIAMOND;
        case BlockType::ItemEmerald:        return TILE_ITEM_EMERALD;
        case BlockType::ItemQuartz:         return TILE_ITEM_QUARTZ;
        case BlockType::ItemRedstoneDust:   return TILE_ITEM_REDSTONE_DUST;
        case BlockType::ItemString:         return TILE_ITEM_STRING;
        case BlockType::ItemFlint:          return TILE_ITEM_FLINT;
        case BlockType::ItemApple:          return TILE_ITEM_APPLE;
        case BlockType::ItemBread:          return TILE_ITEM_BREAD;
        default:                            return TILE_WHITE;
    }
}

std::vector<uint8_t> TextureAtlas::generateAtlasPixels(const std::string& assetsDir) {
    s_cachedAtlasPixels = TextureStitcher::buildAtlas(assetsDir);
    return s_cachedAtlasPixels;
}

std::vector<uint8_t> TextureAtlas::generateLegacyPixels() {
    constexpr int LEGACY_ATLAS_WIDTH = 256;
    constexpr int LEGACY_ATLAS_HEIGHT = 512;
    constexpr int LEGACY_TILES_PER_ROW = 16;

    std::vector<uint8_t> pixels(LEGACY_ATLAS_WIDTH * LEGACY_ATLAS_HEIGHT * 4, 0);

    auto setPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        if (x < 0 || x >= LEGACY_ATLAS_WIDTH || y < 0 || y >= LEGACY_ATLAS_HEIGHT) return;
        int idx = (y * LEGACY_ATLAS_WIDTH + x) * 4;
        pixels[idx + 0] = r;
        pixels[idx + 1] = g;
        pixels[idx + 2] = b;
        pixels[idx + 3] = a;
    };

    auto setTilePixel = [&](int tileIdx, int tx, int ty, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        int legacyIdx = (tileIdx >= 1024) ? ((tileIdx / 64) * 16 + (tileIdx % 64)) : tileIdx;
        if (legacyIdx < 0 || legacyIdx >= 512) return;
        int col = legacyIdx % LEGACY_TILES_PER_ROW;
        int row = legacyIdx / LEGACY_TILES_PER_ROW;
        int px = col * TILE_SIZE + tx;
        int py = row * TILE_SIZE + ty;
        setPixel(px, py, r, g, b, a);
    };

    // Helper: deterministic hash noise for 16x16 tiles
    auto noise = [](int x, int y, int seed) -> float {
        int n = x + y * 57 + seed * 131;
        n = (n << 13) ^ n;
        return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    };

    // 1. Try to load authentic Minecraft 1.5 terrain atlas
    bool loadedFromImage = false;
    int imgW = 0, imgH = 0, imgChannels = 0;
    const char* terrainPaths[] = {
        "assets/textures/terrain.png",
        "x64/Debug/assets/textures/terrain.png",
        "x64/Release/assets/textures/terrain.png",
        "../assets/textures/terrain.png",
        "../../assets/textures/terrain.png",
        "d:/Mahesh/Coding files/PrismCraft/PrismCraft/assets/textures/terrain.png"
    };
    for (const char* p : terrainPaths) {
        stbi_uc* imgData = stbi_load(p, &imgW, &imgH, &imgChannels, 4);
        if (imgData) {
            if (imgW == LEGACY_ATLAS_WIDTH && (imgH == 256 || imgH == LEGACY_ATLAS_HEIGHT)) {
                int copyH = std::min(imgH, 256);
                std::memcpy(pixels.data(), imgData, LEGACY_ATLAS_WIDTH * copyH * 4);
                loadedFromImage = true;
                std::cout << "[TextureAtlas] Loaded authentic terrain.png from " << p << std::endl;
            }
            stbi_image_free(imgData);
            break;
        }
    }

    if (loadedFromImage) {
        // Biome Grass & Foliage Tinting (Minecraft terrain.png has greyscale grass top and leaves)
        // Tint Tile 0: monochromatic grass(top) (col 0, row 0) with lush vibrant green
        {
            int col = 0;
            int row = 0;
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    int idx = ((row * 16 + y) * LEGACY_ATLAS_WIDTH + (col * 16 + x)) * 4;
                    float grey = pixels[idx] / 255.0f;
                    pixels[idx + 0] = static_cast<uint8_t>(std::clamp(grey * 110.0f, 0.0f, 255.0f));
                    pixels[idx + 1] = static_cast<uint8_t>(std::clamp(grey * 190.0f, 0.0f, 255.0f));
                    pixels[idx + 2] = static_cast<uint8_t>(std::clamp(grey * 65.0f, 0.0f, 255.0f));
                    pixels[idx + 3] = 255;
                }
            }
        }

        // Tint Tile 52 & 53 (Leaves: Fancy tile 52, Fast tile 53)
        for (int tile : {52, 53}) {
            int col = tile % LEGACY_TILES_PER_ROW;
            int row = tile / LEGACY_TILES_PER_ROW;
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    int idx = ((row * 16 + y) * LEGACY_ATLAS_WIDTH + (col * 16 + x)) * 4;
                    if (pixels[idx + 3] > 20) {
                        float grey = pixels[idx] / 255.0f;
                        pixels[idx + 0] = static_cast<uint8_t>(std::clamp(grey * 115.0f, 0.0f, 255.0f));
                        pixels[idx + 1] = static_cast<uint8_t>(std::clamp(grey * 210.0f, 0.0f, 255.0f));
                        pixels[idx + 2] = static_cast<uint8_t>(std::clamp(grey * 45.0f, 0.0f, 255.0f));
                    }
                }
            }
        }

        // Tint Tile 39 (Tall Grass)
        {
            int col = 39 % LEGACY_TILES_PER_ROW;
            int row = 39 / LEGACY_TILES_PER_ROW;
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    int idx = ((row * 16 + y) * LEGACY_ATLAS_WIDTH + (col * 16 + x)) * 4;
                    if (pixels[idx + 3] > 20) {
                        float grey = pixels[idx] / 255.0f;
                        pixels[idx + 0] = static_cast<uint8_t>(std::clamp(grey * 110.0f, 0.0f, 255.0f));
                        pixels[idx + 1] = static_cast<uint8_t>(std::clamp(grey * 190.0f, 0.0f, 255.0f));
                        pixels[idx + 2] = static_cast<uint8_t>(std::clamp(grey * 65.0f, 0.0f, 255.0f));
                    }
                }
            }
        }
    }

    if (!loadedFromImage) {
    // Tile 0: Grass Top (Lush vibrant green pixel-art)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 101);
            uint8_t g = static_cast<uint8_t>(std::clamp(170 + static_cast<int>(n * 40.0f), 130, 220));
            uint8_t r = static_cast<uint8_t>(g * 0.45f);
            uint8_t b = static_cast<uint8_t>(g * 0.25f);
            setTilePixel(0, x, y, r, g, b);
        }
    }

    // Tile 1: Grass Side (Green overhang top with jagged dirt underneath)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            int overhang = 3 + static_cast<int>(std::abs(noise(x, 0, 202)) * 3.0f);
            if (y < overhang) {
                // Green grass fringe
                float n = noise(x, y, 203);
                uint8_t g = static_cast<uint8_t>(std::clamp(160 + static_cast<int>(n * 35.0f), 120, 210));
                setTilePixel(1, x, y, static_cast<uint8_t>(g * 0.45f), g, static_cast<uint8_t>(g * 0.25f));
            } else {
                // Dirt base
                float n = noise(x, y, 303);
                uint8_t val = static_cast<uint8_t>(std::clamp(110 + static_cast<int>(n * 30.0f), 70, 150));
                setTilePixel(1, x, y, val, static_cast<uint8_t>(val * 0.65f), static_cast<uint8_t>(val * 0.45f));
            }
        }
    }

    // Tile 2: Dirt
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 303);
            uint8_t val = static_cast<uint8_t>(std::clamp(115 + static_cast<int>(n * 35.0f), 75, 155));
            setTilePixel(2, x, y, val, static_cast<uint8_t>(val * 0.68f), static_cast<uint8_t>(val * 0.46f));
        }
    }

    // Tile 3: Stone (Natural rugged gray stone)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 404);
            uint8_t val = static_cast<uint8_t>(std::clamp(128 + static_cast<int>(n * 32.0f), 85, 170));
            setTilePixel(3, x, y, val, val, val);
        }
    }

    // Tile 4: Cobblestone (Cracked stones with mortar borders)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            bool isMortar = (x == 0 || x == 7 || x == 8 || x == 15 || y == 0 || y == 7 || y == 8 || y == 15);
            float n = noise(x, y, 505);
            uint8_t base = isMortar ? 70 : static_cast<uint8_t>(std::clamp(135 + static_cast<int>(n * 35.0f), 90, 180));
            setTilePixel(4, x, y, base, base, base);
        }
    }

    // Tile 5: Wood Log Side (Bark stripes)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float bark = std::sin(static_cast<float>(x) * 1.8f) * 25.0f + noise(x, y, 606) * 20.0f;
            uint8_t r = static_cast<uint8_t>(std::clamp(110 + static_cast<int>(bark), 70, 155));
            setTilePixel(5, x, y, r, static_cast<uint8_t>(r * 0.65f), static_cast<uint8_t>(r * 0.38f));
        }
    }

    // Tile 6: Wood Log Top (Concentric rings)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float dist = std::sqrt(static_cast<float>((x - 8)*(x - 8) + (y - 8)*(y - 8)));
            if (dist > 7.0f) {
                setTilePixel(6, x, y, 90, 60, 35); // Bark ring
            } else {
                float ring = std::sin(dist * 2.2f) * 20.0f;
                uint8_t r = static_cast<uint8_t>(std::clamp(170 + static_cast<int>(ring), 130, 205));
                setTilePixel(6, x, y, r, static_cast<uint8_t>(r * 0.78f), static_cast<uint8_t>(r * 0.50f));
            }
        }
    }

    // Tile 7: Wood Planks (Horizontal boards with nails)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            bool isSeam = (y == 3 || y == 7 || y == 11 || y == 15);
            float n = noise(x, y, 707);
            uint8_t r = isSeam ? 85 : static_cast<uint8_t>(std::clamp(185 + static_cast<int>(n * 25.0f), 140, 220));
            setTilePixel(7, x, y, r, static_cast<uint8_t>(r * 0.74f), static_cast<uint8_t>(r * 0.44f));
        }
    }

    // Tile 8: Leaves (Proper see-through transparent pixel cutouts!)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 808);
            // 30% of pixels are transparent holes for see-through oak foliage!
            if (n < -0.25f && ((x + y) % 2 == 0)) {
                setTilePixel(8, x, y, 0, 0, 0, 0); // Transparent cutout!
            } else {
                uint8_t g = static_cast<uint8_t>(std::clamp(160 + static_cast<int>(n * 45.0f), 100, 215));
                setTilePixel(8, x, y, static_cast<uint8_t>(g * 0.35f), g, static_cast<uint8_t>(g * 0.20f), 255);
            }
        }
    }

    // Tile 9: Sand (Warm beach sand)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 909);
            uint8_t r = static_cast<uint8_t>(std::clamp(225 + static_cast<int>(n * 20.0f), 195, 245));
            setTilePixel(9, x, y, r, static_cast<uint8_t>(r * 0.90f), static_cast<uint8_t>(r * 0.65f));
        }
    }

    // Tile 10: Glass (Beveled glass frame + transparent center with diagonal glare streaks)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            bool isBorder = (x == 0 || x == 15 || y == 0 || y == 15);
            bool isGlare = ((x == y + 3) && x >= 4 && x <= 8) || ((x == y - 4) && x >= 8 && x <= 12);
            if (isBorder) {
                setTilePixel(10, x, y, 200, 225, 240, 220); // Border frame
            } else if (isGlare) {
                setTilePixel(10, x, y, 255, 255, 255, 180); // Reflective glare streak
            } else {
                setTilePixel(10, x, y, 180, 215, 240, 0);   // Fully transparent see-through center!
            }
        }
    }

    // Tile 11: Water (Translucent vibrant Minecraft blue water with ripples)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float wave = std::sin(static_cast<float>(x * 2 + y) * 0.5f) * 20.0f + std::cos(static_cast<float>(y * 2 - x) * 0.5f) * 15.0f;
            uint8_t r = static_cast<uint8_t>(std::clamp(45 + static_cast<int>(wave * 0.4f), 30, 75));
            uint8_t g = static_cast<uint8_t>(std::clamp(115 + static_cast<int>(wave * 0.7f), 90, 155));
            uint8_t b = static_cast<uint8_t>(std::clamp(225 + static_cast<int>(wave), 195, 255));
            setTilePixel(11, x, y, r, g, b, 170); // 170/255 = ~67% opacity translucent
        }
    }

    // Tile 12: Bedrock (Indestructible dark stone)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 1212);
            uint8_t val = static_cast<uint8_t>(std::clamp(45 + static_cast<int>(n * 35.0f), 15, 90));
            setTilePixel(12, x, y, val, val, val);
        }
    }

    // Tile 13: Coal Ore (Stone with dark coal flecks)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 404);
            uint8_t val = static_cast<uint8_t>(std::clamp(128 + static_cast<int>(n * 32.0f), 85, 170));
            // Coal flecks
            bool isOre = ((x >= 3 && x <= 6 && y >= 3 && y <= 6) || (x >= 9 && x <= 13 && y >= 8 && y <= 12));
            if (isOre && (x + y) % 2 == 0) {
                setTilePixel(13, x, y, 25, 25, 25);
            } else {
                setTilePixel(13, x, y, val, val, val);
            }
        }
    }

    // Tile 14: Iron Ore (Stone with tan/orange iron chunks)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 404);
            uint8_t val = static_cast<uint8_t>(std::clamp(128 + static_cast<int>(n * 32.0f), 85, 170));
            bool isOre = ((x >= 4 && x <= 7 && y >= 7 && y <= 11) || (x >= 10 && x <= 13 && y >= 3 && y <= 6));
            if (isOre && (x * y) % 3 != 0) {
                setTilePixel(14, x, y, 205, 160, 130);
            } else {
                setTilePixel(14, x, y, val, val, val);
            }
        }
    }

    // Tile 15: Gold Ore
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 404);
            uint8_t val = static_cast<uint8_t>(std::clamp(128 + static_cast<int>(n * 32.0f), 85, 170));
            bool isOre = ((x >= 5 && x <= 8 && y >= 4 && y <= 8) || (x >= 9 && x <= 12 && y >= 10 && y <= 13));
            if (isOre && (x + y) % 2 == 0) {
                setTilePixel(15, x, y, 245, 215, 45);
            } else {
                setTilePixel(15, x, y, val, val, val);
            }
        }
    }

    // Tile 16: Diamond Ore (Radiant cyan gems)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 404);
            uint8_t val = static_cast<uint8_t>(std::clamp(128 + static_cast<int>(n * 32.0f), 85, 170));
            bool isOre = ((x >= 4 && x <= 7 && y >= 4 && y <= 7) || (x >= 8 && x <= 12 && y >= 8 && y <= 12));
            if (isOre && (x + y) % 2 == 0) {
                setTilePixel(16, x, y, 65, 235, 240);
            } else {
                setTilePixel(16, x, y, val, val, val);
            }
        }
    }

    // Tile 17: Crafting Table Top (Grid pattern with saw)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            bool isBorder = (x == 0 || x == 15 || y == 0 || y == 15 || x == 7 || x == 8 || y == 7 || y == 8);
            if (isBorder) {
                setTilePixel(17, x, y, 95, 65, 35);
            } else {
                setTilePixel(17, x, y, 175, 130, 75);
            }
        }
    }

    // Tile 18: Crafting Table Side
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            setTilePixel(18, x, y, 150, 110, 60);
        }
    }

    // Tile 19: Furnace Front (Dark stone arch)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            bool isMouth = (x >= 4 && x <= 11 && y >= 6 && y <= 13);
            if (isMouth) {
                setTilePixel(19, x, y, 35, 35, 40);
            } else {
                float n = noise(x, y, 505);
                uint8_t val = static_cast<uint8_t>(std::clamp(125 + static_cast<int>(n * 30.0f), 85, 165));
                setTilePixel(19, x, y, val, val, val);
            }
        }
    }
        // Fallback Block Destroy Stages 0 to 9 (Crack animations in bottom row of atlas when not loaded from image)
        for (int stage = 0; stage < 10; ++stage) {
            int tile = 240 + stage;
            int threshold = (stage + 1) * 3;
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    int crack = (std::abs(x - y) < (1 + stage / 3) && x > 2 && x < 14) ||
                                (std::abs((15 - x) - y) < (1 + stage / 4) && y > 3 && y < 13) ||
                                ((x == 8 || y == 8) && (x + y) % 2 == 0 && stage > 4);
                    if (crack && (x * 7 + y * 13) % 10 <= threshold) {
                        setTilePixel(tile, x, y, 20, 20, 20, 210);
                    } else {
                        setTilePixel(tile, x, y, 0, 0, 0, 0);
                    }
                }
            }
        }
    } // End if (!loadedFromImage)

    // =========================================================================
    // Authentic Tools & Items in Custom Rows 16..18 (Tiles 266..287, 296..301)
    // =========================================================================
    // Tile 266: Stick
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_STICK, x, y, 0, 0, 0, 0);
    for (int i = 3; i <= 12; ++i) {
        setTilePixel(TILE_ITEM_STICK, i, 15 - i, 101, 74, 40, 255);
        setTilePixel(TILE_ITEM_STICK, i - 1, 15 - i, 66, 47, 24, 255);
        setTilePixel(TILE_ITEM_STICK, i, 15 - i + 1, 66, 47, 24, 255);
    }

    struct ToolPalette {
        uint8_t outline_r, outline_g, outline_b;
        uint8_t main_r,    main_g,    main_b;
        uint8_t light_r,   light_g,   light_b;
        uint8_t dark_r,    dark_g,    dark_b;
    };

    ToolPalette tierPalettes[5] = {
        // 0: Wood (Oak timber)
        { 55, 38, 18,    135, 102, 60,   170, 135, 88,    98, 70, 36 },
        // 1: Stone (Granite / Cobblestone)
        { 55, 55, 60,    130, 130, 135,  175, 175, 180,   90, 90, 95 },
        // 2: Iron (Metallic steel)
        { 105, 105, 115, 210, 212, 220,  248, 248, 252,   160, 162, 170 },
        // 3: Diamond (Vibrant cyan/teal with luminous highlights)
        { 18, 92, 102,   46, 204, 218,   125, 248, 255,   28, 142, 155 },
        // 4: Gold (Glistening golden yellow with rich amber shadow)
        { 120, 85, 10,   245, 205, 45,   255, 235, 90,    190, 150, 20 }
    };

    auto drawHandle = [&](int tile, int startIdx = 2, int endIdx = 9) {
        setTilePixel(tile, 1, 14, 66, 47, 24, 255);
        setTilePixel(tile, 2, 14, 50, 34, 16, 255);
        for (int i = startIdx; i <= endIdx; ++i) {
            setTilePixel(tile, i, 15 - i, 101, 74, 40, 255);
            setTilePixel(tile, i - 1, 15 - i, 66, 47, 24, 255);
            setTilePixel(tile, i, 15 - i + 1, 66, 47, 24, 255);
        }
    };

    // 1. Pickaxes
    int pickTileIDs[5] = { TILE_ITEM_WOOD_PICK, TILE_ITEM_STONE_PICK, TILE_ITEM_IRON_PICK, TILE_ITEM_DIAMOND_PICK, TILE_ITEM_GOLD_PICK };
    for (int t = 0; t < 5; ++t) {
        int tile = pickTileIDs[t];
        const auto& pal = tierPalettes[t];
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        drawHandle(tile, 2, 9);

        // Left pick arm
        setTilePixel(tile, 2, 7, pal.outline_r, pal.outline_g, pal.outline_b, 255);
        setTilePixel(tile, 2, 6, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 3, 6, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 3, 5, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 4, 5, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 5, 4, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 6, 4, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 6, 3, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 7, 3, pal.light_r, pal.light_g, pal.light_b, 255);

        // Right pick arm
        setTilePixel(tile, 7, 2, pal.outline_r, pal.outline_g, pal.outline_b, 255);
        setTilePixel(tile, 8, 2, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 8, 3, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 9, 3, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 9, 4, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 10, 4, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 11, 4, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 11, 5, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 12, 5, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 12, 6, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 13, 6, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 13, 7, pal.outline_r, pal.outline_g, pal.outline_b, 255);
    }

    // 2. Shovels
    int shovelTileIDs[5] = { TILE_ITEM_WOOD_SHOVEL, TILE_ITEM_STONE_SHOVEL, TILE_ITEM_IRON_SHOVEL, TILE_ITEM_DIAMOND_SHOVEL, TILE_ITEM_GOLD_SHOVEL };
    for (int t = 0; t < 5; ++t) {
        int tile = shovelTileIDs[t];
        const auto& pal = tierPalettes[t];
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        drawHandle(tile, 2, 8);

        setTilePixel(tile, 8, 7, pal.outline_r, pal.outline_g, pal.outline_b, 255);
        setTilePixel(tile, 9, 6, pal.dark_r, pal.dark_g, pal.dark_b, 255);

        setTilePixel(tile, 9, 5, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 10, 4, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 11, 3, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 12, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 13, 2, pal.light_r, pal.light_g, pal.light_b, 255);

        setTilePixel(tile, 10, 5, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 11, 4, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 12, 3, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 13, 3, pal.main_r, pal.main_g, pal.main_b, 255);

        setTilePixel(tile, 11, 5, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 12, 4, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 13, 4, pal.outline_r, pal.outline_g, pal.outline_b, 255);
        setTilePixel(tile, 12, 5, pal.outline_r, pal.outline_g, pal.outline_b, 255);
    }

    // 3. Axes
    int axeTileIDs[5] = { TILE_ITEM_WOOD_AXE, TILE_ITEM_STONE_AXE, TILE_ITEM_IRON_AXE, TILE_ITEM_DIAMOND_AXE, TILE_ITEM_GOLD_AXE };
    for (int t = 0; t < 5; ++t) {
        int tile = axeTileIDs[t];
        const auto& pal = tierPalettes[t];
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        drawHandle(tile, 2, 9);

        setTilePixel(tile, 7, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 8, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 9, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 10, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 11, 2, pal.light_r, pal.light_g, pal.light_b, 255);

        for (int x = 8; x <= 12; ++x) setTilePixel(tile, x, 3, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 12, 2, pal.light_r, pal.light_g, pal.light_b, 255);

        setTilePixel(tile, 9, 4, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 10, 4, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 11, 4, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 12, 4, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 13, 4, pal.light_r, pal.light_g, pal.light_b, 255);

        setTilePixel(tile, 10, 5, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 11, 5, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 12, 5, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 13, 5, pal.light_r, pal.light_g, pal.light_b, 255);

        setTilePixel(tile, 11, 6, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 12, 6, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 13, 6, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 12, 7, pal.outline_r, pal.outline_g, pal.outline_b, 255);
    }

    // 4. Swords
    int swordTileIDs[5] = { TILE_ITEM_WOOD_SWORD, TILE_ITEM_STONE_SWORD, TILE_ITEM_IRON_SWORD, TILE_ITEM_DIAMOND_SWORD, TILE_ITEM_GOLD_SWORD };
    for (int t = 0; t < 5; ++t) {
        int tile = swordTileIDs[t];
        const auto& pal = tierPalettes[t];
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        setTilePixel(tile, 1, 14, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 2, 14, pal.outline_r, pal.outline_g, pal.outline_b, 255);
        setTilePixel(tile, 1, 13, pal.outline_r, pal.outline_g, pal.outline_b, 255);

        setTilePixel(tile, 2, 13, 101, 74, 40, 255);
        setTilePixel(tile, 3, 12, 101, 74, 40, 255);
        setTilePixel(tile, 4, 11, 66, 47, 24, 255);

        setTilePixel(tile, 2, 10, pal.outline_r, pal.outline_g, pal.outline_b, 255);
        setTilePixel(tile, 3, 10, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 4, 10, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 4, 9, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 5, 9, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 5, 8, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 6, 8, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 7, 8, pal.outline_r, pal.outline_g, pal.outline_b, 255);

        for (int i = 5; i <= 13; ++i) {
            setTilePixel(tile, i, 15 - i, pal.light_r, pal.light_g, pal.light_b, 255);
            setTilePixel(tile, i - 1, 15 - i, pal.main_r, pal.main_g, pal.main_b, 255);
            setTilePixel(tile, i, 15 - i + 1, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        }
        setTilePixel(tile, 14, 1, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 15, 0, pal.light_r, pal.light_g, pal.light_b, 255);
    }

    // 5. Hoes
    int hoeTileIDs[5] = { TILE_ITEM_WOOD_HOE, TILE_ITEM_STONE_HOE, TILE_ITEM_IRON_HOE, TILE_ITEM_DIAMOND_HOE, TILE_ITEM_GOLD_HOE };
    for (int t = 0; t < 5; ++t) {
        int tile = hoeTileIDs[t];
        const auto& pal = tierPalettes[t];
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        drawHandle(tile, 2, 9);

        setTilePixel(tile, 6, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 7, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 8, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 9, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 10, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 11, 2, pal.light_r, pal.light_g, pal.light_b, 255);
        setTilePixel(tile, 12, 2, pal.light_r, pal.light_g, pal.light_b, 255);

        setTilePixel(tile, 12, 3, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 12, 4, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 11, 4, pal.outline_r, pal.outline_g, pal.outline_b, 255);

        setTilePixel(tile, 7, 3, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 8, 3, pal.main_r, pal.main_g, pal.main_b, 255);
        setTilePixel(tile, 9, 3, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 10, 3, pal.dark_r, pal.dark_g, pal.dark_b, 255);
        setTilePixel(tile, 11, 3, pal.main_r, pal.main_g, pal.main_b, 255);
    }

    // 6. Bow
    {
        int tile = TILE_ITEM_BOW;
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        setTilePixel(tile, 12, 2, 66, 47, 24, 255);
        setTilePixel(tile, 11, 2, 101, 74, 40, 255);
        setTilePixel(tile, 11, 3, 140, 105, 55, 255);

        setTilePixel(tile, 10, 3, 101, 74, 40, 255);
        setTilePixel(tile, 9, 4, 101, 74, 40, 255);
        setTilePixel(tile, 8, 5, 140, 105, 55, 255);
        setTilePixel(tile, 7, 6, 101, 74, 40, 255);
        setTilePixel(tile, 6, 7, 101, 74, 40, 255);

        setTilePixel(tile, 5, 8, 66, 47, 24, 255);
        setTilePixel(tile, 5, 9, 66, 47, 24, 255);

        setTilePixel(tile, 6, 10, 101, 74, 40, 255);
        setTilePixel(tile, 7, 11, 101, 74, 40, 255);
        setTilePixel(tile, 8, 12, 140, 105, 55, 255);
        setTilePixel(tile, 9, 13, 101, 74, 40, 255);
        setTilePixel(tile, 10, 13, 101, 74, 40, 255);

        setTilePixel(tile, 11, 14, 66, 47, 24, 255);

        setTilePixel(tile, 11, 4, 220, 220, 225, 255);
        setTilePixel(tile, 10, 5, 220, 220, 225, 255);
        setTilePixel(tile, 9, 6, 220, 220, 225, 255);
        setTilePixel(tile, 8, 7, 220, 220, 225, 255);
        setTilePixel(tile, 7, 8, 220, 220, 225, 255);
        setTilePixel(tile, 7, 9, 220, 220, 225, 255);
        setTilePixel(tile, 8, 10, 220, 220, 225, 255);
        setTilePixel(tile, 9, 11, 220, 220, 225, 255);
        setTilePixel(tile, 10, 12, 220, 220, 225, 255);
    }

    // 7. Arrow
    {
        int tile = TILE_ITEM_ARROW;
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        for (int i = 3; i <= 10; ++i) {
            setTilePixel(tile, i, 15 - i, 101, 74, 40, 255);
        }

        setTilePixel(tile, 1, 14, 180, 180, 185, 255);
        setTilePixel(tile, 2, 14, 225, 225, 230, 255);
        setTilePixel(tile, 2, 13, 225, 225, 230, 255);
        setTilePixel(tile, 3, 14, 160, 160, 165, 255);
        setTilePixel(tile, 1, 13, 160, 160, 165, 255);

        setTilePixel(tile, 11, 4, 75, 75, 80, 255);
        setTilePixel(tile, 12, 3, 120, 120, 125, 255);
        setTilePixel(tile, 13, 2, 160, 160, 165, 255);
        setTilePixel(tile, 10, 4, 60, 60, 65, 255);
        setTilePixel(tile, 12, 5, 60, 60, 65, 255);
    }

    // 8. Coal Item
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_COAL, x, y, 0, 0, 0, 0);
    for (int y = 4; y <= 11; ++y) {
        for (int x = 4; x <= 11; ++x) {
            float n = noise(x, y, 162);
            uint8_t c = (uint8_t)std::clamp(35 + (int)(n * 15.0f), 20, 55);
            setTilePixel(TILE_ITEM_COAL, x, y, c, c, c + 2, 255);
        }
    }
    setTilePixel(TILE_ITEM_COAL, 5, 5, 75, 75, 80, 255);

    // 9. Iron Ingot
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_IRON_INGOT, x, y, 0, 0, 0, 0);
    for (int y = 5; y <= 10; ++y) {
        for (int x = 3; x <= 12; ++x) {
            uint8_t val = (y == 5) ? 235 : ((y == 10) ? 140 : 190);
            setTilePixel(TILE_ITEM_IRON_INGOT, x, y, val, val, val + 5, 255);
        }
    }

    // 10. Diamond Gem
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_DIAMOND, x, y, 0, 0, 0, 0);
    for (int y = 4; y <= 12; ++y) {
        int halfW = (y <= 7) ? (y - 3) * 2 : (12 - y) * 2;
        for (int x = 8 - halfW; x <= 8 + halfW; ++x) {
            setTilePixel(TILE_ITEM_DIAMOND, x, y, 40, 215, 230, 255);
        }
    }
    setTilePixel(TILE_ITEM_DIAMOND, 7, 5, 180, 250, 255, 255);

    // 11. String
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_STRING, x, y, 0, 0, 0, 0);
    for (int i = 3; i <= 12; ++i) {
        int sy = 8 + (int)(std::sin(i * 0.8f) * 3.0f);
        setTilePixel(TILE_ITEM_STRING, i, sy, 220, 220, 225, 255);
        setTilePixel(TILE_ITEM_STRING, i, sy + 1, 180, 180, 185, 255);
    }

    // 12. Flint
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_FLINT, x, y, 0, 0, 0, 0);
    for (int y = 4; y <= 11; ++y) {
        int w = (y <= 7) ? (y - 3) : (11 - y);
        for (int x = 7 - w; x <= 8 + w; ++x) {
            uint8_t c = (x == 7 - w) ? 90 : 50;
            setTilePixel(TILE_ITEM_FLINT, x, y, c, c, c + 5, 255);
        }
    }

    // 13. Gold Ingot
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_GOLD_INGOT, x, y, 0, 0, 0, 0);
    for (int y = 5; y <= 10; ++y) {
        for (int x = 3; x <= 12; ++x) {
            if (y == 5) {
                setTilePixel(TILE_ITEM_GOLD_INGOT, x, y, 255, 240, 110, 255);
            } else if (y == 10) {
                setTilePixel(TILE_ITEM_GOLD_INGOT, x, y, 175, 135, 18, 255);
            } else {
                setTilePixel(TILE_ITEM_GOLD_INGOT, x, y, 245, 205, 45, 255);
            }
        }
    }
    setTilePixel(TILE_ITEM_GOLD_INGOT, 4, 6, 255, 250, 160, 255);

    // 14. Redstone Dust
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_REDSTONE_DUST, x, y, 0, 0, 0, 0);
    for (int y = 6; y <= 12; ++y) {
        int w = (y <= 9) ? (y - 5) * 2 : (13 - y) * 2;
        for (int x = 8 - w; x <= 8 + w; ++x) {
            float n = noise(x, y, 241);
            if (n > 0.3f) {
                setTilePixel(TILE_ITEM_REDSTONE_DUST, x, y, 235, 25, 25, 255);
            } else {
                setTilePixel(TILE_ITEM_REDSTONE_DUST, x, y, 165, 15, 15, 255);
            }
        }
    }
    setTilePixel(TILE_ITEM_REDSTONE_DUST, 7, 7, 255, 120, 120, 255);
    setTilePixel(TILE_ITEM_REDSTONE_DUST, 9, 8, 255, 140, 140, 255);

    // 15. Emerald Gem
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_EMERALD, x, y, 0, 0, 0, 0);
    for (int y = 4; y <= 12; ++y) {
        int halfW = (y <= 7) ? (y - 3) * 2 : (12 - y) * 2;
        for (int x = 8 - halfW; x <= 8 + halfW; ++x) {
            setTilePixel(TILE_ITEM_EMERALD, x, y, 30, 205, 80, 255);
        }
    }
    setTilePixel(TILE_ITEM_EMERALD, 7, 5, 140, 255, 175, 255);
    setTilePixel(TILE_ITEM_EMERALD, 8, 5, 140, 255, 175, 255);

    // 16. Nether Quartz
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_QUARTZ, x, y, 0, 0, 0, 0);
    for (int y = 3; y <= 12; ++y) {
        int w = (y <= 6) ? (y - 2) : (12 - y);
        for (int x = 8 - w; x <= 8 + w; ++x) {
            uint8_t c = (x <= 8) ? 245 : 205;
            setTilePixel(TILE_ITEM_QUARTZ, x, y, c, c - 5, c + 5, 255);
        }
    }
    setTilePixel(TILE_ITEM_QUARTZ, 7, 4, 255, 255, 255, 255);

    // 17. Apple
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_APPLE, x, y, 0, 0, 0, 0);
    // Stem and leaf
    setTilePixel(TILE_ITEM_APPLE, 7, 3, 100, 65, 30, 255);
    setTilePixel(TILE_ITEM_APPLE, 8, 2, 100, 65, 30, 255);
    setTilePixel(TILE_ITEM_APPLE, 9, 2, 50, 195, 40, 255);
    setTilePixel(TILE_ITEM_APPLE, 10, 2, 50, 195, 40, 255);
    setTilePixel(TILE_ITEM_APPLE, 9, 3, 35, 150, 30, 255);
    // Apple fruit
    for (int y = 4; y <= 12; ++y) {
        for (int x = 4; x <= 11; ++x) {
            if ((y == 4 && (x == 4 || x == 11 || x == 7 || x == 8)) ||
                (y == 12 && (x == 4 || x == 11 || x == 7 || x == 8))) continue;
            setTilePixel(TILE_ITEM_APPLE, x, y, 220, 28, 28, 255);
        }
    }
    setTilePixel(TILE_ITEM_APPLE, 5, 5, 255, 130, 130, 255);
    setTilePixel(TILE_ITEM_APPLE, 6, 5, 255, 100, 100, 255);

    // 18. Bread
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_ITEM_BREAD, x, y, 0, 0, 0, 0);
    for (int y = 6; y <= 10; ++y) {
        for (int x = 2; x <= 13; ++x) {
            if ((y == 6 || y == 10) && (x <= 3 || x >= 12)) continue;
            setTilePixel(TILE_ITEM_BREAD, x, y, 195, 130, 50, 255);
        }
    }
    // Crust scores
    setTilePixel(TILE_ITEM_BREAD, 5, 7, 120, 65, 18, 255);
    setTilePixel(TILE_ITEM_BREAD, 6, 8, 120, 65, 18, 255);
    setTilePixel(TILE_ITEM_BREAD, 8, 7, 120, 65, 18, 255);
    setTilePixel(TILE_ITEM_BREAD, 9, 8, 120, 65, 18, 255);
    setTilePixel(TILE_ITEM_BREAD, 11, 7, 120, 65, 18, 255);
    setTilePixel(TILE_ITEM_BREAD, 5, 6, 235, 185, 95, 255);
    setTilePixel(TILE_ITEM_BREAD, 8, 6, 235, 185, 95, 255);

    // 19. Lantern Block (Tile 314)
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(TILE_LANTERN, x, y, 0, 0, 0, 0);
    // Hanging ring
    setTilePixel(TILE_LANTERN, 7, 1, 65, 68, 75, 255);
    setTilePixel(TILE_LANTERN, 8, 1, 65, 68, 75, 255);
    setTilePixel(TILE_LANTERN, 7, 2, 65, 68, 75, 255);
    setTilePixel(TILE_LANTERN, 8, 2, 65, 68, 75, 255);
    // Cap
    for (int x = 5; x <= 10; ++x) setTilePixel(TILE_LANTERN, x, 3, 50, 52, 60, 255);
    for (int x = 4; x <= 11; ++x) setTilePixel(TILE_LANTERN, x, 4, 45, 48, 55, 255);
    // Glass & Flame
    for (int y = 5; y <= 11; ++y) {
        for (int x = 4; x <= 11; ++x) {
            if (x == 4 || x == 11) {
                setTilePixel(TILE_LANTERN, x, y, 45, 48, 55, 255); // Struts
            } else if (y >= 7 && y <= 9 && x >= 7 && x <= 8) {
                setTilePixel(TILE_LANTERN, x, y, 255, 245, 190, 255); // Core Flame
            } else {
                setTilePixel(TILE_LANTERN, x, y, 255, 175, 40, 230); // Glowing amber glass
            }
        }
    }
    // Base
    for (int x = 4; x <= 11; ++x) setTilePixel(TILE_LANTERN, x, 12, 45, 48, 55, 255);
    for (int x = 5; x <= 10; ++x) setTilePixel(TILE_LANTERN, x, 13, 50, 52, 60, 255);

    // 20. Smooth Stone (Tile 315)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float n = noise(x, y, 315);
            uint8_t baseC = static_cast<uint8_t>(std::clamp(158 + (int)(n * 10.0f), 145, 172));
            if (y == 0 || x == 0) {
                setTilePixel(TILE_SMOOTH_STONE, x, y, 185, 185, 190, 255); // Bevel light
            } else if (y == 15 || x == 15) {
                setTilePixel(TILE_SMOOTH_STONE, x, y, 120, 120, 125, 255); // Bevel shadow
            } else {
                setTilePixel(TILE_SMOOTH_STONE, x, y, baseC, baseC, baseC + 2, 255);
            }
        }
    }

    // =========================================================================
    // HUD Status Line Pixel Art (Tiles 256..265)
    // Authentic Minecraft GUI Heart, Hunger, Bubble, and Armor Sprites
    // =========================================================================
    // Tile 256: Heart Empty (Dark grey container with inner shadow)
    {
        int tile = TILE_HEART_EMPTY;
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        const uint8_t heartMask[9][9] = {
            {0, 1, 1, 0, 0, 0, 1, 1, 0},
            {1, 2, 2, 1, 0, 1, 2, 2, 1},
            {1, 2, 2, 2, 1, 2, 2, 2, 1},
            {1, 2, 2, 2, 2, 2, 2, 2, 1},
            {1, 2, 2, 2, 2, 2, 2, 2, 1},
            {0, 1, 2, 2, 2, 2, 2, 1, 0},
            {0, 0, 1, 2, 2, 2, 1, 0, 0},
            {0, 0, 0, 1, 2, 1, 0, 0, 0},
            {0, 0, 0, 0, 1, 0, 0, 0, 0}
        };

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                int px = 3 + c;
                int py = 3 + r;
                uint8_t m = heartMask[r][c];
                if (m == 1) {
                    setTilePixel(tile, px, py, 20, 20, 20, 255); // Dark outline
                } else if (m >= 2) {
                    setTilePixel(tile, px, py, 45, 12, 12, 255); // Dark container red
                }
            }
        }
    }

    // Tile 241: Heart Full (Iconic red Minecraft heart with white sparkle)
    {
        int tile = TILE_HEART_FULL; // 241
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        const uint8_t fullMask[9][9] = {
            {0, 1, 1, 0, 0, 0, 1, 1, 0},
            {1, 4, 3, 1, 0, 1, 2, 2, 1},
            {1, 4, 4, 2, 1, 2, 2, 2, 1},
            {1, 3, 2, 2, 2, 2, 2, 2, 1},
            {1, 2, 2, 2, 2, 2, 2, 2, 1},
            {0, 1, 2, 2, 2, 2, 2, 1, 0},
            {0, 0, 1, 2, 2, 2, 1, 0, 0},
            {0, 0, 0, 1, 2, 1, 0, 0, 0},
            {0, 0, 0, 0, 1, 0, 0, 0, 0}
        };

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                int px = 3 + c;
                int py = 3 + r;
                uint8_t m = fullMask[r][c];
                if (m == 1) {
                    setTilePixel(tile, px, py, 20, 20, 20, 255); // Dark border
                } else if (m == 2) {
                    setTilePixel(tile, px, py, 230, 25, 25, 255); // Vibrant red
                } else if (m == 3) {
                    setTilePixel(tile, px, py, 255, 80, 80, 255); // Light red
                } else if (m == 4) {
                    setTilePixel(tile, px, py, 255, 255, 255, 255); // White shine
                }
            }
        }
    }

    // Tile 242: Heart Half (Left full red, right empty container)
    {
        int tile = TILE_HEART_HALF; // 242
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        const uint8_t halfMask[9][9] = {
            {0, 1, 1, 0, 0, 0, 1, 1, 0},
            {1, 4, 3, 1, 0, 1, 5, 5, 1},
            {1, 4, 4, 2, 1, 5, 5, 5, 1},
            {1, 3, 2, 2, 1, 5, 5, 5, 1},
            {1, 2, 2, 2, 1, 5, 5, 5, 1},
            {0, 1, 2, 2, 1, 5, 5, 1, 0},
            {0, 0, 1, 2, 1, 5, 1, 0, 0},
            {0, 0, 0, 1, 1, 1, 0, 0, 0},
            {0, 0, 0, 0, 1, 0, 0, 0, 0}
        };

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                int px = 3 + c;
                int py = 3 + r;
                uint8_t m = halfMask[r][c];
                if (m == 1) {
                    setTilePixel(tile, px, py, 20, 20, 20, 255);
                } else if (m == 2) {
                    setTilePixel(tile, px, py, 230, 25, 25, 255);
                } else if (m == 3) {
                    setTilePixel(tile, px, py, 255, 80, 80, 255);
                } else if (m == 4) {
                    setTilePixel(tile, px, py, 255, 255, 255, 255);
                } else if (m == 5) {
                    setTilePixel(tile, px, py, 45, 12, 12, 255);
                }
            }
        }
    }

    // Tile 243: Hunger Full (Roasted drumstick with bone handle)
    {
        int tile = TILE_HUNGER_FULL; // 243
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        const uint8_t hungerMask[9][9] = {
            {0, 0, 0, 1, 1, 1, 0, 0, 0},
            {0, 0, 1, 3, 4, 3, 1, 0, 0},
            {0, 1, 3, 4, 4, 3, 3, 1, 0},
            {0, 1, 3, 3, 3, 3, 3, 1, 0},
            {1, 2, 3, 3, 3, 3, 2, 1, 0},
            {1, 5, 2, 3, 3, 2, 1, 0, 0},
            {0, 1, 5, 2, 2, 1, 0, 0, 0},
            {1, 5, 1, 5, 1, 0, 0, 0, 0},
            {0, 1, 0, 1, 0, 0, 0, 0, 0}
        };

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                int px = 3 + c;
                int py = 3 + r;
                uint8_t m = hungerMask[r][c];
                if (m == 1) {
                    setTilePixel(tile, px, py, 40, 22, 10, 255); // Dark roasted border
                } else if (m == 2) {
                    setTilePixel(tile, px, py, 140, 65, 20, 255); // Dark meat
                } else if (m == 3) {
                    setTilePixel(tile, px, py, 195, 95, 30, 255); // Golden roasted meat
                } else if (m == 4) {
                    setTilePixel(tile, px, py, 235, 145, 55, 255); // Crispy highlight
                } else if (m == 5) {
                    setTilePixel(tile, px, py, 240, 235, 215, 255); // White bone
                }
            }
        }
    }

    // Tile 244: Hunger Half (Half drumstick)
    {
        int tile = TILE_HUNGER_HALF; // 244
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        const uint8_t halfHungerMask[9][9] = {
            {0, 0, 0, 1, 1, 0, 0, 0, 0},
            {0, 0, 1, 3, 1, 0, 0, 0, 0},
            {0, 1, 3, 4, 1, 0, 0, 0, 0},
            {0, 1, 3, 3, 1, 0, 0, 0, 0},
            {1, 2, 3, 3, 1, 0, 0, 0, 0},
            {1, 5, 2, 3, 1, 0, 0, 0, 0},
            {0, 1, 5, 2, 1, 0, 0, 0, 0},
            {1, 5, 1, 5, 1, 0, 0, 0, 0},
            {0, 1, 0, 1, 0, 0, 0, 0, 0}
        };

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                int px = 3 + c;
                int py = 3 + r;
                uint8_t m = halfHungerMask[r][c];
                if (m == 1) {
                    setTilePixel(tile, px, py, 40, 22, 10, 255);
                } else if (m == 2) {
                    setTilePixel(tile, px, py, 140, 65, 20, 255);
                } else if (m == 3) {
                    setTilePixel(tile, px, py, 195, 95, 30, 255);
                } else if (m == 4) {
                    setTilePixel(tile, px, py, 235, 145, 55, 255);
                } else if (m == 5) {
                    setTilePixel(tile, px, py, 240, 235, 215, 255);
                }
            }
        }
    }

    // Tile 245: Hunger Empty (Drumstick container outline)
    {
        int tile = TILE_HUNGER_EMPTY; // 245
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        const uint8_t emptyHungerMask[9][9] = {
            {0, 0, 0, 1, 1, 1, 0, 0, 0},
            {0, 0, 1, 2, 2, 2, 1, 0, 0},
            {0, 1, 2, 2, 2, 2, 2, 1, 0},
            {0, 1, 2, 2, 2, 2, 2, 1, 0},
            {1, 2, 2, 2, 2, 2, 2, 1, 0},
            {1, 2, 2, 2, 2, 2, 1, 0, 0},
            {0, 1, 2, 2, 2, 1, 0, 0, 0},
            {1, 2, 1, 2, 1, 0, 0, 0, 0},
            {0, 1, 0, 1, 0, 0, 0, 0, 0}
        };

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                int px = 3 + c;
                int py = 3 + r;
                uint8_t m = emptyHungerMask[r][c];
                if (m == 1) {
                    setTilePixel(tile, px, py, 35, 20, 10, 255);
                } else if (m == 2) {
                    setTilePixel(tile, px, py, 45, 28, 16, 200);
                }
            }
        }
    }

    // Tile 246: Oxygen Bubble (Cyan sphere with white sparkle)
    {
        int tile = TILE_BUBBLE; // 246
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        const uint8_t bubbleMask[9][9] = {
            {0, 0, 1, 1, 1, 1, 1, 0, 0},
            {0, 1, 4, 4, 3, 2, 2, 1, 0},
            {1, 4, 4, 3, 3, 2, 2, 2, 1},
            {1, 4, 3, 3, 2, 2, 2, 2, 1},
            {1, 3, 3, 2, 2, 2, 2, 2, 1},
            {1, 2, 2, 2, 2, 2, 2, 2, 1},
            {1, 2, 2, 2, 2, 2, 2, 2, 1},
            {0, 1, 2, 2, 2, 2, 2, 1, 0},
            {0, 0, 1, 1, 1, 1, 1, 0, 0}
        };

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                int px = 3 + c;
                int py = 3 + r;
                uint8_t m = bubbleMask[r][c];
                if (m == 1) {
                    setTilePixel(tile, px, py, 15, 60, 120, 255); // Deep blue border
                } else if (m == 2) {
                    setTilePixel(tile, px, py, 40, 130, 210, 220); // Cyan base
                } else if (m == 3) {
                    setTilePixel(tile, px, py, 110, 205, 255, 235); // Light cyan
                } else if (m == 4) {
                    setTilePixel(tile, px, py, 255, 255, 255, 255); // Glint
                }
            }
        }
    }

    // Tile 247: Oxygen Bubble Pop
    {
        int tile = TILE_BUBBLE_POP; // 247
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);
        setTilePixel(tile, 5, 4, 180, 230, 255, 240);
        setTilePixel(tile, 9, 4, 180, 230, 255, 240);
        setTilePixel(tile, 4, 8, 180, 230, 255, 240);
        setTilePixel(tile, 10, 8, 180, 230, 255, 240);
        setTilePixel(tile, 7, 10, 180, 230, 255, 240);
    }

    // Tile 248: Armor Full (Silver Chestplate)
    {
        int tile = TILE_ARMOR_FULL; // 248
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        const uint8_t armorMask[9][9] = {
            {1, 1, 0, 0, 0, 0, 0, 1, 1},
            {1, 3, 1, 0, 0, 0, 1, 3, 1},
            {1, 3, 1, 1, 1, 1, 1, 3, 1},
            {1, 3, 3, 3, 3, 3, 3, 3, 1},
            {1, 3, 4, 3, 3, 3, 2, 3, 1},
            {0, 1, 3, 3, 3, 3, 3, 1, 0},
            {0, 1, 3, 3, 3, 3, 3, 1, 0},
            {0, 1, 2, 2, 2, 2, 2, 1, 0},
            {0, 0, 1, 1, 1, 1, 1, 0, 0}
        };

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                int px = 3 + c;
                int py = 3 + r;
                uint8_t m = armorMask[r][c];
                if (m == 1) {
                    setTilePixel(tile, px, py, 45, 48, 55, 255);
                } else if (m == 2) {
                    setTilePixel(tile, px, py, 140, 145, 155, 255);
                } else if (m == 3) {
                    setTilePixel(tile, px, py, 195, 200, 210, 255);
                } else if (m == 4) {
                    setTilePixel(tile, px, py, 245, 248, 255, 255);
                }
            }
        }
    }

    // Tile 249: Armor Half
    {
        int tile = TILE_ARMOR_HALF; // 249
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);

        const uint8_t halfArmorMask[9][9] = {
            {1, 1, 0, 0, 0, 0, 0, 0, 0},
            {1, 3, 1, 0, 0, 0, 0, 0, 0},
            {1, 3, 1, 1, 1, 0, 0, 0, 0},
            {1, 3, 3, 3, 1, 0, 0, 0, 0},
            {1, 3, 4, 3, 1, 0, 0, 0, 0},
            {0, 1, 3, 3, 1, 0, 0, 0, 0},
            {0, 1, 3, 3, 1, 0, 0, 0, 0},
            {0, 1, 2, 2, 1, 0, 0, 0, 0},
            {0, 0, 1, 1, 1, 0, 0, 0, 0}
        };

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                int px = 3 + c;
                int py = 3 + r;
                uint8_t m = halfArmorMask[r][c];
                if (m == 1) {
                    setTilePixel(tile, px, py, 45, 48, 55, 255);
                } else if (m == 2) {
                    setTilePixel(tile, px, py, 140, 145, 155, 255);
                } else if (m == 3) {
                    setTilePixel(tile, px, py, 195, 200, 210, 255);
                } else if (m == 4) {
                    setTilePixel(tile, px, py, 245, 248, 255, 255);
                }
            }
        }
    }

    // Steve skin loading (assets/textures/steve.png) -> Tiles 288..295
    bool steveLoaded = false;
    const char* stevePaths[] = {
        "assets/textures/steve.png",
        "x64/Debug/assets/textures/steve.png",
        "x64/Release/assets/textures/steve.png",
        "../assets/textures/steve.png",
        "../../assets/textures/steve.png",
        "d:/Mahesh/Coding files/PrismCraft/PrismCraft/assets/textures/steve.png"
    };
    int sW = 0, sH = 0, sCh = 0;
    for (const char* sp : stevePaths) {
        stbi_uc* sData = stbi_load(sp, &sW, &sH, &sCh, 4);
        if (sData) {
            auto sampleSkin = [&](int sx, int sy, int sw, int sh, int targetTile) {
                for (int ty = 0; ty < 16; ++ty) {
                    for (int tx = 0; tx < 16; ++tx) {
                        int srcX = sx + (tx * sw) / 16;
                        int srcY = sy + (ty * sh) / 16;
                        int srcIdx = (srcY * sW + srcX) * 4;
                        setTilePixel(targetTile, tx, ty, sData[srcIdx], sData[srcIdx+1], sData[srcIdx+2], sData[srcIdx+3]);
                    }
                }
            };
            sampleSkin(8, 8, 8, 8, TILE_STEVE_HEAD_FRONT);
            sampleSkin(0, 8, 8, 8, TILE_STEVE_HEAD_SIDE);
            sampleSkin(8, 0, 8, 8, TILE_STEVE_HEAD_TOP);
            sampleSkin(16, 0, 8, 8, TILE_STEVE_SKIN_TONE);
            sampleSkin(20, 20, 8, 12, TILE_STEVE_TORSO_FRONT);
            sampleSkin(32, 20, 8, 12, TILE_STEVE_TORSO_BACK);
            sampleSkin(44, 20, 4, 12, TILE_STEVE_ARM);
            sampleSkin(4, 20, 4, 12, TILE_STEVE_LEG);
            stbi_image_free(sData);
            steveLoaded = true;
            std::cout << "[TextureAtlas] Loaded steve.png from " << sp << std::endl;
            break;
        }
    }

    if (!steveLoaded) {
        // Fallback procedural Steve
        for (int ty = 0; ty < 16; ++ty) {
            for (int tx = 0; tx < 16; ++tx) {
                setTilePixel(TILE_STEVE_HEAD_FRONT, tx, ty, 195, 140, 100, 255);
                setTilePixel(TILE_STEVE_HEAD_SIDE, tx, ty, 70, 45, 25, 255);
                setTilePixel(TILE_STEVE_HEAD_TOP, tx, ty, 70, 45, 25, 255);
                setTilePixel(TILE_STEVE_SKIN_TONE, tx, ty, 195, 140, 100, 255);
                setTilePixel(TILE_STEVE_TORSO_FRONT, tx, ty, 0, 160, 175, 255);
                setTilePixel(TILE_STEVE_TORSO_BACK, tx, ty, 0, 150, 165, 255);
                setTilePixel(TILE_STEVE_ARM, tx, ty, (ty < 5 ? 0 : 195), (ty < 5 ? 160 : 140), (ty < 5 ? 175 : 100), 255);
                setTilePixel(TILE_STEVE_LEG, tx, ty, (ty < 13 ? 40 : 80), (ty < 13 ? 50 : 80), (ty < 13 ? 120 : 85), 255);
            }
        }
    }

    // Pixel-Art Minecraft Font Glyphs for ASCII 32 to 126 (Rows 20..25: tiles 320..414)
    for (int c = 32; c <= 126; ++c) {
        int charIdx = c - 32;
        int tile = 320 + charIdx;
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) setTilePixel(tile, x, y, 0, 0, 0, 0);
        for (int row = 0; row < 8; ++row) {
            uint8_t rowBits = FONT_8X8[charIdx][row];
            for (int col = 0; col < 8; ++col) {
                bool on = (rowBits & (1 << (7 - col))) != 0;
                uint8_t rgb = on ? 255 : 0;
                uint8_t a = on ? 255 : 0;
                setTilePixel(tile, col * 2,     row * 2,     rgb, rgb, rgb, a);
                setTilePixel(tile, col * 2 + 1, row * 2,     rgb, rgb, rgb, a);
                setTilePixel(tile, col * 2,     row * 2 + 1, rgb, rgb, rgb, a);
                setTilePixel(tile, col * 2 + 1, row * 2 + 1, rgb, rgb, rgb, a);
            }
        }
    }

    // Tile 510: Translucent Dark Tint (For authentic transparent Pause Menu & HUD backdrops)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            setTilePixel(510, x, y, 15, 15, 20, 150);
        }
    }

    // Tile 509: Translucent Blood-Red Tint (For iconic Minecraft Death Screen overlay)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            setTilePixel(509, x, y, 160, 20, 20, 155);
        }
    }

    // Tile 511: Solid Pure White (For Clouds, Sun/Moon, UI masks, and untextured shapes)
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            setTilePixel(511, x, y, 255, 255, 255, 255);
        }
    }

    // Tile 302: Soft circular ground contact shadow disc
    {
        int legacyTile = (TILE_SHADOW_DISC >= 1024) ? ((TILE_SHADOW_DISC / 64) * 16 + (TILE_SHADOW_DISC % 64)) : TILE_SHADOW_DISC;
        int col = legacyTile % LEGACY_TILES_PER_ROW;
        int row = legacyTile / LEGACY_TILES_PER_ROW;
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                float dx = (x - 7.5f) / 7.0f;
                float dy = (y - 7.5f) / 7.0f;
                float distSq = dx * dx + dy * dy;
                int px = col * 16 + x;
                int py = row * 16 + y;
                if (px >= 0 && px < LEGACY_ATLAS_WIDTH && py >= 0 && py < LEGACY_ATLAS_HEIGHT) {
                    int idx = (py * LEGACY_ATLAS_WIDTH + px) * 4;
                    if (distSq < 1.0f) {
                        float alpha = std::clamp((1.0f - std::sqrt(distSq)) * 220.0f, 0.0f, 200.0f);
                        pixels[idx + 0] = 0;
                        pixels[idx + 1] = 0;
                        pixels[idx + 2] = 0;
                        pixels[idx + 3] = static_cast<uint8_t>(alpha);
                    } else {
                        pixels[idx + 0] = 0;
                        pixels[idx + 1] = 0;
                        pixels[idx + 2] = 0;
                        pixels[idx + 3] = 0;
                    }
                }
            }
        }
    }

    return pixels;
}

bool TextureAtlas::getTilePixel(int tileIndex, int px, int py, glm::vec4& rgba) {
    if (s_cachedAtlasPixels.empty()) {
        s_cachedAtlasPixels = generateAtlasPixels();
    }
    if (px < 0 || px >= TILE_SIZE || py < 0 || py >= TILE_SIZE || tileIndex < 0 || tileIndex >= TOTAL_TILES) {
        rgba = glm::vec4(0.0f);
        return false;
    }
    int col = tileIndex % TILES_PER_ROW;
    int row = tileIndex / TILES_PER_ROW;
    int atlasX = col * TILE_SIZE + px;
    int atlasY = row * TILE_SIZE + py;
    if (atlasX < 0 || atlasX >= ATLAS_WIDTH || atlasY < 0 || atlasY >= ATLAS_HEIGHT) {
        rgba = glm::vec4(0.0f);
        return false;
    }
    size_t idx = (static_cast<size_t>(atlasY) * ATLAS_WIDTH + atlasX) * 4;

    uint8_t r = s_cachedAtlasPixels[idx + 0];
    uint8_t g = s_cachedAtlasPixels[idx + 1];
    uint8_t b = s_cachedAtlasPixels[idx + 2];
    uint8_t a = s_cachedAtlasPixels[idx + 3];

    rgba = glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    return a > 20;
}

} // namespace prismcraft

