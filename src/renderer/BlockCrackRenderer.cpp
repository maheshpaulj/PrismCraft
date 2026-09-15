#include "BlockCrackRenderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "rhi/CommandQueue.hpp"
#include "world/ChunkMesher.hpp"
#include "world/World.hpp"
#include "TextureAtlas.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace prismcraft {

BlockCrackRenderer::BlockCrackRenderer(VulkanContext& context, CommandQueue& cmdQueue)
    : m_context(context)
    , m_cmdQueue(cmdQueue) {
}

void BlockCrackRenderer::rebuildBlockMesh(const World& world, const CellCoord& coord, int stage, uint32_t frameIndex) {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t> indices;

    Cell cell = world.getCell(coord.x, coord.y, coord.z, coord.s);
    if (cell.type == BlockType::Air) {
        m_indexCount[frameIndex] = 0;
        return;
    }

    int clampedStage = std::clamp(stage, 0, 9);
    int tileCrack = TextureAtlas::getDestroyStageTile(clampedStage);
    glm::vec4 uv = TextureAtlas::getTileUV(tileCrack);

    int wx = coord.x;
    int y = coord.y;
    int wz = coord.z;
    int s = coord.s;
    float py = static_cast<float>(y);

    // Well-lit crack lines in both day and night
    glm::vec3 col(1.0f, 1.0f, 0.2f);

    auto addCrackQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& norm) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        float eps = 0.0015f;
        glm::vec3 p0 = v0 + norm * eps;
        glm::vec3 p1 = v1 + norm * eps;
        glm::vec3 p2 = v2 + norm * eps;
        glm::vec3 p3 = v3 + norm * eps;

        vertices.push_back({p0, glm::vec2(uv.x, uv.w), norm, col});
        vertices.push_back({p1, glm::vec2(uv.x, uv.y), norm, col});
        vertices.push_back({p2, glm::vec2(uv.z, uv.y), norm, col});
        vertices.push_back({p3, glm::vec2(uv.z, uv.w), norm, col});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
        indices.push_back(b + 2); indices.push_back(b + 3); indices.push_back(b + 0);
    };

    auto addCrackTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                           const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                           const glm::vec3& norm) {
        uint32_t b = static_cast<uint32_t>(vertices.size());
        float eps = 0.0015f;
        glm::vec3 p0 = v0 + norm * eps;
        glm::vec3 p1 = v1 + norm * eps;
        glm::vec3 p2 = v2 + norm * eps;

        vertices.push_back({p0, uv0, norm, col});
        vertices.push_back({p1, uv1, norm, col});
        vertices.push_back({p2, uv2, norm, col});

        indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
    };

    // Unified Complementary Triangle UV coordinates for top and bottom faces:
    // s=0: (0,0), (1,0), (0,1)
    // s=1: (1,0), (1,1), (0,1)
    glm::vec2 uvT0, uvT1, uvT2;
    glm::vec2 uvB0, uvB1, uvB2;
    if (s == 0) {
        // Up-pointing triangle: (0,0), (1,0), (0,1)
        uvT0 = glm::vec2(uv.x, uv.y);
        uvT1 = glm::vec2(uv.z, uv.y);
        uvT2 = glm::vec2(uv.x, uv.w);

        uvB0 = glm::vec2(uv.x, uv.y);
        uvB1 = glm::vec2(uv.z, uv.y);
        uvB2 = glm::vec2(uv.x, uv.w);
    } else {
        // Down-pointing triangle: (1,0), (1,1), (0,1)
        uvT0 = glm::vec2(uv.z, uv.y);
        uvT1 = glm::vec2(uv.z, uv.w);
        uvT2 = glm::vec2(uv.x, uv.w);

        uvB0 = glm::vec2(uv.z, uv.y);
        uvB1 = glm::vec2(uv.z, uv.w);
        uvB2 = glm::vec2(uv.x, uv.w);
    }

    // 1. Doors (True 3D door panel with swing angle and hinge)
    if (cell.isDoor()) {
        bool isOpen = cell.isDoorOpen();
        uint8_t facing = cell.getDoorFacing(); // 0 = Base wall, 1 = Left wall, 2 = Right wall

        glm::vec2 vXZ[3];
        getPrismVerticesXZ(wx, wz, s, vXZ);

        glm::vec2 w0, w1;
        glm::vec2 wallIn;

        if (s == 0) {
            if (facing == 0) {
                w0 = vXZ[0]; w1 = vXZ[1];
                wallIn = glm::vec2(0.0f, 1.0f);
            } else if (facing == 1) {
                w0 = vXZ[0]; w1 = vXZ[2];
                wallIn = glm::vec2(SQRT_3_OVER_2, -0.5f);
            } else {
                w0 = vXZ[1]; w1 = vXZ[2];
                wallIn = glm::vec2(-SQRT_3_OVER_2, -0.5f);
            }
        } else {
            if (facing == 0) {
                w0 = vXZ[1]; w1 = vXZ[2];
                wallIn = glm::vec2(0.0f, -1.0f);
            } else if (facing == 1) {
                w0 = vXZ[0]; w1 = vXZ[2];
                wallIn = glm::vec2(SQRT_3_OVER_2, 0.5f);
            } else {
                w0 = vXZ[0]; w1 = vXZ[1];
                wallIn = glm::vec2(-SQRT_3_OVER_2, 0.5f);
            }
        }

        glm::vec2 wallDir = glm::normalize(w1 - w0);
        float doorWidth = glm::length(w1 - w0);
        float doorThick = 0.1875f;
        float doorHeight = 1.0f;

        glm::vec3 vecSpan;
        glm::vec3 vecThick;
        glm::vec3 pBase;

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

        glm::vec3 normFront = -glm::normalize(vecThick);
        glm::vec3 normBack  =  glm::normalize(vecThick);
        glm::vec3 normHinge = -glm::normalize(vecSpan);
        glm::vec3 normLatch =  glm::normalize(vecSpan);

        addCrackQuad(c0, c4, c5, c1, normFront);
        addCrackQuad(c2, c6, c7, c3, normBack);
        addCrackQuad(c3, c7, c4, c0, normHinge);
        addCrackQuad(c1, c5, c6, c2, normLatch);
        addCrackQuad(c4, c7, c6, c5, glm::vec3(0, 1, 0));
        addCrackQuad(c0, c1, c2, c3, glm::vec3(0, -1, 0));
    }
    // 2. Trapdoors (Horizontal hatch or vertical open flap)
    else if (cell.isTrapdoor()) {
        bool isOpen = cell.isTrapdoorOpen();
        uint8_t facing = cell.getTrapdoorFacing();
        glm::vec2 vXZ[3];
        getPrismVerticesXZ(wx, wz, s, vXZ);
        float thick = 0.1875f;

        if (!isOpen) {
            glm::vec3 b0(vXZ[0].x, py, vXZ[0].y);
            glm::vec3 b1(vXZ[1].x, py, vXZ[1].y);
            glm::vec3 b2(vXZ[2].x, py, vXZ[2].y);

            glm::vec3 t0(vXZ[0].x, py + thick, vXZ[0].y);
            glm::vec3 t1(vXZ[1].x, py + thick, vXZ[1].y);
            glm::vec3 t2(vXZ[2].x, py + thick, vXZ[2].y);

            addCrackTri(t0, t2, t1, uvT0, uvT2, uvT1, glm::vec3(0, 1, 0));
            addCrackTri(b0, b1, b2, uvB0, uvB1, uvB2, glm::vec3(0, -1, 0));
            addCrackQuad(b0, t0, t1, b1, glm::vec3(0, 0, 1));
            addCrackQuad(b1, t1, t2, b2, glm::vec3(1, 0, 0));
            addCrackQuad(b2, t2, t0, b0, glm::vec3(-1, 0, 0));
        } else {
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

            glm::vec3 normFront = glm::vec3(wallIn.x, 0.0f, wallIn.y);
            glm::vec3 normBack = -normFront;
            glm::vec3 normLeft = -glm::normalize(vecSpan);
            glm::vec3 normRight = glm::normalize(vecSpan);

            // 1. Front face (facing into cell room)
            addCrackQuad(c3, c7, c6, c2, normFront);
            // 2. Back face (facing wall)
            addCrackQuad(c1, c5, c4, c0, normBack);
            // 3. Left edge
            addCrackQuad(c0, c4, c7, c3, normLeft);
            // 4. Right edge
            addCrackQuad(c2, c6, c5, c1, normRight);
            // 5. Top edge
            addCrackQuad(c7, c4, c5, c6, glm::vec3(0, 1, 0));
            // 6. Bottom edge
            addCrackQuad(c3, c2, c1, c0, glm::vec3(0, -1, 0));
        }
    }
    // 3. Beds (True Parallelogram Mattress & Skirt matching ChunkMesher)
    else if (cell.isBed()) {
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

        glm::vec2 P_centF = (V0_F + V2_F) * 0.5f;
        glm::vec2 P_centH = (V0_H + V2_H) * 0.5f;
        float shrink = 0.02f;

        glm::vec2 C0_F = V0_F + glm::normalize(P_centF - V0_F) * shrink;
        glm::vec2 C1_F = V1_F + glm::normalize(P_centF - V1_F) * shrink;
        glm::vec2 C2_F = V2_F + glm::normalize(P_centF - V2_F) * shrink;
        glm::vec2 C3_F = V3_F + glm::normalize(P_centF - V3_F) * shrink;

        glm::vec2 C0_H = V0_H + glm::normalize(P_centH - V0_H) * shrink;
        glm::vec2 C1_H = V1_H + glm::normalize(P_centH - V1_H) * shrink;
        glm::vec2 C2_H = V2_H + glm::normalize(P_centH - V2_H) * shrink;
        glm::vec2 C3_H = V3_H + glm::normalize(P_centH - V3_H) * shrink;

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

        // Top mattress quads
        addCrackTri(M_h0, M_f1, M_h1, uvT0, uvT2, uvT1, glm::vec3(0, 1, 0));
        addCrackTri(M_h0, M_f0, M_f1, uvT0, uvT1, uvT2, glm::vec3(0, 1, 0));

        // Underside quads
        addCrackTri(B_h0, B_h1, B_f1, uvB0, uvB1, uvB2, glm::vec3(0, -1, 0));
        addCrackTri(B_h0, B_f1, B_f0, uvB0, uvB2, uvB1, glm::vec3(0, -1, 0));

        // Headboard & Footboard
        glm::vec2 eHead = glm::normalize(P_head1 - P_head0);
        glm::vec3 nHead(eHead.y, 0.0f, -eHead.x);
        addCrackQuad(B_h0, M_h0, M_h1, B_h1, nHead);

        glm::vec2 eFoot = glm::normalize(P_foot0 - P_foot1);
        glm::vec3 nFoot(eFoot.y, 0.0f, -eFoot.x);
        addCrackQuad(B_f1, M_f1, M_f0, B_f0, nFoot);

        // Side skirts
        glm::vec2 eSideR = glm::normalize(P_foot1 - P_head1);
        glm::vec3 nSideR(eSideR.y, 0.0f, -eSideR.x);
        addCrackQuad(B_h1, M_h1, M_f1, B_f1, nSideR);

        glm::vec2 eSideL = glm::normalize(P_head0 - P_foot0);
        glm::vec3 nSideL(eSideL.y, 0.0f, -eSideL.x);
        addCrackQuad(B_f0, M_f0, M_h0, B_h0, nSideL);
    }
    // 4. Torches (Slender post with upright or 21-deg wall mount)
    else if (cell.isTorch()) {
        glm::vec2 vXZ[3];
        getPrismVerticesXZ(wx, wz, s, vXZ);
        glm::vec2 cent = (vXZ[0] + vXZ[1] + vXZ[2]) / 3.0f;

        uint8_t mount = cell.level;
        glm::vec3 pBase;
        glm::vec3 torchAxis(0.0f, 1.0f, 0.0f);
        glm::vec3 frontDir(0.0f, 0.0f, 1.0f);
        glm::vec3 sideDir(1.0f, 0.0f, 0.0f);

        if (mount == 0) {
            pBase = glm::vec3(cent.x, py, cent.y);
        } else {
            glm::vec2 wallMid = (mount == 1) ? ((vXZ[0] + vXZ[1]) * 0.5f)
                              : (mount == 2) ? ((vXZ[0] + vXZ[2]) * 0.5f)
                                             : ((vXZ[1] + vXZ[2]) * 0.5f);
            glm::vec2 outward = glm::normalize(cent - wallMid);
            pBase = glm::vec3(wallMid.x, py + 0.20f, wallMid.y) + glm::vec3(outward.x, 0.0f, outward.y) * 0.08f;
            float tiltAngle = 0.37f;
            torchAxis = glm::normalize(glm::vec3(outward.x * std::sin(tiltAngle), std::cos(tiltAngle), outward.y * std::sin(tiltAngle)));
            sideDir = glm::normalize(glm::vec3(-outward.y, 0.0f, outward.x));
            frontDir = glm::cross(sideDir, torchAxis);
        }

        float hw = 0.045f;
        float hTotal = 0.58f;

        auto getP = [&](float dx, float dy, float dz) -> glm::vec3 {
            return pBase + sideDir * dx + torchAxis * dy + frontDir * dz;
        };

        addCrackQuad(getP(-hw, 0.0f,  hw), getP(-hw, hTotal,  hw), getP( hw, hTotal,  hw), getP( hw, 0.0f,  hw), frontDir);
        addCrackQuad(getP( hw, 0.0f, -hw), getP( hw, hTotal, -hw), getP(-hw, hTotal, -hw), getP(-hw, 0.0f, -hw), -frontDir);
        addCrackQuad(getP( hw, 0.0f,  hw), getP( hw, hTotal,  hw), getP( hw, hTotal, -hw), getP( hw, 0.0f, -hw), sideDir);
        addCrackQuad(getP(-hw, 0.0f, -hw), getP(-hw, hTotal, -hw), getP(-hw, hTotal,  hw), getP(-hw, 0.0f,  hw), -sideDir);
        addCrackQuad(getP(-hw, hTotal, -hw), getP(-hw, hTotal,  hw), getP( hw, hTotal,  hw), getP( hw, hTotal, -hw), torchAxis);
    }
    // 5. Lanterns (Floor standing or ceiling hanging)
    else if (cell.isLantern()) {
        glm::vec2 vXZ[3];
        getPrismVerticesXZ(wx, wz, s, vXZ);
        glm::vec2 cent = (vXZ[0] + vXZ[1] + vXZ[2]) / 3.0f;
        float halfW = 0.17f;
        float hBody = 0.44f;
        bool isHanging = (cell.level == 1);
        float yBottom = isHanging ? (py + 1.0f - hBody - 0.12f) : py;
        float yTop = yBottom + hBody;

        addCrackQuad(glm::vec3(cent.x - halfW, yBottom, cent.y + halfW),
                     glm::vec3(cent.x - halfW, yTop,    cent.y + halfW),
                     glm::vec3(cent.x + halfW, yTop,    cent.y + halfW),
                     glm::vec3(cent.x + halfW, yBottom, cent.y + halfW), glm::vec3(0, 0, 1));
        addCrackQuad(glm::vec3(cent.x + halfW, yBottom, cent.y - halfW),
                     glm::vec3(cent.x + halfW, yTop,    cent.y - halfW),
                     glm::vec3(cent.x - halfW, yTop,    cent.y - halfW),
                     glm::vec3(cent.x - halfW, yBottom, cent.y - halfW), glm::vec3(0, 0, -1));
        addCrackQuad(glm::vec3(cent.x + halfW, yBottom, cent.y + halfW),
                     glm::vec3(cent.x + halfW, yTop,    cent.y + halfW),
                     glm::vec3(cent.x + halfW, yTop,    cent.y - halfW),
                     glm::vec3(cent.x + halfW, yBottom, cent.y - halfW), glm::vec3(1, 0, 0));
        addCrackQuad(glm::vec3(cent.x - halfW, yBottom, cent.y - halfW),
                     glm::vec3(cent.x - halfW, yTop,    cent.y - halfW),
                     glm::vec3(cent.x - halfW, yTop,    cent.y + halfW),
                     glm::vec3(cent.x - halfW, yBottom, cent.y + halfW), glm::vec3(-1, 0, 0));
        addCrackQuad(glm::vec3(cent.x - halfW, yTop, cent.y + halfW),
                     glm::vec3(cent.x + halfW, yTop, cent.y + halfW),
                     glm::vec3(cent.x + halfW, yTop, cent.y - halfW),
                     glm::vec3(cent.x - halfW, yTop, cent.y - halfW), glm::vec3(0, 1, 0));
    }
    // 6. Cake (Partial height with perimeter inset)
    else if (cell.type == BlockType::Cake) {
        glm::vec2 vXZ[3];
        getPrismVerticesXZ(wx, wz, s, vXZ);
        float hCake = 0.4375f;
        glm::vec2 cent = (vXZ[0] + vXZ[1] + vXZ[2]) / 3.0f;
        float inset = 0.0625f;

        glm::vec2 cv[3];
        for (int i = 0; i < 3; ++i) {
            cv[i] = vXZ[i] + glm::normalize(cent - vXZ[i]) * inset;
        }

        glm::vec3 b0(cv[0].x, py, cv[0].y);
        glm::vec3 b1(cv[1].x, py, cv[1].y);
        glm::vec3 b2(cv[2].x, py, cv[2].y);

        glm::vec3 t0(cv[0].x, py + hCake, cv[0].y);
        glm::vec3 t1(cv[1].x, py + hCake, cv[1].y);
        glm::vec3 t2(cv[2].x, py + hCake, cv[2].y);

        addCrackTri(t0, t2, t1, uvT0, uvT2, uvT1, glm::vec3(0, 1, 0));
        addCrackTri(b0, b1, b2, uvB0, uvB1, uvB2, glm::vec3(0, -1, 0));
        addCrackQuad(b0, t0, t1, b1, glm::vec3(0, 0, 1));
        addCrackQuad(b1, t1, t2, b2, glm::vec3(1, 0, 0));
        addCrackQuad(b2, t2, t0, b0, glm::vec3(-1, 0, 0));
    }
    // 7. Foliage / Flowers / Saplings (Crossed diagonal cutout quads)
    else if (cell.isFoliage()) {
        glm::vec2 vXZ[3];
        getPrismVerticesXZ(wx, wz, s, vXZ);
        glm::vec2 cent = (vXZ[0] + vXZ[1] + vXZ[2]) / 3.0f;

        glm::vec3 q0(vXZ[0].x, py, vXZ[0].y);
        glm::vec3 q1(vXZ[1].x, py, vXZ[1].y);
        glm::vec3 q2(vXZ[1].x, py + 1.0f, vXZ[1].y);
        glm::vec3 q3(vXZ[0].x, py + 1.0f, vXZ[0].y);

        glm::vec3 q4(vXZ[2].x, py, vXZ[2].y);
        glm::vec3 q5(cent.x,   py, cent.y);
        glm::vec3 q6(cent.x,   py + 1.0f, cent.y);
        glm::vec3 q7(vXZ[2].x, py + 1.0f, vXZ[2].y);

        addCrackQuad(q0, q1, q2, q3, glm::vec3(0, 0, 1));
        addCrackQuad(q1, q0, q3, q2, glm::vec3(0, 0, -1));
        addCrackQuad(q4, q5, q6, q7, glm::vec3(1, 0, 0));
        addCrackQuad(q5, q4, q7, q6, glm::vec3(-1, 0, 0));
    }
    // 7b. Cactus (14x14 proportions, inset by 0.0625m)
    else if (cell.isCactus()) {
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

        addCrackTri(t0, t2, t1, uvT0, uvT2, uvT1, glm::vec3(0, 1, 0));
        addCrackTri(b0, b1, b2, uvB0, uvB1, uvB2, glm::vec3(0, -1, 0));

        glm::vec2 e0 = cXZ[1] - cXZ[0];
        glm::vec3 n0 = glm::normalize(glm::vec3(e0.y, 0.0f, -e0.x));
        addCrackQuad(b0, b1, t1, t0, n0);

        glm::vec2 e1 = cXZ[2] - cXZ[1];
        glm::vec3 n1 = glm::normalize(glm::vec3(e1.y, 0.0f, -e1.x));
        addCrackQuad(b1, b2, t2, t1, n1);

        glm::vec2 e2 = cXZ[0] - cXZ[2];
        glm::vec3 n2 = glm::normalize(glm::vec3(e2.y, 0.0f, -e2.x));
        addCrackQuad(b2, b0, t0, t2, n2);
    }
    // 8. Standard Solid Blocks (Equilateral triangular prism)
    else {

        glm::vec2 vXZ[3];
        getPrismVerticesXZ(wx, wz, s, vXZ);

        glm::vec3 B0(vXZ[0].x, py, vXZ[0].y);
        glm::vec3 B1(vXZ[1].x, py, vXZ[1].y);
        glm::vec3 B2(vXZ[2].x, py, vXZ[2].y);

        glm::vec3 T0(vXZ[0].x, py + 1.0f, vXZ[0].y);
        glm::vec3 T1(vXZ[1].x, py + 1.0f, vXZ[1].y);
        glm::vec3 T2(vXZ[2].x, py + 1.0f, vXZ[2].y);

        addCrackTri(T0, T2, T1, uvT0, uvT2, uvT1, glm::vec3(0.0f, 1.0f, 0.0f));
        addCrackTri(B0, B1, B2, uvB0, uvB1, uvB2, glm::vec3(0.0f, -1.0f, 0.0f));

        glm::vec3 nBase  = (s == 0) ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 nLeft  = (s == 0) ? glm::vec3(-SQRT_3_OVER_2, 0.0f, 0.5f) : glm::vec3(-SQRT_3_OVER_2, 0.0f, -0.5f);
        glm::vec3 nRight = (s == 0) ? glm::vec3(SQRT_3_OVER_2, 0.0f, 0.5f) : glm::vec3(SQRT_3_OVER_2, 0.0f, -0.5f);

        if (s == 0) {
            addCrackQuad(B0, T0, T1, B1, nBase);
            addCrackQuad(B2, T2, T0, B0, nLeft);
            addCrackQuad(B1, T1, T2, B2, nRight);
        } else {
            addCrackQuad(B1, T1, T2, B2, nBase);
            addCrackQuad(B2, T2, T0, B0, nLeft);
            addCrackQuad(B1, T1, T0, B0, nRight);
        }
    }

    if (vertices.empty()) {
        m_indexCount[frameIndex] = 0;
        return;
    }

    VkDeviceSize vSize = vertices.size() * sizeof(ChunkVertex);
    VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    if (!m_vbo[frameIndex].isValid() || vSize > m_vbo[frameIndex].getSize()) {
        VkDeviceSize newSize = std::max(vSize, static_cast<VkDeviceSize>(64 * sizeof(ChunkVertex)));
        m_vbo[frameIndex] = Buffer(m_context, newSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    if (!m_ibo[frameIndex].isValid() || iSize > m_ibo[frameIndex].getSize()) {
        VkDeviceSize newSize = std::max(iSize, static_cast<VkDeviceSize>(96 * sizeof(uint32_t)));
        m_ibo[frameIndex] = Buffer(m_context, newSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    m_vbo[frameIndex].upload(vertices.data(), vSize);
    m_ibo[frameIndex].upload(indices.data(), iSize);
    m_indexCount[frameIndex] = static_cast<uint32_t>(indices.size());

    m_lastX[frameIndex] = coord.x;
    m_lastY[frameIndex] = coord.y;
    m_lastZ[frameIndex] = coord.z;
    m_lastS[frameIndex] = coord.s;
    m_lastStage[frameIndex] = stage;
    m_lastDoorState[frameIndex] = cell.level;
}

void BlockCrackRenderer::render(VkCommandBuffer cmd,
                                const Pipeline& pipeline,
                                VkDescriptorSet descSet,
                                const World& world,
                                const std::optional<CellCoord>& targetCell,
                                int crackStage,
                                const glm::mat4& vpMatrix,
                                const PushConstants& pc) {
    if (!targetCell.has_value() || crackStage < 0) return;

    const CellCoord& c = targetCell.value();
    Cell cell = world.getCell(c.x, c.y, c.z, c.s);
    if (cell.type == BlockType::Air) return;

    uint32_t f = m_cmdQueue.getCurrentFrame();

    if (c.x != m_lastX[f] || c.y != m_lastY[f] || c.z != m_lastZ[f] || c.s != m_lastS[f] ||
        crackStage != m_lastStage[f] || cell.level != m_lastDoorState[f] || !m_vbo[f].isValid()) {
        rebuildBlockMesh(world, c, crackStage, f);
    }

    if (m_indexCount[f] == 0 || !m_vbo[f].isValid()) return;

    pipeline.bind(cmd);
    if (descSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getLayout(), 0, 1, &descSet, 0, nullptr);
    }

    PushConstants crackPC = pc;
    std::memcpy(crackPC.mvp, &vpMatrix[0][0], sizeof(float) * 16);
    // Bit 64 indicates multiplicative crack overlay pass to cell.frag
    int crackFlags = static_cast<int>(crackPC.shaderOptions[3] + 0.5f) | 64;
    crackPC.shaderOptions[3] = static_cast<float>(crackFlags);

    vkCmdPushConstants(cmd, pipeline.getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &crackPC);

    VkBuffer vbs[] = {m_vbo[f].getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_ibo[f].getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_indexCount[f], 1, 0, 0, 0);
}

} // namespace prismcraft

