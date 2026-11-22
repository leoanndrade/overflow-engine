cmake_minimum_required(VERSION 3.22)
project(OverflowEngine VERSION 0.1.0 LANGUAGES C CXX)

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
        assets/scripts/MyBehavior.h
        assets/scripts/MyBehavior.h
        assets/scripts/MyBehavior.h
)
target_include_directories(glad PUBLIC vendor/glad/include)

# 3. spdlog (Fast logging)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/spdlog)

# 4. GLM (OpenGL Mathematics)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glm)

# 5. ImGui & ImGuizmo
add_library(imgui STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
    vendor/ImGuizmo/src/ImGuizmo.cpp
        assets/scripts/MyBehavior.h
        assets/scripts/MyBehavior.h
)
target_include_directories(imgui PUBLIC
    vendor/imgui
    vendor/imgui/backends
    vendor/ImGuizmo/src
    vendor/glfw/include
    vendor/glad/include
)

# -----------------------------------------------------------------------------
# Target: Overflow Engine Core (Biblioteca Estática)
# -----------------------------------------------------------------------------
file(GLOB_RECURSE ENGINE_SOURCES
    src/Engine/*.cpp
    src/Engine/*.h
)

add_library(OverflowEngine STATIC ${ENGINE_SOURCES}
        assets/scripts/MyBehavior.h
        assets/scripts/MyBehavior.h)

target_include_directories(OverflowEngine PUBLIC
    src
    vendor/glad/include
    vendor/glfw/include
    vendor/spdlog/include
    vendor/glm
    vendor/imgui
    vendor/ImGuizmo/src
    vendor/stb_image
)

target_link_libraries(OverflowEngine PUBLIC
    glad
    glfw
    spdlog::spdlog
    glm::glm
    imgui
)

if (MSVC)
    target_compile_options(OverflowEngine PRIVATE /W4 /permissive- /wd4127)
    target_link_libraries(OverflowEngine PUBLIC opengl32.lib winmm.lib)
endif()

# -----------------------------------------------------------------------------
# Target: Overflow Editor Executável
# -----------------------------------------------------------------------------
file(GLOB_RECURSE EDITOR_SOURCES
    src/Editor/*.cpp
    src/Editor/*.h
)

add_executable(OverflowEditor ${EDITOR_SOURCES}
        assets/scripts/MyBehavior.h
        assets/scripts/MyBehavior.h)

target_include_directories(OverflowEditor PRIVATE
    src
    vendor/ImGuizmo/src
)

target_link_libraries(OverflowEditor PRIVATE
    OverflowEngine
)
