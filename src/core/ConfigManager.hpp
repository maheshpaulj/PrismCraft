#pragma once
#include "core/GameOptions.hpp"
#include <string>

namespace prismcraft {

class ConfigManager {
public:
    static bool load(GameOptions& options, const std::string& filePath);
    static bool save(const GameOptions& options, const std::string& filePath);
};

} // namespace prismcraft
