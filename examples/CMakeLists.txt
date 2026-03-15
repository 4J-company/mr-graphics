cmake_minimum_required(VERSION 3.27)

add_executable(${CMAKE_PROJECT_NAME}-example "")

target_sources(${CMAKE_PROJECT_NAME}-example PRIVATE
  main.cpp
  render_options/render_options.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME}-example PRIVATE ${MR_GRAPHICS_LIB_NAME})
target_precompile_headers(${CMAKE_PROJECT_NAME}-example PRIVATE ${MR_GRAPHICS_PCH_HEADER})

set(SIMPLE_BENCH_TARGET_NAME simple-bench)
add_executable(${SIMPLE_BENCH_TARGET_NAME} simple_bench.cpp render_options/render_options.cpp)
target_link_libraries(${SIMPLE_BENCH_TARGET_NAME} PRIVATE ${MR_GRAPHICS_LIB_NAME})
target_precompile_headers(${SIMPLE_BENCH_TARGET_NAME} PRIVATE ${MR_GRAPHICS_PCH_HEADER})