find_package(PkgConfig)
pkg_check_modules(GTKMM gtkmm-4.0)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${GTKMM_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR})
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${GTKMM_LIBRARY_DIRS})

# subsystems includes

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pch.hpp)

add_subdirectory(renderer)

target_link_libraries(${CMAKE_PROJECT_NAME} ${GTKMM_LIBRARIES})

target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/pch.hpp)
