#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "game/Crafting.hpp"
#include "core/GameOptions.hpp"
#include "world/SaveManager.hpp"
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
    WorldSelect,
    WorldCreation,
    LoadingWorld,
    Playing,
    Inventory,
    CraftingTable,
    Paused,
    Options,
    VideoSettings,
    LODSettings,
    VibrantVisualsSettings,
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
                int totalChunks = 0,
                VkDescriptorSet descSet = VK_NULL_HANDLE);

    // Returns:
    // 0: None, 1: Play/Resume, 2: Open World Creation, 3: Start Generated World,
    // 4: Return to Main Menu, 5: Quit, 6: Open Options, 7: Set FOV, 8: Set Sens,
    // 9: Toggle Audio, 10: Toggle Video, 11: Respawn, 12: Open World Select, 13: Play Selected World, 14: Delete World
    int handleClick(GameState& state, Player& player, glm::vec2 mousePos, uint32_t screenWidth, uint32_t screenHeight, uint32_t& worldSeed, GameOptions& options, bool isDown = false, bool isRightClick = false, ItemDropManager* itemDrops = nullptr);

    void returnCraftingItems(Player& player);
    [[nodiscard]] GameState getPreviousState() const { return m_previousState; }

    void refreshWorldList();
    [[nodiscard]] const std::vector<struct WorldMetadata>& getWorldList() const { return m_worldList; }
    [[nodiscard]] int getSelectedWorldIndex() const { return m_selectedWorldIndex; }
    void setSelectedWorldIndex(int idx) { m_selectedWorldIndex = idx; }
    [[nodiscard]] const std::string& getNewWorldName() const { return m_newWorldName; }
    void setNewWorldName(const std::string& n) { m_newWorldName = n; }
    [[nodiscard]] const std::string& getNewWorldSeedStr() const { return m_newWorldSeedStr; }
    void setNewWorldSeedStr(const std::string& s) { m_newWorldSeedStr = s; }
    [[nodiscard]] int getCreationFieldFocus() const { return m_creationFieldFocus; }
    void setCreationFieldFocus(int f) { m_creationFieldFocus = f; }

    void handleWorldCreationChar(char c);
    void handleWorldCreationBackspace();
    void handleWorldListScroll(int delta);

    void handleCreativeChar(char c);
    void handleCreativeBackspace();
    void handleCreativeScroll(int delta);
    void clearCreativeSearch();
    [[nodiscard]] const std::string& getCreativeSearchQuery() const { return m_creativeSearchQuery; }
    [[nodiscard]] int getCreativeScrollRow() const { return m_creativeScrollRow; }
    void setCreativeScrollRow(int r) { m_creativeScrollRow = r; }
    [[nodiscard]] bool isStartInCreative() const { return m_startInCreative; }
    void setStartInCreative(bool c) { m_startInCreative = c; }

    std::vector<BlockType> getCreativeCatalog(const std::string& query) const;

private:
    void rebuildMenuMesh(GameState state, const Player& player, uint32_t screenWidth, uint32_t screenHeight, glm::vec2 mousePos, const GameOptions& options, float loadingProgress = 0.0f, int loadedChunks = 0, int totalChunks = 0);

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    static constexpr VkDeviceSize MAX_MENU_VBO_SIZE = 2 * 1024 * 1024; // 2 MB
    static constexpr VkDeviceSize MAX_MENU_IBO_SIZE = 1 * 1024 * 1024; // 1 MB

    Buffer m_vbo[MAX_FRAMES_IN_FLIGHT];
    Buffer m_ibo[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_indexCount[MAX_FRAMES_IN_FLIGHT]{0, 0};

    GameState m_lastState = GameState::MainMenu;
    GameState m_previousState = GameState::MainMenu;
    uint32_t m_lastW = 0, m_lastH = 0;
    uint32_t m_currentSeed = 42;

    std::string m_creativeSearchQuery = "";
    int m_creativeScrollRow = 0;
    bool m_startInCreative = false;

    std::vector<WorldMetadata> m_worldList;
    int m_selectedWorldIndex = 0;
    int m_worldListScroll = 0;
    std::string m_newWorldName = "New World";
    std::string m_newWorldSeedStr = "";
    int m_creationFieldFocus = 0; // 0: Name, 1: Seed

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
