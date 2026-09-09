#pragma once
#include <cstdint>
#include <cmath>
#include <functional>
#include <algorithm>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace prismcraft {

// Chunk dimensions
static constexpr int CHUNK_SIZE_X = 16;
static constexpr int CHUNK_SIZE_Y = 128;
static constexpr int CHUNK_SIZE_Z = 16;
static constexpr int PRISMS_PER_COLUMN = 2; // Each (x,z) column has 2 equilateral prisms: s=0 (up/north), s=1 (down/south)

// Equilateral triangle geometric constants
// Side length = 1.0, Height = sqrt(3)/2 ≈ 0.8660254
static constexpr float TRI_SIDE = 1.0f;
static constexpr float TRI_HEIGHT = 0.8660254037844386f; // sqrt(3) / 2
static constexpr float SQRT_3_OVER_2 = 0.8660254037844386f;

// Floor modulo helper for negative coordinate wrapping
inline int floorMod(int a, int b) {
    return ((a % b) + b) % b;
}

// A cell coordinate in the world: (x, y, z, s) where s is 0 (up) or 1 (down)
struct CellCoord {
    int x, y, z;
    int s; // 0 = UP (points +Z), 1 = DOWN (points -Z)

    bool operator==(const CellCoord& o) const {
        return x == o.x && y == o.y && z == o.z && s == o.s;
    }
    bool operator!=(const CellCoord& o) const {
        return !(*this == o);
    }
};

// Chunk coordinate (horizontal only, chunks are full-height columns)
struct ChunkCoord {
    int cx, cz;
    
    bool operator==(const ChunkCoord& other) const {
        return cx == other.cx && cz == other.cz;
    }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        size_t h1 = std::hash<int>{}(c.cx);
        size_t h2 = std::hash<int>{}(c.cz);
        return h1 ^ (h2 << 16) ^ (h2 >> 16);
    }
};

// Convert world cell position to chunk coordinate
inline ChunkCoord worldToChunk(int worldX, int worldZ) {
    int cx = worldX >= 0 ? worldX / CHUNK_SIZE_X : (worldX - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X;
    int cz = worldZ >= 0 ? worldZ / CHUNK_SIZE_Z : (worldZ - CHUNK_SIZE_Z + 1) / CHUNK_SIZE_Z;
    return {cx, cz};
}

// Convert world cell position to local position within chunk
inline void worldToLocal(int worldX, int worldZ, int& localX, int& localZ) {
    localX = floorMod(worldX, CHUNK_SIZE_X);
    localZ = floorMod(worldZ, CHUNK_SIZE_Z);
}

// Get the horizontal offset of row z
inline float getRowXOffset(int z) {
    return (floorMod(z, 2) == 1) ? 0.5f : 0.0f;
}

// Get the 3 horizontal vertices (in XZ) of an equilateral prism at (x, z, s)
// For s=0 (UP): v0=(x0, z0), v1=(x0+1, z0), v2=(x0+0.5, z1)
// For s=1 (DOWN): v0=(x0+1, z0), v1=(x0+1.5, z1), v2=(x0+0.5, z1)
inline void getPrismVerticesXZ(int x, int z, int s, glm::vec2 v[3]) {
    float x0 = static_cast<float>(x) + getRowXOffset(z);
    float z0 = static_cast<float>(z) * TRI_HEIGHT;
    float z1 = static_cast<float>(z + 1) * TRI_HEIGHT;

    if (s == 0) {
        v[0] = {x0, z0};
        v[1] = {x0 + 1.0f, z0};
        v[2] = {x0 + 0.5f, z1};
    } else {
        v[0] = {x0 + 1.0f, z0};
        v[1] = {x0 + 1.5f, z1};
        v[2] = {x0 + 0.5f, z1};
    }
}

// Get the 5 neighbors of an equilateral cell at (x, y, z, s)
// Order: [0: Top (y+1), 1: Bottom (y-1), 2: Base wall, 3: Left slanted wall, 4: Right slanted wall]
inline void getNeighbors(int x, int y, int z, int s, CellCoord neighbors[5]) {
    neighbors[0] = {x, y + 1, z, s}; // Top
    neighbors[1] = {x, y - 1, z, s}; // Bottom

    int rowParity = floorMod(z, 2);

    if (s == 0) {
        // UP triangle (points +Z)
        // Base wall (at Z = z0)
        neighbors[2] = (rowParity == 0) ? CellCoord{x - 1, y, z - 1, 1} : CellCoord{x, y, z - 1, 1};
        // Left slanted wall (v0 -> v2)
        neighbors[3] = {x - 1, y, z, 1};
        // Right slanted wall (v1 -> v2)
        neighbors[4] = {x, y, z, 1};
    } else {
        // DOWN triangle (points -Z)
        // Base wall (at Z = z1)
        neighbors[2] = (rowParity == 0) ? CellCoord{x, y, z + 1, 0} : CellCoord{x + 1, y, z + 1, 0};
        // Left slanted wall (v0 -> v2)
        neighbors[3] = {x, y, z, 0};
        // Right slanted wall (v0 -> v1)
        neighbors[4] = {x + 1, y, z, 0};
    }
}

// Convert continuous world position (P.x, P.y, P.z) to discrete equilateral CellCoord (x, y, z, s)
inline CellCoord worldToCell(const glm::vec3& pos) {
    int y = static_cast<int>(std::floor(pos.y));
    int z = static_cast<int>(std::floor(pos.z / TRI_HEIGHT));
    float zRel = pos.z - static_cast<float>(z) * TRI_HEIGHT;
    float v = std::clamp(zRel / TRI_HEIGHT, 0.0f, 1.0f);

    float xOff = getRowXOffset(z);
    float x0 = pos.x - xOff;
    int x = static_cast<int>(std::floor(x0));
    float u = x0 - static_cast<float>(x);

    // Test position against the two diagonal bounds of the UP triangle (s=0)
    // Left diagonal line: 2u - v = 0
    // Right diagonal line: 2u + v = 2
    if ((2.0f * u - v) < 0.0f) {
        return {x - 1, y, z, 1};
    } else if ((2.0f * u + v) > 2.0f) {
        return {x, y, z, 1};
    } else {
        return {x, y, z, 0};
    }
}

// Convert CellCoord to the 3D center point of the equilateral prism
inline glm::vec3 cellToWorldCenter(int x, int y, int z, int s) {
    glm::vec2 v[3];
    getPrismVerticesXZ(x, z, s, v);
    float cx = (v[0].x + v[1].x + v[2].x) / 3.0f;
    float cz = (v[0].y + v[1].y + v[2].y) / 3.0f;
    float cy = static_cast<float>(y) + 0.5f;
    return {cx, cy, cz};
}

} // namespace prismcraft
