#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "game/Crafting.hpp"
#include "core/GameOptions.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace prismcraft {

class VulkanContext;
class CommandQueue;
class Player;
class ItemDropManager;

enum class GameState {
    MainMenu,
    WorldCreation,
    LoadingWorld,
    Playing,
    Inventory,
    CraftingTable,
    Paused,
    Options,
    VideoSettings,
    AudioSettings,
    ControlsSettings,
    Death
};

class MenuRenderer {
public:
    MenuRenderer(VulkanContext& context, CommandQueue& cmdQueue);

    void render(VkCommandBuffer cmd,
                const Pipeline& uiPipeline,
                GameState state,
                const Player& player,
                uint32_t screenWidth,
                uint32_t screenHeight,
                glm::vec2 mousePos,
                const GameOptions& options,
                float loadingProgress = 0.0f,
                int loadedChunks = 0,
                int totalChunks = 0);

    // Returns:
    // 0: None, 1: Play/Resume, 2: Open World Creation, 3: Start Generated World,
    // 4: Return to Main Menu, 5: Quit, 6: Open Options, 7: Set FOV, 8: Set Sens,
    // 9: Toggle Audio, 10: Toggle Video, 11: Respawn
    int handleClick(GameState& state, Player& player, glm::vec2 mousePos, uint32_t screenWidth, uint32_t screenHeight, uint32_t& worldSeed, GameOptions& options, bool isDown = false, bool isRightClick = false, ItemDropManager* itemDrops = nullptr);

    void returnCraftingItems(Player& player);
    [[nodiscard]] GameState getPreviousState() const { return m_previousState; }

private:
    void rebuildMenuMesh(GameState state, const Player& player, uint32_t screenWidth, uint32_t screenHeight, glm::vec2 mousePos, const GameOptions& options, float loadingProgress = 0.0f, int loadedChunks = 0, int totalChunks = 0);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo[MAX_FRAMES_IN_FLIGHT];
    Buffer m_ibo[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_indexCount[MAX_FRAMES_IN_FLIGHT]{0, 0};

    GameState m_lastState = GameState::MainMenu;
    GameState m_previousState = GameState::MainMenu;
    uint32_t m_lastW = 0, m_lastH = 0;
    uint32_t m_currentSeed = 42;

    BlockType m_cursorItem = BlockType::Air;
    int m_cursorCount = 0;
    std::array<BlockType, 4> m_craftingGrid{BlockType::Wood, BlockType::Air, BlockType::Air, BlockType::Air};
    std::array<int, 4> m_craftingCounts{4, 0, 0, 0};

    std::array<BlockType, 9> m_craftingTableGrid{
        BlockType::Planks, BlockType::Planks, BlockType::Planks,
        BlockType::Air,    BlockType::ItemStick, BlockType::Air,
        BlockType::Air,    BlockType::ItemStick, BlockType::Air
    };
    std::array<int, 9> m_craftingTableCounts{3, 3, 3, 0, 2, 0, 0, 2, 0};
};

} // namespace prismcraft
