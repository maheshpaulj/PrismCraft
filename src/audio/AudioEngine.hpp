#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace prismcraft {

enum class SoundEffect {
    Click,
    BlockBreak,
    BlockPlace,
    Footstep,
    ItemPop,
    PlayerHurt,
    BowShoot,
    BowHit,
    FallDamage,
    Splash,
    Swim,
    GrassStep,
    StoneStep,
    WoodStep,
    SandStep,
    GrassDig,
    StoneDig,
    WoodDig,
    SandDig,
    COUNT
};

enum class SoundCategory {
    Master,
    Music,
    Blocks,
    Player,
    UI
};

class AudioEngine {
public:
    static AudioEngine& get();

    bool init();
    void shutdown();
    void update(float dt);

    void playSound(SoundEffect effect, float volume = 1.0f);
    void playBlockDig(int blockType, float volume = 1.0f);
    void playBlockStep(int blockType, float volume = 0.6f);

    void setMasterVolume(float vol);
    void setMusicVolume(float vol);
    void setBlocksVolume(float vol);
    void setPlayerVolume(float vol);
    void setAmbientVolume(float vol);

    [[nodiscard]] float getMasterVolume() const { return m_masterVolume; }
    [[nodiscard]] float getMusicVolume() const { return m_musicVolume; }
    [[nodiscard]] float getBlocksVolume() const { return m_blocksVolume; }
    [[nodiscard]] float getPlayerVolume() const { return m_playerVolume; }
    [[nodiscard]] float getAmbientVolume() const { return m_ambientVolume; }

private:
    AudioEngine() = default;
    ~AudioEngine() { shutdown(); }

    void loadAllSounds();
    void generateProceduralFallback();

    bool m_initialized = false;
    float m_masterVolume = 1.0f;
    float m_musicVolume = 0.7f;
    float m_blocksVolume = 0.8f;
    float m_playerVolume = 0.8f;
    float m_ambientVolume = 0.7f;

    struct SoundBuffer {
        std::vector<int16_t> samples;
    };
    std::vector<SoundBuffer> m_soundBanks[static_cast<size_t>(SoundEffect::COUNT)];

    std::vector<SoundBuffer> m_musicTracks;
    size_t m_currentMusicIndex = 0;
    size_t m_musicCursor = 0;
    float m_musicTimer = 1.0f; // Start playing music shortly after startup
    bool m_musicPlaying = false;
};

} // namespace prismcraft
