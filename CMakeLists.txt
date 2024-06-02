cmake_minimum_required(VERSION 3.27)
project(model-renderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)

# download CPM.cmake
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# add dependencies
CPMAddPackage(
  NAME CrossWindow
  GITHUB_REPOSITORY 4J-company/mr-window
  GIT_TAG fix/ubuntu-compilation
  DOWNLOAD_ONLY YES
  )
CPMAddPackage(
  NAME CrossWindowGraphics
  GITHUB_REPOSITORY cone-forest/CrossWindow-Graphics
  GIT_TAG master
  )
CPMAddPackage(
  NAME assimp
  GITHUB_REPOSITORY assimp/assimp
  GIT_TAG master
  )

# link external dependencies
find_package(Vulkan REQUIRED)
find_package(assimp REQUIRED)
add_subdirectory(${CrossWindow_SOURCE_DIR})
find_package(CrossWindow REQUIRED)
find_package(CrossWindowGraphics REQUIRED)

# find sources
execute_process(
    COMMAND bash "-c" "find ${CMAKE_CURRENT_LIST_DIR}/src -name '*.cpp'"
    OUTPUT_VARIABLE SOURCES
)
# add executable
add_executable(${CMAKE_PROJECT_NAME} ${XMAIN_SOURCES})
add_subdirectory(src)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ${XWIN_DEFINITIONS})

# dependencies
target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    Vulkan::Vulkan
    assimp::assimp
    CrossWindow
    CrossWindowGraphics
)
target_include_directories(
  ${CMAKE_PROJECT_NAME} PUBLIC
  ${Vulkan_INCLUDE_DIRS}
  ${assimp_INCLUDE_DIRS}
)

# source
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer
  )
target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/pch.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/renderer.hpp
  )

# copy assets
set(ASSET_DIR bin)
add_custom_target(CopyAssets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/${ASSET_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${ASSET_DIR}
  )
add_dependencies(${CMAKE_PROJECT_NAME} CopyAssets)

