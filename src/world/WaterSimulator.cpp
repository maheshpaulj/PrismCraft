#include "world/WaterSimulator.hpp"
#include "world/World.hpp"
#include <algorithm>
#include <cmath>

namespace prismcraft {

WaterSimulator::WaterSimulator() {
}

bool WaterSimulator::canWaterFlowInto(const Cell& cell) const {
    if (cell.type == BlockType::Air) return true;
    if (cell.isFoliage()) return true; // tall grass, flowers, sugar cane get washed away
    if (cell.isTorch()) return true;   // torches pop off
    return false;
}

bool WaterSimulator::checkInfiniteSource(World& world, int x, int y, int z, int s) const {
    // In Minecraft: an infinite water source forms when an Air block has
    // at least 2 horizontally adjacent source blocks (level == 0) AND
    // the block directly underneath is solid or a water source block.
    CellCoord neighbors[5];
    getNeighbors(x, y, z, s, neighbors);

    Cell cBelow = world.getCell(neighbors[1].x, neighbors[1].y, neighbors[1].z, neighbors[1].s);
    if (!cBelow.isSolid() && !(cBelow.type == BlockType::Water && cBelow.level == 0)) {
        return false;
    }

    int sourceCount = 0;
    for (int i = 2; i <= 4; ++i) {
        Cell c = world.getCell(neighbors[i].x, neighbors[i].y, neighbors[i].z, neighbors[i].s);
        if (c.type == BlockType::Water && c.level == 0) {
            sourceCount++;
        }
    }

    return sourceCount >= 2;
}

void WaterSimulator::scheduleUpdate(int worldX, int y, int worldZ, int s) {
    if (y <= 0 || y >= CHUNK_SIZE_Y - 1) return;
    CellCoord coord{worldX, y, worldZ, s};

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_inQueue.find(coord) == m_inQueue.end()) {
        m_inQueue.insert(coord);
        m_updateQueue.push_back(coord);
    }
}

void WaterSimulator::onBlockChanged(World& /*world*/, int worldX, int y, int worldZ, int s) {
    scheduleUpdate(worldX, y, worldZ, s);
    CellCoord neighbors[5];
    getNeighbors(worldX, y, worldZ, s, neighbors);
    for (int i = 0; i < 5; ++i) {
        scheduleUpdate(neighbors[i].x, neighbors[i].y, neighbors[i].z, neighbors[i].s);
    }
}

void WaterSimulator::update(float dt, World& world, const glm::vec3& playerPos) {
    m_tickTimer += dt;
    while (m_tickTimer >= TICK_INTERVAL) {
        m_tickTimer -= TICK_INTERVAL;
        stepSimulation(world, playerPos);
    }
}

void WaterSimulator::stepSimulation(World& world, const glm::vec3& playerPos) {
    std::vector<CellCoord> batch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = std::min(m_updateQueue.size(), MAX_UPDATES_PER_TICK);
        for (size_t i = 0; i < count; ++i) {
            batch.push_back(m_updateQueue.front());
            m_inQueue.erase(m_updateQueue.front());
            m_updateQueue.pop_front();
        }
    }

    if (batch.empty()) return;

    for (const auto& c : batch) {
        if (c.y <= 0 || c.y >= CHUNK_SIZE_Y - 1) continue;

        // Distance culling: only simulate water in active chunks near player (within ~140m)
        glm::vec3 center = cellToWorldCenter(c.x, c.y, c.z, c.s);
        float dx = center.x - playerPos.x;
        float dz = center.z - playerPos.z;
        if ((dx * dx + dz * dz) > (140.0f * 140.0f)) continue;

        Cell current = world.getCell(c.x, c.y, c.z, c.s);
        CellCoord neighbors[5];
        getNeighbors(c.x, c.y, c.z, c.s, neighbors);

        if (current.type == BlockType::Water) {
            if (current.level == 0) {
                // -------------------------------------------------------------
                // SOURCE BLOCK: Permanent, never decays
                // -------------------------------------------------------------
                Cell below = world.getCell(neighbors[1].x, neighbors[1].y, neighbors[1].z, neighbors[1].s);
                if (canWaterFlowInto(below)) {
                    // Downward waterfall flow (falling water has level 1)
                    world.setCell(neighbors[1].x, neighbors[1].y, neighbors[1].z, neighbors[1].s, Cell{BlockType::Water, 1}, false);
                    scheduleUpdate(neighbors[1].x, neighbors[1].y, neighbors[1].z, neighbors[1].s);
                } else {
                    // Floor is solid/water, spread horizontally into open space
                    for (int i = 2; i <= 4; ++i) {
                        CellCoord nb = neighbors[i];
                        Cell cNb = world.getCell(nb.x, nb.y, nb.z, nb.s);
                        if (canWaterFlowInto(cNb)) {
                            if (checkInfiniteSource(world, nb.x, nb.y, nb.z, nb.s)) {
                                world.setCell(nb.x, nb.y, nb.z, nb.s, Cell{BlockType::Water, 0}, false);
                            } else {
                                world.setCell(nb.x, nb.y, nb.z, nb.s, Cell{BlockType::Water, 1}, false);
                            }
                            scheduleUpdate(nb.x, nb.y, nb.z, nb.s);
                        } else if (cNb.type == BlockType::Water && cNb.level > 1) {
                            world.setCell(nb.x, nb.y, nb.z, nb.s, Cell{BlockType::Water, 1}, false);
                            scheduleUpdate(nb.x, nb.y, nb.z, nb.s);
                        }
                    }
                }
            } else {
                // -------------------------------------------------------------
                // FLOWING WATER: Check if still fed by an upstream source
                // -------------------------------------------------------------
                Cell cAbove = world.getCell(neighbors[0].x, neighbors[0].y, neighbors[0].z, neighbors[0].s);
                bool fedFromAbove = (cAbove.type == BlockType::Water);

                uint8_t minParentLevel = 255;
                for (int i = 2; i <= 4; ++i) {
                    Cell cNb = world.getCell(neighbors[i].x, neighbors[i].y, neighbors[i].z, neighbors[i].s);
                    if (cNb.type == BlockType::Water && cNb.level < current.level) {
                        minParentLevel = std::min(minParentLevel, cNb.level);
                    }
                }

                if (!fedFromAbove && minParentLevel == 255) {
                    // Upstream source was removed! Dry up!
                    world.setCell(c.x, c.y, c.z, c.s, Cell{BlockType::Air, 0}, false);
                    for (int i = 0; i < 5; ++i) {
                        scheduleUpdate(neighbors[i].x, neighbors[i].y, neighbors[i].z, neighbors[i].s);
                    }
                    continue;
                }

                uint8_t targetLevel = fedFromAbove ? 1 : (minParentLevel + 1);
                if (targetLevel > MAX_FLOW_DISTANCE) {
                    world.setCell(c.x, c.y, c.z, c.s, Cell{BlockType::Air, 0}, false);
                    for (int i = 0; i < 5; ++i) {
                        scheduleUpdate(neighbors[i].x, neighbors[i].y, neighbors[i].z, neighbors[i].s);
                    }
                    continue;
                }

                if (targetLevel != current.level) {
                    world.setCell(c.x, c.y, c.z, c.s, Cell{BlockType::Water, targetLevel}, false);
                    current.level = targetLevel;
                }

                // Propagate downwards
                Cell below = world.getCell(neighbors[1].x, neighbors[1].y, neighbors[1].z, neighbors[1].s);
                if (canWaterFlowInto(below)) {
                    world.setCell(neighbors[1].x, neighbors[1].y, neighbors[1].z, neighbors[1].s, Cell{BlockType::Water, 1}, false);
                    scheduleUpdate(neighbors[1].x, neighbors[1].y, neighbors[1].z, neighbors[1].s);
                } else if (current.level < MAX_FLOW_DISTANCE) {
                    // Propagate horizontally
                    uint8_t spreadLevel = current.level + 1;
                    for (int i = 2; i <= 4; ++i) {
                        CellCoord nb = neighbors[i];
                        Cell cNb = world.getCell(nb.x, nb.y, nb.z, nb.s);
                        if (canWaterFlowInto(cNb)) {
                            if (checkInfiniteSource(world, nb.x, nb.y, nb.z, nb.s)) {
                                world.setCell(nb.x, nb.y, nb.z, nb.s, Cell{BlockType::Water, 0}, false);
                            } else {
                                world.setCell(nb.x, nb.y, nb.z, nb.s, Cell{BlockType::Water, spreadLevel}, false);
                            }
                            scheduleUpdate(nb.x, nb.y, nb.z, nb.s);
                        } else if (cNb.type == BlockType::Water && cNb.level > spreadLevel) {
                            world.setCell(nb.x, nb.y, nb.z, nb.s, Cell{BlockType::Water, spreadLevel}, false);
                            scheduleUpdate(nb.x, nb.y, nb.z, nb.s);
                        }
                    }
                }
            }
        } else if (canWaterFlowInto(current)) {
            // -------------------------------------------------------------
            // AIR / FOLIAGE: Check if adjacent water should flow into it
            // -------------------------------------------------------------
            if (checkInfiniteSource(world, c.x, c.y, c.z, c.s)) {
                world.setCell(c.x, c.y, c.z, c.s, Cell{BlockType::Water, 0}, false);
                for (int i = 0; i < 5; ++i) {
                    scheduleUpdate(neighbors[i].x, neighbors[i].y, neighbors[i].z, neighbors[i].s);
                }
            } else {
                Cell cAbove = world.getCell(neighbors[0].x, neighbors[0].y, neighbors[0].z, neighbors[0].s);
                if (cAbove.type == BlockType::Water) {
                    world.setCell(c.x, c.y, c.z, c.s, Cell{BlockType::Water, 1}, false);
                    for (int i = 0; i < 5; ++i) {
                        scheduleUpdate(neighbors[i].x, neighbors[i].y, neighbors[i].z, neighbors[i].s);
                    }
                } else {
                    uint8_t minLevel = 255;
                    for (int i = 2; i <= 4; ++i) {
                        Cell cNb = world.getCell(neighbors[i].x, neighbors[i].y, neighbors[i].z, neighbors[i].s);
                        if (cNb.type == BlockType::Water && cNb.level < MAX_FLOW_DISTANCE) {
                            CellCoord nbBelowCoord = neighbors[i];
                            nbBelowCoord.y--;
                            Cell nbBelow = world.getCell(nbBelowCoord.x, nbBelowCoord.y, nbBelowCoord.z, nbBelowCoord.s);
                            if (nbBelow.isSolid() || nbBelow.type == BlockType::Water) {
                                minLevel = std::min(minLevel, cNb.level);
                            }
                        }
                    }
                    if (minLevel != 255 && (minLevel + 1) <= MAX_FLOW_DISTANCE) {
                        world.setCell(c.x, c.y, c.z, c.s, Cell{BlockType::Water, static_cast<uint8_t>(minLevel + 1)}, false);
                        for (int i = 0; i < 5; ++i) {
                            scheduleUpdate(neighbors[i].x, neighbors[i].y, neighbors[i].z, neighbors[i].s);
                        }
                    }
                }
            }
        }
    }
}

} // namespace prismcraft
