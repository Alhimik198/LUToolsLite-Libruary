cmake_minimum_required(VERSION 3.10)

project(LUToolsLite VERSION 1.0 LANGUAGES CXX)

# ─────────────────────────────────────────────────────────────
# ➜ 1. Глобальные флаги компиляции/оптимизации
# ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

# (опционально) собирать с /MT, чтобы CRT был статически линкуем
option(LTL_STATIC_CRT "Link the MSVC runtime statically (/MT)" ON)

if (MSVC)
    if (LTL_STATIC_CRT)
        foreach(flag_var CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG
                         CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG)
            string (REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endforeach()
    endif()
    add_compile_options(/O2)
else()
    add_compile_options(-O3 -fPIC)
endif()

# ─────────────────────────────────────────────────────────────
# ➜ 2. Исходники / include‑пути
# ─────────────────────────────────────────────────────────────
set(LTL_SRC
    lutools.cpp
    image_io.cpp
    cube_loader.cpp
)

set(LTL_HEADERS
    LUToolsLite.h       #       ←  публичный
    cube_loader.hpp
    interpolator.hpp
)

# Путь к header‑only библиотекам stb
set(STB_DIR "${CMAKE_SOURCE_DIR}/stb")

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${STB_DIR}
)

# ─────────────────────────────────────────────────────────────
# ➜ 3. Цель shared‑library
# ─────────────────────────────────────────────────────────────
add_library(LUToolsLite SHARED
    ${LTL_SRC}
    ${LTL_HEADERS}
)

# Чтобы хедер видел __declspec(dllexport)
target_compile_definitions(LUToolsLite PRIVATE LUTOOLSLITE_EXPORTS)

# Для не‑MSVC добавляем -fvisibility=hidden и заставляем экспортировать только,
# что помечено LTL_API  (необязательно, но аккуратно)
if (NOT MSVC)
    set_target_properties(LUToolsLite PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()

# ─────────────────────────────────────────────────────────────
# ➜ 4. Установка (cmake --install)
# ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)

# 4.1 – Бинарник и импорт‑lib
install(TARGETS LUToolsLite
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 4.2 – Публичные заголовки + stb headers (header‑only → просто скопировать)
install(FILES
        LUToolsLite.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY ${STB_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/stb
        FILES_MATCHING PATTERN "*.h")

# ─────────────────────────────────────────────────────────────
# ➜ 5. Показываем результат при конфигурировании
# ─────────────────────────────────────────────────────────────
message(STATUS "--------------------------------------------------")
message(STATUS "LUToolsLite  will be installed to: ${CMAKE_INSTALL_PREFIX}")
if (MSVC AND LTL_STATIC_CRT)
    message(STATUS "MSVC runtime: static (/MT)")
elseif(MSVC)
    message(STATUS "MSVC runtime: dynamic (/MD)  → потребуется VC‑Redistributable")
endif()
message(STATUS "--------------------------------------------------")
