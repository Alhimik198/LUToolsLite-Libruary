#pragma once

#include "cube_loader.hpp"
#include <algorithm>

inline Color interpolateLUT(const Color& input, const LUTFlat& lut, int size) {
    float r = std::clamp(input.r, 0.0f, 1.0f);
    float g = std::clamp(input.g, 0.0f, 1.0f);
    float b = std::clamp(input.b, 0.0f, 1.0f);

    float x = r * (size - 1);
    float y = g * (size - 1);
    float z = b * (size - 1);

    int x0 = static_cast<int>(x), x1 = std::min(x0 + 1, size - 1);
    int y0 = static_cast<int>(y), y1 = std::min(y0 + 1, size - 1);
    int z0 = static_cast<int>(z), z1 = std::min(z0 + 1, size - 1);

    float xd = x - x0;
    float yd = y - y0;
    float zd = z - z0;

    auto get = [&](int x, int y, int z) -> const Color& {
        int idx = x + y * size + z * size * size;
        return lut[idx];
    };

    const Color& c000 = get(x0, y0, z0);
    const Color& c001 = get(x0, y0, z1);
    const Color& c010 = get(x0, y1, z0);
    const Color& c011 = get(x0, y1, z1);
    const Color& c100 = get(x1, y0, z0);
    const Color& c101 = get(x1, y0, z1);
    const Color& c110 = get(x1, y1, z0);
    const Color& c111 = get(x1, y1, z1);

    Color result;
    for (int i = 0; i < 3; ++i) {
        float c00 = c000[i] * (1 - xd) + c100[i] * xd;
        float c01 = c001[i] * (1 - xd) + c101[i] * xd;
        float c10 = c010[i] * (1 - xd) + c110[i] * xd;
        float c11 = c011[i] * (1 - xd) + c111[i] * xd;
        float c0  = c00 * (1 - yd) + c10 * yd;
        float c1  = c01 * (1 - yd) + c11 * yd;
        result[i] = c0 * (1 - zd) + c1 * zd;
    }

    return result;
}

inline Color blend(const Color& original, const Color& mapped, float alpha) {
    return {
        original.r * (1.0f - alpha) + mapped.r * alpha,
        original.g * (1.0f - alpha) + mapped.g * alpha,
        original.b * (1.0f - alpha) + mapped.b * alpha
    };
}