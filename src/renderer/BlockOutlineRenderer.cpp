#include "BlockOutlineRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/ChunkMesher.hpp"
#include "world/World.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstring>
#include <algorithm>

namespace prismcraft {

BlockOutlineRenderer::BlockOutlineRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
    buildPrismWireframe(0);
    buildPrismWireframe(1);
}

void BlockOutlineRenderer::buildPrismWireframe(int subIndex) {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec3 wireColor(1.0f, 1.0f, 1.0f); // White for mathematical color inversion (ONE_MINUS_DST_COLOR)

    auto addLineQuad = [&](const glm::vec3& p0, const glm::vec3& p1, float thickness) {
        glm::vec3 dir = p1 - p0;
        float len = glm::length(dir);
        if (len < 0.0001f) return;
        dir /= len;

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        if (std::abs(dir.y) > 0.9f) up = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 side = glm::normalize(glm::cross(dir, up)) * (thickness * 0.5f);

        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec2 uv(0.0f, 0.0f);
        glm::vec3 norm(0.0f, 1.0f, 0.0f);
        vertices.push_back({p0 - side, uv, norm, wireColor});
        vertices.push_back({p0 + side, uv, norm, wireColor});
        vertices.push_back({p1 + side, uv, norm, wireColor});
        vertices.push_back({p1 - side, uv, norm, wireColor});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    float th = 0.02f; // Outline line thickness
    float eps = 0.002f; // Slight expansion so it doesn't z-fight with the block surface

    if (subIndex == 0) {
        // s=0 equilateral vertices: (0,0), (1,0), (0.5, TRI_HEIGHT)
        glm::vec3 v0(-eps, -eps, -eps);
        glm::vec3 v1(1.0f + eps, -eps, -eps);
        glm::vec3 v2(0.5f, -eps, TRI_HEIGHT + eps);

        glm::vec3 v0_top(-eps, 1.0f + eps, -eps);
        glm::vec3 v1_top(1.0f + eps, 1.0f + eps, -eps);
        glm::vec3 v2_top(0.5f, 1.0f + eps, TRI_HEIGHT + eps);

        // Bottom 3 edges
        addLineQuad(v0, v1, th);
        addLineQuad(v1, v2, th);
        addLineQuad(v2, v0, th);

        // Top 3 edges
        addLineQuad(v0_top, v1_top, th);
        addLineQuad(v1_top, v2_top, th);
        addLineQuad(v2_top, v0_top, th);

        // 3 Vertical edges
        addLineQuad(v0, v0_top, th);
        addLineQuad(v1, v1_top, th);
        addLineQuad(v2, v2_top, th);

        m_indexCount0 = static_cast<uint32_t>(indices.size());
        VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
        VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

        m_vbo0 = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_vbo0.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

        m_ibo0 = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_ibo0.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
    } else {
        // s=1 equilateral vertices: (1,0), (1.5, TRI_HEIGHT), (0.5, TRI_HEIGHT)
        glm::vec3 v0(1.0f, -eps, -eps);
        glm::vec3 v1(1.5f + eps, -eps, TRI_HEIGHT + eps);
        glm::vec3 v2(0.5f - eps, -eps, TRI_HEIGHT + eps);

        glm::vec3 v0_top(1.0f, 1.0f + eps, -eps);
        glm::vec3 v1_top(1.5f + eps, 1.0f + eps, TRI_HEIGHT + eps);
        glm::vec3 v2_top(0.5f - eps, 1.0f + eps, TRI_HEIGHT + eps);

        // Bottom 3 edges
        addLineQuad(v0, v1, th);
        addLineQuad(v1, v2, th);
        addLineQuad(v2, v0, th);

        // Top 3 edges
        addLineQuad(v0_top, v1_top, th);
        addLineQuad(v1_top, v2_top, th);
        addLineQuad(v2_top, v0_top, th);

        // 3 Vertical edges
        addLineQuad(v0, v0_top, th);
        addLineQuad(v1, v1_top, th);
        addLineQuad(v2, v2_top, th);

        m_indexCount1 = static_cast<uint32_t>(indices.size());
        VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
        VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

        m_vbo1 = Buffer(m_context, vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_vbo1.uploadStaged(m_context, m_cmdQueue, vertices.data(), vSize);

        m_ibo1 = Buffer(m_context, iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        m_ibo1.uploadStaged(m_context, m_cmdQueue, indices.data(), iSize);
    }
}

void BlockOutlineRenderer::render(VkCommandBuffer cmd,
                                  const Pipeline& pipeline,
                                  const std::optional<CellCoord>& targetCell,
                                  const glm::mat4& vpMatrix,
                                  const World& world) {
    if (!targetCell.has_value()) return;

    const CellCoord& c = targetCell.value();
    Cell cell = world.getCell(c.x, c.y, c.z, c.s);
    if (cell.type == BlockType::Air) return;

    bool isCustom = cell.isDoor() || cell.isTrapdoor() || cell.isBed() ||
                    cell.isLantern() || cell.isTorch() || cell.type == BlockType::Cake ||
                    cell.isCactus() || cell.isFoliage();

    if (!isCustom) {
        // Standard Equilateral Triangular Prism Wireframe
        float posX = static_cast<float>(c.x) + getRowXOffset(c.z);
        float posY = static_cast<float>(c.y);
        float posZ = static_cast<float>(c.z) * TRI_HEIGHT;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, posZ));
        glm::mat4 mvp = vpMatrix * model;

        PushConstants pc{};
        std::memcpy(pc.mvp, &mvp[0][0], sizeof(float) * 16);
        pc.sunDir[3] = 2.0f; // Self-illuminated pure white for negative inversion
        pc.lightColor[0] = 1.0f; pc.lightColor[1] = 1.0f; pc.lightColor[2] = 1.0f;
        pc.skyFog[3] = 1000.0f;

        vkCmdPushConstants(cmd, pipeline.getLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &pc);

        if (c.s == 0 && m_indexCount0 > 0 && m_vbo0.isValid()) {
            VkBuffer vbs[] = {m_vbo0.getBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
            vkCmdBindIndexBuffer(cmd, m_ibo0.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, m_indexCount0, 1, 0, 0, 0);
        } else if (c.s == 1 && m_indexCount1 > 0 && m_vbo1.isValid()) {
            VkBuffer vbs[] = {m_vbo1.getBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
            vkCmdBindIndexBuffer(cmd, m_ibo1.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, m_indexCount1, 1, 0, 0, 0);
        }
        return;
    }

    // Dynamic Shape-Aware Wireframe in World Coordinates
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;
    glm::vec3 wireColor(1.0f, 1.0f, 1.0f);
    float th = 0.02f;

    auto addLineQuad = [&](const glm::vec3& p0, const glm::vec3& p1) {
        glm::vec3 dir = p1 - p0;
        float len = glm::length(dir);
        if (len < 0.0001f) return;
        dir /= len;

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        if (std::abs(dir.y) > 0.9f) up = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 side = glm::normalize(glm::cross(dir, up)) * (th * 0.5f);

        uint32_t b = static_cast<uint32_t>(vertices.size());
        glm::vec2 uv(0.0f, 0.0f);
        glm::vec3 norm(0.0f, 1.0f, 0.0f);
        vertices.push_back({p0 - side, uv, norm, wireColor});
        vertices.push_back({p0 + side, uv, norm, wireColor});
        vertices.push_back({p1 + side, uv, norm, wireColor});
        vertices.push_back({p1 - side, uv, norm, wireColor});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    auto addBox12 = [&](const glm::vec3& c0, const glm::vec3& c1, const glm::vec3& c2, const glm::vec3& c3,
                       const glm::vec3& c4, const glm::vec3& c5, const glm::vec3& c6, const glm::vec3& c7) {
        addLineQuad(c0, c1); addLineQuad(c1, c2); addLineQuad(c2, c3); addLineQuad(c3, c0);
        addLineQuad(c4, c5); addLineQuad(c5, c6); addLineQuad(c6, c7); addLineQuad(c7, c4);
        addLineQuad(c0, c4); addLineQuad(c1, c5); addLineQuad(c2, c6); addLineQuad(c3, c7);
    };

    int wx = c.x;
    int wz = c.z;
    int s = c.s;
    int y = c.y;
    float py = static_cast<float>(y);

    glm::vec2 vXZ[3];
    getPrismVerticesXZ(wx, wz, s, vXZ);

    if (cell.isDoor()) {
        bool isOpen = cell.isDoorOpen();
        uint8_t facing = cell.getDoorFacing();
        glm::vec2 w0, w1, wallIn;

        if (s == 0) {
            if (facing == 0)      { w0 = vXZ[0]; w1 = vXZ[1]; wallIn = glm::vec2(0.0f, 1.0f); }
            else if (facing == 1) { w0 = vXZ[0]; w1 = vXZ[2]; wallIn = glm::vec2(SQRT_3_OVER_2, -0.5f); }
            else                  { w0 = vXZ[1]; w1 = vXZ[2]; wallIn = glm::vec2(-SQRT_3_OVER_2, -0.5f); }
        } else {
            if (facing == 0)      { w0 = vXZ[1]; w1 = vXZ[2]; wallIn = glm::vec2(0.0f, -1.0f); }
            else if (facing == 1) { w0 = vXZ[0]; w1 = vXZ[2]; wallIn = glm::vec2(SQRT_3_OVER_2, 0.5f); }
            else                  { w0 = vXZ[0]; w1 = vXZ[1]; wallIn = glm::vec2(-SQRT_3_OVER_2, 0.5f); }
        }

        glm::vec2 wallDir = glm::normalize(w1 - w0);
        float doorWidth = glm::length(w1 - w0);
        float doorThick = 0.1875f;
        float doorHeight = 1.0f;

        glm::vec3 vecSpan, vecThick, pBase;
        if (!isOpen) {
            vecSpan = glm::vec3(wallDir.x, 0.0f, wallDir.y) * doorWidth;
            vecThick = glm::vec3(wallIn.x, 0.0f, wallIn.y) * doorThick;
            pBase = glm::vec3(w0.x, py, w0.y) + glm::vec3(wallIn.x, 0.0f, wallIn.y) * 0.02f;
        } else {
            vecSpan = glm::vec3(wallIn.x, 0.0f, wallIn.y) * doorWidth;
            vecThick = glm::vec3(-wallDir.x, 0.0f, -wallDir.y) * doorThick;
            pBase = glm::vec3(w0.x, py, w0.y) + glm::vec3(wallIn.x, 0.0f, wallIn.y) * 0.02f;
        }

        glm::vec3 vecUp(0.0f, doorHeight, 0.0f);
        glm::vec3 c0 = pBase;
        glm::vec3 c1 = pBase + vecSpan;
        glm::vec3 c2 = pBase + vecSpan + vecThick;
        glm::vec3 c3 = pBase + vecThick;
        glm::vec3 c4 = c0 + vecUp;
        glm::vec3 c5 = c1 + vecUp;
        glm::vec3 c6 = c2 + vecUp;
        glm::vec3 c7 = c3 + vecUp;

        addBox12(c0, c1, c2, c3, c4, c5, c6, c7);
    } else if (cell.isTrapdoor()) {
        bool isOpen = cell.isTrapdoorOpen();
        uint8_t facing = cell.getTrapdoorFacing();
        float thick = 0.1875f;

        glm::vec2 w0, w1, wallIn;
        if (s == 0) {
            if (facing == 0)      { w0 = vXZ[0]; w1 = vXZ[1]; wallIn = glm::vec2(0.0f, 1.0f); }
            else if (facing == 1) { w0 = vXZ[0]; w1 = vXZ[2]; wallIn = glm::vec2(SQRT_3_OVER_2, -0.5f); }
            else                  { w0 = vXZ[1]; w1 = vXZ[2]; wallIn = glm::vec2(-SQRT_3_OVER_2, -0.5f); }
        } else {
            if (facing == 0)      { w0 = vXZ[1]; w1 = vXZ[2]; wallIn = glm::vec2(0.0f, -1.0f); }
            else if (facing == 1) { w0 = vXZ[0]; w1 = vXZ[2]; wallIn = glm::vec2(SQRT_3_OVER_2, 0.5f); }
            else                  { w0 = vXZ[0]; w1 = vXZ[1]; wallIn = glm::vec2(-SQRT_3_OVER_2, 0.5f); }
        }

        if (!isOpen) {
            glm::vec3 b0(vXZ[0].x, py, vXZ[0].y);
            glm::vec3 b1(vXZ[1].x, py, vXZ[1].y);
            glm::vec3 b2(vXZ[2].x, py, vXZ[2].y);
            glm::vec3 t0(vXZ[0].x, py + thick, vXZ[0].y);
            glm::vec3 t1(vXZ[1].x, py + thick, vXZ[1].y);
            glm::vec3 t2(vXZ[2].x, py + thick, vXZ[2].y);

            addLineQuad(b0, b1); addLineQuad(b1, b2); addLineQuad(b2, b0);
            addLineQuad(t0, t1); addLineQuad(t1, t2); addLineQuad(t2, t0);
            addLineQuad(b0, t0); addLineQuad(b1, t1); addLineQuad(b2, t2);
        } else {
            glm::vec3 vecSpan(w1.x - w0.x, 0.0f, w1.y - w0.y);
            glm::vec3 vecThick = glm::vec3(wallIn.x, 0.0f, wallIn.y) * thick;
            glm::vec3 vecUp(0.0f, 1.0f, 0.0f);
            glm::vec3 pBase = glm::vec3(w0.x, py, w0.y) + glm::vec3(wallIn.x, 0.0f, wallIn.y) * 0.015f;

            glm::vec3 c0 = pBase;
            glm::vec3 c1 = pBase + vecSpan;
            glm::vec3 c2 = pBase + vecSpan + vecThick;
            glm::vec3 c3 = pBase + vecThick;
            glm::vec3 c4 = c0 + vecUp;
            glm::vec3 c5 = c1 + vecUp;
            glm::vec3 c6 = c2 + vecUp;
            glm::vec3 c7 = c3 + vecUp;

            addBox12(c0, c1, c2, c3, c4, c5, c6, c7);
        }
    } else if (cell.isBed()) {
        CellCoord bedCells[4];
        getBedAllCells(wx, y, wz, s, cell.level, bedCells);

        uint8_t facing = cell.getBedFacing();
        int xF = bedCells[0].x, zF = bedCells[0].z;
        int xH = bedCells[2].x, zH = bedCells[2].z;

        float x0_F = static_cast<float>(xF) + getRowXOffset(zF);
        float z0_F = static_cast<float>(zF) * TRI_HEIGHT;
        float z1_F = static_cast<float>(zF + 1) * TRI_HEIGHT;

        float x0_H = static_cast<float>(xH) + getRowXOffset(zH);
        float z0_H = static_cast<float>(zH) * TRI_HEIGHT;
        float z1_H = static_cast<float>(zH + 1) * TRI_HEIGHT;

        glm::vec2 V0_F(x0_F, z0_F), V1_F(x0_F + 1.0f, z0_F), V2_F(x0_F + 1.5f, z1_F), V3_F(x0_F + 0.5f, z1_F);
        glm::vec2 V0_H(x0_H, z0_H), V1_H(x0_H + 1.0f, z0_H), V2_H(x0_H + 1.5f, z1_H), V3_H(x0_H + 0.5f, z1_H);

        glm::vec2 C0_F = V0_F;
        glm::vec2 C1_F = V1_F;
        glm::vec2 C2_F = V2_F;
        glm::vec2 C3_F = V3_F;

        glm::vec2 C0_H = V0_H;
        glm::vec2 C1_H = V1_H;
        glm::vec2 C2_H = V2_H;
        glm::vec2 C3_H = V3_H;

        glm::vec2 P_head0, P_head1, P_foot0, P_foot1;
        if (facing == 0) { // +X
            P_head0 = C1_H; P_head1 = C2_H;
            P_foot0 = C3_F; P_foot1 = C0_F;
        } else if (facing == 1) { // -X
            P_head0 = C3_H; P_head1 = C0_H;
            P_foot0 = C1_F; P_foot1 = C2_F;
        } else if (facing == 2) { // +Z
            P_head0 = C2_H; P_head1 = C3_H;
            P_foot0 = C0_F; P_foot1 = C1_F;
        } else { // -Z (3)
            P_head0 = C0_H; P_head1 = C1_H;
            P_foot0 = C2_F; P_foot1 = C3_F;
        }

        float hLeg = 0.1875f;
        float hBed = 0.50f;

        glm::vec3 M_h0(P_head0.x, py + hBed, P_head0.y);
        glm::vec3 M_h1(P_head1.x, py + hBed, P_head1.y);
        glm::vec3 M_f1(P_foot1.x, py + hBed, P_foot1.y);
        glm::vec3 M_f0(P_foot0.x, py + hBed, P_foot0.y);

        glm::vec3 B_h0(P_head0.x, py + hLeg, P_head0.y);
        glm::vec3 B_h1(P_head1.x, py + hLeg, P_head1.y);
        glm::vec3 B_f1(P_foot1.x, py + hLeg, P_foot1.y);
        glm::vec3 B_f0(P_foot0.x, py + hLeg, P_foot0.y);

        // Top mattress frame
        addLineQuad(M_h0, M_h1); addLineQuad(M_h1, M_f1); addLineQuad(M_f1, M_f0); addLineQuad(M_f0, M_h0);
        // Base skirt frame
        addLineQuad(B_h0, B_h1); addLineQuad(B_h1, B_f1); addLineQuad(B_f1, B_f0); addLineQuad(B_f0, B_h0);
        // Vertical corner posts
        addLineQuad(B_h0, M_h0); addLineQuad(B_h1, M_h1); addLineQuad(B_f1, M_f1); addLineQuad(B_f0, M_f0);
        // 4 Legs down to floor
        addLineQuad(glm::vec3(P_head0.x, py, P_head0.y), B_h0);
        addLineQuad(glm::vec3(P_head1.x, py, P_head1.y), B_h1);
        addLineQuad(glm::vec3(P_foot1.x, py, P_foot1.y), B_f1);
        addLineQuad(glm::vec3(P_foot0.x, py, P_foot0.y), B_f0);
    } else if (cell.isLantern()) {
        glm::vec2 cent = (vXZ[0] + vXZ[1] + vXZ[2]) / 3.0f;
        bool isHanging = (cell.level == 1);
        float halfW = 0.17f;
        float hBody = 0.44f;
        float yBottom = isHanging ? (py + 1.0f - hBody - 0.12f) : py;
        float yTop = yBottom + hBody;

        glm::vec3 c0(cent.x - halfW, yBottom, cent.y - halfW);
        glm::vec3 c1(cent.x + halfW, yBottom, cent.y - halfW);
        glm::vec3 c2(cent.x + halfW, yBottom, cent.y + halfW);
        glm::vec3 c3(cent.x - halfW, yBottom, cent.y + halfW);
        glm::vec3 c4(cent.x - halfW, yTop, cent.y - halfW);
        glm::vec3 c5(cent.x + halfW, yTop, cent.y - halfW);
        glm::vec3 c6(cent.x + halfW, yTop, cent.y + halfW);
        glm::vec3 c7(cent.x - halfW, yTop, cent.y + halfW);

        addBox12(c0, c1, c2, c3, c4, c5, c6, c7);
        if (isHanging) {
            addLineQuad(glm::vec3(cent.x, yTop, cent.y), glm::vec3(cent.x, py + 1.0f, cent.y));
        }
    } else if (cell.isTorch()) {
        glm::vec3 center = cellToWorldCenter(wx, y, wz, s);
        uint8_t attachment = cell.getTorchAttachment();
        glm::vec3 pBase = glm::vec3(center.x, py, center.z);
        glm::vec3 torchAxis(0.0f, 1.0f, 0.0f);
        if (attachment != 0) {
            glm::vec2 wP0, wP1;
            glm::vec3 wallDir;
            if (attachment == 1) {
                if (s == 0) { wP0 = vXZ[0]; wP1 = vXZ[1]; wallDir = glm::vec3(0.0f, 0.0f, 1.0f); }
                else        { wP0 = vXZ[1]; wP1 = vXZ[2]; wallDir = glm::vec3(0.0f, 0.0f, -1.0f); }
            } else if (attachment == 2) {
                wP0 = vXZ[0]; wP1 = vXZ[2];
                wallDir = (s == 0) ? glm::vec3(SQRT_3_OVER_2, 0.0f, -0.5f) : glm::vec3(SQRT_3_OVER_2, 0.0f, 0.5f);
            } else {
                if (s == 0) { wP0 = vXZ[1]; wP1 = vXZ[2]; wallDir = glm::vec3(-SQRT_3_OVER_2, 0.0f, -0.5f); }
                else        { wP0 = vXZ[0]; wP1 = vXZ[1]; wallDir = glm::vec3(-SQRT_3_OVER_2, 0.0f, 0.5f); }
            }
            glm::vec2 wallMid = (wP0 + wP1) * 0.5f;
            pBase = glm::vec3(wallMid.x, py + 0.22f, wallMid.y) + wallDir * 0.04f;
            torchAxis = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f) + wallDir * 0.38f);
        }
        glm::vec3 upRef = (std::abs(torchAxis.y) > 0.9f) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 uAxis = glm::normalize(glm::cross(torchAxis, upRef)) * 0.14f;
        glm::vec3 vAxis = glm::normalize(glm::cross(torchAxis, uAxis)) * 0.14f;
        glm::vec3 pBot = pBase - torchAxis * 0.04f;
        glm::vec3 pTop = pBase + torchAxis * 0.68f;

        glm::vec3 c0 = pBot - uAxis - vAxis;
        glm::vec3 c1 = pBot + uAxis - vAxis;
        glm::vec3 c2 = pBot + uAxis + vAxis;
        glm::vec3 c3 = pBot - uAxis + vAxis;
        glm::vec3 c4 = pTop - uAxis - vAxis;
        glm::vec3 c5 = pTop + uAxis - vAxis;
        glm::vec3 c6 = pTop + uAxis + vAxis;
        glm::vec3 c7 = pTop - uAxis + vAxis;

        addBox12(c0, c1, c2, c3, c4, c5, c6, c7);
    } else if (cell.isFoliage()) {
        glm::vec3 cent = cellToWorldCenter(wx, y, wz, s);
        float hw = 0.28f;
        float h = 0.82f;
        glm::vec3 c0(cent.x - hw, py, cent.z - hw);
        glm::vec3 c1(cent.x + hw, py, cent.z - hw);
        glm::vec3 c2(cent.x + hw, py, cent.z + hw);
        glm::vec3 c3(cent.x - hw, py, cent.z + hw);
        glm::vec3 c4(cent.x - hw, py + h, cent.z - hw);
        glm::vec3 c5(cent.x + hw, py + h, cent.z - hw);
        glm::vec3 c6(cent.x + hw, py + h, cent.z + hw);
        glm::vec3 c7(cent.x - hw, py + h, cent.z + hw);
        addBox12(c0, c1, c2, c3, c4, c5, c6, c7);
    } else if (cell.isCactus()) {
        float x0 = static_cast<float>(wx) + getRowXOffset(wz);
        float z0 = static_cast<float>(wz) * TRI_HEIGHT;
        float z1 = static_cast<float>(wz + 1) * TRI_HEIGHT;

        glm::vec2 V0(x0, z0);
        glm::vec2 V1(x0 + 1.0f, z0);
        glm::vec2 V2(x0 + 1.5f, z1);
        glm::vec2 V3(x0 + 0.5f, z1);

        glm::vec2 P_cent = (V0 + V2) * 0.5f;
        float d = 0.0625f;

        glm::vec2 C0 = V0 + glm::normalize(P_cent - V0) * (2.0f * d);
        glm::vec2 C2 = V2 + glm::normalize(P_cent - V2) * (2.0f * d);
        glm::vec2 C1 = V1 + glm::normalize(P_cent - V1) * (d / SQRT_3_OVER_2);
        glm::vec2 C3 = V3 + glm::normalize(P_cent - V3) * (d / SQRT_3_OVER_2);

        glm::vec2 cXZ[3];
        if (s == 0) { cXZ[0] = C0; cXZ[1] = C1; cXZ[2] = C3; }
        else        { cXZ[0] = C1; cXZ[1] = C2; cXZ[2] = C3; }

        glm::vec3 b0(cXZ[0].x, py, cXZ[0].y);
        glm::vec3 b1(cXZ[1].x, py, cXZ[1].y);
        glm::vec3 b2(cXZ[2].x, py, cXZ[2].y);
        glm::vec3 t0(cXZ[0].x, py + 1.0f, cXZ[0].y);
        glm::vec3 t1(cXZ[1].x, py + 1.0f, cXZ[1].y);
        glm::vec3 t2(cXZ[2].x, py + 1.0f, cXZ[2].y);

        addLineQuad(b0, b1); addLineQuad(b1, b2); addLineQuad(b2, b0);
        addLineQuad(t0, t1); addLineQuad(t1, t2); addLineQuad(t2, t0);
        addLineQuad(b0, t0); addLineQuad(b1, t1); addLineQuad(b2, t2);
    } else if (cell.type == BlockType::Cake) {
        glm::vec2 cent = (vXZ[0] + vXZ[1] + vXZ[2]) / 3.0f;
        float shrink = 0.06f;
        glm::vec2 c0 = vXZ[0] + glm::normalize(cent - vXZ[0]) * shrink;
        glm::vec2 c1 = vXZ[1] + glm::normalize(cent - vXZ[1]) * shrink;
        glm::vec2 c2 = vXZ[2] + glm::normalize(cent - vXZ[2]) * shrink;
        float hCake = 0.4375f;

        glm::vec3 b0(c0.x, py, c0.y);
        glm::vec3 b1(c1.x, py, c1.y);
        glm::vec3 b2(c2.x, py, c2.y);
        glm::vec3 t0(c0.x, py + hCake, c0.y);
        glm::vec3 t1(c1.x, py + hCake, c1.y);
        glm::vec3 t2(c2.x, py + hCake, c2.y);

        addLineQuad(b0, b1); addLineQuad(b1, b2); addLineQuad(b2, b0);
        addLineQuad(t0, t1); addLineQuad(t1, t2); addLineQuad(t2, t0);
        addLineQuad(b0, t0); addLineQuad(b1, t1); addLineQuad(b2, t2);
    }

    if (indices.empty()) return;

    uint32_t f = m_cmdQueue.getCurrentFrame();
    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    if (!m_dynVbo[f].isValid() || m_dynVbo[f].getSize() < vSize) {
        m_dynVbo[f] = Buffer(m_context, std::max(vSize, (VkDeviceSize)16384),
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    if (!m_dynIbo[f].isValid() || m_dynIbo[f].getSize() < iSize) {
        m_dynIbo[f] = Buffer(m_context, std::max(iSize, (VkDeviceSize)8192),
                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    m_dynVbo[f].upload(vertices.data(), vSize);
    m_dynIbo[f].upload(indices.data(), iSize);
    m_dynIndexCount[f] = static_cast<uint32_t>(indices.size());

    PushConstants pc{};
    std::memcpy(pc.mvp, &vpMatrix[0][0], sizeof(float) * 16);
    pc.sunDir[3] = 2.0f;
    pc.lightColor[0] = 1.0f; pc.lightColor[1] = 1.0f; pc.lightColor[2] = 1.0f;
    pc.skyFog[3] = 1000.0f;

    vkCmdPushConstants(cmd, pipeline.getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &pc);

    VkBuffer vbs[] = {m_dynVbo[f].getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_dynIbo[f].getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_dynIndexCount[f], 1, 0, 0, 0);
}

} // namespace prismcraft
