# sandbox_game/CMakeLists.txt
add_executable(sandbox_game)

target_sources(sandbox_game
    PRIVATE
        main.cpp
        game_layer.h
        game_layer.cpp
        common/game_pch.h
)

add_compile_definitions(
    GAME_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_precompile_headers(sandbox_game
    PRIVATE
    common/game_pch.h
)

target_include_directories(sandbox_game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sandbox_game
    PRIVATE
		ix_interface
        ix_engine
        glm
        nlohmann_json::nlohmann_json
        spdlog::spdlog_header_only
)

