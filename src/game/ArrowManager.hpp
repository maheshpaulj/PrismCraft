#pragma once
#include "rhi/Buffer.hpp"
#include "rhi/Pipeline.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace prismcraft {

class VulkanContext;
class CommandQueue;
class World;
class Player;

struct Arrow {
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 heading{0.0f, 0.0f, -1.0f};
    float life = 0.0f;
    bool inGround = false;
};

class ArrowManager {
public:
    ArrowManager(VulkanContext& context, CommandQueue& cmdQueue);

    void spawnArrow(const glm::vec3& pos, const glm::vec3& velocity);
    void update(float dt, const World& world, Player& player);

    void render(VkCommandBuffer cmd,
                const Pipeline& pipeline,
                const glm::mat4& vpMatrix);

    [[nodiscard]] const std::vector<Arrow>& getArrows() const { return m_arrows; }

private:
    VulkanContext& m_context;
    CommandQueue& m_cmdQueue;

    Buffer m_vbo[2], m_ibo[2];
    uint32_t m_indexCount[2]{0, 0};
    uint32_t m_frameIndex = 0;

    std::vector<Arrow> m_arrows;
};

} // namespace prismcraft
