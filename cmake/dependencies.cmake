file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# install libraries with no binaries available
find_package(Boost REQUIRED)
find_package(glfw3 REQUIRED)
find_package(stb REQUIRED)
find_package(mr-math REQUIRED)
find_package(mr-utils REQUIRED)
find_package(mr-importer REQUIRED)
find_package(Tracy REQUIRED)

CPMAddPackage("gh:Cvelth/vkfw#main")
CPMAddPackage("gh:charles-lunarg/vk-bootstrap@1.4.321")
CPMAddPackage("gh:bemanproject/inplace_vector#72e9bab")
CPMAddPackage("gh:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator@3.3.0")

if (${vkfw_ADDED})
  # VKFW_INLINE carries [[msvc::forceinline]] / [[gnu::always_inline]]. On the two
  # variables below it sits after constexpr, so GCC 15 rejects it ("attributes in
  # middle of decl-specifiers") and MSVC rejects forceinline on a non-function.
  # TODO: drop this once Cvelth/vkfw stops marking these variables VKFW_INLINE.
  set(_vkfw_header "${vkfw_SOURCE_DIR}/include/vkfw/vkfw.hpp")
  file(READ "${_vkfw_header}" _vkfw_text)
  set(_vkfw_orig "${_vkfw_text}")
  string(REPLACE
    "VKFW_CONSTEXPR VKFW_INLINE unsigned AnyPosition"
    "VKFW_CONSTEXPR inline unsigned AnyPosition"
    _vkfw_text "${_vkfw_text}")
  string(REPLACE
    "VKFW_CONSTEXPR VKFW_INLINE Nullopt nullopt"
    "VKFW_CONSTEXPR inline Nullopt nullopt"
    _vkfw_text "${_vkfw_text}")
  if(NOT _vkfw_text STREQUAL _vkfw_orig)
    file(WRITE "${_vkfw_header}" "${_vkfw_text}")
  endif()

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
find_package(TBB REQUIRED tbb)

# set important variables
set(DEPS_LIBRARIES
  Vulkan::Vulkan
  GPUOpen::VulkanMemoryAllocator
  libvkfw
  vk-bootstrap-lib
  stb::stb
  beman.inplace_vector
  boost::boost
  TBB::tbb
  Tracy::TracyClient

  mr-math::mr-math
  mr-utils::mr-utils
  mr-importer::mr-importer
)

# install CMake scripts
include(${CMAKE_SOURCE_DIR}/cmake/scripts.cmake)
