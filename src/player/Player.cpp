#include "Player.hpp"
#include "world/World.hpp"
#include "world/Coordinates.hpp"
#include "core/Input.hpp"
#include "Raycast.hpp"
#include "audio/AudioEngine.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace prismcraft {

// Exact 2D SAT (Separating Axis Theorem) test between AABB [minX, maxX] x [minZ, maxZ]
// and an equilateral triangle with 3 vertices in XZ plane (v[i].x = X, v[i].y = Z)
static inline bool testAABBTriangleSAT(float minX, float maxX, float minZ, float maxZ, const glm::vec2 v[3]) {
    // Axis 1: X-axis
    float triMinX = std::min({v[0].x, v[1].x, v[2].x});
    float triMaxX = std::max({v[0].x, v[1].x, v[2].x});
    if (triMaxX <= minX || triMinX >= maxX) return false;

    // Axis 2: Z-axis (v[i].y is Z)
    float triMinZ = std::min({v[0].y, v[1].y, v[2].y});
    float triMaxZ = std::max({v[0].y, v[1].y, v[2].y});
    if (triMaxZ <= minZ || triMinZ >= maxZ) return false;

    glm::vec2 aabbCenter((minX + maxX) * 0.5f, (minZ + maxZ) * 0.5f);
    glm::vec2 aabbHalf((maxX - minX) * 0.5f, (maxZ - minZ) * 0.5f);

    // Axes 3, 4, 5: Perpendicular normals to the 3 edges of the triangle
    glm::vec2 edges[3] = {
        v[1] - v[0],
        v[2] - v[1],
        v[0] - v[2]
    };

    for (int i = 0; i < 3; ++i) {
        // Normal perpendicular to edge (dx, dz) is (-dz, dx)
        glm::vec2 axis(-edges[i].y, edges[i].x);

        // Project AABB onto axis
        float aabbCenterProj = axis.x * aabbCenter.x + axis.y * aabbCenter.y;
        float aabbRadius = std::abs(axis.x) * aabbHalf.x + std::abs(axis.y) * aabbHalf.y;
        float aabbMinProj = aabbCenterProj - aabbRadius;
        float aabbMaxProj = aabbCenterProj + aabbRadius;

        // Project triangle vertices onto axis
        float p0 = axis.x * v[0].x + axis.y * v[0].y;
        float p1 = axis.x * v[1].x + axis.y * v[1].y;
        float p2 = axis.x * v[2].x + axis.y * v[2].y;
        float triMinProj = std::min({p0, p1, p2});
        float triMaxProj = std::max({p0, p1, p2});

        if (triMaxProj <= aabbMinProj || triMinProj >= aabbMaxProj) {
            return false; // Separating axis exists -> no collision!
        }
    }

    return true; // Overlap on all 5 axes -> actual collision!
}

Player::Player(const glm::vec3& startPos)
    : m_position(startPos) {
    m_camera.setPosition(m_position + glm::vec3(0.0f, m_eyeHeight, 0.0f));
}

void Player::setPosition(const glm::vec3& pos) {
    m_position = pos;
    m_velocity = glm::vec3(0.0f);
    m_lastFallVelY = 0.0f;
    m_onGround = true;
    m_camera.setPosition(m_position + glm::vec3(0.0f, m_eyeHeight, 0.0f));
}

void Player::respawn(const glm::vec3& groundPos) {
    m_position = groundPos;
    m_velocity = glm::vec3(0.0f);
    m_lastFallVelY = 0.0f;
    m_health = 20.0f;
    m_oxygen = 10.0f;
    m_drownTimer = 0.0f;
    m_onGround = true;
    m_camera.setPosition(m_position + glm::vec3(0.0f, m_eyeHeight, 0.0f));
}

void Player::toggleFlying() {
    m_flying = !m_flying;
    m_velocity = glm::vec3(0.0f);
}

void Player::triggerSwing() {
    if (m_swingProgress <= 0.0f) {
        m_swingProgress = 0.01f;
    }
}

void Player::triggerPlace() {
    if (m_placeProgress <= 0.0f) {
        m_placeProgress = 0.01f;
    }
}

void Player::takeDamage(float dmg) {
    if (m_flying) return;
    m_health = std::max(0.0f, m_health - dmg);
    AudioEngine::get().playSound(SoundEffect::PlayerHurt, 0.9f);
}

void Player::setSelectedSlot(int slot) {
    if (slot >= 0 && slot < 10) {
        m_selectedSlot = slot;
    }
}

void Player::setHotbarBlock(int slot, BlockType type, int count) {
    if (slot >= 0 && slot < 10) {
        m_hotbar[slot] = type;
        m_hotbarCounts[slot] = (type == BlockType::Air) ? 0 : count;
    }
}

void Player::setStorageSlot(int slot, BlockType type, int count) {
    if (slot >= 0 && slot < 30) {
        m_storage[slot] = type;
        m_storageCounts[slot] = (type == BlockType::Air) ? 0 : count;
    }
}

bool Player::pickupItem(BlockType type, int count) {
    if (type == BlockType::Air || count <= 0) return false;
    int initialCount = count;

    // 1. Try to add to existing stacks in hotbar
    for (int i = 0; i < 10; ++i) {
        if (m_hotbar[i] == type && m_hotbarCounts[i] < 64) {
            int canTake = std::min(count, 64 - m_hotbarCounts[i]);
            m_hotbarCounts[i] += canTake;
            count -= canTake;
            if (count == 0) return true;
        }
    }

    // 2. Try to add to existing stacks in storage
    for (int i = 0; i < 30; ++i) {
        if (m_storage[i] == type && m_storageCounts[i] < 64) {
            int canTake = std::min(count, 64 - m_storageCounts[i]);
            m_storageCounts[i] += canTake;
            count -= canTake;
            if (count == 0) return true;
        }
    }

    // 3. Try to place in empty slot in hotbar
    for (int i = 0; i < 10; ++i) {
        if (m_hotbar[i] == BlockType::Air || m_hotbarCounts[i] == 0) {
            m_hotbar[i] = type;
            int canTake = std::min(count, 64);
            m_hotbarCounts[i] = canTake;
            count -= canTake;
            if (count == 0) return true;
        }
    }

    // 4. Try to place in empty slot in storage
    for (int i = 0; i < 30; ++i) {
        if (m_storage[i] == BlockType::Air || m_storageCounts[i] == 0) {
            m_storage[i] = type;
            int canTake = std::min(count, 64);
            m_storageCounts[i] = canTake;
            count -= canTake;
            if (count == 0) return true;
        }
    }

    return count < initialCount;
}

void Player::consumeSelectedItem() {
    if (m_selectedSlot >= 0 && m_selectedSlot < 10) {
        if (m_hotbarCounts[m_selectedSlot] > 0) {
            m_hotbarCounts[m_selectedSlot]--;
            if (m_hotbarCounts[m_selectedSlot] <= 0) {
                m_hotbar[m_selectedSlot] = BlockType::Air;
                m_hotbarCounts[m_selectedSlot] = 0;
            }
        }
    }
}

bool Player::hasItem(BlockType type, int count) const {
    if (type == BlockType::Air || count <= 0) return true;
    int total = 0;
    for (int i = 0; i < 10; ++i) {
        if (m_hotbar[i] == type) {
            total += m_hotbarCounts[i];
            if (total >= count) return true;
        }
    }
    for (int i = 0; i < 30; ++i) {
        if (m_storage[i] == type) {
            total += m_storageCounts[i];
            if (total >= count) return true;
        }
    }
    return total >= count;
}

bool Player::consumeItem(BlockType type, int count) {
    if (!hasItem(type, count)) return false;
    int remaining = count;

    // First consume from hotbar
    for (int i = 0; i < 10 && remaining > 0; ++i) {
        if (m_hotbar[i] == type) {
            int take = std::min(remaining, m_hotbarCounts[i]);
            m_hotbarCounts[i] -= take;
            remaining -= take;
            if (m_hotbarCounts[i] <= 0) {
                m_hotbar[i] = BlockType::Air;
                m_hotbarCounts[i] = 0;
            }
        }
    }

    // Then from storage
    for (int i = 0; i < 30 && remaining > 0; ++i) {
        if (m_storage[i] == type) {
            int take = std::min(remaining, m_storageCounts[i]);
            m_storageCounts[i] -= take;
            remaining -= take;
            if (m_storageCounts[i] <= 0) {
                m_storage[i] = BlockType::Air;
                m_storageCounts[i] = 0;
            }
        }
    }

    return true;
}

bool Player::checkCollision(const glm::vec3& pos, const World& world) const {
    float halfW = m_width * 0.5f;
    float minX = pos.x - halfW;
    float maxX = pos.x + halfW;
    float minY = pos.y;
    float currentHeight = m_crouching ? 1.5f : m_height;
    float maxY = pos.y + currentHeight;
    float minZ = pos.z - halfW;
    float maxZ = pos.z + halfW;

    int startX = static_cast<int>(std::floor(minX - 1.0f));
    int endX = static_cast<int>(std::ceil(maxX + 1.0f));
    int startY = static_cast<int>(std::floor(minY));
    int endY = static_cast<int>(std::floor(maxY));
    int startZ = static_cast<int>(std::floor(minZ / TRI_HEIGHT)) - 1;
    int endZ = static_cast<int>(std::ceil(maxZ / TRI_HEIGHT)) + 1;

    for (int y = startY; y <= endY; ++y) {
        float cellMinY = static_cast<float>(y);
        float cellMaxY = cellMinY + 1.0f;
        if (maxY <= cellMinY || minY >= cellMaxY) continue;

        for (int z = startZ; z <= endZ; ++z) {
            for (int x = startX; x <= endX; ++x) {
                for (int s = 0; s < 2; ++s) {
                    Cell cell = world.getCell(x, y, z, s);
                    if (cell.isSolid()) {
                        glm::vec2 v[3];
                        getPrismVerticesXZ(x, z, s, v);
                        if (testAABBTriangleSAT(minX, maxX, minZ, maxZ, v)) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void Player::moveAndCollide(glm::vec3 move, const World& world) {
    float stepHeight = 0.55f;

    // Sneak edge protection: when crouching on solid ground, prevent walking off cliffs
    if (m_crouching && m_onGround) {
        if (move.x != 0.0f) {
            if (!checkCollision(glm::vec3(m_position.x + move.x, m_position.y - 0.4f, m_position.z), world)) {
                move.x = 0.0f;
            }
        }
        if (move.z != 0.0f) {
            if (!checkCollision(glm::vec3(m_position.x, m_position.y - 0.4f, m_position.z + move.z), world)) {
                move.z = 0.0f;
            }
        }
        if (move.x != 0.0f && move.z != 0.0f) {
            if (!checkCollision(glm::vec3(m_position.x + move.x, m_position.y - 0.4f, m_position.z + move.z), world)) {
                move.x = 0.0f;
                move.z = 0.0f;
            }
        }
    }

    // X axis
    glm::vec3 newPosX = m_position;
    newPosX.x += move.x;
    if (!checkCollision(newPosX, world)) {
        m_position.x = newPosX.x;
    } else {
        // Step up over 1-block ledge / shore
        glm::vec3 stepUpX = newPosX + glm::vec3(0.0f, stepHeight, 0.0f);
        if (!checkCollision(stepUpX, world)) {
            m_position.x = stepUpX.x;
            m_position.y = stepUpX.y;
        } else {
            m_velocity.x = 0.0f;
        }
    }

    // Z axis
    glm::vec3 newPosZ = m_position;
    newPosZ.z += move.z;
    if (!checkCollision(newPosZ, world)) {
        m_position.z = newPosZ.z;
    } else {
        // Step up over 1-block ledge / shore
        glm::vec3 stepUpZ = newPosZ + glm::vec3(0.0f, stepHeight, 0.0f);
        if (!checkCollision(stepUpZ, world)) {
            m_position.z = stepUpZ.z;
            m_position.y = stepUpZ.y;
        } else {
            m_velocity.z = 0.0f;
        }
    }

    // Y axis
    glm::vec3 newPosY = m_position;
    newPosY.y += move.y;
    if (!checkCollision(newPosY, world)) {
        m_position.y = newPosY.y;
        m_onGround = false;
    } else {
        if (move.y < 0.0f) {
            if (!m_onGround && m_lastFallVelY < -13.0f && !m_flying) {
                float dmg = (std::abs(m_lastFallVelY) - 13.0f) * 1.5f;
                takeDamage(dmg);
            }
            m_onGround = true;
        }
        m_velocity.y = 0.0f;
    }
}

void Player::handleInput(float dt) {
    if (Input::isKeyPressed(GLFW_KEY_F)) {
        toggleFlying();
    }

    // 1-9 to select hotbar
    for (int i = 0; i < 9; ++i) {
        if (Input::isKeyPressed(GLFW_KEY_1 + i)) {
            setSelectedSlot(i);
        }
    }
    if (Input::isKeyPressed(GLFW_KEY_0)) {
        setSelectedSlot(9);
    }

    // Mouse look with custom sensitivity
    glm::vec2 mouseDelta = Input::getMouseDelta() * mouseSensitivity;
    m_camera.processMouseMovement(mouseDelta.x, mouseDelta.y);

    // FreeCam / Spectator mode: camera moves freely in 3D noclip, player stays in place
    if (m_camera.getMode() == CameraMode::FreeCam) {
        float freeCamSpeed = Input::isKeyDown(GLFW_KEY_LEFT_CONTROL) ? 32.0f : 12.0f;
        glm::vec3 move(0.0f);
        if (Input::isKeyDown(GLFW_KEY_W)) move += m_camera.getForward();
        if (Input::isKeyDown(GLFW_KEY_S)) move -= m_camera.getForward();
        if (Input::isKeyDown(GLFW_KEY_A)) move -= m_camera.getRight();
        if (Input::isKeyDown(GLFW_KEY_D)) move += m_camera.getRight();
        if (Input::isKeyDown(GLFW_KEY_SPACE)) move += glm::vec3(0.0f, 1.0f, 0.0f);
        if (Input::isKeyDown(GLFW_KEY_LEFT_SHIFT)) move -= glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::length(move) > 0.001f) {
            move = glm::normalize(move) * freeCamSpeed * dt;
            m_camera.moveFreeCam(move);
        }
        m_velocity = glm::vec3(0.0f);
        return;
    }

    glm::vec3 forward = m_camera.getForward();
    glm::vec3 right = m_camera.getRight();
    glm::vec3 horizForward = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));

    glm::vec3 inputDir{0.0f};
    if (Input::isKeyDown(GLFW_KEY_W)) inputDir += horizForward;
    if (Input::isKeyDown(GLFW_KEY_S)) inputDir -= horizForward;
    if (Input::isKeyDown(GLFW_KEY_A)) inputDir -= right;
    if (Input::isKeyDown(GLFW_KEY_D)) inputDir += right;

    if (glm::length(inputDir) > 0.001f) {
        inputDir = glm::normalize(inputDir);
    }

    // Crouching (Left Shift)
    m_crouching = Input::isKeyDown(GLFW_KEY_LEFT_SHIFT) && !m_flying && !m_inWater;
    bool sprinting = Input::isKeyDown(GLFW_KEY_LEFT_CONTROL) && !m_crouching;
    float currentSpeed = m_crouching ? 2.2f : (sprinting ? m_sprintSpeed : m_walkSpeed);

    if (m_flying) {
        m_velocity.x = inputDir.x * m_flySpeed;
        m_velocity.z = inputDir.z * m_flySpeed;

        if (Input::isKeyDown(GLFW_KEY_SPACE)) {
            m_velocity.y = m_flySpeed;
        } else if (Input::isKeyDown(GLFW_KEY_LEFT_SHIFT)) {
            m_velocity.y = -m_flySpeed;
        } else {
            m_velocity.y = 0.0f;
        }
    } else if (m_inWater) {
        // Swimming physics
        m_velocity.x = inputDir.x * (currentSpeed * 0.70f);
        m_velocity.z = inputDir.z * (currentSpeed * 0.70f);

        if (Input::isKeyDown(GLFW_KEY_SPACE)) {
            if (!m_underwater) {
                // At water surface: leap out onto bank
                m_velocity.y = 6.2f;
            } else {
                // Underwater: swim upwards smoothly
                m_velocity.y = 4.2f;
            }
        } else if (Input::isKeyDown(GLFW_KEY_LEFT_SHIFT)) {
            m_velocity.y = -3.5f; // Dive down
        } else {
            // Gentle buoyant slow sink
            m_velocity.y += (m_gravity * 0.12f) * dt;
            if (m_velocity.y < -2.6f) m_velocity.y = -2.6f;
        }
    } else {
        m_velocity.x = inputDir.x * currentSpeed;
        m_velocity.z = inputDir.z * currentSpeed;

        if (Input::isKeyDown(GLFW_KEY_SPACE) && m_onGround) {
            m_velocity.y = m_jumpVelocity;
            m_onGround = false;
        }

        m_velocity.y += m_gravity * dt;
        if (m_velocity.y < -40.0f) m_velocity.y = -40.0f;
    }

    m_lastFallVelY = m_velocity.y;
}

void Player::update(float dt, const World& world) {
    if (m_flying) {
        m_position += m_velocity * dt;
    } else {
        moveAndCollide(m_velocity * dt, world);
    }

    // Smooth crouch eye-height transition
    float targetEyeHeight = m_crouching ? 1.38f : 1.62f;
    m_eyeHeight += (targetEyeHeight - m_eyeHeight) * std::min(1.0f, dt * 14.0f);

    // Update swing animation (continuous punch while LMB is held)
    if (m_swingProgress > 0.0f) {
        m_swingProgress += dt * 4.8f;
        if (m_swingProgress >= 1.0f) {
            if (Input::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
                m_swingProgress = 0.01f; // Chain immediately into next swing
            } else {
                m_swingProgress = 0.0f;
            }
        }
    }

    // Update place animation
    if (m_placeProgress > 0.0f) {
        m_placeProgress += dt * 6.0f;
        if (m_placeProgress >= 1.0f) {
            m_placeProgress = 0.0f;
        }
    }

    // Footsteps & Bobbing
    float horizSpeed = glm::length(glm::vec2(m_velocity.x, m_velocity.z));
    if (m_onGround && horizSpeed > 0.5f) {
        m_bobTime += dt * horizSpeed * 1.8f;
        m_bobWeight = std::min(1.0f, m_bobWeight + dt * 5.0f);

        m_footstepTimer += dt * horizSpeed;
        if (m_footstepTimer > 2.2f) {
            CellCoord groundCell = worldToCell(m_position - glm::vec3(0.0f, 0.2f, 0.0f));
            Cell gBlock = world.getCell(groundCell.x, groundCell.y, groundCell.z, groundCell.s);
            AudioEngine::get().playBlockStep(static_cast<int>(gBlock.type), 0.6f);
            m_footstepTimer = 0.0f;
        }
    } else {
        m_bobWeight = std::max(0.0f, m_bobWeight - dt * 5.0f);
        m_footstepTimer = 0.0f;
    }

    float bobOffset = std::sin(m_bobTime * 2.0f) * 0.05f * m_bobWeight;
    glm::vec3 eyePos = m_position + glm::vec3(0.0f, m_eyeHeight + bobOffset, 0.0f);
    if (m_camera.getMode() != CameraMode::FreeCam) {
        m_camera.setPosition(eyePos);
    }

    // Calibrated Water & Underwater Detection (Query actual world block at exact positions)
    CellCoord feetCell = worldToCell(m_position + glm::vec3(0.0f, 0.15f, 0.0f));
    Cell feetBlock = world.getCell(feetCell.x, feetCell.y, feetCell.z, feetCell.s);

    CellCoord waistCell = worldToCell(m_position + glm::vec3(0.0f, 0.85f, 0.0f));
    Cell waistBlock = world.getCell(waistCell.x, waistCell.y, waistCell.z, waistCell.s);

    CellCoord headCell = worldToCell(eyePos);
    Cell headBlock = world.getCell(headCell.x, headCell.y, headCell.z, headCell.s);

    m_underwater = (headBlock.type == BlockType::Water);
    m_inWater = (feetBlock.type == BlockType::Water || waistBlock.type == BlockType::Water || m_underwater);

    if (m_underwater) {
        m_oxygen = std::max(0.0f, m_oxygen - dt * 0.65f);
        if (m_oxygen <= 0.0f) {
            m_drownTimer += dt;
            if (m_drownTimer >= 1.0f) {
                takeDamage(2.0f);
                m_drownTimer = 0.0f;
            }
        }
    } else {
        m_oxygen = std::min(10.0f, m_oxygen + dt * 3.5f);
        m_drownTimer = 0.0f;
    }
}

void Player::clearInventory() {
    m_hotbar.fill(BlockType::Air);
    m_hotbarCounts.fill(0);
    m_storage.fill(BlockType::Air);
    m_storageCounts.fill(0);
}

void Player::resetToStarterInventory() {
    clearInventory();
    m_hotbar[0] = BlockType::Wood;
    m_hotbarCounts[0] = 32;
    m_hotbar[1] = BlockType::Torch;
    m_hotbarCounts[1] = 8;
    m_hotbar[2] = BlockType::Glass;
    m_hotbarCounts[2] = 64;
    m_selectedSlot = 0;
}

} // namespace prismcraft
