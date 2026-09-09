#pragma once

namespace prismcraft {

struct GameOptions {
    int fov = 75;
    float mouseSens = 1.0f;
    float audioVolume = 0.8f;
    float masterVolume = 1.0f;
    float musicVolume = 0.7f;
    float blocksVolume = 0.8f;
    float playerVolume = 0.8f;
    float ambientVolume = 0.7f;
    float uiScale = 1.0f;          // Discrete toggles: 1.0f, 1.5f, 2.0f, 2.5f, 3.0f
    int windowMode = 0;            // 0: Windowed, 1: Borderless, 2: Fullscreen
    int resIndex = 0;              // 0: 1280x720, 1: 1600x900, 2: 1920x1080, 3: 2560x1440
    int maxFps = 0;                // 0: Unlimited, 30, 60, 90, 120, 144, 240
    bool vsync = true;
    int renderDistance = 8;        // 4, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256
    int lodPreset = 1;             // 0: Performance, 1: Balanced, 2: Quality, 3: Custom
    float fogFalloff = 0.85f;      // 0.65f: Dense Mist, 0.85f: Atmospheric, 1.0f: Clear Sky
    bool clouds = true;
    bool cloudShadows = true;
    bool vibrantVisuals = true;    // Bedrock RTX-Style Vibrant Visuals Shaders
    bool lightOverlay = false;     // F7 toggle
    bool debugHUD = false;         // F3 toggle
};

} // namespace prismcraft
