#pragma once
#include "world/Coordinates.hpp"
#include <glm/glm.hpp>
#include <optional>

namespace prismcraft {

class World;

struct RaycastHit {
    CellCoord hitCell;       // The block to break
    CellCoord placeCell;     // The empty space adjacent to the hit face (to place block into)
    glm::vec3 hitPoint;      // Point of intersection in world space
    glm::vec3 hitNormal;     // Normal of the face hit
    float distance;
};

class Raycast {
public:
    static std::optional<RaycastHit> cast(
        const World& world,
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance = 6.0f);

    // Helper: convert any continuous world position (x, y, z) into the CellCoord (x, y, z, s)
    static CellCoord pointToCell(const glm::vec3& point);
};

} // namespace prismcraft
