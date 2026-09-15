#include "SaveManager.hpp"
#include "Chunk.hpp"
#include "data/SimpleJson.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

namespace prismcraft {

#pragma pack(push, 1)
struct ChunkFileHeader {
    char magic[4]{'P', 'C', 'C', 'K'};
    uint32_t version{1};
    int32_t cx{0};
    int32_t cz{0};
    uint32_t cellCount{CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * PRISMS_PER_COLUMN};
};
#pragma pack(pop)

std::string SaveManager::getSavesDirectory() {
    fs::path savesPath = fs::current_path() / "saves";
    try {
        if (!fs::exists(savesPath)) {
            fs::create_directories(savesPath);
        }
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] Failed to create saves dir: " << e.what() << std::endl;
    }
    return savesPath.string();
}

std::string SaveManager::sanitizeFolderName(const std::string& name) {
    std::string safe = name;
    const std::string illegalChars = "\\/:*?\"<>|";
    for (char& c : safe) {
        if (illegalChars.find(c) != std::string::npos || static_cast<unsigned char>(c) < 32) {
            c = '_';
        }
    }
    // Trim leading and trailing spaces or dots
    while (!safe.empty() && (safe.front() == ' ' || safe.front() == '.')) safe.erase(0, 1);
    while (!safe.empty() && (safe.back() == ' ' || safe.back() == '.')) safe.pop_back();
    if (safe.empty()) safe = "World";
    return safe;
}

std::string SaveManager::formatTimestamp(int64_t timestamp) {
    if (timestamp <= 0) return "Unknown";
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm tmVal;
#if defined(_WIN32)
    localtime_s(&tmVal, &t);
#else
    localtime_r(&t, &tmVal);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmVal, "%Y-%m-%d %H:%M");
    return oss.str();
}

std::vector<WorldMetadata> SaveManager::listWorlds() {
    std::vector<WorldMetadata> worlds;
    std::string savesDir = getSavesDirectory();

    try {
        for (const auto& entry : fs::directory_iterator(savesDir)) {
            if (entry.is_directory()) {
                fs::path levelPath = entry.path() / "level.json";
                if (fs::exists(levelPath)) {
                    WorldMetadata meta;
                    if (loadWorldMetadata(entry.path().filename().string(), meta)) {
                        worlds.push_back(meta);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] Error listing worlds: " << e.what() << std::endl;
    }

    // Sort by last played descending (most recently played first)
    std::sort(worlds.begin(), worlds.end(), [](const WorldMetadata& a, const WorldMetadata& b) {
        return a.lastPlayed > b.lastPlayed;
    });

    return worlds;
}

bool SaveManager::loadWorldMetadata(const std::string& folderName, WorldMetadata& outMeta) {
    fs::path levelPath = fs::path(getSavesDirectory()) / folderName / "level.json";
    if (!fs::exists(levelPath)) return false;

    std::ifstream file(levelPath);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string err;
    JsonValue root = JsonValue::parse(content, err);
    if (!root.isObject()) {
        std::cerr << "[SaveManager] JSON parse error in " << levelPath.string() << ": " << err << std::endl;
        return false;
    }

    outMeta.name = root["name"].asString(folderName);
    outMeta.folderName = folderName;
    outMeta.seed = static_cast<uint32_t>(root["seed"].asInt(12345));
    outMeta.gameMode = root["gameMode"].asInt(0);
    outMeta.timeOfDay = root["timeOfDay"].asFloat(0.19f);
    outMeta.lastPlayed = static_cast<int64_t>(root["lastPlayed"].asDouble(0.0));
    outMeta.lastPlayedFormatted = formatTimestamp(outMeta.lastPlayed);

    if (root.contains("player") && root["player"].isObject()) {
        const auto& p = root["player"];
        outMeta.playerPos.x = p["x"].asFloat(0.5f);
        outMeta.playerPos.y = p["y"].asFloat(65.0f);
        outMeta.playerPos.z = p["z"].asFloat(0.5f);
        outMeta.yaw = p["yaw"].asFloat(-1.5707963f);
        outMeta.pitch = p["pitch"].asFloat(0.0f);
        outMeta.health = p["health"].asFloat(20.0f);
        outMeta.hunger = p["hunger"].asFloat(20.0f);
        outMeta.selectedSlot = p["selectedSlot"].asInt(0);

        outMeta.inventory.clear();
        if (p.contains("inventory") && p["inventory"].isArray()) {
            for (const auto& item : p["inventory"].asArray()) {
                if (item.isObject()) {
                    SavedSlot s;
                    s.slot = item["slot"].asInt(0);
                    s.type = static_cast<BlockType>(item["type"].asInt(0));
                    s.count = item["count"].asInt(0);
                    outMeta.inventory.push_back(s);
                }
            }
        }
    }

    return true;
}

bool SaveManager::saveWorldMetadata(const std::string& folderName, const WorldMetadata& meta) {
    fs::path worldDir = fs::path(getSavesDirectory()) / folderName;
    try {
        if (!fs::exists(worldDir)) {
            fs::create_directories(worldDir);
        }
    } catch (...) {
        return false;
    }

    fs::path levelPath = worldDir / "level.json";
    std::ofstream file(levelPath, std::ios::trunc);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"name\": \"" << meta.name << "\",\n";
    file << "  \"seed\": " << meta.seed << ",\n";
    file << "  \"gameMode\": " << meta.gameMode << ",\n";
    file << "  \"timeOfDay\": " << meta.timeOfDay << ",\n";
    file << "  \"lastPlayed\": " << meta.lastPlayed << ",\n";
    file << "  \"player\": {\n";
    file << "    \"x\": " << meta.playerPos.x << ",\n";
    file << "    \"y\": " << meta.playerPos.y << ",\n";
    file << "    \"z\": " << meta.playerPos.z << ",\n";
    file << "    \"yaw\": " << meta.yaw << ",\n";
    file << "    \"pitch\": " << meta.pitch << ",\n";
    file << "    \"health\": " << meta.health << ",\n";
    file << "    \"hunger\": " << meta.hunger << ",\n";
    file << "    \"selectedSlot\": " << meta.selectedSlot << ",\n";
    file << "    \"inventory\": [\n";

    for (size_t i = 0; i < meta.inventory.size(); ++i) {
        const auto& s = meta.inventory[i];
        file << "      {\"slot\": " << s.slot << ", \"type\": " << static_cast<int>(s.type) << ", \"count\": " << s.count << "}";
        if (i + 1 < meta.inventory.size()) file << ",";
        file << "\n";
    }

    file << "    ]\n";
    file << "  }\n";
    file << "}\n";

    return true;
}

bool SaveManager::createWorld(const std::string& name, uint32_t seed, bool creative, WorldMetadata& outMeta) {
    std::string baseFolder = sanitizeFolderName(name);
    std::string folder = baseFolder;
    std::string savesDir = getSavesDirectory();

    // Ensure unique folder name
    int counter = 1;
    while (fs::exists(fs::path(savesDir) / folder)) {
        folder = baseFolder + " (" + std::to_string(counter++) + ")";
    }

    fs::path worldPath = fs::path(savesDir) / folder;
    try {
        fs::create_directories(worldPath / "chunks");
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] Failed to create world dirs: " << e.what() << std::endl;
        return false;
    }

    outMeta.name = name;
    outMeta.folderName = folder;
    outMeta.seed = seed;
    outMeta.gameMode = creative ? 1 : 0;
    outMeta.timeOfDay = 0.19f;
    outMeta.lastPlayed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    outMeta.lastPlayedFormatted = formatTimestamp(outMeta.lastPlayed);

    outMeta.playerPos = glm::vec3(0.5f, 65.0f, 0.5f);
    outMeta.yaw = -1.5707963f;
    outMeta.pitch = 0.0f;
    outMeta.health = 20.0f;
    outMeta.hunger = 20.0f;
    outMeta.selectedSlot = 0;
    outMeta.inventory.clear();

    return saveWorldMetadata(folder, outMeta);
}

bool SaveManager::deleteWorld(const std::string& folderName) {
    if (folderName.empty()) return false;
    fs::path worldPath = fs::path(getSavesDirectory()) / folderName;
    try {
        if (fs::exists(worldPath)) {
            fs::remove_all(worldPath);
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] Error deleting world " << folderName << ": " << e.what() << std::endl;
    }
    return false;
}

bool SaveManager::saveChunk(const std::string& folderName, const Chunk& chunk) {
    if (folderName.empty()) return false;
    fs::path chunkDir = fs::path(getSavesDirectory()) / folderName / "chunks";
    try {
        if (!fs::exists(chunkDir)) {
            fs::create_directories(chunkDir);
        }
    } catch (...) {
        return false;
    }

    ChunkCoord coord = chunk.getCoord();
    std::string filename = "c." + std::to_string(coord.cx) + "." + std::to_string(coord.cz) + ".bin";
    fs::path filePath = chunkDir / filename;

    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;

    ChunkFileHeader header;
    header.cx = coord.cx;
    header.cz = coord.cz;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(chunk.getRawCells()), header.cellCount * sizeof(Cell));

    return file.good();
}

bool SaveManager::loadChunk(const std::string& folderName, Chunk& chunk) {
    if (folderName.empty()) return false;
    ChunkCoord coord = chunk.getCoord();
    std::string filename = "c." + std::to_string(coord.cx) + "." + std::to_string(coord.cz) + ".bin";
    fs::path filePath = fs::path(getSavesDirectory()) / folderName / "chunks" / filename;

    if (!fs::exists(filePath)) return false;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    ChunkFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file.good()) return false;

    if (header.magic[0] != 'P' || header.magic[1] != 'C' || header.magic[2] != 'C' || header.magic[3] != 'K' ||
        header.version != 1 || header.cellCount != (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * PRISMS_PER_COLUMN)) {
        std::cerr << "[SaveManager] Corrupted chunk header: " << filePath.string() << std::endl;
        return false;
    }

    std::vector<Cell> buffer(header.cellCount);
    file.read(reinterpret_cast<char*>(buffer.data()), header.cellCount * sizeof(Cell));
    if (!file.good()) return false;

    chunk.loadRawCells(buffer.data());
    return true;
}

} // namespace prismcraft
