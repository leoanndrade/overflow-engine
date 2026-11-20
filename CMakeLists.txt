cmake_minimum_required(VERSION 3.22)
project(CustomEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Diretórios de saída de binários centralizados
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# -----------------------------------------------------------------------------
# Dependências Externas (vendor/)
# -----------------------------------------------------------------------------

# 1. GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glfw)

# 2. GLAD (OpenGL 4.6 Core Profile Loader)
add_library(glad STATIC
    vendor/glad/src/gl.c
    vendor/glad/include/glad/gl.h
    vendor/glad/include/KHR/khrplatform.h
)
target_include_directories(glad PUBLIC vendor/glad/include)

# 3. spdlog (Fast logging)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/spdlog)

# 4. GLM (OpenGL Mathematics)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glm)

# 5. ImGui
add_library(imgui STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    vendor/imgui
    vendor/imgui/backends
    vendor/glfw/include
    vendor/glad/include
)

# -----------------------------------------------------------------------------
# Target: Engine Core (Biblioteca Estática)
# -----------------------------------------------------------------------------
file(GLOB_RECURSE ENGINE_SOURCES
    src/Engine/*.cpp
    src/Engine/*.h
)

add_library(Engine STATIC ${ENGINE_SOURCES})

target_include_directories(Engine PUBLIC
    src
    vendor/glad/include
    vendor/glfw/include
    vendor/spdlog/include
    vendor/glm
    vendor/imgui
    vendor/stb_image
)

target_link_libraries(Engine PUBLIC
    glad
    glfw
    spdlog::spdlog
    glm::glm
    imgui
)

if (MSVC)
    target_compile_options(Engine PRIVATE /W4 /permissive-)
    target_link_libraries(Engine PUBLIC opengl32.lib)
endif()

# -----------------------------------------------------------------------------
# Target: Editor Executável
# -----------------------------------------------------------------------------
file(GLOB_RECURSE EDITOR_SOURCES
    src/Editor/*.cpp
    src/Editor/*.h
)

add_executable(CustomEditor ${EDITOR_SOURCES})

target_include_directories(CustomEditor PRIVATE
    src
)

target_link_libraries(CustomEditor PRIVATE
    Engine
)
