cmake_minimum_required(VERSION 3.27)
project(model-renderer)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/options.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/settings.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake)

# add executable
add_executable(${CMAKE_PROJECT_NAME} ${XMAIN_SOURCES})
add_subdirectory(src)

# dependencies
target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    ${DEPS_LIBRARIES}
)

target_include_directories(
  ${CMAKE_PROJECT_NAME} PUBLIC
  ${DEPS_INCLUDE_DIRS}
)

target_compile_definitions(
  ${CMAKE_PROJECT_NAME} PRIVATE
  ${DEPS_DEFINITIONS}
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
add_custom_target(copy-assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/${ASSET_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${ASSET_DIR}
)

set(TARGET_DEPS copy-assets write-licenses)
if (GENERATE_DEPENDENCY_GRAPH)
  set(TARGET_DEPS dep-graph-${CMAKE_PROJECT_NAME})
endif()
if (FORMAT_SOURCE)
  set(TARGET_DEPS format-fix)
endif()

add_dependencies(
  ${CMAKE_PROJECT_NAME}
  ${TARGET_DEPS}
)
# GroupSourcesByFolder(${CMAKE_PROJECT_NAME}) # better IDE integration
