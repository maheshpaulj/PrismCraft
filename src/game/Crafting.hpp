#pragma once
#include "world/Cell.hpp"
#include <array>
#include <vector>

namespace prismcraft {

struct CraftingResult {
    BlockType item = BlockType::Air;
    int count = 0;
};

class CraftingSystem {
public:
    static CraftingResult craft2x2(const std::array<BlockType, 4>& grid);
    static CraftingResult craft3x3(const std::array<BlockType, 9>& grid);
};

} // namespace prismcraft
