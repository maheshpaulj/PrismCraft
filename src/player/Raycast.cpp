#include "Raycast.hpp"
#include "world/World.hpp"
#include <cmath>

namespace prismcraft {

CellCoord Raycast::pointToCell(const glm::vec3& point) {
    return worldToCell(point);
}

std::optional<RaycastHit> Raycast::cast(
    const World& world,
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance) {
    
    glm::vec3 normDir = glm::normalize(direction);
    float stepSize = 0.04f; // Fine step for accurate face detection
    
    CellCoord lastEmptyCoord = pointToCell(origin);
    glm::vec3 lastEmptyPos = origin;

    for (float t = 0.05f; t <= maxDistance; t += stepSize) {
        glm::vec3 currentPoint = origin + normDir * t;
        CellCoord currentCoord = pointToCell(currentPoint);

        Cell cell = world.getCell(currentCoord.x, currentCoord.y, currentCoord.z, currentCoord.s);
        if (cell.isTargetable()) {
            RaycastHit hit;
            hit.hitCell = currentCoord;
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
