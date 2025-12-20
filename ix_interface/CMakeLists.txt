# Imaginatrix/ix_interface/CMakeLists.txt

add_library(ix_interface INTERFACE)
target_include_directories(ix_interface
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)