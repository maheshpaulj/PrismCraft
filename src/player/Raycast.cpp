#include "Raycast.hpp"
#include "world/World.hpp"
#include <cmath>

namespace prismcraft {

CellCoord Raycast::pointToCell(const glm::vec3& point) {
    return worldToCell(point);
}

static bool hitTestBlock(const World& world, const CellCoord& coord, const Cell& cell, const glm::vec3& point) {
    (void)world;
    if (cell.type == BlockType::Air || cell.type == BlockType::Water || cell.isItem()) {
        return false;
    }

    int wx = coord.x;
    int wz = coord.z;
    int s = coord.s;
    int y = coord.y;
    float py = static_cast<float>(y);

    if (cell.isDoor()) {
        bool isOpen = cell.isDoorOpen();
        uint8_t facing = cell.getDoorFacing();
        glm::vec2 vXZ[3];
        getPrismVerticesXZ(wx, wz, s, vXZ);

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

        glm::vec3 d = point - pBase;
        if (d.y < -0.05f || d.y > doorHeight + 0.05f) return false;

        float lenSpan = glm::length(vecSpan);
        if (lenSpan < 0.001f) return false;
        float dSpan = glm::dot(d, vecSpan / lenSpan);
        if (dSpan < -0.05f || dSpan > lenSpan + 0.05f) return false;

        float lenThick = glm::length(vecThick);
        if (lenThick < 0.001f) return false;
        float dThick = glm::dot(d, vecThick / lenThick);
        if (dThick < -0.05f || dThick > lenThick + 0.05f) return false;

        return true;
    }

    if (cell.isTrapdoor()) {
        bool isOpen = cell.isTrapdoorOpen();
        uint8_t facing = cell.getTrapdoorFacing();
        float thick = 0.1875f;

        if (!isOpen) {
            return (point.y >= py - 0.02f && point.y <= py + thick + 0.02f);
        } else {
            glm::vec2 vXZ[3];
            getPrismVerticesXZ(wx, wz, s, vXZ);
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
            glm::vec3 pBase = glm::vec3(w0.x, py, w0.y) + glm::vec3(wallIn.x, 0.0f, wallIn.y) * 0.015f;

            glm::vec3 d = point - pBase;
            if (d.y < -0.05f || d.y > 1.05f) return false;

            float lenSpan = glm::length(vecSpan);
            if (lenSpan < 0.001f) return false;
            float dSpan = glm::dot(d, vecSpan / lenSpan);
            if (dSpan < -0.05f || dSpan > lenSpan + 0.05f) return false;

            float lenThick = glm::length(vecThick);
            if (lenThick < 0.001f) return false;
            float dThick = glm::dot(d, vecThick / lenThick);
            if (dThick < -0.05f || dThick > lenThick + 0.05f) return false;

            return true;
        }
    }

    if (cell.isBed()) {
        return (point.y >= py - 0.02f && point.y <= py + 0.55f);
    }

    if (cell.isTorch()) {
        glm::vec3 center = cellToWorldCenter(wx, y, wz, s);
        uint8_t attachment = cell.getTorchAttachment();
        glm::vec3 pBase = glm::vec3(center.x, py, center.z);
        glm::vec3 torchAxis(0.0f, 1.0f, 0.0f);
        if (attachment != 0) {
            glm::vec2 vXZ[3];
            getPrismVerticesXZ(wx, wz, s, vXZ);
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
        float proj = glm::dot(point - pBase, torchAxis);
        if (proj < -0.08f || proj > 0.72f) return false;
        glm::vec3 closest = pBase + torchAxis * std::clamp(proj, 0.0f, 0.65f);
        return glm::distance(point, closest) <= 0.22f;
    }

    if (cell.isFoliage()) {
        glm::vec3 cent = cellToWorldCenter(wx, y, wz, s);
        return (point.y >= py - 0.02f && point.y <= py + 0.85f &&
                std::abs(point.x - cent.x) <= 0.38f &&
                std::abs(point.z - cent.z) <= 0.38f);
    }

    if (cell.type == BlockType::Cake) {
        return (point.y >= py - 0.02f && point.y <= py + 0.46f);
    }

    if (cell.isCactus()) {
        if (point.y < py - 0.02f || point.y > py + 1.02f) return false;
        float x0 = static_cast<float>(wx) + getRowXOffset(wz);
        float z0 = static_cast<float>(wz) * TRI_HEIGHT;
        float z1 = static_cast<float>(wz + 1) * TRI_HEIGHT;
        glm::vec2 V0(x0, z0), V2(x0 + 1.5f, z1);
        glm::vec2 P_cent = (V0 + V2) * 0.5f;
        float d = 0.0625f;
        glm::vec2 C0 = V0 + glm::normalize(P_cent - V0) * (2.0f * d);
        glm::vec2 C2 = V2 + glm::normalize(P_cent - V2) * (2.0f * d);
        glm::vec2 C1 = glm::vec2(x0 + 1.0f, z0) + glm::normalize(P_cent - glm::vec2(x0 + 1.0f, z0)) * (d / SQRT_3_OVER_2);
        glm::vec2 C3 = glm::vec2(x0 + 0.5f, z1) + glm::normalize(P_cent - glm::vec2(x0 + 0.5f, z1)) * (d / SQRT_3_OVER_2);

        glm::vec2 p2(point.x, point.z);
        glm::vec2 a = (s == 0) ? C0 : C1;
        glm::vec2 b = (s == 0) ? C1 : C2;
        glm::vec2 c = (s == 0) ? C3 : C3;
        auto sign = [](const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3) {
            return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
        };
        float d1 = sign(p2, a, b);
        float d2 = sign(p2, b, c);
        float d3 = sign(p2, c, a);
        bool has_neg = (d1 < -0.02f) || (d2 < -0.02f) || (d3 < -0.02f);
        bool has_pos = (d1 > 0.02f) || (d2 > 0.02f) || (d3 > 0.02f);
        return !(has_neg && has_pos);
    }

    return cell.isTargetable();
}

std::optional<RaycastHit> Raycast::cast(
    const World& world,
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance) {
    
    glm::vec3 normDir = glm::normalize(direction);
    float stepSize = 0.035f; // High precision raymarching step
    
    CellCoord lastEmptyCoord = pointToCell(origin);
    glm::vec3 lastEmptyPos = origin;

    for (float t = 0.05f; t <= maxDistance; t += stepSize) {
        glm::vec3 currentPoint = origin + normDir * t;
        CellCoord currentCoord = pointToCell(currentPoint);

        Cell cell = world.getCell(currentCoord.x, currentCoord.y, currentCoord.z, currentCoord.s);
        CellCoord hitCoord = currentCoord;
        bool foundHit = false;

        // 1. Direct hit test on cell at currentCoord
        if (hitTestBlock(world, currentCoord, cell, currentPoint)) {
            foundHit = true;
            hitCoord = currentCoord;
        }

        // 2. If no hit on currentCoord, check if an open door or open trapdoor in a neighbor cell overlaps currentPoint
        if (!foundHit) {
            CellCoord neighbors[5];
            getNeighbors(currentCoord.x, currentCoord.y, currentCoord.z, currentCoord.s, neighbors);

            CellCoord checkList[7] = {
                neighbors[2], neighbors[3], neighbors[4],
                CellCoord{currentCoord.x, currentCoord.y + 1, currentCoord.z, currentCoord.s},
                CellCoord{currentCoord.x, currentCoord.y - 1, currentCoord.z, currentCoord.s},
                neighbors[0], neighbors[1]
            };

            for (const auto& nc : checkList) {
                Cell nCell = world.getCell(nc.x, nc.y, nc.z, nc.s);
                if ((nCell.isDoor() && nCell.isDoorOpen()) || (nCell.isTrapdoor() && nCell.isTrapdoorOpen())) {
                    if (hitTestBlock(world, nc, nCell, currentPoint)) {
                        foundHit = true;
                        hitCoord = nc;
                        break;
                    }
                }
            }
        }

        if (foundHit) {
            RaycastHit hit;
            hit.hitCell = hitCoord;
            hit.placeCell = lastEmptyCoord;
            hit.hitPoint = currentPoint;
            hit.distance = t;

            // Approximate normal by vector from hit point back to last empty step
            glm::vec3 diff = lastEmptyPos - currentPoint;
            if (std::abs(diff.y) > std::abs(diff.x) && std::abs(diff.y) > std::abs(diff.z)) {
                hit.hitNormal = glm::vec3(0.0f, (diff.y > 0.0f ? 1.0f : -1.0f), 0.0f);
            } else if (std::abs(diff.x) > std::abs(diff.z)) {
                hit.hitNormal = glm::vec3((diff.x > 0.0f ? 1.0f : -1.0f), 0.0f, 0.0f);
            } else {
                hit.hitNormal = glm::vec3(0.0f, 0.0f, (diff.z > 0.0f ? 1.0f : -1.0f));
            }

            return hit;
        }

        lastEmptyCoord = currentCoord;
        lastEmptyPos = currentPoint;
    }

    return std::nullopt;
}

} // namespace prismcraft
