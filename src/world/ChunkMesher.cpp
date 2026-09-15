#include "ChunkMesher.hpp"
#include "Chunk.hpp"
#include "Cell.hpp"
#include "Coordinates.hpp"
#include "world/World.hpp"
#include "world/TerrainGen.hpp"
#include "world/Biome.hpp"
#include "renderer/TextureAtlas.hpp"
#include "data/BlockRegistry.hpp"
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

    auto addOpaqueQuadAdv = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                                const glm::vec4& uv,
                                const glm::vec3& n0, const glm::vec3& n1, const glm::vec3& n2, const glm::vec3& n3,
                                const glm::vec3& c0, const glm::vec3& c1, const glm::vec3& c2, const glm::vec3& c3) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.opaqueVertices.size());
        mesh.opaqueVertices.push_back({v0, glm::vec2(uv.x, uv.w), n0, c0});
        mesh.opaqueVertices.push_back({v1, glm::vec2(uv.x, uv.y), n1, c1});
        mesh.opaqueVertices.push_back({v2, glm::vec2(uv.z, uv.y), n2, c2});
        mesh.opaqueVertices.push_back({v3, glm::vec2(uv.z, uv.w), n3, c3});
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

    auto addWaterWallQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                                const glm::vec4& uv, const glm::vec3& normBot, const glm::vec3& normTop,
                                const glm::vec3& c0, const glm::vec3& c1, const glm::vec3& c2, const glm::vec3& c3) {
        uint32_t baseIdx = static_cast<uint32_t>(mesh.waterVertices.size());
        mesh.waterVertices.push_back({v0, glm::vec2(uv.x, uv.w), normBot, c0});
        mesh.waterVertices.push_back({v1, glm::vec2(uv.x, uv.y), normTop, c1});
        mesh.waterVertices.push_back({v2, glm::vec2(uv.z, uv.y), normTop, c2});
        mesh.waterVertices.push_back({v3, glm::vec2(uv.z, uv.w), normBot, c3});
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
        if (c.isLeaves() && n.isLeaves()) return true; // Fancy leaves: draw transparent cutouts!
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

                    // 1. Foliage Rendering (Crossed Quads with alpha cutout)
                    if (cell.isFoliage()) {
                        glm::vec3 center = cellToWorldCenter(wx, y, wz, s);
                        center.y = static_cast<float>(y); // Base at ground level
                        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 0));
                        float hw = 0.38f;
                        float fh = 0.85f;
                        float sunlight = getSunlightFactor(x, y, z);
                        float torchL = getTorchLight(center);
                        glm::vec3 folColorBot(sunlight, 0.72f, torchL);
                        glm::vec3 folColorTop(sunlight, 1.0f, torchL);

                        // Foliage normals: bottom anchored (normLen 1.0 -> no sway), top sways (normLen 2.0 -> full wave)
                        glm::vec3 nFolBot = nTop * 1.0f;
                        glm::vec3 nFolTop = nTop * 2.0f;

                        // Diagonal quad 1 (Double sided)
                        addOpaqueQuadAdv(center + glm::vec3(-hw, 0.0f, -hw), center + glm::vec3(-hw, fh, -hw),
                                         center + glm::vec3(hw, fh, hw), center + glm::vec3(hw, 0.0f, hw),
                                         uv, nFolBot, nFolTop, nFolTop, nFolBot, folColorBot, folColorTop, folColorTop, folColorBot);
                        addOpaqueQuadAdv(center + glm::vec3(hw, 0.0f, hw), center + glm::vec3(hw, fh, hw),
                                         center + glm::vec3(-hw, fh, -hw), center + glm::vec3(-hw, 0.0f, -hw),
                                         uv, nFolBot, nFolTop, nFolTop, nFolBot, folColorBot, folColorTop, folColorTop, folColorBot);

                        // Diagonal quad 2 (Double sided)
                        addOpaqueQuadAdv(center + glm::vec3(-hw, 0.0f, hw), center + glm::vec3(-hw, fh, hw),
                                         center + glm::vec3(hw, fh, -hw), center + glm::vec3(hw, 0.0f, -hw),
                                         uv, nFolBot, nFolTop, nFolTop, nFolBot, folColorBot, folColorTop, folColorTop, folColorBot);
                        addOpaqueQuadAdv(center + glm::vec3(hw, 0.0f, -hw), center + glm::vec3(hw, fh, -hw),
                                         center + glm::vec3(-hw, fh, hw), center + glm::vec3(-hw, 0.0f, hw),
                                         uv, nFolBot, nFolTop, nFolTop, nFolBot, folColorBot, folColorTop, folColorTop, folColorBot);
                        continue;
                    }

                    // 1b. True 3D Torch Model (Floor standing or wall-mounted angled into the room)
                    if (cell.isTorch()) {
                        glm::vec3 center = cellToWorldCenter(wx, y, wz, s);
                        uint8_t attachment = cell.getTorchAttachment(); // 0 = floor, 1 = Base wall, 2 = Left wall, 3 = Right wall

                        glm::vec3 pBase;
                        glm::vec3 torchAxis;
                        glm::vec3 sideDir;
                        glm::vec3 frontDir;

                        if (attachment == 0) {
                            // Floor torch: standing vertical in cell centroid
                            pBase = glm::vec3(center.x, static_cast<float>(y), center.z);
                            torchAxis = glm::vec3(0.0f, 1.0f, 0.0f);
                            sideDir   = glm::vec3(1.0f, 0.0f, 0.0f);
                            frontDir  = glm::vec3(0.0f, 0.0f, 1.0f);
                        } else {
                            // Wall torch: attached to wall, leaning ~22 degrees inward
                            glm::vec2 vXZ[3];
                            getPrismVerticesXZ(wx, wz, s, vXZ);

                            glm::vec2 wP0, wP1;
                            glm::vec3 wallDir; // Direction pointing INTO the cell from the wall

                            if (attachment == 1) {
                                // Base wall
                                if (s == 0) { wP0 = vXZ[0]; wP1 = vXZ[1]; wallDir = glm::vec3(0.0f, 0.0f, 1.0f); }
                                else        { wP0 = vXZ[1]; wP1 = vXZ[2]; wallDir = glm::vec3(0.0f, 0.0f, -1.0f); }
                            } else if (attachment == 2) {
                                // Left slanted wall
                                wP0 = vXZ[0]; wP1 = vXZ[2];
                                wallDir = (s == 0) ? glm::vec3(SQRT_3_OVER_2, 0.0f, -0.5f) : glm::vec3(SQRT_3_OVER_2, 0.0f, 0.5f);
                            } else {
                                // Right slanted wall
                                if (s == 0) { wP0 = vXZ[1]; wP1 = vXZ[2]; wallDir = glm::vec3(-SQRT_3_OVER_2, 0.0f, -0.5f); }
                                else        { wP0 = vXZ[0]; wP1 = vXZ[1]; wallDir = glm::vec3(-SQRT_3_OVER_2, 0.0f, 0.5f); }
                            }

                            glm::vec2 wallMid = (wP0 + wP1) * 0.5f;
                            pBase = glm::vec3(wallMid.x, static_cast<float>(y) + 0.22f, wallMid.y) + wallDir * 0.04f;

                            torchAxis = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f) + wallDir * 0.38f);
                            sideDir = glm::cross(torchAxis, wallDir);
                            if (glm::length(sideDir) > 0.01f) {
                                sideDir = glm::normalize(sideDir);
                            } else {
                                sideDir = glm::vec3(1.0f, 0.0f, 0.0f);
                            }
                            frontDir = glm::cross(sideDir, torchAxis);
                        }

                        // Geometry dimensions: authentic Minecraft 2x10 pixel 3D torch cuboid
                        float hw = 0.0625f;  // 1/16 block (2/16 = 0.125m total width)
                        float hTotal = 0.625f; // 10/16 block (0.625m height)

                        glm::vec4 uvTorch = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 0));
                        float uSpan = (uvTorch.z - uvTorch.x);
                        float vSpan = (uvTorch.w - uvTorch.y);

                        // 4 Side walls: columns 7..8 (u: 7/16 to 9/16), rows 6..15 (v: 6/16 to 16/16)
                        glm::vec4 uvSide(uvTorch.x + uSpan * (7.0f / 16.0f),
                                         uvTorch.y + vSpan * (6.0f / 16.0f),
                                         uvTorch.x + uSpan * (9.0f / 16.0f),
                                         uvTorch.w);

                        // Top cap: columns 7..8, rows 6..7 (flame top tip)
                        glm::vec4 uvTop(uvTorch.x + uSpan * (7.0f / 16.0f),
                                        uvTorch.y + vSpan * (6.0f / 16.0f),
                                        uvTorch.x + uSpan * (9.0f / 16.0f),
                                        uvTorch.y + vSpan * (8.0f / 16.0f));

                        // Bottom cap: columns 7..8, rows 14..15 (stick base)
                        glm::vec4 uvBot(uvTorch.x + uSpan * (7.0f / 16.0f),
                                        uvTorch.y + vSpan * (14.0f / 16.0f),
                                        uvTorch.x + uSpan * (9.0f / 16.0f),
                                        uvTorch.w);

                        glm::vec3 flameCol(1.0f, 1.0f, 1.0f); // 100% emissive flame radiance
                        if (cell.type == BlockType::TorchSoul) {
                            flameCol = glm::vec3(0.3f, 0.95f, 1.0f);
                        } else if (cell.type == BlockType::TorchRedstone) {
                            flameCol = glm::vec3(1.0f, 0.2f, 0.2f);
                        }

                        auto getP = [&](float dx, float dy, float dz) -> glm::vec3 {
                            return pBase + sideDir * dx + torchAxis * dy + frontDir * dz;
                        };

                        auto addTorchQuad = [&](const glm::vec3& bl, const glm::vec3& tl, const glm::vec3& tr, const glm::vec3& br,
                                                const glm::vec4& uv, const glm::vec3& norm, const glm::vec3& col) {
                            addOpaqueQuad(bl, tl, tr, br, uv, norm, col, col, col, col);
                        };

                        // 1. Bottom Cap (-torchAxis)
                        addTorchQuad(getP(-hw, 0.0f, -hw), getP(-hw, 0.0f,  hw), getP( hw, 0.0f,  hw), getP( hw, 0.0f, -hw),
                                     uvBot, -torchAxis, flameCol);

                        // 2. Top Cap (+torchAxis)
                        addTorchQuad(getP(-hw, hTotal,  hw), getP(-hw, hTotal, -hw), getP( hw, hTotal, -hw), getP( hw, hTotal,  hw),
                                     uvTop, torchAxis, flameCol);

                        // 3. Front Face (+frontDir)
                        addTorchQuad(getP( hw, 0.0f,  hw), getP( hw, hTotal,  hw), getP(-hw, hTotal,  hw), getP(-hw, 0.0f,  hw),
                                     uvSide, frontDir, flameCol);

                        // 4. Back Face (-frontDir)
                        addTorchQuad(getP(-hw, 0.0f, -hw), getP(-hw, hTotal, -hw), getP( hw, hTotal, -hw), getP( hw, 0.0f, -hw),
                                     uvSide, -frontDir, flameCol);

                        // 5. Right Face (+sideDir)
                        addTorchQuad(getP( hw, 0.0f, -hw), getP( hw, hTotal, -hw), getP( hw, hTotal,  hw), getP( hw, 0.0f,  hw),
                                     uvSide, sideDir, flameCol);

                        // 6. Left Face (-sideDir)
                        addTorchQuad(getP(-hw, 0.0f,  hw), getP(-hw, hTotal,  hw), getP(-hw, hTotal, -hw), getP(-hw, 0.0f, -hw),
                                     uvSide, -sideDir, flameCol);

                        continue;
                    }

                    // 1b-2. True 3D Lantern Model (Standing on floor or hanging from ceiling)
                    if (cell.isLantern()) {
                        glm::vec2 vXZ[3];
                        getPrismVerticesXZ(wx, wz, s, vXZ);
                        glm::vec2 cent = (vXZ[0] + vXZ[1] + vXZ[2]) / 3.0f;

                        bool isHanging = false;
                        if (y + 1 < CHUNK_SIZE_Y) {
                            Cell cellAbove = chunk.getCell(x, y + 1, z, s);
                            isHanging = cellAbove.isSolid() && cellAbove.isOpaque();
                        }

                        float py = static_cast<float>(y);
                        float halfW = 0.17f;
                        float hBody = 0.44f;
                        float yBottom = isHanging ? (py + 1.0f - hBody - 0.12f) : py;
                        float yTop = yBottom + hBody;

                        int tileLantern = TextureAtlas::getTileForBlock(cell.type, 0);
                        glm::vec4 uv = TextureAtlas::getTileUV(tileLantern);

                        glm::vec3 lantCol = (cell.type == BlockType::LanternSoul)
                            ? glm::vec3(0.3f, 1.0f, 1.0f)
                            : glm::vec3(1.0f, 1.0f, 0.95f);

                        // 4 side walls of lantern box
                        addOpaqueQuad(glm::vec3(cent.x - halfW, yBottom, cent.y + halfW),
                                      glm::vec3(cent.x - halfW, yTop,    cent.y + halfW),
                                      glm::vec3(cent.x + halfW, yTop,    cent.y + halfW),
                                      glm::vec3(cent.x + halfW, yBottom, cent.y + halfW),
                                      uv, glm::vec3(0, 0, 1), lantCol, lantCol, lantCol, lantCol);
                        addOpaqueQuad(glm::vec3(cent.x + halfW, yBottom, cent.y - halfW),
                                      glm::vec3(cent.x + halfW, yTop,    cent.y - halfW),
                                      glm::vec3(cent.x - halfW, yTop,    cent.y - halfW),
                                      glm::vec3(cent.x - halfW, yBottom, cent.y - halfW),
                                      uv, glm::vec3(0, 0, -1), lantCol, lantCol, lantCol, lantCol);
                        addOpaqueQuad(glm::vec3(cent.x + halfW, yBottom, cent.y + halfW),
                                      glm::vec3(cent.x + halfW, yTop,    cent.y + halfW),
                                      glm::vec3(cent.x + halfW, yTop,    cent.y - halfW),
                                      glm::vec3(cent.x + halfW, yBottom, cent.y - halfW),
                                      uv, glm::vec3(1, 0, 0), lantCol, lantCol, lantCol, lantCol);
                        addOpaqueQuad(glm::vec3(cent.x - halfW, yBottom, cent.y - halfW),
                                      glm::vec3(cent.x - halfW, yTop,    cent.y - halfW),
                                      glm::vec3(cent.x - halfW, yTop,    cent.y + halfW),
                                      glm::vec3(cent.x - halfW, yBottom, cent.y + halfW),
                                      uv, glm::vec3(-1, 0, 0), lantCol, lantCol, lantCol, lantCol);

                        // Top & Bottom caps
                        addOpaqueQuad(glm::vec3(cent.x - halfW, yTop, cent.y + halfW),
                                      glm::vec3(cent.x + halfW, yTop, cent.y + halfW),
                                      glm::vec3(cent.x + halfW, yTop, cent.y - halfW),
                                      glm::vec3(cent.x - halfW, yTop, cent.y - halfW),
                                      uv, glm::vec3(0, 1, 0), lantCol, lantCol, lantCol, lantCol);
                        addOpaqueQuad(glm::vec3(cent.x - halfW, yBottom, cent.y - halfW),
                                      glm::vec3(cent.x + halfW, yBottom, cent.y - halfW),
                                      glm::vec3(cent.x + halfW, yBottom, cent.y + halfW),
                                      glm::vec3(cent.x - halfW, yBottom, cent.y + halfW),
                                      uv, glm::vec3(0, -1, 0), lantCol, lantCol, lantCol, lantCol);

                        // Chain if hanging
                        if (isHanging) {
                            float chW = 0.035f;
                            addOpaqueQuad(glm::vec3(cent.x - chW, yTop, cent.y),
                                          glm::vec3(cent.x - chW, py + 1.0f, cent.y),
                                          glm::vec3(cent.x + chW, py + 1.0f, cent.y),
                                          glm::vec3(cent.x + chW, yTop, cent.y),
                                          uv, glm::vec3(0, 0, 1), lantCol * 0.7f, lantCol * 0.7f, lantCol * 0.7f, lantCol * 0.7f);
                        }

                        continue;
                    }

                    // 1b-3. True 3D Trapdoor Model (Horizontal closed hatch or vertical open flap)
                    if (cell.isTrapdoor()) {
                        bool isOpen = cell.isTrapdoorOpen();
                        uint8_t facing = cell.getTrapdoorFacing(); // 0 = Base wall, 1 = Left wall, 2 = Right wall
                        glm::vec2 vXZ[3];
                        getPrismVerticesXZ(wx, wz, s, vXZ);

                        float py = static_cast<float>(y);
                        float thick = 0.1875f; // 3/16 block thickness (authentic Minecraft)

                        int tileTrapdoor = TextureAtlas::getTileForBlock(cell.type, 0);
                        glm::vec4 uvTrapdoor = TextureAtlas::getTileUV(tileTrapdoor);

                        // Wood-matching plank texture for edges
                        std::string plankName = "oak_planks";
                        if (cell.type == BlockType::TrapdoorSpruce) plankName = "spruce_planks";
                        else if (cell.type == BlockType::TrapdoorBirch) plankName = "birch_planks";
                        else if (cell.type == BlockType::TrapdoorJungle) plankName = "jungle_planks";
                        else if (cell.type == BlockType::TrapdoorAcacia) plankName = "acacia_planks";
                        else if (cell.type == BlockType::TrapdoorDarkOak) plankName = "dark_oak_planks";
                        else if (cell.type == BlockType::TrapdoorIron) plankName = "iron_block";

                        int tilePlank = BlockRegistry::getTextureTile(plankName);
                        if (tilePlank < 0) tilePlank = (cell.type == BlockType::TrapdoorIron) ? 22 : 4;
                        glm::vec4 rawUvPlank = TextureAtlas::getTileUV(tilePlank);
                        float edgeVSpan = (rawUvPlank.w - rawUvPlank.y) * (thick / 1.0f);
                        glm::vec4 uvEdge(rawUvPlank.x, rawUvPlank.y, rawUvPlank.z, rawUvPlank.y + edgeVSpan);

                        // Wall vertices w0 and w1, and inward normal wallIn
                        glm::vec2 w0, w1;
                        glm::vec2 wallIn;
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
                            // 3D Horizontal Slab on the floor (thickness 0.1875m)
                            glm::vec3 b0(vXZ[0].x, py, vXZ[0].y);
                            glm::vec3 b1(vXZ[1].x, py, vXZ[1].y);
                            glm::vec3 b2(vXZ[2].x, py, vXZ[2].y);

                            glm::vec3 t0(vXZ[0].x, py + thick, vXZ[0].y);
                            glm::vec3 t1(vXZ[1].x, py + thick, vXZ[1].y);
                            glm::vec3 t2(vXZ[2].x, py + thick, vXZ[2].y);

                            glm::vec2 uvT0, uvT1, uvT2;
                            if (s == 0) {
                                uvT0 = glm::vec2(uvTrapdoor.x, uvTrapdoor.y);
                                uvT1 = glm::vec2(uvTrapdoor.z, uvTrapdoor.y);
                                uvT2 = glm::vec2(uvTrapdoor.x, uvTrapdoor.w);
                            } else {
                                uvT0 = glm::vec2(uvTrapdoor.z, uvTrapdoor.y);
                                uvT1 = glm::vec2(uvTrapdoor.z, uvTrapdoor.w);
                                uvT2 = glm::vec2(uvTrapdoor.x, uvTrapdoor.w);
                            }

                            auto [sTop, tTop] = getVertexLight((t0 + t1 + t2) / 3.0f, glm::vec3(0, 1, 0));
                            glm::vec3 colTop(sTop, 1.0f, tTop);
                            addOpaqueTri(t0, t2, t1, uvT0, uvT2, uvT1, glm::vec3(0, 1, 0), colTop, colTop, colTop);

                            auto [sBot, tBot] = getVertexLight((b0 + b1 + b2) / 3.0f, glm::vec3(0, -1, 0));
                            glm::vec3 colBot(sBot, 1.0f, tBot);
                            addOpaqueTri(b0, b1, b2, uvT0, uvT1, uvT2, glm::vec3(0, -1, 0), colBot, colBot, colBot);

                            // 3 Side edge quads with proportional wood plank texture
                            glm::vec2 e0 = vXZ[1] - vXZ[0];
                            glm::vec3 n0 = glm::normalize(glm::vec3(e0.y, 0.0f, -e0.x));
                            auto [s0, torch0] = getVertexLight((b0 + b1 + t1 + t0) * 0.25f, n0);
                            glm::vec3 col0(s0, 1.0f, torch0);
                            addOpaqueQuad(b0, t0, t1, b1, uvEdge, n0, col0, col0, col0, col0);

                            glm::vec2 e1 = vXZ[2] - vXZ[1];
                            glm::vec3 n1 = glm::normalize(glm::vec3(e1.y, 0.0f, -e1.x));
                            auto [s1, torch1] = getVertexLight((b1 + b2 + t2 + t1) * 0.25f, n1);
                            glm::vec3 col1(s1, 1.0f, torch1);
                            addOpaqueQuad(b1, t1, t2, b2, uvEdge, n1, col1, col1, col1, col1);

                            glm::vec2 e2 = vXZ[0] - vXZ[2];
                            glm::vec3 n2 = glm::normalize(glm::vec3(e2.y, 0.0f, -e2.x));
                            auto [s2, torch2] = getVertexLight((b2 + b0 + t0 + t2) * 0.25f, n2);
                            glm::vec3 col2(s2, 1.0f, torch2);
                            addOpaqueQuad(b2, t2, t0, b0, uvEdge, n2, col2, col2, col2, col2);
                        } else {
                            // Full 3D Upright Open Flap (6 faces, thickness 0.1875m) attached to hinge wall
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

                            glm::vec3 normFront = glm::normalize(vecThick);
                            glm::vec3 normBack  = -normFront;
                            glm::vec3 normLeft  = -glm::normalize(vecSpan);
                            glm::vec3 normRight =  glm::normalize(vecSpan);

                            auto [sf, tf] = getVertexLight((c3 + c7 + c6 + c2) * 0.25f, normFront);
                            glm::vec3 colFront(sf, 1.0f, tf);

                            auto [sb, tb] = getVertexLight((c0 + c1 + c5 + c4) * 0.25f, normBack);
                            glm::vec3 colBack(sb, 1.0f, tb);

                            auto [sl, tl] = getVertexLight((c0 + c4 + c7 + c3) * 0.25f, normLeft);
                            glm::vec3 colLeft(sl, 1.0f, tl);

                            auto [sr, tr] = getVertexLight((c1 + c2 + c6 + c5) * 0.25f, normRight);
                            glm::vec3 colRight(sr, 1.0f, tr);

                            auto [st, tt] = getVertexLight((c4 + c5 + c6 + c7) * 0.25f, glm::vec3(0, 1, 0));
                            glm::vec3 colTop(st, 1.0f, tt);

                            auto [sbot, tbot] = getVertexLight((c0 + c1 + c2 + c3) * 0.25f, glm::vec3(0, -1, 0));
                            glm::vec3 colBot(sbot, 1.0f, tbot);

                            // 1. Front face (facing into cell room)
                            addOpaqueQuad(c3, c7, c6, c2, uvTrapdoor, normFront, colFront, colFront, colFront, colFront);

                            // 2. Back face (facing wall)
                            glm::vec4 uvTrapdoorFlip(uvTrapdoor.z, uvTrapdoor.y, uvTrapdoor.x, uvTrapdoor.w);
                            addOpaqueQuad(c1, c5, c4, c0, uvTrapdoorFlip, normBack, colBack, colBack, colBack, colBack);

                            // 3. Left edge (at w0)
                            addOpaqueQuad(c0, c4, c7, c3, uvEdge, normLeft, colLeft, colLeft, colLeft, colLeft);

                            // 4. Right edge (at w1)
                            addOpaqueQuad(c2, c6, c5, c1, uvEdge, normRight, colRight, colRight, colRight, colRight);

                            // 5. Top edge
                            addOpaqueQuad(c7, c4, c5, c6, uvEdge, glm::vec3(0, 1, 0), colTop, colTop, colTop, colTop);

                            // 6. Bottom edge (floor hinge)
                            addOpaqueQuad(c3, c2, c1, c0, uvEdge, glm::vec3(0, -1, 0), colBot, colBot, colBot, colBot);
                        }

                        continue;
                    }

                    // 1c. True 3D Door Model (Wooden & Iron Doors, Open/Closed Swing)
                    if (cell.isDoor()) {
                        bool isUpper = cell.isDoorUpper();
                        bool isOpen = cell.isDoorOpen();
                        uint8_t facing = cell.getDoorFacing(); // 0 = Base wall, 1 = Left wall, 2 = Right wall

                        glm::vec2 vXZ[3];
                        getPrismVerticesXZ(wx, wz, s, vXZ);

                        // Wall vertices w0 and w1
                        glm::vec2 w0, w1;
                        glm::vec2 wallIn; // Direction pointing INTO cell from the wall

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
                        float doorThick = 0.1875f; // Authentic 3/16 block thickness
                        float doorHeight = 1.0f;
                        float py = static_cast<float>(y);

                        // Orientation vectors
                        glm::vec3 vecSpan;
                        glm::vec3 vecThick;
                        glm::vec3 pBase;

                        if (!isOpen) {
                            // Closed: spans along the doorway from w0 to w1
                            vecSpan = glm::vec3(wallDir.x, 0.0f, wallDir.y) * doorWidth;
                            vecThick = glm::vec3(wallIn.x, 0.0f, wallIn.y) * doorThick;
                            pBase = glm::vec3(w0.x, py, w0.y) + glm::vec3(wallIn.x, 0.0f, wallIn.y) * 0.02f;
                        } else {
                            // Open: pivoted 90 degrees inward around hinge w0
                            vecSpan = glm::vec3(wallIn.x, 0.0f, wallIn.y) * doorWidth;
                            vecThick = glm::vec3(-wallDir.x, 0.0f, -wallDir.y) * doorThick;
                            pBase = glm::vec3(w0.x, py, w0.y) + glm::vec3(wallIn.x, 0.0f, wallIn.y) * 0.02f;
                        }

                        glm::vec3 vecUp(0.0f, doorHeight, 0.0f);

                        // 8 corners of the 3D door panel
                        glm::vec3 c0 = pBase;
                        glm::vec3 c1 = pBase + vecSpan;
                        glm::vec3 c2 = pBase + vecSpan + vecThick;
                        glm::vec3 c3 = pBase + vecThick;

                        glm::vec3 c4 = c0 + vecUp;
                        glm::vec3 c5 = c1 + vecUp;
                        glm::vec3 c6 = c2 + vecUp;
                        glm::vec3 c7 = c3 + vecUp;

                        // Textures
                        int tileDoor = -1;
                        if (BlockRegistry::isInitialized()) {
                            const auto& def = BlockRegistry::getDef(cell.type);
                            const std::string& texName = isUpper ? def.texDoorUpper : def.texDoorLower;
                            if (!texName.empty()) {
                                tileDoor = BlockRegistry::getTextureTile(texName);
                            }
                        }
                        if (tileDoor < 0) {
                            tileDoor = (cell.type == BlockType::DoorIron)
                                ? (isUpper ? TextureAtlas::TILE_DOOR_IRON_UPPER : TextureAtlas::TILE_DOOR_IRON_LOWER)
                                : (isUpper ? TextureAtlas::TILE_DOOR_WOOD_UPPER : TextureAtlas::TILE_DOOR_WOOD_LOWER);
                        }
                        // Pick wood-matching plank texture for door edges
                        std::string edgePlankName = "oak_planks";
                        if (cell.type == BlockType::DoorSpruce) edgePlankName = "spruce_planks";
                        else if (cell.type == BlockType::DoorBirch) edgePlankName = "birch_planks";
                        else if (cell.type == BlockType::DoorJungle) edgePlankName = "jungle_planks";
                        else if (cell.type == BlockType::DoorAcacia) edgePlankName = "acacia_planks";
                        else if (cell.type == BlockType::DoorDarkOak) edgePlankName = "dark_oak_planks";
                        else if (cell.type == BlockType::DoorIron) edgePlankName = "iron_block";

                        int tileEdge = BlockRegistry::getTextureTile(edgePlankName);
                        if (tileEdge < 0) tileEdge = (cell.type == BlockType::DoorIron) ? 22 : 4;

                        glm::vec4 uvDoor = TextureAtlas::getTileUV(tileDoor);
                        glm::vec4 rawUvEdge = TextureAtlas::getTileUV(tileEdge);
                        // Proportional UV width matching door thickness (3/16 wide), preventing texture stretching
                        float edgeUSpan = (rawUvEdge.z - rawUvEdge.x) * (doorThick / 1.0f);
                        glm::vec4 uvEdge(rawUvEdge.x, rawUvEdge.y, rawUvEdge.x + edgeUSpan, rawUvEdge.w);
                        float edgeVSpan = (rawUvEdge.w - rawUvEdge.y) * (doorThick / 1.0f);
                        glm::vec4 uvEdgeCap(rawUvEdge.x, rawUvEdge.y, rawUvEdge.z, rawUvEdge.y + edgeVSpan);

                        // Normals
                        glm::vec3 normFront = -glm::normalize(vecThick);
                        glm::vec3 normBack  =  glm::normalize(vecThick);
                        glm::vec3 normHinge = -glm::normalize(vecSpan);
                        glm::vec3 normLatch =  glm::normalize(vecSpan);

                        // Lighting
                        auto [s0, t0] = getVertexLight((c0 + c1 + c4 + c5) * 0.25f, normFront);
                        glm::vec3 colFront(s0, 1.0f, t0);

                        auto [s1, t1] = getVertexLight((c2 + c3 + c6 + c7) * 0.25f, normBack);
                        glm::vec3 colBack(s1, 1.0f, t1);

                        auto [s2, t2] = getVertexLight((c0 + c3 + c4 + c7) * 0.25f, normHinge);
                        glm::vec3 colHinge(s2, 1.0f, t2);

                        auto [s3, t3] = getVertexLight((c1 + c2 + c5 + c6) * 0.25f, normLatch);
                        glm::vec3 colLatch(s3, 1.0f, t3);

                        auto [sTop, tTop] = getVertexLight((c4 + c5 + c6 + c7) * 0.25f, glm::vec3(0, 1, 0));
                        glm::vec3 colTop(sTop, 1.0f, tTop);

                        auto [sBot, tBot] = getVertexLight((c0 + c1 + c2 + c3) * 0.25f, glm::vec3(0, -1, 0));
                        glm::vec3 colBot(sBot, 1.0f, tBot);

                        // 1. Front face (BL: c0, TL: c4, TR: c5, BR: c1)
                        addOpaqueQuad(c0, c4, c5, c1, uvDoor, normFront, colFront, colFront, colFront, colFront);

                        // 2. Back face (BL: c2, TL: c6, TR: c7, BR: c3) - UV flipped horizontally
                        glm::vec4 uvDoorFlip(uvDoor.z, uvDoor.y, uvDoor.x, uvDoor.w);
                        addOpaqueQuad(c2, c6, c7, c3, uvDoorFlip, normBack, colBack, colBack, colBack, colBack);

                        // 3. Hinge edge (BL: c3, TL: c7, TR: c4, BR: c0)
                        addOpaqueQuad(c3, c7, c4, c0, uvEdge, normHinge, colHinge, colHinge, colHinge, colHinge);

                        // 4. Latch edge (BL: c1, TL: c5, TR: c6, BR: c2)
                        addOpaqueQuad(c1, c5, c6, c2, uvEdge, normLatch, colLatch, colLatch, colLatch, colLatch);

                        // 5. Top edge (only on upper half)
                        if (isUpper) {
                            addOpaqueQuad(c4, c7, c6, c5, uvEdgeCap, glm::vec3(0, 1, 0), colTop, colTop, colTop, colTop);
                        }

                        // 6. Bottom edge (only on lower half)
                        if (!isUpper) {
                            addOpaqueQuad(c0, c1, c2, c3, uvEdgeCap, glm::vec3(0, -1, 0), colBot, colBot, colBot, colBot);
                        }

                        continue;
                    }

                    // 1d. True 3D Bed Model (4-Cell Architecture: 2 Columns = 4 Prisms)
                    // Head column (s=0, s=1) and Foot column (s=0, s=1)
                    if (cell.isBed()) {
                        // To avoid duplicate mesh, each column is meshed only once when s == 0
                        if (s == 1) continue;

                        bool isHead = cell.isBedHead();
                        uint8_t facing = cell.getBedFacing(); // 0: +X, 1: -X, 2: +Z, 3: -Z

                        float x0 = static_cast<float>(wx) + getRowXOffset(wz);
                        float z0 = static_cast<float>(wz) * TRI_HEIGHT;
                        float z1 = static_cast<float>(wz + 1) * TRI_HEIGHT;

                        // 4 Outer corners of the column rhombus: V0(SW), V1(SE), V2(NE), V3(NW)
                        glm::vec2 V0(x0, z0);
                        glm::vec2 V1(x0 + 1.0f, z0);
                        glm::vec2 V2(x0 + 1.5f, z1);
                        glm::vec2 V3(x0 + 0.5f, z1);

                        glm::vec2 P_center = (V0 + V2) * 0.5f;

                        // Exact column corners with NO shrink on the joint seam, eliminating any gap between Head and Foot
                        glm::vec2 C0 = V0;
                        glm::vec2 C1 = V1;
                        glm::vec2 C2 = V2;
                        glm::vec2 C3 = V3;

                        // Identify the 4 ordered corners of this bed half in CCW order:
                        // P_end0 -> P_end1 (Outer end: Headboard or Footboard)
                        // P_end1 -> P_seam1 (Right side skirt)
                        // P_seam1 -> P_seam0 (Internal seam joining the other half)
                        // P_seam0 -> P_end0 (Left side skirt)
                        glm::vec2 P_end0, P_end1, P_seam1, P_seam0;

                        if (facing == 0) { // +X
                            if (isHead) { P_end0 = C1; P_end1 = C2; P_seam1 = C3; P_seam0 = C0; }
                            else        { P_end0 = C3; P_end1 = C0; P_seam1 = C1; P_seam0 = C2; }
                        } else if (facing == 1) { // -X
                            if (isHead) { P_end0 = C3; P_end1 = C0; P_seam1 = C1; P_seam0 = C2; }
                            else        { P_end0 = C1; P_end1 = C2; P_seam1 = C3; P_seam0 = C0; }
                        } else if (facing == 2) { // +Z
                            if (isHead) { P_end0 = C2; P_end1 = C3; P_seam1 = C0; P_seam0 = C1; }
                            else        { P_end0 = C0; P_end1 = C1; P_seam1 = C2; P_seam0 = C3; }
                        } else { // -Z (3)
                            if (isHead) { P_end0 = C0; P_end1 = C1; P_seam1 = C2; P_seam0 = C3; }
                            else        { P_end0 = C2; P_end1 = C3; P_seam1 = C0; P_seam0 = C1; }
                        }

                        float py = static_cast<float>(y);
                        float hLeg = 0.1875f; // 3/16 block legs
                        float hBed = 0.50f;   // 8/16 block mattress top

                        // Textures
                        int tileTop  = isHead ? BlockRegistry::getTextureTile("bed_head_top") : BlockRegistry::getTextureTile("bed_foot_top");
                        int tileSide = BlockRegistry::getTextureTile("bed_side");
                        int tileEnd  = isHead ? BlockRegistry::getTextureTile("bed_head_end") : BlockRegistry::getTextureTile("bed_foot_end");
                        if (tileTop < 0)  tileTop  = isHead ? TextureAtlas::TILE_BED_HEAD_TOP : TextureAtlas::TILE_BED_FOOT_TOP;
                        if (tileSide < 0) tileSide = isHead ? TextureAtlas::TILE_BED_HEAD_SIDE : TextureAtlas::TILE_BED_FOOT_SIDE;
                        if (tileEnd < 0)  tileEnd  = isHead ? TextureAtlas::TILE_BED_HEAD_END : TextureAtlas::TILE_BED_FOOT_END;

                        int tileUnderside = BlockRegistry::getTextureTile("bed_underside");
                        if (tileUnderside < 0) tileUnderside = 4; // Oak Planks fallback
                        glm::vec4 uvUnderside = TextureAtlas::getTileUV(tileUnderside);

                        int tileLeg = BlockRegistry::getTextureTile("bed_leg");
                        if (tileLeg < 0) tileLeg = tileUnderside;
                        glm::vec4 uvLeg = TextureAtlas::getTileUV(tileLeg);

                        int tileLegBot = BlockRegistry::getTextureTile("bed_leg_bottom");
                        if (tileLegBot < 0) tileLegBot = tileLeg;
                        glm::vec4 uvLegBot = TextureAtlas::getTileUV(tileLegBot);

                        glm::vec4 uvTop  = TextureAtlas::getTileUV(tileTop);
                        glm::vec4 uvSide = TextureAtlas::getTileUV(tileSide);
                        glm::vec4 uvEnd  = TextureAtlas::getTileUV(tileEnd);

                        // 3D Positions for mattress top (T) and bottom (B)
                        glm::vec3 T_end0(P_end0.x, py + hBed, P_end0.y);
                        glm::vec3 T_end1(P_end1.x, py + hBed, P_end1.y);
                        glm::vec3 T_seam1(P_seam1.x, py + hBed, P_seam1.y);
                        glm::vec3 T_seam0(P_seam0.x, py + hBed, P_seam0.y);

                        glm::vec3 B_end0(P_end0.x, py + hLeg, P_end0.y);
                        glm::vec3 B_end1(P_end1.x, py + hLeg, P_end1.y);
                        glm::vec3 B_seam1(P_seam1.x, py + hLeg, P_seam1.y);
                        glm::vec3 B_seam0(P_seam0.x, py + hLeg, P_seam0.y);

                        auto [sTop, tTop] = getVertexLight((T_end0 + T_end1 + T_seam1 + T_seam0) * 0.25f, glm::vec3(0, 1, 0));
                        glm::vec3 colTop(sTop, 1.0f, tTop);

                        auto [sBot, tBot] = getVertexLight((B_end0 + B_end1 + B_seam1 + B_seam0) * 0.25f, glm::vec3(0, -1, 0));
                        glm::vec3 colBot(sBot, 1.0f, tBot);

                        // 1. Mattress Top Quad
                        glm::vec2 uvE0(uvTop.x, uvTop.y);
                        glm::vec2 uvE1(uvTop.z, uvTop.y);
                        glm::vec2 uvS1(uvTop.z, uvTop.w);
                        glm::vec2 uvS0(uvTop.x, uvTop.w);

                        if (!isHead) {
                            std::swap(uvE0, uvS0);
                            std::swap(uvE1, uvS1);
                        }

                        addOpaqueTri(T_end0, T_seam1, T_end1, uvE0, uvS1, uvE1, glm::vec3(0, 1, 0), colTop, colTop, colTop);
                        addOpaqueTri(T_end0, T_seam0, T_seam1, uvE0, uvS0, uvS1, glm::vec3(0, 1, 0), colTop, colTop, colTop);

                        // 2. Underside Quad (Bed Underside Planks)
                        glm::vec2 uvU0(uvUnderside.x, uvUnderside.y);
                        glm::vec2 uvU1(uvUnderside.z, uvUnderside.y);
                        glm::vec2 uvU2(uvUnderside.z, uvUnderside.w);
                        glm::vec2 uvU3(uvUnderside.x, uvUnderside.w);
                        addOpaqueTri(B_end0, B_end1, B_seam1, uvU0, uvU1, uvU2, glm::vec3(0, -1, 0), colBot, colBot, colBot);
                        addOpaqueTri(B_end0, B_seam1, B_seam0, uvU0, uvU2, uvU3, glm::vec3(0, -1, 0), colBot, colBot, colBot);

                        // 3. Board face (Headboard on Head, Footboard on Foot)
                        glm::vec2 eEnd = glm::normalize(P_end1 - P_end0);
                        glm::vec3 nEnd(eEnd.y, 0.0f, -eEnd.x);
                        if (glm::dot(glm::vec2(nEnd.x, nEnd.z), P_end0 - P_center) < 0.0f) nEnd = -nEnd;
                        auto [sE, tE] = getVertexLight((B_end0 + T_end0 + T_end1 + B_end1) * 0.25f, nEnd);
                        glm::vec3 colEnd(sE, 1.0f, tE);
                        addOpaqueQuad(B_end0, T_end0, T_end1, B_end1, uvEnd, nEnd, colEnd, colEnd, colEnd, colEnd);

                        // 4. Side Skirts
                        glm::vec2 eSide1 = glm::normalize(P_seam1 - P_end1);
                        glm::vec3 nSide1(eSide1.y, 0.0f, -eSide1.x);
                        if (glm::dot(glm::vec2(nSide1.x, nSide1.z), P_end1 - P_center) < 0.0f) nSide1 = -nSide1;
                        auto [sS1, tS1] = getVertexLight((B_end1 + T_end1 + T_seam1 + B_seam1) * 0.25f, nSide1);
                        glm::vec3 colSide1(sS1, 1.0f, tS1);
                        addOpaqueQuad(B_end1, T_end1, T_seam1, B_seam1, uvSide, nSide1, colSide1, colSide1, colSide1, colSide1);

                        glm::vec2 eSide0 = glm::normalize(P_end0 - P_seam0);
                        glm::vec3 nSide0(eSide0.y, 0.0f, -eSide0.x);
                        if (glm::dot(glm::vec2(nSide0.x, nSide0.z), P_seam0 - P_center) < 0.0f) nSide0 = -nSide0;
                        auto [sS0, tS0] = getVertexLight((B_seam0 + T_seam0 + T_end0 + B_end0) * 0.25f, nSide0);
                        glm::vec3 colSide0(sS0, 1.0f, tS0);
                        addOpaqueQuad(B_seam0, T_seam0, T_end0, B_end0, uvSide, nSide0, colSide0, colSide0, colSide0, colSide0);

                        // 5. Sturdy 3D Corner Legs (3x3 pixels / 0.1875m wide orthogonal square cuboid posts flush with outer edges)
                        float legW = 0.1875f;
                        float legD = 0.1875f;
                        glm::vec3 colLeg = colBot * 0.90f;

                        // Unit vector along the outer end board (from P_end0 towards P_end1)
                        glm::vec2 uEnd = glm::normalize(P_end1 - P_end0);
                        // Unit vector pointing inward into the bed, perpendicular to uEnd
                        glm::vec2 uIn(-uEnd.y, uEnd.x);
                        if (glm::dot(uIn, P_center - P_end0) < 0.0f) {
                            uIn = -uIn;
                        }

                        auto buildAndAddLeg = [&](const glm::vec2& corner, const glm::vec2& dEnd, const glm::vec2& dLen) {
                            glm::vec2 c0 = corner;
                            glm::vec2 c1 = corner + dEnd;
                            glm::vec2 c2 = corner + dEnd + dLen;
                            glm::vec2 c3 = corner + dLen;

                            // Ensure strict CCW ordering in XZ (looking from above): cross product of (c1-c0) and (c3-c0) > 0
                            float k = (c1.x - c0.x) * (c3.y - c0.y) - (c1.y - c0.y) * (c3.x - c0.x);
                            std::array<glm::vec2, 4> pts = (k > 0.0f) ? std::array<glm::vec2, 4>{c0, c1, c2, c3}
                                                                      : std::array<glm::vec2, 4>{c0, c3, c2, c1};

                            // 4 Vertical outward-facing walls with correct CCW winding and outward normals
                            for (int i = 0; i < 4; ++i) {
                                const glm::vec2& pL = pts[i];
                                const glm::vec2& pR = pts[(i + 1) % 4];
                                glm::vec3 bL(pL.x, py, pL.y);
                                glm::vec3 tL(pL.x, py + hLeg, pL.y);
                                glm::vec3 tR(pR.x, py + hLeg, pR.y);
                                glm::vec3 bR(pR.x, py, pR.y);
                                glm::vec2 e = pR - pL;
                                glm::vec3 nOut = glm::normalize(glm::vec3(e.y, 0.0f, -e.x));
                                addOpaqueQuad(bL, tL, tR, bR, uvLeg, nOut, colLeg, colLeg, colLeg, colLeg);
                            }

                            // Bottom cap facing down (-Y, CCW when viewed from below)
                            glm::vec3 v0(pts[0].x, py, pts[0].y);
                            glm::vec3 v1(pts[1].x, py, pts[1].y);
                            glm::vec3 v2(pts[2].x, py, pts[2].y);
                            glm::vec3 v3(pts[3].x, py, pts[3].y);
                            glm::vec2 uvLB0(uvLegBot.x, uvLegBot.y);
                            glm::vec2 uvLB1(uvLegBot.z, uvLegBot.y);
                            glm::vec2 uvLB2(uvLegBot.z, uvLegBot.w);
                            glm::vec2 uvLB3(uvLegBot.x, uvLegBot.w);
                            addOpaqueTri(v0, v3, v2, uvLB0, uvLB3, uvLB2, glm::vec3(0.0f, -1.0f, 0.0f), colLeg, colLeg, colLeg);
                            addOpaqueTri(v0, v2, v1, uvLB0, uvLB2, uvLB1, glm::vec3(0.0f, -1.0f, 0.0f), colLeg, colLeg, colLeg);
                        };

                        // Leg at corner P_end0: extends +uEnd * legW, +uIn * legD
                        buildAndAddLeg(P_end0, uEnd * legW, uIn * legD);
                        // Leg at corner P_end1: extends -uEnd * legW, +uIn * legD
                        buildAndAddLeg(P_end1, -uEnd * legW, uIn * legD);

                        continue;
                    }

                    // 1e. True 3D Cake Model (Partial-height triangular prism)
                    if (cell.type == BlockType::Cake) {
                        glm::vec2 vXZ[3];
                        getPrismVerticesXZ(wx, wz, s, vXZ);

                        glm::vec2 cent = (vXZ[0] + vXZ[1] + vXZ[2]) / 3.0f;
                        float shrink = 0.06f; // 1 pixel inset from edges
                        glm::vec2 c0 = vXZ[0] + glm::normalize(cent - vXZ[0]) * shrink;
                        glm::vec2 c1 = vXZ[1] + glm::normalize(cent - vXZ[1]) * shrink;
                        glm::vec2 c2 = vXZ[2] + glm::normalize(cent - vXZ[2]) * shrink;

                        float py = static_cast<float>(y);
                        float hCake = 0.4375f; // 7/16 block height

                        glm::vec3 B0(c0.x, py, c0.y);
                        glm::vec3 B1(c1.x, py, c1.y);
                        glm::vec3 B2(c2.x, py, c2.y);

                        glm::vec3 T0(c0.x, py + hCake, c0.y);
                        glm::vec3 T1(c1.x, py + hCake, c1.y);
                        glm::vec3 T2(c2.x, py + hCake, c2.y);

                        glm::vec4 uvTop = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 0));
                        glm::vec4 uvBot = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 1));
                        glm::vec4 uvSide = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 2));

                        glm::vec2 uvTop0, uvTop1, uvTop2;
                        if (s == 0) {
                            uvTop0 = glm::vec2(uvTop.x, uvTop.y);
                            uvTop1 = glm::vec2(uvTop.z, uvTop.y);
                            uvTop2 = glm::vec2(uvTop.x, uvTop.w);
                        } else {
                            uvTop0 = glm::vec2(uvTop.z, uvTop.y);
                            uvTop1 = glm::vec2(uvTop.z, uvTop.w);
                            uvTop2 = glm::vec2(uvTop.x, uvTop.w);
                        }

                        glm::vec2 uvBot0, uvBot1, uvBot2;
                        if (s == 0) {
                            uvBot0 = glm::vec2(uvBot.x, uvBot.y);
                            uvBot1 = glm::vec2(uvBot.z, uvBot.y);
                            uvBot2 = glm::vec2(uvBot.x, uvBot.w);
                        } else {
                            uvBot0 = glm::vec2(uvBot.z, uvBot.y);
                            uvBot1 = glm::vec2(uvBot.z, uvBot.w);
                            uvBot2 = glm::vec2(uvBot.x, uvBot.w);
                        }

                        // Top frosting face (CCW winding: T0, T2, T1)
                        auto [sT, tT] = getVertexLight((T0 + T1 + T2) / 3.0f, nTop);
                        glm::vec3 colTop(sT, 1.0f, tT);
                        addOpaqueTri(T0, T2, T1, uvTop0, uvTop2, uvTop1, nTop, colTop, colTop, colTop);

                        // Bottom sponge face (CCW winding: B0, B1, B2)
                        auto [sB, tB] = getVertexLight((B0 + B1 + B2) / 3.0f, nBot);
                        glm::vec3 colBot(sB, 1.0f, tB);
                        addOpaqueTri(B0, B1, B2, uvBot0, uvBot1, uvBot2, nBot, colBot, colBot, colBot);

                        // 3 side walls
                        glm::vec2 e0 = c1 - c0;
                        glm::vec3 n0 = glm::normalize(glm::vec3(e0.y, 0.0f, -e0.x));
                        auto [s0, t0] = getVertexLight((B0 + B1 + T1 + T0) * 0.25f, n0);
                        glm::vec3 cSide0(s0, 1.0f, t0);
                        addOpaqueQuad(B0, B1, T1, T0, uvSide, n0, cSide0, cSide0, cSide0, cSide0);

                        glm::vec2 e1 = c2 - c1;
                        glm::vec3 n1 = glm::normalize(glm::vec3(e1.y, 0.0f, -e1.x));
                        auto [s1, t1] = getVertexLight((B1 + B2 + T2 + T1) * 0.25f, n1);
                        glm::vec3 cSide1(s1, 1.0f, t1);
                        addOpaqueQuad(B1, B2, T2, T1, uvSide, n1, cSide1, cSide1, cSide1, cSide1);

                        glm::vec2 e2 = c0 - c2;
                        glm::vec3 n2 = glm::normalize(glm::vec3(e2.y, 0.0f, -e2.x));
                        auto [s2, t2] = getVertexLight((B2 + B0 + T0 + T2) * 0.25f, n2);
                        glm::vec3 cSide2(s2, 1.0f, t2);
                        addOpaqueQuad(B2, B0, T0, T2, uvSide, n2, cSide2, cSide2, cSide2, cSide2);

                        continue;
                    }

                    // 1f. True 3D Cactus Model (14x14 proportions, inset by 0.0625m, seamless column join)
                    if (cell.isCactus()) {
                        float x0 = static_cast<float>(wx) + getRowXOffset(wz);
                        float z0 = static_cast<float>(wz) * TRI_HEIGHT;
                        float z1 = static_cast<float>(wz + 1) * TRI_HEIGHT;

                        glm::vec2 V0(x0, z0);
                        glm::vec2 V1(x0 + 1.0f, z0);
                        glm::vec2 V2(x0 + 1.5f, z1);
                        glm::vec2 V3(x0 + 0.5f, z1);

                        glm::vec2 P_cent = (V0 + V2) * 0.5f;
                        float d = 0.0625f; // 1/16 block inset

                        // Corner insets: acute corners inset along bisector by 2d, obtuse corners by d / sin(60)
                        glm::vec2 C0 = V0 + glm::normalize(P_cent - V0) * (2.0f * d);
                        glm::vec2 C2 = V2 + glm::normalize(P_cent - V2) * (2.0f * d);
                        glm::vec2 C1 = V1 + glm::normalize(P_cent - V1) * (d / SQRT_3_OVER_2);
                        glm::vec2 C3 = V3 + glm::normalize(P_cent - V3) * (d / SQRT_3_OVER_2);

                        glm::vec2 cXZ[3];
                        if (s == 0) {
                            cXZ[0] = C0; cXZ[1] = C1; cXZ[2] = C3;
                        } else {
                            cXZ[0] = C1; cXZ[1] = C2; cXZ[2] = C3;
                        }

                        float py = static_cast<float>(y);
                        glm::vec3 B0(cXZ[0].x, py, cXZ[0].y);
                        glm::vec3 B1(cXZ[1].x, py, cXZ[1].y);
                        glm::vec3 B2(cXZ[2].x, py, cXZ[2].y);

                        glm::vec3 T0(cXZ[0].x, py + 1.0f, cXZ[0].y);
                        glm::vec3 T1(cXZ[1].x, py + 1.0f, cXZ[1].y);
                        glm::vec3 T2(cXZ[2].x, py + 1.0f, cXZ[2].y);

                        int tileTop  = TextureAtlas::getTileForBlock(cell.type, 0); // Cactus Top
                        int tileBot  = TextureAtlas::getTileForBlock(cell.type, 1); // Cactus Bottom
                        int tileSide = TextureAtlas::getTileForBlock(cell.type, 2); // Cactus Side

                        glm::vec4 uvTopRaw = TextureAtlas::getTileUV(tileTop);
                        glm::vec4 uvBotRaw = TextureAtlas::getTileUV(tileBot);
                        glm::vec4 uvSideRaw = TextureAtlas::getTileUV(tileSide);

                        // Sample inner [1/16, 15/16] to strictly avoid the 1-pixel transparent border
                        float uScale = uvTopRaw.z - uvTopRaw.x;
                        float vScale = uvTopRaw.w - uvTopRaw.y;
                        float uMin = uvTopRaw.x + uScale * (1.0f / 16.0f);
                        float uMax = uvTopRaw.x + uScale * (15.0f / 16.0f);
                        float vMin = uvTopRaw.y + vScale * (1.0f / 16.0f);
                        float vMax = uvTopRaw.y + vScale * (15.0f / 16.0f);

                        // Side faces: horizontally crop [1/16, 15/16] to match 14x14 geometry and eliminate transparent edge gaps
                        float uSideSpan = uvSideRaw.z - uvSideRaw.x;
                        glm::vec4 uvSideCropped(
                            uvSideRaw.x + uSideSpan * (1.0f / 16.0f),
                            uvSideRaw.y,
                            uvSideRaw.x + uSideSpan * (15.0f / 16.0f),
                            uvSideRaw.w
                        );

                        glm::vec2 uvT0, uvT1, uvT2;
                        glm::vec2 uvB0, uvB1, uvB2;
                        if (s == 0) {
                            // Up-pointing triangle: (uMin, vMin), (uMax, vMin), (uMin, vMax)
                            uvT0 = glm::vec2(uMin, vMin);
                            uvT1 = glm::vec2(uMax, vMin);
                            uvT2 = glm::vec2(uMin, vMax);

                            uvB0 = uvT0;
                            uvB1 = uvT1;
                            uvB2 = uvT2;
                        } else {
                            // Down-pointing triangle: (uMax, vMin), (uMax, vMax), (uMin, vMax)
                            uvT0 = glm::vec2(uMax, vMin);
                            uvT1 = glm::vec2(uMax, vMax);
                            uvT2 = glm::vec2(uMin, vMax);

                            uvB0 = uvT0;
                            uvB1 = uvT1;
                            uvB2 = uvT2;
                        }

                        CellCoord nbrs[5];
                        getNeighbors(wx, y, wz, s, nbrs);

                        // Top face (only if block above is not cactus)
                        Cell cellAbove = getCellAtWorld(nbrs[0].x, nbrs[0].y, nbrs[0].z, nbrs[0].s);
                        if (!cellAbove.isCactus()) {
                            auto [sT, tT] = getVertexLight((T0 + T1 + T2) / 3.0f, nTop);
                            glm::vec3 colTop(sT, 1.0f, tT);
                            addOpaqueTri(T0, T2, T1, uvT0, uvT2, uvT1, nTop, colTop, colTop, colTop);
                        }

                        // Bottom face (only if block below is not cactus)
                        Cell cellBelow = getCellAtWorld(nbrs[1].x, nbrs[1].y, nbrs[1].z, nbrs[1].s);
                        if (!cellBelow.isCactus()) {
                            auto [sB, tB] = getVertexLight((B0 + B1 + B2) / 3.0f, nBot);
                            glm::vec3 colBot(sB, 1.0f, tB);
                            addOpaqueTri(B0, B1, B2, uvB0, uvB1, uvB2, nBot, colBot, colBot, colBot);
                        }

                        // 3 Lateral faces (Upright quad order: BL=B_i, TL=T_i, TR=T_{i+1}, BR=B_{i+1}):
                        // For s=0:
                        // Edge 0: C0->C1 (Base wall, nbrs[2])
                        // Edge 1: C1->C3 (Hypotenuse seam, nbrs[4])
                        // Edge 2: C3->C0 (Left slanted wall, nbrs[3])
                        // For s=1:
                        // Edge 0: C1->C2 (Right slanted wall, nbrs[4])
                        // Edge 1: C2->C3 (Base wall, nbrs[2])
                        // Edge 2: C3->C1 (Hypotenuse seam, nbrs[3])

                        int nbrEdge0 = (s == 0) ? 2 : 4;
                        int nbrEdge1 = (s == 0) ? 4 : 2;
                        int nbrEdge2 = 3;

                        Cell n0Cell = getCellAtWorld(nbrs[nbrEdge0].x, nbrs[nbrEdge0].y, nbrs[nbrEdge0].z, nbrs[nbrEdge0].s);
                        if (!n0Cell.isCactus()) {
                            glm::vec2 e0 = cXZ[1] - cXZ[0];
                            glm::vec3 n0 = glm::normalize(glm::vec3(e0.y, 0.0f, -e0.x));
                            auto [s0, t0] = getVertexLight((B0 + B1 + T1 + T0) * 0.25f, n0);
                            glm::vec3 col0(s0, 1.0f, t0);
                            addOpaqueQuad(B0, T0, T1, B1, uvSideCropped, n0, col0, col0, col0, col0);
                        }

                        Cell n1Cell = getCellAtWorld(nbrs[nbrEdge1].x, nbrs[nbrEdge1].y, nbrs[nbrEdge1].z, nbrs[nbrEdge1].s);
                        if (!n1Cell.isCactus()) {
                            glm::vec2 e1 = cXZ[2] - cXZ[1];
                            glm::vec3 n1 = glm::normalize(glm::vec3(e1.y, 0.0f, -e1.x));
                            auto [s1, t1] = getVertexLight((B1 + B2 + T2 + T1) * 0.25f, n1);
                            glm::vec3 col1(s1, 1.0f, t1);
                            addOpaqueQuad(B1, T1, T2, B2, uvSideCropped, n1, col1, col1, col1, col1);
                        }

                        Cell n2Cell = getCellAtWorld(nbrs[nbrEdge2].x, nbrs[nbrEdge2].y, nbrs[nbrEdge2].z, nbrs[nbrEdge2].s);
                        if (!n2Cell.isCactus()) {
                            glm::vec2 e2 = cXZ[0] - cXZ[2];
                            glm::vec3 n2 = glm::normalize(glm::vec3(e2.y, 0.0f, -e2.x));
                            auto [s2, t2] = getVertexLight((B2 + B0 + T0 + T2) * 0.25f, n2);
                            glm::vec3 col2(s2, 1.0f, t2);
                            addOpaqueQuad(B2, T2, T0, B0, uvSideCropped, n2, col2, col2, col2, col2);
                        }

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
                        // Still water sources (level == 0) never flow
                        if (curCell.level == 0) {
                            return glm::vec2(0.0f);
                        }

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
                            if (nCell.type == BlockType::Air) {
                                if (nBelow.type == BlockType::Air) {
                                    weight = 4.0f; // Waterfall cliff drop into open air
                                } else {
                                    weight = 2.0f; // Spreading onto flat ground
                                }
                            } else if (nCell.type == BlockType::Water && nCell.level > curCell.level) {
                                weight = static_cast<float>(nCell.level - curCell.level);
                            }
                            flow += wallDirs[i] * weight;
                        }

                        if (glm::length(flow) > 0.001f) {
                            return glm::normalize(flow);
                        }
                        return glm::vec2(0.0f);
                    };

                    // Minecraft-style corner height averaging for seamless flowing water slopes
                    auto getCornerWaterHeight = [&](float vx, float vz, int ySurf) -> float {
                        float sumHeight = 0.0f;
                        int waterCount = 0;
                        bool hasWaterAbove = false;

                        CellCoord seen[6];
                        int numSeen = 0;

                        for (int i = 0; i < 6; ++i) {
                            glm::vec3 probePos(vx + triSectorAngles[i][0] * 0.25f, static_cast<float>(ySurf) + 0.5f, vz + triSectorAngles[i][1] * 0.25f);
                            CellCoord sc = worldToCell(probePos);

                            bool already = false;
                            for (int k = 0; k < numSeen; ++k) {
                                if (seen[k] == sc) { already = true; break; }
                            }
                            if (already) continue;
                            seen[numSeen++] = sc;

                            // If any touching column has water in the layer above, this corner connects to that waterfall column
                            Cell cAbove = getCellAtWorld(sc.x, ySurf + 1, sc.z, sc.s);
                            if (cAbove.type == BlockType::Water) {
                                hasWaterAbove = true;
                            }

                            Cell c = getCellAtWorld(sc.x, ySurf, sc.z, sc.s);
                            if (c.type == BlockType::Water) {
                                float h = (c.level == 0) ? 0.90f : std::max(0.20f, 0.90f - static_cast<float>(c.level) * 0.10f);
                                sumHeight += h;
                                waterCount++;
                            }
                        }

                        if (hasWaterAbove) {
                            return 1.0f;
                        }

                        if (waterCount > 0) {
                            return sumHeight / static_cast<float>(waterCount);
                        }
                        return 0.90f;
                    };

                    // ---------------------------------------------------------
                    // Face 0: Top face (+Y)
                    // ---------------------------------------------------------
                    Cell n0 = getCellAtWorld(neighbors[0].x, neighbors[0].y, neighbors[0].z, neighbors[0].s);
                    if (shouldDrawFace(cell, n0)) {
                        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 0));
                        glm::vec2 uvT0, uvT1, uvT2;
                        if (s == 0) {
                            // Up-pointing triangle: (0,0), (1,0), (0,1)
                            uvT0 = glm::vec2(uv.x, uv.y);
                            uvT1 = glm::vec2(uv.z, uv.y);
                            uvT2 = glm::vec2(uv.x, uv.w);
                        } else {
                            // Down-pointing triangle: (1,0), (1,1), (0,1)
                            uvT0 = glm::vec2(uv.z, uv.y);
                            uvT1 = glm::vec2(uv.z, uv.w);
                            uvT2 = glm::vec2(uv.x, uv.w);
                        }

                        auto [sun0, t0] = getVertexLight(T0, nTop);
                        auto [sun1, t1] = getVertexLight(T1, nTop);
                        auto [sun2, t2] = getVertexLight(T2, nTop);

                        if (isWaterBlock) {
                            glm::vec3 waterCol0(1.0f, sun0, t0);
                            glm::vec3 waterCol1(1.0f, sun1, t1);
                            glm::vec3 waterCol2(1.0f, sun2, t2);

                            // Seamless corner-averaged water height (identical across shared triangle vertices)
                            float h0 = getCornerWaterHeight(vXZ[0].x, vXZ[0].y, y);
                            float h1 = getCornerWaterHeight(vXZ[1].x, vXZ[1].y, y);
                            float h2 = getCornerWaterHeight(vXZ[2].x, vXZ[2].y, y);

                            glm::vec3 W0(vXZ[0].x, py + h0, vXZ[0].y);
                            glm::vec3 W1(vXZ[1].x, py + h1, vXZ[1].y);
                            glm::vec3 W2(vXZ[2].x, py + h2, vXZ[2].y);

                            // Flow vector passed via normal.xz (zero for still water, directional for flowing)
                            glm::vec2 flowVec = getWaterFlowVector(wx, y, wz, s, cell);
                            glm::vec3 nWaterTop(flowVec.x, 1.0f, flowVec.y);

                            addWaterTri(W0, W2, W1, uvT0, uvT2, uvT1, nWaterTop, waterCol0, waterCol2, waterCol1);
                        } else {
                            bool isSubmerged = (n0.type == BlockType::Water);
                            float ao0 = getTopVertexAO(T0, y);
                            float ao1 = getTopVertexAO(T1, y);
                            float ao2 = getTopVertexAO(T2, y);

                            glm::vec3 c0(sun0, 1.0f * ao0, isSubmerged ? (t0 + 2.0f) : t0);
                            glm::vec3 c1(sun1, 1.0f * ao1, isSubmerged ? (t1 + 2.0f) : t1);
                            glm::vec3 c2(sun2, 1.0f * ao2, isSubmerged ? (t2 + 2.0f) : t2);

                            addOpaqueTri(T0, T2, T1, uvT0, uvT2, uvT1, cell.isLeaves() ? (nTop * 1.4f) : nTop, c0, c2, c1);
                        }
                    }

                    // ---------------------------------------------------------
                    // Face 1: Bottom face (-Y)
                    // ---------------------------------------------------------
                    Cell n1 = getCellAtWorld(neighbors[1].x, neighbors[1].y, neighbors[1].z, neighbors[1].s);
                    if (shouldDrawFace(cell, n1)) {
                        glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(cell.type, 1));
                        glm::vec2 uvB0, uvB1, uvB2;
                        if (s == 0) {
                            // Up-pointing triangle: (0,0), (1,0), (0,1)
                            uvB0 = glm::vec2(uv.x, uv.y);
                            uvB1 = glm::vec2(uv.z, uv.y);
                            uvB2 = glm::vec2(uv.x, uv.w);
                        } else {
                            // Down-pointing triangle: (1,0), (1,1), (0,1)
                            uvB0 = glm::vec2(uv.z, uv.y);
                            uvB1 = glm::vec2(uv.z, uv.w);
                            uvB2 = glm::vec2(uv.x, uv.w);
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

                            addOpaqueTri(B0, B1, B2, uvB0, uvB1, uvB2, cell.isLeaves() ? (nBot * 1.4f) : nBot, c0, c1, c2);
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
                            glm::vec3 wc_B0(1.0f, sun_B0, t_B0);
                            glm::vec3 wc_T0(1.0f, sun_T0, t_T0);
                            glm::vec3 wc_T1(1.0f, sun_T1, t_T1);
                            glm::vec3 wc_B1(1.0f, sun_B1, t_B1);
                            bool hasWaterAbove = (getCellAtWorld(wx, y + 1, wz, s).type == BlockType::Water);
                            float h0 = getCornerWaterHeight(vB0.x, vB0.z, y);
                            float h1 = getCornerWaterHeight(vB1.x, vB1.z, y);
                            float topY0 = hasWaterAbove ? (py + 1.0f) : (py + h0);
                            float topY1 = hasWaterAbove ? (py + 1.0f) : (py + h1);
                            glm::vec3 wT0 = vT0; wT0.y = topY0;
                            glm::vec3 wT1 = vT1; wT1.y = topY1;
                            glm::vec3 normTop = hasWaterAbove ? wallNorm : glm::vec3(wallNorm.x, 0.5f, wallNorm.z);
                            addWaterWallQuad(vB0, wT0, wT1, vB1, uv, wallNorm, normTop, wc_B0, wc_T0, wc_T1, wc_B1);
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

                            addOpaqueQuad(vB0, vT0, vT1, vB1, uv, cell.isLeaves() ? (wallNorm * 1.4f) : wallNorm, c_B0, c_T0, c_T1, c_B1);
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
                glm::vec2 uvT0, uvT1, uvT2;
                if (s == 0) {
                    // Up-pointing triangle: (0,0), (1,0), (0,1)
                    uvT0 = glm::vec2(uv.x, uv.y);
                    uvT1 = glm::vec2(uv.z, uv.y);
                    uvT2 = glm::vec2(uv.x, uv.w);
                } else {
                    // Down-pointing triangle: (1,0), (1,1), (0,1)
                    uvT0 = glm::vec2(uv.z, uv.y);
                    uvT1 = glm::vec2(uv.z, uv.w);
                    uvT2 = glm::vec2(uv.x, uv.w);
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

ChunkMesh ChunkMesher::generateImposterMesh(const ChunkCoord& coord, const TerrainGen& terrainGen, int step) {
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
    const float fStep = static_cast<float>(step);

    int solidHeightGrid[16][16];
    BlockType solidTypeGrid[16][16];
    bool hasWaterGrid[16][16];

    for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
        for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
            int topY = -1;
            BlockType topType = BlockType::Air;
            bool hasWater = false;

            int sampleStep = std::max(1, step / 2);
            for (int dx = 0; dx < step; dx += sampleStep) {
                for (int dz = 0; dz < step; dz += sampleStep) {
                    int wx = worldX + lx + dx;
                    int wz = worldZ + lz + dz;
                    int h = terrainGen.getHeight(wx, wz);
                    if (h <= 44) {
                        hasWater = true;
                    }
                    if (h > topY) {
                        topY = h;
                        BiomeType b = terrainGen.getBiome(wx, wz);
                        topType = getBiomeSurfaceBlock(b, h);
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

    // B. Water Surface: Macro water quad matching ocean surface at Y = 44.90f
    for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
        for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
            if (hasWaterGrid[lx][lz]) {
                int wx = worldX + lx;
                int wz = worldZ + lz;
                float x0 = static_cast<float>(wx) + getRowXOffset(wz);
                float x1 = x0 + fStep;
                float z0 = static_cast<float>(wz) * TRI_HEIGHT;
                float z1 = static_cast<float>(wz + step) * TRI_HEIGHT;

                glm::vec4 uv = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(BlockType::Water, 0));
                float waterY = 44.90f;
                glm::vec3 v0(x0, waterY, z0);
                glm::vec3 v1(x0, waterY, z1);
                glm::vec3 v2(x1, waterY, z1);
                glm::vec3 v3(x1, waterY, z0);
                addWaterQuad(v0, v1, v2, v3, uv, nTop, glm::vec3(1.0f, 1.0f, 0.0f));
            }
        }
    }

    // 2. Internal Height Skirts between coarse cells for Solid Terrain (only when step < 16)
    if (step < 16) {
        // X boundary between (0, lz) and (step, lz)
        for (int lz = 0; lz < CHUNK_SIZE_Z; lz += step) {
            int yA = solidHeightGrid[0][lz];
            int yB = solidHeightGrid[step][lz];
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
                    BlockType bType = solidTypeGrid[step][lz];
                    glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                    glm::vec3 v0(x1, static_cast<float>(yA) + 1.0f, z1);
                    glm::vec3 v1(x1, static_cast<float>(yB) + 1.0f, z1);
                    glm::vec3 v2(x1, static_cast<float>(yB) + 1.0f, z0);
                    glm::vec3 v3(x1, static_cast<float>(yA) + 1.0f, z0);
                    addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.65f, 0.0f));
                }
            }
        }

        // Z boundary between (lx, 0) and (lx, step)
        for (int lx = 0; lx < CHUNK_SIZE_X; lx += step) {
            int yA = solidHeightGrid[lx][0];
            int yC = solidHeightGrid[lx][step];
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
                    BlockType bType = solidTypeGrid[lx][step];
                    glm::vec4 sideUV = TextureAtlas::getTileUV(TextureAtlas::getTileForBlock(bType, 2));
                    glm::vec3 v0(x0, static_cast<float>(yA) + 1.0f, z1);
                    glm::vec3 v1(x0, static_cast<float>(yC) + 1.0f, z1);
                    glm::vec3 v2(x1, static_cast<float>(yC) + 1.0f, z1);
                    glm::vec3 v3(x1, static_cast<float>(yA) + 1.0f, z1);
                    addOpaqueQuad(v0, v1, v2, v3, sideUV, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.80f, 0.0f));
                }
            }
        }
    }

    return mesh;
}

} // namespace prismcraft
