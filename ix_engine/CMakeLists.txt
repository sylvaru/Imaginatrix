# ix_engine/CMakeLists.txt
cmake_minimum_required(VERSION 3.24)

project(ix_engine)

add_library(ix_engine STATIC
    core/engine.h
    core/engine.cpp
 )

target_precompile_headers(ix_engine
    PRIVATE
    common/engine_pch.h
)

target_include_directories(ix_engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/
)

target_link_libraries(ix_engine
    PUBLIC ix_interface
)