#pragma once

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize.h"
#include "cube_loader.hpp"
#include "interpolator.hpp"
#include <string>
#include <vector>
#include <iostream>

struct Image {
    std::vector<unsigned char> data;
    int width, height, channels;

    bool valid() const { return !data.empty() && width > 0 && height > 0 && channels == 3; }
};

Image loadImage(const std::string& inputPath);
bool saveImage(const Image& img, const std::string& outputPath, const std::string& format);
Image processImage(const Image& input, const LUTFlat& lut, int lutSize, float blendAmount, float whiteBalance, float tint, float brightness, float contrast, float saturation);
Image processImageParallel(const Image& input, const LUTFlat& lut, int lutSize, float blendAmount, float whiteBalance, float tint, float brightness, float contrast, float saturation);
Image resizeImage(const Image& input, int newWidth, int newHeight);