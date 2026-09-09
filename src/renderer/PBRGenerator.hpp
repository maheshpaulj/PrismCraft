#pragma once
#include <cstdint>
#include <vector>

namespace prismcraft {

class PBRGenerator {
public:
    static std::vector<uint8_t> generateNormalMap(const std::vector<uint8_t>& albedo,
                                                  uint32_t width, uint32_t height);

    static std::vector<uint8_t> generateORMMap(const std::vector<uint8_t>& albedo,
                                               uint32_t width, uint32_t height);
};

} // namespace prismcraft
