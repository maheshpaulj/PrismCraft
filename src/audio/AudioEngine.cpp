#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "AudioEngine.hpp"
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <cstring>

namespace prismcraft {

struct ActiveVoice {
    const int16_t* samples = nullptr;
    size_t length = 0;
    size_t cursor = 0;
    float volume = 1.0f;
    SoundCategory category = SoundCategory::Player;
    bool active = false;
};

static constexpr int MAX_VOICES = 32;
static ActiveVoice s_voices[MAX_VOICES];
static std::mutex s_audioMutex;
static ma_device s_maDevice;

// Music streaming state
static const int16_t* s_musicSamples = nullptr;
static size_t s_musicLength = 0;
static size_t s_musicCursor = 0;
static float s_musicVolume = 1.0f;
static bool s_musicActive = false;

static void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pDevice;
    (void)pInput;
    float* out = static_cast<float*>(pOutput);
    std::fill_n(out, frameCount * 2, 0.0f); // Stereo output

    std::lock_guard<std::mutex> lock(s_audioMutex);
    float masterVol = AudioEngine::get().getMasterVolume();
    float musicVol = AudioEngine::get().getMusicVolume();
    float blocksVol = AudioEngine::get().getBlocksVolume();
    float playerVol = AudioEngine::get().getPlayerVolume();
    float uiVol = 1.0f;

    // 1. Mix Sound Effect Voices
    for (int v = 0; v < MAX_VOICES; ++v) {
        if (!s_voices[v].active) continue;

        float catVol = 1.0f;
        switch (s_voices[v].category) {
            case SoundCategory::Blocks: catVol = blocksVol; break;
            case SoundCategory::Player: catVol = playerVol; break;
            case SoundCategory::UI:     catVol = uiVol; break;
            default: break;
        }
        float effectiveVol = s_voices[v].volume * catVol * masterVol;

        for (ma_uint32 i = 0; i < frameCount; ++i) {
            if (s_voices[v].cursor < s_voices[v].length) {
                float sample = (s_voices[v].samples[s_voices[v].cursor] / 32768.0f) * effectiveVol;
                out[i * 2 + 0] += sample;
                out[i * 2 + 1] += sample;
                s_voices[v].cursor++;
            } else {
                s_voices[v].active = false;
                break;
            }
        }
    }

    // 2. Mix Background Music
    if (s_musicActive && s_musicSamples != nullptr && s_musicLength > 0) {
        float effectiveMusicVol = s_musicVolume * musicVol * masterVol * 0.75f;
        for (ma_uint32 i = 0; i < frameCount; ++i) {
            if (s_musicCursor < s_musicLength) {
                float sample = (s_musicSamples[s_musicCursor] / 32768.0f) * effectiveMusicVol;
                out[i * 2 + 0] += sample;
                out[i * 2 + 1] += sample;
                s_musicCursor++;
            } else {
                s_musicActive = false;
                break;
            }
        }
    }

    // 3. Soft clip limiter
    for (ma_uint32 i = 0; i < frameCount * 2; ++i) {
        out[i] = std::clamp(out[i], -1.0f, 1.0f);
    }
}

static bool loadWavFile(const std::string& filepath, std::vector<int16_t>& outSamples) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return false;

    char header[12];
    if (!file.read(header, 12)) return false;
    if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
        return false;
    }

    uint16_t numChannels = 1;
    uint32_t sampleRate = 44100;
    uint16_t bitsPerSample = 16;
    bool foundFmt = false;
    bool foundData = false;

    while (file) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        if (!file.read(chunkId, 4) || !file.read(reinterpret_cast<char*>(&chunkSize), 4)) break;

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFormat = 0;
            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            file.read(reinterpret_cast<char*>(&numChannels), 2);
            file.read(reinterpret_cast<char*>(&sampleRate), 4);
            file.seekg(4 + 2, std::ios::cur); // skip byteRate & blockAlign
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
            if (chunkSize > 16) file.seekg(chunkSize - 16, std::ios::cur);
            foundFmt = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            if (!foundFmt || bitsPerSample != 16) {
                file.seekg(chunkSize, std::ios::cur);
                continue;
            }
            size_t totalSamples = chunkSize / sizeof(int16_t);
            if (numChannels == 1) {
                outSamples.resize(totalSamples);
                file.read(reinterpret_cast<char*>(outSamples.data()), chunkSize);
            } else if (numChannels == 2) {
                // Downmix stereo to mono
                std::vector<int16_t> stereo(totalSamples);
                file.read(reinterpret_cast<char*>(stereo.data()), chunkSize);
                outSamples.resize(totalSamples / 2);
                for (size_t i = 0; i < outSamples.size(); ++i) {
                    int32_t mixed = (static_cast<int32_t>(stereo[i * 2]) + static_cast<int32_t>(stereo[i * 2 + 1])) / 2;
                    outSamples[i] = static_cast<int16_t>(mixed);
                }
            } else {
                file.seekg(chunkSize, std::ios::cur);
            }
            foundData = true;
            break;
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }
    return foundData && !outSamples.empty();
}

AudioEngine& AudioEngine::get() {
    static AudioEngine instance;
    return instance;
}

bool AudioEngine::init() {
    if (m_initialized) return true;

    loadAllSounds();

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2; // Stereo
    config.sampleRate = 44100;
    config.dataCallback = audioCallback;
    config.pUserData = nullptr;

    if (ma_device_init(nullptr, &config, &s_maDevice) != MA_SUCCESS) {
        return false;
    }

    if (ma_device_start(&s_maDevice) != MA_SUCCESS) {
        ma_device_uninit(&s_maDevice);
        return false;
    }

    m_initialized = true;
    return true;
}

void AudioEngine::shutdown() {
    if (m_initialized) {
        ma_device_stop(&s_maDevice);
        ma_device_uninit(&s_maDevice);
        m_initialized = false;
    }
}

void AudioEngine::setMasterVolume(float vol) {
    m_masterVolume = std::clamp(vol, 0.0f, 1.0f);
}
void AudioEngine::setMusicVolume(float vol) {
    m_musicVolume = std::clamp(vol, 0.0f, 1.0f);
}
void AudioEngine::setBlocksVolume(float vol) {
    m_blocksVolume = std::clamp(vol, 0.0f, 1.0f);
}
void AudioEngine::setPlayerVolume(float vol) {
    m_playerVolume = std::clamp(vol, 0.0f, 1.0f);
}
void AudioEngine::setAmbientVolume(float vol) {
    m_ambientVolume = std::clamp(vol, 0.0f, 1.0f);
}

void AudioEngine::loadAllSounds() {
    auto tryLoadBank = [&](SoundEffect effect, const std::vector<std::string>& files) {
        auto& bank = m_soundBanks[static_cast<size_t>(effect)];
        bank.clear();
        for (const auto& file : files) {
            std::vector<int16_t> samples;
            // Try relative to current working dir, and relative to assets
            std::string paths[] = {
                "assets/sounds/" + file,
                "x64/Release/assets/sounds/" + file,
                "../assets/sounds/" + file
            };
            for (const auto& p : paths) {
                if (loadWavFile(p, samples)) {
                    bank.push_back({std::move(samples)});
                    break;
                }
            }
        }
    };

    tryLoadBank(SoundEffect::Click, {"click.wav"});
    tryLoadBank(SoundEffect::ItemPop, {"pop.wav"});
    tryLoadBank(SoundEffect::BowShoot, {"bow_shoot.wav"});
    tryLoadBank(SoundEffect::BowHit, {"bow_hit1.wav", "bow_hit2.wav", "bow_hit3.wav", "bow_hit4.wav"});
    tryLoadBank(SoundEffect::PlayerHurt, {"hurt1.wav", "hurt2.wav", "hurt3.wav"});
    tryLoadBank(SoundEffect::FallDamage, {"fall.wav"});
    tryLoadBank(SoundEffect::Splash, {"splash.wav"});
    tryLoadBank(SoundEffect::Swim, {"swim1.wav", "swim2.wav", "swim3.wav", "swim4.wav"});

    tryLoadBank(SoundEffect::GrassStep, {"grass_step1.wav", "grass_step2.wav", "grass_step3.wav", "grass_step4.wav"});
    tryLoadBank(SoundEffect::StoneStep, {"stone_step1.wav", "stone_step2.wav", "stone_step3.wav", "stone_step4.wav"});
    tryLoadBank(SoundEffect::WoodStep, {"wood_step1.wav", "wood_step2.wav", "wood_step3.wav", "wood_step4.wav"});
    tryLoadBank(SoundEffect::SandStep, {"sand_step1.wav", "sand_step2.wav", "sand_step3.wav", "sand_step4.wav"});

    tryLoadBank(SoundEffect::GrassDig, {"grass_dig1.wav", "grass_dig2.wav", "grass_dig3.wav", "grass_dig4.wav"});
    tryLoadBank(SoundEffect::StoneDig, {"stone_dig1.wav", "stone_dig2.wav", "stone_dig3.wav", "stone_dig4.wav"});
    tryLoadBank(SoundEffect::WoodDig, {"wood_dig1.wav", "wood_dig2.wav", "wood_dig3.wav", "wood_dig4.wav"});
    tryLoadBank(SoundEffect::SandDig, {"sand_dig1.wav", "sand_dig2.wav", "sand_dig3.wav", "sand_dig4.wav"});

    // Map legacy/composite effects to default variations
    m_soundBanks[static_cast<size_t>(SoundEffect::Footstep)] = m_soundBanks[static_cast<size_t>(SoundEffect::GrassStep)];
    m_soundBanks[static_cast<size_t>(SoundEffect::BlockBreak)] = m_soundBanks[static_cast<size_t>(SoundEffect::StoneDig)];
    m_soundBanks[static_cast<size_t>(SoundEffect::BlockPlace)] = m_soundBanks[static_cast<size_t>(SoundEffect::WoodDig)];

    // Load Music tracks
    std::string musicFiles[] = {"minecraft.wav", "sweden.wav", "subwoofer_lullaby.wav"};
    for (const auto& mf : musicFiles) {
        std::vector<int16_t> mSamples;
        std::string paths[] = {
            "assets/music/" + mf,
            "x64/Debug/assets/music/" + mf,
            "x64/Release/assets/music/" + mf,
            "../assets/music/" + mf,
            "../../assets/music/" + mf,
            "d:/Mahesh/Coding files/PrismCraft/PrismCraft/assets/music/" + mf
        };
        bool loaded = false;
        for (const auto& p : paths) {
            if (loadWavFile(p, mSamples)) {
                std::cout << "[Audio] Loaded music track: " << mf << " (" << mSamples.size() << " samples) from " << p << std::endl;
                m_musicTracks.push_back({std::move(mSamples)});
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            std::cerr << "[Audio] WARNING: Failed to locate music track: " << mf << std::endl;
        }
    }
    std::cout << "[Audio] Total BGM tracks loaded: " << m_musicTracks.size() << std::endl;

    // If any essential banks are empty, fill with procedural fallbacks
    generateProceduralFallback();
}

void AudioEngine::generateProceduralFallback() {
    auto& clickBank = m_soundBanks[static_cast<size_t>(SoundEffect::Click)];
    if (clickBank.empty()) {
        std::vector<int16_t> samples(441, 0); // 10ms click
        for (size_t i = 0; i < samples.size(); ++i) {
            float t = static_cast<float>(i) / 44100.0f;
            float env = 1.0f - static_cast<float>(i) / samples.size();
            samples[i] = static_cast<int16_t>(std::sin(2.0f * 3.14159f * 1200.0f * t) * env * 16000.0f);
        }
        clickBank.push_back({std::move(samples)});
    }
}

void AudioEngine::playSound(SoundEffect effect, float volume) {
    if (!m_initialized) return;

    size_t idx = static_cast<size_t>(effect);
    if (idx >= static_cast<size_t>(SoundEffect::COUNT)) return;

    const auto& bank = m_soundBanks[idx];
    if (bank.empty()) return;

    // Pick random variation from the bank
    int varIdx = std::rand() % bank.size();
    const auto& sbuf = bank[varIdx];
    if (sbuf.samples.empty()) return;

    SoundCategory cat = SoundCategory::Player;
    switch (effect) {
        case SoundEffect::Click:
            cat = SoundCategory::UI;
            break;
        case SoundEffect::BlockBreak:
        case SoundEffect::BlockPlace:
        case SoundEffect::Footstep:
        case SoundEffect::GrassStep:
        case SoundEffect::StoneStep:
        case SoundEffect::WoodStep:
        case SoundEffect::SandStep:
        case SoundEffect::GrassDig:
        case SoundEffect::StoneDig:
        case SoundEffect::WoodDig:
        case SoundEffect::SandDig:
            cat = SoundCategory::Blocks;
            break;
        default:
            cat = SoundCategory::Player;
            break;
    }

    std::lock_guard<std::mutex> lock(s_audioMutex);
    for (int v = 0; v < MAX_VOICES; ++v) {
        if (!s_voices[v].active) {
            s_voices[v].samples = sbuf.samples.data();
            s_voices[v].length = sbuf.samples.size();
            s_voices[v].cursor = 0;
            s_voices[v].volume = volume;
            s_voices[v].category = cat;
            s_voices[v].active = true;
            break;
        }
    }
}

void AudioEngine::playBlockDig(int blockType, float volume) {
    // Determine dig sound based on block type
    // 1: Grass, 2: Dirt, 3: Stone, 4: Sand, 5: Wood, 6: Leaves, 7: Bedrock, 8: Planks, 9: Cobble
    switch (blockType) {
        case 1: // Grass
        case 6: // Leaves
            playSound(SoundEffect::GrassDig, volume);
            break;
        case 2: // Dirt
        case 4: // Sand
            playSound(SoundEffect::SandDig, volume);
            break;
        case 5: // Wood
        case 8: // Planks
        case 11: // Crafting Table
            playSound(SoundEffect::WoodDig, volume);
            break;
        default: // Stone, Cobble, Ores, Furnace
            playSound(SoundEffect::StoneDig, volume);
            break;
    }
}

void AudioEngine::playBlockStep(int blockType, float volume) {
    switch (blockType) {
        case 1: // Grass
        case 6: // Leaves
            playSound(SoundEffect::GrassStep, volume);
            break;
        case 2: // Dirt
        case 4: // Sand
            playSound(SoundEffect::SandStep, volume);
            break;
        case 5: // Wood
        case 8: // Planks
            playSound(SoundEffect::WoodStep, volume);
            break;
        default:
            playSound(SoundEffect::StoneStep, volume);
            break;
    }
}

void AudioEngine::update(float dt) {
    if (!m_initialized || m_musicTracks.empty()) return;

    if (!s_musicActive) {
        m_musicTimer -= dt;
        if (m_musicTimer <= 0.0f) {
            // Start next music track
            std::lock_guard<std::mutex> lock(s_audioMutex);
            const auto& track = m_musicTracks[m_currentMusicIndex];
            s_musicSamples = track.samples.data();
            s_musicLength = track.samples.size();
            s_musicCursor = 0;
            s_musicVolume = 1.0f;
            s_musicActive = true;

            std::cout << "[Audio] Playing BGM track " << m_currentMusicIndex << " (" << s_musicLength << " samples)" << std::endl;
            m_currentMusicIndex = (m_currentMusicIndex + 1) % m_musicTracks.size();
            m_musicTimer = 30.0f; // Gap between songs
        }
    }
}

} // namespace prismcraft
