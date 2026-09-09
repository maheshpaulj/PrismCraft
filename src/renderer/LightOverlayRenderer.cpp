#include "LightOverlayRenderer.hpp"
#include "world/World.hpp"
#include "world/Coordinates.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "ui/FontData.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstring>
#include <string>

namespace prismcraft {

LightOverlayRenderer::LightOverlayRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
}

void LightOverlayRenderer::rebuildMesh(const World& world, const glm::vec3& playerPos, const glm::vec3& camPos) {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    CellCoord playerCell = worldToCell(playerPos);
    int rad = 10;
    float charW = 0.18f;
    float charH = 0.22f;

    auto addGlyphQuad3D = [&](const glm::vec3& center, glm::vec2 uv, const glm::vec3& col, const glm::vec3& right, const glm::vec3& up) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 norm = glm::normalize(glm::cross(right, up));
        glm::vec3 p0 = center - right * (charW * 0.5f) - up * (charH * 0.5f);
        glm::vec3 p1 = center + right * (charW * 0.5f) - up * (charH * 0.5f);
        glm::vec3 p2 = center + right * (charW * 0.5f) + up * (charH * 0.5f);
        glm::vec3 p3 = center - right * (charW * 0.5f) + up * (charH * 0.5f);

        glm::vec4 tileUV = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);
        (void)uv;
        vertices.push_back({p0, glm::vec2(tileUV.x, tileUV.w), norm, col});
        vertices.push_back({p1, glm::vec2(tileUV.z, tileUV.w), norm, col});
        vertices.push_back({p2, glm::vec2(tileUV.z, tileUV.y), norm, col});
        vertices.push_back({p3, glm::vec2(tileUV.x, tileUV.y), norm, col});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    for (int dx = -rad; dx <= rad; ++dx) {
        int wx = playerCell.x + dx;
        for (int dz = -rad; dz <= rad; ++dz) {
            int wz = playerCell.z + dz;
            for (int dy = -5; dy <= 5; ++dy) {
                int y = playerCell.y + dy;
                if (y < 1 || y >= CHUNK_SIZE_Y - 2) continue;

                for (int s = 0; s < 2; ++s) {
                    Cell ground = world.getCell(wx, y, wz, s);
                    if (!ground.isSolid()) continue;

                    Cell above = world.getCell(wx, y + 1, wz, s);
                    if (above.isSolid() || above.type == BlockType::Water) continue;

                    int skyLight = 15, blockLight = 0;
                    world.getLightLevels(wx, y + 1, wz, s, skyLight, blockLight);
                    int totalLight = std::max(skyLight, blockLight);

                    glm::vec3 blockCenter = cellToWorldCenter(wx, y, wz, s);
                    blockCenter.y = static_cast<float>(y + 1) + 0.006f; // Lie flat on block top surface

                    // Calculate look direction so digits orient right-side-up toward player
                    glm::vec3 camDir = camPos - blockCenter;
                    camDir.y = 0.0f;
                    float len = glm::length(camDir);
                    if (len < 0.001f) camDir = glm::vec3(0.0f, 0.0f, 1.0f);
                    else camDir /= len;

                    glm::vec3 norm(0.0f, 1.0f, 0.0f);
                    glm::vec3 textUp = -camDir;
                    glm::vec3 textRight = glm::normalize(glm::cross(textUp, norm));

                    // Mob spawn danger: <= 7 is Red, >= 8 is Green
                    glm::vec3 col = (totalLight <= 7) ? glm::vec3(1.0f, 0.20f, 0.20f) : glm::vec3(0.20f, 1.0f, 0.35f);

                    std::string str = std::to_string(totalLight);
                    float totalW = static_cast<float>(str.size()) * (charW + 0.04f);
                    glm::vec3 start = blockCenter - textRight * (totalW * 0.5f - charW * 0.5f);

                    for (size_t k = 0; k < str.size(); ++k) {
                        char c = str[k];
                        glm::vec3 cPos = start + textRight * (static_cast<float>(k) * (charW + 0.04f));
                        glm::vec4 glyphUV = TextureAtlas::getTileUV(TextureAtlas::getFontTile(c));

                        uint32_t b = static_cast<uint32_t>(vertices.size());
                        glm::vec3 p0 = cPos - textRight * (charW * 0.5f) - textUp * (charH * 0.5f);
                        glm::vec3 p1 = cPos + textRight * (charW * 0.5f) - textUp * (charH * 0.5f);
                        glm::vec3 p2 = cPos + textRight * (charW * 0.5f) + textUp * (charH * 0.5f);
                        glm::vec3 p3 = cPos - textRight * (charW * 0.5f) + textUp * (charH * 0.5f);

                        // Flat horizontal glyph quad on block surface
                        vertices.push_back({p0, glm::vec2(glyphUV.x, glyphUV.w), norm, col});
                        vertices.push_back({p1, glm::vec2(glyphUV.z, glyphUV.w), norm, col});
                        vertices.push_back({p2, glm::vec2(glyphUV.z, glyphUV.y), norm, col});
                        vertices.push_back({p3, glm::vec2(glyphUV.x, glyphUV.y), norm, col});

                        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
                        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
                    }
                }
            }
        }
    }

    m_indexCount[m_frameIndex] = static_cast<uint32_t>(indices.size());
    if (indices.empty()) return;

    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_vbo[m_frameIndex] = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_vbo[m_frameIndex].uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_ibo[m_frameIndex] = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ibo[m_frameIndex].uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
}

void LightOverlayRenderer::render(VkCommandBuffer cmd,
                                  const Pipeline& pipeline,
                                  const World& world,
                                  const glm::vec3& playerPos,
                                  const glm::vec3& camPos,
                                  const glm::mat4& vpMatrix) {
    if (glm::distance(playerPos, m_lastPlayerPos) > 0.8f) {
        rebuildMesh(world, playerPos, camPos);
        m_lastPlayerPos = playerPos;
    }

    if (m_indexCount[m_frameIndex] == 0 || !m_vbo[m_frameIndex].isValid()) return;

    pipeline.bind(cmd);

    PushConstants pc{};
    std::memcpy(pc.mvp, &vpMatrix[0][0], sizeof(float) * 16);
    pc.sunDir[3] = 2.0f; // Self-illuminated bright digits
    pc.lightColor[0] = 1.0f; pc.lightColor[1] = 1.0f; pc.lightColor[2] = 1.0f;
    pc.skyFog[3] = 1000.0f;

    vkCmdPushConstants(cmd, pipeline.getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &pc);

    VkBuffer vbs[] = {m_vbo[m_frameIndex].getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo[m_frameIndex].getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_indexCount[m_frameIndex], 1, 0, 0, 0);

    m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace prismcraft
