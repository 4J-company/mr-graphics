file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# install libraries with no binaries available
find_package(glfw3 REQUIRED)
find_package(stb REQUIRED)
find_package(mr-math REQUIRED)
find_package(mr-utils REQUIRED)
find_package(mr-manager REQUIRED)
find_package(mr-importer REQUIRED)

CPMAddPackage("gh:Cvelth/vkfw#main")
CPMAddPackage("gh:charles-lunarg/vk-bootstrap@1.4.321")
CPMAddPackage("gh:bemanproject/inplace_vector#b81a3c7")
CPMAddPackage("gh:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator@3.3.0")

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

find_package(Vulkan)

# set important variables
set(DEPS_LIBRARIES
  Vulkan::Vulkan
  GPUOpen::VulkanMemoryAllocator
  libvkfw
  vk-bootstrap-lib
  stb::stb
  beman.inplace_vector

  mr-math::mr-math
  mr-utils::mr-utils
  mr-manager::mr-manager
  mr-importer::mr-importer
)

# TBB is required since it's dependency of PSTL on GCC and Clang
if (NOT MSVC)
  find_package(TBB REQUIRED tbb)
  set(DEPS_LIBRARIES ${DEPS_LIBRARIES} tbb)
endif()

# install CMake scripts
include(${CMAKE_SOURCE_DIR}/cmake/scripts.cmake)
