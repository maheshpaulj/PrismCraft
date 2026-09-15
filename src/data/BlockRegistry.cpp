#include "BlockRegistry.hpp"
#include "SimpleJson.hpp"
#include <iostream>
#include <algorithm>

namespace prismcraft {

bool BlockRegistry::s_initialized = false;
std::array<BlockDefinition, 256> BlockRegistry::s_defs{};
std::unordered_map<std::string, BlockType> BlockRegistry::s_nameToType{};
std::unordered_map<std::string, int> BlockRegistry::s_texToTile{};
BlockDefinition BlockRegistry::s_airDef{};

bool BlockRegistry::isInitialized() {
    return s_initialized;
}

const BlockDefinition& BlockRegistry::getDef(BlockType type) {
    size_t idx = static_cast<size_t>(type);
    if (idx < s_defs.size() && s_defs[idx].type != BlockType::Air) {
        return s_defs[idx];
    }
    return s_airDef;
}

const BlockDefinition* BlockRegistry::getDefByName(const std::string& name) {
    auto it = s_nameToType.find(name);
    if (it != s_nameToType.end()) {
        return &getDef(it->second);
    }
    return nullptr;
}

BlockType BlockRegistry::getTypeByName(const std::string& name) {
    auto it = s_nameToType.find(name);
    if (it != s_nameToType.end()) {
        return it->second;
    }
    return BlockType::Air;
}

void BlockRegistry::setTextureTile(const std::string& textureName, int tileIndex) {
    s_texToTile[textureName] = tileIndex;
}

int BlockRegistry::getTextureTile(const std::string& textureName) {
    auto it = s_texToTile.find(textureName);
    if (it != s_texToTile.end()) {
        return it->second;
    }
    return -1;
}

const std::unordered_map<std::string, int>& BlockRegistry::getTextureTileMap() {
    return s_texToTile;
}

int BlockRegistry::getTileForBlock(BlockType type, int faceIndex) {
    switch (type) {
        case BlockType::ItemStick:          { int t = getTextureTile("stick"); if (t >= 0) return t; break; }
        case BlockType::ItemCoal:           { int t = getTextureTile("coal"); if (t >= 0) return t; break; }
        case BlockType::ItemIronIngot:      { int t = getTextureTile("iron_ingot"); if (t >= 0) return t; break; }
        case BlockType::ItemGoldIngot:      { int t = getTextureTile("gold_ingot"); if (t >= 0) return t; break; }
        case BlockType::ItemDiamond:        { int t = getTextureTile("diamond"); if (t >= 0) return t; break; }
        case BlockType::ItemEmerald:        { int t = getTextureTile("emerald"); if (t >= 0) return t; break; }
        case BlockType::ItemQuartz:         { int t = getTextureTile("quartz"); if (t >= 0) return t; break; }
        case BlockType::ItemRedstoneDust:   { int t = getTextureTile("redstone"); if (t >= 0) return t; break; }
        case BlockType::ItemString:         { int t = getTextureTile("string"); if (t >= 0) return t; break; }
        case BlockType::ItemFlint:          { int t = getTextureTile("flint"); if (t >= 0) return t; break; }
        case BlockType::ItemApple:          { int t = getTextureTile("apple"); if (t >= 0) return t; break; }
        case BlockType::ItemBread:          { int t = getTextureTile("bread"); if (t >= 0) return t; break; }
        case BlockType::ItemWoodenPickaxe:  { int t = getTextureTile("wooden_pickaxe"); if (t >= 0) return t; break; }
        case BlockType::ItemStonePickaxe:   { int t = getTextureTile("stone_pickaxe"); if (t >= 0) return t; break; }
        case BlockType::ItemIronPickaxe:    { int t = getTextureTile("iron_pickaxe"); if (t >= 0) return t; break; }
        case BlockType::ItemGoldenPickaxe:  { int t = getTextureTile("golden_pickaxe"); if (t >= 0) return t; break; }
        case BlockType::ItemDiamondPickaxe: { int t = getTextureTile("diamond_pickaxe"); if (t >= 0) return t; break; }
        case BlockType::ItemWoodenAxe:      { int t = getTextureTile("wooden_axe"); if (t >= 0) return t; break; }
        case BlockType::ItemStoneAxe:       { int t = getTextureTile("stone_axe"); if (t >= 0) return t; break; }
        case BlockType::ItemIronAxe:        { int t = getTextureTile("iron_axe"); if (t >= 0) return t; break; }
        case BlockType::ItemGoldenAxe:      { int t = getTextureTile("golden_axe"); if (t >= 0) return t; break; }
        case BlockType::ItemDiamondAxe:     { int t = getTextureTile("diamond_axe"); if (t >= 0) return t; break; }
        case BlockType::ItemWoodenShovel:   { int t = getTextureTile("wooden_shovel"); if (t >= 0) return t; break; }
        case BlockType::ItemStoneShovel:    { int t = getTextureTile("stone_shovel"); if (t >= 0) return t; break; }
        case BlockType::ItemIronShovel:     { int t = getTextureTile("iron_shovel"); if (t >= 0) return t; break; }
        case BlockType::ItemGoldenShovel:   { int t = getTextureTile("golden_shovel"); if (t >= 0) return t; break; }
        case BlockType::ItemDiamondShovel:  { int t = getTextureTile("diamond_shovel"); if (t >= 0) return t; break; }
        case BlockType::ItemWoodenSword:    { int t = getTextureTile("wooden_sword"); if (t >= 0) return t; break; }
        case BlockType::ItemStoneSword:     { int t = getTextureTile("stone_sword"); if (t >= 0) return t; break; }
        case BlockType::ItemIronSword:      { int t = getTextureTile("iron_sword"); if (t >= 0) return t; break; }
        case BlockType::ItemGoldenSword:    { int t = getTextureTile("golden_sword"); if (t >= 0) return t; break; }
        case BlockType::ItemDiamondSword:   { int t = getTextureTile("diamond_sword"); if (t >= 0) return t; break; }
        case BlockType::ItemWoodenHoe:      { int t = getTextureTile("wooden_hoe"); if (t >= 0) return t; break; }
        case BlockType::ItemStoneHoe:       { int t = getTextureTile("stone_hoe"); if (t >= 0) return t; break; }
        case BlockType::ItemIronHoe:        { int t = getTextureTile("iron_hoe"); if (t >= 0) return t; break; }
        case BlockType::ItemGoldenHoe:      { int t = getTextureTile("golden_hoe"); if (t >= 0) return t; break; }
        case BlockType::ItemDiamondHoe:     { int t = getTextureTile("diamond_hoe"); if (t >= 0) return t; break; }
        case BlockType::ItemBow:            { int t = getTextureTile("bow"); if (t >= 0) return t; break; }
        case BlockType::ItemArrow:          { int t = getTextureTile("arrow"); if (t >= 0) return t; break; }
        default: break;
    }

    const auto& def = getDef(type);
    if (def.type == BlockType::Air) {
        return -1;
    }

    std::string tex;
    if (faceIndex == 0) { // Top / Item Icon
        if (!def.texItem.empty() && (def.renderType == BlockRenderType::Door || def.renderType == BlockRenderType::Bed || def.texTop.empty())) {
            tex = def.texItem;
        } else {
            tex = !def.texTop.empty() ? def.texTop : def.texAll;
        }
    } else if (faceIndex == 1) { // Bottom
        tex = !def.texBottom.empty() ? def.texBottom : def.texAll;
    } else if (faceIndex == 2 && !def.texFront.empty()) { // Front face
        tex = def.texFront;
    } else { // Sides 2, 3, 4
        tex = !def.texSide.empty() ? def.texSide : def.texAll;
    }

    if (!tex.empty()) {
        int tile = getTextureTile(tex);
        if (tile >= 0) return tile;
    }

    if (!def.texItem.empty()) {
        int tile = getTextureTile(def.texItem);
        if (tile >= 0) return tile;
    }

    return -1;
}

std::vector<std::string> BlockRegistry::getReferencedTextureNames() {
    std::vector<std::string> names;
    auto addName = [&](const std::string& n) {
        if (!n.empty() && std::find(names.begin(), names.end(), n) == names.end()) {
            names.push_back(n);
        }
    };

    for (const auto& def : s_defs) {
        if (def.type == BlockType::Air) continue;
        addName(def.texAll);
        addName(def.texTop);
        addName(def.texBottom);
        addName(def.texSide);
        addName(def.texFront);
        addName(def.texDoorUpper);
        addName(def.texDoorLower);
        addName(def.texItem);
    }
    return names;
}

BlockType BlockRegistry::stringToBlockType(const std::string& name, int id) {
    (void)name;
    if (id > 0 && id < 256) {
        return static_cast<BlockType>(id);
    }
    return BlockType::Air;
}

bool BlockRegistry::init(const std::string& blocksJsonPath) {
    s_airDef.type = BlockType::Air;
    s_airDef.name = "air";
    s_airDef.displayName = "Air";
    s_airDef.isSolid = false;
    s_airDef.isOpaque = false;
    s_airDef.isTransparent = true;

    // Search paths
    const std::vector<std::string> searchPaths = {
        blocksJsonPath,
        "assets/blocks.json",
        "../assets/blocks.json",
        "../../assets/blocks.json",
        "d:/Mahesh/Coding files/PrismCraft/PrismCraft/assets/blocks.json"
    };

    std::string err;
    JsonValue root;
    std::string resolvedPath;
    for (const auto& p : searchPaths) {
        root = JsonValue::parseFile(p, err);
        if (err.empty() && root.isObject()) {
            resolvedPath = p;
            break;
        }
    }

    if (resolvedPath.empty()) {
        std::cerr << "[BlockRegistry] Warning: Could not load blocks.json (" << err << "). Registering fallback defaults." << std::endl;
        registerFallbackDefaults();
        s_initialized = true;
        return false;
    }

    std::cout << "[BlockRegistry] Loading blocks from " << resolvedPath << std::endl;

    const auto& blocksObj = root["blocks"];
    if (!blocksObj.isObject()) {
        std::cerr << "[BlockRegistry] Error: 'blocks' object missing in JSON." << std::endl;
        registerFallbackDefaults();
        s_initialized = true;
        return false;
    }

    int count = 0;
    for (const auto& [name, bVal] : blocksObj.asObject()) {
        int id = bVal["id"].asInt(0);
        if (id <= 0 || id >= 256) continue;

        BlockDefinition def;
        def.type = static_cast<BlockType>(id);
        def.name = name;
        def.displayName = bVal["display_name"].asString(name);

        std::string rType = bVal["render_type"].asString("standard");
        if (rType == "foliage")      def.renderType = BlockRenderType::Foliage;
        else if (rType == "torch")   def.renderType = BlockRenderType::Torch;
        else if (rType == "door")    def.renderType = BlockRenderType::Door;
        else if (rType == "bed")     def.renderType = BlockRenderType::Bed;
        else if (rType == "trapdoor")def.renderType = BlockRenderType::Trapdoor;
        else if (rType == "lantern") def.renderType = BlockRenderType::Lantern;
        else if (rType == "cake")    def.renderType = BlockRenderType::Cake;
        else                         def.renderType = BlockRenderType::Standard;

        const auto& texObj = bVal["textures"];
        if (texObj.isObject()) {
            def.texAll       = texObj["all"].asString();
            def.texTop       = texObj["top"].asString();
            def.texBottom    = texObj["bottom"].asString();
            def.texSide      = texObj["sides"].asString(texObj["side"].asString());
            def.texFront     = texObj["front"].asString();
            def.texDoorUpper = texObj["upper"].asString();
            def.texDoorLower = texObj["lower"].asString();
            def.texItem      = texObj["item"].asString();
        }

        if (def.texItem.empty()) {
            if (def.renderType == BlockRenderType::Door) {
                def.texItem = def.name; // e.g. "oak_door", "iron_door", etc.
            } else if (def.renderType == BlockRenderType::Bed) {
                def.texItem = "bed";
            }
        }

        const auto& tintObj = bVal["tint"];
        if (tintObj.isObject() && tintObj.contains("top")) {
            const auto& tArr = tintObj["top"].asArray();
            if (tArr.size() >= 3) {
                def.hasTopTint = true;
                def.topTint = glm::vec3(tArr[0].asFloat() / 255.0f,
                                        tArr[1].asFloat() / 255.0f,
                                        tArr[2].asFloat() / 255.0f);
            }
        }

        const auto& propObj = bVal["properties"];
        if (propObj.isObject()) {
            def.isSolid       = propObj["solid"].asBool(true);
            def.isOpaque      = propObj["opaque"].asBool(true);
            def.isTransparent = !def.isOpaque;
            def.hardness      = propObj["hardness"].asFloat(1.0f);
            def.tool          = propObj["tool"].asString("none");
            def.lightLevel    = static_cast<uint8_t>(propObj["light_level"].asInt(0));
            def.drops         = propObj["drops"].asString(name);
            def.sound         = propObj["sound"].asString("stone");
        }

        const auto& tagsArr = bVal["tags"].asArray();
        for (const auto& tagVal : tagsArr) {
            def.tags.push_back(tagVal.asString());
        }

        s_defs[id] = def;
        s_nameToType[name] = def.type;
        count++;
    }

    std::cout << "[BlockRegistry] Successfully registered " << count << " blocks." << std::endl;
    s_initialized = true;
    return true;
}

void BlockRegistry::registerFallbackDefaults() {
    auto registerBasic = [](BlockType t, const std::string& name, const std::string& tex, bool solid = true, bool opaque = true) {
        int id = static_cast<int>(t);
        BlockDefinition d;
        d.type = t;
        d.name = name;
        d.displayName = name;
        d.texAll = tex;
        d.isSolid = solid;
        d.isOpaque = opaque;
        d.isTransparent = !opaque;
        s_defs[id] = d;
        s_nameToType[name] = t;
    };

    registerBasic(BlockType::Grass, "grass_block", "grass_block_side");
    s_defs[static_cast<int>(BlockType::Grass)].texTop = "grass_block_top";
    s_defs[static_cast<int>(BlockType::Grass)].texBottom = "dirt";
    s_defs[static_cast<int>(BlockType::Grass)].hasTopTint = true;
    s_defs[static_cast<int>(BlockType::Grass)].topTint = glm::vec3(110/255.0f, 190/255.0f, 65/255.0f);

    registerBasic(BlockType::Dirt, "dirt", "dirt");
    registerBasic(BlockType::Stone, "stone", "stone");
    registerBasic(BlockType::CobbleStone, "cobblestone", "cobblestone");
    registerBasic(BlockType::Bedrock, "bedrock", "bedrock");
    registerBasic(BlockType::Sand, "sand", "sand");
    registerBasic(BlockType::Gravel, "gravel", "gravel");

    registerBasic(BlockType::Wood, "oak_log", "oak_log");
    s_defs[static_cast<int>(BlockType::Wood)].texTop = "oak_log_top";
    s_defs[static_cast<int>(BlockType::Wood)].texBottom = "oak_log_top";

    registerBasic(BlockType::Planks, "oak_planks", "oak_planks");
    registerBasic(BlockType::Leaves, "oak_leaves", "oak_leaves", true, false);
    registerBasic(BlockType::Glass, "glass", "glass", true, false);
}

} // namespace prismcraft
