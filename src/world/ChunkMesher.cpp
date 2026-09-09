#include "ChunkMesher.hpp"
#include "Chunk.hpp"
#include "Cell.hpp"
#include "Coordinates.hpp"
#include "world/World.hpp"
#include "world/TerrainGen.hpp"
#include "renderer/TextureAtlas.hpp"
#include <algorithm>

namespace prismcraft {

ChunkMesh ChunkMesher::generateMesh(const Chunk& chunk,
    const Chunk* northNeighbor,
    const Chunk* southNeighbor,
    const Chunk* westNeighbor,
    const Chunk* eastNeighbor,
    const World* world) {
    
    ChunkMesh mesh;
    
    int curCX = chunk.getCoord().cx;
    int curCZ = chunk.getCoord().cz;

    auto getCellAtWorld = [&](int gx, int gy, int gz, int gs) -> Cell {
        if (gy < 0 || gy >= CHUNK_SIZE_Y) return Cell{BlockType::Air};
        ChunkCoord c = worldToChunk(gx, gz);
        int lx, lz;
        worldToLocal(gx, gz, lx, lz);

        if (c.cx == curCX && c.cz == curCZ) {
            return chunk.getCell(lx, gy, lz, gs);
        } else if (c.cx == curCX && c.cz == curCZ - 1) {
            return northNeighbor ? northNeighbor->getCell(lx, gy, lz, gs) : Cell{BlockType::Air};
        } else if (c.cx == curCX && c.cz == curCZ + 1) {
            return southNeighbor ? southNeighbor->getCell(lx, gy, lz, gs) : Cell{BlockType::Air};
        } else if (c.cx == curCX - 1 && c.cz == curCZ) {
            return westNeighbor ? westNeighbor->getCell(lx, gy, lz, gs) : Cell{BlockType::Air};
        } else if (c.cx == curCX + 1 && c.cz == curCZ) {
            return eastNeighbor ? eastNeighbor->getCell(lx, gy, lz, gs) : Cell{BlockType::Air};
        } else if (world) {
            return world->getCell(gx, gy, gz, gs);
        }
        return Cell{BlockType::Air};
    };

    int worldX = chunk.getWorldX();
    int worldZ = chunk.getWorldZ();

    // -------------------------------------------------------------
    // Geometry Appenders for Opaque and Translucent Water Passes
    // -------------------------------------------------------------
    auto addOpaqueQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                             const glm::vec4& uv, const glm::vec3& normal,
                             const glm::vec3& c0, const glm::vec3& c1, const glm::vec3& c2, const glm::vec3& c3) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.opaqueVertices.size());
        mesh.opaqueVertices.push_back({v0, glm::vec2(uv.x, uv.w), normal, c0});
        mesh.opaqueVertices.push_back({v1, glm::vec2(uv.x, uv.y), normal, c1});
        mesh.opaqueVertices.push_back({v2, glm::vec2(uv.z, uv.y), normal, c2});
        mesh.opaqueVertices.push_back({v3, glm::vec2(uv.z, uv.w), normal, c3});
        mesh.opaqueIndices.push_back(baseIdx + 0);
        mesh.opaqueIndices.push_back(baseIdx + 1);
        mesh.opaqueIndices.push_back(baseIdx + 2);
        mesh.opaqueIndices.push_back(baseIdx + 2);
        mesh.opaqueIndices.push_back(baseIdx + 3);
        mesh.opaqueIndices.push_back(baseIdx + 0);
    };

    auto addOpaqueTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                            const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                            const glm::vec3& normal,
                            const glm::vec3& c0, const glm::vec3& c1, const glm::vec3& c2) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.opaqueVertices.size());
        mesh.opaqueVertices.push_back({v0, uv0, normal, c0});
        mesh.opaqueVertices.push_back({v1, uv1, normal, c1});
        mesh.opaqueVertices.push_back({v2, uv2, normal, c2});
        mesh.opaqueIndices.push_back(baseIdx + 0);
        mesh.opaqueIndices.push_back(baseIdx + 1);
        mesh.opaqueIndices.push_back(baseIdx + 2);
    };

    auto addWaterQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                            const glm::vec4& uv, const glm::vec3& normal,
                            const glm::vec3& c0, const glm::vec3& c1, const glm::vec3& c2, const glm::vec3& c3) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.waterVertices.size());
        mesh.waterVertices.push_back({v0, glm::vec2(uv.x, uv.w), normal, c0});
        mesh.waterVertices.push_back({v1, glm::vec2(uv.x, uv.y), normal, c1});
        mesh.waterVertices.push_back({v2, glm::vec2(uv.z, uv.y), normal, c2});
        mesh.waterVertices.push_back({v3, glm::vec2(uv.z, uv.w), normal, c3});
        mesh.waterIndices.push_back(baseIdx + 0);
        mesh.waterIndices.push_back(baseIdx + 1);
        mesh.waterIndices.push_back(baseIdx + 2);
        mesh.waterIndices.push_back(baseIdx + 2);
        mesh.waterIndices.push_back(baseIdx + 3);
        mesh.waterIndices.push_back(baseIdx + 0);
    };

    auto addWaterTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                           const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                           const glm::vec3& normal,
                           const glm::vec3& c0, const glm::vec3& c1, const glm::vec3& c2) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.waterVertices.size());
        mesh.waterVertices.push_back({v0, uv0, normal, c0});
        mesh.waterVertices.push_back({v1, uv1, normal, c1});
        mesh.waterVertices.push_back({v2, uv2, normal, c2});
        mesh.waterIndices.push_back(baseIdx + 0);
        mesh.waterIndices.push_back(baseIdx + 1);
        mesh.waterIndices.push_back(baseIdx + 2);
    };

    const glm::vec3 nTop(0.0f, 1.0f, 0.0f);
    const glm::vec3 nBot(0.0f, -1.0f, 0.0f);

    // Precompute top-most opaque voxel per column for block-to-block directional shadows
    int maxOpaqueY[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    std::vector<glm::vec3> chunkTorches;

    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        int wx = worldX + x;
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            int wz = worldZ + z;
            maxOpaqueY[x][z] = -1;
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
                Cell c0 = chunk.getCell(x, y, z, 0);
                Cell c1 = chunk.getCell(x, y, z, 1);
                if (c0.isTorch()) chunkTorches.push_back(cellToWorldCenter(wx, y, wz, 0));
                if (c1.isTorch()) chunkTorches.push_back(cellToWorldCenter(wx, y, wz, 1));
                if (maxOpaqueY[x][z] < 0 && (c0.isOpaque() || c1.isOpaque() || c0.type == BlockType::Leaves || c1.type == BlockType::Leaves)) {
                    maxOpaqueY[x][z] = y;
                }
            }
        }
    }

    auto getSunlightFactor = [&](int x, int y, int z) -> float {
        int wx = worldX + x;
        int wz = worldZ + z;

        if (y >= CHUNK_SIZE_Y - 1) return 1.0f;

        // 1. Column shadow with gradual diffusion under overhangs / ceilings
        int roofY = (x >= 0 && x < CHUNK_SIZE_X && z >= 0 && z < CHUNK_SIZE_Z) ? maxOpaqueY[x][z] : -1;
        if (roofY < 0) {
            for (int sy = CHUNK_SIZE_Y - 1; sy >= y; --sy) {
                Cell sc0 = getCellAtWorld(wx, sy, wz, 0);
                Cell sc1 = getCellAtWorld(wx, sy, wz, 1);
                if (sc0.isOpaque() || sc1.isOpaque() || sc0.type == BlockType::Leaves || sc1.type == BlockType::Leaves) {
                    roofY = sy;
                    break;
                }
            }
        }

        if (roofY > y) {
            int depth = roofY - y;
            return std::max(0.08f, 0.90f - depth * 0.16f); // Gradual skylight decay under overhangs
        }

        // 2. Fast directional sunlight shadow check from sun elevation
        for (int step = 1; step <= 3; ++step) {
            int sy = y + step;
            if (sy >= CHUNK_SIZE_Y) break;
            int sx = x + step;
            int sz = z + step;
            if (sx >= 0 && sx < CHUNK_SIZE_X && sz >= 0 && sz < CHUNK_SIZE_Z) {
                if (maxOpaqueY[sx][sz] >= sy) {
                    return 0.12f; // Terrain blocks the sun at this angle
                }
            } else {
                Cell sc0 = getCellAtWorld(wx + step, sy, wz + step, 0);
                Cell sc1 = getCellAtWorld(wx + step, sy, wz + step, 1);
                if (sc0.isOpaque() || sc1.isOpaque() || sc0.type == BlockType::Leaves || sc1.type == BlockType::Leaves) {
                    return 0.12f;
                }
            }
        }

        return 1.0f;
    };

    auto getTorchLight = [&](const glm::vec3& faceCenter) -> float {
        float torchLight = 0.0f;
        for (const auto& tp : chunkTorches) {
            float dist = glm::distance(faceCenter, tp);
            if (dist < 8.0f) {
                float atten = 1.0f - dist / 8.0f;
                torchLight += atten * atten * 1.5f;
            }
        }
        return std::min(1.0f, torchLight);
    };

    // Minecraft Smooth Lighting: Sample light at vertex corner offset into free space
    auto getVertexLight = [&](const glm::vec3& V, const glm::vec3& faceNormal) -> std::pair<float, float> {
        glm::vec3 probePos = V + faceNormal * 0.12f;
        CellCoord sc = worldToCell(probePos);
        float sun = getSunlightFactor(sc.x - worldX, sc.y, sc.z - worldZ);
        float torch = getTorchLight(probePos);
        return {sun, torch};
    };

    // -------------------------------------------------------------
    // Minecraft Smooth Lighting: Per-Vertex Ambient Occlusion
    // -------------------------------------------------------------
    // 1. Top face vertex AO: probes 6 directions in the horizontal plane around the corner
    auto getTopVertexAO = [&](const glm::vec3& V, int y) -> float {
        int occluded = 0;
        static const float cosSin6[6][2] = {
            { 1.0f, 0.0f },
            { 0.5f, 0.8660254f },
            { -0.5f, 0.8660254f },
            { -1.0f, 0.0f },
            { -0.5f, -0.8660254f },
            { 0.5f, -0.8660254f }
        };
        for (int i = 0; i < 6; ++i) {
            glm::vec3 probePos(V.x + cosSin6[i][0] * 0.35f, static_cast<float>(y) + 1.25f, V.z + cosSin6[i][1] * 0.35f);
            CellCoord sc = worldToCell(probePos);
            Cell scell = getCellAtWorld(sc.x, sc.y, sc.z, sc.s);
            if (scell.isOpaque() || scell.type == BlockType::Leaves) {
                occluded++;
            }
        }
        if (occluded == 0) return 1.00f;
        if (occluded == 1) return 0.82f;
        if (occluded == 2) return 0.65f;
        if (occluded == 3) return 0.52f;
        return 0.42f;
    };

    // 2. Bottom face vertex AO: probes below ceiling
    auto getBotVertexAO = [&](const glm::vec3& V, int y) -> float {
        int occluded = 0;
        static const float cosSin6[6][2] = {
            { 1.0f, 0.0f },
            { 0.5f, 0.8660254f },
            { -0.5f, 0.8660254f },
            { -1.0f, 0.0f },
            { -0.5f, -0.8660254f },
            { 0.5f, -0.8660254f }
        };
        for (int i = 0; i < 6; ++i) {
            glm::vec3 probePos(V.x + cosSin6[i][0] * 0.35f, static_cast<float>(y) - 0.25f, V.z + cosSin6[i][1] * 0.35f);
            CellCoord sc = worldToCell(probePos);
            Cell scell = getCellAtWorld(sc.x, sc.y, sc.z, sc.s);
            if (scell.isOpaque() || scell.type == BlockType::Leaves) {
                occluded++;
            }
        }
        if (occluded == 0) return 1.00f;
        if (occluded == 1) return 0.82f;
        if (occluded == 2) return 0.65f;
        if (occluded == 3) return 0.52f;
        return 0.42f;
    };

    // 3. Wall quad vertex AO: probes floor/ceiling contact and lateral corner seams
    auto getWallVertexAO = [&](const glm::vec3& V, const glm::vec3& wallNorm, const glm::vec3& tang, bool isBottom, float lateralSign) -> float {
        glm::vec3 Pout = V + wallNorm * 0.08f;
        int occluded = 0;

        // Test vertical contact (floor or ceiling)
        glm::vec3 vertProbe = Pout + glm::vec3(0.0f, isBottom ? -0.4f : 0.4f, 0.0f);
        Cell scVert = getCellAtWorld(worldToCell(vertProbe).x, worldToCell(vertProbe).y, worldToCell(vertProbe).z, worldToCell(vertProbe).s);
        if (scVert.isOpaque() || scVert.type == BlockType::Leaves) occluded++;

        // Test lateral corner seam
        glm::vec3 latProbe = Pout + tang * (lateralSign * 0.38f);
        Cell scLat = getCellAtWorld(worldToCell(latProbe).x, worldToCell(latProbe).y, worldToCell(latProbe).z, worldToCell(latProbe).s);
        if (scLat.isOpaque() || scLat.type == BlockType::Leaves) occluded++;

        // Test diagonal corner
        glm::vec3 diagProbe = Pout + tang * (lateralSign * 0.38f) + glm::vec3(0.0f, isBottom ? -0.4f : 0.4f, 0.0f);
        Cell scDiag = getCellAtWorld(worldToCell(diagProbe).x, worldToCell(diagProbe).y, worldToCell(diagProbe).z, worldToCell(diagProbe).s);
        if (scDiag.isOpaque() || scDiag.type == BlockType::Leaves) occluded++;

        if (occluded == 0) return 1.00f;
        if (occluded == 1) return 0.78f;
        if (occluded == 2) return 0.58f;
        return 0.42f;
    };

    auto shouldDrawFace = [&](Cell c, Cell n) -> bool {
        if (!n.isTransparent()) return false;
        if (c.type == BlockType::Water && n.type == BlockType::Water) return false;
        if (c.type == BlockType::Leaves && n.type == BlockType::Leaves) return true; // Fancy leaves: draw transparent cutouts!
        return true;
    };

    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        int wx = worldX + x;
        for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                int wz = worldZ + z;
                for (int s = 0; s < 2; ++s) {
                    Cell cell = chunk.getCell(x, y, z, s);
                    if (cell.type == BlockType::Air) continue;

                    // 1. Foliage & Torch Rendering (Crossed Quads with alpha cutout)
                    if (cell.isFoliage() || cell.isTorch()) {
                        glm::vec3 center = cellToWorldCenter(wx, y, wz, s);
                        center.y = static_cast<float>(y); // Base at ground level
                        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 0));
                        float hw = cell.isTorch() ? 0.16f : 0.38f;
                        float fh = cell.isTorch() ? 0.62f : 0.85f;
                        float sunlight = getSunlightFactor(x, y, z);
                        float torchL = getTorchLight(center);
                        glm::vec3 folColor = cell.isTorch() ? glm::vec3(1.0f, 1.0f, 1.0f) : glm::vec3(sunlight, 1.0f, torchL);

                        // Diagonal quad 1 (Double sided)
                        addOpaqueQuad(center + glm::vec3(-hw, 0.0f, -hw), center + glm::vec3(-hw, fh, -hw),
                                      center + glm::vec3(hw, fh, hw), center + glm::vec3(hw, 0.0f, hw),
                                      uv, nTop, folColor, folColor, folColor, folColor);
                        addOpaqueQuad(center + glm::vec3(hw, 0.0f, hw), center + glm::vec3(hw, fh, hw),
                                      center + glm::vec3(-hw, fh, -hw), center + glm::vec3(-hw, 0.0f, -hw),
                                      uv, nTop, folColor, folColor, folColor, folColor);

                        // Diagonal quad 2 (Double sided)
                        addOpaqueQuad(center + glm::vec3(-hw, 0.0f, hw), center + glm::vec3(-hw, fh, hw),
                                      center + glm::vec3(hw, fh, -hw), center + glm::vec3(hw, 0.0f, -hw),
                                      uv, nTop, folColor, folColor, folColor, folColor);
                        addOpaqueQuad(center + glm::vec3(hw, 0.0f, -hw), center + glm::vec3(hw, fh, -hw),
                                      center + glm::vec3(-hw, fh, hw), center + glm::vec3(-hw, 0.0f, hw),
                                      uv, nTop, folColor, folColor, folColor, folColor);
                        continue;
                    }

                    // 2. Regular Equilateral Triangular Prism
                    glm::vec2 vXZ[3];
                    getPrismVerticesXZ(wx, wz, s, vXZ);

                    float py = static_cast<float>(y);
                    glm::vec3 B0(vXZ[0].x, py, vXZ[0].y);
                    glm::vec3 B1(vXZ[1].x, py, vXZ[1].y);
                    glm::vec3 B2(vXZ[2].x, py, vXZ[2].y);

                    glm::vec3 T0(vXZ[0].x, py + 1.0f, vXZ[0].y);
                    glm::vec3 T1(vXZ[1].x, py + 1.0f, vXZ[1].y);
                    glm::vec3 T2(vXZ[2].x, py + 1.0f, vXZ[2].y);

                    glm::vec3 blockCenter = cellToWorldCenter(wx, y, wz, s);

                    CellCoord neighbors[5];
                    getNeighbors(wx, y, wz, s, neighbors);

                    bool isWaterBlock = (cell.type == BlockType::Water);

                    // Compute water column depth smoothly per vertex for Beer-Lambert absorption in water shader
                    auto getWaterDepthAt = [&](float wx_f, float wz_f, int ySurf) -> float {
                        CellCoord cc = worldToCell(glm::vec3(wx_f, static_cast<float>(ySurf), wz_f));
                        int d = 1;
                        while (ySurf - d >= 0) {
                            Cell c = getCellAtWorld(cc.x, ySurf - d, cc.z, cc.s);
                            if (c.type != BlockType::Water) break;
                            d++;
                        }
                        return std::clamp(static_cast<float>(d) / 7.0f, 0.1f, 1.0f);
                    };

                    // ---------------------------------------------------------
                    // Face 0: Top face (+Y)
                    // ---------------------------------------------------------
                    Cell n0 = getCellAtWorld(neighbors[0].x, neighbors[0].y, neighbors[0].z, neighbors[0].s);
                    if (shouldDrawFace(cell, n0)) {
                        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 0));
                        float uMid = uv.x + (uv.z - uv.x) * 0.5f;
                        glm::vec2 uvT0, uvT1, uvT2;
                        if (s == 0) {
                            uvT0 = glm::vec2(uv.x, uv.w);
                            uvT1 = glm::vec2(uv.z, uv.w);
                            uvT2 = glm::vec2(uMid, uv.y);
                        } else {
                            uvT0 = glm::vec2(uMid, uv.w);
                            uvT1 = glm::vec2(uv.z, uv.y);
                            uvT2 = glm::vec2(uv.x, uv.y);
                        }

                        auto [sun0, t0] = getVertexLight(T0, nTop);
                        auto [sun1, t1] = getVertexLight(T1, nTop);
                        auto [sun2, t2] = getVertexLight(T2, nTop);

                        if (isWaterBlock) {
                            float d0 = getWaterDepthAt(vXZ[0].x, vXZ[0].y, y);
                            float d1 = getWaterDepthAt(vXZ[1].x, vXZ[1].y, y);
                            float d2 = getWaterDepthAt(vXZ[2].x, vXZ[2].y, y);
                            glm::vec3 waterCol0(d0, sun0, t0);
                            glm::vec3 waterCol1(d1, sun1, t1);
                            glm::vec3 waterCol2(d2, sun2, t2);
                            addWaterTri(T0, T2, T1, uvT0, uvT2, uvT1, nTop, waterCol0, waterCol2, waterCol1);
                        } else {
                            bool isSubmerged = (n0.type == BlockType::Water);
                            float ao0 = getTopVertexAO(T0, y);
                            float ao1 = getTopVertexAO(T1, y);
                            float ao2 = getTopVertexAO(T2, y);

                            glm::vec3 c0(sun0, 1.0f * ao0, isSubmerged ? (t0 + 2.0f) : t0);
                            glm::vec3 c1(sun1, 1.0f * ao1, isSubmerged ? (t1 + 2.0f) : t1);
                            glm::vec3 c2(sun2, 1.0f * ao2, isSubmerged ? (t2 + 2.0f) : t2);

                            addOpaqueTri(T0, T2, T1, uvT0, uvT2, uvT1, nTop, c0, c2, c1);
                        }
                    }

                    // ---------------------------------------------------------
                    // Face 1: Bottom face (-Y)
                    // ---------------------------------------------------------
                    Cell n1 = getCellAtWorld(neighbors[1].x, neighbors[1].y, neighbors[1].z, neighbors[1].s);
                    if (shouldDrawFace(cell, n1)) {
                        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 1));
                        float uMidB = uv.x + (uv.z - uv.x) * 0.5f;
                        glm::vec2 uvB0, uvB1, uvB2;
                        if (s == 0) {
                            uvB0 = glm::vec2(uv.x, uv.w);
                            uvB1 = glm::vec2(uv.z, uv.w);
                            uvB2 = glm::vec2(uMidB, uv.y);
                        } else {
                            uvB0 = glm::vec2(uMidB, uv.w);
                            uvB1 = glm::vec2(uv.z, uv.y);
                            uvB2 = glm::vec2(uv.x, uv.y);
                        }

                        auto [sun0, t0] = getVertexLight(B0, nBot);
                        auto [sun1, t1] = getVertexLight(B1, nBot);
                        auto [sun2, t2] = getVertexLight(B2, nBot);

                        if (isWaterBlock) {
                            float d0 = getWaterDepthAt(vXZ[0].x, vXZ[0].y, y);
                            float d1 = getWaterDepthAt(vXZ[1].x, vXZ[1].y, y);
                            float d2 = getWaterDepthAt(vXZ[2].x, vXZ[2].y, y);
                            glm::vec3 waterCol0(d0, sun0, t0);
                            glm::vec3 waterCol1(d1, sun1, t1);
                            glm::vec3 waterCol2(d2, sun2, t2);
                            addWaterTri(B0, B1, B2, uvB0, uvB1, uvB2, nBot, waterCol0, waterCol1, waterCol2);
                        } else {
                            bool isSubmerged = (n1.type == BlockType::Water);
                            float ao0 = getBotVertexAO(B0, y);
                            float ao1 = getBotVertexAO(B1, y);
                            float ao2 = getBotVertexAO(B2, y);

                            glm::vec3 c0(sun0, 0.55f * ao0, isSubmerged ? (t0 + 2.0f) : t0);
                            glm::vec3 c1(sun1, 0.55f * ao1, isSubmerged ? (t1 + 2.0f) : t1);
                            glm::vec3 c2(sun2, 0.55f * ao2, isSubmerged ? (t2 + 2.0f) : t2);

                            addOpaqueTri(B0, B1, B2, uvB0, uvB1, uvB2, nBot, c0, c1, c2);
                        }
                    }

                    // ---------------------------------------------------------
                    // Lateral Faces: Base Wall, Left Slanted, Right Slanted
                    // ---------------------------------------------------------
                    auto addWallFace = [&](const glm::vec3& vB0, const glm::vec3& vT0,
                                           const glm::vec3& vT1, const glm::vec3& vB1,
                                           const glm::vec3& wallNorm, uint32_t tileIndex, Cell neighborCell) {
                        if (isWaterBlock) {
                            // Water NEVER draws lateral walls inside lakes/seas at or below sea level (y <= 44),
                            // or against other water or solid blocks!
                            if (y <= 44 || neighborCell.type == BlockType::Water || neighborCell.isSolid()) return;
                        }

                        glm::vec4 uv = TextureAtlas::getTileUV(tileIndex);
                        auto [sun_B0, t_B0] = getVertexLight(vB0, wallNorm);
                        auto [sun_T0, t_T0] = getVertexLight(vT0, wallNorm);
                        auto [sun_T1, t_T1] = getVertexLight(vT1, wallNorm);
                        auto [sun_B1, t_B1] = getVertexLight(vB1, wallNorm);

                        if (isWaterBlock) {
                            float d0 = getWaterDepthAt(vB0.x, vB0.z, y);
                            float d1 = getWaterDepthAt(vB1.x, vB1.z, y);
                            glm::vec3 wc_B0(d0, sun_B0, t_B0);
                            glm::vec3 wc_T0(d0, sun_T0, t_T0);
                            glm::vec3 wc_T1(d1, sun_T1, t_T1);
                            glm::vec3 wc_B1(d1, sun_B1, t_B1);
                            addWaterQuad(vB0, vT0, vT1, vB1, uv, wallNorm, wc_B0, wc_T0, wc_T1, wc_B1);
                        } else {
                            bool isSubmerged = (neighborCell.type == BlockType::Water);

                            // Minecraft directional shading multiplier based on face normal:
                            // Base Wall (|Nx| < 0.2): 0.80f, Left Slanted (Nx < -0.2): 0.65f, Right Slanted (Nx > 0.2): 0.72f
                            float faceDir = 0.80f;
                            if (wallNorm.x < -0.2f) faceDir = 0.65f;
                            else if (wallNorm.x > 0.2f) faceDir = 0.72f;
                            glm::vec3 tang = glm::normalize(vB1 - vB0);

                            float ao_B0 = getWallVertexAO(vB0, wallNorm, tang, true, -1.0f);
                            float ao_T0 = getWallVertexAO(vT0, wallNorm, tang, false, -1.0f);
                            float ao_T1 = getWallVertexAO(vT1, wallNorm, tang, false, 1.0f);
                            float ao_B1 = getWallVertexAO(vB1, wallNorm, tang, true, 1.0f);

                            glm::vec3 c_B0(sun_B0, faceDir * ao_B0, isSubmerged ? (t_B0 + 2.0f) : t_B0);
                            glm::vec3 c_T0(sun_T0, faceDir * ao_T0, isSubmerged ? (t_T0 + 2.0f) : t_T0);
                            glm::vec3 c_T1(sun_T1, faceDir * ao_T1, isSubmerged ? (t_T1 + 2.0f) : t_T1);
                            glm::vec3 c_B1(sun_B1, faceDir * ao_B1, isSubmerged ? (t_B1 + 2.0f) : t_B1);

                            addOpaqueQuad(vB0, vT0, vT1, vB1, uv, wallNorm, c_B0, c_T0, c_T1, c_B1);
                        }
                    };

                    if (s == 0) {
                        // Base wall (along Z = z0, normal = (0, 0, -1))
                        Cell n2 = getCellAtWorld(neighbors[2].x, neighbors[2].y, neighbors[2].z, neighbors[2].s);
                        if (shouldDrawFace(cell, n2)) {
                            addWallFace(B0, T0, T1, B1, glm::vec3(0.0f, 0.0f, -1.0f), TextureAtlas::getTileForBlock(cell.type, 2), n2);
                        }

                        // Left slanted wall (normal = (-0.866, 0, 0.5))
                        Cell n3 = getCellAtWorld(neighbors[3].x, neighbors[3].y, neighbors[3].z, neighbors[3].s);
                        if (shouldDrawFace(cell, n3)) {
                            addWallFace(B2, T2, T0, B0, glm::vec3(-SQRT_3_OVER_2, 0.0f, 0.5f), TextureAtlas::getTileForBlock(cell.type, 3), n3);
                        }

                        // Right slanted wall (normal = (0.866, 0, 0.5))
                        Cell n4 = getCellAtWorld(neighbors[4].x, neighbors[4].y, neighbors[4].z, neighbors[4].s);
                        if (shouldDrawFace(cell, n4)) {
                            addWallFace(B1, T1, T2, B2, glm::vec3(SQRT_3_OVER_2, 0.0f, 0.5f), TextureAtlas::getTileForBlock(cell.type, 4), n4);
                        }
                    } else {
                        // Base wall (along Z = z1, normal = (0, 0, 1))
                        Cell n2 = getCellAtWorld(neighbors[2].x, neighbors[2].y, neighbors[2].z, neighbors[2].s);
                        if (shouldDrawFace(cell, n2)) {
                            addWallFace(B1, T1, T2, B2, glm::vec3(0.0f, 0.0f, 1.0f), TextureAtlas::getTileForBlock(cell.type, 2), n2);
                        }

                        // Left slanted wall (normal = (-0.866, 0, -0.5))
                        Cell n3 = getCellAtWorld(neighbors[3].x, neighbors[3].y, neighbors[3].z, neighbors[3].s);
                        if (shouldDrawFace(cell, n3)) {
                            addWallFace(B2, T2, T0, B0, glm::vec3(-SQRT_3_OVER_2, 0.0f, -0.5f), TextureAtlas::getTileForBlock(cell.type, 3), n3);
                        }

                        // Right slanted wall (normal = (0.866, 0, -0.5))
                        Cell n4 = getCellAtWorld(neighbors[4].x, neighbors[4].y, neighbors[4].z, neighbors[4].s);
                        if (shouldDrawFace(cell, n4)) {
                            addWallFace(B0, T0, T1, B1, glm::vec3(SQRT_3_OVER_2, 0.0f, -0.5f), TextureAtlas::getTileForBlock(cell.type, 4), n4);
                        }
                    }
                }
            }
        }
    }
    
    return mesh;
}

ChunkMesh ChunkMesher::generateLODMesh(const Chunk& chunk, LODLevel lod) {
    ChunkMesh mesh;
    int step = (lod == LODLevel::LOD1_Medium) ? 2 : 4;
    int worldX = chunk.getWorldX();
    int worldZ = chunk.getWorldZ();

    auto addOpaqueQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                             const glm::vec4& uv, const glm::vec3& normal,
                             const glm::vec3& c) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.opaqueVertices.size());
        mesh.opaqueVertices.push_back({v0, glm::vec2(uv.x, uv.w), normal, c});
        mesh.opaqueVertices.push_back({v1, glm::vec2(uv.x, uv.y), normal, c});
        mesh.opaqueVertices.push_back({v2, glm::vec2(uv.z, uv.y), normal, c});
        mesh.opaqueVertices.push_back({v3, glm::vec2(uv.z, uv.w), normal, c});
        mesh.opaqueIndices.push_back(baseIdx + 0);
        mesh.opaqueIndices.push_back(baseIdx + 1);
        mesh.opaqueIndices.push_back(baseIdx + 2);
        mesh.opaqueIndices.push_back(baseIdx + 2);
        mesh.opaqueIndices.push_back(baseIdx + 3);
        mesh.opaqueIndices.push_back(baseIdx + 0);
    };

    auto addOpaqueTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                            const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                            const glm::vec3& normal, const glm::vec3& c) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.opaqueVertices.size());
        mesh.opaqueVertices.push_back({v0, uv0, normal, c});
        mesh.opaqueVertices.push_back({v1, uv1, normal, c});
        mesh.opaqueVertices.push_back({v2, uv2, normal, c});
        mesh.opaqueIndices.push_back(baseIdx + 0);
        mesh.opaqueIndices.push_back(baseIdx + 1);
        mesh.opaqueIndices.push_back(baseIdx + 2);
    };

    auto addWaterTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                           const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                           const glm::vec3& normal, const glm::vec3& c) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.waterVertices.size());
        mesh.waterVertices.push_back({v0, uv0, normal, c});
        mesh.waterVertices.push_back({v1, uv1, normal, c});
        mesh.waterVertices.push_back({v2, uv2, normal, c});
        mesh.waterIndices.push_back(baseIdx + 0);
        mesh.waterIndices.push_back(baseIdx + 1);
        mesh.waterIndices.push_back(baseIdx + 2);
    };

    const glm::vec3 nTop(0.0f, 1.0f, 0.0f);
    const float fStep = static_cast<float>(step);
    const float halfStep = fStep * 0.5f;

    // Grid of heights to detect height drops between adjacent coarse cells
    int heightGrid[16][16];
    BlockType typeGrid[16][16];
    for (int x = 0; x < CHUNK_SIZE_X; x += step) {
        for (int z = 0; z < CHUNK_SIZE_Z; z += step) {
            int topY = -1;
            BlockType topType = BlockType::Air;
            for (int dx = 0; dx < step; ++dx) {
                for (int dz = 0; dz < step; ++dz) {
                    for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
                        Cell c0 = chunk.getCell(x + dx, y, z + dz, 0);
                        Cell c1 = chunk.getCell(x + dx, y, z + dz, 1);
                        if (c0.isSolid() || c0.type == BlockType::Water) {
                            if (y > topY) { topY = y; topType = c0.type; }
                            break;
                        }
                        if (c1.isSolid() || c1.type == BlockType::Water) {
                            if (y > topY) { topY = y; topType = c1.type; }
                            break;
                        }
                    }
                }
            }
            heightGrid[x][z] = topY;
            typeGrid[x][z] = topType;
        }
    }

    for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
        for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
            int topY = heightGrid[lx][lz];
            if (topY < 0) continue;
            BlockType topType = typeGrid[lx][lz];

            int wx = worldX + lx;
            int wz = worldZ + lz;
            float x0 = static_cast<float>(wx) + getRowXOffset(wz);
            float z0 = static_cast<float>(wz) * TRI_HEIGHT;
            float z1 = static_cast<float>(wz + step) * TRI_HEIGHT;
            float py = static_cast<float>(topY);

            glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(topType, 0));
            float uMid = uv.x + (uv.z - uv.x) * 0.5f;
            glm::vec3 litColor(1.0f, 1.0f, 0.0f); // Full sunlight, 1.0 AO, 0.0 torch

            // s = 0 (UP triangle)
            glm::vec3 T0_s0(x0, py + 1.0f, z0);
            glm::vec3 T1_s0(x0 + fStep, py + 1.0f, z0);
            glm::vec3 T2_s0(x0 + halfStep, py + 1.0f, z1);
            glm::vec2 uv0_s0(uv.x, uv.w);
            glm::vec2 uv1_s0(uv.z, uv.w);
            glm::vec2 uv2_s0(uMid, uv.y);

            // s = 1 (DOWN triangle)
            glm::vec3 T0_s1(x0 + fStep, py + 1.0f, z0);
            glm::vec3 T1_s1(x0 + fStep + halfStep, py + 1.0f, z1);
            glm::vec3 T2_s1(x0 + halfStep, py + 1.0f, z1);
            glm::vec2 uv0_s1(uMid, uv.w);
            glm::vec2 uv1_s1(uv.z, uv.y);
            glm::vec2 uv2_s1(uv.x, uv.y);

            if (topType == BlockType::Water) {
                glm::vec3 waterCol(0.5f, 1.0f, 0.0f);
                addWaterTri(T0_s0, T2_s0, T1_s0, uv0_s0, uv2_s0, uv1_s0, nTop, waterCol);
                addWaterTri(T0_s1, T2_s1, T1_s1, uv0_s1, uv2_s1, uv1_s1, nTop, waterCol);
            } else {
                addOpaqueTri(T0_s0, T2_s0, T1_s0, uv0_s0, uv2_s0, uv1_s0, nTop, litColor);
                addOpaqueTri(T0_s1, T2_s1, T1_s1, uv0_s1, uv2_s1, uv1_s1, nTop, litColor);
            }

            // Boundary and step skirts to prevent T-junction crack slivers
            glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(topType, 2));
            float skirtDepth = 2.0f;

            // South boundary (lz == 0, Base Wall: 0.80)
            if (lz == 0) {
                glm::vec3 vB0(x0, py + 1.0f - skirtDepth, z0);
                glm::vec3 vT0(x0, py + 1.0f, z0);
                glm::vec3 vT1(x0 + fStep, py + 1.0f, z0);
                glm::vec3 vB1(x0 + fStep, py + 1.0f - skirtDepth, z0);
                addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.80f, 0.0f));
            }
            // North boundary (lz + step >= CHUNK_SIZE_Z, Base Wall: 0.80)
            if (lz + step >= CHUNK_SIZE_Z) {
                glm::vec3 vB0(x0 + fStep + halfStep, py + 1.0f - skirtDepth, z1);
                glm::vec3 vT0(x0 + fStep + halfStep, py + 1.0f, z1);
                glm::vec3 vT1(x0 + halfStep, py + 1.0f, z1);
                glm::vec3 vB1(x0 + halfStep, py + 1.0f - skirtDepth, z1);
                addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.80f, 0.0f));
            }
            // West boundary (lx == 0, Left Slanted: 0.65)
            if (lx == 0) {
                glm::vec3 vB0(x0 + halfStep, py + 1.0f - skirtDepth, z1);
                glm::vec3 vT0(x0 + halfStep, py + 1.0f, z1);
                glm::vec3 vT1(x0, py + 1.0f, z0);
                glm::vec3 vB1(x0, py + 1.0f - skirtDepth, z0);
                addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(-SQRT_3_OVER_2, 0.0f, 0.5f), glm::vec3(1.0f, 0.65f, 0.0f));
            }
            // East boundary (lx + step >= CHUNK_SIZE_X, Right Slanted: 0.72)
            if (lx + step >= CHUNK_SIZE_X) {
                glm::vec3 vB0(x0 + fStep, py + 1.0f - skirtDepth, z0);
                glm::vec3 vT0(x0 + fStep, py + 1.0f, z0);
                glm::vec3 vT1(x0 + fStep + halfStep, py + 1.0f, z1);
                glm::vec3 vB1(x0 + fStep + halfStep, py + 1.0f - skirtDepth, z1);
                addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(SQRT_3_OVER_2, 0.0f, 0.5f), glm::vec3(1.0f, 0.72f, 0.0f));
            }
        }
    }

    return mesh;
}

ChunkMesh ChunkMesher::generateImposterMesh(const ChunkCoord& coord, const TerrainGen& terrainGen) {
    ChunkMesh mesh;
    int worldX = coord.cx * CHUNK_SIZE_X;
    int worldZ = coord.cz * CHUNK_SIZE_Z;

    auto addOpaqueTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                            const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                            const glm::vec3& normal, const glm::vec3& c) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.opaqueVertices.size());
        mesh.opaqueVertices.push_back({v0, uv0, normal, c});
        mesh.opaqueVertices.push_back({v1, uv1, normal, c});
        mesh.opaqueVertices.push_back({v2, uv2, normal, c});
        mesh.opaqueIndices.push_back(baseIdx + 0);
        mesh.opaqueIndices.push_back(baseIdx + 1);
        mesh.opaqueIndices.push_back(baseIdx + 2);
    };

    auto addWaterTri = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                           const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                           const glm::vec3& normal, const glm::vec3& c) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.waterVertices.size());
        mesh.waterVertices.push_back({v0, uv0, normal, c});
        mesh.waterVertices.push_back({v1, uv1, normal, c});
        mesh.waterVertices.push_back({v2, uv2, normal, c});
        mesh.waterIndices.push_back(baseIdx + 0);
        mesh.waterIndices.push_back(baseIdx + 1);
        mesh.waterIndices.push_back(baseIdx + 2);
    };

    const glm::vec3 nTop(0.0f, 1.0f, 0.0f);
    const int step = 8; // 2x2 coarse cells per chunk (8 triangles total!)
    const float fStep = static_cast<float>(step);
    const float halfStep = fStep * 0.5f;

    for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
        for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
            int wx = worldX + lx;
            int wz = worldZ + lz;

            int rawH = terrainGen.getHeight(wx + step / 2, wz + step / 2);
            bool isWater = (rawH <= 44);
            float py = isWater ? 44.0f : static_cast<float>(rawH);

            BlockType bType = BlockType::Grass;
            if (isWater) bType = BlockType::Water;
            else if (rawH <= 46) bType = BlockType::Sand;
            else if (rawH >= 74) bType = BlockType::Snow;

            float x0 = static_cast<float>(wx) + getRowXOffset(wz);
            float z0 = static_cast<float>(wz) * TRI_HEIGHT;
            float z1 = static_cast<float>(wz + step) * TRI_HEIGHT;

            glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 0));
            float uMid = uv.x + (uv.z - uv.x) * 0.5f;
            glm::vec3 litColor(1.0f, 1.0f, 0.0f);

            // s = 0
            glm::vec3 T0_s0(x0, py + 1.0f, z0);
            glm::vec3 T1_s0(x0 + fStep, py + 1.0f, z0);
            glm::vec3 T2_s0(x0 + halfStep, py + 1.0f, z1);
            glm::vec2 uv0_s0(uv.x, uv.w);
            glm::vec2 uv1_s0(uv.z, uv.w);
            glm::vec2 uv2_s0(uMid, uv.y);

            // s = 1
            glm::vec3 T0_s1(x0 + fStep, py + 1.0f, z0);
            glm::vec3 T1_s1(x0 + fStep + halfStep, py + 1.0f, z1);
            glm::vec3 T2_s1(x0 + halfStep, py + 1.0f, z1);
            glm::vec2 uv0_s1(uMid, uv.w);
            glm::vec2 uv1_s1(uv.z, uv.y);
            glm::vec2 uv2_s1(uv.x, uv.y);

            if (isWater) {
                glm::vec3 waterCol(0.5f, 1.0f, 0.0f);
                addWaterTri(T0_s0, T2_s0, T1_s0, uv0_s0, uv2_s0, uv1_s0, nTop, waterCol);
                addWaterTri(T0_s1, T2_s1, T1_s1, uv0_s1, uv2_s1, uv1_s1, nTop, waterCol);
            } else {
                addOpaqueTri(T0_s0, T2_s0, T1_s0, uv0_s0, uv2_s0, uv1_s0, nTop, litColor);
                addOpaqueTri(T0_s1, T2_s1, T1_s1, uv0_s1, uv2_s1, uv1_s1, nTop, litColor);
            }
        }
    }

    return mesh;
}

} // namespace prismcraft
