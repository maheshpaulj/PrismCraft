#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <glm/vec3.hpp>
#include "world/Cell.hpp"
#include "world/Coordinates.hpp"

namespace prismcraft {

class Chunk;
class Player;

struct SavedSlot {
    int slot = 0;
    BlockType type = BlockType::Air;
    int count = 0;
};

struct WorldMetadata {
    std::string name = "New World";
    std::string folderName = "New World";
    uint32_t seed = 12345;
    int gameMode = 0; // 0: Survival, 1: Creative
    float timeOfDay = 0.19f;
    int64_t lastPlayed = 0;
    std::string lastPlayedFormatted = "";

    // Player State
    glm::vec3 playerPos{0.5f, 65.0f, 0.5f};
    float yaw = -1.5707963f;
    float pitch = 0.0f;
    float health = 20.0f;
    float hunger = 20.0f;
    int selectedSlot = 0;
    std::vector<SavedSlot> inventory;
};

class SaveManager {
public:
    static std::string getSavesDirectory();
    static std::string sanitizeFolderName(const std::string& name);
    static std::string formatTimestamp(int64_t timestamp);

    static std::vector<WorldMetadata> listWorlds();
    static bool loadWorldMetadata(const std::string& folderName, WorldMetadata& outMeta);
    static bool saveWorldMetadata(const std::string& folderName, const WorldMetadata& meta);
    static bool createWorld(const std::string& name, uint32_t seed, bool creative, WorldMetadata& outMeta);
    static bool deleteWorld(const std::string& folderName);

    static bool saveChunk(const std::string& folderName, const Chunk& chunk);
    static bool loadChunk(const std::string& folderName, Chunk& chunk);
};

} // namespace prismcraft
