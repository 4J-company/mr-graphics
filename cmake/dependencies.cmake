file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# install libraries with no binaries available
find_package(glfw3 REQUIRED)
find_package(meshoptimizer REQUIRED)
find_package(mr-utils REQUIRED)

CPMAddPackage("gh:Cvelth/vkfw#main")
CPMAddPackage("gh:4j-company/mr-math#master")
CPMAddPackage("gh:charles-lunarg/vk-bootstrap@1.4.321")
CPMAddPackage("gh:bemanproject/inplace_vector#b81a3c7")

if (${vkfw_ADDED})
  add_library(libvkfw INTERFACE "")
  target_link_libraries(libvkfw INTERFACE glfw)
  target_include_directories(libvkfw INTERFACE ${vkfw_SOURCE_DIR}/include)
endif()

if (${vk-bootstrap_ADDED})
  add_library(vk-bootstrap-lib INTERFACE "")
  target_link_libraries(vk-bootstrap-lib INTERFACE vk-bootstrap::vk-bootstrap)
  target_include_directories(vk-bootstrap-lib INTERFACE ${vk-bootstrap_SOURCE_DIR}/src)
endif()

CPMFindPackage(
  NAME tinygltf
  GITHUB_REPOSITORY syoyo/tinygltf
  GIT_TAG release
  OPTIONS
    "TINYGLTF_BUILD_LOADER_EXAMPLE OFF"
)

if (NOT TARGET libstb-image)
  # download a single file from stb
  file(
    DOWNLOAD
    https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    ${CMAKE_CURRENT_BINARY_DIR}/_deps/stb-src/stb/stb_image.h
    EXPECTED_HASH SHA256=594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
  )
  add_library(libstb-image INTERFACE "")
  target_include_directories(libstb-image INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/_deps/stb-src/)
endif()

find_package(Vulkan)

# set important variables
set(DEPS_LIBRARIES
  Vulkan::Vulkan
  mr-math-lib
  mr-utils::mr-utils
  tinygltf
  libvkfw
  libstb-image
  vk-bootstrap-lib
  meshoptimizer::meshoptimizer
  beman.inplace_vector
)

# TBB is required since it's dependency of PSTL on GCC and Clang
if (NOT MSVC)
  find_package(TBB REQUIRED tbb)
  set(DEPS_LIBRARIES ${DEPS_LIBRARIES} tbb)
endif()

# install CMake scripts
include(${CMAKE_SOURCE_DIR}/cmake/scripts.cmake)
