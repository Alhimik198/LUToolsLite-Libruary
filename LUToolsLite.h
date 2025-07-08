#ifndef LUTOOLSLITE_H
#define LUTOOLSLITE_H

#ifdef __cplusplus
extern "C" {
#endif

// === УПРАВЛЕНИЕ ЭКСПОРТОМ (Windows / Linux / macOS) ===
#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef LUTOOLSLITE_EXPORTS
#    define LTL_API __declspec(dllexport)
#  else
#    define LTL_API __declspec(dllimport)
#  endif
#else
#  define LTL_API
#endif

// === КОДЫ ОШИБОК ===
#define SUCCESS 0
#define INVALID_LUT 1
#define INVALID_IMAGE 2
#define MEMORY_ALLOCATION_FAILED 3
#define CANCELLED 4
#define INITIALIZATION_FAILED 5

// === КОЛБЭКИ ===
typedef void (*LogCallback)(const char* message, int is_error, void* user_data);
typedef void (*ProgressCallback)(float progress, void* user_data);

// === ИНИЦИАЛИЗАЦИЯ / ОСВОБОЖДЕНИЕ ===
LTL_API int  LUTools_Init();
LTL_API void LUTools_Cleanup();

// === LUT ===
LTL_API int  LUTools_LoadLUT(const char* filePath, float blend, int* lutId);
LTL_API void LUTools_UnloadLUT(int lutId);
LTL_API void LUTools_ClearLUTs();

// === ОБРАБОТКА ФАЙЛОВ ===
LTL_API int LUTools_ProcessFile(
    const char* inputPath,
    const char* outputPath,
    const int* lutIds,
    int lutCount,
    float whiteBalance, float tint, float brightness, float contrast, float saturation,
    LogCallback logCallback, void* userData);

LTL_API int LUTools_ProcessFiles(
    const char** inputPaths,
    const char** outputPaths,
    int fileCount,
    const int* lutIds,
    int lutCount,
    float whiteBalance, float tint, float brightness, float contrast, float saturation,
    LogCallback logCallback, void* userData);

// === ОБРАБОТКА ИЗОБРАЖЕНИЙ ===
LTL_API int LUTools_ProcessImage(
    unsigned char* inputData,
    int width, int height, int channels,
    const int* lutIds,
    int lutCount,
    float whiteBalance, float tint, float brightness, float contrast, float saturation,
    unsigned char** outputData,
    int* outWidth, int* outHeight, int* outChannels);

LTL_API int LUTools_GeneratePreview(
    unsigned char* inputData,
    int width, int height, int channels,
    const int* lutIds,
    int lutCount,
    float whiteBalance, float tint, float brightness, float contrast, float saturation,
    int previewWidth, int previewHeight,
    unsigned char** outputData,
    int* outWidth, int* outHeight, int* outChannels);

// === НОВОЕ: превью с пропорциональной подгонкой ===
LTL_API int LUTools_GeneratePreviewFit(
    unsigned char* inputData,
    int width, int height, int channels,
    const int* lutIds,
    int lutCount,
    float whiteBalance, float tint, float brightness, float contrast, float saturation,
    int maxWidth, int maxHeight,
    unsigned char** outputData,
    int* outWidth, int* outHeight, int* outChannels);

// === НОВОЕ: простой ресайз изображения ===
LTL_API int LUTools_ResizeImage(
    unsigned char* inputData,
    int width, int height, int channels,
    int newWidth, int newHeight,
    unsigned char** outputData,
    int* outWidth, int* outHeight, int* outChannels);

// === ОСВОБОЖДЕНИЕ ПАМЯТИ ===
LTL_API void LUTools_FreeMemory(unsigned char* data);

// === КОЛБЭКИ ===
LTL_API void LUTools_SetLogCallback(LogCallback callback, void* userData);
LTL_API void LUTools_SetProgressCallback(ProgressCallback callback, void* userData);

// === ОТМЕНА ===
LTL_API void LUTools_SetCancelFlag(int* cancelFlag);
LTL_API int  LUTools_IsCancelled();

// === ПОЛУЧЕНИЕ ОШИБКИ ===
LTL_API void LUTools_GetLastErrorMessage(const char** message);

// === СОЗДАНИЕ LUT ИЗ ИЗОБРАЖЕНИЙ ===
LTL_API int LUTools_CreateLUTFromImages(
    const char* before_path,
    const char* after_path,
    const char* output_path_prefix,
    const int* lut_sizes,
    int num_sizes);

#ifdef __cplusplus
}
#endif

#endif // LUTOOLSLITE_H
