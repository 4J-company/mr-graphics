cmake_minimum_required(VERSION 3.27)
project(model-renderer)

# set config dirs
set(MR_PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR})
if (NOT DEFINED MR_RES_DIR)
  set(MR_RES_FULL_DIR ${MR_PROJECT_DIR}/bin)
else()
  set(MR_RES_FULL_DIR ${MR_PROJECT_DIR}/${MR_RES_DIR})
endif()

message(STATUS "Generating with following config paths: ")
message(STATUS "  Project dir: ${MR_PROJECT_DIR}")
message(STATUS "  Resource dir: ${MR_RES_FULL_DIR}")
message("")

include(${CMAKE_CURRENT_LIST_DIR}/cmake/options.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/settings.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake)

# add executable
add_executable(${CMAKE_PROJECT_NAME} "")
add_subdirectory(src)

# dependencies
target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    ${DEPS_LIBRARIES}
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
target_compile_definitions(
  ${CMAKE_PROJECT_NAME} PRIVATE
  MR_PROJECT_DIR="${CMAKE_CURRENT_LIST_DIR}"
  MR_RES_DIR="${MR_RES_FULL_DIR}"
)

set(TARGET_DEPS write-licenses)
if (GENERATE_DEPENDENCY_GRAPH)
  set(TARGET_DEPS dep-graph-${CMAKE_PROJECT_NAME})
endif()
if (FORMAT_SOURCE)
  set(TARGET_DEPS fix-format)
endif()

add_dependencies(
  ${CMAKE_PROJECT_NAME}
  ${TARGET_DEPS}
)
GroupSourcesByFolder(${CMAKE_PROJECT_NAME}) # better IDE integration
