#include "cube_loader.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>

LUTFlat loadCubeLUT(const std::string& path, int& lutSizeOut) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Файл не найден: " + path);
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Не удалось открыть файл: " + path);
    }

    std::string line;
    std::vector<Color> rawColors;
    int size = 0;

    while (std::getline(file, line))
    {
        // 1) обрезаем пробелы в начале/конце
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#')
            continue; // комментарий / пустая строка

        std::istringstream ss(line);

        // 2) Пытаемся считать 3 числа (RGB)
        float r, g, b;
        if (ss >> r >> g >> b)
        {
            rawColors.push_back({ r, g, b });
            continue;
        }

        // 3) Не числа → пытаемся разобрать ключевое слово
        ss.clear(); ss.str(line);
        std::string kw; ss >> kw;

        if (kw == "LUT_3D_SIZE")
        {
            if (!(ss >> size) || size <= 0)
                throw std::runtime_error("Некорректный размер LUT: " + line);
            continue;
        }

        if (kw == "DOMAIN_MIN" || kw == "DOMAIN_MAX")
            continue;

        // 4) Прочий текст (например TITLE) игнорируем
    }

    if (file.fail() && !file.eof()) {
        throw std::runtime_error("Ошибка чтения файла: " + path);
    }

    if (size == 0) {
        throw std::runtime_error("Размер LUT не указан в файле: " + path);
    }

    if (rawColors.size() != static_cast<size_t>(size * size * size)) {
        throw std::runtime_error("Некорректное количество RGB значений: ожидалось " +
            std::to_string(size * size * size) + ", найдено " +
            std::to_string(rawColors.size()));
    }

    lutSizeOut = size;
    return rawColors;
}
