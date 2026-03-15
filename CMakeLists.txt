cmake_minimum_required(VERSION 3.27)

set(MR_GRAPHICS_PROJECT_NAME mr-graphics)
set(MR_GRAPHICS_LIB_NAME mr-graphics-lib)
set(MR_GRAPHICS_LIB_VK_NAME mr-graphics-lib-vk)

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

# Vulkan backend layer: no RenderContext, Scene, Application
set(MR_GRAPHICS_LIB_VK_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/vulkan_state.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/buffer/buffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/descriptor/descriptor.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/images/image.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/pipelines/pipeline.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/pipelines/compute_pipeline.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/pipelines/graphics_pipeline.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/shaders/shader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/shaders/shader_resource.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/command_unit/command_unit.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/texture/texture.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/texture/sampler/sampler.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/resources/framedata/framedata.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/window/swapchain.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/mesh/mesh.cpp
)

# High-level layer: RenderContext, Scene, Model, Application, etc.
set(MR_GRAPHICS_LIB_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/window/render_context.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/window/window.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/window/file_writter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/window/dummy_presenter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/window/input_state.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/renderer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/renderer/material/material.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/lights/base/base_light.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/lights/directional/directional_light.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/scene/scene.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/model/model.cpp
)

add_library(${MR_GRAPHICS_LIB_VK_NAME} ${MR_GRAPHICS_LIB_VK_SOURCES})

target_link_libraries(${MR_GRAPHICS_LIB_VK_NAME} PUBLIC ${DEPS_LIBRARIES})

target_include_directories(${MR_GRAPHICS_LIB_VK_NAME}
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/renderer
  )
target_precompile_headers(${MR_GRAPHICS_LIB_VK_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/pch.hpp
  )
target_compile_definitions(${MR_GRAPHICS_LIB_VK_NAME} PRIVATE MR_GRAPHICS_VK_LAYER)
target_compile_definitions(${MR_GRAPHICS_LIB_VK_NAME} PUBLIC
  MR_PROJECT_DIR="${CMAKE_CURRENT_LIST_DIR}"
  MR_RES_DIR="${MR_RES_FULL_DIR}"
)

add_library(${MR_GRAPHICS_LIB_NAME} ${MR_GRAPHICS_LIB_SOURCES})

target_link_libraries(${MR_GRAPHICS_LIB_NAME} PUBLIC ${MR_GRAPHICS_LIB_VK_NAME})

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
target_compile_definitions(${MR_GRAPHICS_LIB_NAME} PUBLIC
  MR_PROJECT_DIR="${CMAKE_CURRENT_LIST_DIR}"
  MR_RES_DIR="${MR_RES_FULL_DIR}"
)

set(MR_GRAPHICS_PCH_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp)

option(BUILD_EXAMPLES "Build examples" ON)
if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if (GENERATE_DEPENDENCY_GRAPH)
  add_dependencies(${MR_GRAPHICS_LIB_NAME} dep-graph-${CMAKE_PROJECT_NAME})
endif()
if (FORMAT_SOURCE)
  add_dependencies(${MR_GRAPHICS_LIB_NAME} fix-format)
endif()

# Install rules
include(GNUInstallDirs)

install(TARGETS ${MR_GRAPHICS_LIB_VK_NAME} ${MR_GRAPHICS_LIB_NAME}
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
