CPMAddPackage(
  NAME Format.cmake
  GITHUB_REPOSITORY TheLartians/Format.cmake
  VERSION 1.0
  CMAKE_FORMAT_EXTRA_ARGS -c ${CMAKE_SOURCE_DIR}/.clang-format
)

CPMAddPackage(
  NAME GroupSourcesByFolder.cmake
  GITHUB_REPOSITORY TheLartians/GroupSourcesByFolder.cmake
  VERSION 1.0
)

CPMAddPackage(
  NAME CPMLicenses.cmake
  GITHUB_REPOSITORY cpm-cmake/CPMLicenses.cmake
  VERSION 0.0.5
)

if (CPMLicenses.cmake_ADDED)
  cpm_licenses_create_disclaimer_target(
    write-licenses "${CMAKE_CURRENT_BINARY_DIR}/third_party_LICENSE.txt" "${CPM_PACKAGES}"
  )
endif()

CPMAddPackage(
  NAME Ccache.cmake
  GITHUB_REPOSITORY TheLartians/Ccache.cmake
  VERSION 1.2
)

CPMAddPackage(
  NAME cmake-scripts
  GITHUB_REPOSITORY StableCoder/cmake-scripts
  GIT_TAG 24.04.1
)

if (cmake-scripts_ADDED)
  include(${cmake-scripts_SOURCE_DIR}/c++-standards.cmake)
  include(${cmake-scripts_SOURCE_DIR}/compiler-options.cmake)

  cxx_23()
  if (SANITIZE)
    include(${cmake-scripts_SOURCE_DIR}/tools.cmake)
    include_what_you_use()
    cppcheck()
  endif()
  if (GENERATE_DEPENDENCY_GRAPH)
    include(${cmake-scripts_SOURCE_DIR}/dependency-graph.cmake)
    gen_dep_graph(png)
  endif()
endif()

