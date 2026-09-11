#pragma once
#include "world/Coordinates.hpp"
#include "world/Cell.hpp"
#include <deque>
#include <unordered_set>
#include <mutex>
#include <glm/vec3.hpp>

namespace prismcraft {

class World;

class WaterSimulator {
public:
    WaterSimulator();
    ~WaterSimulator() = default;

    // Called once per frame from World::update
    void update(float dt, World& world, const glm::vec3& playerPos);

    // Called when a block is placed, broken, or replaced
    void onBlockChanged(World& world, int worldX, int y, int worldZ, int s);

    // Explicitly schedule a cell for fluid update
    void scheduleUpdate(int worldX, int y, int worldZ, int s);

    // Configuration constants
    static constexpr uint8_t MAX_FLOW_DISTANCE = 7;
    static constexpr float TICK_INTERVAL = 0.12f; // ~8 ticks per sec for natural fluid flow
    static constexpr size_t MAX_UPDATES_PER_TICK = 256;

private:
    void stepSimulation(World& world, const glm::vec3& playerPos);
    [[nodiscard]] bool canWaterFlowInto(const Cell& cell) const;
    [[nodiscard]] bool checkInfiniteSource(World& world, int x, int y, int z, int s) const;

    float m_tickTimer = 0.0f;
    std::mutex m_mutex;
    std::deque<CellCoord> m_updateQueue;
    std::unordered_set<CellCoord, CellCoordHash> m_inQueue;
};

} // namespace prismcraft
