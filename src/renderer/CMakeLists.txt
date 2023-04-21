# cmake_minimum_required(VERSION 3.10)
# project(tgr VERSION 1.0 DESCRIPTION "General purpose render Vulkan-based library") # Tough Graphics Renderer

# add_library(${PROJECT_NAME} SHARED
#   renderer.cpp
#   )
# set_target_properties(${PROJECT_NAME} PROPERTIES 
#   VERSION ${PROJECT_VERSION}
#   PUBLIC_HEADER renderer.hpp)
#
# configure_file(mylib.pc.in mylib.pc @ONLY)
# target_include_directories(mylib PRIVATE .)
# 
# include(GNUInstallDirs)
# install(TARGETS ${PROJECT_NAME}
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/window_context/window_system)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/renderer.cpp 
  ${CMAKE_CURRENT_LIST_DIR}/renderer.hpp
  ${CMAKE_CURRENT_LIST_DIR}/window_context/window_context.cpp
  ${CMAKE_CURRENT_LIST_DIR}/window_context/window_context.hpp
  )

find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)


target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/renderer.hpp)
# vma, obj-loader, stb-image
