CPMAddPackage(
  NAME GroupSourcesByFolder.cmake
  GITHUB_REPOSITORY TheLartians/GroupSourcesByFolder.cmake
  VERSION 1.0
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

