#pragma once
#include <cstdint>
#include <string_view>

namespace prismcraft {

enum class BlockType : uint8_t {
    Air = 0,
    Grass, Dirt, Stone, Sand, Wood, Leaves, Water, Bedrock,
    Planks, CobbleStone, Glass, CraftingTable, Furnace,
    OreCoal, OreIron, OreGold, OreDiamond, Gravel, Snow,
    TallGrass, FlowerRose, FlowerDandelion,
    Torch,
    // Additional Ores
    OreRedstone, OreEmerald, OreNetherQuartz,
    // Solid Mineral Blocks
    BlockIron, BlockGold, BlockDiamond, BlockEmerald, BlockRedstone,
    // Building & Masonry
    Bookshelf, MossyCobble, Obsidian, Sponge, StoneBricks, Brick,
    // Nature & Plants
    Cactus, Clay, SugarCane, Pumpkin, JackOLantern, Melon, Cake, Ice,
    // Wood Planks Variants
    PlanksPine, PlanksBirch, PlanksJungle,
    // Nether Blocks
    Netherrack, SoulSand, Glowstone, NetherBrick,
    // Furniture & Decorative
    Bed, DoorWood, DoorIron, Sandstone, WoolWhite,
    // Crafting Materials
    ItemStick, ItemCoal, ItemIronIngot, ItemDiamond, ItemString, ItemFlint,
    // Pickaxes
    ItemWoodenPickaxe, ItemStonePickaxe, ItemIronPickaxe, ItemDiamondPickaxe,
    // Axes
    ItemWoodenAxe, ItemStoneAxe, ItemIronAxe, ItemDiamondAxe,
    // Shovels
    ItemWoodenShovel, ItemStoneShovel, ItemIronShovel, ItemDiamondShovel,
    // Swords
    ItemWoodenSword, ItemStoneSword, ItemIronSword, ItemDiamondSword,
    // Hoes
    ItemWoodenHoe, ItemStoneHoe, ItemIronHoe, ItemDiamondHoe,
    // Ranged
    ItemBow, ItemArrow,
    COUNT
};

struct Cell {
    BlockType type = BlockType::Air;
    uint8_t level = 0; // 0 = source block / standard, 1..7 = flowing water distance

    [[nodiscard]] bool isWater() const {
        return type == BlockType::Water;
    }

    [[nodiscard]] bool isWaterSource() const {
        return type == BlockType::Water && level == 0;
    }

    [[nodiscard]] bool isFlowingWater() const {
        return type == BlockType::Water && level > 0;
    }

    [[nodiscard]] bool isTorch() const {
        return type == BlockType::Torch;
    }

    [[nodiscard]] bool isFoliage() const {
        return type == BlockType::TallGrass || type == BlockType::FlowerRose || type == BlockType::FlowerDandelion ||
               type == BlockType::SugarCane;
    }

    [[nodiscard]] bool isItem() const {
        return type >= BlockType::ItemStick && type < BlockType::COUNT;
    }

    [[nodiscard]] bool isPickaxe() const {
        return type == BlockType::ItemWoodenPickaxe || type == BlockType::ItemStonePickaxe ||
               type == BlockType::ItemIronPickaxe || type == BlockType::ItemDiamondPickaxe;
    }

    [[nodiscard]] bool isAxe() const {
        return type == BlockType::ItemWoodenAxe || type == BlockType::ItemStoneAxe ||
               type == BlockType::ItemIronAxe || type == BlockType::ItemDiamondAxe;
    }

    [[nodiscard]] bool isShovel() const {
        return type == BlockType::ItemWoodenShovel || type == BlockType::ItemStoneShovel ||
               type == BlockType::ItemIronShovel || type == BlockType::ItemDiamondShovel;
    }

    [[nodiscard]] bool isSword() const {
        return type == BlockType::ItemWoodenSword || type == BlockType::ItemStoneSword ||
               type == BlockType::ItemIronSword || type == BlockType::ItemDiamondSword;
    }

    [[nodiscard]] bool isHoe() const {
        return type == BlockType::ItemWoodenHoe || type == BlockType::ItemStoneHoe ||
               type == BlockType::ItemIronHoe || type == BlockType::ItemDiamondHoe;
    }

    [[nodiscard]] bool isBow() const {
        return type == BlockType::ItemBow;
    }

    [[nodiscard]] bool isArrow() const {
        return type == BlockType::ItemArrow;
    }

    [[nodiscard]] bool isTool() const {
        return isPickaxe() || isAxe() || isShovel() || isSword() || isHoe() || isBow();
    }

    [[nodiscard]] int getToolTier() const {
        // 0: none, 1: wood, 2: stone, 3: iron, 4: diamond
        switch (type) {
            case BlockType::ItemWoodenPickaxe:
            case BlockType::ItemWoodenAxe:
            case BlockType::ItemWoodenShovel:
            case BlockType::ItemWoodenSword:
            case BlockType::ItemWoodenHoe: return 1;
            case BlockType::ItemStonePickaxe:
            case BlockType::ItemStoneAxe:
            case BlockType::ItemStoneShovel:
            case BlockType::ItemStoneSword:
            case BlockType::ItemStoneHoe: return 2;
            case BlockType::ItemIronPickaxe:
            case BlockType::ItemIronAxe:
            case BlockType::ItemIronShovel:
            case BlockType::ItemIronSword:
            case BlockType::ItemIronHoe: return 3;
            case BlockType::ItemDiamondPickaxe:
            case BlockType::ItemDiamondAxe:
            case BlockType::ItemDiamondShovel:
            case BlockType::ItemDiamondSword:
            case BlockType::ItemDiamondHoe: return 4;
            default: return 0;
        }
    }

    [[nodiscard]] bool isLightEmitter() const {
        return type == BlockType::Torch || type == BlockType::Glowstone || type == BlockType::JackOLantern;
    }

    [[nodiscard]] bool isOpaque() const {
        return type != BlockType::Air && type != BlockType::Water && type != BlockType::Glass &&
               type != BlockType::Leaves && !isFoliage() && !isTorch() && !isItem() &&
               type != BlockType::Ice && type != BlockType::Cake && type != BlockType::Bed &&
               type != BlockType::DoorWood && type != BlockType::DoorIron;
    }

    [[nodiscard]] bool isSolid() const {
        return type != BlockType::Air && type != BlockType::Water && !isFoliage() && !isTorch() && !isItem();
    }

    [[nodiscard]] bool isTargetable() const {
        return type != BlockType::Air && type != BlockType::Water && !isItem();
    }

    [[nodiscard]] bool isTransparent() const {
        return !isOpaque();
    }
};

inline std::string_view getItemDisplayName(BlockType type) {
    switch (type) {
        case BlockType::Grass: return "Grass Block";
        case BlockType::Dirt: return "Dirt";
        case BlockType::Stone: return "Stone";
        case BlockType::Sand: return "Sand";
        case BlockType::Wood: return "Oak Log";
        case BlockType::Leaves: return "Oak Leaves";
        case BlockType::Water: return "Water";
        case BlockType::Bedrock: return "Bedrock";
        case BlockType::Planks: return "Oak Planks";
        case BlockType::CobbleStone: return "Cobblestone";
        case BlockType::Glass: return "Glass";
        case BlockType::CraftingTable: return "Crafting Table";
        case BlockType::Furnace: return "Furnace";
        case BlockType::OreCoal: return "Coal Ore";
        case BlockType::OreIron: return "Iron Ore";
        case BlockType::OreGold: return "Gold Ore";
        case BlockType::OreDiamond: return "Diamond Ore";
        case BlockType::Gravel: return "Gravel";
        case BlockType::Snow: return "Snow";
        case BlockType::TallGrass: return "Tall Grass";
        case BlockType::FlowerRose: return "Poppy";
        case BlockType::FlowerDandelion: return "Dandelion";
        case BlockType::Torch: return "Torch";
        // Additional Ores
        case BlockType::OreRedstone: return "Redstone Ore";
        case BlockType::OreEmerald: return "Emerald Ore";
        case BlockType::OreNetherQuartz: return "Nether Quartz Ore";
        // Mineral Blocks
        case BlockType::BlockIron: return "Block of Iron";
        case BlockType::BlockGold: return "Block of Gold";
        case BlockType::BlockDiamond: return "Block of Diamond";
        case BlockType::BlockEmerald: return "Block of Emerald";
        case BlockType::BlockRedstone: return "Block of Redstone";
        // Building & Masonry
        case BlockType::Bookshelf: return "Bookshelf";
        case BlockType::MossyCobble: return "Mossy Cobblestone";
        case BlockType::Obsidian: return "Obsidian";
        case BlockType::Sponge: return "Sponge";
        case BlockType::StoneBricks: return "Stone Bricks";
        case BlockType::Brick: return "Brick Block";
        // Nature & Plants
        case BlockType::Cactus: return "Cactus";
        case BlockType::Clay: return "Clay Block";
        case BlockType::SugarCane: return "Sugar Cane";
        case BlockType::Pumpkin: return "Pumpkin";
        case BlockType::JackOLantern: return "Jack o'Lantern";
        case BlockType::Melon: return "Melon";
        case BlockType::Cake: return "Cake";
        case BlockType::Ice: return "Ice";
        // Planks Variants
        case BlockType::PlanksPine: return "Pine Planks";
        case BlockType::PlanksBirch: return "Birch Planks";
        case BlockType::PlanksJungle: return "Jungle Planks";
        // Nether
        case BlockType::Netherrack: return "Netherrack";
        case BlockType::SoulSand: return "Soul Sand";
        case BlockType::Glowstone: return "Glowstone";
        case BlockType::NetherBrick: return "Nether Brick";
        // Furniture & Decorative
        case BlockType::Bed: return "Bed";
        case BlockType::DoorWood: return "Wooden Door";
        case BlockType::DoorIron: return "Iron Door";
        case BlockType::Sandstone: return "Sandstone";
        case BlockType::WoolWhite: return "White Wool";
        case BlockType::ItemStick: return "Stick";
        case BlockType::ItemCoal: return "Coal";
        case BlockType::ItemIronIngot: return "Iron Ingot";
        case BlockType::ItemDiamond: return "Diamond";
        case BlockType::ItemString: return "String";
        case BlockType::ItemFlint: return "Flint";
        case BlockType::ItemWoodenPickaxe: return "Wooden Pickaxe";
        case BlockType::ItemStonePickaxe: return "Stone Pickaxe";
        case BlockType::ItemIronPickaxe: return "Iron Pickaxe";
        case BlockType::ItemDiamondPickaxe: return "Diamond Pickaxe";
        case BlockType::ItemWoodenAxe: return "Wooden Axe";
        case BlockType::ItemStoneAxe: return "Stone Axe";
        case BlockType::ItemIronAxe: return "Iron Axe";
        case BlockType::ItemDiamondAxe: return "Diamond Axe";
        case BlockType::ItemWoodenShovel: return "Wooden Shovel";
        case BlockType::ItemStoneShovel: return "Stone Shovel";
        case BlockType::ItemIronShovel: return "Iron Shovel";
        case BlockType::ItemDiamondShovel: return "Diamond Shovel";
        case BlockType::ItemWoodenSword: return "Wooden Sword";
        case BlockType::ItemStoneSword: return "Stone Sword";
        case BlockType::ItemIronSword: return "Iron Sword";
        case BlockType::ItemDiamondSword: return "Diamond Sword";
        case BlockType::ItemWoodenHoe: return "Wooden Hoe";
        case BlockType::ItemStoneHoe: return "Stone Hoe";
        case BlockType::ItemIronHoe: return "Iron Hoe";
        case BlockType::ItemDiamondHoe: return "Diamond Hoe";
        case BlockType::ItemBow: return "Bow";
        case BlockType::ItemArrow: return "Arrow";
        default: return "";
    }
}

} // namespace prismcraft
