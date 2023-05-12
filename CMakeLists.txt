cmake_minimum_required(VERSION 3.25)
project(cross-platform-app)

set(CXX_STANDARD C++23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ASSET_DIR bin)

add_custom_target(copy_assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/${ASSET_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${ASSET_DIR}
)
add_executable(${CMAKE_PROJECT_NAME} "")

add_subdirectory(src)
add_dependencies(${CMAKE_PROJECT_NAME} copy_assets)

