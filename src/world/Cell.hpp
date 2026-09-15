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
    // Decorative & Functional
    Lantern, SmoothStone,
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
    ItemStick, ItemCoal, ItemIronIngot, ItemGoldIngot, ItemDiamond, ItemEmerald, ItemQuartz, ItemRedstoneDust, ItemString, ItemFlint, ItemApple, ItemBread,
    // Pickaxes
    ItemWoodenPickaxe, ItemStonePickaxe, ItemIronPickaxe, ItemGoldenPickaxe, ItemDiamondPickaxe,
    // Axes
    ItemWoodenAxe, ItemStoneAxe, ItemIronAxe, ItemGoldenAxe, ItemDiamondAxe,
    // Shovels
    ItemWoodenShovel, ItemStoneShovel, ItemIronShovel, ItemGoldenShovel, ItemDiamondShovel,
    // Swords
    ItemWoodenSword, ItemStoneSword, ItemIronSword, ItemGoldenSword, ItemDiamondSword,
    // Hoes
    ItemWoodenHoe, ItemStoneHoe, ItemIronHoe, ItemGoldenHoe, ItemDiamondHoe,
    // Ranged
    ItemBow, ItemArrow,

    // Wood variants (Logs)
    LogSpruce = 99,
    LogBirch,
    LogJungle,
    LogDarkOak,
    LogAcacia,

    // Planks variants
    PlanksDarkOak,
    PlanksAcacia,

    // Leaves variants
    LeavesSpruce,
    LeavesBirch,
    LeavesJungle,
    LeavesDarkOak,
    LeavesAcacia,

    // Saplings
    SaplingOak,
    SaplingSpruce,
    SaplingBirch,
    SaplingJungle,
    SaplingDarkOak,
    SaplingAcacia,

    // Trapdoors
    TrapdoorOak,
    TrapdoorSpruce,
    TrapdoorBirch,
    TrapdoorJungle,
    TrapdoorDarkOak,
    TrapdoorAcacia,
    TrapdoorIron,

    // Doors
    DoorSpruce,
    DoorBirch,
    DoorJungle,
    DoorDarkOak,
    DoorAcacia,

    // Stone & Masonry
    Andesite,
    Diorite,
    Granite,
    PolishedAndesite,
    PolishedDiorite,
    PolishedGranite,
    CoarseDirt,
    Terracotta,
    SmoothSandstone,
    Tuff,
    Calcite,
    Deepslate,
    CobbledDeepslate,
    DeepslateBricks,
    DeepslateTiles,
    MudBricks,

    // 16 Colored Wools
    WoolOrange,
    WoolMagenta,
    WoolLightBlue,
    WoolYellow,
    WoolLime,
    WoolPink,
    WoolGray,
    WoolLightGray,
    WoolCyan,
    WoolPurple,
    WoolBlue,
    WoolBrown,
    WoolGreen,
    WoolRed,
    WoolBlack,

    // 16 Colored Concretes
    ConcreteWhite,
    ConcreteOrange,
    ConcreteMagenta,
    ConcreteLightBlue,
    ConcreteYellow,
    ConcreteLime,
    ConcretePink,
    ConcreteGray,
    ConcreteLightGray,
    ConcreteCyan,
    ConcretePurple,
    ConcreteBlue,
    ConcreteBrown,
    ConcreteGreen,
    ConcreteRed,
    ConcreteBlack,

    // Ores & Raw Blocks
    OreCopper,
    BlockCopper,
    OreLapis,
    BlockLapis,
    DeepslateCoal,
    DeepslateIron,
    DeepslateGold,
    DeepslateDiamond,
    DeepslateRedstone,
    DeepslateEmerald,
    DeepslateCopper,

    // Torches & Lanterns
    TorchSoul,
    TorchRedstone,
    LanternSoul,

    // Utility
    Barrel,
    Smoker,
    BlastFurnace,
    TNT = 193,

    COUNT
};

struct Cell {
    BlockType type = BlockType::Air;
    uint8_t level = 0; // Water: flow distance 0..7; Torch: 0=floor, 1=Base wall, 2=Left wall, 3=Right wall

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
        return type == BlockType::Torch || type == BlockType::TorchSoul || type == BlockType::TorchRedstone;
    }

    [[nodiscard]] bool isFloorTorch() const {
        return isTorch() && level == 0;
    }

    [[nodiscard]] bool isWallTorch() const {
        return isTorch() && level > 0;
    }

    [[nodiscard]] uint8_t getTorchAttachment() const {
        return level;
    }

    [[nodiscard]] bool isLantern() const {
        return type == BlockType::Lantern || type == BlockType::LanternSoul;
    }

    [[nodiscard]] bool isDoor() const {
        return type == BlockType::DoorWood || type == BlockType::DoorIron ||
               (type >= BlockType::DoorSpruce && type <= BlockType::DoorAcacia);
    }

    [[nodiscard]] bool isTrapdoor() const {
        return type >= BlockType::TrapdoorOak && type <= BlockType::TrapdoorIron;
    }

    [[nodiscard]] bool isTrapdoorOpen() const {
        return isTrapdoor() && ((level & 8) != 0);
    }

    [[nodiscard]] uint8_t getTrapdoorFacing() const {
        return level & 3;
    }

    void setTrapdoorFacing(uint8_t f) {
        level = (level & ~3) | (f & 3);
    }

    [[nodiscard]] bool isDoorOpen() const {
        return isDoor() && ((level & 8) != 0);
    }

    [[nodiscard]] bool isDoorUpper() const {
        return isDoor() && ((level & 4) != 0);
    }

    [[nodiscard]] uint8_t getDoorFacing() const {
        return level & 3;
    }

    [[nodiscard]] bool isBed() const {
        return type == BlockType::Bed;
    }

    [[nodiscard]] bool isBedHead() const {
        return isBed() && ((level & 1) != 0);
    }

    [[nodiscard]] bool isBedFoot() const {
        return isBed() && ((level & 1) == 0);
    }

    [[nodiscard]] uint8_t getBedFacing() const {
        return (level >> 2) & 3;
    }

    [[nodiscard]] bool isCactus() const {
        return type == BlockType::Cactus;
    }

    [[nodiscard]] bool isSapling() const {
        return type >= BlockType::SaplingOak && type <= BlockType::SaplingAcacia;
    }

    [[nodiscard]] bool isLeaves() const {
        return type == BlockType::Leaves || (type >= BlockType::LeavesSpruce && type <= BlockType::LeavesAcacia);
    }

    [[nodiscard]] bool isFoliage() const {
        return type == BlockType::TallGrass || type == BlockType::FlowerRose || type == BlockType::FlowerDandelion ||
               type == BlockType::SugarCane || isSapling();
    }

    [[nodiscard]] bool isItem() const {
        return type >= BlockType::ItemStick && type <= BlockType::ItemArrow;
    }

    [[nodiscard]] bool isFlatItem() const {
        return isItem() || isDoor() || isTrapdoor() || isLantern() || isTorch() || isFoliage() || isBed() || type == BlockType::Cake;
    }

    [[nodiscard]] bool isPickaxe() const {
        return type == BlockType::ItemWoodenPickaxe || type == BlockType::ItemStonePickaxe ||
               type == BlockType::ItemIronPickaxe || type == BlockType::ItemGoldenPickaxe ||
               type == BlockType::ItemDiamondPickaxe;
    }

    [[nodiscard]] bool isAxe() const {
        return type == BlockType::ItemWoodenAxe || type == BlockType::ItemStoneAxe ||
               type == BlockType::ItemIronAxe || type == BlockType::ItemGoldenAxe ||
               type == BlockType::ItemDiamondAxe;
    }

    [[nodiscard]] bool isShovel() const {
        return type == BlockType::ItemWoodenShovel || type == BlockType::ItemStoneShovel ||
               type == BlockType::ItemIronShovel || type == BlockType::ItemGoldenShovel ||
               type == BlockType::ItemDiamondShovel;
    }

    [[nodiscard]] bool isSword() const {
        return type == BlockType::ItemWoodenSword || type == BlockType::ItemStoneSword ||
               type == BlockType::ItemIronSword || type == BlockType::ItemGoldenSword ||
               type == BlockType::ItemDiamondSword;
    }

    [[nodiscard]] bool isHoe() const {
        return type == BlockType::ItemWoodenHoe || type == BlockType::ItemStoneHoe ||
               type == BlockType::ItemIronHoe || type == BlockType::ItemGoldenHoe ||
               type == BlockType::ItemDiamondHoe;
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
        // 0: none, 1: wood, 2: stone, 3: iron, 4: gold, 5: diamond
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
            case BlockType::ItemGoldenPickaxe:
            case BlockType::ItemGoldenAxe:
            case BlockType::ItemGoldenShovel:
            case BlockType::ItemGoldenSword:
            case BlockType::ItemGoldenHoe: return 4;
            case BlockType::ItemDiamondPickaxe:
            case BlockType::ItemDiamondAxe:
            case BlockType::ItemDiamondShovel:
            case BlockType::ItemDiamondSword:
            case BlockType::ItemDiamondHoe: return 5;
            default: return 0;
        }
    }

    [[nodiscard]] bool isLightEmitter() const {
        return isTorch() || isLantern() || type == BlockType::Glowstone || type == BlockType::JackOLantern;
    }

    [[nodiscard]] bool isOpaque() const {
        return type != BlockType::Air && type != BlockType::Water && type != BlockType::Glass &&
               !isLeaves() && !isFoliage() && !isTorch() && !isLantern() && !isItem() &&
               type != BlockType::Ice && type != BlockType::Cake && type != BlockType::Bed &&
               !isDoor() && !isTrapdoor() && !isCactus();
    }

    [[nodiscard]] bool isSolid() const {
        if (isDoor()) return !isDoorOpen();
        if (isTrapdoor()) return !isTrapdoorOpen();
        return type != BlockType::Air && type != BlockType::Water && !isFoliage() && !isTorch() && !isLantern() && !isItem();
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
        case BlockType::Lantern: return "Lantern";
        case BlockType::SmoothStone: return "Smooth Stone";
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
        case BlockType::ItemGoldIngot: return "Gold Ingot";
        case BlockType::ItemDiamond: return "Diamond";
        case BlockType::ItemEmerald: return "Emerald";
        case BlockType::ItemQuartz: return "Nether Quartz";
        case BlockType::ItemRedstoneDust: return "Redstone Dust";
        case BlockType::ItemString: return "String";
        case BlockType::ItemFlint: return "Flint";
        case BlockType::ItemApple: return "Apple";
        case BlockType::ItemBread: return "Bread";
        case BlockType::ItemWoodenPickaxe: return "Wooden Pickaxe";
        case BlockType::ItemStonePickaxe: return "Stone Pickaxe";
        case BlockType::ItemIronPickaxe: return "Iron Pickaxe";
        case BlockType::ItemGoldenPickaxe: return "Golden Pickaxe";
        case BlockType::ItemDiamondPickaxe: return "Diamond Pickaxe";
        case BlockType::ItemWoodenAxe: return "Wooden Axe";
        case BlockType::ItemStoneAxe: return "Stone Axe";
        case BlockType::ItemIronAxe: return "Iron Axe";
        case BlockType::ItemGoldenAxe: return "Golden Axe";
        case BlockType::ItemDiamondAxe: return "Diamond Axe";
        case BlockType::ItemWoodenShovel: return "Wooden Shovel";
        case BlockType::ItemStoneShovel: return "Stone Shovel";
        case BlockType::ItemIronShovel: return "Iron Shovel";
        case BlockType::ItemGoldenShovel: return "Golden Shovel";
        case BlockType::ItemDiamondShovel: return "Diamond Shovel";
        case BlockType::ItemWoodenSword: return "Wooden Sword";
        case BlockType::ItemStoneSword: return "Stone Sword";
        case BlockType::ItemIronSword: return "Iron Sword";
        case BlockType::ItemGoldenSword: return "Golden Sword";
        case BlockType::ItemDiamondSword: return "Diamond Sword";
        case BlockType::ItemWoodenHoe: return "Wooden Hoe";
        case BlockType::ItemStoneHoe: return "Stone Hoe";
        case BlockType::ItemIronHoe: return "Iron Hoe";
        case BlockType::ItemGoldenHoe: return "Golden Hoe";
        case BlockType::ItemDiamondHoe: return "Diamond Hoe";
        case BlockType::ItemBow: return "Bow";
        case BlockType::ItemArrow: return "Arrow";

        // New Wood Logs
        case BlockType::LogSpruce: return "Spruce Log";
        case BlockType::LogBirch: return "Birch Log";
        case BlockType::LogJungle: return "Jungle Log";
        case BlockType::LogDarkOak: return "Dark Oak Log";
        case BlockType::LogAcacia: return "Acacia Log";

        // Planks Variants
        case BlockType::PlanksDarkOak: return "Dark Oak Planks";
        case BlockType::PlanksAcacia: return "Acacia Planks";

        // Leaves Variants
        case BlockType::LeavesSpruce: return "Spruce Leaves";
        case BlockType::LeavesBirch: return "Birch Leaves";
        case BlockType::LeavesJungle: return "Jungle Leaves";
        case BlockType::LeavesDarkOak: return "Dark Oak Leaves";
        case BlockType::LeavesAcacia: return "Acacia Leaves";

        // Saplings
        case BlockType::SaplingOak: return "Oak Sapling";
        case BlockType::SaplingSpruce: return "Spruce Sapling";
        case BlockType::SaplingBirch: return "Birch Sapling";
        case BlockType::SaplingJungle: return "Jungle Sapling";
        case BlockType::SaplingDarkOak: return "Dark Oak Sapling";
        case BlockType::SaplingAcacia: return "Acacia Sapling";

        // Trapdoors
        case BlockType::TrapdoorOak: return "Oak Trapdoor";
        case BlockType::TrapdoorSpruce: return "Spruce Trapdoor";
        case BlockType::TrapdoorBirch: return "Birch Trapdoor";
        case BlockType::TrapdoorJungle: return "Jungle Trapdoor";
        case BlockType::TrapdoorDarkOak: return "Dark Oak Trapdoor";
        case BlockType::TrapdoorAcacia: return "Acacia Trapdoor";
        case BlockType::TrapdoorIron: return "Iron Trapdoor";

        // Doors
        case BlockType::DoorSpruce: return "Spruce Door";
        case BlockType::DoorBirch: return "Birch Door";
        case BlockType::DoorJungle: return "Jungle Door";
        case BlockType::DoorDarkOak: return "Dark Oak Door";
        case BlockType::DoorAcacia: return "Acacia Door";

        // Stone & Masonry
        case BlockType::Andesite: return "Andesite";
        case BlockType::Diorite: return "Diorite";
        case BlockType::Granite: return "Granite";
        case BlockType::PolishedAndesite: return "Polished Andesite";
        case BlockType::PolishedDiorite: return "Polished Diorite";
        case BlockType::PolishedGranite: return "Polished Granite";
        case BlockType::CoarseDirt: return "Coarse Dirt";
        case BlockType::Terracotta: return "Terracotta";
        case BlockType::SmoothSandstone: return "Smooth Sandstone";
        case BlockType::Tuff: return "Tuff";
        case BlockType::Calcite: return "Calcite";
        case BlockType::Deepslate: return "Deepslate";
        case BlockType::CobbledDeepslate: return "Cobbled Deepslate";
        case BlockType::DeepslateBricks: return "Deepslate Bricks";
        case BlockType::DeepslateTiles: return "Deepslate Tiles";
        case BlockType::MudBricks: return "Mud Bricks";

        // 16 Colored Wools
        case BlockType::WoolOrange: return "Orange Wool";
        case BlockType::WoolMagenta: return "Magenta Wool";
        case BlockType::WoolLightBlue: return "Light Blue Wool";
        case BlockType::WoolYellow: return "Yellow Wool";
        case BlockType::WoolLime: return "Lime Wool";
        case BlockType::WoolPink: return "Pink Wool";
        case BlockType::WoolGray: return "Gray Wool";
        case BlockType::WoolLightGray: return "Light Gray Wool";
        case BlockType::WoolCyan: return "Cyan Wool";
        case BlockType::WoolPurple: return "Purple Wool";
        case BlockType::WoolBlue: return "Blue Wool";
        case BlockType::WoolBrown: return "Brown Wool";
        case BlockType::WoolGreen: return "Green Wool";
        case BlockType::WoolRed: return "Red Wool";
        case BlockType::WoolBlack: return "Black Wool";

        // 16 Colored Concretes
        case BlockType::ConcreteWhite: return "White Concrete";
        case BlockType::ConcreteOrange: return "Orange Concrete";
        case BlockType::ConcreteMagenta: return "Magenta Concrete";
        case BlockType::ConcreteLightBlue: return "Light Blue Concrete";
        case BlockType::ConcreteYellow: return "Yellow Concrete";
        case BlockType::ConcreteLime: return "Lime Concrete";
        case BlockType::ConcretePink: return "Pink Concrete";
        case BlockType::ConcreteGray: return "Gray Concrete";
        case BlockType::ConcreteLightGray: return "Light Gray Concrete";
        case BlockType::ConcreteCyan: return "Cyan Concrete";
        case BlockType::ConcretePurple: return "Purple Concrete";
        case BlockType::ConcreteBlue: return "Blue Concrete";
        case BlockType::ConcreteBrown: return "Brown Concrete";
        case BlockType::ConcreteGreen: return "Green Concrete";
        case BlockType::ConcreteRed: return "Red Concrete";
        case BlockType::ConcreteBlack: return "Black Concrete";

        // Ores & Raw Blocks
        case BlockType::OreCopper: return "Copper Ore";
        case BlockType::BlockCopper: return "Block of Copper";
        case BlockType::OreLapis: return "Lapis Lazuli Ore";
        case BlockType::BlockLapis: return "Block of Lapis Lazuli";
        case BlockType::DeepslateCoal: return "Deepslate Coal Ore";
        case BlockType::DeepslateIron: return "Deepslate Iron Ore";
        case BlockType::DeepslateGold: return "Deepslate Gold Ore";
        case BlockType::DeepslateDiamond: return "Deepslate Diamond Ore";
        case BlockType::DeepslateRedstone: return "Deepslate Redstone Ore";
        case BlockType::DeepslateEmerald: return "Deepslate Emerald Ore";
        case BlockType::DeepslateCopper: return "Deepslate Copper Ore";

        // Torches & Lanterns
        case BlockType::TorchSoul: return "Soul Torch";
        case BlockType::TorchRedstone: return "Redstone Torch";
        case BlockType::LanternSoul: return "Soul Lantern";

        // Utility
        case BlockType::Barrel: return "Barrel";
        case BlockType::Smoker: return "Smoker";
        case BlockType::BlastFurnace: return "Blast Furnace";
        case BlockType::TNT: return "TNT";

        default: return "";
    }
}

} // namespace prismcraft
