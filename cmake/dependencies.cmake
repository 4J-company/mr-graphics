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
CPMAddPackage(
  NAME CrossWindow
  GITHUB_REPOSITORY 4J-company/mr-window
  GIT_TAG master
  DOWNLOAD_ONLY YES
)

if (CrossWindow_ADDED)
  add_subdirectory(${CrossWindow_SOURCE_DIR})
endif()

CPMAddPackage(
  NAME CrossWindowGraphics
  GITHUB_REPOSITORY cone-forest/CrossWindow-Graphics
  GIT_TAG master
)

CPMFindPackage(
  NAME assimp
  GITHUB_REPOSITORY assimp/assimp
  GIT_TAG master
  OPTIONS
    "ASSIMP_WARNINGS_AS_ERRORS OFF"
    "ASSIMP_BUILD_TESTS OFF"
    "ASSIMP_NO_EXPORT ON"
)

# download a single file from stb
file(
  DOWNLOAD
  https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
  ${CMAKE_CURRENT_BINARY_DIR}/_deps/stb/stb_image.h
  EXPECTED_HASH SHA256=594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
)

find_package(Vulkan)

# set important variables
set(DEPS_LIBRARIES
  Vulkan::Vulkan
  assimp::assimp
  CrossWindow
  CrossWindowGraphics
)

set(DEPS_INCLUDE_DIRS
  ${assimp_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}/_deps/stb
)

set(DEPS_DEFINITIONS
  ${XWIN_DEFINITIONS}
)

# install CMake scripts
include(${CMAKE_SOURCE_DIR}/cmake/scripts.cmake)
