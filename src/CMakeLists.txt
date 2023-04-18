find_package(PkgConfig)
pkg_check_modules(GTKMM gtkmm-4.0)

include_directories(${GTKMM_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR})
link_directories(${GTKMM_LIBRARY_DIRS})

# subsystems includes

set(SOURCE_FILES main.cpp pch.hpp)
add_executable(${CMAKE_PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${CMAKE_PROJECT_NAME} ${GTKMM_LIBRARIES})

target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE pch.hpp)
