Типы данных



LogCallbackType = CFUNCTYPE(None, c_char_p, c_int, c_void_p)
ProgressCallbackType = CFUNCTYPE(None, c_float, c_void_p)
📘 Определение сигнатур
1. 🎬 Инициализация и очистка



lutools.LUTools_Init.restype = c_int
lutools.LUTools_Cleanup.restype = None
2. 📦 Работа с LUT



lutools.LUTools_LoadLUT.argtypes = [c_char_p, c_float, POINTER(c_int)]
lutools.LUTools_LoadLUT.restype = c_int

lutools.LUTools_UnloadLUT.argtypes = [c_int]
lutools.LUTools_UnloadLUT.restype = None

lutools.LUTools_ClearLUTs.restype = None
3. 🖼️ Обработка одиночного файла



lutools.LUTools_ProcessFile.argtypes = [
    c_char_p, c_char_p,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    LogCallbackType, c_void_p
]
lutools.LUTools_ProcessFile.restype = c_int
4. 🧃 Обработка изображения в памяти



lutools.LUTools_ProcessImage.argtypes = [
    POINTER(c_ubyte), c_int, c_int, c_int,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    POINTER(POINTER(c_ubyte)),
    POINTER(c_int), POINTER(c_int), POINTER(c_int)
]
lutools.LUTools_ProcessImage.restype = c_int
5. 🖼️ Генерация превью
5.1 С заданным размером



lutools.LUTools_GeneratePreview.argtypes = [
    POINTER(c_ubyte), c_int, c_int, c_int,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    c_int, c_int,
    POINTER(POINTER(c_ubyte)),
    POINTER(c_int), POINTER(c_int), POINTER(c_int)
]
lutools.LUTools_GeneratePreview.restype = c_int
5.2 Вписывание с сохранением пропорций



lutools.LUTools_GeneratePreviewFit.argtypes = [
    POINTER(c_ubyte), c_int, c_int, c_int,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    c_int, c_int,
    POINTER(POINTER(c_ubyte)),
    POINTER(c_int), POINTER(c_int), POINTER(c_int)
]
lutools.LUTools_GeneratePreviewFit.restype = c_int
6. 📐 Ресайз изображения



lutools.LUTools_ResizeImage.argtypes = [
    POINTER(c_ubyte),
    c_int, c_int, c_int,
    c_int, c_int,
    POINTER(POINTER(c_ubyte)),
    POINTER(c_int), POINTER(c_int), POINTER(c_int)
]
lutools.LUTools_ResizeImage.restype = c_int
7. 🗂️ Пакетная обработка файлов



lutools.LUTools_ProcessFiles.argtypes = [
    POINTER(c_char_p), POINTER(c_char_p), c_int,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    LogCallbackType, c_void_p
]
lutools.LUTools_ProcessFiles.restype = c_int
8. 🧹 Освобождение памяти



lutools.LUTools_FreeMemory.argtypes = [POINTER(c_ubyte)]
lutools.LUTools_FreeMemory.restype = None
🧠 Обязательно освобождай память, выделенную под outputData, этой функцией!

9. 📢 Колбэки



lutools.LUTools_SetLogCallback.argtypes = [LogCallbackType, c_void_p]
lutools.LUTools_SetLogCallback.restype = None

lutools.LUTools_SetProgressCallback.argtypes = [ProgressCallbackType, c_void_p]
lutools.LUTools_SetProgressCallback.restype = None
10. ⛔ Отмена операций



lutools.LUTools_SetCancelFlag.argtypes = [POINTER(c_int)]
lutools.LUTools_SetCancelFlag.restype = None

lutools.LUTools_IsCancelled.restype = c_int
11. 🧾 Получение последней ошибки



lutools.LUTools_GetLastErrorMessage.argtypes = [POINTER(c_char_p)]
lutools.LUTools_GetLastErrorMessage.restype = None
12. 🎨 Создание LUT из изображений



lutools.LUTools_CreateLUTFromImages.argtypes = [
    c_char_p, c_char_p,
    c_char_p,
    POINTER(c_int), c_int
]
lutools.LUTools_CreateLUTFromImages.restype = c_int
Пояснение:
beforePath: путь к "до"-изображению

afterPath: путь к "после"-изображению

outputPathPrefix: префикс для .cube-файлов

lutSizes: массив размеров (16, 32, 64)

numSizes: длина массива lutSizes

✅ Пример использования



lut_id = c_int()
res = lutools.LUTools_LoadLUT(b"D:/LT/mylut.cube", c_float(1.0), ctypes.byref(lut_id))
if res != 0:
    err = c_char_p()
    lutools.LUTools_GetLastErrorMessage(ctypes.byref(err))
    print("Ошибка:", err.value.decode())
📌 Особенности
Все изображения должны быть в формате RGB (3 канала).

Все цвета работают в диапазоне 0.0 - 1.0.

Если возвращается ненулевой код, всегда вызывай LUTools_GetLastErrorMessage.

Используй LUTools_FreeMemory, чтобы избежать утечек при получении outputData.



