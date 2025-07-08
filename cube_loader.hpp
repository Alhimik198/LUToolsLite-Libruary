#pragma once

#include <vector>
#include <string>

struct Color {
    float r, g, b;

    float& operator[](int i) {
        if (i == 0) return r;
        if (i == 1) return g;
        return b;
    }

    float operator[](int i) const {
        if (i == 0) return r;
        if (i == 1) return g;
        return b;
    }
};

using LUTFlat = std::vector<Color>;

LUTFlat loadCubeLUT(const std::string& path, int& lutSizeOut);