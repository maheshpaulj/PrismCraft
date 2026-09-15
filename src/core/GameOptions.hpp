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
    int resIndex = 0;              // 0: 1280x720, 1: 1600x900, 2: 1920x1080, 3: 2560x1440, 4: Native
    int maxFps = 0;                // 0: Unlimited, 30, 60, 90, 120, 144, 240
    bool vsync = true;
    int renderDistance = 8;        // Active interactive chunks (4, 6, 8, 10, 12, 16, 24)
    int lodDistance = 128;         // Distant Horizons LOD radius in chunks (16, 24, 32, 48, 64, 96, 128, 160, 192, 224, 256)
    int lodPreset = 1;             // 0: Performance, 1: Balanced, 2: Quality, 3: Extreme
    bool showFPS = true;           // HUD FPS counter toggle
    bool showXYZ = true;           // HUD XYZ / Biome / Direction toggle
    bool shadersEnabled = true;    // Post-process shader effects toggle
    float fogFalloff = 0.85f;      // 0.65f: Dense Mist, 0.85f: Atmospheric, 1.0f: Clear Sky
    bool clouds = true;
    bool cloudShadows = true;
    int cloudSeed = 1337;          // Deterministic seed for baked volumetric cloud map
    bool vibrantVisuals = true;    // Bedrock RTX-Style Vibrant Visuals Shaders
    int shadowQuality = 2;         // 0: Off, 1: Low, 2: Medium, 3: High
    int shadowDistance = 1;        // 0: Near (48m), 1: Medium (80m), 2: Far (120m), 3: Ultra (160m)
    bool playerShadow = true;      // Steve's shadow on ground
    int waterQuality = 2;          // 0: Fast, 1: Fancy, 2: RTX
    int colorGrading = 1;          // 0: Off, 1: Cinematic, 2: Vibrant, 3: Warm, 4: Cool
    bool torchColorBleed = true;   // Colored torchlight falloff & tinting
    bool smoothLighting = true;    // Smooth vertex AO lighting
    float aoStrength = 1.0f;       // Ambient occlusion intensity multiplier (0.0: Off, 1.0: Subtle Realistic, 1.4: Enhanced)
    bool lightOverlay = false;     // F7 toggle
    bool debugHUD = false;         // F3 toggle
    float exposure = 0.95f;        // Physical camera exposure scale
    float fogDensity = 1.0f;       // Atmospheric optical depth multiplier
    float fogHeight = 62.0f;       // Base altitude of low-altitude ground haze
    float fogStartDist = 12.0f;    // Near distance threshold where atmospheric perspective begins
    float scatteringStrength = 1.0f; // Multiplier on forward solar Mie scattering
    int upscalerMode = 2;          // 0: Native/Off, 1: FSR 1.0 Spatial, 2: TSR Temporal (FSR 2-grade)
    int upscalerQuality = 1;       // 0: Ultra Quality (77%), 1: Quality (67%), 2: Balanced (59%), 3: Performance (50%)
    float upscalerSharpness = 0.75f; // 0.0 to 1.0 (AMD RCAS sharpness)
    int ramCacheSize = 2;          // 0: 512MB, 1: 1GB, 2: 2GB, 3: 4GB
};

} // namespace prismcraft
