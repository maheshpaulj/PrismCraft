#include "CelestialRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/ChunkMesher.hpp"
#include "TextureAtlas.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstring>
#include <cmath>

namespace prismcraft {

CelestialRenderer::CelestialRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
    buildSun();
    buildMoon();
}

void CelestialRenderer::buildSun() {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE); // Solid white tile
    // Sun colors matching Complementary Reimagined aesthetic:
    // Outer warm golden amber border, Inner luminous warm core
    glm::vec3 cOuter(0.98f, 0.82f, 0.40f);
    glm::vec3 cInner(1.00f, 0.96f, 0.85f);
    glm::vec3 cWall(0.88f, 0.72f, 0.32f);

    float H = 0.8660254f; // sqrt(3)/2
    float zFront = 0.15f;
    float zBack  = -0.15f;

    // Centroid at (0, 0)
    glm::vec2 o0(0.0f, (2.0f / 3.0f) * H);
    glm::vec2 o1(-0.5f, -(1.0f / 3.0f) * H);
    glm::vec2 o2(0.5f, -(1.0f / 3.0f) * H);

    float innerScale = 0.72f;
    glm::vec2 i0 = o0 * innerScale;
    glm::vec2 i1 = o1 * innerScale;
    glm::vec2 i2 = o2 * innerScale;

    auto addFrontTri = [&](const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec3& c) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 n(0.0f, 0.0f, 1.0f);
        vertices.push_back({glm::vec3(p0, zFront), glm::vec2(uv.x, uv.y), n, c});
        vertices.push_back({glm::vec3(p1, zFront), glm::vec2(uv.z, uv.y), n, c});
        vertices.push_back({glm::vec3(p2, zFront), glm::vec2(uv.x, uv.w), n, c});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    auto addBackTri = [&](const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec3& c) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 n(0.0f, 0.0f, -1.0f);
        vertices.push_back({glm::vec3(p0, zBack), glm::vec2(uv.x, uv.y), n, c});
        vertices.push_back({glm::vec3(p2, zBack), glm::vec2(uv.x, uv.w), n, c});
        vertices.push_back({glm::vec3(p1, zBack), glm::vec2(uv.z, uv.y), n, c});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    auto addEdgeQuad = [&](const glm::vec2& p0, const glm::vec2& p1, const glm::vec3& c) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec2 d = p1 - p0;
        glm::vec3 n = glm::normalize(glm::vec3(d.y, -d.x, 0.0f));
        vertices.push_back({glm::vec3(p0, zFront), glm::vec2(uv.x, uv.w), n, c});
        vertices.push_back({glm::vec3(p1, zFront), glm::vec2(uv.z, uv.w), n, c});
        vertices.push_back({glm::vec3(p1, zBack),  glm::vec2(uv.z, uv.y), n, c});
        vertices.push_back({glm::vec3(p0, zBack),  glm::vec2(uv.x, uv.y), n, c});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    // Front: 3 Outer Bevel Trapezoids + Inner Core Triangle
    addFrontTri(o0, o1, i1, cOuter);
    addFrontTri(o0, i1, i0, cOuter);
    addFrontTri(o1, o2, i2, cOuter);
    addFrontTri(o1, i2, i1, cOuter);
    addFrontTri(o2, o0, i0, cOuter);
    addFrontTri(o2, i0, i2, cOuter);
    addFrontTri(i0, i1, i2, cInner);

    // Back: 3 Outer Bevel Trapezoids + Inner Core Triangle
    addBackTri(o0, o1, i1, cOuter);
    addBackTri(o0, i1, i0, cOuter);
    addBackTri(o1, o2, i2, cOuter);
    addBackTri(o1, i2, i1, cOuter);
    addBackTri(o2, o0, i0, cOuter);
    addBackTri(o2, i0, i2, cOuter);
    addBackTri(i0, i1, i2, cInner);

    // 3 Side Walls
    addEdgeQuad(o0, o1, cWall);
    addEdgeQuad(o1, o2, cWall);
    addEdgeQuad(o2, o0, cWall);

    m_sunIndexCount = static_cast<uint32_t>(indices.size());
    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_sunVbo = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_sunVbo.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_sunIbo = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_sunIbo.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
}

void CelestialRenderer::buildMoon() {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::TILE_WHITE);
    glm::vec3 cOuter(0.70f, 0.85f, 0.95f); // Silver-cyan outer border
    glm::vec3 cInner(0.95f, 0.98f, 1.00f); // Ice-white glowing core
    glm::vec3 cWall(0.55f, 0.68f, 0.78f);

    float H = 0.8660254f;
    float zFront = 0.15f;
    float zBack  = -0.15f;

    glm::vec2 o0(0.0f, (2.0f / 3.0f) * H);
    glm::vec2 o1(-0.5f, -(1.0f / 3.0f) * H);
    glm::vec2 o2(0.5f, -(1.0f / 3.0f) * H);

    float innerScale = 0.72f;
    glm::vec2 i0 = o0 * innerScale;
    glm::vec2 i1 = o1 * innerScale;
    glm::vec2 i2 = o2 * innerScale;

    auto addFrontTri = [&](const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec3& c) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 n(0.0f, 0.0f, 1.0f);
        vertices.push_back({glm::vec3(p0, zFront), glm::vec2(uv.x, uv.y), n, c});
        vertices.push_back({glm::vec3(p1, zFront), glm::vec2(uv.z, uv.y), n, c});
        vertices.push_back({glm::vec3(p2, zFront), glm::vec2(uv.x, uv.w), n, c});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    auto addBackTri = [&](const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec3& c) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec3 n(0.0f, 0.0f, -1.0f);
        vertices.push_back({glm::vec3(p0, zBack), glm::vec2(uv.x, uv.y), n, c});
        vertices.push_back({glm::vec3(p2, zBack), glm::vec2(uv.x, uv.w), n, c});
        vertices.push_back({glm::vec3(p1, zBack), glm::vec2(uv.z, uv.y), n, c});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    auto addEdgeQuad = [&](const glm::vec2& p0, const glm::vec2& p1, const glm::vec3& c) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec2 d = p1 - p0;
        glm::vec3 n = glm::normalize(glm::vec3(d.y, -d.x, 0.0f));
        vertices.push_back({glm::vec3(p0, zFront), glm::vec2(uv.x, uv.w), n, c});
        vertices.push_back({glm::vec3(p1, zFront), glm::vec2(uv.z, uv.w), n, c});
        vertices.push_back({glm::vec3(p1, zBack),  glm::vec2(uv.z, uv.y), n, c});
        vertices.push_back({glm::vec3(p0, zBack),  glm::vec2(uv.x, uv.y), n, c});
        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    addFrontTri(o0, o1, i1, cOuter);
    addFrontTri(o0, i1, i0, cOuter);
    addFrontTri(o1, o2, i2, cOuter);
    addFrontTri(o1, i2, i1, cOuter);
    addFrontTri(o2, o0, i0, cOuter);
    addFrontTri(o2, i0, i2, cOuter);
    addFrontTri(i0, i1, i2, cInner);

    addBackTri(o0, o1, i1, cOuter);
    addBackTri(o0, i1, i0, cOuter);
    addBackTri(o1, o2, i2, cOuter);
    addBackTri(o1, i2, i1, cOuter);
    addBackTri(o2, o0, i0, cOuter);
    addBackTri(o2, i0, i2, cOuter);
    addBackTri(i0, i1, i2, cInner);

    addEdgeQuad(o0, o1, cWall);
    addEdgeQuad(o1, o2, cWall);
    addEdgeQuad(o2, o0, cWall);

    m_moonIndexCount = static_cast<uint32_t>(indices.size());
    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    m_moonVbo = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_moonVbo.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

    m_moonIbo = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_moonIbo.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
}

void CelestialRenderer::render(VkCommandBuffer cmd,
                               const Pipeline& pipeline,
                               const glm::vec3& camPos,
                               const glm::vec3& sunDir,
                               const glm::mat4& vpMatrix) {
    pipeline.bind(cmd);

    // 1. Draw Equilateral Triangular Sun (Golden / Brilliant Warm White)
    if (sunDir.y > -0.2f && m_sunIndexCount > 0 && m_sunVbo.isValid()) {
        glm::vec3 sunPos = camPos + sunDir * 220.0f;
        glm::vec3 forward = -sunDir;
        glm::vec3 upWorld(0.0f, 1.0f, 0.0f);
        if (std::abs(forward.y) > 0.98f) upWorld = glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 right = glm::normalize(glm::cross(upWorld, forward));
        glm::vec3 up = glm::cross(forward, right);

        glm::mat4 rot(1.0f);
        rot[0] = glm::vec4(right, 0.0f);
        rot[1] = glm::vec4(up, 0.0f);
        rot[2] = glm::vec4(forward, 0.0f);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), sunPos) * rot * glm::scale(glm::mat4(1.0f), glm::vec3(18.0f, 18.0f, 4.0f));

        glm::mat4 mvp = vpMatrix * model;
        PushConstants pc{};
        std::memcpy(pc.mvp, &mvp[0][0], sizeof(float) * 16);
        pc.sunDir[0] = sunDir.x; pc.sunDir[1] = sunDir.y; pc.sunDir[2] = sunDir.z; pc.sunDir[3] = 2.0f; // Self-illuminated flag >= 1.9
        pc.lightColor[0] = 1.0f; pc.lightColor[1] = 0.95f; pc.lightColor[2] = 0.4f; pc.lightColor[3] = 0.0f;
        pc.skyFog[3] = 2000.0f;

        vkCmdPushConstants(cmd, pipeline.getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &pc);

        VkBuffer vbs[] = {m_sunVbo.getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
        vkCmdBindIndexBuffer(cmd, m_sunIbo.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_sunIndexCount, 1, 0, 0, 0);
    }

    // 2. Draw Equilateral Triangular Moon (Luminous Silver-Cyan / Ice White)
    glm::vec3 moonDir = -sunDir;
    if (moonDir.y > -0.2f && m_moonIndexCount > 0 && m_moonVbo.isValid()) {
        glm::vec3 moonPos = camPos + moonDir * 220.0f;
        glm::vec3 forward = -moonDir;
        glm::vec3 upWorld(0.0f, 1.0f, 0.0f);
        if (std::abs(forward.y) > 0.98f) upWorld = glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 right = glm::normalize(glm::cross(upWorld, forward));
        glm::vec3 up = glm::cross(forward, right);

        glm::mat4 rot(1.0f);
        rot[0] = glm::vec4(right, 0.0f);
        rot[1] = glm::vec4(up, 0.0f);
        rot[2] = glm::vec4(forward, 0.0f);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), moonPos) * rot * glm::scale(glm::mat4(1.0f), glm::vec3(15.0f, 15.0f, 3.5f));

        glm::mat4 mvp = vpMatrix * model;
        PushConstants pc{};
        std::memcpy(pc.mvp, &mvp[0][0], sizeof(float) * 16);
        pc.sunDir[0] = moonDir.x; pc.sunDir[1] = moonDir.y; pc.sunDir[2] = moonDir.z; pc.sunDir[3] = 2.0f; // Self-illuminated flag >= 1.9
        pc.lightColor[0] = 0.85f; pc.lightColor[1] = 0.90f; pc.lightColor[2] = 1.0f; pc.lightColor[3] = 0.0f;
        pc.skyFog[3] = 2000.0f;

        vkCmdPushConstants(cmd, pipeline.getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &pc);

        VkBuffer vbs[] = {m_moonVbo.getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
        vkCmdBindIndexBuffer(cmd, m_moonIbo.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, m_moonIndexCount, 1, 0, 0, 0);
    }
}

} // namespace prismcraft
