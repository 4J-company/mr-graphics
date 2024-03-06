cmake_minimum_required(VERSION 3.27)

set(DEBUG false)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_COMPILER clang)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS OFF)

project(model-renderer)

# add dependencies
include(cmake/CPM.cmake)
include(cmake/download_dependencies.cmake)

# add executable
xwin_add_executable(${CMAKE_PROJECT_NAME} "")

# link external dependencies
find_package(Vulkan REQUIRED)
find_package(stb REQUIRED)
find_package(assimp REQUIRED)
find_package(CrossWindow REQUIRED)
find_package(CrossWindowGraphics REQUIRED)

target_include_directories(
  ${CMAKE_PROJECT_NAME} PUBLIC
  ${Vulkan_INCLUDE_DIRS}
  ${stb_INCLUDE_DIRS}
  ${assimp_INCLUDE_DIRS}
)

target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    Vulkan::Vulkan
    assimp::assimp
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
