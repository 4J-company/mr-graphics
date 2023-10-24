cmake_minimum_required(VERSION 3.27)

set(DEBUG false)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_COMPILER clang)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(model-renderer)

# add dependencies
include(cmake/CPM.cmake)
include(cmake/download_dependencies.cmake)

# add executable
xwin_add_executable(${CMAKE_PROJECT_NAME} "")

## link external dependencies
# VULKAN
find_package(Vulkan REQUIRED)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} Vulkan::Vulkan)
# STB
find_package(stb REQUIRED)
target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    stb::image
    CrossWindow
    CrossWindowGraphics
)

add_subdirectory(src)

# copy assets
set(ASSET_DIR bin)
add_custom_target(CopyAssets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/${ASSET_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${ASSET_DIR}
  )
add_dependencies(${CMAKE_PROJECT_NAME} CopyAssets)
