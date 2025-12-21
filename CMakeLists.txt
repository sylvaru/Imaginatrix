cmake_minimum_required(VERSION 3.24)
project(Imaginatrix LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# ----------------------------
# Options
# ----------------------------
option(IMAGINATRIX_BUILD_TESTS "Build tests" ON)
option(IMAGINATRIX_BUILD_EDITOR "Build editor" OFF)
option(IMAGINATRIX_BUILD_SHARED "Build libraries as shared" OFF)

# ----------------------------
# Platform-specific flags
# ----------------------------
if(MSVC)
    # Dynamic CRT for hot reload if you want /MD /MDd
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ----------------------------
# Output dirs
# ----------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ----------------------------
# Global definitions
# ----------------------------
add_compile_definitions(
    GLM_ENABLE_EXPERIMENTAL
    PROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# ----------------------------
# External libraries
# ----------------------------
add_subdirectory(ix_external)

# ----------------------------
# Core targets
# ----------------------------
add_subdirectory(ix_interface)
add_subdirectory(ix_engine)
add_subdirectory(sandbox_game)

# ----------------------------
# Shader compilation
# ----------------------------
file(GLOB SHADER_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/sandbox_game/res/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/sandbox_game/res/shaders/*.frag"
)

set(SPIRV_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sandbox_game/res/shaders/spirV")
file(MAKE_DIRECTORY ${SPIRV_OUTPUT_DIR})

set(SPIRV_BINARIES)
foreach(SHADER_FILE ${SHADER_FILES})
    get_filename_component(FILE_NAME ${SHADER_FILE} NAME)
    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT_DIR}/${FILE_NAME}.spv
        COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader: ${FILE_NAME}"
        COMMAND glslangValidator -V --target-env vulkan1.3 ${SHADER_FILE} -o ${SPIRV_OUTPUT_DIR}/${FILE_NAME}.spv
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling ${FILE_NAME} to SPIR-V"
    )
    list(APPEND SPIRV_BINARIES ${SPIRV_OUTPUT_DIR}/${FILE_NAME}.spv)
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SPIRV_BINARIES})
add_dependencies(sandbox_game CompileShaders)