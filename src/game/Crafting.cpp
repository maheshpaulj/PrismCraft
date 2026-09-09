#include "Crafting.hpp"

namespace prismcraft {

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
    if (itemCount == 1 && singleItem == BlockType::Wood) {
        return {BlockType::Planks, 4};
    }

    // 2. 2 Planks vertically -> 4 Sticks
    if (itemCount == 2) {
        if ((grid[0] == BlockType::Planks && grid[2] == BlockType::Planks) ||
            (grid[1] == BlockType::Planks && grid[3] == BlockType::Planks)) {
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
    }

    // 3. 4 Planks (fill all 4 slots) -> Crafting Table
    if (itemCount == 4 && grid[0] == BlockType::Planks && grid[1] == BlockType::Planks &&
        grid[2] == BlockType::Planks && grid[3] == BlockType::Planks) {
        return {BlockType::CraftingTable, 1};
    }

    // 4. 4 Sand -> 4 Glass
    if (itemCount == 4 && grid[0] == BlockType::Sand && grid[1] == BlockType::Sand &&
        grid[2] == BlockType::Sand && grid[3] == BlockType::Sand) {
        return {BlockType::Glass, 4};
    }

    // 5. 2x2 Wooden Sword (slot 0 Planks, slot 2 Stick)
    if (itemCount == 2 && grid[0] == BlockType::Planks && grid[2] == BlockType::ItemStick) {
        return {BlockType::ItemWoodenSword, 1};
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
            if (topMat == BlockType::Planks) return {BlockType::ItemWoodenPickaxe, 1};
            if (topMat == BlockType::CobbleStone || topMat == BlockType::Stone) return {BlockType::ItemStonePickaxe, 1};
            if (topMat == BlockType::ItemIronIngot || topMat == BlockType::OreIron) return {BlockType::ItemIronPickaxe, 1};
            if (topMat == BlockType::ItemDiamond || topMat == BlockType::OreDiamond) return {BlockType::ItemDiamondPickaxe, 1};
        }
    }

    // 2. Axes (3 material in L-shape at top-left or top-right, 2 sticks below)
    if (itemCount == 5 && checkToolHandle(1)) {
        // Left-facing Axe: (0, 1, 3) are material
        BlockType mat = grid[0];
        if (mat != BlockType::Air && grid[1] == mat && grid[3] == mat) {
            if (mat == BlockType::Planks) return {BlockType::ItemWoodenAxe, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneAxe, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronAxe, 1};
            if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondAxe, 1};
        }
        // Right-facing Axe: (1, 2, 5) are material
        mat = grid[2];
        if (mat != BlockType::Air && grid[1] == mat && grid[5] == mat) {
            if (mat == BlockType::Planks) return {BlockType::ItemWoodenAxe, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneAxe, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronAxe, 1};
            if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondAxe, 1};
        }
    }

    // 3. Shovels (1 material top center [1], 2 sticks below [4, 7])
    if (itemCount == 3 && checkToolHandle(1)) {
        BlockType mat = grid[1];
        if (mat == BlockType::Planks) return {BlockType::ItemWoodenShovel, 1};
        if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneShovel, 1};
        if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronShovel, 1};
        if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondShovel, 1};
    }

    // 4. Swords (2 material vertical [1, 4], 1 stick below [7])
    if (itemCount == 3 && grid[7] == BlockType::ItemStick) {
        BlockType mat = grid[1];
        if (mat != BlockType::Air && grid[4] == mat) {
            if (mat == BlockType::Planks) return {BlockType::ItemWoodenSword, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneSword, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronSword, 1};
            if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondSword, 1};
        }
    }

    // 5. Bow: 3 Sticks (slots 1, 3, 7) + 3 Strings (slots 2, 5, 8) or 3 Sticks anywhere curved
    if (itemCount == 6) {
        if (grid[1] == BlockType::ItemStick && grid[3] == BlockType::ItemStick && grid[7] == BlockType::ItemStick &&
            (grid[2] == BlockType::ItemString || grid[2] == BlockType::Leaves) &&
            (grid[5] == BlockType::ItemString || grid[5] == BlockType::Leaves) &&
            (grid[8] == BlockType::ItemString || grid[8] == BlockType::Leaves)) {
            return {BlockType::ItemBow, 1};
        }
    }
    // Also allow crafting Bow with 3 sticks + 2 strings/leaves
    if ((grid[1] == BlockType::ItemStick && grid[3] == BlockType::ItemStick && grid[7] == BlockType::ItemStick) &&
        (grid[5] == BlockType::ItemString || grid[5] == BlockType::Leaves || grid[5] == BlockType::ItemStick)) {
        return {BlockType::ItemBow, 1};
    }

    // 6. Arrows: 1 Flint/Stone (top [1]), 1 Stick (center [4]), 1 Leaf/Flower (bottom [7]) -> 4 Arrows
    if (itemCount == 3 && grid[4] == BlockType::ItemStick &&
        (grid[1] == BlockType::ItemFlint || grid[1] == BlockType::CobbleStone || grid[1] == BlockType::Stone) &&
        (grid[7] == BlockType::Leaves || grid[7] == BlockType::TallGrass || grid[7] == BlockType::ItemStick)) {
        return {BlockType::ItemArrow, 4};
    }

    // 7. Hoes (2 material top row [0, 1] or [1, 2], 2 sticks below [4, 7])
    if (itemCount == 4 && checkToolHandle(1)) {
        if (grid[0] != BlockType::Air && grid[1] == grid[0]) {
            BlockType mat = grid[0];
            if (mat == BlockType::Planks) return {BlockType::ItemWoodenHoe, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneHoe, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronHoe, 1};
            if (mat == BlockType::ItemDiamond || mat == BlockType::OreDiamond) return {BlockType::ItemDiamondHoe, 1};
        } else if (grid[2] != BlockType::Air && grid[1] == grid[2]) {
            BlockType mat = grid[2];
            if (mat == BlockType::Planks) return {BlockType::ItemWoodenHoe, 1};
            if (mat == BlockType::CobbleStone || mat == BlockType::Stone) return {BlockType::ItemStoneHoe, 1};
            if (mat == BlockType::ItemIronIngot || mat == BlockType::OreIron) return {BlockType::ItemIronHoe, 1};
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

    // 8. 1 Coal over 1 Stick -> 4 Torches (any column in 3x3)
    for (int col = 0; col < 3; ++col) {
        if (itemCount == 2) {
            if ((grid[col] == BlockType::ItemCoal || grid[col] == BlockType::OreCoal) && grid[col + 3] == BlockType::ItemStick) return {BlockType::Torch, 4};
            if ((grid[col + 3] == BlockType::ItemCoal || grid[col + 3] == BlockType::OreCoal) && grid[col + 6] == BlockType::ItemStick) return {BlockType::Torch, 4};
        }
    }

    // 9. 2 Planks vertically -> 4 Sticks (anywhere in 3x3)
    for (int col = 0; col < 3; ++col) {
        if (itemCount == 2) {
            if (grid[col] == BlockType::Planks && grid[col + 3] == BlockType::Planks) return {BlockType::ItemStick, 4};
            if (grid[col + 3] == BlockType::Planks && grid[col + 6] == BlockType::Planks) return {BlockType::ItemStick, 4};
        }
    }

    // 10. 4 Planks 2x2 -> Crafting Table
    if (itemCount == 4) {
        if (grid[0] == BlockType::Planks && grid[1] == BlockType::Planks &&
            grid[3] == BlockType::Planks && grid[4] == BlockType::Planks) return {BlockType::CraftingTable, 1};
        if (grid[1] == BlockType::Planks && grid[2] == BlockType::Planks &&
            grid[4] == BlockType::Planks && grid[5] == BlockType::Planks) return {BlockType::CraftingTable, 1};
        if (grid[3] == BlockType::Planks && grid[4] == BlockType::Planks &&
            grid[6] == BlockType::Planks && grid[7] == BlockType::Planks) return {BlockType::CraftingTable, 1};
        if (grid[4] == BlockType::Planks && grid[5] == BlockType::Planks &&
            grid[7] == BlockType::Planks && grid[8] == BlockType::Planks) return {BlockType::CraftingTable, 1};
    }

    // 11. Single Wood Log anywhere -> 4 Planks
    if (itemCount == 1) {
        for (int i = 0; i < 9; ++i) {
            if (grid[i] == BlockType::Wood) return {BlockType::Planks, 4};
        }
    }

    return {BlockType::Air, 0};
}

} // namespace prismcraft
