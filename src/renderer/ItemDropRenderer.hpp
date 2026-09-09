#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include "world/Cell.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <optional>
#include <utility>

namespace prismcraft {

class VulkanContext;
class CommandQueue;
class World;
class Player;

struct ItemDrop {
    glm::vec3 pos;
    glm::vec3 vel{0.0f, 2.5f, 0.0f};
    BlockType type;
    float age = 0.0f;
    float rotAngle = 0.0f;
    bool collected = false;
    float groundY = 0.0f;
    bool inSunlight = true;
};

class ItemDropManager {
public:
    ItemDropManager(VulkanContext& context, CommandQueue& cmdQueue);

    void spawnDrop(const glm::vec3& pos, BlockType type, const glm::vec3& vel = glm::vec3(0.0f, 3.2f, 0.0f));
    void update(float dt, const World& world, Player& player);

    // Returns (worldPos, intensity) of the nearest active dropped torch
    [[nodiscard]] std::optional<std::pair<glm::vec3, float>> getNearestTorchDrop(const glm::vec3& refPos, float maxDist = 22.0f) const;

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                const glm::mat4& vpMatrix,
                const PushConstants& scenePC);

private:
    

    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo[2], m_ibo[2];
    uint32_t m_indexCount[2]{0, 0};

    std::vector<ItemDrop> m_drops;
};

} // namespace prismcraft
