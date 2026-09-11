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

    // Precompute top-most solid and leaf voxel per column for natural sunlight penetration
    int maxSolidY[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    int maxLeafY[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    std::vector<glm::vec3> chunkTorches;

    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        int wx = worldX + x;
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            int wz = worldZ + z;
            maxSolidY[x][z] = -1;
            maxLeafY[x][z] = -1;
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
                Cell c0 = chunk.getCell(x, y, z, 0);
                Cell c1 = chunk.getCell(x, y, z, 1);
                if (c0.isTorch()) chunkTorches.push_back(cellToWorldCenter(wx, y, wz, 0));
                if (c1.isTorch()) chunkTorches.push_back(cellToWorldCenter(wx, y, wz, 1));
                if (maxSolidY[x][z] < 0 && (c0.isOpaque() || c1.isOpaque()) && c0.type != BlockType::Leaves && c1.type != BlockType::Leaves) {
                    maxSolidY[x][z] = y;
                }
                if (maxLeafY[x][z] < 0 && (c0.type == BlockType::Leaves || c1.type == BlockType::Leaves)) {
                    maxLeafY[x][z] = y;
                }
            }
        }
    }

    auto getSunlightFactor = [&](int x, int y, int z) -> float {
        int wx = worldX + x;
        int wz = worldZ + z;

        if (y >= CHUNK_SIZE_Y - 1) return 1.0f;

        // 1. Column shadow with gradual diffusion under overhangs / ceilings
        int solidRoofY = (x >= 0 && x < CHUNK_SIZE_X && z >= 0 && z < CHUNK_SIZE_Z) ? maxSolidY[x][z] : -1;
        int leafRoofY  = (x >= 0 && x < CHUNK_SIZE_X && z >= 0 && z < CHUNK_SIZE_Z) ? maxLeafY[x][z]  : -1;
        if (solidRoofY < 0 && leafRoofY < 0) {
            for (int sy = CHUNK_SIZE_Y - 1; sy >= y; --sy) {
                Cell sc0 = getCellAtWorld(wx, sy, wz, 0);
                Cell sc1 = getCellAtWorld(wx, sy, wz, 1);
                bool isLeaf = (sc0.type == BlockType::Leaves || sc1.type == BlockType::Leaves);
                bool isOpaque = (sc0.isOpaque() || sc1.isOpaque());
                if (solidRoofY < 0 && isOpaque && !isLeaf) solidRoofY = sy;
                if (leafRoofY < 0 && isLeaf) leafRoofY = sy;
                if (solidRoofY >= 0) break;
            }
        }

        // Solid roof (caves, overhangs, buildings): full gradual skylight decay
        if (solidRoofY > y) {
            int depth = solidRoofY - y;
            return std::max(0.04f, 0.85f - depth * 0.12f);
        }

        // Leaf canopy (trees): translucent foliage lets abundant ambient daylight through
        if (leafRoofY > y) {
            int depth = leafRoofY - y;
            return std::max(0.65f, 0.95f - depth * 0.04f); // Under trees stays bright, soft, and realistic
        }

        // 2. Fast directional sunlight shadow check from sun elevation
        for (int step = 1; step <= 3; ++step) {
            int sy = y + step;
            if (sy >= CHUNK_SIZE_Y) break;
            int sx = x + step;
            int sz = z + step;
            if (sx >= 0 && sx < CHUNK_SIZE_X && sz >= 0 && sz < CHUNK_SIZE_Z) {
                if (maxSolidY[sx][sz] >= sy) {
                    return 0.15f; // Solid terrain blocks directional sun
                }
                if (maxLeafY[sx][sz] >= sy) {
                    return 0.60f; // Dappled light filtering through leaf canopy
                }
            } else {
                Cell sc0 = getCellAtWorld(wx + step, sy, wz + step, 0);
                Cell sc1 = getCellAtWorld(wx + step, sy, wz + step, 1);
                bool isLeaf = (sc0.type == BlockType::Leaves || sc1.type == BlockType::Leaves);
                if (sc0.isOpaque() || sc1.isOpaque()) {
                    return isLeaf ? 0.60f : 0.15f;
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
    // Geometry-Aware Voxel Contact Ambient Occlusion
    // Tailored for Equilateral Triangular Prism Honeycomb
    // -------------------------------------------------------------
    auto getOcclusionWeight = [&](const Cell& c) -> float {
        if (c.type == BlockType::Air || c.type == BlockType::Water) return 0.0f;
        if (c.isOpaque()) return 1.0f;
        if (c.type == BlockType::Leaves) return 0.70f; // Leaves cast soft, believable canopy & foliage occlusion
        if (c.isSolid()) return 0.80f; // Glass, Ice, etc.
        if (c.isFoliage()) return 0.20f; // Contact touch from tall grass, flowers
        return 0.0f;
    };

    // Equilateral triangle sector bisectors around ANY vertex in the triangular lattice.
    // 6 triangles meet at 60-degree increments: 30, 90, 150, 210, 270, 330 deg.
    static const float triSectorAngles[6][2] = {
        {  0.8660254f,  0.5000000f }, // 30 deg
        {  0.0000000f,  1.0000000f }, // 90 deg
        { -0.8660254f,  0.5000000f }, // 150 deg
        { -0.8660254f, -0.5000000f }, // 210 deg
        {  0.0000000f, -1.0000000f }, // 270 deg
        {  0.8660254f, -0.5000000f }  // 330 deg
    };

    // 1. Top face vertex AO: probes the 6 horizontal triangular prism sectors meeting at vertex V in layer y+1
    auto getTopVertexAO = [&](const glm::vec3& V, int y) -> float {
        float occScore = 0.0f;
        for (int i = 0; i < 6; ++i) {
            glm::vec3 probePos(V.x + triSectorAngles[i][0] * 0.38f, static_cast<float>(y) + 1.25f, V.z + triSectorAngles[i][1] * 0.38f);
            CellCoord sc = worldToCell(probePos);
            Cell scell = getCellAtWorld(sc.x, sc.y, sc.z, sc.s);
            occScore += getOcclusionWeight(scell);
        }
        // Vertical overhang / leaf canopy check (1 block higher)
        glm::vec3 aboveProbe(V.x, static_cast<float>(y) + 2.25f, V.z);
        CellCoord scAbove = worldToCell(aboveProbe);
        Cell scellAbove = getCellAtWorld(scAbove.x, scAbove.y, scAbove.z, scAbove.s);
        occScore += getOcclusionWeight(scellAbove) * 0.35f;

        if (occScore <= 0.05f) return 1.00f;
        // Subtle, non-crushing curve: 0 occluders = 1.0, 1 = 0.87, 2 = 0.74, 3 = 0.61, 4+ = 0.48
        float ao = 1.0f - std::min(occScore, 4.0f) * 0.13f;
        return std::clamp(ao, 0.48f, 1.0f);
    };

    // 2. Bottom face vertex AO: probes the 6 horizontal triangular prism sectors meeting at vertex V in layer y-1
    auto getBotVertexAO = [&](const glm::vec3& V, int y) -> float {
        float occScore = 0.0f;
        for (int i = 0; i < 6; ++i) {
            glm::vec3 probePos(V.x + triSectorAngles[i][0] * 0.38f, static_cast<float>(y) - 0.25f, V.z + triSectorAngles[i][1] * 0.38f);
            CellCoord sc = worldToCell(probePos);
            Cell scell = getCellAtWorld(sc.x, sc.y, sc.z, sc.s);
            occScore += getOcclusionWeight(scell);
        }
        glm::vec3 belowProbe(V.x, static_cast<float>(y) - 1.25f, V.z);
        CellCoord scBelow = worldToCell(belowProbe);
        Cell scellBelow = getCellAtWorld(scBelow.x, scBelow.y, scBelow.z, scBelow.s);
        occScore += getOcclusionWeight(scellBelow) * 0.35f;

        if (occScore <= 0.05f) return 1.00f;
        float ao = 1.0f - std::min(occScore, 4.0f) * 0.13f;
        return std::clamp(ao, 0.48f, 1.0f);
    };

    // 3. Wall quad vertex AO: probes floor/ceiling contact, lateral corner seams, and diagonal crevice pockets
    auto getWallVertexAO = [&](const glm::vec3& V, const glm::vec3& wallNorm, const glm::vec3& tang, bool isBottom, float lateralSign) -> float {
        glm::vec3 Pout = V + wallNorm * 0.12f;

        // Test vertical contact (floor at y-1 or overhang ceiling at y+1)
        glm::vec3 vertProbe = Pout + glm::vec3(0.0f, isBottom ? -0.35f : 0.35f, 0.0f);
        Cell scVert = getCellAtWorld(worldToCell(vertProbe).x, worldToCell(vertProbe).y, worldToCell(vertProbe).z, worldToCell(vertProbe).s);
        float wVert = getOcclusionWeight(scVert);

        // Test lateral corner seam (adjacent block forming an inner corner)
        glm::vec3 latProbe = Pout + tang * (lateralSign * 0.30f);
        Cell scLat = getCellAtWorld(worldToCell(latProbe).x, worldToCell(latProbe).y, worldToCell(latProbe).z, worldToCell(latProbe).s);
        float wLat = getOcclusionWeight(scLat);

        // Test diagonal corner pocket (meeting point of floor/ceiling and lateral wall)
        glm::vec3 diagProbe = Pout + tang * (lateralSign * 0.30f) + glm::vec3(0.0f, isBottom ? -0.35f : 0.35f, 0.0f);
        Cell scDiag = getCellAtWorld(worldToCell(diagProbe).x, worldToCell(diagProbe).y, worldToCell(diagProbe).z, worldToCell(diagProbe).s);
        float wDiag = getOcclusionWeight(scDiag);

        float occScore = wVert * 1.0f + wLat * 1.0f + wDiag * 0.65f;
        if (occScore <= 0.02f) return 1.00f;

        // Smooth subtle falloff: floor contact = ~0.84, inner corner = ~0.68, 3-way pocket = ~0.56
        float ao = 1.0f - std::min(occScore, 3.2f) * 0.16f;
        return std::clamp(ao, 0.48f, 1.0f);
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
                        glm::vec3 folColorBot = cell.isTorch() ? glm::vec3(1.0f, 1.0f, 1.0f) : glm::vec3(sunlight, 0.72f, torchL);
                        glm::vec3 folColorTop = cell.isTorch() ? glm::vec3(1.0f, 1.0f, 1.0f) : glm::vec3(sunlight, 1.0f, torchL);

                        // Diagonal quad 1 (Double sided)
                        addOpaqueQuad(center + glm::vec3(-hw, 0.0f, -hw), center + glm::vec3(-hw, fh, -hw),
                                      center + glm::vec3(hw, fh, hw), center + glm::vec3(hw, 0.0f, hw),
                                      uv, nTop, folColorBot, folColorTop, folColorTop, folColorBot);
                        addOpaqueQuad(center + glm::vec3(hw, 0.0f, hw), center + glm::vec3(hw, fh, hw),
                                      center + glm::vec3(-hw, fh, -hw), center + glm::vec3(-hw, 0.0f, -hw),
                                      uv, nTop, folColorBot, folColorTop, folColorTop, folColorBot);

                        // Diagonal quad 2 (Double sided)
                        addOpaqueQuad(center + glm::vec3(-hw, 0.0f, hw), center + glm::vec3(-hw, fh, hw),
                                      center + glm::vec3(hw, fh, -hw), center + glm::vec3(hw, 0.0f, -hw),
                                      uv, nTop, folColorBot, folColorTop, folColorTop, folColorBot);
                        addOpaqueQuad(center + glm::vec3(hw, 0.0f, -hw), center + glm::vec3(hw, fh, -hw),
                                      center + glm::vec3(-hw, fh, hw), center + glm::vec3(-hw, 0.0f, hw),
                                      uv, nTop, folColorBot, folColorTop, folColorTop, folColorBot);
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

                    // Directional horizontal flow vector for water column
                    auto getWaterFlowVector = [&](int cx, int cy, int cz, int cs, const Cell& curCell) -> glm::vec2 {
                        CellCoord nbrs[5];
                        getNeighbors(cx, cy, cz, cs, nbrs);

                        glm::vec2 wallDirs[3];
                        if (cs == 0) {
                            wallDirs[0] = glm::vec2(0.0f, -1.0f);
                            wallDirs[1] = glm::vec2(-SQRT_3_OVER_2, 0.5f);
                            wallDirs[2] = glm::vec2(SQRT_3_OVER_2, 0.5f);
                        } else {
                            wallDirs[0] = glm::vec2(0.0f, 1.0f);
                            wallDirs[1] = glm::vec2(-SQRT_3_OVER_2, -0.5f);
                            wallDirs[2] = glm::vec2(SQRT_3_OVER_2, -0.5f);
                        }

                        glm::vec2 flow(0.0f);
                        for (int i = 0; i < 3; ++i) {
                            CellCoord nc = nbrs[i + 2];
                            Cell nCell = getCellAtWorld(nc.x, nc.y, nc.z, nc.s);
                            Cell nBelow = getCellAtWorld(nc.x, nc.y - 1, nc.z, nc.s);

                            float weight = 0.0f;
                            if (nCell.type == BlockType::Air || nCell.type == BlockType::Water) {
                                if (nBelow.type == BlockType::Air || nBelow.type == BlockType::Water) {
                                    weight = 4.0f; // Drop / waterfall
                                } else if (nCell.type == BlockType::Water) {
                                    int diff = static_cast<int>(nCell.level) - static_cast<int>(curCell.level);
                                    weight = static_cast<float>(diff);
                                } else if (nCell.type == BlockType::Air) {
                                    weight = 2.0f;
                                }
                            }
                            flow += wallDirs[i] * weight;
                        }

                        if (glm::length(flow) > 0.001f) {
                            return glm::normalize(flow);
                        }
                        return glm::vec2(0.0f);
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

                            // Solid, deterministic water height: 0.90 for source, flowing water drops by level
                            // Perfectly shared across adjacent prisms to eliminate any triangular holes/cracks
                            float waterHeight = (cell.level == 0) ? 0.90f : std::max(0.20f, 0.90f - static_cast<float>(cell.level) * 0.10f);
                            glm::vec3 W0(vXZ[0].x, py + waterHeight, vXZ[0].y);
                            glm::vec3 W1(vXZ[1].x, py + waterHeight, vXZ[1].y);
                            glm::vec3 W2(vXZ[2].x, py + waterHeight, vXZ[2].y);

                            // Continuous world-space UVs to prevent any block seams
                            glm::vec2 uvW0 = glm::vec2(vXZ[0].x, vXZ[0].y) * 0.25f;
                            glm::vec2 uvW1 = glm::vec2(vXZ[1].x, vXZ[1].y) * 0.25f;
                            glm::vec2 uvW2 = glm::vec2(vXZ[2].x, vXZ[2].y) * 0.25f;

                            // Flow vector passed via normal.xz
                            glm::vec2 flowVec = getWaterFlowVector(wx, y, wz, s, cell);
                            glm::vec3 nWaterTop(flowVec.x, 1.0f, flowVec.y);

                            addWaterTri(W0, W2, W1, uvW0, uvW2, uvW1, nWaterTop, waterCol0, waterCol2, waterCol1);
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
                        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 0));
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

                            glm::vec3 c0(sun0, ao0, isSubmerged ? (t0 + 2.0f) : t0);
                            glm::vec3 c1(sun1, ao1, isSubmerged ? (t1 + 2.0f) : t1);
                            glm::vec3 c2(sun2, ao2, isSubmerged ? (t2 + 2.0f) : t2);

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
                            // Water only draws lateral walls when exposed to air and non-solid
                            if (neighborCell.type == BlockType::Water || neighborCell.isSolid()) return;
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
                            bool hasWaterAbove = (getCellAtWorld(wx, y + 1, wz, s).type == BlockType::Water);
                            float waterHeight = (cell.level == 0) ? 0.90f : std::max(0.20f, 0.90f - static_cast<float>(cell.level) * 0.10f);
                            float topY = hasWaterAbove ? (py + 1.0f) : (py + waterHeight);
                            glm::vec3 wT0 = vT0; wT0.y = topY;
                            glm::vec3 wT1 = vT1; wT1.y = topY;
                            addWaterQuad(vB0, wT0, wT1, vB1, uv, wallNorm, wc_B0, wc_T0, wc_T1, wc_B1);
                        } else {
                            bool isSubmerged = (neighborCell.type == BlockType::Water);
                            glm::vec3 tang = glm::normalize(vB1 - vB0);

                            float ao_B0 = getWallVertexAO(vB0, wallNorm, tang, true, -1.0f);
                            float ao_T0 = getWallVertexAO(vT0, wallNorm, tang, false, -1.0f);
                            float ao_T1 = getWallVertexAO(vT1, wallNorm, tang, false, 1.0f);
                            float ao_B1 = getWallVertexAO(vB1, wallNorm, tang, true, 1.0f);

                            glm::vec3 c_B0(sun_B0, ao_B0, isSubmerged ? (t_B0 + 2.0f) : t_B0);
                            glm::vec3 c_T0(sun_T0, ao_T0, isSubmerged ? (t_T0 + 2.0f) : t_T0);
                            glm::vec3 c_T1(sun_T1, ao_T1, isSubmerged ? (t_T1 + 2.0f) : t_T1);
                            glm::vec3 c_B1(sun_B1, ao_B1, isSubmerged ? (t_B1 + 2.0f) : t_B1);

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

    auto addWaterQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                            const glm::vec4& uv, const glm::vec3& normal,
                            const glm::vec3& c) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.waterVertices.size());
        mesh.waterVertices.push_back({v0, glm::vec2(uv.x, uv.w), normal, c});
        mesh.waterVertices.push_back({v1, glm::vec2(uv.x, uv.y), normal, c});
        mesh.waterVertices.push_back({v2, glm::vec2(uv.z, uv.y), normal, c});
        mesh.waterVertices.push_back({v3, glm::vec2(uv.z, uv.w), normal, c});
        mesh.waterIndices.push_back(baseIdx + 0);
        mesh.waterIndices.push_back(baseIdx + 1);
        mesh.waterIndices.push_back(baseIdx + 2);
        mesh.waterIndices.push_back(baseIdx + 2);
        mesh.waterIndices.push_back(baseIdx + 3);
        mesh.waterIndices.push_back(baseIdx + 0);
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

    // 1. Scan column heights for solid terrain and water presence
    int solidHeightGrid[16][16];
    BlockType solidTypeGrid[16][16];
    bool hasWaterGrid[16][16];

    for (int x = 0; x < CHUNK_SIZE_X; x += step) {
        for (int z = 0; z < CHUNK_SIZE_Z; z += step) {
            int topY = -1;
            BlockType topType = BlockType::Air;
            bool hasWater = false;

            for (int dx = 0; dx < step; ++dx) {
                for (int dz = 0; dz < step; ++dz) {
                    for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
                        Cell c0 = chunk.getCell(x + dx, y, z + dz, 0);
                        Cell c1 = chunk.getCell(x + dx, y, z + dz, 1);
                        if (c0.type == BlockType::Water || c1.type == BlockType::Water) {
                            hasWater = true;
                        }
                        if (c0.isSolid()) {
                            if (y > topY) { topY = y; topType = c0.type; }
                            break;
                        }
                        if (c1.isSolid()) {
                            if (y > topY) { topY = y; topType = c1.type; }
                            break;
                        }
                    }
                }
            }
            solidHeightGrid[x][z] = topY;
            solidTypeGrid[x][z] = (topY >= 0) ? topType : BlockType::Dirt;
            hasWaterGrid[x][z] = hasWater || (topY >= 0 && topY < 44);
        }
    }

    // 2. Emit Solid Terrain Top Quads and Water Top Quads
    for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
        for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
            int wx = worldX + lx;
            int wz = worldZ + lz;
            float x0 = static_cast<float>(wx) + getRowXOffset(wz);
            float x1 = x0 + fStep;
            float z0 = static_cast<float>(wz) * TRI_HEIGHT;
            float z1 = static_cast<float>(wz + step) * TRI_HEIGHT;

            // A. Solid Terrain (Seabed or ground)
            int solidY = solidHeightGrid[lx][lz];
            if (solidY >= 0) {
                BlockType solidType = solidTypeGrid[lx][lz];
                float py = static_cast<float>(solidY);
                glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(solidType, 0));
                glm::vec3 v0(x0, py + 1.0f, z0);
                glm::vec3 v1(x0, py + 1.0f, z1);
                glm::vec3 v2(x1, py + 1.0f, z1);
                glm::vec3 v3(x1, py + 1.0f, z0);
                addOpaqueQuad(v0, v1, v2, v3, uv, nTop, glm::vec3(1.0f, 1.0f, 0.0f));

                // Outer chunk border skirts for solid ground
                glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(solidType, 2));
                float skirtDepth = std::max(12.0f, py - 32.0f);

                if (lz == 0) {
                    glm::vec3 vB0(x0, py + 1.0f - skirtDepth, z0);
                    glm::vec3 vT0(x0, py + 1.0f, z0);
                    glm::vec3 vT1(x1, py + 1.0f, z0);
                    glm::vec3 vB1(x1, py + 1.0f - skirtDepth, z0);
                    addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                }
                if (lz + step >= CHUNK_SIZE_Z) {
                    glm::vec3 vB0(x1, py + 1.0f - skirtDepth, z1);
                    glm::vec3 vT0(x1, py + 1.0f, z1);
                    glm::vec3 vT1(x0, py + 1.0f, z1);
                    glm::vec3 vB1(x0, py + 1.0f - skirtDepth, z1);
                    addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                }
                if (lx == 0) {
                    glm::vec3 vB0(x0, py + 1.0f - skirtDepth, z1);
                    glm::vec3 vT0(x0, py + 1.0f, z1);
                    glm::vec3 vT1(x0, py + 1.0f, z0);
                    glm::vec3 vB1(x0, py + 1.0f - skirtDepth, z0);
                    addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                }
                if (lx + step >= CHUNK_SIZE_X) {
                    glm::vec3 vB0(x1, py + 1.0f - skirtDepth, z0);
                    glm::vec3 vT0(x1, py + 1.0f, z0);
                    glm::vec3 vT1(x1, py + 1.0f, z1);
                    glm::vec3 vB1(x1, py + 1.0f - skirtDepth, z1);
                    addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                }
            }
        }
    }

    // B. Water Surface: Exact equilateral triangular mesh matching LOD0 perfectly at Y = 45.0f
    // Every column with water produces identical vertices to LOD0, guaranteeing 100% gapless stitching!
    for (int clx = 0; clx < CHUNK_SIZE_X; ++clx) {
        for (int clz = 0; clz < CHUNK_SIZE_Z; ++clz) {
            for (int s = 0; s < 2; ++s) {
                Cell cell = chunk.getCell(clx, 44, clz, s);
                bool hasWaterHere = (cell.type == BlockType::Water);
                if (!hasWaterHere) {
                    for (int wy = 43; wy >= 24; --wy) {
                        if (chunk.getCell(clx, wy, clz, s).type == BlockType::Water) {
                            hasWaterHere = true;
                            break;
                        }
                    }
                }
                if (!hasWaterHere) continue;

                // If cell above is water or solid, top face is covered
                Cell cAbove = chunk.getCell(clx, 45, clz, s);
                if (cAbove.type == BlockType::Water || cAbove.isSolid()) continue;

                glm::vec2 vXZ[3];
                getPrismVerticesXZ(worldX + clx, worldZ + clz, s, vXZ);

                float waterY = 44.0f;
                glm::vec3 T0(vXZ[0].x, waterY + 0.90f, vXZ[0].y);
                glm::vec3 T1(vXZ[1].x, waterY + 0.90f, vXZ[1].y);
                glm::vec3 T2(vXZ[2].x, waterY + 0.90f, vXZ[2].y);

                glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Water, 0));
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

                float depthFactor = 0.65f;
                for (int sy = 43; sy >= 0; --sy) {
                    if (chunk.getCell(clx, sy, clz, s).isSolid()) {
                        depthFactor = std::clamp((44.0f - static_cast<float>(sy)) / 7.0f, 0.1f, 1.0f);
                        break;
                    }
                }

                glm::vec3 waterCol(depthFactor, 1.0f, 0.0f);
                addWaterTri(T0, T2, T1, uvT0, uvT2, uvT1, nTop, waterCol);
            }
        }
    }

    // 3. Internal Height Skirts for Solid Terrain (prevents see-through holes between uneven solid voxels)
    // A. Skirts between adjacent cells in X
    for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
        int wz = worldZ + lz;
        float z0 = static_cast<float>(wz) * TRI_HEIGHT;
        float z1 = static_cast<float>(wz + step) * TRI_HEIGHT;

        for (int lx = 0; lx < CHUNK_SIZE_X - step; lx += step) {
            int yA = solidHeightGrid[lx][lz];
            int yB = solidHeightGrid[lx + step][lz];
            if (yA < 0 || yB < 0 || yA == yB) continue;

            int wx = worldX + lx;
            float x1 = static_cast<float>(wx) + getRowXOffset(wz) + fStep;

            if (yA > yB) {
                BlockType bType = solidTypeGrid[lx][lz];
                glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                glm::vec3 v0(x1, static_cast<float>(yB) + 1.0f, z0);
                glm::vec3 v1(x1, static_cast<float>(yA) + 1.0f, z0);
                glm::vec3 v2(x1, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v3(x1, static_cast<float>(yB) + 1.0f, z1);
                addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.72f, 0.0f));
            } else {
                BlockType bType = solidTypeGrid[lx + step][lz];
                glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                glm::vec3 v0(x1, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v1(x1, static_cast<float>(yB) + 1.0f, z1);
                glm::vec3 v2(x1, static_cast<float>(yB) + 1.0f, z0);
                glm::vec3 v3(x1, static_cast<float>(yA) + 1.0f, z0);
                addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.65f, 0.0f));
            }
        }
    }

    // B. Skirts between adjacent cells in Z
    for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
        for (int lz = 0; lz < CHUNK_SIZE_Z - step; lz += step) {
            int yA = solidHeightGrid[lx][lz];
            int yC = solidHeightGrid[lx][lz + step];
            if (yA < 0 || yC < 0 || yA == yC) continue;

            int wx = worldX + lx;
            int wz = worldZ + lz;
            float x0 = static_cast<float>(wx) + getRowXOffset(wz);
            float x1 = x0 + fStep;
            float z1 = static_cast<float>(wz + step) * TRI_HEIGHT;

            if (yA > yC) {
                BlockType bType = solidTypeGrid[lx][lz];
                glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                glm::vec3 v0(x1, static_cast<float>(yC) + 1.0f, z1);
                glm::vec3 v1(x1, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v2(x0, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v3(x0, static_cast<float>(yC) + 1.0f, z1);
                addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.80f, 0.0f));
            } else {
                BlockType bType = solidTypeGrid[lx][lz + step];
                glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                glm::vec3 v0(x0, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v1(x0, static_cast<float>(yC) + 1.0f, z1);
                glm::vec3 v2(x1, static_cast<float>(yC) + 1.0f, z1);
                glm::vec3 v3(x1, static_cast<float>(yA) + 1.0f, z1);
                addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.80f, 0.0f));
            }
        }
    }

    return mesh;
}

ChunkMesh ChunkMesher::generateImposterMesh(const ChunkCoord& coord, const TerrainGen& terrainGen) {
    ChunkMesh mesh;
    int worldX = coord.cx * CHUNK_SIZE_X;
    int worldZ = coord.cz * CHUNK_SIZE_Z;

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

    auto addWaterQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                            const glm::vec4& uv, const glm::vec3& normal,
                            const glm::vec3& c) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.waterVertices.size());
        mesh.waterVertices.push_back({v0, glm::vec2(uv.x, uv.w), normal, c});
        mesh.waterVertices.push_back({v1, glm::vec2(uv.x, uv.y), normal, c});
        mesh.waterVertices.push_back({v2, glm::vec2(uv.z, uv.y), normal, c});
        mesh.waterVertices.push_back({v3, glm::vec2(uv.z, uv.w), normal, c});
        mesh.waterIndices.push_back(baseIdx + 0);
        mesh.waterIndices.push_back(baseIdx + 1);
        mesh.waterIndices.push_back(baseIdx + 2);
        mesh.waterIndices.push_back(baseIdx + 2);
        mesh.waterIndices.push_back(baseIdx + 3);
        mesh.waterIndices.push_back(baseIdx + 0);
    };

    const glm::vec3 nTop(0.0f, 1.0f, 0.0f);
    const int step = 8; // 2x2 coarse cells per chunk
    const float fStep = static_cast<float>(step);

    int solidHeightGrid[16][16];
    BlockType solidTypeGrid[16][16];
    bool hasWaterGrid[16][16];

    for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
        for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
            int topY = -1;
            BlockType topType = BlockType::Air;
            bool hasWater = false;

            for (int dx = 0; dx < step; ++dx) {
                for (int dz = 0; dz < step; ++dz) {
                    int wx = worldX + lx + dx;
                    int wz = worldZ + lz + dz;
                    int h = terrainGen.getHeight(wx, wz);
                    if (h <= 44) {
                        hasWater = true;
                    }
                    if (h > topY) {
                        topY = h;
                        if (h <= 44) topType = BlockType::Sand;
                        else if (h <= 46) topType = BlockType::Sand;
                        else if (h >= 74) topType = BlockType::Snow;
                        else topType = BlockType::Grass;
                    }
                }
            }
            solidHeightGrid[lx][lz] = (topY <= 44) ? std::min(topY, 41) : topY;
            solidTypeGrid[lx][lz] = (topY <= 44) ? BlockType::Sand : topType;
            hasWaterGrid[lx][lz] = hasWater;
        }
    }

    // 1. Solid Terrain Surface Quads
    for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
        for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
            int topY = solidHeightGrid[lx][lz];
            BlockType solidType = solidTypeGrid[lx][lz];

            int wx = worldX + lx;
            int wz = worldZ + lz;
            float x0 = static_cast<float>(wx) + getRowXOffset(wz);
            float x1 = x0 + fStep;
            float z0 = static_cast<float>(wz) * TRI_HEIGHT;
            float z1 = static_cast<float>(wz + step) * TRI_HEIGHT;
            float py = static_cast<float>(topY);

            // A. Solid Terrain (Ground / Seabed)
            glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(solidType, 0));
            glm::vec3 v0(x0, py + 1.0f, z0);
            glm::vec3 v1(x0, py + 1.0f, z1);
            glm::vec3 v2(x1, py + 1.0f, z1);
            glm::vec3 v3(x1, py + 1.0f, z0);
            addOpaqueQuad(v0, v1, v2, v3, uv, nTop, glm::vec3(1.0f, 1.0f, 0.0f));

            // Generous border skirts down towards sea level so imposter solid terrain never floats
            glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(solidType, 2));
            float skirtDepth = std::max(16.0f, py - 28.0f);

            if (lz == 0) {
                glm::vec3 vB0(x0, py + 1.0f - skirtDepth, z0);
                glm::vec3 vT0(x0, py + 1.0f, z0);
                glm::vec3 vT1(x1, py + 1.0f, z0);
                glm::vec3 vB1(x1, py + 1.0f - skirtDepth, z0);
                addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.80f, 0.0f));
            }
            if (lz + step >= CHUNK_SIZE_Z) {
                glm::vec3 vB0(x1, py + 1.0f - skirtDepth, z1);
                glm::vec3 vT0(x1, py + 1.0f, z1);
                glm::vec3 vT1(x0, py + 1.0f, z1);
                glm::vec3 vB1(x0, py + 1.0f - skirtDepth, z1);
                addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.80f, 0.0f));
            }
            if (lx == 0) {
                glm::vec3 vB0(x0, py + 1.0f - skirtDepth, z1);
                glm::vec3 vT0(x0, py + 1.0f, z1);
                glm::vec3 vT1(x0, py + 1.0f, z0);
                glm::vec3 vB1(x0, py + 1.0f - skirtDepth, z0);
                addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.65f, 0.0f));
            }
            if (lx + step >= CHUNK_SIZE_X) {
                glm::vec3 vB0(x1, py + 1.0f - skirtDepth, z0);
                glm::vec3 vT0(x1, py + 1.0f, z0);
                glm::vec3 vT1(x1, py + 1.0f, z1);
                glm::vec3 vB1(x1, py + 1.0f - skirtDepth, z1);
                addOpaqueQuad(vB0, vT0, vT1, vB1, sideUV, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.72f, 0.0f));
            }
        }
    }

    // B. Water Surface: Exact equilateral triangular mesh matching LOD0 and nearby chunks at Y = 45.0f
    for (int clx = 0; clx < CHUNK_SIZE_X; ++clx) {
        for (int clz = 0; clz < CHUNK_SIZE_Z; ++clz) {
            int wx = worldX + clx;
            int wz = worldZ + clz;
            int h = terrainGen.getHeight(wx, wz);
            if (h <= 44) {
                for (int s = 0; s < 2; ++s) {
                    glm::vec2 vXZ[3];
                    getPrismVerticesXZ(wx, wz, s, vXZ);

                    float waterY = 44.0f;
                    glm::vec3 T0(vXZ[0].x, waterY + 1.0f, vXZ[0].y);
                    glm::vec3 T1(vXZ[1].x, waterY + 1.0f, vXZ[1].y);
                    glm::vec3 T2(vXZ[2].x, waterY + 1.0f, vXZ[2].y);

                    glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Water, 0));
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

                    float depthFactor = std::clamp((44.0f - static_cast<float>(h)) / 7.0f, 0.1f, 1.0f);
                    glm::vec3 waterCol(depthFactor, 1.0f, 0.0f);
                    addWaterTri(T0, T2, T1, uvT0, uvT2, uvT1, nTop, waterCol);
                }
            }
        }
    }

    // 2. Internal Height Skirts between 8x8 coarse cells for Solid Terrain
    // X boundary between (0, lz) and (8, lz)
    for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
        int yA = solidHeightGrid[0][lz];
        int yB = solidHeightGrid[8][lz];
        if (yA != yB) {
            int wz = worldZ + lz;
            float z0 = static_cast<float>(wz) * TRI_HEIGHT;
            float z1 = static_cast<float>(wz + step) * TRI_HEIGHT;
            float x1 = static_cast<float>(worldX) + getRowXOffset(wz) + fStep;

            if (yA > yB) {
                BlockType bType = solidTypeGrid[0][lz];
                glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                glm::vec3 v0(x1, static_cast<float>(yB) + 1.0f, z0);
                glm::vec3 v1(x1, static_cast<float>(yA) + 1.0f, z0);
                glm::vec3 v2(x1, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v3(x1, static_cast<float>(yB) + 1.0f, z1);
                addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.72f, 0.0f));
            } else {
                BlockType bType = solidTypeGrid[8][lz];
                glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                glm::vec3 v0(x1, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v1(x1, static_cast<float>(yB) + 1.0f, z1);
                glm::vec3 v2(x1, static_cast<float>(yB) + 1.0f, z0);
                glm::vec3 v3(x1, static_cast<float>(yA) + 1.0f, z0);
                addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.65f, 0.0f));
            }
        }
    }

    // Z boundary between (lx, 0) and (lx, 8)
    for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
        int yA = solidHeightGrid[lx][0];
        int yC = solidHeightGrid[lx][8];
        if (yA != yC) {
            int wx = worldX + lx;
            float x0 = static_cast<float>(wx) + getRowXOffset(worldZ);
            float x1 = x0 + fStep;
            float z1 = static_cast<float>(worldZ + step) * TRI_HEIGHT;

            if (yA > yC) {
                BlockType bType = solidTypeGrid[lx][0];
                glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                glm::vec3 v0(x1, static_cast<float>(yC) + 1.0f, z1);
                glm::vec3 v1(x1, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v2(x0, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v3(x0, static_cast<float>(yC) + 1.0f, z1);
                addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.80f, 0.0f));
            } else {
                BlockType bType = solidTypeGrid[lx][8];
                glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                glm::vec3 v0(x0, static_cast<float>(yA) + 1.0f, z1);
                glm::vec3 v1(x0, static_cast<float>(yC) + 1.0f, z1);
                glm::vec3 v2(x1, static_cast<float>(yC) + 1.0f, z1);
                glm::vec3 v3(x1, static_cast<float>(yA) + 1.0f, z1);
                addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.80f, 0.0f));
            }
        }
    }

    return mesh;
}

} // namespace prismcraft
