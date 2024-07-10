file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

if (BINARY_DEPS)
else()
endif()

# install libraries with no binaries available
CPMFindPackage(
  NAME glfw3
  GITHUB_REPOSITORY glfw/glfw
  GIT_TAG 3.4
  OPTIONS
    "GLFW_BUILD_TESTS OFF"
    "GLFW_BUILD_EXAMPLES OFF"
    "GLFW_BULID_DOCS OFF"
)

CPMFindPackage(
  NAME vkfw
  GITHUB_REPOSITORY Cvelth/vkfw
  GIT_TAG glfw-3.4
)

CPMFindPackage(
  NAME assimp
  URL "https://github.com/assimp/assimp/archive/refs/tags/v5.4.2.zip"
  OPTIONS
    "ASSIMP_WARNINGS_AS_ERRORS OFF"
    "ASSIMP_BUILD_TESTS OFF"
    "ASSIMP_NO_EXPORT ON"
)

# download a single file from stb
file(
  DOWNLOAD
  https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
  ${CMAKE_CURRENT_BINARY_DIR}/_deps/stb-src/stb/stb_image.h
  EXPECTED_HASH SHA256=594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
)

find_package(Vulkan)

# set important variables
set(DEPS_LIBRARIES
  Vulkan::Vulkan
  assimp::assimp
  glfw
)

set(DEPS_INCLUDE_DIRS
  ${assimp_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}/_deps/stb-src
  ${vkfw_SOURCE_DIR}/include
)

set(DEPS_DEFINITIONS
)

# install CMake scripts
include(${CMAKE_SOURCE_DIR}/cmake/scripts.cmake)
