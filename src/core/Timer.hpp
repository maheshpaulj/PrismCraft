#pragma once
#include <chrono>

namespace prismcraft {

class Timer {
public:
    Timer() : m_startTime(Clock::now()), m_lastTime(m_startTime), m_deltaTime(0.0f) {}

    // Call once per frame. Returns delta time in seconds.
    float tick() {
        auto now = Clock::now();
        m_deltaTime = std::chrono::duration<float>(now - m_lastTime).count();
        m_lastTime = now;
        // Clamp to prevent spiral of death after breakpoints/pauses
        if (m_deltaTime > 0.25f) m_deltaTime = 0.25f;
        return m_deltaTime;
    }

    [[nodiscard]] float getDeltaTime() const { return m_deltaTime; }
    [[nodiscard]] float getElapsedTime() const {
        return std::chrono::duration<float>(Clock::now() - m_startTime).count();
    }
    [[nodiscard]] float getFPS() const { return m_deltaTime > 0.0f ? 1.0f / m_deltaTime : 0.0f; }

private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point m_startTime;
    Clock::time_point m_lastTime;
    float m_deltaTime;
};

} // namespace prismcraft
