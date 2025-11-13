cmake_minimum_required(VERSION 3.27)

add_executable(${CMAKE_PROJECT_NAME}-example "")

target_sources(${CMAKE_PROJECT_NAME}-example PRIVATE 
  main.cpp
  render_options/render_options.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME}-example PRIVATE ${MR_GRAPHICS_LIB_NAME})

