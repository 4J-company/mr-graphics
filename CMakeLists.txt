cmake_minimum_required(VERSION 3.27)

set(MR_GRAPHICS_PROJECT_NAME mr-graphics)
set(MR_GRAPHICS_LIB_NAME mr-graphics-lib)

project(
  ${MR_GRAPHICS_PROJECT_NAME}
  VERSION 1.0.0
  LANGUAGES CXX
)

set(MR_PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE STRING "Project working directory")
set(MR_RES_DIR     bin                       CACHE STRING "Project asset directory")

set(MR_RES_FULL_DIR ${MR_PROJECT_DIR}/${MR_RES_DIR})

message(STATUS "Generating with following config paths: ")
message(STATUS "  Project directory: ${MR_PROJECT_DIR}")
message(STATUS "  Resource directory: ${MR_RES_FULL_DIR}")

include(${CMAKE_CURRENT_LIST_DIR}/cmake/options.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/settings.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake)

file(GLOB_RECURSE SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)

file(GLOB_RECURSE HEADERS
  ${CMAKE_CURRENT_LIST_DIR}/*.hpp
)

add_library(${MR_GRAPHICS_LIB_NAME} ${SOURCES} ${HEADERS})

option(BUILD_EXAMPLES "Build examples" ON)
if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# dependencies
target_link_libraries(
  ${MR_GRAPHICS_LIB_NAME}
    ${DEPS_LIBRARIES}
)

# headers
target_include_directories(${MR_GRAPHICS_LIB_NAME}
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/renderer
  )
target_precompile_headers(${MR_GRAPHICS_LIB_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/pch.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/renderer.hpp
  )
target_compile_definitions(
  ${MR_GRAPHICS_LIB_NAME} PUBLIC
  MR_PROJECT_DIR="${CMAKE_CURRENT_LIST_DIR}"
  MR_RES_DIR="${MR_RES_FULL_DIR}"
)

if (GENERATE_DEPENDENCY_GRAPH)
  add_dependencies(
    ${MR_GRAPHICS_LIB_NAME}
    dep-graph-${CMAKE_PROJECT_NAME}
  )
endif()
if (FORMAT_SOURCE)
  add_dependencies(
    ${MR_GRAPHICS_LIB_NAME}
    fix-format
  )
endif()

# Install rules
include(GNUInstallDirs)

# Install all targets together in the same export set
# This is required because CMake checks that all dependencies of an exported target
# are also in the export set. The wrapper libraries must be installed with the main target.
install(TARGETS ${MR_GRAPHICS_LIB_NAME}
              libvkfw
              vk-bootstrap-lib vk-bootstrap vk-bootstrap-compiler-warnings
              VulkanMemoryAllocator
        EXPORT ${MR_GRAPHICS_LIB_NAME}-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        OPTIONAL)

install(EXPORT ${MR_GRAPHICS_LIB_NAME}-targets
        NAMESPACE ${MR_GRAPHICS_LIB_NAME}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${MR_GRAPHICS_LIB_NAME})
