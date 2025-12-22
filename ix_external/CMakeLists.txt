# ix_external/CMakeLists.txt
cmake_minimum_required(VERSION 3.24)
project(ix_external)

find_package(Vulkan REQUIRED)

# ==========================================================
# spdlog (Logging)
# ==========================================================
# Use header only target when linking
add_subdirectory(spdlog)

# ==========================================================
# GLM (Math - header-only)
# ==========================================================
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/glm")

# ==========================================================
# GLFW (Windowing)
# ==========================================================
add_subdirectory(glfw)

# ==========================================================
# json (Serialization - header-only)
# ==========================================================
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
add_subdirectory(json)

# ==========================================================
# KTX (Texture Loading)
# ==========================================================
add_library(ktx INTERFACE)

target_include_directories(ktx INTERFACE 
    "${CMAKE_CURRENT_SOURCE_DIR}/ktx/interface"      # Main public API (ktx.h)
    "${CMAKE_CURRENT_SOURCE_DIR}/ktx/other_include"  # Support headers
    "${CMAKE_CURRENT_SOURCE_DIR}/ktx/lib"            # Internal library headers
)

# ==========================================================
# STB (Image/Utility - header-only)
# ==========================================================
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb")

# ==========================================================
# tinygltf (GLTF Loading - header-only)
# ==========================================================
add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/tinygltf")

# ==========================================================
# EnTT (ECS Framework - header-only)
# ==========================================================
add_library(entt INTERFACE)
target_include_directories(entt INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/entt/src")


# ==========================================================
# Add vulkan interface target
# ==========================================================
add_library(vulkan_i INTERFACE)

target_link_libraries(vulkan_i INTERFACE
    Vulkan::Vulkan
)