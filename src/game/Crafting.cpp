#include "Crafting.hpp"

namespace prismcraft {

static inline bool isAnyPlanks(BlockType b) {
    return b == BlockType::Planks || b == BlockType::PlanksPine || b == BlockType::PlanksBirch ||
           b == BlockType::PlanksJungle || b == BlockType::PlanksDarkOak || b == BlockType::PlanksAcacia;
}

static inline bool isAnyWool(BlockType b) {
    return (b == BlockType::WoolWhite || (b >= BlockType::WoolOrange && b <= BlockType::WoolBlack) ||
            b == BlockType::Leaves || (b >= BlockType::LeavesSpruce && b <= BlockType::LeavesAcacia));
}

static inline BlockType getPlanksFromLog(BlockType log) {
    switch (log) {
        case BlockType::Wood: return BlockType::Planks;
        case BlockType::LogSpruce: return BlockType::PlanksPine;
        case BlockType::LogBirch: return BlockType::PlanksBirch;
        case BlockType::LogJungle: return BlockType::PlanksJungle;
        case BlockType::LogDarkOak: return BlockType::PlanksDarkOak;
        case BlockType::LogAcacia: return BlockType::PlanksAcacia;
        default: return BlockType::Air;
    }
}

static inline BlockType getTrapdoorFromPlanks(BlockType planks) {
    switch (planks) {
        case BlockType::Planks: return BlockType::TrapdoorOak;
        case BlockType::PlanksPine: return BlockType::TrapdoorSpruce;
        case BlockType::PlanksBirch: return BlockType::TrapdoorBirch;
        case BlockType::PlanksJungle: return BlockType::TrapdoorJungle;
        case BlockType::PlanksDarkOak: return BlockType::TrapdoorDarkOak;
        case BlockType::PlanksAcacia: return BlockType::TrapdoorAcacia;
        default: return BlockType::TrapdoorOak;
    }
}

static inline BlockType getDoorFromPlanks(BlockType planks) {
    switch (planks) {
        case BlockType::Planks: return BlockType::DoorWood;
        case BlockType::PlanksPine: return BlockType::DoorSpruce;
        case BlockType::PlanksBirch: return BlockType::DoorBirch;
        case BlockType::PlanksJungle: return BlockType::DoorJungle;
        case BlockType::PlanksDarkOak: return BlockType::DoorDarkOak;
        case BlockType::PlanksAcacia: return BlockType::DoorAcacia;
        default: return BlockType::DoorWood;
    }
}

CraftingResult CraftingSystem::craft2x2(const std::array<BlockType, 4>& grid) {
    int itemCount = 0;
    BlockType singleItem = BlockType::Air;
    for (int i = 0; i < 4; ++i) {
        if (grid[i] != BlockType::Air) {
            itemCount++;
            singleItem = grid[i];
        }
    }

    // 1. Single Wood Log (any slot) -> 4 Planks
    if (itemCount == 1) {
        BlockType p = getPlanksFromLog(singleItem);
        if (p != BlockType::Air) {
            return {p, 4};
        }
    }

    // 2. 2 Planks vertically -> 4 Sticks
    if (itemCount == 2) {
        if ((isAnyPlanks(grid[0]) && isAnyPlanks(grid[2])) ||
            (isAnyPlanks(grid[1]) && isAnyPlanks(grid[3]))) {
            return {BlockType::ItemStick, 4};
        }
        // 1 Coal / OreCoal over 1 Stick -> 4 Torches
        if (((grid[0] == BlockType::ItemCoal || grid[0] == BlockType::OreCoal) && grid[2] == BlockType::ItemStick) ||
            ((grid[1] == BlockType::ItemCoal || grid[1] == BlockType::OreCoal) && grid[3] == BlockType::ItemStick)) {
            return {BlockType::Torch, 4};
        }
        // 1 Stick over 1 Cobblestone/Flint -> 2 Arrows (quick 2x2 arrow recipe)
        if ((grid[0] == BlockType::CobbleStone || grid[0] == BlockType::ItemFlint) && grid[2] == BlockType::ItemStick) {
            return {BlockType::ItemArrow, 4};
        }
        // 2 Planks horizontally -> 2 Trapdoors
        if (isAnyPlanks(grid[0]) && grid[1] == grid[0]) {
            return {getTrapdoorFromPlanks(grid[0]), 2};
        }
        if (isAnyPlanks(grid[2]) && grid[3] == grid[2]) {
            return {getTrapdoorFromPlanks(grid[2]), 2};
        }
    }

    // 3. 4 Planks (fill all 4 slots) -> Crafting Table
    if (itemCount == 4 && isAnyPlanks(grid[0]) && isAnyPlanks(grid[1]) &&
        isAnyPlanks(grid[2]) && isAnyPlanks(grid[3])) {
        return {BlockType::CraftingTable, 1};
    }

    // 4. 4 Sand -> 4 Glass
    if (itemCount == 4 && grid[0] == BlockType::Sand && grid[1] == BlockType::Sand &&
        grid[2] == BlockType::Sand && grid[3] == BlockType::Sand) {
        return {BlockType::Glass, 4};
    }

    // 5. 2x2 Wooden / Golden Sword
    if (itemCount == 2 && grid[2] == BlockType::ItemStick) {
        if (isAnyPlanks(grid[0])) return {BlockType::ItemWoodenSword, 1};
        if (grid[0] == BlockType::ItemGoldIngot || grid[0] == BlockType::OreGold) return {BlockType::ItemGoldenSword, 1};
        if (grid[0] == BlockType::CobbleStone || grid[0] == BlockType::Stone) return {BlockType::ItemStoneSword, 1};
        if (grid[0] == BlockType::ItemIronIngot || grid[0] == BlockType::OreIron) return {BlockType::ItemIronSword, 1};
        if (grid[0] == BlockType::ItemDiamond || grid[0] == BlockType::OreDiamond) return {BlockType::ItemDiamondSword, 1};
    }

    // 6. 1 Torch + 1 Iron Ingot (any 2 slots) -> 1 Lantern / Soul Lantern
    if (itemCount == 2) {
        bool hasTorch = (grid[0] == BlockType::Torch || grid[1] == BlockType::Torch || grid[2] == BlockType::Torch || grid[3] == BlockType::Torch);
        bool hasSoulTorch = (grid[0] == BlockType::TorchSoul || grid[1] == BlockType::TorchSoul || grid[2] == BlockType::TorchSoul || grid[3] == BlockType::TorchSoul);
        bool hasIron = (grid[0] == BlockType::ItemIronIngot || grid[1] == BlockType::ItemIronIngot || grid[2] == BlockType::ItemIronIngot || grid[3] == BlockType::ItemIronIngot ||
                        grid[0] == BlockType::OreIron || grid[1] == BlockType::OreIron || grid[2] == BlockType::OreIron || grid[3] == BlockType::OreIron);
        if (hasTorch && hasIron) return {BlockType::Lantern, 1};
        if (hasSoulTorch && hasIron) return {BlockType::LanternSoul, 1};
    }

    // 6b. Sand + Coal / Redstone -> 1 TNT
    if (itemCount == 2) {
        bool hasSand = (grid[0] == BlockType::Sand || grid[1] == BlockType::Sand || grid[2] == BlockType::Sand || grid[3] == BlockType::Sand);
        bool hasPowder = (grid[0] == BlockType::ItemCoal || grid[1] == BlockType::ItemCoal || grid[2] == BlockType::ItemCoal || grid[3] == BlockType::ItemCoal ||
                          grid[0] == BlockType::ItemRedstoneDust || grid[1] == BlockType::ItemRedstoneDust || grid[2] == BlockType::ItemRedstoneDust || grid[3] == BlockType::ItemRedstoneDust ||
                          grid[0] == BlockType::OreRedstone || grid[1] == BlockType::OreRedstone || grid[2] == BlockType::OreRedstone || grid[3] == BlockType::OreRedstone);
        if (hasSand && hasPowder) return {BlockType::TNT, 1};
    }

    // 7. 4 Stone 2x2 -> 4 Smooth Stone
    if (itemCount == 4 && grid[0] == BlockType::Stone && grid[1] == BlockType::Stone &&
        grid[2] == BlockType::Stone && grid[3] == BlockType::Stone) {
        return {BlockType::SmoothStone, 4};
    }

    // 8. Mineral block decompression (1 block -> 9 items)
    if (itemCount == 1) {
        if (singleItem == BlockType::BlockIron) return {BlockType::ItemIronIngot, 9};
        if (singleItem == BlockType::BlockGold) return {BlockType::ItemGoldIngot, 9};
        if (singleItem == BlockType::BlockDiamond) return {BlockType::ItemDiamond, 9};
        if (singleItem == BlockType::BlockEmerald) return {BlockType::ItemEmerald, 9};
        if (singleItem == BlockType::BlockRedstone) return {BlockType::ItemRedstoneDust, 9};
        if (singleItem == BlockType::BlockCopper) return {BlockType::OreCopper, 9};
        if (singleItem == BlockType::BlockLapis) return {BlockType::OreLapis, 9};
    }

    // 9. 4 Ingots/Gems 2x2 -> 1 Block
    if (itemCount == 4 && grid[0] == grid[1] && grid[1] == grid[2] && grid[2] == grid[3]) {
        if (grid[0] == BlockType::ItemIronIngot) return {BlockType::BlockIron, 1};
        if (grid[0] == BlockType::ItemGoldIngot) return {BlockType::BlockGold, 1};
        if (grid[0] == BlockType::ItemDiamond) return {BlockType::BlockDiamond, 1};
        if (grid[0] == BlockType::ItemEmerald) return {BlockType::BlockEmerald, 1};
        if (grid[0] == BlockType::ItemRedstoneDust) return {BlockType::BlockRedstone, 1};
        if (grid[0] == BlockType::OreCopper) return {BlockType::BlockCopper, 1};
        if (grid[0] == BlockType::OreLapis) return {BlockType::BlockLapis, 1};
        if (grid[0] == BlockType::CobbledDeepslate) return {BlockType::DeepslateBricks, 4};
        if (grid[0] == BlockType::DeepslateBricks) return {BlockType::DeepslateTiles, 4};
    }

    // 10. 2x2 Bed (2 Wool/Leaves top row, 2 Planks bottom row)
    if (itemCount == 4 &&
        isAnyWool(grid[0]) && isAnyWool(grid[1]) &&
        isAnyPlanks(grid[2]) && isAnyPlanks(grid[3])) {
        return {BlockType::Bed, 1};
    }

    // 11. 2x2 Wooden Door (2 Planks in column 0, 1 Stick in column 1)
    if (itemCount == 3 && isAnyPlanks(grid[0]) && grid[2] == grid[0] && grid[3] == BlockType::ItemStick) {
        return {getDoorFromPlanks(grid[0]), 1};
    }

    return {BlockType::Air, 0};
}

CraftingResult CraftingSystem::craft3x3(const std::array<BlockType, 9>& grid) {
    int itemCount = 0;
    for (int i = 0; i < 9; ++i) {
        if (grid[i] != BlockType::Air) itemCount++;
    }

    // Helper to check standard vertical tool handles (sticks at slot 4 and 7, or slot 3 and 6, or slot 5 and 8)
    auto checkToolHandle = [&](int col) -> bool {
        return grid[col + 3] == BlockType::ItemStick && grid[col + 6] == BlockType::ItemStick;
    };

    // 1. Pickaxes (3 material on top row, 2 sticks below in center)
    if (itemCount == 5 && checkToolHandle(1)) {
        BlockType topMat = grid[0];
        if (topMat != BlockType::Air && grid[1] == topMat && grid[2] == topMat) {
            if (isAnyPlanks(topMat)) return {BlockType::ItemWoodenPickaxe, 1};
            if (topMat == BlockType::CobbleStone || topMat == BlockType::Stone) return {BlockType::ItemStonePickaxe, 1};
            if (topMat == BlockType::ItemIronIngot || topMat == BlockType::OreIron) return {BlockType::ItemIronPickaxe, 1};
            if (topMat == BlockType::ItemGoldIngot || topMat == BlockType::OreGold) return {BlockType::ItemGoldenPickaxe, 1};
            if (topMat == BlockType::ItemDiamond || topMat == BlockType::OreDiamond) return {BlockType::ItemDiamondPickaxe, 1};
        }
    }

    // 2. Axes (3 material in L-shape at top-left or top-right, 2 sticks below)
    if (itemCount == 5 && checkToolHandle(1)) {
        // Left-facing Axe: (0, 1, 3) are material
        BlockType mat = grid[0];
        if (mat != BlockType::Air && grid[1] == mat && grid[3] == mat) {
            if (isAnyPlanks(mat)) return {BlockType::ItemWoodenAxe, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneAxe, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronAxe, 1};
            if (mat == BlockType::ItemGoldIngot || mat == BlockType::OreGold) return {BlockType::ItemGoldenAxe, 1};
            if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondAxe, 1};
        }
        // Right-facing Axe: (1, 2, 5) are material
        mat = grid[2];
        if (mat != BlockType::Air && grid[1] == mat && grid[5] == mat) {
            if (isAnyPlanks(mat)) return {BlockType::ItemWoodenAxe, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneAxe, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronAxe, 1};
            if (mat == BlockType::ItemGoldIngot || mat == BlockType::OreGold) return {BlockType::ItemGoldenAxe, 1};
            if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondAxe, 1};
        }
    }

    // 3. Shovels (1 material top center [1], 2 sticks below [4, 7])
    if (itemCount == 3 && checkToolHandle(1)) {
        BlockType mat = grid[1];
        if (isAnyPlanks(mat)) return {BlockType::ItemWoodenShovel, 1};
        if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneShovel, 1};
        if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronShovel, 1};
        if (mat == BlockType::ItemGoldIngot || mat == BlockType::OreGold) return {BlockType::ItemGoldenShovel, 1};
        if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondShovel, 1};
    }

    // 4. Swords (2 material vertical [1, 4], 1 stick below [7])
    if (itemCount == 3 && grid[7] == BlockType::ItemStick) {
        BlockType mat = grid[1];
        if (mat != BlockType::Air && grid[4] == mat) {
            if (isAnyPlanks(mat)) return {BlockType::ItemWoodenSword, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneSword, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronSword, 1};
            if (mat == BlockType::ItemGoldIngot || mat == BlockType::OreGold) return {BlockType::ItemGoldenSword, 1};
            if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondSword, 1};
        }
    }

    // 5. Bow: 3 Sticks (slots 1, 3, 7) + 3 Strings (slots 2, 5, 8) or 3 Sticks anywhere curved
    if (itemCount == 6) {
        if (grid[1] == BlockType::ItemStick && grid[3] == BlockType::ItemStick && grid[7] == BlockType::ItemStick &&
            (grid[2] == BlockType::ItemString || isAnyWool(grid[2])) &&
            (grid[5] == BlockType::ItemString || isAnyWool(grid[5])) &&
            (grid[8] == BlockType::ItemString || isAnyWool(grid[8]))) {
            return {BlockType::ItemBow, 1};
        }
    }
    // Also allow crafting Bow with 3 sticks + 2 strings/leaves
    if ((grid[1] == BlockType::ItemStick && grid[3] == BlockType::ItemStick && grid[7] == BlockType::ItemStick) &&
        (grid[5] == BlockType::ItemString || isAnyWool(grid[5]) || grid[5] == BlockType::ItemStick)) {
        return {BlockType::ItemBow, 1};
    }

    // 6. Arrows: 1 Flint/Stone (top [1]), 1 Stick (center [4]), 1 Leaf/Flower (bottom [7]) -> 4 Arrows
    if (itemCount == 3 && grid[4] == BlockType::ItemStick &&
        (grid[1] == BlockType::ItemFlint || grid[1] == BlockType::CobbleStone || grid[1] == BlockType::Stone) &&
        (Cell{grid[7]}.isLeaves() || grid[7] == BlockType::TallGrass || grid[7] == BlockType::ItemStick)) {
        return {BlockType::ItemArrow, 4};
    }

    // 7. Hoes (2 material top row [0, 1] or [1, 2], 2 sticks below [4, 7])
    if (itemCount == 4 && checkToolHandle(1)) {
        if (grid[0] != BlockType::Air && grid[1] == grid[0]) {
            BlockType mat = grid[0];
            if (isAnyPlanks(mat)) return {BlockType::ItemWoodenHoe, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneHoe, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronHoe, 1};
            if (mat == BlockType::ItemGoldIngot || mat == BlockType::OreGold) return {BlockType::ItemGoldenHoe, 1};
            if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondHoe, 1};
        } else if (grid[2] != BlockType::Air && grid[1] == grid[2]) {
            BlockType mat = grid[2];
            if (isAnyPlanks(mat)) return {BlockType::ItemWoodenHoe, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneHoe, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronHoe, 1};
            if (mat == BlockType::ItemGoldIngot || mat == BlockType::OreGold) return {BlockType::ItemGoldenHoe, 1};
            if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondHoe, 1};
        }
    }

    // 8. Furnace: 8 Cobblestone around center (slot 4 is Air)
    if (itemCount == 8 &&
        grid[0] == BlockType::CobbleStone && grid[1] == BlockType::CobbleStone && grid[2] == BlockType::CobbleStone &&
        grid[3] == BlockType::CobbleStone && grid[4] == BlockType::Air         && grid[5] == BlockType::CobbleStone &&
        grid[6] == BlockType::CobbleStone && grid[7] == BlockType::CobbleStone && grid[8] == BlockType::CobbleStone) {
        return {BlockType::Furnace, 1};
    }

    // 9. Lantern & Soul Lantern
    if (itemCount == 9 && (grid[4] == BlockType::Torch || grid[4] == BlockType::TorchSoul)) {
        bool allIron = true;
        for (int i = 0; i < 9; ++i) {
            if (i == 4) continue;
            if (grid[i] != BlockType::ItemIronIngot && grid[i] != BlockType::OreIron) {
                allIron = false;
                break;
            }
        }
        if (allIron) return {grid[4] == BlockType::TorchSoul ? BlockType::LanternSoul : BlockType::Lantern, 1};
    }
    // Also 1 Torch + 1 Iron Ingot in center of 3x3
    if (itemCount == 2 && (grid[4] == BlockType::Torch || grid[4] == BlockType::TorchSoul) &&
        (grid[1] == BlockType::ItemIronIngot || grid[3] == BlockType::ItemIronIngot ||
         grid[5] == BlockType::ItemIronIngot || grid[7] == BlockType::ItemIronIngot ||
         grid[1] == BlockType::OreIron || grid[3] == BlockType::OreIron ||
         grid[5] == BlockType::OreIron || grid[7] == BlockType::OreIron)) {
        return {grid[4] == BlockType::TorchSoul ? BlockType::LanternSoul : BlockType::Lantern, 1};
    }

    // 10. 9 Mineral Ingots/Gems -> 1 Block
    if (itemCount == 9) {
        bool uniform = true;
        for (int i = 1; i < 9; ++i) {
            if (grid[i] != grid[0]) { uniform = false; break; }
        }
        if (uniform) {
            if (grid[0] == BlockType::ItemIronIngot) return {BlockType::BlockIron, 1};
            if (grid[0] == BlockType::ItemGoldIngot) return {BlockType::BlockGold, 1};
            if (grid[0] == BlockType::ItemDiamond) return {BlockType::BlockDiamond, 1};
            if (grid[0] == BlockType::ItemEmerald) return {BlockType::BlockEmerald, 1};
            if (grid[0] == BlockType::ItemRedstoneDust) return {BlockType::BlockRedstone, 1};
            if (grid[0] == BlockType::OreCopper) return {BlockType::BlockCopper, 1};
            if (grid[0] == BlockType::OreLapis) return {BlockType::BlockLapis, 1};
        }
    }

    // 11. Bread: 3 TallGrass or 3 SugarCane in any horizontal row
    if (itemCount == 3) {
        for (int row = 0; row < 3; ++row) {
            int r = row * 3;
            if ((grid[r] == BlockType::TallGrass || grid[r] == BlockType::SugarCane) &&
                grid[r + 1] == grid[r] && grid[r + 2] == grid[r]) {
                return {BlockType::ItemBread, 1};
            }
        }
    }

    // 12. Bed: 3 Wool (or Leaves) horizontal over 3 Planks
    if (itemCount == 6) {
        bool woolRow1 = (isAnyWool(grid[3]) && isAnyWool(grid[4]) && isAnyWool(grid[5]));
        bool planksRow2 = (isAnyPlanks(grid[6]) && isAnyPlanks(grid[7]) && isAnyPlanks(grid[8]));
        if (woolRow1 && planksRow2) return {BlockType::Bed, 1};

        bool woolRow0 = (isAnyWool(grid[0]) && isAnyWool(grid[1]) && isAnyWool(grid[2]));
        bool planksRow1 = (isAnyPlanks(grid[3]) && isAnyPlanks(grid[4]) && isAnyPlanks(grid[5]));
        if (woolRow0 && planksRow1) return {BlockType::Bed, 1};
    }

    // 13. Wooden Doors: 6 Planks of same type in 2 vertical columns (yields 3 doors)
    if (itemCount == 6) {
        if (isAnyPlanks(grid[0]) && grid[1] == grid[0] &&
            grid[3] == grid[0] && grid[4] == grid[0] &&
            grid[6] == grid[0] && grid[7] == grid[0]) {
            return {getDoorFromPlanks(grid[0]), 3};
        }
        if (isAnyPlanks(grid[1]) && grid[2] == grid[1] &&
            grid[4] == grid[1] && grid[5] == grid[1] &&
            grid[7] == grid[1] && grid[8] == grid[1]) {
            return {getDoorFromPlanks(grid[1]), 3};
        }
    }

    // 14. Trapdoors: 6 Planks of same type in 2 horizontal rows (yields 2 trapdoors)
    if (itemCount == 6) {
        if (isAnyPlanks(grid[0]) && grid[1] == grid[0] && grid[2] == grid[0] &&
            grid[3] == grid[0] && grid[4] == grid[0] && grid[5] == grid[0]) {
            return {getTrapdoorFromPlanks(grid[0]), 2};
        }
        if (isAnyPlanks(grid[3]) && grid[4] == grid[3] && grid[5] == grid[3] &&
            grid[6] == grid[3] && grid[7] == grid[3] && grid[8] == grid[3]) {
            return {getTrapdoorFromPlanks(grid[3]), 2};
        }
    }

    // 15. Iron Door: 6 Iron Ingots in 2 vertical columns
    if (itemCount == 6) {
        auto isIron = [](BlockType b) { return b == BlockType::ItemIronIngot || b == BlockType::OreIron; };
        if (isIron(grid[0]) && isIron(grid[1]) &&
            isIron(grid[3]) && isIron(grid[4]) &&
            isIron(grid[6]) && isIron(grid[7])) {
            return {BlockType::DoorIron, 1};
        }
        if (isIron(grid[1]) && isIron(grid[2]) &&
            isIron(grid[4]) && isIron(grid[5]) &&
            isIron(grid[7]) && isIron(grid[8])) {
            return {BlockType::DoorIron, 1};
        }
    }

    // 16. Iron Trapdoor: 4 Iron Ingots in 2x2
    auto isIron = [](BlockType b) { return b == BlockType::ItemIronIngot || b == BlockType::OreIron; };
    if (itemCount == 4) {
        if ((isIron(grid[0]) && isIron(grid[1]) && isIron(grid[3]) && isIron(grid[4])) ||
            (isIron(grid[1]) && isIron(grid[2]) && isIron(grid[4]) && isIron(grid[5])) ||
            (isIron(grid[3]) && isIron(grid[4]) && isIron(grid[6]) && isIron(grid[7])) ||
            (isIron(grid[4]) && isIron(grid[5]) && isIron(grid[7]) && isIron(grid[8]))) {
            return {BlockType::TrapdoorIron, 1};
        }
    }

    // 17. Torches: 1 Coal over 1 Stick -> 4 Torches (any column in 3x3)
    for (int col = 0; col < 3; ++col) {
        if (itemCount == 2) {
            if ((grid[col] == BlockType::ItemCoal || grid[col] == BlockType::OreCoal) && grid[col + 3] == BlockType::ItemStick) return {BlockType::Torch, 4};
            if ((grid[col + 3] == BlockType::ItemCoal || grid[col + 3] == BlockType::OreCoal) && grid[col + 6] == BlockType::ItemStick) return {BlockType::Torch, 4};
        }
        // Soul Torch: 1 Coal over 1 Stick over 1 SoulSand
        if (itemCount == 3) {
            if ((grid[col] == BlockType::ItemCoal || grid[col] == BlockType::OreCoal) &&
                grid[col + 3] == BlockType::ItemStick &&
                grid[col + 6] == BlockType::SoulSand) {
                return {BlockType::TorchSoul, 4};
            }
        }
    }

    // 18. Sticks: 2 Planks vertically -> 4 Sticks (anywhere in 3x3)
    for (int col = 0; col < 3; ++col) {
        if (itemCount == 2) {
            if (isAnyPlanks(grid[col]) && isAnyPlanks(grid[col + 3])) return {BlockType::ItemStick, 4};
            if (isAnyPlanks(grid[col + 3]) && isAnyPlanks(grid[col + 6])) return {BlockType::ItemStick, 4};
        }
    }

    // 19. 4 Planks 2x2 -> Crafting Table
    if (itemCount == 4) {
        auto checkPlanks2x2 = [&](int a, int b, int c, int d) {
            return isAnyPlanks(grid[a]) && isAnyPlanks(grid[b]) && isAnyPlanks(grid[c]) && isAnyPlanks(grid[d]);
        };
        if (checkPlanks2x2(0, 1, 3, 4) || checkPlanks2x2(1, 2, 4, 5) ||
            checkPlanks2x2(3, 4, 6, 7) || checkPlanks2x2(4, 5, 7, 8)) {
            return {BlockType::CraftingTable, 1};
        }

        // Deepslate bricks & tiles (4 in 2x2)
        auto is2x2Block = [&](BlockType target) -> bool {
            return (grid[0] == target && grid[1] == target && grid[3] == target && grid[4] == target) ||
                   (grid[1] == target && grid[2] == target && grid[4] == target && grid[5] == target) ||
                   (grid[3] == target && grid[4] == target && grid[6] == target && grid[7] == target) ||
                   (grid[4] == target && grid[5] == target && grid[7] == target && grid[8] == target);
        };
        if (is2x2Block(BlockType::CobbledDeepslate)) return {BlockType::DeepslateBricks, 4};
        if (is2x2Block(BlockType::DeepslateBricks)) return {BlockType::DeepslateTiles, 4};
    }

    // 20. Single Wood Log anywhere -> 4 Planks
    if (itemCount == 1) {
        for (int i = 0; i < 9; ++i) {
            BlockType p = getPlanksFromLog(grid[i]);
            if (p != BlockType::Air) return {p, 4};
        }
    }

    // 21. TNT: 4 Sand + 1 Coal/Redstone in cross/center
    if (itemCount == 5 && grid[4] != BlockType::Air) {
        bool isCore = (grid[4] == BlockType::ItemCoal || grid[4] == BlockType::OreCoal ||
                       grid[4] == BlockType::OreRedstone || grid[4] == BlockType::ItemRedstoneDust);
        bool isSands = (grid[1] == BlockType::Sand && grid[3] == BlockType::Sand &&
                        grid[5] == BlockType::Sand && grid[7] == BlockType::Sand);
        if (isCore && isSands) return {BlockType::TNT, 1};
    }

    return {BlockType::Air, 0};
}

} // namespace prismcraft
