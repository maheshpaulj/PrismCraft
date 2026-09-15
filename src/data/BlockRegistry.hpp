#pragma once
#include "world/Cell.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>

namespace prismcraft {

enum class BlockRenderType : uint8_t {
    Standard,   // Full triangular prism
    Foliage,    // Crossed X quads with alpha cutout
    Torch,      // 3D wooden post with glowing head
    Door,       // 3D swinging panel
    Bed,        // 3D triangular mattress with legs
    Trapdoor,   // 3D flipping triangular panel
    Lantern,    // 3D hanging/standing small box
    Cake        // Partial-height prism
};

struct BlockDefinition {
    BlockType type = BlockType::Air;
    std::string name;
    std::string displayName;
    BlockRenderType renderType = BlockRenderType::Standard;

    // Textures (names without .png)
    std::string texAll;
    std::string texTop;
    std::string texBottom;
    std::string texSide;
    std::string texFront;  // Optional (e.g. furnace front, crafting table front)
    std::string texDoorUpper;
    std::string texDoorLower;
    std::string texItem;

    // Tint
    bool hasTopTint = false;
    glm::vec3 topTint{1.0f};

    // Properties
    bool isSolid = true;
    bool isOpaque = true;
    bool isTransparent = false;
    float hardness = 1.0f;
    std::string tool = "none";
    uint8_t lightLevel = 0;
    std::string drops;
    std::string sound = "stone";

    // Tags
    std::vector<std::string> tags;

    [[nodiscard]] bool hasTag(const std::string& tag) const {
        for (const auto& t : tags) {
            if (t == tag) return true;
        }
        return false;
    }
};

class BlockRegistry {
public:
    static bool init(const std::string& blocksJsonPath = "assets/blocks.json");
    static bool isInitialized();

    static const BlockDefinition& getDef(BlockType type);
    static const BlockDefinition* getDefByName(const std::string& name);
    static BlockType getTypeByName(const std::string& name);

    // Texture tile index binding
    static void setTextureTile(const std::string& textureName, int tileIndex);
    static int getTextureTile(const std::string& textureName);
    static const std::unordered_map<std::string, int>& getTextureTileMap();

    // Query tile index for a block face (0=top, 1=bottom, 2=base, 3=left, 4=right)
    static int getTileForBlock(BlockType type, int faceIndex);

    // List of all texture names referenced by all loaded blocks
    static std::vector<std::string> getReferencedTextureNames();

private:
    static void registerFallbackDefaults();
    static BlockType stringToBlockType(const std::string& name, int id);

    static bool s_initialized;
    static std::array<BlockDefinition, 256> s_defs;
    static std::unordered_map<std::string, BlockType> s_nameToType;
    static std::unordered_map<std::string, int> s_texToTile;
    static BlockDefinition s_airDef;
};

} // namespace prismcraft
