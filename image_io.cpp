#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "image_io.hpp"
#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

Image loadImage(const std::string& inputPath) {
    Image img;
    int width, height, channels;
    unsigned char* raw_data = stbi_load(inputPath.c_str(), &width, &height, &channels, 3);
    if (!raw_data) {
        std::cerr << "Не удалось загрузить " << inputPath << "\n";
        return img;
    }
    img.width = width;
    img.height = height;
    img.channels = channels;
    img.data.assign(raw_data, raw_data + width * height * channels);
    stbi_image_free(raw_data);
    return img;
}

bool saveImage(const Image& img, const std::string& outputPath, const std::string& format) {
    if (!img.valid()) {
        std::cerr << "Некорректное изображение для сохранения " << outputPath << "\n";
        return false;
    }
    int success = 0;
    if (format == "jpg") {
        success = stbi_write_jpg(outputPath.c_str(), img.width, img.height, img.channels, img.data.data(), 95);
    }
    if (!success) {
        std::cerr << "Ошибка при сохранении " << outputPath << "\n";
    }
    return success != 0;
}

Image processImage(const Image& input, const LUTFlat& lut, int lutSize, float blendAmount, float whiteBalance, float tint, float brightness, float contrast, float saturation) {
    Image output;
    if (!input.valid()) {
        return output;
    }
    output.width = input.width;
    output.height = input.height;
    output.channels = input.channels;
    output.data.resize(input.width * input.height * input.channels);

    for (int i = 0; i < input.width * input.height; ++i) {
        Color px;
        px.r = input.data[i * 3 + 0] / 255.0f;
        px.g = input.data[i * 3 + 1] / 255.0f;
        px.b = input.data[i * 3 + 2] / 255.0f;

        // Apply LUT
        Color final = px;
        if (blendAmount > 0.0f) {
            Color mapped = interpolateLUT(px, lut, lutSize);
            if (blendAmount < 1.0f) {
                final = blend(px, mapped, blendAmount);
            } else {
                final = mapped;
            }
        }

        // Apply white balance
        if (whiteBalance != 0.0f) {
            final.r = std::clamp(final.r * (1.0f + whiteBalance * 0.5f), 0.0f, 1.0f);
            final.b = std::clamp(final.b * (1.0f - whiteBalance * 0.5f), 0.0f, 1.0f);
        }

        // Apply tint
        if (tint != 0.0f) {
            final.g = std::clamp(final.g * (1.0f + tint * 0.5f), 0.0f, 1.0f);
            final.r = std::clamp(final.r * (1.0f - tint * 0.3f), 0.0f, 1.0f);
            final.b = std::clamp(final.b * (1.0f - tint * 0.3f), 0.0f, 1.0f);
        }

        // Apply brightness
        if (brightness != 0.0f) {
            final.r = std::clamp(final.r * (1.0f + brightness * 0.5f), 0.0f, 1.0f);
            final.g = std::clamp(final.g * (1.0f + brightness * 0.5f), 0.0f, 1.0f);
            final.b = std::clamp(final.b * (1.0f + brightness * 0.5f), 0.0f, 1.0f);
        }

        // Apply contrast
        if (contrast != 0.0f) {
            float factor = (1.0f + contrast) / (1.0f - contrast + 0.0001f);
            final.r = std::clamp((final.r - 0.5f) * factor + 0.5f, 0.0f, 1.0f);
            final.g = std::clamp((final.g - 0.5f) * factor + 0.5f, 0.0f, 1.0f);
            final.b = std::clamp((final.b - 0.5f) * factor + 0.5f, 0.0f, 1.0f);
        }

        // Apply saturation
        if (saturation != 0.0f) {
            float r = final.r, g = final.g, b = final.b;
            float max = std::max({r, g, b}), min = std::min({r, g, b});
            float delta = max - min;
            float h, s, v = max;
            if (delta != 0.0f) {
                s = delta / max;
                if (max == r) h = (g - b) / delta;
                else if (max == g) h = 2.0f + (b - r) / delta;
                else h = 4.0f + (r - g) / delta;
                h *= 60.0f;
                if (h < 0.0f) h += 360.0f;
            } else {
                s = 0.0f; h = 0.0f;
            }
            s = std::clamp(s * (1.0f + saturation), 0.0f, 1.0f);
            if (s == 0.0f) {
                final.r = final.g = final.b = v;
            } else {
                float c = v * s;
                float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
                float m = v - c;
                float r1, g1, b1;
                if (h < 60.0f) { r1 = c; g1 = x; b1 = 0.0f; }
                else if (h < 120.0f) { r1 = x; g1 = c; b1 = 0.0f; }
                else if (h < 180.0f) { r1 = 0.0f; g1 = c; b1 = x; }
                else if (h < 240.0f) { r1 = 0.0f; g1 = x; b1 = c; }
                else if (h < 300.0f) { r1 = x; g1 = 0.0f; b1 = c; }
                else { r1 = c; g1 = 0.0f; b1 = x; }
                final.r = std::clamp(r1 + m, 0.0f, 1.0f);
                final.g = std::clamp(g1 + m, 0.0f, 1.0f);
                final.b = std::clamp(b1 + m, 0.0f, 1.0f);
            }
        }

        output.data[i * 3 + 0] = static_cast<unsigned char>(std::clamp(final.r * 255.0f, 0.0f, 255.0f));
        output.data[i * 3 + 1] = static_cast<unsigned char>(std::clamp(final.g * 255.0f, 0.0f, 255.0f));
        output.data[i * 3 + 2] = static_cast<unsigned char>(std::clamp(final.b * 255.0f, 0.0f, 255.0f));
    }
    return output;
}

// Вспомогательная функция для обработки диапазона строк
void processPixelRange(const Image& input, Image& output, const LUTFlat& lut, int lutSize, float blendAmount,
                      float whiteBalance, float tint, float brightness, float contrast, float saturation,
                      int startRow, int endRow) {
    for (int y = startRow; y < endRow; ++y) {
        for (int x = 0; x < input.width; ++x) {
            int i = y * input.width + x;
            Color px;
            px.r = input.data[i * 3 + 0] / 255.0f;
            px.g = input.data[i * 3 + 1] / 255.0f;
            px.b = input.data[i * 3 + 2] / 255.0f;

            Color final = px;
            if (blendAmount > 0.0f) {
                Color mapped = interpolateLUT(px, lut, lutSize);
                if (blendAmount < 1.0f) {
                    final = blend(px, mapped, blendAmount);
                } else {
                    final = mapped;
                }
            }

            // Apply white balance
            if (whiteBalance != 0.0f) {
                final.r = std::clamp(final.r * (1.0f + whiteBalance * 0.5f), 0.0f, 1.0f);
                final.b = std::clamp(final.b * (1.0f - whiteBalance * 0.5f), 0.0f, 1.0f);
            }

            // Apply tint
            if (tint != 0.0f) {
                final.g = std::clamp(final.g * (1.0f + tint * 0.5f), 0.0f, 1.0f);
                final.r = std::clamp(final.r * (1.0f - tint * 0.3f), 0.0f, 1.0f);
                final.b = std::clamp(final.b * (1.0f - tint * 0.3f), 0.0f, 1.0f);
            }

            // Apply brightness
            if (brightness != 0.0f) {
                final.r = std::clamp(final.r * (1.0f + brightness * 0.5f), 0.0f, 1.0f);
                final.g = std::clamp(final.g * (1.0f + brightness * 0.5f), 0.0f, 1.0f);
                final.b = std::clamp(final.b * (1.0f + brightness * 0.5f), 0.0f, 1.0f);
            }

            // Apply contrast
            if (contrast != 0.0f) {
                float factor = (1.0f + contrast) / (1.0f - contrast + 0.0001f);
                final.r = std::clamp((final.r - 0.5f) * factor + 0.5f, 0.0f, 1.0f);
                final.g = std::clamp((final.g - 0.5f) * factor + 0.5f, 0.0f, 1.0f);
                final.b = std::clamp((final.b - 0.5f) * factor + 0.5f, 0.0f, 1.0f);
            }

            // Apply saturation
            if (saturation != 0.0f) {
                float r = final.r, g = final.g, b = final.b;
                float max = std::max({r, g, b}), min = std::min({r, g, b});
                float delta = max - min;
                float h, s, v = max;
                if (delta != 0.0f) {
                    s = delta / max;
                    if (max == r) h = (g - b) / delta;
                    else if (max == g) h = 2.0f + (b - r) / delta;
                    else h = 4.0f + (r - g) / delta;
                    h *= 60.0f;
                    if (h < 0.0f) h += 360.0f;
                } else {
                    s = 0.0f; h = 0.0f;
                }
                s = std::clamp(s * (1.0f + saturation), 0.0f, 1.0f);
                if (s == 0.0f) {
                    final.r = final.g = final.b = v;
                } else {
                    float c = v * s;
                    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
                    float m = v - c;
                    float r1, g1, b1;
                    if (h < 60.0f) { r1 = c; g1 = x; b1 = 0.0f; }
                    else if (h < 120.0f) { r1 = x; g1 = c; b1 = 0.0f; }
                    else if (h < 180.0f) { r1 = 0.0f; g1 = c; b1 = x; }
                    else if (h < 240.0f) { r1 = 0.0f; g1 = x; b1 = c; }
                    else if (h < 300.0f) { r1 = x; g1 = 0.0f; b1 = c; }
                    else { r1 = c; g1 = 0.0f; b1 = x; }
                    final.r = std::clamp(r1 + m, 0.0f, 1.0f);
                    final.g = std::clamp(g1 + m, 0.0f, 1.0f);
                    final.b = std::clamp(b1 + m, 0.0f, 1.0f);
                }
            }

            output.data[i * 3 + 0] = static_cast<unsigned char>(std::clamp(final.r * 255.0f, 0.0f, 255.0f));
            output.data[i * 3 + 1] = static_cast<unsigned char>(std::clamp(final.g * 255.0f, 0.0f, 255.0f));
            output.data[i * 3 + 2] = static_cast<unsigned char>(std::clamp(final.b * 255.0f, 0.0f, 255.0f));
        }
    }
}

Image processImageParallel(const Image& input, const LUTFlat& lut, int lutSize, float blendAmount,
                           float whiteBalance, float tint, float brightness, float contrast, float saturation) {
    Image output;
    if (!input.valid()) {
        return output;
    }
    output.width = input.width;
    output.height = input.height;
    output.channels = input.channels;
    output.data.resize(input.width * input.height * input.channels);

    // Используем многопоточность только для больших изображений (> 1 МП)
    int numThreads = (input.width * input.height > 1000000) ? std::thread::hardware_concurrency() : 1;
    int rowsPerThread = input.height / numThreads;
    if (rowsPerThread == 0) rowsPerThread = 1;

    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        int startRow = t * rowsPerThread;
        int endRow = (t == numThreads - 1) ? input.height : startRow + rowsPerThread;
        if (startRow >= input.height) break;
        threads.emplace_back(processPixelRange, std::ref(input), std::ref(output), std::ref(lut), lutSize,
                             blendAmount, whiteBalance, tint, brightness, contrast, saturation, startRow, endRow);
    }
    for (auto& th : threads) {
        th.join();
    }
    return output;
}

Image resizeImage(const Image& input, int newWidth, int newHeight) {

    Image output;
    if (!input.valid()) {
        return output;
    }
    output.width = newWidth;
    output.height = newHeight;
    output.channels = input.channels;
    output.data.resize(newWidth * newHeight * input.channels);
    int success = stbir_resize_uint8(input.data.data(), input.width, input.height, 0,
                                    output.data.data(), newWidth, newHeight, 0, input.channels);
    if (!success) {
        output.data.clear();
    }
    return output;
}