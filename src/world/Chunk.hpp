#pragma once
#include "Cell.hpp"
#include "Coordinates.hpp"
#include <array>
#include <vector>
#include <mutex>
#include <atomic>

namespace prismcraft {

class Chunk {
public:
    Chunk(ChunkCoord coord);
    
    [[nodiscard]] Cell getCell(int localX, int y, int localZ, int s) const;
    void setCell(int localX, int y, int localZ, int s, Cell cell);
    
    [[nodiscard]] ChunkCoord getCoord() const { return m_coord; }
    [[nodiscard]] int getWorldX() const { return m_coord.cx * CHUNK_SIZE_X; }
    [[nodiscard]] int getWorldZ() const { return m_coord.cz * CHUNK_SIZE_Z; }
    
    [[nodiscard]] bool isDirty() const { return m_dirty; }
    void markDirty() { m_dirty = true; }
    void clearDirty() { m_dirty = false; }
    
    [[nodiscard]] bool isGenerated() const { return m_generated; }
    void setGenerated() { m_generated = true; }

    [[nodiscard]] const Cell* getRawCells() const { return reinterpret_cast<const Cell*>(m_cells); }
    Cell* getRawCells() { return reinterpret_cast<Cell*>(m_cells); }
    void loadRawCells(const Cell* src) {
        std::memcpy(m_cells, src, sizeof(m_cells));
        m_dirty = false;
        m_generated = true;
    }
    
private:
    ChunkCoord m_coord;
    // cells[x][y][z][s] - x,z are 0..15, y is 0..127, s is 0..1
    Cell m_cells[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z][PRISMS_PER_COLUMN];
    bool m_dirty = true;
    bool m_generated = false;
};

} // namespace prismcraft
