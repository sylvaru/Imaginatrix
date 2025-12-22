# ix_engine/CMakeLists.txt
cmake_minimum_required(VERSION 3.24)

project(ix_engine)

add_library(ix_engine STATIC
    platform/glfw_platform.h
    platform/glfw_platform.cpp
    platform/rendering/rendering_api.cpp
    platform/rendering/vk/vk_instance.h
    platform/rendering/vk/vk_instance.cpp
    platform/rendering/vk/vk_context.cpp
    platform/rendering/vk/vk_context.h
    platform/rendering/vk/vk_swapchain.h
    platform/rendering/vk/vk_swapchain.cpp
    platform/rendering/vk/vk_image.h
    platform/rendering/vk/vk_image.cpp
    platform/rendering/vk/vk_pipeline.h
    platform/rendering/vk/vk_pipeline.cpp
    platform/rendering/vk/vk_pipeline_manager.h
    platform/rendering/vk/vk_pipeline_manager.cpp
    platform/rendering/vk/vk_renderer.h 
    platform/rendering/vk/vk_renderer.cpp

    core/asset_manager.h
    core/asset_manager.cpp
    core/scene.h
    core/scene.cpp
    core/scene_manager.h
    core/scene_manager.cpp
    core/entity.h

    engine.h
    engine.cpp
  )

target_precompile_headers(ix_engine
    PRIVATE
    common/engine_pch.h
)
add_compile_definitions(
    IX_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_include_directories(ix_engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/ix_external/glfw/include
)

target_link_libraries(ix_engine
    PUBLIC 
        ix_interface
        glm
        nlohmann_json::nlohmann_json
        entt
        spdlog::spdlog_header_only
    PRIVATE 
        glfw
        vulkan_i
)