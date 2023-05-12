find_package(PkgConfig)
target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/pch.hpp)
# subsystems includes
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pch.hpp
  ${CMAKE_CURRENT_LIST_DIR}/system.hpp)

add_subdirectory(renderer)

