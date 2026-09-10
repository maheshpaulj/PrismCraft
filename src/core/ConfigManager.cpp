#include "ConfigManager.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace prismcraft {

static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool ConfigManager::load(GameOptions& options, const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cout << "[Config] No options file found at " << filePath << ", using defaults." << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eqPos));
        std::string val = trim(line.substr(eqPos + 1));

        try {
            if (key == "fov") options.fov = std::clamp(std::stoi(val), 60, 110);
            else if (key == "mouseSens") options.mouseSens = std::clamp(std::stof(val), 0.1f, 4.0f);
            else if (key == "masterVolume") options.masterVolume = std::clamp(std::stof(val), 0.0f, 1.0f);
            else if (key == "audioVolume") options.audioVolume = std::clamp(std::stof(val), 0.0f, 1.0f);
            else if (key == "musicVolume") options.musicVolume = std::clamp(std::stof(val), 0.0f, 1.0f);
            else if (key == "blocksVolume") options.blocksVolume = std::clamp(std::stof(val), 0.0f, 1.0f);
            else if (key == "playerVolume") options.playerVolume = std::clamp(std::stof(val), 0.0f, 1.0f);
            else if (key == "ambientVolume") options.ambientVolume = std::clamp(std::stof(val), 0.0f, 1.0f);
            else if (key == "uiScale") options.uiScale = std::clamp(std::stof(val), 1.0f, 3.0f);
            else if (key == "windowMode") options.windowMode = std::clamp(std::stoi(val), 0, 2);
            else if (key == "resIndex") options.resIndex = std::clamp(std::stoi(val), 0, 3);
            else if (key == "maxFps") options.maxFps = std::stoi(val);
            else if (key == "vsync") options.vsync = (val == "1" || val == "true" || val == "True");
            else if (key == "renderDistance") options.renderDistance = std::clamp(std::stoi(val), 4, 24);
            else if (key == "lodPreset") options.lodPreset = std::clamp(std::stoi(val), 0, 3);
            else if (key == "fogFalloff") options.fogFalloff = std::clamp(std::stof(val), 0.5f, 1.2f);
            else if (key == "clouds") options.clouds = (val == "1" || val == "true" || val == "True");
            else if (key == "cloudShadows") options.cloudShadows = (val == "1" || val == "true" || val == "True");
            else if (key == "cloudSeed") options.cloudSeed = std::stoi(val);
            else if (key == "vibrantVisuals") options.vibrantVisuals = (val == "1" || val == "true" || val == "True");
            else if (key == "shadowQuality") options.shadowQuality = std::clamp(std::stoi(val), 0, 3);
            else if (key == "shadowDistance") options.shadowDistance = std::clamp(std::stoi(val), 0, 3);
            else if (key == "playerShadow") options.playerShadow = (val == "1" || val == "true" || val == "True");
            else if (key == "waterQuality") options.waterQuality = std::clamp(std::stoi(val), 0, 2);
            else if (key == "colorGrading") options.colorGrading = std::clamp(std::stoi(val), 0, 4);
            else if (key == "atmosphericFog") options.atmosphericFog = std::clamp(std::stoi(val), 0, 2);
            else if (key == "torchColorBleed") options.torchColorBleed = (val == "1" || val == "true" || val == "True");
            else if (key == "smoothLighting") options.smoothLighting = (val == "1" || val == "true" || val == "True");
            else if (key == "aoStrength") options.aoStrength = std::clamp(std::stof(val), 0.0f, 3.0f);
            else if (key == "lightOverlay") options.lightOverlay = (val == "1" || val == "true" || val == "True");
            else if (key == "exposure") options.exposure = std::clamp(std::stof(val), 0.2f, 3.0f);
            else if (key == "fogDensity") options.fogDensity = std::clamp(std::stof(val), 0.1f, 5.0f);
            else if (key == "fogHeight") options.fogHeight = std::clamp(std::stof(val), 20.0f, 160.0f);
            else if (key == "fogStartDist") options.fogStartDist = std::clamp(std::stof(val), 0.0f, 100.0f);
            else if (key == "scatteringStrength") options.scatteringStrength = std::clamp(std::stof(val), 0.0f, 4.0f);
        } catch (...) {
            // Ignore malformed line
        }
    }

    std::cout << "[Config] Successfully loaded options from " << filePath 
              << " (RenderDist: " << options.renderDistance << " chunks, LOD Preset: " << options.lodPreset << ")" << std::endl;
    return true;
}

bool ConfigManager::save(const GameOptions& options, const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to save options to " << filePath << std::endl;
        return false;
    }

    file << "# PrismCraft Game Configuration Options\n";
    file << "fov=" << options.fov << "\n";
    file << "mouseSens=" << options.mouseSens << "\n";
    file << "masterVolume=" << options.masterVolume << "\n";
    file << "audioVolume=" << options.audioVolume << "\n";
    file << "musicVolume=" << options.musicVolume << "\n";
    file << "blocksVolume=" << options.blocksVolume << "\n";
    file << "playerVolume=" << options.playerVolume << "\n";
    file << "ambientVolume=" << options.ambientVolume << "\n";
    file << "uiScale=" << options.uiScale << "\n";
    file << "windowMode=" << options.windowMode << "\n";
    file << "resIndex=" << options.resIndex << "\n";
    file << "maxFps=" << options.maxFps << "\n";
    file << "vsync=" << (options.vsync ? 1 : 0) << "\n";
    file << "renderDistance=" << options.renderDistance << "\n";
    file << "lodPreset=" << options.lodPreset << "\n";
    file << "fogFalloff=" << options.fogFalloff << "\n";
    file << "clouds=" << (options.clouds ? 1 : 0) << "\n";
    file << "cloudShadows=" << (options.cloudShadows ? 1 : 0) << "\n";
    file << "cloudSeed=" << options.cloudSeed << "\n";
    file << "vibrantVisuals=" << (options.vibrantVisuals ? 1 : 0) << "\n";
    file << "shadowQuality=" << options.shadowQuality << "\n";
    file << "shadowDistance=" << options.shadowDistance << "\n";
    file << "playerShadow=" << (options.playerShadow ? 1 : 0) << "\n";
    file << "waterQuality=" << options.waterQuality << "\n";
    file << "colorGrading=" << options.colorGrading << "\n";
    file << "atmosphericFog=" << options.atmosphericFog << "\n";
    file << "torchColorBleed=" << (options.torchColorBleed ? 1 : 0) << "\n";
    file << "smoothLighting=" << (options.smoothLighting ? 1 : 0) << "\n";
    file << "aoStrength=" << options.aoStrength << "\n";
    file << "lightOverlay=" << (options.lightOverlay ? 1 : 0) << "\n";
    file << "debugHUD=" << (options.debugHUD ? 1 : 0) << "\n";
    file << "exposure=" << options.exposure << "\n";
    file << "fogDensity=" << options.fogDensity << "\n";
    file << "fogHeight=" << options.fogHeight << "\n";
    file << "fogStartDist=" << options.fogStartDist << "\n";
    file << "scatteringStrength=" << options.scatteringStrength << "\n";

    std::cout << "[Config] Saved options to " << filePath << std::endl;
    return true;
}

} // namespace prismcraft
