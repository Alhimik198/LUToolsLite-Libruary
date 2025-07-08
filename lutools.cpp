#define NOMINMAX // Отключаем макросы min/max из windows.h
#include "LUToolsLite.h"
#include "cube_loader.hpp"
#include "image_io.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <future>
#include <windows.h>
#include <memory>
#include <thread>
#include <algorithm>
#include <fstream>
#include <cmath> 

#include <chrono>
#include "interpolator.hpp"

#include <iomanip>
#include <random>
#include <numeric>

#include <string>
using namespace std::string_literals;   // ← добавить один раз в начале файла

using std::round;
using Mutex = std::recursive_mutex;
using LockG = std::lock_guard<Mutex>;

struct LUTData {
    int id;
    LUTFlat lut;
    int size;
    float blend;
};

static std::vector<LUTData> g_luts;
static Mutex g_mutex;
static LogCallback g_logCallback = nullptr;
static void* g_logUserData = nullptr;
static ProgressCallback g_progressCallback = nullptr;
static void* g_progressUserData = nullptr;
static int* g_cancelFlag = nullptr;
static std::string g_lastError;
static std::atomic<int> g_nextLutId{1};

void Log(const std::string& message, int is_error) {
    LockG lock(g_mutex);
    std::string logMessage = (is_error ? "[ОШИБКА] " : "[Лог] ") + message;
    // Временное логирование в файл для диагностики
    std::ofstream logFile("D:\\LT\\lutools_log.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << logMessage << std::endl;
        logFile.close();
    }
    if (g_logCallback) {
        try {
            g_logCallback(logMessage.c_str(), is_error, g_logUserData);
        } catch (...) {
            // Игнорируем исключения в callback
            std::ofstream logFile("D:\\LT\\lutools_log.txt", std::ios::app);
            if (logFile.is_open()) {
                logFile << "[ОШИБКА] Exception in g_logCallback" << std::endl;
                logFile.close();
            }
        }
    }
}

int LUTools_Init() {
    try {
        LockG lock(g_mutex);
        g_luts.clear();
        g_nextLutId = 1;
        g_lastError.clear();
        Log("Initialized LUToolsLite", 0);
        return SUCCESS;
    } catch (const std::exception& e) {
        g_lastError = "Exception in LUTools_Init: " + std::string(e.what());
        Log(g_lastError, 1);
        return INITIALIZATION_FAILED;
    } catch (...) {
        g_lastError = "Unknown exception in LUTools_Init";
        Log(g_lastError, 1);
        return INITIALIZATION_FAILED;
    }
}

void LUTools_Cleanup() {
    LockG lock(g_mutex);
    g_luts.clear();
    g_logCallback = nullptr;
    g_logUserData = nullptr;
    g_progressCallback = nullptr;
    g_progressUserData = nullptr;
    g_cancelFlag = nullptr;
    g_lastError.clear();
    Log("Cleaned up LUToolsLite", 0);
}

int LUTools_LoadLUT(const char* filePath, float blend, int* lutId) {
    if (!filePath || !lutId) {
        g_lastError = "Invalid filePath or lutId pointer";
        return INVALID_LUT;
    }
    int size = 0;
    try {
        LUTFlat lut = loadCubeLUT(filePath, size);
        if (lut.empty()) {
            g_lastError = "Failed to load LUT: " + std::string(filePath);
            return INVALID_LUT;
        }
        LockG lock(g_mutex);
        int id = g_nextLutId++;
        g_luts.push_back({id, std::move(lut), size, std::clamp(blend, 0.0f, 1.0f)});
        *lutId = id;
        Log("Loaded LUT: " + std::string(filePath) + " with blend: " + std::to_string(blend), 0);
        return SUCCESS;
    } catch (const std::exception& e) {
        g_lastError = "Error loading LUT: " + std::string(e.what());
        return INVALID_LUT;
    }
}

void LUTools_UnloadLUT(int lutId) {
    LockG lock(g_mutex);
    g_luts.erase(std::remove_if(g_luts.begin(), g_luts.end(),
        [lutId](const LUTData& lut) { return lut.id == lutId; }), g_luts.end());
    Log("Unloaded LUT ID: " + std::to_string(lutId), 0);
}

void LUTools_ClearLUTs() {
    LockG lock(g_mutex);
    g_luts.clear();
    Log("Cleared all LUTs", 0);
}

int LUTools_ProcessFile(const char* inputPath, const char* outputPath, const int* lutIds, int lutCount, float whiteBalance, float tint, float brightness, float contrast, float saturation, LogCallback logCallback, void* userData) {
    if (!inputPath || !outputPath || !lutIds) {
        g_lastError = "Invalid input/output paths or LUT IDs";
        return INVALID_IMAGE;
    }
    std::vector<LUTData> luts;
    {
        LockG lock(g_mutex);
        for (int i = 0; i < lutCount; ++i) {
            auto it = std::find_if(g_luts.begin(), g_luts.end(),
                [lutId = lutIds[i]](const LUTData& lut) { return lut.id == lutId; });
            if (it == g_luts.end()) {
                g_lastError = "Invalid LUT ID: " + std::to_string(lutIds[i]);
                return INVALID_LUT;
            }
            luts.push_back(*it);
        }
    }
    Image img = loadImage(inputPath);
    if (!img.valid()) {
        g_lastError = "Failed to load image: " + std::string(inputPath);
        Log(g_lastError, 1);
        return INVALID_IMAGE;
    }
    if (LUTools_IsCancelled()) {
        g_lastError = "Operation cancelled";
        Log(g_lastError, 1);
        return CANCELLED;
    }
    for (const auto& lut : luts) {
        img = processImageParallel(img, lut.lut, lut.size, lut.blend, whiteBalance, tint, brightness, contrast, saturation);
        if (!img.valid()) {
            g_lastError = "Failed to process image with LUT";
            Log(g_lastError, 1);
            return INVALID_IMAGE;
        }
    }
    bool success = saveImage(img, outputPath, "jpg");
    if (!success) {
        g_lastError = "Failed to save image: " + std::string(outputPath);
        Log(g_lastError, 1);
        return INVALID_IMAGE;
    }
    Log("Processed: " + std::string(inputPath) + " -> " + std::string(outputPath), 0);
    return SUCCESS;
}

int LUTools_ProcessImage(unsigned char* inputData, int width, int height, int channels, const int* lutIds, int lutCount, float whiteBalance, float tint, float brightness, float contrast, float saturation, unsigned char** outputData, int* outWidth, int* outHeight, int* outChannels) {
    if (!inputData || !outputData || !outWidth || !outHeight || !outChannels || channels != 3) {
        g_lastError = "Invalid input data or parameters";
        return INVALID_IMAGE;
    }
    *outputData = nullptr;
    Image img;
    img.width = width;
    img.height = height;
    img.channels = channels;
    img.data.assign(inputData, inputData + width * height * channels);
    std::vector<LUTData> luts;
    {
        LockG lock(g_mutex);
        for (int i = 0; i < lutCount; ++i) {
            auto it = std::find_if(g_luts.begin(), g_luts.end(),
                [lutId = lutIds[i]](const LUTData& lut) { return lut.id == lutId; });
            if (it == g_luts.end()) {
                g_lastError = "Invalid LUT ID: " + std::to_string(lutIds[i]);
                return INVALID_LUT;
            }
            luts.push_back(*it);
        }
    }
    if (LUTools_IsCancelled()) {
        g_lastError = "Operation cancelled";
        return CANCELLED;
    }
    for (const auto& lut : luts) {
        img = processImageParallel(img, lut.lut, lut.size, lut.blend, whiteBalance, tint, brightness, contrast, saturation);
        if (!img.valid()) {
            g_lastError = "Failed to process image with LUT";
            return INVALID_IMAGE;
        }
    }
    *outWidth = img.width;
    *outHeight = img.height;
    *outChannels = img.channels;
    *outputData = (unsigned char*)malloc(img.data.size());
    if (!*outputData) {
        g_lastError = "Memory allocation failed";
        return MEMORY_ALLOCATION_FAILED;
    }
    std::copy(img.data.begin(), img.data.end(), *outputData);
    return SUCCESS;
}

int LUTools_GeneratePreview(unsigned char* inputData, int width, int height, int channels, const int* lutIds, int lutCount, float whiteBalance, float tint, float brightness, float contrast, float saturation, int previewWidth, int previewHeight, unsigned char** outputData, int* outWidth, int* outHeight, int* outChannels) {
    if (!inputData || !outputData || !outWidth || !outHeight || !outChannels || channels != 3) {
        g_lastError = "Invalid input data or parameters";
        return INVALID_IMAGE;
    }
    *outputData = nullptr;
    Image img;
    img.width = width;
    img.height = height;
    img.channels = channels;
    img.data.assign(inputData, inputData + width * height * channels);
    Image resized = resizeImage(img, previewWidth, previewHeight);
    if (!resized.valid()) {
        g_lastError = "Failed to resize image for preview";
        return INVALID_IMAGE;
    }
    std::vector<LUTData> luts;
    {
        LockG lock(g_mutex);
        for (int i = 0; i < lutCount; ++i) {
            auto it = std::find_if(g_luts.begin(), g_luts.end(),
                [lutId = lutIds[i]](const LUTData& lut) { return lut.id == lutId; });
            if (it == g_luts.end()) {
                g_lastError = "Invalid LUT ID: " + std::to_string(lutIds[i]);
                return INVALID_LUT;
            }
            luts.push_back(*it);
        }
    }
    if (LUTools_IsCancelled()) {
        g_lastError = "Operation cancelled";
        return CANCELLED;
    }
    for (const auto& lut : luts) {
        resized = processImageParallel(resized, lut.lut, lut.size, lut.blend, whiteBalance, tint, brightness, contrast, saturation);
        if (!resized.valid()) {
            g_lastError = "Failed to process preview image";
            return INVALID_IMAGE;
        }
    }
    *outWidth = resized.width;
    *outHeight = resized.height;
    *outChannels = resized.channels;
    *outputData = (unsigned char*)malloc(resized.data.size());
    if (!*outputData) {
        g_lastError = "Memory allocation failed";
        return MEMORY_ALLOCATION_FAILED;
    }
    std::copy(resized.data.begin(), resized.data.end(), *outputData);
    return SUCCESS;
}

int LUTools_GeneratePreviewFit(
    unsigned char* inputData, int width, int height, int channels,
    const int* lutIds, int lutCount,
    float whiteBalance, float tint, float brightness, float contrast, float saturation,
    int maxWidth, int maxHeight,
    unsigned char** outputData, int* outWidth, int* outHeight, int* outChannels)
{
    if (!inputData || !outputData || !outWidth || !outHeight || !outChannels || channels != 3) {
        g_lastError = "Invalid input data or parameters";
        return INVALID_IMAGE;
    }

    // Расчёт целевого размера с сохранением пропорций
    if (maxWidth <= 0 || maxHeight <= 0) {
        g_lastError = "maxWidth / maxHeight must be > 0";
        return INVALID_IMAGE;
    }
    float scale = std::min(
        static_cast<float>(maxWidth) / static_cast<float>(width),
        static_cast<float>(maxHeight) / static_cast<float>(height));

    int targetW = std::max(1, static_cast<int>(width * scale + 0.5f));
    int targetH = std::max(1, static_cast<int>(height * scale + 0.5f));

    Image img;
    img.width = width;
    img.height = height;
    img.channels = channels;
    img.data.assign(inputData, inputData + width * height * channels);

    Image resized = resizeImage(img, targetW, targetH);
    if (!resized.valid()) {
        g_lastError = "Failed to resize image for preview";
        return INVALID_IMAGE;
    }

    // Применяем LUT-цепочку
    std::vector<LUTData> luts;
    {
        LockG lock(g_mutex);
        for (int i = 0; i < lutCount; ++i) {
            auto it = std::find_if(g_luts.begin(), g_luts.end(),
                [id = lutIds[i]](const LUTData& l) { return l.id == id; });
            if (it == g_luts.end()) {
                g_lastError = "Invalid LUT ID: " + std::to_string(lutIds[i]);
                return INVALID_LUT;
            }
            luts.push_back(*it);
        }
    }
    if (LUTools_IsCancelled()) {
        g_lastError = "Operation cancelled";
        return CANCELLED;
    }
    for (const auto& lut : luts) {
        resized = processImageParallel(resized, lut.lut, lut.size, lut.blend,
                              whiteBalance, tint, brightness, contrast, saturation);
        if (!resized.valid()) {
            g_lastError = "Failed to process preview image";
            return INVALID_IMAGE;
        }
    }

    *outWidth = resized.width;
    *outHeight = resized.height;
    *outChannels = resized.channels;
    *outputData = static_cast<unsigned char*>(malloc(resized.data.size()));
    if (!*outputData) {
        g_lastError = "Memory allocation failed";
        return MEMORY_ALLOCATION_FAILED;
    }
    std::copy(resized.data.begin(), resized.data.end(), *outputData);
    return SUCCESS;
}


int LUTools_ProcessFiles(const char** inputPaths, const char** outputPaths, int fileCount, const int* lutIds, int lutCount, float whiteBalance, float tint, float brightness, float contrast, float saturation, LogCallback logCallback, void* userData) {
    if (!inputPaths || !outputPaths || fileCount <= 0 || !lutIds) {
        g_lastError = "Invalid input/output paths or file count";
        return INVALID_IMAGE;
    }
    std::vector<LUTData> luts;
    {
        LockG lock(g_mutex);
        for (int i = 0; i < lutCount; ++i) {
            auto it = std::find_if(g_luts.begin(), g_luts.end(),
                [lutId = lutIds[i]](const LUTData& lut) { return lut.id == lutId; });
            if (it == g_luts.end()) {
                g_lastError = "Invalid LUT ID: " + std::to_string(lutIds[i]);
                return INVALID_LUT;
            }
            luts.push_back(*it);
        }
    }
    std::vector<std::future<void>> tasks;
    std::atomic<int> processed{0};
    unsigned int maxThreads = std::thread::hardware_concurrency();
    for (int i = 0; i < fileCount; ++i) {
        if (LUTools_IsCancelled()) {
            g_lastError = "Operation cancelled";
            Log(g_lastError, 1);
            return CANCELLED;
        }
        tasks.push_back(std::async(std::launch::async, [i, inputPaths, outputPaths, &luts, whiteBalance, tint, brightness, contrast, saturation, logCallback, userData, &processed, fileCount]() {
            Image img = loadImage(inputPaths[i]);
            if (!img.valid()) {
                Log("Failed to load image: " + std::string(inputPaths[i]), 1);
                return;
            }
            for (const auto& lut : luts) {
                img = processImageParallel(img, lut.lut, lut.size, lut.blend, whiteBalance, tint, brightness, contrast, saturation);
                if (!img.valid()) {
                    Log("Failed to process image: " + std::string(inputPaths[i]), 1);
                    return;
                }
            }
            bool success = saveImage(img, outputPaths[i], "jpg");
            if (!success) {
                Log("Failed to save image: " + std::string(outputPaths[i]), 1);
                return;
            }
            Log("Processed: " + std::string(inputPaths[i]) + " -> " + std::string(outputPaths[i]), 0);
            if (g_progressCallback) {
                float progress = static_cast<float>(++processed) / fileCount;
                try {
                    g_progressCallback(progress, g_progressUserData);
                } catch (...) {
                    // Игнорируем исключения в callback
                }
            }
        }));
    }
    for (auto& t : tasks) {
        try {
            t.get();
        } catch (const std::exception& e) {
            g_lastError = "Exception in task: " + std::string(e.what());
            Log(g_lastError, 1);
        }
    }
    return SUCCESS;
}

void LUTools_FreeMemory(unsigned char* data) {
    if (data) {
        free(data);
    }
}

void LUTools_SetLogCallback(LogCallback callback, void* userData) {
    LockG lock(g_mutex);
    g_logCallback = callback;
    g_logUserData = userData;
}

void LUTools_SetProgressCallback(ProgressCallback callback, void* userData) {
    LockG lock(g_mutex);
    g_progressCallback = callback;
    g_progressUserData = userData;
}

void LUTools_SetCancelFlag(int* cancelFlag) {
    LockG lock(g_mutex);
    g_cancelFlag = cancelFlag;
}

int LUTools_IsCancelled() {
    LockG lock(g_mutex);
    return g_cancelFlag && *g_cancelFlag != 0;
}

void LUTools_GetLastErrorMessage(const char** message) {
    LockG lock(g_mutex);
    *message = g_lastError.c_str();
}

int LUTools_ResizeImage(unsigned char* inputData,
                        int width, int height, int channels,
                        int newWidth, int newHeight,
                        unsigned char** outputData,
                        int* outWidth, int* outHeight, int* outChannels)
{
    if (!inputData || !outputData || !outWidth || !outHeight || !outChannels || channels != 3)
    {
        g_lastError = "Invalid input data or parameters";
        return INVALID_IMAGE;
    }

    Image src;
    src.width  = width;
    src.height = height;
    src.channels = channels;
    src.data.assign(inputData, inputData + width * height * channels);

    Image dst  = resizeImage(src, newWidth, newHeight);
    if (!dst.valid()) {
        g_lastError = "Resize failed";
        return INVALID_IMAGE;
    }

    *outWidth  = dst.width;
    *outHeight = dst.height;
    *outChannels = dst.channels;
    *outputData = (unsigned char*)malloc(dst.data.size());
    if (!*outputData) {
        g_lastError = "Memory allocation failed";
        return MEMORY_ALLOCATION_FAILED;
    }
    std::copy(dst.data.begin(), dst.data.end(), *outputData);
    return SUCCESS;
}






// ───────────────────────────────────────────────────────────────
//  Создание 3‑D LUT из пары картинок "до / после"
//     before_path  – необработанное изображение
//     after_path   – то же изображение, уже покрашенное
//     output_path_prefix – куда писать *.cube (без расширения)
//     lut_sizes[]  – массив из {16,32,64…}, сколько LUT‑ов делать
// ───────────────────────────────────────────────────────────────
int LUTools_CreateLUTFromImages(const char* before_path,
                                const char* after_path,
                                const char* output_path_prefix,
                                const int* lut_sizes,
                                int num_sizes)
{
    LockG lock(g_mutex);
    auto t0 = std::chrono::high_resolution_clock::now();

    // 1️⃣  Загрузка и первичная проверка
    Image before = loadImage(before_path);
    Image after  = loadImage(after_path);
    if (!before.valid() || !after.valid()) {
        g_lastError = "Failed to load images: "s + before_path + " / " + after_path;
        Log(g_lastError, 1);
        return INVALID_IMAGE;
    }
    if (before.channels != 3 || after.channels != 3) {
        g_lastError = "Images must be RGB (3 channels)";
        Log(g_lastError, 1);
        return INVALID_IMAGE;
    }

    // 2️⃣  Даунскейл до 1000×750 (чтобы ускорить k‑NN)
    const int MAX_W = 1000, MAX_H = 750;
    float sc = std::min(float(MAX_W)/before.width, float(MAX_H)/before.height);
    int dw = std::max(1, int(before.width  * sc + .5f));
    int dh = std::max(1, int(before.height * sc + .5f));
    before = resizeImage(before, dw, dh);
    after  = resizeImage(after , dw, dh);
    if (!before.valid() || !after.valid()) {
        g_lastError = "Resize failed";
        Log(g_lastError, 1);
        return INVALID_IMAGE;
    }

    // 3️⃣  Переводим в float‑вектора
    int pixels = dw * dh;
    std::vector<float> src(pixels*3), dst(pixels*3);
    for (int i = 0; i < pixels; ++i) {
        src[i*3+0] = before.data[i*3+0] / 255.f;
        src[i*3+1] = before.data[i*3+1] / 255.f;
        src[i*3+2] = before.data[i*3+2] / 255.f;
        dst[i*3+0] = after .data[i*3+0] / 255.f;
        dst[i*3+1] = after .data[i*3+1] / 255.f;
        dst[i*3+2] = after .data[i*3+2] / 255.f;
    }

    //  🔹  Ограничим до 1 000 000 пикселей случайной выборкой
    const int MAX_SAMPLES = 1'000'000;
    if (pixels > MAX_SAMPLES) {
        std::vector<int> idx(pixels);
        std::iota(idx.begin(), idx.end(), 0);
        std::shuffle(idx.begin(), idx.end(), std::mt19937(std::random_device{}()));

        std::vector<float> s2(MAX_SAMPLES*3), d2(MAX_SAMPLES*3);
        for (int i = 0; i < MAX_SAMPLES; ++i) {
            s2[i*3+0] = src[idx[i]*3+0];
            s2[i*3+1] = src[idx[i]*3+1];
            s2[i*3+2] = src[idx[i]*3+2];
            d2[i*3+0] = dst[idx[i]*3+0];
            d2[i*3+1] = dst[idx[i]*3+1];
            d2[i*3+2] = dst[idx[i]*3+2];
        }
        src.swap(s2);  dst.swap(d2);
        pixels = MAX_SAMPLES;
        Log("Down‑sampled to " + std::to_string(MAX_SAMPLES) + " pixels", 0);
    }

    // 4️⃣  Генерируем LUT для каждого размера
    const int K = 15;          // k‑NN
    const float BLEND = 1.0f;  // 1.0 = чистая коррекция
    unsigned threads = std::thread::hardware_concurrency();

    for (int s = 0; s < num_sizes; ++s) {
        int N = lut_sizes[s];                   // размер решётки
        if (N < 2) continue;
        int total = N*N*N;                      // кол-во узлов
        std::vector<float> lut(total*3, 0.f);

        // ◾️ 4.1  Строим Hald‑CLUT координаты (0 … 1)
        std::vector<float> grid(total*3);
        float inv = 1.f / float(N-1);           // 👉 делим на (N‑1) !
        int p = 0;
        for (int r = 0; r < N; ++r)
            for (int g = 0; g < N; ++g)
                for (int b = 0; b < N; ++b, ++p) {
                    grid[p*3+0] = r*inv;
                    grid[p*3+1] = g*inv;
                    grid[p*3+2] = b*inv;
                }

        // ◾️ 4.2  k‑NN (параллельно)
        std::vector<std::future<void>> pool;
        for (int n = 0; n < total; ++n) {
            pool.emplace_back(std::async(std::launch::async,[&,n]{
                const float* q = &grid[n*3];
                // простой O(N) k‑NN (для картинки хватает)
                std::vector<std::pair<float,int>> best(K,{1e9f,0});
                for (int i = 0; i < pixels; ++i) {
                    float dr = q[0]-src[i*3+0];
                    float dg = q[1]-src[i*3+1];
                    float db = q[2]-src[i*3+2];
                    float d2 = dr*dr + dg*dg + db*db;
                    if (d2 < best.back().first) {
                        best.back() = {d2,i};
                        std::sort(best.begin(),best.end(),
                                  [](auto&a,auto&b){return a.first<b.first;});
                    }
                }
                // Взвешенное усреднение 1/дистанция
                float sumW=0, r=0,g=0,b=0;
                for (auto& pr: best) {
                    float w = 1.f / (std::sqrt(pr.first)+1e-5f);
                    int idx = pr.second;
                    r += dst[idx*3+0]*w;
                    g += dst[idx*3+1]*w;
                    b += dst[idx*3+2]*w;
                    sumW += w;
                }
                r/=sumW; g/=sumW; b/=sumW;
                // BLEND между исходной решёткой и таргетом
                lut[n*3+0] = std::clamp( BLEND*r + (1-BLEND)*q[0], 0.f,1.f );
                lut[n*3+1] = std::clamp( BLEND*g + (1-BLEND)*q[1], 0.f,1.f );
                lut[n*3+2] = std::clamp( BLEND*b + (1-BLEND)*q[2], 0.f,1.f );
            }));
            if (pool.size()>=threads){
                for(auto& f:pool) f.get();
                pool.clear();
            }
        }
        for(auto& f:pool) f.get();

        // ◾️ 4.3  Экспорт *.cube  (B‑outer, G‑mid, R‑inner)
        std::string path = std::string(output_path_prefix) + "_" + std::to_string(N)+".cube";
        std::ofstream f(path);
        if(!f){ Log("Can't write "+path,1); continue; }

        f << "TITLE \"Generated LUT\"\n"
          << "LUT_3D_SIZE " << N << "\n"
          << "DOMAIN_MIN 0.0 0.0 0.0\n"
          << "DOMAIN_MAX 1.0 1.0 1.0\n"
          << std::fixed << std::setprecision(10);

        for (int bb=0; bb<N; ++bb)
            for (int gg=0; gg<N; ++gg)
                for (int rr=0; rr<N; ++rr) {
                    int id = (rr*N*N + gg*N + bb)*3;
                    f << lut[id  ] << ' ' << lut[id+1] << ' ' << lut[id+2] << '\n';
                }
        f.close();
        Log("Created LUT: "+path,0);

        if (g_progressCallback) {
            try { g_progressCallback(float(s+1)/num_sizes, g_progressUserData); }
            catch(...) {}
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    Log("LUT creation took "
        + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count())
        + " ms",0);
    return SUCCESS;
}
