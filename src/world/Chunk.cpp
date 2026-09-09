#include "Chunk.hpp"
#include <cstring>

namespace prismcraft {

Chunk::Chunk(ChunkCoord coord) : m_coord(coord) {
    std::memset(m_cells, 0, sizeof(m_cells)); // All Air
}

Cell Chunk::getCell(int localX, int y, int localZ, int s) const {
    if (localX < 0 || localX >= CHUNK_SIZE_X ||
        y < 0 || y >= CHUNK_SIZE_Y ||
        localZ < 0 || localZ >= CHUNK_SIZE_Z ||
        s < 0 || s >= PRISMS_PER_COLUMN) {
        Cell air;
        air.type = BlockType::Air;
        return air;
    }
    return m_cells[localX][y][localZ][s];
}

void Chunk::setCell(int localX, int y, int localZ, int s, Cell cell) {
    if (localX < 0 || localX >= CHUNK_SIZE_X ||
        y < 0 || y >= CHUNK_SIZE_Y ||
        localZ < 0 || localZ >= CHUNK_SIZE_Z ||
        s < 0 || s >= PRISMS_PER_COLUMN) {
        return;
    }
    m_cells[localX][y][localZ][s] = cell;
    m_dirty = true;
}

} // namespace prismcraft
