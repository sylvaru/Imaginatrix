# ix_engine/CMakeLists.txt
cmake_minimum_required(VERSION 3.24)

project(ix_engine)

add_library(ix_engine STATIC

    # Misc depends
    common/vk_mem_alloc.cpp 
    common/handles.h

    # Platform/API specific
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
    platform/rendering/vk/vk_compute_pipeline.h
    platform/rendering/vk/vk_compute_pipeline.cpp
    platform/rendering/vk/vk_pipeline_manager.h
    platform/rendering/vk/vk_pipeline_manager.cpp
    platform/rendering/vk/vk_descriptor_manager.h
    platform/rendering/vk/vk_descriptor_manager.cpp
    platform/rendering/vk/vk_buffer.h
    platform/rendering/vk/vk_buffer.cpp
    platform/rendering/vk/vk_renderer.h 
    platform/rendering/vk/vk_renderer.cpp
    platform/rendering/vk/render_graph/vk_render_graph_registry.h
    platform/rendering/vk/render_graph/vk_render_graph_registry.cpp
    platform/rendering/vk/render_graph/vk_render_graph.h
    platform/rendering/vk/render_graph/vk_render_graph.cpp
    platform/rendering/vk/render_graph/vk_render_graph_builder.h
    platform/rendering/vk/render_graph/vk_render_graph_builder.cpp
    platform/rendering/vk/resource_types/vk_resource_types.h
    platform/rendering/vk/resource_types/vk_resource_types.cpp
    platform/rendering/vk/passes/forward_pass.h 
    platform/rendering/vk/passes/forward_pass.cpp
    platform/rendering/vk/passes/equirect_to_cube_pass.h
    platform/rendering/vk/passes/equirect_to_cube_pass.cpp
    platform/rendering/vk/passes/compute_culling_pass.h
    platform/rendering/vk/passes/compute_culling_pass.cpp
    platform/rendering/vk/passes/skybox_pass.h
    platform/rendering/vk/passes/skybox_pass.cpp
    platform/rendering/vk/passes/imgui_pass.h
    platform/rendering/vk/passes/imgui_pass.cpp
    platform/rendering/vk/passes/depth_pre_pass.h
    platform/rendering/vk/passes/depth_pre_pass.cpp
    platform/rendering/vk/passes/cluster_culling_pass.h
    platform/rendering/vk/passes/cluster_culling_pass.cpp
    platform/rendering/vk/passes/cluster_build_pass.h
    platform/rendering/vk/passes/cluster_build_pass.cpp

    # Core
    core/asset_manager.h
    core/asset_manager.cpp
    core/scene.h
    core/scene.cpp
    core/scene_manager.h
    core/scene_manager.cpp
    core/layers/imgui_layer.h
    core/layers/imgui_layer.cpp
    core/entity.h
    core/components.h

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
        vulkan_i
        vma
        imgui
    PRIVATE 
        glfw
        stb
        tinygltf
)