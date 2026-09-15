#pragma once
#include "Camera.hpp"
#include "world/Cell.hpp"
#include <glm/glm.hpp>
#include <array>

namespace prismcraft {

class World;

class Player {
public:
    Player(const glm::vec3& startPos = glm::vec3(0.0f, 82.0f, 0.0f));

    void update(float dt, const World& world);
    void handleInput(float dt);

    // Getters
    [[nodiscard]] Camera& getCamera() { return m_camera; }
    [[nodiscard]] const Camera& getCamera() const { return m_camera; }
    [[nodiscard]] const glm::vec3& getPosition() const { return m_position; }
    [[nodiscard]] const glm::vec3& getVelocity() const { return m_velocity; }
    [[nodiscard]] bool isOnGround() const { return m_onGround; }
    [[nodiscard]] bool isFlying() const { return m_flying; }
    [[nodiscard]] bool isCreative() const { return m_creative; }
    void setCreative(bool c);

    // Health (0.0 to 20.0, 20 = 10 full hearts)
    [[nodiscard]] float getHealth() const { return m_health; }
    void setHealth(float h) { m_health = h; }
    void takeDamage(float dmg);

    // Hunger & Armor
    [[nodiscard]] float getHunger() const { return m_hunger; }
    void setHunger(float h) { m_hunger = h; }
    [[nodiscard]] float getArmor() const { return m_armor; }
    void setArmor(float a) { m_armor = a; }

    // Hand & Bobbing & Animations
    [[nodiscard]] float getBobTime() const { return m_bobTime; }
    [[nodiscard]] float getBobWeight() const { return m_bobWeight; }
    [[nodiscard]] float getSwingProgress() const { return m_swingProgress; }
    [[nodiscard]] float getPlaceProgress() const { return m_placeProgress; }
    void triggerSwing();
    void triggerPlace();

    // Oxygen & Drowning & Water
    [[nodiscard]] float getOxygen() const { return m_oxygen; }
    [[nodiscard]] bool isUnderwater() const { return m_underwater; }
    [[nodiscard]] bool isInWater() const { return m_inWater; }
    [[nodiscard]] bool isCrouching() const { return m_crouching; }

    // Hotbar selection (10 slots matching alternating equilateral UI)
    [[nodiscard]] int getSelectedSlot() const { return m_selectedSlot; }
    void setSelectedSlot(int slot);
    [[nodiscard]] BlockType getSelectedBlock() const { return m_hotbar[m_selectedSlot]; }
    void setHotbarBlock(int slot, BlockType type, int count = 1);
    [[nodiscard]] BlockType getHotbarBlock(int slot) const { return (slot >= 0 && slot < 10) ? m_hotbar[slot] : BlockType::Air; }
    [[nodiscard]] int getHotbarCount(int slot) const { return (slot >= 0 && slot < 10) ? m_hotbarCounts[slot] : 0; }
    [[nodiscard]] const std::array<BlockType, 10>& getHotbar() const { return m_hotbar; }
    [[nodiscard]] const std::array<int, 10>& getHotbarCounts() const { return m_hotbarCounts; }

    // Storage inventory (30 slots matching 3 rows of alternating equilateral triangles)
    [[nodiscard]] BlockType getStorageBlock(int slot) const { return (slot >= 0 && slot < 30) ? m_storage[slot] : BlockType::Air; }
    [[nodiscard]] int getStorageCount(int slot) const { return (slot >= 0 && slot < 30) ? m_storageCounts[slot] : 0; }
    void setStorageSlot(int slot, BlockType type, int count);
    [[nodiscard]] const std::array<BlockType, 30>& getStorage() const { return m_storage; }
    [[nodiscard]] const std::array<int, 30>& getStorageCounts() const { return m_storageCounts; }

    bool pickupItem(BlockType type, int count = 1);
    void consumeSelectedItem();
    [[nodiscard]] bool hasItem(BlockType type, int count = 1) const;
    bool consumeItem(BlockType type, int count = 1);

    // Bow mechanics
    [[nodiscard]] float getBowCharge() const { return m_bowCharge; }
    void setBowCharge(float c) { m_bowCharge = c; }
    [[nodiscard]] bool isDrawingBow() const { return m_drawingBow; }
    void setDrawingBow(bool d) { m_drawingBow = d; }

    // Sword blocking stance
    [[nodiscard]] bool isBlocking() const { return m_blocking; }
    void setBlocking(bool b) { m_blocking = b; }

    void setPosition(const glm::vec3& pos);
    void respawn(const glm::vec3& groundPos);
    [[nodiscard]] bool hasSpawnPoint() const { return m_hasSpawnPoint; }
    [[nodiscard]] const glm::vec3& getSpawnPoint() const { return m_spawnPoint; }
    void setSpawnPoint(const glm::vec3& p) { m_spawnPoint = p; m_hasSpawnPoint = true; }
    void clearSpawnPoint() { m_hasSpawnPoint = false; }
    void toggleFlying();
    void setFlying(bool f) { m_flying = f; }
    [[nodiscard]] float getEyeHeight() const { return m_eyeHeight; }
    void clearInventory();
    void resetToStarterInventory();

    float mouseSensitivity = 1.0f;

private:
    bool checkCollision(const glm::vec3& pos, const World& world) const;
    void moveAndCollide(glm::vec3 move, const World& world);

    Camera m_camera;
    glm::vec3 m_position;
    glm::vec3 m_velocity{0.0f};
    bool m_hasSpawnPoint = false;
    glm::vec3 m_spawnPoint{0.0f};

    bool m_onGround = false;
    bool m_flying = false;
    bool m_creative = false;
    float m_health = 20.0f;
    float m_hunger = 20.0f;
    float m_armor = 0.0f;
    float m_oxygen = 10.0f;
    float m_drownTimer = 0.0f;
    bool m_underwater = false;
    bool m_inWater = false;
    bool m_crouching = false;
    float m_lastFallVelY = 0.0f;
    float m_footstepTimer = 0.0f;

    // Dimensions
    float m_width = 0.5f;
    float m_height = 1.8f;
    float m_eyeHeight = 1.62f;

    // Movement parameters
    float m_walkSpeed = 5.0f;
    float m_sprintSpeed = 8.5f;
    float m_flySpeed = 16.0f;
    float m_jumpVelocity = 8.2f;
    float m_gravity = -24.0f;

    // View bobbing & hand animation
    float m_bobTime = 0.0f;
    float m_bobWeight = 0.0f;
    float m_swingProgress = 0.0f;
    float m_placeProgress = 0.0f;
    float m_bowCharge = 0.0f;
    bool m_drawingBow = false;
    bool m_blocking = false;

    // Hotbar inventory (10 slots: 32x Wood, 8x Torch, 64x Glass, 4x DoorWood, 2x Bed)
    std::array<BlockType, 10> m_hotbar{
        BlockType::Wood,
        BlockType::Torch,
        BlockType::Glass,
        BlockType::DoorWood,
        BlockType::Bed,
        BlockType::Air,
        BlockType::Air,
        BlockType::Air,
        BlockType::Air,
        BlockType::Air
    };
    std::array<int, 10> m_hotbarCounts{32, 8, 64, 4, 2, 0, 0, 0, 0, 0};
    int m_selectedSlot = 0;

    // Storage inventory (30 slots: empty by default for new world)
    std::array<BlockType, 30> m_storage{
        BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air,
        BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air,
        BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air,
        BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air,
        BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air,
        BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air, BlockType::Air
    };
    std::array<int, 30> m_storageCounts{
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0
    };
};

} // namespace prismcraft
