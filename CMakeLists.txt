cmake_minimum_required(VERSION 3.25)

set(CXX_STANDARD C++20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_COMPILER clang)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ASSET_DIR bin)
add_custom_target(copy_assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/${ASSET_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${ASSET_DIR}
)

project(model-renderer)

# external dependencies
add_subdirectory(${PROJECT_SOURCE_DIR}/external)

# add executable
xwin_add_executable(${CMAKE_PROJECT_NAME} "")

# link external dependencies
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    CrossWindow
    CrossWindowGraphics
)

add_subdirectory(src)
add_dependencies(${CMAKE_PROJECT_NAME} copy_assets)

